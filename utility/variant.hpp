/*
    参考：
        https://timsong-cpp.github.io/cppwp/n4861/variant
        https://stackoverflow.com/questions/39547777/implementing-stdvariant-converting-constructor-or-how-to-find-first-overloa 选择转换构造函数最适合的类型
        https://zhuanlan.zhihu.com/p/675735999 variant 实现
*/
#pragma once
#include "utility.hpp"
#include "tuple.hpp"

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
    struct _variant_storage;

    template <>
    struct _variant_storage<> {};

    template <typename T, typename... Types>
    struct _variant_storage<T, Types...> {
      public:
        _variant_storage() {}
        _variant_storage(const _variant_storage&) = default;
        _variant_storage(_variant_storage&&) = default;
        ~_variant_storage() noexcept {}

      public:
        constexpr auto operator=(const _variant_storage&) -> _variant_storage& = default;
        constexpr auto operator=(_variant_storage&&) -> _variant_storage& = default;

      public:
        union {
            T val;
            _variant_storage<Types...> next;
        };
    };

    template <size_t Idx, typename T, typename... Types>
    constexpr auto _variant_storage_destroy(_variant_storage<T, Types...>& v) {
        if constexpr (Idx == 0) {
            v.val.~T();
        } else {
            _variant_storage_destroy<Idx - 1>(v.next);
        }
    }

    // 只进行重新赋值，销毁交给 variant
    template <size_t Idx, typename... Types, typename T>
    constexpr auto _variant_storage_assign(_variant_storage<Types...>& v, T&& t) {
        if constexpr (Idx == 0) {
            v.val = std::forward<T>(t);
        } else {
            _variant_storage_assign<Idx - 1>(v.next, std::forward<T>(t));
        }
    }

    template <size_t Idx, typename... Types, typename... Args>
    constexpr auto _variant_storage_assign(_variant_storage<Types...>& v, Args&&... args) {
        if constexpr (Idx == 0) {
            std::construct_at(&v.val, std::forward<Args>(args)...);
        } else {
            _variant_storage_assign<Idx - 1>(v.next, std::forward<Args>(args)...);
        }
    }

    template <size_t Idx, typename... Types, typename U, typename... Args>
    constexpr auto _variant_storage_assign(_variant_storage<Types...>& v, std::initializer_list<U> lst, Args&&... args) {
        if constexpr (Idx == 0) {
            std::construct_at(&v.val, lst, std::forward<Args>(args)...);
        } else {
            _variant_storage_assign(v.next, lst, std::forward<Args>(args)...);
        }
    }

}  // namespace mtl

