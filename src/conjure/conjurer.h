#ifndef CONJURE_CONJURER_H_
#define CONJURE_CONJURER_H_

#include "conjure/config.h"
#include "conjure/conjury.h"
#include "conjure/scheduler.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace conjure {

template <typename F, typename... Args>
using ConjuryClientT = ConjuryClient<WrapperResultT<F, Args...>>;

class Conjurer {
  public:
    Conjurer()
        : scheduler_(std::make_unique<Scheduler>()),
          sche_co_(
              UnmanagedConjure(Config("__scheduler__"), &RunScheduler, this)) {
        auto main_co = std::make_unique<Conjury>();
        main_co->Name("__main__");
        active_conjury_ = main_co.get();
        conjuries_.push_back(std::move(main_co));
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
        conjuries_.push_back(std::move(co));

        return co_client;
    }

    template <typename T>
    T Wait(ConjuryClient<T> *co) {
        Conjury *current = SetNextActive(co);
        if (not current->Wait(*co)) {
            SetNextActive(sche_co_.get());
            sche_co_->ResumeFrom(*current);
        }
        T result = co->UnsafeGetResult();
        Destroy(co);
        return result; // NRVO
    }

    void End() {
        // printf("%s ending...\n", ActiveConjury()->Name());
        Conjury *next = ActiveConjury()->Parent();
        if (next == nullptr) {
            next = sche_co_.get();
        }
        // else {
        // printf("parent is %s\n", next->Name());
        // }
        Conjury *current = SetNextActive(next);
        current->Done(*next);
    }

    template <typename P = Void>
    void Suspend(P p = P{}) {
        Conjury *current = SetNextActive(sche_co_.get());
        if constexpr (std::is_same_v<P, Void>) {
            scheduler_->RegisterSuspended(current);
        } else {
            scheduler_->RegisterSuspended(current, std::move(p));
        }
        current->Suspend(*sche_co_);
    }

    void Yield() {
        YieldTo(sche_co_.get());
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
        auto gen_co = GetActiveConjuryAs<ConjureGen<G>>();
        if (gen_co == nullptr) {
            throw std::runtime_error("invalid yielding context");
        }
        gen_co->StoreGen(std::forward<U>(u));
        SetNextActive(gen_co->Parent());
        gen_co->Wait(*gen_co->Parent());
    }

    template <typename G>
    bool GenMoveNext(ConjuryClient<ConjureGen<G>> *gen_co) {
        Conjury *current = SetNextActive(gen_co);
        current->Wait(*gen_co);
        if (gen_co->IsFinished()) {
            Destroy(gen_co);
            return false;
        }
        return true;
    }

    bool Destroy(Conjury *co) {
        auto iter = std::find_if(
            begin(conjuries_), end(conjuries_),
            [co](const auto &cop) { return cop.get() == co; });
        if (iter == end(conjuries_)) {
            return false;
        }
        conjuries_.erase(iter);
        return true;
    }

    Conjury *ActiveConjury() {
        return active_conjury_;
    }

  private:
    static std::unique_ptr<Conjurer> instance_;

    static void RunScheduler(Conjurer *conjurer) {
        for (;;) {
            Conjury *next = conjurer->scheduler_->GetNext();
            if (next != nullptr) {
                conjurer->UnscheduledYieldTo(next);
            }
        }
    }

    void YieldTo(Conjury *next) {
        scheduler_->RegisterReady(ActiveConjury());
        UnscheduledYieldTo(next);
    }

    void UnscheduledYieldTo(Conjury *next) {
        Conjury *current = SetNextActive(next);
        current->Yield(*next);
    }

    template <typename R>
    ConjuryClient<R> *GetActiveConjuryAs() {
        auto co_client = dynamic_cast<ConjuryClient<R> *>(active_conjury_);
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

    Conjury *SetNextActive(Conjury *co) {
        Conjury *old = active_conjury_;
        active_conjury_ = co;
        return old;
    }

    Conjury *active_conjury_;

    Scheduler::Pointer scheduler_;

    Conjury::Pointer sche_co_;

    std::vector<Conjury::Pointer> conjuries_;
};

} // namespace conjure

#endif // CONJURE_CONJURER_H_
