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
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
};

struct Context {
    Context(void *stack_ptr, void *return_addr)
        : stack_ptr(stack_ptr), return_addr(return_addr) {}
    detail::CalleeSavedRegs registers;
    void *stack_ptr;
    void *return_addr;
};

} // namespace detail

template <typename F, typename... Args>
struct RoutineWrapper {
    using CallerT = void (*)(RoutineWrapper *);

    template <typename... FwdArgs>
    RoutineWrapper(F f, FwdArgs &&... args)
        : f(std::move(f)),
          args(std::make_tuple(std::forward<FwdArgs>(args)...)) {}
    F f;
    std::tuple<Args...> args;
};

template <typename F, typename... Args>
void CallWrapper(RoutineWrapper<F, Args...> *this_) {
    int a;
    printf("stack approximate: %p\n", &a);
    printf("calling wrapper with this %p, f: %p\n", this_, this_->f);
    std::apply(this_->f, std::move(this_->args));
}

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
        using WrapperT = RoutineWrapper<F, Args...>;

        template <typename... FwdArgs>
        WrappedFunc(F f, FwdArgs &&... args)
            : wrapper(std::move(f), std::forward<FwdArgs>(args)...) {}

        virtual void SwitchAndCall(detail::Context &from, detail::Context &to) {
            to.return_addr =
                (void *)(static_cast<typename WrapperT::CallerT>(&CallWrapper));
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
    Conjure(F f, Args &&... args) : Conjure() {
        using WrappedT = WrappedFunc<F, std::decay_t<Args>...>;
        func_ = std::make_unique<WrappedT>(
            std::move(f), std::forward<Args>(args)...);
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
        return (addr + 15) / 16 * 16 + 8;
    }

    template <typename F, typename... Args>
    Conjure *NewRoutine(const Config &config, F f, Args &&... args) {
        Conjure *c = routines_
                         .emplace_back(std::make_unique<Conjure>(
                             std::move(f), std::forward<Args>(args)...))
                         .get();
        uint64_t addr = (uint64_t)malloc(config.stack_size);
        c->context.stack_ptr = (void *)(Align(addr));
        printf("allocating stack: %p\n", c->context.stack_ptr);
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
