/*
    参考：
        https://timsong-cpp.github.io/cppwp/n4861/variant
*/
#pragma once
#include "utility.hpp"

// 获取所有类型中尺寸最大类型
namespace {
    template <typename T, typename... Types>
    struct max_type {
        using type = max_type<T, typename max_type<Types...>::type>::type;
    };

    template <typename T, typename U>
    requires(sizeof(T) >= sizeof(U))
    struct max_type<T, U> {
        using type = T;
    };

    template <typename T, typename U>
    requires(sizeof(T) < sizeof(U))
    struct max_type<T, U> {
        using type = U;
    };

    template <typename T>
    struct max_type<T> {
        using type = T;
    };

    template <typename... Types>
    using max_type_t = max_type<Types...>::type;
}  // namespace

// variant
namespace mtl {
    template <typename... Types>
    requires(sizeof...(Types) >= 1)
    class variant<Types...> {
      private:
        max_type_t<Types...> m_val;
    };
}  // namespace mtl