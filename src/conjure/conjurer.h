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
  public:
    Conjurer()
        : stage_(InitMainConjury()), scheduler_(std::make_unique<Scheduler>()),
          sche_co_(
              UnmanagedConjure(Config("__scheduler__"), &RunScheduler, this)) {
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
        if (IsWaitedByOthers(co)) {
            throw InconsistentWait(ActiveConjury(), co);
        }
        // Conjury *current = SetNextActive(co);
        Conjury *current = ActiveConjury();
        co->ReturnTarget(current);
        if (not stage_.SwitchTo(co, State::kWaiting)) {
            current->UnsafeSetState(State::kWaiting);
            stage_.UnsafeSwitchTo(sche_co_.get());
        }
        T result = co->UnsafeGetResult();
        stage_.Destroy(co);
        return result; // NRVO
    }

    void End() {
        CONJURE_LOGF("%s ending", ActiveConjury()->Name());
        Conjury *next = nullptr;
        if (Conjury *target = ActiveConjury()->ReturnTarget();
            target != nullptr) {
            // TODO: is this assert correct?
            assert(target->GetState() == State::kWaiting);
            next = target;
            // printf("parent is %s\n", next->Name());
        } else {
            next = sche_co_.get();
        }
        stage_.UnsafeSwitchTo(next, State::kFinished);
    }

    template <typename P = Void>
    void Suspend(P p = P{}) {
        Conjury *current = ActiveConjury();
        if constexpr (std::is_same_v<P, Void>) {
            scheduler_->RegisterSuspended(current);
        } else {
            scheduler_->RegisterSuspended(current, std::move(p));
        }
        stage_.UnsafeSwitchTo(sche_co_.get(), State::kSuspended);
    }

    void Yield() {
        stage_.UnsafeSwitchTo(sche_co_.get(), State::kReady);
    }

    bool Resume(Conjury *next) {
        if (not state::IsExecutable(next->GetState())) {
            return false;
        }
        YieldTo(next);
        return true;
    }

    template <typename U, typename G = std::decay_t<U>>
    void Yield(U &&u) {
        // auto gen_co = GetActiveConjuryAs<ConjureGen<G>>();
        // if (gen_co == nullptr) {
        // throw InvalidYieldContext<G>(ActiveConjury());
        // }
        // gen_co->StoreGen(std::forward<U>(u));
        // SetNextActive(gen_co->ReturnTarget());
        // gen_co->Wait(*gen_co->ReturnTarget());
    }

    template <typename G>
    bool GenMoveNext(ConjuryClient<ConjureGen<G>> *gen_co) {
        // Conjury *current = ActiveConjury();
        // current->Wait(*gen_co);
        // stage_.SwitchTo(gen_co);
        // if (gen_co->IsFinished()) {
        // Destroy(gen_co);
        // return false;
        // }
        // return true;
    }

    Conjury *ActiveConjury() {
        return stage_.ActiveConjury();
    }

  private:
    static std::unique_ptr<Conjurer> instance_;

    static Conjury::Pointer InitMainConjury() {
        auto main_co = std::make_unique<Conjury>();
        main_co->Name("__main__");
        return main_co;
    }

    static void RunScheduler(Conjurer *conjurer) {
        for (;;) {
            Conjury *next = conjurer->scheduler_->GetNext();
            if (next != nullptr) {
                conjurer->stage_.UnsafeSwitchTo(next);
            }
        }
    }

    bool IsWaitedByOthers(Conjury *c) {
        return c->ReturnTarget() != nullptr and
               c->ReturnTarget() != ActiveConjury();
    }

    void YieldTo(Conjury *next) {
        scheduler_->RegisterReady(ActiveConjury());
        stage_.SwitchTo(next, State::kReady);
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
