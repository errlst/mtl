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

// function
namespace mtl {
    class bad_function_call : public std::exception {};

    template <typename R, typename... A>
    class _function_storage {
        using del_type = void (*)(void *);                                         // 删除器
        using cop_type = void (*)(const _function_storage *, _function_storage *); // 拷贝器（拷贝自身给其他对象
        using mov_type = void (*)(_function_storage *, _function_storage *);       // 移动器（移动自身给其他对象
        using ivk_type = R (*)(const _function_storage *, A...);                   // 调用器

      public:
        template <typename F>
            requires(!std::same_as<F, _function_storage>)
        constexpr _function_storage(F f) {
            if constexpr (sizeof(F) <= sizeof(void *)) {
                std::construct_at(reinterpret_cast<F *>(stack_mem), std::forward<F>(f));
                m_del = [](void *mem) { (*reinterpret_cast<F *>(mem)).~F(); };
            } else {
                heap_mem = new F(std::forward<F>(f));
                m_del = [](void *mem) { delete reinterpret_cast<F *>(mem); };
            }
            m_cop = [](const _function_storage *_this, _function_storage *other) {
                other->reset();
                if constexpr (sizeof(F) <= sizeof(void *)) {
                    std::construct_at(reinterpret_cast<F *>(other->stack_mem), *reinterpret_cast<F *>(const_cast<char *>(_this->stack_mem)));
                } else {
                    other->stack_mem = new F(*reinterpret_cast<F *>(_this->heap_mem));
                }
                other->m_cop = _this->m_cop;
                other->m_mov = _this->m_mov;
                other->m_del = _this->m_del;
                other->m_ivk = _this->m_ivk;
                other->m_tinfo = _this->m_tinfo;
            };
            m_mov = [](_function_storage *_this, _function_storage *other) {
                other->reset();
                other->heap_mem = _this->heap_mem;
                other->m_cop = _this->m_cop;
                other->m_mov = _this->m_mov;
                other->m_del = _this->m_del;
                other->m_ivk = _this->m_ivk;
                other->m_tinfo = _this->m_tinfo;
                _this->m_tinfo = nullptr;
            };
            m_ivk = [](const _function_storage *_this, A... args) -> R {
                if constexpr (sizeof(F) <= sizeof(void *)) {
                    return invoke(*reinterpret_cast<F *>(const_cast<char *>(_this->stack_mem)), std::forward<A>(args)...);
                } else {
                    return invoke(reinterpret_cast<F *>(_this->heap_mem), std::forward<A>(args)...);
                }
            };
            m_tinfo = &typeid(f);
        }

        constexpr _function_storage(const _function_storage &fs) { fs.m_cop(&fs, this); }

        constexpr _function_storage(_function_storage &&fs) { fs.m_mov(&fs, this); }

        ~_function_storage() {
            if (m_tinfo != nullptr) {
                m_del(stack_mem);
            }
        }

      public:
        auto reset() -> void {
            if (m_tinfo) {
                m_del(stack_mem);
                m_tinfo = nullptr;
            }
        }

        explicit operator bool() const noexcept { return m_tinfo != nullptr; }

        auto operator()(A... args) const -> R { return m_ivk(this, std::forward<A>(args)...); }

        auto swap(_function_storage &other) -> void {
            std::swap(m_cop, other.m_cop);
            std::swap(m_mov, other.m_mov);
            std::swap(m_del, other.m_del);
            std::swap(m_ivk, other.m_ivk);
            std::swap(m_tinfo, other.m_tinfo);
        }

      public:
        union {
            char stack_mem[sizeof(void *)];
            void *heap_mem;
        };
        cop_type m_cop;
        mov_type m_mov;
        del_type m_del;
        ivk_type m_ivk;
        const std::type_info *m_tinfo{nullptr};
    };

    template <typename R, typename... A>
    class function<R(A...)> {
      public:
        using result_type = R;

        // 构造
      public:
        function() noexcept = default;

        function(std::nullptr_t) noexcept {}

        function(const function &f) : m_storage(f.m_storage) {}

        function(function &&f) noexcept : m_storage(std::move(f.m_storage)) {}

        template <typename F>
            requires(std::same_as<R, std::invoke_result_t<F, A...>>)
        function(F f) : m_storage(f) {}

        ~function() = default;

        // assign
      public:
        template <typename F>
            requires(std::same_as<R, std::invoke_result_t<std::decay_t<F>, A...>>)
        auto operator=(F &&f) -> function & {
            function(std::forward<F>(f)).swap(*this);
            return *this;
        }

        auto operator=(std::nullptr_t) noexcept -> function & {
            m_storage.reset();
            return *this;
        }

        // target access
      public:
        auto target_type() const noexcept -> const std::type_info & {
            if (m_storage.m_tinfo) {
                return *m_storage.m_tinfo;
            }
            return typeid(void);
        }

        template <typename T>
        auto target() noexcept -> T * {
            if constexpr (sizeof(T) <= sizeof(void *)) {
                return target_type() == typeid(T) ? const_cast<char *>(m_storage.stack_mem) : nullptr;
            } else {
                return target_type() == typeid(T) ? m_storage.heap_mem : nullptr;
            }
        }

        template <typename T>
        auto target() const noexcept -> const T * {
            if constexpr (sizeof(T) <= sizeof(void *)) {
                return target_type() == typeid(T) ? const_cast<char *>(m_storage.stack_mem) : nullptr;
            } else {
                return target_type() == typeid(T) ? m_storage.heap_mem : nullptr;
            }
        }

        //
      public:
        explicit operator bool() const noexcept { return static_cast<bool>(m_storage); }

        auto operator()(A... args) const -> R {
            if (*this) {
                return m_storage(std::forward<A>(args)...);
            }
            throw bad_function_call();
        }

        auto swap(function &other) noexcept -> void { m_storage.swap(other.m_storage); }

      public:
        _function_storage<R, A...> m_storage;
    };

    // 推导指南
    template <typename T>
    struct _function_guide_helper;

    template <typename R, typename G, typename... A, bool NX>
    struct _function_guide_helper<R (G::*)(A...) noexcept(NX)> {
        using type = R(A...);
    };

    template <typename R, typename G, typename... A, bool NX>
    struct _function_guide_helper<R (G::*)(A...) & noexcept(NX)> {
        using type = R(A...);
    };

    template <typename R, typename G, typename... A, bool NX>
    struct _function_guide_helper<R (G::*)(A...) const noexcept(NX)> {
        using type = R(A...);
    };

    template <typename R, typename G, typename... A, bool NX>
    struct _function_guide_helper<R (G::*)(A...) const & noexcept(NX)> {
        using type = R(A...);
    };

    template <typename T>
    using _function_guide_helper_t = _function_guide_helper<T>::type;

    template <typename R, typename... Args>
    function(R (*)(Args...)) -> function<R(Args...)>;

    template <typename F>
    function(F) -> function<_function_guide_helper_t<decltype(&F::operator())>>;

    // 全局函数
    template <typename R, typename... Args>
    auto operator==(const function<R(Args...)> &lhs, std::nullptr_t) -> bool { return !lhs; }

    template <typename R, typename... Args>
    auto swap(function<R(Args...)> &lhs, function<R(Args...)> &rhs) -> void { lhs.swap(rhs); }
} // namespace mtl