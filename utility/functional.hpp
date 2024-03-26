#pragma once
#include "utility.hpp"

// invoke
namespace mtl {
    // f 是 T 的成员函数指针
    template <typename Ret, typename Class, typename... Args, typename T>
    constexpr auto _invoke_impl(Ret (Class::*f)(Args...), T &&t, Args &&...args) -> decltype(auto)
        requires(std::is_same_v<Class, std::remove_pointer_t<T>>)
    {
        if constexpr (std::is_base_of_v<T, std::remove_reference_t<decltype(t)>>) {
            return (t.*f)(std::forward<Args>(args)...);
        } else if constexpr (is_specialization<std::reference_wrapper>(std::remove_reference_t<decltype(t)>())) {
            return (t.get().*f)(std::forward<Args>(args)...);
        } else {
            return ((*t).*f)(std::forward<Args>(args)...);
        }
    }

    // f 是 T 的数据成员指针
    template <typename Type, typename Class, typename T>
    constexpr auto _invoke_impl(Type(Class::*f), T &&t) -> decltype(auto) {
        if constexpr (std::is_base_of_v<T, std::remove_reference_t<decltype(t)>>) {
            return t.*f;
        } else if constexpr (is_specialization<std::reference_wrapper>(std::remove_reference_t<decltype(t)>())) {
            return t.get().*f;
        } else {
            return (*t).*f;
        }
    }

    template <typename F, typename T, typename... Args>
    constexpr auto invoke(F &&f, T &&t, Args &&...args) noexcept(std::is_nothrow_invocable_v<F, T, Args...>) -> std::invoke_result_t<F, T, Args...> {
        if constexpr (requires { _invoke_impl(std::forward<F>(f), std::forward<T>(t), std::forward<Args>(args)...); }) {
            return _invoke_impl(std::forward<F>(f), std::forward<T>(t), std::forward<Args>(args)...);
        } else {
            return f(std::forward<Args>...);
        }
    }
} // namespace mtl