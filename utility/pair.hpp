/*
    https://timsong-cpp.github.io/cppwp/n4861/pairs
*/
#pragma once
#include "utility.hpp"

// pair
namespace mtl {
    template <typename T1, typename T2>
    struct pair {
      public:
        using first_type = T1;
        using second_type = T2;

      public:  // 构造
        pair(const pair&) = default;

        pair(pair&&) = default;

        constexpr explicit(!is_implicitly_default_constructible<T1> && !is_implicitly_default_constructible<T2>)
            pair()
        requires(std::is_default_constructible_v<T1> &&
                 std::is_default_constructible_v<T2>)
            : first{}, second{} {}

        constexpr explicit(!std::is_convertible_v<const T1&, T1> ||
                           !std::is_convertible_v<const T2&, T2>)
            pair(const T1& _1, const T2& _2)
        requires(std::is_copy_constructible_v<T1> &&
                 std::is_copy_constructible_v<T2>)
            : first{_1}, second{_2} {}

        template <typename U1 = T1, typename U2 = T2>
        requires std::is_constructible_v<T1, U1> && std::is_constructible_v<T2, U2>
        constexpr explicit(!std::is_convertible_v<U1, T1> ||
                           !std::is_convertible_v<U2, T2>)
            pair(U1&& _1, U2&& _2)
            : first(std::forward<U1>(_1)), second(std::forward<U2>(_2)) {}

#define __explicit(T1, U1, T2, U2) explicit(!std::is_convertible_v<T1, U1> || !std::is_convertible_v<T2, U2>)
#define __requires(T1, U1, T2, U2) requires std::is_constructible_v<T1, U1> && std::is_constructible_v<T2, U2>

        template <typename U1, typename U2>
        __requires(T1, const U1&, T2, const U2&) constexpr __explicit(T1, const U1&, T2, const U2&)
            pair(const pair<U1, U2>& p)
            : first(p.first), second(p.second) {}

        template <typename U1, typename U2>
        __requires(T1, U1&&, T2, U2&&) constexpr __explicit(T1, U1&&, T2, U2&&)
            pair(pair<U1, U2>&& p)
            : first(std::forward<U1>(p.first)), second(std::forward<U2>(p.second)) {}

#undef __explicit
#undef __requires

      private:
        template <typename... Types1, size_t... Idx1, typename... Types2, size_t... Idx2>
        constexpr pair(tuple<Types1...>& args1, index_sequence<Idx1...>, tuple<Types2...>& args2, index_sequence<Idx2...>)
            : first(std::forward<Types1>(get<Idx1>(args1))...),
              second(std::forward<Types2>(get<Idx2>(args2))...) {}

      public:
        template <typename... Types1, typename... Types2>
        requires(std::is_constructible_v<T1, Types1...> && std::is_constructible_v<T2, Types2...>)
        constexpr pair(piecewise_construct_t, tuple<Types1...> args1, tuple<Types2...> args2)
            : pair(args1, make_index_sequence<sizeof...(Types1)>(), args2, make_index_sequence<sizeof...(Types2)>()) {}

      public:  // operator
        constexpr auto operator=(const pair&) -> pair& = delete;

        constexpr auto operator=(const pair& p) -> pair&
        requires std::is_copy_assignable_v<T1> && std::is_copy_assignable_v<T2>
        {
            first = p.first;
            second = p.second;
            return *this;
        }

        template <typename U1, typename U2>
        requires std::is_assignable_v<T1&, const U1&> && std::is_assignable_v<T2&, const U2&>
        constexpr auto operator=(const pair<U1, U2>& p) -> pair& {
            first = p.first;
            second = p.second;
            return *this;
        }

        constexpr auto operator=(pair&& p) -> pair&
        requires std::is_move_assignable_v<T1> && std::is_move_assignable_v<T2>
        {
            first = std::forward<T1>(p.first);
            second = std::forward<T2>(p.second);
            return *this;
        }

        template <typename U1, typename U2>
        requires std::is_assignable_v<T1&, U1> && std::is_assignable_v<T2&, U2>
        constexpr auto operator=(pair<U1, U2>&& p) -> pair& {
            first = std::forward<U1>(p.first);
            second = std::forward<U2>(p.second);
            return *this;
        }

      public:  // swap
        constexpr auto swap(pair& p) noexcept(std::is_nothrow_swappable_v<T1>&& std::is_nothrow_swappable_v<T2>) -> void
        requires(std::is_swappable_v<T1> && std::is_swappable_v<T2>)
        {
            std::swap(first, p.first);
            std::swap(second, p.second);
        }

      public:
        T1 first;
        T2 second;
    };

    // ?? 搞不懂的显示推导指南
    template <typename T1, typename T2>
    pair(T1, T2) -> pair<T1, T2>;

}  // namespace mtl

