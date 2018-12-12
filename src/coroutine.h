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
    Context(void *stack_ptr, void *return_addr)
        : stack_ptr(stack_ptr), return_addr(return_addr) {}

    detail::CalleeSavedRegs registers;

    void *stack_ptr = nullptr;
    void *return_addr = nullptr;
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
    struct FuncProxy {
        using Pointer = std::unique_ptr<FuncProxy>;
        virtual void
        SwitchAndCall(detail::Context &from, detail::Context &to) = 0;

        virtual ~FuncProxy() = default;
    };

    template <typename F, typename... Args>
    struct WrappedFunc : FuncProxy {
        using WrapperT = FunctionWrapper<F, Args...>;

        template <typename W>
        WrappedFunc(W &&w) : wrapper(std::forward<W>(w)) {}

        virtual void SwitchAndCall(detail::Context &from, detail::Context &to) {
            printf(
                "target stack: %p, wrapper addr: %p\n", to.stack_ptr, &wrapper);
            ContextSwitch(&wrapper, &from, &to);
        }
        WrapperT wrapper;
    };

  public:
    enum class State { kInitial, kWaiting, kRunning, kFinished };

    using Pointer = std::unique_ptr<Conjure>;

    Conjure() : context(nullptr, nullptr) {}

    template <typename F, typename... Args>
    Conjure(Stack stk, FunctionWrapper<F, Args...> wrapper)
        : stack(std::move(stk)),
          context(stack.stack_start, wrapper.CallAddress()),
          func_(std::make_unique<WrappedFunc<F, Args...>>(std::move(wrapper))) {
        printf("coroutine stack: %p\n", context.stack_ptr);
    }

    void ResumeFrom(Conjure &from) {
        if (state == State::kFinished) {
            return;
        }
        // assert(state != State::kRunning);

        if (state == State::kInitial) {
            state = State::kRunning;
            func_->SwitchAndCall(from.context, context);
        } else {
            state = State::kRunning;
            ContextSwitch(nullptr, &from.context, &context);
        }
    }

    void YieldTo(Conjure &to) {
        to.ResumeFrom(*this);
    }

    Stack stack;
    detail::Context context;
    State state = State::kInitial;

  private:
    FuncProxy::Pointer func_;
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

    static uint64_t Align(uint64_t addr) {
        return addr / 16 * 16 - 8;
    }

    template <typename F, typename... Args>
    Conjure *NewRoutine(const Config &config, F f, Args &&... args) {
        Stack stack(config.stack_size);
        Conjure *c =
            routines_
                .emplace_back(std::make_unique<Conjure>(
                    std::move(stack),
                    WrapCall(std::move(f), std::forward<Args>(args)...)))
                .get();
        parent_map_[c] = active_routine_;
        name_map_[c] = config.routine_name;
        return c;
    }

    void Yield() {
        active_routine_->state = Conjure::State::kWaiting;
        Resume(parent_map_[active_routine_]);
    }

    void Resume(Conjure *co) {
        active_routine_->state = Conjure::State::kWaiting;
        ResumeImpl(co);
    }

    void ResumeImpl(Conjure *co) {
        Conjure *from = active_routine_, *to = co;
        active_routine_ = to;
        printf("%s yield to %s\n", NameOf(from), NameOf(to));
        from->YieldTo(*to);
    }

    void Exit() {
        active_routine_->state = Conjure::State::kFinished;
        printf("%s exitted\n", Name(active_routine_));
        ResumeImpl(parent_map_[active_routine_]);
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
