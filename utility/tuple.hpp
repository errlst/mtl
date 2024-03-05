/*
    参考:
        https://timsong-cpp.github.io/cppwp/n4861/tuple
        https://zhuanlan.zhihu.com/p/143715615                          tuple 实现
        https://www.zhihu.com/question/524545946/answer/2412805155      tuple_cat 实现

    未实现用户自定义分配器相关内容
*/
#pragma once
#include "utility.hpp"

// 获取第一个类型
namespace {
    template <typename T, typename... Types>
    struct first {
        using type = T;
    };
    template <typename... Types>
    using first_t = first<Types...>::type;
}  // namespace

// 类型出现的次数
namespace {
    template <typename TD, typename T, typename... Types>
    struct type_app_times {
        constexpr static size_t times = type_app_times<TD, Types...>::times + std::is_same_v<TD, T>;
    };

    template <typename TD, typename T>
    struct type_app_times<TD, T> {
        constexpr static size_t times = std::is_same_v<TD, T>;
    };

    template <typename TD, typename... Types>
    constexpr static bool type_unique_v = (type_app_times<TD, Types...>::times == 1);
}  // namespace

// tuple
namespace mtl {
    template <>
    class tuple<> {
      public:
        constexpr auto swap(tuple&) noexcept {}
    };

    template <typename T, typename... Types>
    class tuple<T, Types...> : public tuple<Types...> {
        using base = tuple<Types...>;

        template <typename... UTypes>
        friend class tuple;

      public:  // 构造
        constexpr explicit(std::is_default_constructible_v<T>)
            tuple()
        requires(std::is_default_constructible_v<T>)
        = default;

        constexpr explicit(!std::conjunction_v<std::is_convertible<const T&, T>>)
            tuple(const T& arg, const Types&... args)
        requires(std::is_copy_constructible_v<T>)
            : base(args...), m_ele(arg) {}

        template <typename U, typename... UTypes>
        requires(std::is_convertible_v<U, T>)
        constexpr explicit(!std::conjunction_v<std::is_convertible<U, T>>)
            tuple(U&& arg, UTypes&&... args)
            : base(std::forward<UTypes>(args)...), m_ele(std::forward<U>(arg)) {}

        tuple(const tuple&)
        requires(std::is_copy_constructible_v<T>)
        = default;

        tuple(tuple&&)
        requires(std::is_move_constructible_v<T>)
        = default;

        template <typename U, typename... UTypes>
        requires(sizeof...(UTypes) == sizeof...(Types) && std::is_constructible_v<T, const U&>)
        constexpr explicit(!std::conjunction_v<std::is_convertible<const U&, T>>)
            tuple(const tuple<U, UTypes...>& t)  // 将 t 的基类部分传给基类构造器
            : base(dynamic_cast<const tuple<UTypes...>&>(t)), m_ele(t.m_ele) {}

        template <typename U, typename... UTypes>
        requires(sizeof...(UTypes) == sizeof...(Types) && std::is_constructible_v<T, U>)
        constexpr explicit(!std::conjunction_v<std::is_convertible<U, T>>)
            tuple(tuple<U, UTypes...>&& t)  // 同上
            : base(dynamic_cast<tuple<UTypes...>&&>(t)), m_ele(std::forward<U>(t.m_ele)) {}

        template <typename U1, typename U2>
        requires(sizeof...(Types) == 1 && std::is_constructible_v<T, const U1&>)
        constexpr explicit(!std::is_convertible_v<const U1&, T>)
            tuple(const pair<U1, U2>& p)
            : base(p.second), m_ele(p.first) {}

        template <typename U1, typename U2>
        requires(sizeof...(Types) == 1 && std::is_constructible_v<T, U1>)
        constexpr explicit(!std::is_convertible_v<U1, T>)
            tuple(pair<U1, U2>&& p)
            : base(std::forward<U2>(p.second)), m_ele(std::forward<U1>(p.first)) {}

      public:  // 赋值
        constexpr auto operator=(const tuple& t) -> tuple&
        requires(std::is_copy_assignable_v<T>)
        {
            m_ele = t.m_ele;
            dynamic_cast<base&>(*this) = dynamic_cast<const base&>(t);
            return *this;
        }

        constexpr auto operator=(tuple&& t) noexcept(std::is_nothrow_move_assignable_v<T>) -> tuple&
        requires(std::is_move_assignable_v<T>)
        {
            m_ele = std::move(t.m_ele);
            dynamic_cast<base&>(*this) = dynamic_cast<base&&>(t);
            return *this;
        }

        template <typename U, typename... UTypes>
        requires(sizeof...(Types) == sizeof...(UTypes) && std::is_assignable_v<T&, const U&>)
        constexpr auto operator=(const tuple<U, UTypes...>& t) -> tuple& {
            m_ele = t.m_ele;
            dynamic_cast<base&>(*this) = dynamic_cast<const tuple<UTypes...>&>(t);
            return *this;
        }

