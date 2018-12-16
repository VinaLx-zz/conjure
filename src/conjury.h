#ifndef CONJURE_CONJURY_H_
#define CONJURE_CONJURY_H_

#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "./function-wrapper.h"
#include "./stack.h"

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

struct Config {
    static constexpr int kDefaultStackSize = 16 * 1024;

    Config() = default;

    Config(const std::string &name) : routine_name(name) {}

    int stack_size = kDefaultStackSize;
    std::string routine_name;
};

class Conjury;

const char *Name(Conjury *c);

class Conjury {
  public:
    enum class State { kInitial, kBlocking, kReady, kRunning, kFinished };

    using Pointer = std::unique_ptr<Conjury>;

    Conjury() : context_(nullptr, nullptr) {}

    Conjury(Stack stk) : stack_(std::move(stk)), context_(stack_.stack_start) {
        printf("coroutine stack: %p\n", context_.stack_ptr);
    }

    bool IsFinished() const {
        return state_ == State::kFinished;
    }

    void FinishYield(Conjury &next) {
        // printf("ending current, this: %p, parent: %p\n", this, parent_conjury_);
        if (Parent()->state_ == State::kBlocking) {
            Parent()->state_ = State::kReady;
        }
        YieldAndSetState(State::kFinished, next);
    }

    void Yield(Conjury &next) {
        YieldAndSetState(State::kReady, next);
    }

    void BlockingYield(Conjury &next) {
        YieldAndSetState(State::kBlocking, next);
    }

    void Wait(Conjury &next) {
        // printf("setting parent of %p to %p\n", &next, this);
        next.parent_conjury_ = this;
        BlockingYield(next);
    }

    Conjury *Parent() {
        return parent_conjury_;
    }

    virtual ~Conjury() = default;

    State GetState() const {
        return state_;
    }

  private:
    void ResumeFrom(Conjury &from) {
        if (state_ == State::kFinished) {
            return;
        }
        // assert(state != State::kRunning);

        parent_conjury_ = &from;

        if (state_ == State::kInitial) {
            state_ = State::kRunning;
            ContextSwitch(func_wrapper_this_, &from.context_, &context_);
        } else {
            state_ = State::kRunning;
            ContextSwitch(nullptr, &from.context_, &context_);
        }
    }

    void YieldAndSetState(State s, Conjury &to) {
        state_ = s;
        to.ResumeFrom(*this);
    }

  protected:
    Stack stack_;
    detail::Context context_;
    State state_ = State::kInitial;

    void *func_wrapper_this_ = nullptr;
    Conjury *parent_conjury_ = nullptr;
};

void End();

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

    G UnsafeGetGen() {
        G val = std::move(gen_store_.value());
        gen_store_.reset();
        return val;
    }

    template <typename U>
    void StoreGen(U &&u) {
        gen_store_.emplace(std::forward<U>(u));
    }

  private:
    std::optional<G> gen_store_;
};

} // namespace conjure

#endif // CONJURE_CONJURY_H_
