#ifndef CONJURE_COROUTINE_H_
#define CONJURE_COROUTINE_H_

#include <memory>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "./function-wrapper.h"
#include "./stack.h"

namespace conjure::routine::detail {

struct Context;

} // namespace conjure::routine::detail

extern "C" {

void ContextSwitch(
    void *this_, conjure::routine::detail::Context *from,
    conjure::routine::detail::Context *to) asm("ContextSwitch");
}

namespace conjure::routine {

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

class Conjure;

const char *Name(Conjure *c);

class Conjure {
  public:
    enum class State { kInitial, kWaiting, kRunning, kFinished };

    using Pointer = std::unique_ptr<Conjure>;

    Conjure() : context_(nullptr, nullptr) {}

    Conjure(Stack stk) : stack_(std::move(stk)), context_(stack_.stack_start) {
        printf("coroutine stack: %p\n", context_.stack_ptr);
    }

    void ResumeFrom(Conjure &from) {
        if (state_ == State::kFinished) {
            return;
        }
        // assert(state != State::kRunning);

        if (state_ == State::kInitial) {
            state_ = State::kRunning;
            ContextSwitch(func_wrapper_this_, &from.context_, &context_);
        } else {
            state_ = State::kRunning;
            ContextSwitch(nullptr, &from.context_, &context_);
        }
    }

    void YieldAndSetState(State s, Conjure &to) {
        state_ = s;
        to.ResumeFrom(*this);
    }

    State GetState() const {
        return state_;
    }

  protected:
    Stack stack_;
    detail::Context context_;
    State state_ = State::kInitial;

    void *func_wrapper_this_;
};

template <typename Result>
class ConjureClient : public Conjure {
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
    template <typename F, typename... Args>
    ConjureClient(Stack stack, FunctionWrapper<F, Args...> wrapper)
        : Conjure(std::move(stack)) {
        auto result_store_impl =
            std::make_unique<ResultStoreImpl<F, Args...>>(std::move(wrapper));
        this->func_wrapper_this_ = &result_store_impl->wrapper;
        this->context_.return_addr = result_store_impl->wrapper.CallAddress();
        result_ = std::move(result_store_impl);
    }

    Result UnsafeGetResult() {
        return result_->Get().value();
    }

  private:
    std::unique_ptr<ResultStore> result_;
};

struct Conjurer {
    Conjurer() {
        routines_.push_back(std::make_unique<Conjure>());
        active_routine_ = routines_.back().get();
        name_map_[active_routine_] = "Main";
    }

    static Conjurer &Instance() {
        static Conjurer conjurer;
        return conjurer;
    }

    template <typename F, typename... Args>
    auto NewRoutine(const Config &config, F f, Args &&... args) {
        using R = typename WrapperT<F, Args...>::ResultT;
        Stack stack(config.stack_size);

        auto co = std::make_unique<ConjureClient<R>>(
            std::move(stack),
            WrapCall(std::move(f), std::forward<Args>(args)...));

        ConjureClient<R> *co_client = co.get();
        routines_.push_back(std::move(co));

        parent_map_[co_client] = active_routine_;
        name_map_[co_client] = config.routine_name;
        return co_client;
    }

    void Yield() {
        Resume(parent_map_[active_routine_]);
    }

    void Resume(Conjure *co) {
        ResumeImpl(Conjure::State::kWaiting, co);
    }

    void ResumeImpl(Conjure::State s, Conjure *co) {
        Conjure *from = active_routine_, *to = co;
        active_routine_ = to;
        printf("%s yield to %s\n", NameOf(from), NameOf(to));
        from->YieldAndSetState(s, *to);
    }

    void Exit() {
        printf("%s exitted\n", Name(active_routine_));
        ResumeImpl(Conjure::State::kFinished, parent_map_[active_routine_]);
    }

    const char *NameOf(Conjure *c) {
        return name_map_[c].data();
    }
    std::unordered_map<Conjure *, std::string> name_map_;

  private:
    Conjure *active_routine_ = nullptr;
    std::unordered_map<Conjure *, Conjure *> parent_map_;
    std::vector<Conjure::Pointer> routines_;
};

const char *Name(Conjure *c) {
    return Conjurer::Instance().NameOf(c);
}

inline void Yield() {
    Conjurer::Instance().Yield();
}

template <typename F, typename... Args>
Conjure *NewRoutine(const Config &config, F f, Args &&... args) {
    return Conjurer::Instance().NewRoutine(
        config, std::move(f), std::forward<Args>(args)...);
}

inline void Return() {
    Conjurer::Instance().Exit();
}

inline void Resume(Conjure *c) {
    Conjurer::Instance().Resume(c);
}

} // namespace conjure::routine

#endif // CONJURE_COROUTINE_H_
