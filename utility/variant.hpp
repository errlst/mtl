/*
    参考：
        https://timsong-cpp.github.io/cppwp/n4861/variant
        https://stackoverflow.com/questions/39547777/implementing-stdvariant-converting-constructor-or-how-to-find-first-overloa 选择转换构造函数最适合的类型
        https://zhuanlan.zhihu.com/p/675735999 variant 实现
        https://mpark.github.io/programming/2015/07/07/variant-visitation/  visit 实现
*/
#pragma once
#include "utility.hpp"
#include "tuple.hpp"
#include <experimental/array>

// bad variant access
namespace mtl {
    struct bad_variant_access : public std::exception {};
}  // namespace mtl

// variant size
namespace mtl {
    template <typename... Types>
    struct variant_size<variant<Types...>> : public std::integral_constant<size_t, sizeof...(Types)> {};
}  // namespace mtl

// variant alternative
namespace mtl {
    template <size_t Idx, typename... Types>
    struct variant_alternative<Idx, variant<Types...>> {
        using type = tuple_element_t<Idx, tuple<Types...>>;
    };

    template <typename T, typename... Types>
    constexpr auto holds_alternative(const variant<Types...>&) noexcept -> bool { return type_app_times_v<T, Types...> != 0; }
}  // namespace mtl

// 转换构造函数接受的类型
namespace mtl {
    // 创建重载集，借助重载决策选择最合适的类型
    namespace {
        template <typename T_i>
        using arr = T_i[1];

        template <typename... Types>
        struct overload;

        template <>
        struct overload<> {
            constexpr auto operator()() const -> void;
        };

        template <typename T, typename... Types>
        struct overload<T, Types...> : public overload<Types...> {
            using overload<Types...>::operator();

            constexpr auto operator()(T) const -> T
            requires requires { arr<T>{std::declval<T>()}; };
        };

        // variant<void> 是有效的，但 void 不能作为参数
        template <typename... Types>
        struct overload<void, Types...> : public overload<Types...> {
            using overload<Types...>::operator();

            constexpr auto operator()() const -> void;
        };
    }  // namespace

    // ! _variant_accept_t<int, double, long> 重载决议失败
    template <typename T, typename... Types>
    using _variant_accept_t = decltype(std::declval<overload<Types...>>()(std::declval<T>()));
}  // namespace mtl

// variant storage
namespace mtl {
    template <typename... Types>
    struct _variant_data;

    template <>
    struct _variant_data<> {};

    template <typename T, typename... Types>
    struct _variant_data<T, Types...> {
      public:
        _variant_data() {}
        _variant_data(const _variant_data&) = default;
        _variant_data(_variant_data&&) = default;
        ~_variant_data() noexcept {}

      public:
        constexpr auto operator=(const _variant_data&) -> _variant_data& = default;
        constexpr auto operator=(_variant_data&&) -> _variant_data& = default;

      public:
        union {
            T val;
            _variant_data<Types...> next;
        };
    };

    template <size_t Idx, typename T, typename... Types>
    constexpr auto _variant_data_destroy(_variant_data<T, Types...>& v) {
        if constexpr (Idx == 0) {
            v.val.~T();
        } else {
            _variant_data_destroy<Idx - 1>(v.next);
        }
    }

    // 只进行重新赋值，销毁交给 variant
    template <size_t Idx, typename... Types, typename T>
    constexpr auto _variant_data_assign(_variant_data<Types...>& v, T&& t) {
        if constexpr (Idx == 0) {
            v.val = std::forward<T>(t);
        } else {
            _variant_data_assign<Idx - 1>(v.next, std::forward<T>(t));
        }
    }

    template <size_t Idx, typename... Types, typename... Args>
    constexpr auto _varaint_data_assign(_variant_data<Types...>& v, Args&&... args) {
        if constexpr (Idx == 0) {
            std::construct_at(&v.val, std::forward<Args>(args)...);
        } else {
            _variant_data_assign<Idx - 1>(v.next, std::forward<Args>(args)...);
        }
    }