        template <typename U, typename... UTypes>
        requires(sizeof...(Types) == sizeof...(UTypes) && std::is_assignable_v<T&, U>)
        constexpr auto operator=(tuple<U, UTypes...>&& t) -> tuple& {
            m_ele = std::forward<U>(t.m_ele);
            dynamic_cast<base&>(*this) = dynamic_cast<tuple<UTypes...>&&>(t);
            return *this;
        }

        template <typename U1, typename U2>
        requires(sizeof...(Types) == 1 && std::is_assignable_v<T&, const U1&> && std::is_assignable_v<first_t<Types...>, const U2&>)
        constexpr auto operator=(const pair<U1, U2>& p) -> tuple& {
            m_ele = p.first;
            dynamic_cast<base&>(*this).m_ele = p.second;
            return *this;
        }

        template <typename U1, typename U2>
        requires(sizeof...(Types) == 1 && std::is_assignable_v<T&, U1> && std::is_assignable_v<first_t<Types...>, U2>)
        constexpr auto operator=(pair<U1, U2>&& p) -> tuple& {
            m_ele = std::forward<U1>(p.first);
            dynamic_cast<base&>(*this).m_ele = std::forward<U2>(p.second);
            return *this;
        }

      public:  // 交换
        constexpr auto swap(tuple& t) noexcept(std::is_nothrow_swappable_v<T>) -> void {
            std::swap(m_ele, t.m_ele);
            std::swap(dynamic_cast<base&>(*this), dynamic_cast<base&>(t));
        }

        // TODO 不会声明 get 的友元，暂时声明为 public
      public:
        T m_ele;
    };

    template <typename... Types>
    tuple(Types...) -> tuple<Types...>;

    template <typename T1, typename T2>
    tuple(pair<T1, T2>) -> tuple<T1, T2>;
}  // namespace mtl

// relational operator
namespace mtl {
    template <typename LT, typename... LTypes, typename RT, typename... RTypes>
    constexpr auto operator==(const tuple<LT, LTypes...>& lhs, const tuple<RT, RTypes...>& rhs) -> bool {
        if (!std::is_same_v<LT, RT> || sizeof...(LTypes) != sizeof...(RTypes) || lhs.m_ele != rhs.m_ele) {
            return false;
        }
        return dynamic_cast<const tuple<LTypes...>&>(lhs) == dynamic_cast<const tuple<RTypes...>&>(rhs);
    }

    template <typename LT, typename... LTypes, typename RT, typename... RTypes>
    constexpr auto operator<=>(const tuple<LT, LTypes...>& lhs, const tuple<RT, RTypes...>& rhs)
        -> std::common_comparison_category<synth_three_way_result<LT, RT>> {
        if (auto _ = synth_three_way(lhs.m_ele, rhs.m_ele); _ != 0) {
            return _;
        }
        return dynamic_cast<const tuple<LTypes...>&>(lhs) <=> dynamic_cast<const tuple<RTypes...>&>(rhs);
    }
}  // namespace mtl

// tuple size
namespace mtl {
    template <typename... Types>
    struct tuple_size<tuple<Types...>> : public std::integral_constant<size_t, sizeof...(Types)> {};
}  // namespace mtl

// tuple element
namespace mtl {
    template <size_t Idx, typename T, typename... Types>
    struct tuple_element<Idx, tuple<T, Types...>> : public tuple_element<Idx - 1, tuple<Types...>> {
        static_assert(Idx <= sizeof...(Types), "index out of tuple range");
    };

    template <typename T, typename... Types>  // 折叠到对应节点
    struct tuple_element<0, tuple<T, Types...>> {
        using type = T;
    };
}  // namespace mtl

// get
namespace mtl {
    template <size_t Idx, typename T, typename... Types>
    constexpr auto get(tuple<T, Types...>& t) noexcept -> tuple_element_t<Idx, tuple<T, Types...>>& {
        if constexpr (Idx == 0) {
            return t.m_ele;
        } else {
            return get<Idx - 1>(dynamic_cast<tuple<Types...>&>(t));
        }
    }

    template <size_t Idx, typename T, typename... Types>
    constexpr auto get(tuple<T, Types...>&& t) noexcept -> tuple_element_t<Idx, tuple<T, Types...>>&& {
        if constexpr (Idx == 0) {
            return std::forward<T>(t.m_ele);
        } else {
            return get<Idx - 1>(dynamic_cast<tuple<Types...>&&>(t));
        }
    }

    template <size_t Idx, typename T, typename... Types>
    constexpr auto get(const tuple<T, Types...>& t) noexcept -> const tuple_element_t<Idx, tuple<T, Types...>>& {
        if constexpr (Idx == 0) {
            return t.m_ele;
        } else {
            return get<Idx - 1>(dynamic_cast<const tuple<Types...>&>(t));
        }
    }

