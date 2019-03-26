#ifndef CONJURE_CONJURER_H_
#define CONJURE_CONJURER_H_

#include "conjure/config.h"
#include "conjure/conjury.h"
#include "conjure/exceptions.h"
#include "conjure/scheduler.h"
#include "conjure/stage.h"
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace conjure {

template <typename F, typename... Args>
using ConjuryClientT = ConjuryClient<WrapperResultT<F, Args...>>;

class Conjurer {
    friend class Scheduler;

  public:
    Conjurer()
        : stage_(InitMainConjury()),
          scheduler_(std::make_unique<Scheduler>(this)),
          sche_co_(UnmanagedConjure(
              Config("__scheduler__"), &Scheduler::Run, scheduler_.get())) {
        // printf(
        // "main_co: %p, scheduler: %p\n", active_conjury_, sche_co_.get());
    }

    static Conjurer *Instance() {
        static Conjurer conjurer;
        return &conjurer;
    }

    template <typename F, typename... Args>
    ConjuryClientT<F, Args...> *
    Conjure(const Config &config, F f, Args &&... args) {
        auto co =
            UnmanagedConjure(config, std::move(f), std::forward<Args>(args)...);
        auto co_client = co.get();
        stage_.Manage(std::move(co));

        return co_client;
    }

    template <typename T>
    T Wait(ConjuryClient<T> *co) {
        ActiveConjury()->WaitTarget(co);
        if (not WaitAndSwitch(co)) {
            throw InconsistentWait(ActiveConjury(), co);
        }
        T result = co->UnsafeGetResult();
        stage_.Destroy(co);
        return result; // NRVO
    }

    void End() {
        Conjury *me = ActiveConjury();
        CONJURE_LOGF("%s ending", me->Name());
        if (Conjury *target = me->ReturnTarget();
            target != nullptr and target->WaitTarget() == me) {
            // TODO: is this assert correct?
            assert(target->GetState() == State::kWaiting);
            target->WaitTarget(nullptr);
            ForceYieldBack(State::kFinished);
            // printf("parent is %s\n", next->Name());
        } else {
            YieldToScheduler(State::kFinished);
        }
    }

    template <typename P = Void>
    void Suspend(P p = P{}) {
        Conjury *current = ActiveConjury();
        if constexpr (std::is_same_v<P, Void>) {
            scheduler_->RegisterSuspended(current);
        } else {
            scheduler_->RegisterSuspended(current, std::move(p));
        }
        YieldToScheduler(State::kSuspended);
    }

    void Yield() {
        Conjury *return_target = ActiveConjury()->ReturnTarget();
        scheduler_->RegisterReady(ActiveConjury());
        if (return_target != nullptr and return_target->IsExecutable()) {
            ForceYieldBack(State::kReady);
        } else {
            YieldToScheduler(State::kReady);
        }
    }

    bool Resume(Conjury *next) {
        if (not state::IsExecutable(next->GetState())) {
            return false;
        }
        scheduler_->RegisterReady(ActiveConjury());
        SetReturnTargetAndSwitch(next, State::kReady);
        return true;
    }

    template <typename U, typename G = std::decay_t<U>>
    void YieldWith(U &&u) {
        GenerateImpl(std::forward<U>(u));
        ForceYieldBack(State::kReady);
    }

    template <typename U>
    void Generate(U &&u) {
        GenerateImpl(std::forward<U>(u));
        if (Conjury *target = ActiveConjury()->ReturnTarget()) {
            target->UnsafeSetState(State::kReady);
        }
    }

    template <typename G>
    bool GenMoveNext(ConjuryClient<Generating<G>> *gen_co) {
        ActiveConjury()->WaitTarget(gen_co);
        WaitAndSwitch(gen_co);
        if (gen_co->IsFinished()) {
            stage_.Destroy(gen_co);
            return false;
        }
        return true;
    }

    Conjury *ActiveConjury() {
        return stage_.ActiveConjury();
    }

  private:
    static std::unique_ptr<Conjurer> instance_;

    bool WaitAndSwitch(Conjury *co) {
        if (IsWaitedByOthers(co)) {
            return false;
        }
        co->ReturnTarget(ActiveConjury());
        ActiveConjury()->WaitTarget(co);
        SwitchToTargetOrScheduler(co, State::kWaiting);
        return true;
    }

    void SetReturnTargetAndSwitch(Conjury *co, State s) {
        co->ReturnTarget(ActiveConjury());
        // TODO: when does this happen?
        SwitchToTargetOrScheduler(co, s);
    }

    void SwitchToTargetOrScheduler(Conjury *co, State s) {
        if (not stage_.SwitchTo(co, s)) {
            YieldToScheduler(s);
        }
    }

    static Conjury::Pointer InitMainConjury() {
        auto main_co = std::make_unique<Conjury>();
        main_co->Name("__main__");
        return main_co;
    }

    bool IsWaitedByOthers(Conjury *c) {
        return c->ReturnTarget() != nullptr and
               c->ReturnTarget() != ActiveConjury();
    }

    void YieldTo(Conjury *next) {
        scheduler_->RegisterReady(ActiveConjury());
        stage_.SwitchTo(next, State::kReady);
    }

    void ForceYieldBack(State s) {
        Conjury *target = ActiveConjury()->ReturnTarget();
        assert(target != nullptr);
        assert(target->GetState() == State::kWaiting);
        stage_.UnsafeSwitchTo(target, s);
    }

    template <typename G>
    const G *WaitGenerate(ConjuryClient<Generating<G>> *co) {
        if (not GenMoveNext(co)) {
            return nullptr;
        }
        return co->GetGenPtr();
    }

    template <typename S = Void>
    void YieldToScheduler(S s = S{}) {
        stage_.UnsafeSwitchTo(sche_co_.get(), s);
    }

    template <typename U, typename G = std::decay_t<U>>
    void GenerateImpl(U &&u) {
        auto gen_co = GetActiveConjuryAs<Generating<G>>();
        if (gen_co == nullptr or gen_co->ReturnTarget() == nullptr) {
            throw InvalidYieldContext<G>(ActiveConjury());
        }
        gen_co->StoreGen(std::forward<U>(u));
    }

    template <typename R>
    ConjuryClient<R> *GetActiveConjuryAs() {
        auto co_client = dynamic_cast<ConjuryClient<R> *>(ActiveConjury());
        return co_client;
    }

    template <typename F, typename... Args>
    static typename ConjuryClientT<F, Args...>::Pointer
    UnmanagedConjure(const Config &config, F f, Args &&... args) {
        using C = ConjuryClientT<F, Args...>;

        Stack stack(config.stack_size);

        auto co = std::make_unique<C>(
            std::move(stack),
            WrapCall(std::move(f), std::forward<Args>(args)...));
        co->Name(config.name);
        return co;
    }

    Stage stage_;

    Scheduler::Pointer scheduler_;

    Conjury::Pointer sche_co_;
};

} // namespace conjure

#endif // CONJURE_CONJURER_H_