// get
namespace mtl {
    template <size_t Idx, typename... Types>
    constexpr auto _variant_storage_get(_variant_storage<Types...>& v) -> nth_type_t<Idx, Types...>& {
        if constexpr (Idx == 0) {
            return v.val;
        } else {
            return _variant_storage_get<Idx - 1>(v.next);
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_storage_get(const _variant_storage<Types...>& v) -> const nth_type_t<Idx, Types...>& {
        if constexpr (Idx == 0) {
            return v.val;
        } else {
            return _variant_storage_get<Idx - 1>(v.next);
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_storage_get(_variant_storage<Types...>&& v) -> nth_type_t<Idx, Types...>&& {
        if constexpr (Idx == 0) {
            return std::move(v.val);
        } else {
            return std::move(_variant_storage_get<Idx - 1>(v.next));
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto _variant_storage_get(const _variant_storage<Types...>&& v) -> const nth_type_t<Idx, Types...>&& {
        if constexpr (Idx == 0) {
            return std::move(v.val);
        } else {
            return std::move(_variant_storage_get<Idx - 1>(v.next));
        }
    }

    template <size_t Idx, typename... Types>
    constexpr auto get(variant<Types...>& v) -> nth_type_t<Idx, Types...>& { return _variant_storage_get<Idx>(v.m_storage); }

    template <size_t Idx, typename... Types>
    constexpr auto get(const variant<Types...>& v) -> const nth_type_t<Idx, Types...>& { return _variant_storage_get<Idx>(v.m_storage); }

    template <size_t Idx, typename... Types>
    constexpr auto get(variant<Types...>&& v) -> nth_type_t<Idx, Types...>&& { return std::move(_variant_storage_get<Idx>(v.m_storage)); }

    template <size_t Idx, typename... Types>
    constexpr auto get(const variant<Types...>&& v) -> const nth_type_t<Idx, Types...>&& { return std::move(_variant_storage_get<Idx>(v.m_storage)); }

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
            : m_idx(Idx) { _variant_storage_assign<Idx>(m_storage, std::forward<T>(t)); }

        template <typename T, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        requires(std::is_constructible_v<T, Args...>)
        constexpr explicit variant(in_place_type_t<T>, Args&&... args)
            : m_idx(Idx) { _variant_storage_assign<Idx>(m_storage, std::forward<Args>(args)...); }

        template <typename T, typename U, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        requires(std::is_constructible_v<T, std::initializer_list<U>, Args...>)
        constexpr explicit variant(in_place_type_t<T>, std::initializer_list<U> lst, Args&&... args)
            : m_idx(Idx) { _variant_storage_assign<Idx>(m_storage, lst, std::forward<Args>(args)...); }

        template <size_t Idx, typename... Args>
        requires(Idx < sizeof...(Types))
        constexpr explicit variant(in_place_index_t<Idx>, Args&&... args)
            : m_idx(Idx) { _variant_storage_assign<Idx>(m_storage, std::forward<Args>(args)...); }

        template <size_t Idx, typename U, typename... Args>
        requires(Idx < sizeof...(Types))
        constexpr explicit variant(in_place_index_t<Idx>, std::initializer_list<U> lst, Args&&... args)
            : m_idx(Idx) { _variant_storage_assign<Idx>(m_storage, lst, std::forward<Args>(args)...); }

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
                    m_storage = v.m_storage;
                    m_idx = v.m_idx;
                }
            }
            return *this;
        }

        constexpr auto operator=(variant&& v) -> variant& {
            if (!valueless_by_exception() && !v.valueless_by_exception()) {
                _variant_destroy();
                if (!v.valueless_by_exception()) {
                    m_storage = std::move(v.m_storage);
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
            _variant_storage_assign<Idx>(m_storage, std::forward<Args>(args)...);
            m_idx = Idx;
            return get<Idx>(*this);
        }

        template <size_t Idx, typename U, typename... Args>
        constexpr auto empalce(std::initializer_list<U> lst, Args&&... args) -> nth_type_t<Idx, Types...>& {
            _variant_destroy();
            _variant_storage_assign<Idx>(m_storage, lst, std::forward<Args>(args)...);
            m_idx = Idx;
            return get<Idx>(*this);
        }

        template <typename T, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        constexpr auto emplace(Args&&... args) -> T& { return emplace<Idx>(std::forward<Args>(args)...); }

        template <typename T, typename U, typename... Args, size_t Idx = type_idx_v<T, Types...>>
        constexpr auto empalce(std::initializer_list<U> lst, Args&&... args) { return emplace<Idx>(lst, std::forward<Args>(args)...); }

        // value state
      public:
        constexpr auto valueless_by_exception() const noexcept -> bool { return m_idx != variant_npos; }

        constexpr auto index() const noexcept -> size_t { return m_idx; }

        // 销毁函数
      private:
        template <typename T>
        static constexpr auto _variant_destroy_func(void* ptr) {
            auto& v = *reinterpret_cast<_variant_storage<Types...>*>(ptr);
            _variant_storage_destroy<type_idx_v<T, Types...>>(v);
        }

        constexpr auto _variant_destroy() {
            if (!valueless_by_exception()) {
                destroy_funcs[m_idx](&m_storage);
                m_idx = variant_npos;
            }
        }

      public:
        size_t m_idx = variant_npos;
        _variant_storage<Types...> m_storage;
        constexpr static _variant_destroy_t destroy_funcs[] = {_variant_destroy_func<Types>...};
        constexpr static auto trivaially_destructible = (std::is_trivially_destructible_v<Types> && ...);
    };
};  // namespace mtl

// relational operators
namespace mtl {
    
}