    template <size_t Idx, typename T, typename... Types>
    constexpr auto get(const tuple<T, Types...>&& t) noexcept -> const tuple_element_t<Idx, tuple<T, Types...>>&& {
        if constexpr (Idx == 0) {
            return std::forward<T>(t.m_ele);
        } else {
            return get<Idx - 1>(dynamic_cast<const tuple<Types...>&&>(t));
        }
    }

    // 使用单独的递归模板检查是否存在相同元素
    template <typename TD, typename T, typename... Types>
    constexpr auto get(tuple<T, Types...>& t) noexcept -> TD& requires(type_unique_v<TD, T, Types...>) {
        if constexpr (std::is_same_v<TD, T>) {
            return t.m_ele;
        } else {
            return get<TD>(dynamic_cast<tuple<Types...>&>(t));
        }
    }

    template <typename TD, typename T, typename... Types>
    constexpr auto get(tuple<T, Types...>&& t) noexcept -> TD&& requires(type_unique_v<TD, T, Types...>) {
        if constexpr (std::is_same_v<TD, T>) {
            return std::forward<T>(t.m_ele);
        } else {
            return get<TD>(dynamic_cast<tuple<Types...>&&>(t));
        }
    }

    template <typename TD, typename T, typename... Types>
    constexpr auto get(const tuple<T, Types...>& t) noexcept -> const TD& requires(type_unique_v<TD, T, Types...>) {
        if constexpr (std::is_same_v<TD, T>) {
            return t.m_ele;
        } else {
            return get<TD>(dynamic_cast<const tuple<Types...>&>(t));
        }
    }

    template <typename TD, typename T, typename... Types>
    constexpr auto get(const tuple<T, Types...>&& t) noexcept -> const TD&& requires(type_unique_v<TD, T, Types...>) {
        if constexpr (std::is_same_v<TD, T>) {
            return std::forward<T>(t.m_ele);
        } else {
            return get<TD>(dynamic_cast<const tuple<Types...>&&>(t));
        }
    }
}  // namespace mtl

// specialized algorithm
namespace mtl {
    template <typename... Types>
    constexpr auto swap(tuple<Types...>& lhs, tuple<Types...>& rhs) noexcept(noexcept(lhs.swap(rhs))) -> void {
        return lhs.swap(rhs);
    }
}  // namespace mtl

// call function
namespace mtl {
    namespace {
        template <typename F, typename Tup, size_t... Idx>
        constexpr auto apply_impl(F&& f, Tup&& t, index_sequence<Idx...>) -> decltype(auto) {
            return f(get<Idx>(std::forward<Tup>(t))...);
        }
    }  // namespace

    template <typename F, typename Tup>
    constexpr auto apply(F&& f, Tup&& t) -> decltype(auto) {
        return apply_impl(std::forward<F>(f), std::forward<Tup>(t), make_index_sequence<tuple_size_v<std::remove_reference_t<Tup>>>());
    }
}  // namespace mtl

// create function
namespace mtl {
    template <typename... Types>
    constexpr auto make_tuple(Types&&... args) -> tuple<std::unwrap_ref_decay_t<Types>...> { return {std::forward<Types>(args)...}; }

    template <typename... Types>
    constexpr auto forward_as_tuple(Types&&... args) noexcept -> tuple<Types&&...> { return {std::forward<Types>(args)...}; }

    template <typename... Types>
    constexpr auto tie(Types&... args) -> tuple<Types&...> { return {args...}; }

    namespace {
        constexpr auto tuple_cat_impl() -> tuple<> { return {}; }

        template <typename Tup>
        constexpr auto tuple_cat_impl(Tup&& t) -> decltype(auto) { return std::forward<Tup>(t); }

        template <typename LTup, typename RTup>
        constexpr auto tuple_cat_impl(LTup&& l_tup, RTup&& r_tup) -> decltype(auto) {
            // 使用 apply 拆解所有参数
            return apply(
                [&]<typename... LTypes>(LTypes&&... l_args) {
                    return apply(
                        [&]<typename... RTypes>(RTypes&&... r_args) {
                            return tuple<LTypes..., RTypes...>{std::forward<LTypes>(l_args)..., std::forward<RTypes>(r_args)...};
                        },
                        std::forward<RTup>(r_tup));
                },
                std::forward<LTup>(l_tup));
        }

        template <typename LTup, typename RTup, typename... Tups>
        constexpr auto tuple_cat_impl(LTup&& l_tup, RTup&& r_tup, Tups&&... tups) -> decltype(auto) {
            return tuple_cat_impl(tuple_cat_impl(std::forward<LTup>(l_tup), std::forward<RTup>(r_tup)),
                                  std::forward<Tups>(tups)...);
        }
    }  // namespace

    template <typename... Tups>
    constexpr auto tuple_cat(Tups&&... tups) -> decltype(auto) { return tuple_cat_impl(std::forward<Tups>(tups)...); }
}  // namespace mtl