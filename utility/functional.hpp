#pragma once
#include "tuple.hpp"
#include "utility.hpp"

//  invoke
namespace mtl {
    //  f 是 T 的成员函数指针
    //  传入的 t 可以是引用、reference_wrap、指针
    //  只能使用 std::reference_wrap，因为没有实现 mtl::remove_reference_t
    template <typename Ret, typename Class, typename... Args, typename T>
    constexpr auto _invoke_impl(Ret (Class::*f)(Args...), T &&t, Args &&...args) -> decltype(auto) {
        if constexpr (std::is_base_of_v<Class, std::remove_reference_t<decltype(t)>>) {
            return (t.*f)(std::forward<Args>(args)...);
        } else if constexpr (std::is_same_v<std::true_type, decltype(is_specialization<std::reference_wrapper>(std::declval<T>()))>) {
            return (t.get().*f)(std::forward<Args>(args)...);
        } else {
            return ((*t).*f)(std::forward<Args>(args)...);
        }
    }

    //  f 是 T 的数据成员指针
    template <typename Type, typename Class, typename T>
    constexpr auto _invoke_impl(Type(Class::*f), T &&t) -> decltype(auto) {
        if constexpr (std::is_base_of_v<Class, std::remove_reference_t<decltype(t)>>) {
            return t.*f;
        } else if constexpr (std::is_same_v<std::true_type, decltype(is_specialization<std::reference_wrapper>(std::declval<T>()))>) {
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
            return f(std::forward<T>(t), std::forward<Args>(args)...);
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
        constexpr auto operator()(Args &&...args) -> std::invoke_result_t<T &, Args...> { return invoke(get(), std::forward<Args>(args)...); }

      public:
        T *m_ptr;
    };

    template <typename T>
    reference_wrapper(T &) -> reference_wrapper<T>;

    //  ref
    template <typename T>
    constexpr auto ref(T &t) noexcept -> reference_wrapper<T> { return {t}; }

    template <typename T>
    constexpr auto ref(reference_wrapper<T> r) noexcept -> reference_wrapper<T> { return ref(r.get()); }

    template <typename T>
    constexpr auto cref(const T &t) noexcept -> reference_wrapper<const T> { return {t}; }

    template <typename T>
    constexpr auto cref(reference_wrapper<T> r) noexcept -> reference_wrapper<const T> { return cref(r.get()); }

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
#define _BIND_FRONT_RET_OP_INVOKE(_QUAL)                                                                                                              \
    template <typename... CallArgs>                                                                                                                   \
    auto operator()(CallArgs &&...c_args) _QUAL->std::invoke_result_t<FD _QUAL, BoundArgs _QUAL..., CallArgs...> {                                    \
        return [&, this]<size_t... Idx>(index_sequence<Idx...>) {                                                                                     \
            return invoke(std::forward<FD _QUAL>(m_f), get<Idx>(std::forward<decltype(m_args) _QUAL>(m_args))..., std::forward<CallArgs>(c_args)...); \
        }(make_index_sequence<sizeof...(BoundArgs)>());                                                                                               \
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
    constexpr auto bind_front(F &&f, Args &&...args) -> _bind_front_ret<std::decay_t<F>, std::decay_t<Args>...> {
        return _bind_front_ret<std::decay_t<F>, std::decay_t<Args>...>{std::forward<F>(f), std::forward<Args>(args)...};
    }
} // namespace mtl

//  function 类似 any，小对象直接存放在栈内存，大对象存放在堆内存。
//  function 的实现借助 lambda，对删除、拷贝、移动、调用操作进行类型擦除。
namespace mtl {
    class bad_function_call : public std::exception {};

    template <typename Ret, typename... Args>
    class _function_storage {
        using del_type = void (*)(void *);                                         // 删除器，传入 stack_mem
        using cop_type = void (*)(const _function_storage *, _function_storage *); // 拷贝器（拷贝自身给其他对象
        using mov_type = void (*)(_function_storage *, _function_storage *);       // 移动器（移动自身给其他对象
        using ivk_type = Ret (*)(const _function_storage *, Args...);              // 调用器

      public:
        template <typename F>
            requires(!std::same_as<F, _function_storage>)
        constexpr _function_storage(F f) {
            if constexpr (sizeof(F) <= sizeof(void *)) {
                std::construct_at(reinterpret_cast<F *>(stack_mem), std::forward<F>(f));
                m_del = [](void *mem) { (*reinterpret_cast<F *>(mem)).~F(); };
            } else {
                heap_mem = new F(std::forward<F>(f));
                m_del = [](void *mem) { delete reinterpret_cast<F *>((*reinterpret_cast<std::ptrdiff_t *>(mem))); }; // 通过 stack_mem 获取 heap_mem 的值
            }
            m_cop = [](const _function_storage *src, _function_storage *dst) {
                dst->reset();
                if constexpr (sizeof(F) <= sizeof(void *)) {
                    std::construct_at(reinterpret_cast<F *>(dst->stack_mem), *reinterpret_cast<F *>(const_cast<char *>(src->stack_mem)));
                } else {
                    dst->heap_mem = new F(*reinterpret_cast<F *>(src->heap_mem));
                }
                dst->m_cop = src->m_cop;
                dst->m_mov = src->m_mov;
                dst->m_del = src->m_del;
                dst->m_ivk = src->m_ivk;
                dst->m_tinfo = src->m_tinfo;
            };
            m_mov = [](_function_storage *src, _function_storage *dst) {
                dst->reset();
                dst->heap_mem = src->heap_mem;
                dst->m_cop = src->m_cop;
                dst->m_mov = src->m_mov;
                dst->m_del = src->m_del;
                dst->m_ivk = src->m_ivk;
                dst->m_tinfo = src->m_tinfo;
                src->m_tinfo = nullptr;
            };
            m_ivk = [](const _function_storage *_this, Args... args) -> Ret {
                if constexpr (sizeof(F) <= sizeof(void *)) {
                    return invoke(*reinterpret_cast<F *>(const_cast<char *>(_this->stack_mem)), std::forward<Args>(args)...);
                } else {
                    return invoke(*reinterpret_cast<F *>(_this->heap_mem), std::forward<Args>(args)...);
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

        auto operator()(Args... args) const -> Ret { return m_ivk(this, std::forward<Args>(args)...); }

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

    template <typename Ret, typename... Args>
    class function<Ret(Args...)> {
      public:
        using result_type = Ret;

        // 构造
      public:
        function() noexcept = default;

        function(std::nullptr_t) noexcept {}

        function(const function &f) : m_storage(f.m_storage) {}

        function(function &&f) noexcept : m_storage(std::move(f.m_storage)) {}

        template <typename F>
            requires requires { invoke(std::declval<F>(), std::declval<Args>()...); }
        function(F f) : m_storage(f) {}

        ~function() = default;

        // assign
      public:
        template <typename F>
            requires requires { invoke(std::declval<F>(), std::declval<Args>()...); }
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

        auto operator()(Args... args) const -> Ret {
            if (*this) {
                return m_storage(std::forward<Args>(args)...);
            }
            throw bad_function_call();
        }

        auto swap(function &other) noexcept -> void { m_storage.swap(other.m_storage); }

      public:
        _function_storage<Ret, Args...> m_storage;
    };

    // 推导指南
    template <typename T>
    struct _function_guide_helper;

#define _FUNCTION_GUIDE_HELPER(_QUAL)                                           \
    template <typename Ret, typename Class, typename... Args, bool NX>          \
    struct _function_guide_helper<Ret (Class::*)(Args...) _QUAL noexcept(NX)> { \
        using type = Ret(Args...);                                              \
    };
    _FUNCTION_GUIDE_HELPER()
    _FUNCTION_GUIDE_HELPER(&)
    _FUNCTION_GUIDE_HELPER(const)
    _FUNCTION_GUIDE_HELPER(const &)
#undef _FUNCTION_GUIDE_HELPER

    template <typename T>
    using _function_guide_helper_t = _function_guide_helper<T>::type;

    template <typename Ret, typename... Args>
    function(Ret (*)(Args...)) -> function<Ret(Args...)>;

    template <typename F>
    function(F) -> function<_function_guide_helper_t<decltype(&F::operator())>>;

    // 全局函数
    template <typename R, typename... Args>
    auto operator==(const function<R(Args...)> &lhs, std::nullptr_t) -> bool {
        return !lhs;
    }

    template <typename R, typename... Args>
    auto swap(function<R(Args...)> &lhs, function<R(Args...)> &rhs) -> void {
        lhs.swap(rhs);
    }
} // namespace mtl