    template <size_t Idx, typename... Types, typename U, typename... Args>
    constexpr auto _variant_data_assign(_variant_data<Types...>& v, std::initializer_list<U> lst, Args&&... args) {
        if constexpr (Idx == 0) {
            std::construct_at(&v.val, lst, std::forward<Args>(args)...);
        } else {
            _variant_data_assign(v.next, lst, std::forward<Args>(args)...);
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_data_get(_variant_data<Types...>& v) -> nth_type_t<Idx, Types...>& {
        if constexpr (Idx == 0) {
            return v.val;
        } else {
            return _variant_data_get<Idx - 1>(v.next);
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_data_get(const _variant_data<Types...>& v) -> const nth_type_t<Idx, Types...>& {
        if constexpr (Idx == 0) {
            return v.val;
        } else {
            return _variant_data_get<Idx - 1>(v.next);
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_data_get(_variant_data<Types...>&& v) -> nth_type_t<Idx, Types...>&& {
        if constexpr (Idx == 0) {
            return std::move(v.val);
        } else {
            return std::move(_variant_data_get<Idx - 1>(v.next));
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_data_get(const _variant_data<Types...>&& v) -> const nth_type_t<Idx, Types...>&& {
        if constexpr (Idx == 0) {
            return std::move(v.val);
        } else {
            return std::move(_variant_data_get<Idx - 1>(v.next));
        }
    }

    enum class _variant_data_comp_t {
        less,
        equal,
        greater
    };

    constexpr auto _variant_data_comp(size_t idx, const _variant_data<>& lhs, const _variant_data<>& rhs) { return _variant_data_comp_t::equal; }

    template <typename... Types>
    constexpr auto _variant_data_comp(size_t idx, const _variant_data<Types...>& lhs, const _variant_data<Types...>& rhs) -> _variant_data_comp_t {
        if (idx == 0) {
            if (lhs.val > rhs.val) {
                return _variant_data_comp_t::greater;
            } else if (lhs.val == rhs.val) {
                return _variant_data_comp_t::equal;
            } else {
                return _variant_data_comp_t::less;
            }
        }
        return _variant_data_comp(idx - 1, lhs.next, rhs.next);
    }

    constexpr auto _variant_data_tway_comp(size_t idx, const _variant_data<>& lhs, const _variant_data<>& rhs) { return std::strong_ordering::equal; }

    template <typename... Types>
    constexpr auto _variant_data_tway_comp(size_t idx, const _variant_data<Types...>& lhs, const _variant_data<Types...>& rhs)
        -> std::common_comparison_category_t<std::compare_three_way_result_t<Types>...> {
        if (idx == 0) {
            return lhs.val <=> rhs.val;
        }
        return _variant_data_tway_comp(idx - 1, lhs.next, rhs.next);
    }
}  // namespace mtl

// get
namespace mtl {
    template <size_t Idx, typename... Types>
    constexpr auto get(variant<Types...>& v) -> nth_type_t<Idx, Types...>& { return _variant_data_get<Idx>(v.m_data); }

    template <size_t Idx, typename... Types>
    constexpr auto get(const variant<Types...>& v) -> const nth_type_t<Idx, Types...>& { return _variant_data_get<Idx>(v.m_data); }

    template <size_t Idx, typename... Types>
    constexpr auto get(variant<Types...>&& v) -> nth_type_t<Idx, Types...>&& { return std::move(_variant_data_get<Idx>(v.m_data)); }

    template <size_t Idx, typename... Types>
    constexpr auto get(const variant<Types...>&& v) -> const nth_type_t<Idx, Types...>&& { return std::move(_variant_data_get<Idx>(v.m_data)); }

    template <typename T, typename... Types, size_t Idx = type_idx_v<T, Types...>>
    constexpr auto get(variant<Types...>& v) -> T& { return get(v); }

    template <typename T, typename... Types, size_t Idx = type_idx_v<T, Types...>>
    constexpr auto get(const variant<Types...>& v) -> const T& { return get(v); }

    template <typename T, typename... Types, size_t Idx = type_idx_v<T, Types...>>
    constexpr auto get(variant<Types...>&& v) -> T&& { return std::move(get(v)); }

    template <typename T, typename... Types, size_t Idx = type_idx_v<T, Types...>>
    constexpr auto get(const variant<Types...>&& v) -> const T&& { return std::move(get(v)); }
}  // namespace mtl

// variant
namespace mtl {
    template <typename... Types>
    class variant {
      private:
        using _variant_destroy_t = void (*)(void*);

        // 构造
      public:
        constexpr variant() = default;

        constexpr variant(const variant&) = default;

        constexpr variant(variant&&) = default;

        template <typename T, typename Ti = _variant_accept_t<T, Types...>, size_t Idx = type_idx_v<Ti, Types...>>
        requires(sizeof...(Types) > 0 && !std::is_same_v<std::remove_cvref_t<T>, variant>)
        constexpr variant(T&& t)
            : m_idx(Idx) { _variant_data_assign<Idx>(m_data, std::forward<T>(t)); }

        template <typename T, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        requires(std::is_constructible_v<T, Args...>)
        constexpr explicit variant(in_place_type_t<T>, Args&&... args)
            : m_idx(Idx) { _variant_data_assign<Idx>(m_data, std::forward<Args>(args)...); }

        template <typename T, typename U, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        requires(std::is_constructible_v<T, std::initializer_list<U>, Args...>)
        constexpr explicit variant(in_place_type_t<T>, std::initializer_list<U> lst, Args&&... args)
            : m_idx(Idx) { _variant_data_assign<Idx>(m_data, lst, std::forward<Args>(args)...); }

        template <size_t Idx, typename... Args>
        requires(Idx < sizeof...(Types))
        constexpr explicit variant(in_place_index_t<Idx>, Args&&... args)
            : m_idx(Idx) { _variant_data_assign<Idx>(m_data, std::forward<Args>(args)...); }

        template <size_t Idx, typename U, typename... Args>
        requires(Idx < sizeof...(Types))
        constexpr explicit variant(in_place_index_t<Idx>, std::initializer_list<U> lst, Args&&... args)
            : m_idx(Idx) { _variant_data_assign<Idx>(m_data, lst, std::forward<Args>(args)...); }

        // 析构
      public:
        constexpr ~variant() { _variant_destroy(); }

        // assignment
      public:
        // * operator=(variant) 的实现有所简化，不完全符合标准
        constexpr auto operator=(const variant& v) -> variant& {
            if (!valueless_by_exception() && !v.valueless_by_exception()) {
                _variant_destroy();
                if (!v.valueless_by_exception()) {
                    m_data = v.m_data;
                    m_idx = v.m_idx;
                }
            }
            return *this;
        }

        constexpr auto operator=(variant&& v) -> variant& {
            if (!valueless_by_exception() && !v.valueless_by_exception()) {
                _variant_destroy();
                if (!v.valueless_by_exception()) {
                    m_data = std::move(v.m_data);
                    m_idx = v.m_idx;
                }
            }
            return *this;
        }

        template <typename T, typename Ti = _variant_accept_t<T, Types...>, size_t Idx = type_idx_v<Ti, Types...>>
        requires(!std::is_same_v<T, variant> && std::is_constructible_v<Ti, T>)
        constexpr auto operator=(T&& t) -> variant& {
            emplace(t);
            return *this;
        }

        // emplace
      public:
        template <size_t Idx, typename... Args>
        constexpr auto emplace(Args&&... args) -> nth_type_t<Idx, Types...>& {
            _variant_destroy();
            _variant_data_assign<Idx>(m_data, std::forward<Args>(args)...);
            m_idx = Idx;
            return get<Idx>(*this);
        }

        template <size_t Idx, typename U, typename... Args>
        constexpr auto empalce(std::initializer_list<U> lst, Args&&... args) -> nth_type_t<Idx, Types...>& {
            _variant_destroy();
            _variant_data_assign<Idx>(m_data, lst, std::forward<Args>(args)...);
            m_idx = Idx;
            return get<Idx>(*this);
        }

        template <typename T, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        constexpr auto emplace(Args&&... args) -> T& { return emplace<Idx>(std::forward<Args>(args)...); }

        template <typename T, typename U, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        constexpr auto empalce(std::initializer_list<U> lst, Args&&... args) { return emplace<Idx>(lst, std::forward<Args>(args)...); }

        // value state
      public:
        constexpr auto valueless_by_exception() const noexcept -> bool { return m_idx == variant_npos; }

        constexpr auto index() const noexcept -> size_t { return m_idx; }

        // 销毁函数
      private:
        template <typename T>
        static constexpr auto _variant_destroy_func(void* ptr) {
            auto& v = *reinterpret_cast<_variant_data<Types...>*>(ptr);
            _variant_data_destroy<type_idx_v<T, Types...>>(v);
        }

        constexpr auto _variant_destroy() {
            if (!valueless_by_exception()) {
                destroy_funcs[m_idx](&m_data);
                m_idx = variant_npos;
            }
        }

      public:
        size_t m_idx = variant_npos;
        _variant_data<Types...> m_data;
        constexpr static _variant_destroy_t destroy_funcs[] = {_variant_destroy_func<Types>...};
        constexpr static auto is_trivaially_destructible = (std::is_trivially_destructible_v<Types> && ...);
    };
};  // namespace mtl

// relational operators
namespace mtl {
    template <typename... Types>
    constexpr auto operator==(const variant<Types...>& lhs, const variant<Types...>& rhs) -> bool {
        if (lhs.index() != rhs.index()) {
            return false;
        } else if (lhs.valueless_by_exception()) {
            return true;
        }
        return _variant_data_comp(lhs.index(), lhs.m_data, rhs.m_data) == _variant_data_comp_t::equal;
    }

    template <typename... Types>
    constexpr auto operator!=(const variant<Types...>& lhs, const variant<Types...>& rhs) -> bool { return !(lhs == rhs); }

    template <typename... Types>
    constexpr auto operator<(const variant<Types...>& lhs, const variant<Types...>& rhs) -> bool {
        if (rhs.valueless_by_exception()) {
            return false;
        } else if (lhs.valueless_by_exception()) {
            return true;
        } else if (lhs.index() < rhs.index()) {
            return true;
        } else if (lhs.index() > rhs.index()) {
            return false;
        }
        return _variant_data_comp(lhs.index(), lhs.m_data, rhs.m_data) == _variant_data_comp_t::less;
    }

    // 简化实现
    template <typename... Types>
    constexpr auto operator>(const variant<Types...>& lhs, const variant<Types...>& rhs) -> bool { return !(lhs < rhs) && !(lhs == rhs); }

    template <typename... Types>
    constexpr auto operator<=(const variant<Types...>& lhs, const variant<Types...>& rhs) -> bool { return (lhs < rhs) || (lhs == rhs); }

    template <typename... Types>
    constexpr auto operator>=(const variant<Types...>& lhs, const variant<Types...>& rhs) -> bool { return (lhs > rhs) || (lhs == rhs); }

    template <typename... Types>
    constexpr auto operator<=>(const variant<Types...>& lhs, const variant<Types...>& rhs)
        -> std::common_comparison_category_t<std::compare_three_way_result_t<Types>...> {
        if (lhs.valueless_by_exception() && rhs.valueless_by_exception()) {
            return std::strong_ordering::equal;
        } else if (lhs.valueless_by_exception()) {
            return std::strong_ordering::less;
        } else if (rhs.valueless_by_exception()) {
            return std::strong_ordering::greater;
        }
        if (auto _ = lhs.index() <=> rhs.index(); _ != 0) {
            return _;
        }
        return _variant_data_tway_comp(lhs.index(), lhs.m_data, rhs.m_data);
    }
}  // namespace mtl

// visit
namespace mtl {
    /*
    visit 实现思路：
        对于 variant<T1, T2>，生成两个对应的分配函数，并存入数组。
            生成 dispatcher<T1> dispatcher<T2>
                dispatchers[0] = dispatcher<T1>
                dispatchers[1] = dispatcher<T2>

        对于 variant<T1,T2> 和 variant<U1,U2>，生成 2x2=4个对应的分配函数，并存入数组。
            生成 dispatcher<T1,U1> dispatcher<T1,U2> dispatcher<T2,U1> dispatcher<T2,U2>
                dispatchers[0][0] = dispatcher<T1,U1>
                dispatchers[0][1] = dispatcher<T1,U2>
                dispatchers[1][0] = dispatcher<T2,U1>
                dispatchers[1][1] = dispatcher<T2,U2>
    */
    
    // 生成单个 dispatcher
    template <typename F, typename... Variants, size_t... Idxs>
    constexpr auto _variant_make_dispatcher(index_sequence<Idxs...>) {
        struct _ {
            static constexpr auto dispatch(F&& f, Variants&&... variants) -> decltype(auto) {
                return f(get<Idxs>(std::forward<Variants>(variants))...);
            }
        };
        return &_::dispatch;
    }

    // 生成 dispatchers 数组
    template <typename F,
              typename... Variants,
              size_t... Idxs1,
              size_t... Idxs2,
              typename... Seqs>
    constexpr auto _variant_make_dispatcher(index_sequence<Idxs1...>, index_sequence<Idxs2...>, Seqs... seqs) {
        // 借助标准库实现创建多维数组（懒得自己写了（可能写不出来
        return std::experimental::make_array(
            _variant_make_dispatcher<F, Variants...>(index_sequence<Idxs1..., Idxs2>{}, seqs...)...);
    }

    // 封装 make_dispatcher
    template <typename F, typename... Variants>
    constexpr auto _variant_make_dispatchers() {
        return _variant_make_dispatcher<F, Variants...>(
            index_sequence<>{},  // 传递空序列，因此不需要对只有一个 variant 的情况进行特殊处理
            make_index_sequence<variant_size_v<std::decay_t<Variants>>>()...);
    }

    // 搜索 dispatcher
    template <typename T>
    constexpr auto _variant_get_dispatch(T&& t) { return std::forward<T>(t); }

    template <typename T, typename... Idxs>
    constexpr auto _variant_get_dispatch(T&& t, size_t idx, Idxs... idxs) {
        return _variant_get_dispatch(std::forward<T>(t)[idx], idxs...);
    }

    // c++2b 才支持在函数内部创建 static constexpr 变量
    template <typename F, typename... Variants>
    static constexpr auto _variant_dispatchers = _variant_make_dispatchers<F&&, Variants&&...>();

    template <typename F, typename... Variants>
    constexpr auto visit(F&& f, Variants&&... variants) {
        auto dispatcher = _variant_get_dispatch(_variant_dispatchers<F, Variants...>, variants.index()...);
        return dispatcher(std::forward<F>(f), std::forward<Variants>(variants)...);
    }
}  // namespace mtl