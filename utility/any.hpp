/*
    https://timsong-cpp.github.io/cppwp/n4861/any
    https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/std/any 借助类型擦除对存储的对象和类型进行管理
*/
#pragma once
#include "utility.hpp"

// any cast
namespace mtl {
    class bad_any_cast : public std::exception {};

    template <typename T>
        requires(!std::is_void_v<T>)
    constexpr auto any_cast(any *ap) -> T *;

    template <typename T>
        requires(!std::is_void_v<T>)
    constexpr auto any_cast(const any *ap) -> const T *;

    template <typename T, typename U = std::remove_cvref_t<T>>
        requires(std::is_constructible_v<T, const U &>)
    constexpr auto any_cast(const any &a) -> T { return static_cast<T>(*any_cast<U>(&a)); }

    template <typename T, typename U = std::remove_cvref_t<T>>
        requires(std::is_constructible_v<T, U &>)
    constexpr auto any_cast(any &a) -> T { return static_cast<T>(*any_cast<U>(&a)); }

    template <typename T, typename U = std::remove_cvref_t<T>>
        requires(std::is_constructible_v<T, U>)
    constexpr auto any_cast(any &&a) -> T { return static_cast<T>(std::move(*any_cast<U>(&a))); }
} // namespace mtl

// any
namespace mtl {
    class any {
      public:
        constexpr any() noexcept = default;

        constexpr any(const any &a) {
            if (a.has_value()) {
                auto arg = _any_manager_manage_arg{
                    .op = _any_manager_op::copy_assign,
                    .src_any = &a,
                    .dst_any = this};
                a.m_manage(arg);
                m_manage = a.m_manage;
            }
        }

        constexpr any(any &&a) {
            if (a.has_value()) {
                auto arg = _any_manager_manage_arg{
                    .op = _any_manager_op::move_assign,
                    .src_any = &a,
                    .dst_any = this};
                a.m_manage(arg);
                m_manage = a.m_manage;
            }
        }

        template <typename T, typename VT = std::decay_t<T>>
            requires(!std::is_same_v<VT, any> && std::is_copy_constructible_v<VT>)
        constexpr any(T &&t)
            : any(in_place_type<T>, std::forward<T>(t)) {}

        template <typename T, typename... Args, typename VT = std::decay_t<T>>
            requires(std::is_copy_constructible_v<VT> && std::is_constructible_v<VT, Args...>)
        constexpr any(in_place_type_t<T>, Args &&...args) {
            _any_manager_t<VT>::construct(*this, std::forward<Args>(args)...);
            m_manage = _any_manager_t<VT>::manage;
        }

        template <typename T, typename U, typename... Args, typename VT = std::decay_t<T>>
            requires(std::is_copy_constructible_v<VT> &&
                     std::is_constructible_v<VT, std::initializer_list<U>, Args...>)
        constexpr any(in_place_type_t<T>, std::initializer_list<U> lst, Args &&...args) {
            _any_manager_t<VT>::construct(*this, lst, std::forward<Args>(args)...);
            m_manage = _any_manager_t<VT>::manage;
        }

        ~any() { reset(); }

        // assignment
      public:
        auto operator=(const any &a) -> any & {
            any{a}.swap(*this);
            return *this;
        }

        auto operator=(any &&a) -> any & {
            any{std::move(a)}.swap(*this);
            return *this;
        }

        template <typename T>
            requires(std::is_copy_constructible_v<std::decay_t<T>>)
        auto operator=(T &&t) -> any & {
            any{std::forward<T>(t)}.swap(*this);
            return *this;
        }

        // modifier
      public:
        template <typename T, typename... Args, typename VT = std::decay_t<T>>
            requires(std::is_copy_constructible_v<VT> && std::is_constructible_v<VT, Args...>)
        constexpr auto emplace(Args &&...args) -> VT & {
            reset();
            _any_manager_t<VT>::construct(*this, std::forward<Args>(args)...);
            m_manage = _any_manager_t<VT>::manage;
        }

        template <typename T, typename U, typename... Args, typename VT = std::decay_t<T>>
        constexpr auto emplace(std::initializer_list<U> lst, Args &&...args) -> VT & { return emplace(std::move(lst), std::forward<Args>(args)...); }

        constexpr auto reset() noexcept -> void {
            if (has_value()) {
                auto arg = _any_manager_manage_arg{.op = _any_manager_op::destruct, .dst_any = this};
                m_manage(arg);
            }
        }

        constexpr auto swap(any &a) noexcept -> void {
            std::swap(m_manage, a.m_manage);
            std::swap(m_dbuf, a.m_dbuf); // dbuf 和 cache 共用一块内存，交换一次即可
        }

        // state
      public:
        constexpr auto has_value() const noexcept -> bool { return m_manage != nullptr; }

        constexpr auto type() const -> const std::type_info & {
            if (has_value()) {
                auto arg = _any_manager_manage_arg{.op = _any_manager_op::get_typeinfo};
                m_manage(arg);
                return *arg.t_info;
            }
            return typeid(void);
        }

