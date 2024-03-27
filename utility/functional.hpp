#pragma once
#include "tuple.hpp"
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
            return f(std::forward<T>(t), std::forward<Args>...);
        }
    }

    template <typename F>
    constexpr auto invoke(F &&f) noexcept(std::is_nothrow_invocable_v<F>) -> std::invoke_result_t<F> { return f(); }
} // namespace mtl

// reference_wrap
namespace mtl {
    template <typename T>
    class reference_wrapper {
      public:
        using type = T;

      private:
        static auto _fun(T &t) noexcept { return std::addressof(t); }
        static auto _fun(T &&) = delete;

        // 构造
      public:
        template <typename U>
            requires(requires { _fun(std::declval<U>()); } &&
                     !std::is_same_v<std::remove_cvref_t<U>, reference_wrapper>)
        constexpr reference_wrapper(U &&u) noexcept(noexcept(_fun(std::declval<U>())))
            : m_ptr(_fun(std::forward<U>(u))) {}

        constexpr reference_wrapper(const reference_wrapper &r) = default;

        // assign
      public:
        constexpr auto operator=(const reference_wrapper &r) noexcept -> reference_wrapper & = default;

        // access
      public:
        constexpr operator T &() const noexcept { return *m_ptr; }

        constexpr auto get() const noexcept -> T & { return *m_ptr; }

        // invoke
      public:
        template <typename... Args>
        constexpr auto operator()(Args &&...args) -> std::invoke_result_t<T &, Args...> {
            return invoke(get(), std::forward<Args>(args)...);
        }

      public:
        T *m_ptr;
    };

    template <typename T>
    reference_wrapper(T &) -> reference_wrapper<T>;
} // namespace mtl

// not_fn
namespace mtl {
    template <typename F>
    class _not_fn_ret {
      public:
        explicit constexpr _not_fn_ret(F &&f) : m_f(std::forward<F>(f)) {}

        _not_fn_ret(const _not_fn_ret &) = default;

        _not_fn_ret(_not_fn_ret &&) = default;

      public:
#define _NOT_FN_OP_INVOKE(_QUAL)                                                                                \
    template <typename... Args>                                                                                 \
    auto operator()(Args &&...args) _QUAL noexcept(std::is_nothrow_invocable_v<std::decay_t<F> _QUAL, Args...>) \
        ->decltype(!std::declval<std::invoke_result_t<std::decay_t<F> _QUAL, Args...>>) {                       \
        return !invoke(std::forward<std::decay_t<F> _QUAL>(m_f), std::forward<Args>(args)...);                  \
    }
        _NOT_FN_OP_INVOKE(&);
        _NOT_FN_OP_INVOKE(const &);
        _NOT_FN_OP_INVOKE(&&);
        _NOT_FN_OP_INVOKE(const &&);
#undef _NOT_FN_OP_INVOKE

      public:
        F m_f;
    };

    template <typename F>
    auto not_fn(F &&f) -> _not_fn_ret<F> {
        return _not_fn_ret{std::forward<F>(f)};
    }
} // namespace mtl

// bind_front
namespace mtl {
    template <typename FD, typename... BoundArgs>
    class _bind_front_ret {
      public:
        template <typename F, typename... Args>
        explicit _bind_front_ret(F &&f, Args &&...args)
            : m_f(std::forward<F>(f)), m_args(std::forward<Args>(args)...) {
            static_assert(sizeof...(BoundArgs) == sizeof...(Args));
        }

      public:
#define _BIND_FRONT_RET_OP_INVOKE(_QUAL)                                                                                                       \
    template <typename... CallArgs>                                                                                                            \
    auto operator()(CallArgs &&...c_args) _QUAL->std::invoke_result_t<FD _QUAL, BoundArgs _QUAL..., CallArgs...> {                             \
        return [&, this]<size_t... Idx>(index_sequence<Idx...>) {                                                                              \
            invoke(std::forward<FD _QUAL>(m_f), get<Idx>(std::forward<decltype(m_args) _QUAL>(m_args))..., std::forward<CallArgs>(c_args)...); \
        }(make_index_sequence<sizeof...(BoundArgs)>());                                                                                        \
    }
        _BIND_FRONT_RET_OP_INVOKE(&);
        _BIND_FRONT_RET_OP_INVOKE(const &);
        _BIND_FRONT_RET_OP_INVOKE(&&);
        _BIND_FRONT_RET_OP_INVOKE(const &&);
#undef _BIND_FRONT_RET_OP_INVOKE

      public:
        FD m_f;
        tuple<BoundArgs...> m_args;
    };

    template <typename F, typename... Args>
        requires(std::is_constructible_v<std::decay_t<F>, F> &&
                 std::is_move_constructible_v<std::decay_t<F>> &&
                 (std::is_constructible_v<std::decay_t<Args>, Args> && ...) &&
                 (std::is_move_constructible_v<std::decay_t<Args>> && ...))
    constexpr auto bind_front(F &&f, Args &&...args) -> decltype(auto) {
        return _bind_front_ret<std::decay_t<F>, std::decay_t<Args>...>{std::forward<F>(f), std::forward<Args>(args)...};
    }
} // namespace mtl

// mem_fn
namespace mtl{
    
}