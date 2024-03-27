#pragma once
#include "tools.hpp" // IWYU pragma: keep

// piecewise construct
namespace mtl {
    struct piecewise_construct_t {
        explicit piecewise_construct_t() = default;
    };

    constexpr auto piecewise_construct = piecewise_construct_t{};
} // namespace mtl

// inplace
namespace mtl {
    struct in_place_t {
        explicit in_place_t() = default;
    };
    constexpr auto in_place = in_place_t{};

    template <typename T>
    struct in_place_type_t {
        explicit in_place_type_t() = default;
    };

    template <typename T>
    constexpr auto in_place_type = in_place_type_t<T>{};

    template <size_t Idx>
    struct in_place_index_t {
        explicit in_place_index_t() = default;
    };

    template <size_t Idx>
    constexpr auto in_place_index = in_place_index_t<Idx>{};
} // namespace mtl

// synth three way
namespace mtl {
    constexpr auto synth_three_way = []<typename T1, typename T2>(const T1 &lhs, const T2 &rhs) {
        if constexpr (std::three_way_comparable_with<T1, T2>) {
            return lhs <=> rhs;
        } else {
            if (lhs < rhs) {
                return std::weak_ordering::less;
            } else if (lhs > rhs) {
                return std::weak_ordering::greater;
            }
        }
        return std::weak_ordering::equivalent;
    };

    template <typename T1, typename T2>
    using synth_three_way_result = decltype(synth_three_way(std::declval<T1 &>(), std::declval<T2 &>()));
} // namespace mtl

// utilities
namespace mtl {
    template <typename T1, typename T2>
    struct pair;

    template <typename... Types>
    class tuple;

    template <typename T>
    class optional;

    template <typename... Types>
    class variant;

    class any;

    template <size_t N>
    class bitset;

    template <typename T>
    struct default_delete;

    template <typename T>
    struct allocator;

    template <typename T, typename D = default_delete<T>>
    class unique_ptr;

    template <typename T>
    class shared_ptr;

    template <typename T>
    class weak_ptr;

    template <typename F>
    class function;
} // namespace mtl

// tuple size
namespace mtl {
    template <typename>
    struct tuple_size;

    template <typename T>
    struct tuple_size<const T> : public tuple_size<T> {};

    template <typename T>
    inline constexpr auto tuple_size_v = tuple_size<T>::value;
} // namespace mtl

// tuple element
namespace mtl {
    template <size_t Idx, typename T>
    struct tuple_element;

    template <size_t Idx, typename T>
    struct tuple_element<Idx, const T> : public tuple_element<Idx, T> {};

    template <size_t Idx, typename T>
    using tuple_element_t = tuple_element<Idx, T>::type;
} // namespace mtl

// variant size
namespace mtl {
    template <typename T>
    struct variant_size;

    template <typename T>
    struct variant_size<const T> : public variant_size<T> {};

    template <typename T>
    inline constexpr auto variant_size_v = variant_size<T>::value;
} // namespace mtl

// variant alternative
namespace mtl {
    template <size_t Idx, typename T>
    struct variant_alternative;

    template <size_t Idx, typename T>
    struct variant_alternative<Idx, const T> : public variant_alternative<Idx, T> {};

    template <size_t Idx, typename T>
    using variant_alternative_t = variant_alternative<Idx, T>::type;

    inline constexpr size_t variant_npos = -1;
} // namespace mtl

// integer sequence
namespace mtl {
    template <typename T, T... Idx>
    struct integer_sequence {
        using value_type = T;
        static constexpr auto size() -> size_t { return sizeof...(Idx); }
    };

    namespace {
        template <typename T, T N> // N!=0
        struct make_integer_sequence_helper {
            template <T... Idxs>
            constexpr static auto seq_inc(integer_sequence<T, Idxs...>) // 序列后追加一个数字
                -> integer_sequence<T, Idxs..., sizeof...(Idxs)>;

            // 沿着 make_integer_sequence_helper 递归，每次递归使得 N-1，直到 N==0
            // 从递归链返回时，借助 seq_inc，每次在序列后追加一个数字
            // N==0 时，type <=> helper<integer_sequence<T>>::type <=> integer_sequence<T,0>
            // N==1 时，type <=> helper<integer_sequence<T,0>>::type <=> integer_sequence<T,0,1>
            using type = decltype(seq_inc(typename make_integer_sequence_helper<T, N - 1>::type()));
        };

        template <typename T, T N> // N==0
            requires(N == 0)
        struct make_integer_sequence_helper<T, N> {
            using type = integer_sequence<T>;
        };
    } // namespace

    template <typename T, T N>
    using make_integer_sequence = typename make_integer_sequence_helper<T, N>::type;

    template <size_t... Idx>
    using index_sequence = integer_sequence<size_t, Idx...>;

    template <size_t N>
    using make_index_sequence = typename make_integer_sequence_helper<size_t, N>::type;
} // namespace mtl