        // managers
      public:
        enum class _any_manager_op {
            access,
            get_typeinfo,
            copy_assign,
            move_assign,
            destruct
        };

        struct _any_manager_manage_arg {
            _any_manager_op op;           // input
            const any *src_any;           // input
            any *dst_any;                 // input
            const std::type_info *t_info; // output
            void *obj;                    // output, 接收 m_cache 或 m_dbuf
        };

        using _any_manager_manage_t = void (*)(_any_manager_manage_arg &);

        template <typename T>
        struct _any_manager_internal { // 管理缓存
            constexpr static auto manage(_any_manager_manage_arg &arg) -> void {
                using enum _any_manager_op;
                switch (arg.op) {
                case access:
                    arg.obj = arg.dst_any->m_cache;
                    break;
                case get_typeinfo:
                    arg.t_info = &typeid(T);
                    break;
                case copy_assign:
                    destroy(arg);
                    arg.dst_any->m_manage = arg.src_any->m_manage;
                    std::construct_at(reinterpret_cast<T *>(arg.dst_any->m_cache), *reinterpret_cast<const T *>(arg.src_any->m_cache));
                    break;
                case move_assign:
                    destroy(arg);
                    arg.dst_any->m_manage = arg.src_any->m_manage;
                    std::construct_at(reinterpret_cast<T *>(arg.dst_any->m_cache), std::move(*reinterpret_cast<const T *>(arg.src_any->m_cache)));
                    break;
                case destruct:
                    destroy(arg);
                    break;
                }
            }

            template <typename... Args>
            constexpr static auto construct(any &a, Args &&...args) {
                std::construct_at(reinterpret_cast<T *>(a.m_cache), std::forward<Args>(args)...);
            }

            constexpr static auto destroy(_any_manager_manage_arg &arg) {
                if (arg.dst_any->has_value()) {
                    arg.dst_any->m_manage = nullptr;
                    std::destroy_at(reinterpret_cast<T *>(arg.dst_any->m_cache));
                }
            }
        };

        template <typename T>
        struct _any_manager_external { // 管理动态内存
            constexpr static auto manage(_any_manager_manage_arg &arg) -> void {
                using enum _any_manager_op;
                switch (arg.op) {
                case access:
                    arg.obj = arg.dst_any->m_dbuf;
                    break;
                case get_typeinfo:
                    arg.t_info = &typeid(T);
                    break;
                case copy_assign:
                    destroy(arg);
                    arg.dst_any->m_manage = arg.src_any->m_manage;
                    arg.dst_any->m_dbuf = new T(*reinterpret_cast<T *>(arg.src_any->m_dbuf));
                    break;
                case move_assign:
                    destroy(arg);
                    arg.dst_any->m_manage = arg.src_any->m_manage;
                    arg.dst_any->m_dbuf = new T(std::move(*reinterpret_cast<T *>(arg.src_any->m_dbuf)));
                    break;
                case destruct:
                    destroy(arg);
                    break;
                }
            }

            template <typename... Args>
            constexpr static auto construct(any &a, Args &&...args) {
                a.m_dbuf = new T(std::forward<Args>(args)...);
            }

            constexpr static auto destroy(_any_manager_manage_arg &arg) {
                if (arg.dst_any->has_value()) {
                    arg.dst_any->m_manage = nullptr;
                    delete reinterpret_cast<T *>(arg.dst_any->m_dbuf);
                }
            }
        };

        template <typename T>
        using _any_manager_t = std::conditional_t<sizeof(T) <= sizeof(void *) && std::is_nothrow_constructible_v<T>,
                                                  _any_manager_internal<T>, _any_manager_external<T>>;

      public:
        union {
            char m_cache[sizeof(void *)]; // 缓存
            void *m_dbuf;                 // 动态内存
        };
        _any_manager_manage_t m_manage;
    };
} // namespace mtl

// any cast
namespace mtl {
    template <typename T>
        requires(!std::is_void_v<T>)
    constexpr auto any_cast(any *ap) -> T * {
        if (ap->type() != typeid(T)) {
            throw bad_any_cast{};
        } else if (ap != nullptr) {
            auto arg = any::_any_manager_manage_arg{.op = any::_any_manager_op::access, .dst_any = ap};
            ap->m_manage(arg);
            return reinterpret_cast<T *>(arg.obj);
        }
        return nullptr;
    }

    template <typename T>
        requires(!std::is_void_v<T>)
    constexpr auto any_cast(const any *ap) -> const T * {
        return const_cast<const T *>(any_cast<T>(const_cast<any *>(ap)));
    }
} // namespace mtl

// swap
namespace mtl {
    constexpr auto swap(any &lhs, any &rhs) -> void { lhs.swap(rhs); }
} // namespace mtl

// make any
namespace mtl {
    template <typename T, typename... Args>
    auto make_any(Args &&...args) -> any { return {in_place_type<T>, std::forward<Args>(args)...}; }

    template <typename T, typename U, typename... Args>
    auto make_any(std::initializer_list<U> lst, Args &&...args) { return make_any<T>(std::move(lst), std::forward<Args>(args)...); }
} // namespace mtl