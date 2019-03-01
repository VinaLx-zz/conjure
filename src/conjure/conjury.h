#ifndef CONJURE_CONJURY_H_
#define CONJURE_CONJURY_H_

#include "conjure/function-wrapper.h"
#include "conjure/log.h"
#include "conjure/stack.h"
#include "conjure/state.h"
#include "conjure/system.h"
#include "conjure/value-tunnel.h"
#include <assert.h>
#include <stdint.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace conjure {

class Conjury {
  public:
    using Pointer = std::unique_ptr<Conjury>;

    Conjury() : context_(nullptr, nullptr) {}

    Conjury(Stack stk) : stack_(std::move(stk)), context_(stack_.stack_start) {
        // printf("coroutine stack: %p\n", context_.stack_ptr);
    }

    virtual ~Conjury() = default;

    bool IsFinished() const {
        return state_ == State::kFinished;
    }

    bool Wake() {
        if (not wakeup_flag_) {
            return wakeup_flag_ = true;
        }
        return false;
    }

    bool ConsumeWakeUp() {
        if (wakeup_flag_) {
            state_ = State::kReady;
            wakeup_flag_ = false;
            return true;
        }
        return false;
    }

    Conjury *ReturnTarget() {
        return return_target_;
    }

    void ReturnTarget(Conjury *conjury) {
        return_target_ = conjury;
    }

    State GetState() const {
        return state_;
    }

    void UnsafeSetState(State state) {
        state_ = state;
    }

    const char *Name() const {
        return name_.data();
    }

    void Name(const std::string &name) {
        name_ = name;
    }

    void SwitchTo(Conjury &to) {
        void *wrapper_this =
            to.GetState() == State::kInitial ? to.func_wrapper_this_ : nullptr;
        to.state_ = State::kRunning;
        CONJURE_LOGF("%s ==> %s", Name(), to.Name());
        ContextSwitch(wrapper_this, &context_, &to.context_);
    }

  protected:
    Stack stack_;
    system::Context context_;
    State state_ = State::kInitial;

    void *func_wrapper_this_ = nullptr;
    Conjury *return_target_ = nullptr;

    volatile bool wakeup_flag_ = false;

    std::string name_;
};

inline void End();

template <typename F, typename... Args>
void ConjuryCallWrapper(FunctionWrapper<F, Args...> *this_) {
    this_->Call();
    End();
}

template <typename Gen>
struct ConjureGen {};

template <typename Result>
class ConjuryClientImpl : public Conjury {
    struct ResultStore {
        virtual std::optional<Result> Get() = 0;
        virtual ~ResultStore() = default;
    };
    template <typename F, typename... Args>
    struct ResultStoreImpl : ResultStore {
        using WrapperT = FunctionWrapper<F, Args...>;

        ResultStoreImpl(FunctionWrapper<F, Args...> w)
            : wrapper(std::move(w)) {}

        virtual std::optional<Result> Get() override {
            std::optional<Result> result = std::move(wrapper.result_);
            wrapper.result_.reset();
            return result; // RVO
        }

        WrapperT wrapper;
    };

  public:
    using ResultT = Result;

    template <typename F, typename... Args>
    ConjuryClientImpl(Stack stack, FunctionWrapper<F, Args...> wrapper)
        : Conjury(std::move(stack)) {
        auto result_store_impl =
            std::make_unique<ResultStoreImpl<F, Args...>>(std::move(wrapper));
        this->func_wrapper_this_ = &result_store_impl->wrapper;
        this->context_.return_addr =
            (void *)static_cast<typename FunctionWrapper<F, Args...>::CallerT>(
                &ConjuryCallWrapper);
        result_ = std::move(result_store_impl);
    }

    Result UnsafeGetResult() {
        return result_->Get().value();
    }

  private:
    std::unique_ptr<ResultStore> result_;
};

template <typename Result>
class ConjuryClient : public ConjuryClientImpl<Result> {
  public:
    using Pointer = std::unique_ptr<ConjuryClient>;
    using ConjuryClientImpl<Result>::ConjuryClientImpl;
};

template <typename G>
class ConjuryClient<ConjureGen<G>> : public ConjuryClientImpl<ConjureGen<G>> {
  public:
    using Pointer = std::unique_ptr<ConjuryClient>;
    using ConjuryClientImpl<ConjureGen<G>>::ConjuryClientImpl;

    const G *GetGenPtr() {
        return tunnel_.GetOne();
    }

    template <typename U>
    void StoreGen(U &&u) {
        tunnel_.Pass(std::forward<U>(u));
    }

  private:
    ValueTunnel<G> tunnel_;
};

} // namespace conjure

#endif // CONJURE_CONJURY_H_
