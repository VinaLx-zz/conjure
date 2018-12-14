#ifndef CONJURE_FUNCTION_WRAPPER_H_
#define CONJURE_FUNCTION_WRAPPER_H_

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace conjure {

struct Void {};

namespace detail {

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

} // namespace detail

template <typename F, typename... Args>
struct FunctionWrapper;

template <typename F, typename... Args>
void CallWrapper(FunctionWrapper<F, Args...> *this_);

template <typename F, typename... Args>
struct FunctionWrapper {
    using CallerT = void (*)(FunctionWrapper *);
    using ResultT = detail::NormalizeVoidT<std::invoke_result_t<F, Args...>>;

    friend void CallWrapper(FunctionWrapper *this_);
    template <typename... FwdArgs>
    FunctionWrapper(F f, FwdArgs &&... args)
        : f_(std::move(f)),
          args_(std::make_tuple(std::forward<FwdArgs>(args)...)) {}

    void Call() {
        if constexpr (std::is_same_v<ResultT, Void>) {
            std::apply(f_, std::move(args_));
            result_ = Void();
        } else {
            result_ = std::apply(f_, std::move(args_));
        }
    }

    F f_;
    std::tuple<Args...> args_;
    std::optional<ResultT> result_;
};

template <typename F, typename... FwdArgs>
using WrapperT = FunctionWrapper<F, std::decay_t<FwdArgs>...>;

template <typename F, typename... FwdArgs>
using WrapperResultT = typename WrapperT<F, FwdArgs...>::ResultT;

template <typename F, typename... FwdArgs>
WrapperT<F, FwdArgs...> WrapCall(F f, FwdArgs &&... args) {
    return WrapperT<F, FwdArgs...>(
        std::move(f), std::forward<FwdArgs>(args)...);
}

} // namespace conjure

#endif // CONJURE_FUNCTION_WRAPPER_H_
