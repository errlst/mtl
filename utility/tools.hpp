/*
    一些通用工具，与标准无关
*/
#pragma once
#include <cstdint>  // IWYU pragma: keep
#include <iostream> // IWYU pragma: keep
#include <numeric>  // IWYU pragma: keep

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
    } // namespace

    template <typename TD, typename... Types>
    constexpr static size_t type_app_times_v = type_app_times<TD, Types...>::value;

    template <typename TD, typename... Types>
    constexpr static bool type_app_unique = (type_app_times_v<TD, Types...> == 1);
} // namespace mtl

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
} // namespace mtl

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
} // namespace mtl

// 可隐式默认构造
namespace mtl {
    template <typename T>
    concept is_implicitly_default_constructible = requires { std::add_const_t<std::add_lvalue_reference_t<T>>{}; };
} // namespace mtl

// 完整类型
namespace mtl {
    template <typename T>
    constexpr bool is_complete = (!std::is_void_v<T>)&&(sizeof(T));
}

// 类型是否是特化
namespace mtl {
    template <template <typename...> typename Temp, template <typename...> typename Spec, typename... Types>
    constexpr auto is_specialization(Spec<Types...>) -> std::conditional_t<std::is_same_v<Temp<Types...>, Spec<Types...>>, std::true_type, std::false_type>;

    template <typename Temp, typename Spec>
    constexpr auto is_specialization(Spec) -> std::false_type;

    template <template <typename...> typename Temp, typename Spec>
    constexpr auto is_specialization(Spec) -> std::false_type;

    template <typename Temp, template <typename...> typename Spec, typename... Types>
    constexpr auto is_specialization(Spec<Types...>) -> std::false_type;
} // namespace mtl
