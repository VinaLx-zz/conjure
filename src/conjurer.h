#ifndef CONJURE_CONJURER_H_
#define CONJURE_CONJURER_H_

#include "./coroutine.h"
#include <memory>
#include <stdexcept>
#include <vector>

namespace conjure {

template <typename F, typename... Args>
using ConjuryClientT = ConjuryClient<WrapperResultT<F, Args...>>;

class Conjurer {
  public:
    Conjurer() : scheduler_(ConjureImpl(Config(), &Scheduler)) {
        auto main_co = std::make_unique<Conjury>();
        active_conjury_ = main_co.get();
        conjuries_.push_back(std::move(main_co));
        printf(
            "main_co: %p, scheduler: %p\n", active_conjury_, scheduler_.get());
    }

    static Conjurer *Instance() {
        static Conjurer conjurer;
        return &conjurer;
    }

    template <typename F, typename... Args>
    ConjuryClientT<F, Args...> *
    Conjure(const Config &config, F f, Args &&... args) {
        auto co =
            ConjureImpl(config, std::move(f), std::forward<Args>(args)...);
        auto co_client = co.get();
        conjuries_.push_back(std::move(co));

        return co_client;
    }

    template <typename T>
    T Wait(ConjuryClient<T> *co) {
        Conjury *current = SetNextActive(co);
        current->Wait(*co);
        T result = co->UnsafeGetResult();
        Destroy(co);
        return result; // NRVO
    }

    void End() {
        Conjury *current = SetNextActive(scheduler_.get());
        current->FinishYield(*scheduler_);
    }

    void Yield() {
        YieldImpl(scheduler_.get());
    }

    template <typename U, typename G = std::decay_t<U>>
    void Yield(U &&u) {
        auto gen_co = GetActiveConjuryAs<ConjureGen<G>>();
        if (gen_co == nullptr) {
            throw std::runtime_error("invalid yielding context");
        }
        gen_co->StoreGen(std::forward<U>(u));
        SetNextActive(gen_co->Parent());
        gen_co->BlockingYield(*gen_co->Parent());
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

  private:
    static std::unique_ptr<Conjurer> instance_;

    static void Scheduler() {
        // TODO: add a non-trivial scheduler
        Conjurer &conjurer = *Instance();
        for (;;) {
            // printf("scheduler start\n");
            Conjury *next = nullptr;
            for (auto &co : conjurer.conjuries_) {
                if (co->GetState() == Conjury::State::kReady) {
                    next = co.get();
                    break;
                }
            }
            if (next != nullptr) {
                conjurer.YieldImpl(next);
            }
        }
    }

    void YieldImpl(Conjury *next) {
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
    ConjureImpl(const Config &config, F f, Args &&... args) {
        using C = ConjuryClientT<F, Args...>;

        Stack stack(config.stack_size);

        return std::make_unique<C>(
            std::move(stack),
            WrapCall(std::move(f), std::forward<Args>(args)...));
    }

    Conjury *SetNextActive(Conjury *co) {
        Conjury *old = active_conjury_;
        active_conjury_ = co;
        return old;
    }

    Conjury *active_conjury_;

    Conjury::Pointer scheduler_;

    std::vector<Conjury::Pointer> conjuries_;
};

inline void Yield() {
    Conjurer::Instance()->Yield();
}

template <typename T>
T Wait(ConjuryClient<T> *co) {
    return Conjurer::Instance()->Wait(co);
}

void End() {
    Conjurer::Instance()->End();
}

template <typename F, typename... Args>
ConjuryClientT<F, Args...> *
Conjure(const Config &config, F f, Args &&... args) {
    return Conjurer::Instance()->Conjure(
        config, std::move(f), std::forward<Args>(args)...);
}

template <typename G>
bool GenMoveNext(ConjuryClient<ConjureGen<G>> *co) {
    return Conjurer::Instance()->GenMoveNext(co);
}

template <typename U>
void Yield(U &&u) {
    return Conjurer::Instance()->Yield(std::forward<U>(u));
}

} // namespace conjure

#endif // CONJURE_CONJURER_H_
