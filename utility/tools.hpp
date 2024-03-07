/*
    一些通用工具，与标准无关
*/
#pragma once
#include <iostream>

// 类型出现的次数
namespace mtl {
    namespace {
        template <typename TD, typename T, typename... Types>
        struct type_app_times {
            constexpr static size_t value = type_app_times<TD, Types...>::value + std::is_same_v<TD, T>;
        };

        template <typename TD, typename T>
        struct type_app_times<TD, T> {
            constexpr static size_t value = std::is_same_v<TD, T>;
        };
    }  // namespace

    template <typename TD, typename... Types>
    constexpr static size_t type_app_times_v = type_app_times<TD, Types...>::value;

    template <typename TD, typename... Types>
    constexpr static bool type_app_unique = (type_app_times_v<TD, Types...> == 1);
}  // namespace mtl

// 获取第 N 个类型
namespace mtl {
    template <size_t Idx, typename... Types>
    struct nth_type;

    template <size_t Idx, typename T, typename... Types>
    struct nth_type<Idx, T, Types...> : public nth_type<Idx - 1, Types...> {};

    template <typename T, typename... Types>
    struct nth_type<0, T, Types...> {
        using type = T;
    };

    template <size_t Idx, typename... Types>
    using nth_type_t = nth_type<Idx, Types...>::type;
}  // namespace mtl

// T 所处索引
namespace mtl {
    template <typename TD, size_t Idx, typename T, typename... Types>
    constexpr auto type_idx() -> size_t {
        if constexpr (std::is_same_v<TD, T>) {
            return Idx;
        } else {
            static_assert(sizeof...(Types) > 0, "type invalid");
            return type_idx<TD, Idx + 1, Types...>();
        }
    }

    template <typename TD, typename... Types>
    constexpr auto type_idx_v = type_idx<TD, 0, Types...>();
}  // namespace mtl

// 最大尺寸的类型
namespace mtl {
    namespace {
        template <typename T>
        constexpr auto max_type() -> T;

        template <typename T, typename U>
        constexpr auto max_type() -> std::conditional_t<sizeof(T) >= sizeof(U), T, U>;

        template <typename T, typename... Types>
        requires(sizeof...(Types) >= 2)
        constexpr auto max_type() -> decltype(max_type<T, decltype(max_type<Types...>())>());
    }  // namespace

    template <typename... Types>
    using max_type_t = decltype(max_type<Types...>());
}  // namespace mtl

// 判断是否可隐式默认构造
namespace mtl {
    namespace {
        template <typename T>
        using const_ref_t = std::add_const_t<std::add_lvalue_reference_t<T>>;
    }

    template <typename T>
    concept is_implicitly_default_constructible = requires { const_ref_t<T>{}; };
}  // namespace mtl