// relational operator
namespace mtl {
    template <typename T1, typename T2>
    constexpr auto operator==(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) -> bool { return lhs.first == rhs.first && lhs.second == rhs.second; }

    template <typename T1, typename T2>
    constexpr auto operator<=>(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) -> std::common_comparison_category_t<synth_three_way_result<T1, T2>> {
        if (auto _ = synth_three_way(lhs.first, rhs.first); _ != 0) {
            return _;
        }
        return synth_three_way(lhs.second, rhs.second);
    }
}  // namespace mtl

// swap
namespace mtl {
    template <typename T1, typename T2>
    requires(std::is_swappable_v<T1> && std::is_swappable_v<T2>)
    constexpr auto swap(pair<T1, T2>& lhs, pair<T1, T2>& rhs) noexcept(noexcept(lhs.swap(rhs))) -> void { lhs.swap(rhs); }
}  // namespace mtl

// make function
namespace mtl {
    template <typename T1, typename T2>
    constexpr auto make_pair(T1&& arg1, T2&& arg2) -> pair<std::unwrap_ref_decay_t<T1>, std::unwrap_ref_decay_t<T2>> { return {std::forward<T1>(arg1), std::forward<T2>(arg2)}; }
}  // namespace mtl

// tuple size
namespace mtl {
    template <typename T1, typename T2>
    struct tuple_size<pair<T1, T2>> : std::integral_constant<size_t, 2> {};
}  // namespace mtl

// tuple element
namespace mtl {
    template <size_t Idx, typename T1, typename T2>
    requires(Idx == 0 || Idx == 1)
    struct tuple_element<Idx, pair<T1, T2>> {
        using type = std::conditional_t<Idx == 0, T1, T2>;
    };
}  // namespace mtl

// get
namespace mtl {
    template <size_t Idx, typename T1, typename T2>
    requires(Idx == 0 || Idx == 1)
    constexpr auto get(pair<T1, T2>& p) noexcept -> tuple_element_t<Idx, tuple<T1, T2>>& {
        if constexpr (Idx == 0) {
            return p.first;
        } else {
            return p.second;
        }
    }

    template <size_t Idx, typename T1, typename T2>
    requires(Idx == 0 || Idx == 1)
    constexpr auto get(const pair<T1, T2>& p) noexcept -> const tuple_element_t<Idx, tuple<T1, T2>>& {
        if constexpr (Idx == 0) {
            return p.first;
        } else {
            return p.second;
        }
    }

    template <size_t Idx, typename T1, typename T2>
    requires(Idx == 0 || Idx == 1)
    constexpr auto get(pair<T1, T2>&& p) noexcept -> tuple_element_t<Idx, tuple<T1, T2>>&& {
        if constexpr (Idx == 0) {
            return p.first;
        } else {
            return p.second;
        }
    }

    template <size_t Idx, typename T1, typename T2>
    requires(Idx == 0 || Idx == 1)
    constexpr auto get(const pair<T1, T2>&& p) noexcept -> const tuple_element_t<Idx, tuple<T1, T2>>&& {
        if constexpr (Idx == 0) {
            return p.first;
        } else {
            return p.second;
        }
    }

    // 为了简化函数数量，后续 get 未完全遵守标准
    template <typename T, typename T1, typename T2>
    requires(!std::is_same_v<T1, T2> && (std::is_same_v<T, T1> || std::is_same_v<T, T2>))
    constexpr auto get(pair<T1, T2>& p) -> T& {
        if constexpr (std::is_same_v<T, T1>) {
            return p.first;
        } else {
            return p.second;
        }
    }

    template <typename T, typename T1, typename T2>
    requires(!std::is_same_v<T1, T2> && (std::is_same_v<T, T1> || std::is_same_v<T, T2>))
    constexpr auto get(const pair<T1, T2>& p) -> const T& {
        if constexpr (std::is_same_v<T, T1>) {
            return p.first;
        } else {
            return p.second;
        }
    }

    template <typename T, typename T1, typename T2>
    requires(!std::is_same_v<T1, T2> && (std::is_same_v<T, T1> || std::is_same_v<T, T2>))
    constexpr auto get(pair<T1, T2>&& p) -> T&& {
        if constexpr (std::is_same_v<T, T1>) {
            return p.first;
        } else {
            return p.second;
        }
    }

    template <typename T, typename T1, typename T2>
    requires(!std::is_same_v<T1, T2> && (std::is_same_v<T, T1> || std::is_same_v<T, T2>))
    constexpr auto get(const pair<T1, T2>&& p) -> const T&& {
        if constexpr (std::is_same_v<T, T1>) {
            return p.first;
        } else {
            return p.second;
        }
    }
}  // namespace mtl