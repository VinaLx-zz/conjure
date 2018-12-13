#ifndef CONJURE_FUNCTION_WRAPPER_H_
#define CONJURE_FUNCTION_WRAPPER_H_

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace conjure {

struct Void {};

template <typename T>
struct NormalizeVoid {
    using Type = T;
};

template <>
struct NormalizeVoid<void> {
    using Type = Void;
};

template <typename T>
using NormalizeVoidT = typename NormalizeVoid<T>::Type;

template <typename F, typename... Args>
struct FunctionWrapper;

template <typename F, typename... Args>
void CallWrapper(FunctionWrapper<F, Args...> *this_);

template <typename F, typename... Args>
struct FunctionWrapper {
    using CallerT = void (*)(FunctionWrapper *);
    using ResultT = NormalizeVoidT<std::invoke_result_t<F, Args...>>;

    friend void CallWrapper(FunctionWrapper *this_);
    template <typename... FwdArgs>
    FunctionWrapper(F f, FwdArgs &&... args)
        : f_(std::move(f)),
          args_(std::make_tuple(std::forward<FwdArgs>(args)...)) {}

    static void *CallAddress() {
        return (void *)static_cast<CallerT>(&CallWrapper);
    }

    F f_;
    std::tuple<Args...> args_;
    std::optional<ResultT> result_;
};

template <typename F, typename... FwdArgs>
using WrapperT = FunctionWrapper<F, std::decay_t<FwdArgs>...>;

template <typename F, typename... FwdArgs>
WrapperT<F, FwdArgs...> WrapCall(F f, FwdArgs &&... args) {
    return WrapperT<F, FwdArgs...>(
        std::move(f), std::forward<FwdArgs>(args)...);
}

template <typename F, typename... Args>
void CallWrapper(FunctionWrapper<F, Args...> *this_) {
    if constexpr (std::is_same_v<
                      typename FunctionWrapper<F, Args...>::ResultT, Void>) {
        std::apply(this_->f_, std::move(this_->args_));
        this_->result_ = Void();
    } else {
        this_->result_ = std::apply(this_->f_, std::move(this_->args_));
    }
}

} // namespace conjure

#endif // CONJURE_FUNCTION_WRAPPER_H_
