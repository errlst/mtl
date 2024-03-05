/*
    参考：
        https://timsong-cpp.github.io/cppwp/n4861/variant
*/
#pragma once
#include "utility.hpp"

// 获取所有类型中尺寸最大类型
namespace mtl {
    template <typename T>
    constexpr auto max_type() -> T;

    template <typename T, typename U>
    requires(sizeof(T) >= sizeof(U))
    constexpr auto max_type() -> T;

    template <typename T, typename U>
    requires(sizeof(T) < sizeof(U))
    constexpr auto max_type() -> U;

    template <typename T, typename... Types>
    requires(sizeof...(Types) >= 2)
    constexpr auto max_type() -> decltype(max_type<T, decltype(max_type<Types...>())>());

    template <typename... Types>
    using max_type_t = decltype(max_type<Types...>());

    template <typename... Types>
    constexpr auto max_type_size = sizeof(max_type_t<Types...>);
}  // namespace mtl

// variant
namespace mtl {
    template <typename... Types>
    requires(sizeof...(Types) >= 1)
    class variant<Types...> {
      private:
        char m_buf[max_type_size<Types...>];  // 栈缓冲区存放有效值
    };
}  // namespace mtl