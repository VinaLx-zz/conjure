#ifndef CONJURE_CONJURY_H_
#define CONJURE_CONJURY_H_

#include "conjure/function-wrapper.h"
#include "conjure/log.h"
#include "conjure/stack.h"
#include "conjure/state.h"
#include "conjure/value-tunnel.h"
#include <assert.h>
#include <stdint.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace conjure::detail {

struct Context;

} // namespace conjure::detail

extern "C" {

void ContextSwitch(
    void *this_, conjure::detail::Context *from,
    conjure::detail::Context *to) asm("ContextSwitch");

} // extern "C"

namespace conjure {

namespace detail {

struct CalleeSavedRegs {
    uint64_t rbx = 0;
    uint64_t rbp = 0;
    uint64_t r12 = 0;
    uint64_t r13 = 0;
    uint64_t r14 = 0;
    uint64_t r15 = 0;
};

struct Context {
    Context(void *stack_ptr = nullptr, void *return_addr = nullptr)
        : stack_ptr(stack_ptr), return_addr(return_addr) {}

    detail::CalleeSavedRegs registers;

    void *stack_ptr;
    void *return_addr;
};

} // namespace detail

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

    void Done(Conjury &next) {
        // printf("ending current, this: %p, parent: %p\n", this,
        // parent_conjury_);
        if (Parent()) {
            assert(Parent()->state_ == State::kWaiting);
            Parent()->state_ = State::kReady;
        }
        YieldAndSetState(State::kFinished, next);
    }

    void Yield(Conjury &next) {
        [[maybe_unused]] bool success = YieldAndSetState(State::kReady, next);
        assert(success);
    }

    void Suspend(Conjury &next) {
        [[maybe_unused]] bool success =
            YieldAndSetState(State::kSuspended, next);
        assert(success);
    }

    bool Wake() {
        if (state_ == State::kSuspended) {
            state_ = State::kReady;
            return true;
        }
        return false;
    }

    bool Wait(Conjury &next) {
        // printf("setting parent of %s to %s\n", next.Name(), Name());
        next.parent_conjury_ = this;
        return YieldAndSetState(State::kWaiting, next);
    }

    Conjury *Parent() {
        return parent_conjury_;
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

    bool ResumeFrom(Conjury &from) {
        if (state_ == State::kFinished) {
            return true;
        }
        if (not state::IsExecutable(state_)) {
            // puts(state::ToString(state_));
            return false;
        }
        // assert(state != State::kRunning);
        UnsafeResumeFrom(from);
        return true;
    }

  private:
    void UnsafeResumeFrom(Conjury &from) {
        if (state_ == State::kInitial) {
            state_ = State::kRunning;
            CONJURE_LOGF("%s ==> %s", from.Name(), Name());
            ContextSwitch(func_wrapper_this_, &from.context_, &context_);
        } else {
            state_ = State::kRunning;
            CONJURE_LOGF("%s ==> %s", from.Name(), Name());
            ContextSwitch(nullptr, &from.context_, &context_);
        }
    }

    bool YieldAndSetState(State s, Conjury &to) {
        state_ = s;
        return to.ResumeFrom(*this);
    }

  protected:
    Stack stack_;
    detail::Context context_;
    State state_ = State::kInitial;

    void *func_wrapper_this_ = nullptr;
    Conjury *parent_conjury_ = nullptr;

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
        this->UnsafeSetState(State::kReady);
        return tunnel_.GetOne();
    }

    template <typename U>
    void StoreGen(U &&u) {
        tunnel_.Pass(std::forward<U>(u));
        this->Parent()->UnsafeSetState(State::kReady);
    }

  private:
    ValueTunnel<G> tunnel_;
};

} // namespace conjure

#endif // CONJURE_CONJURY_H_
