/*
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
    // 小类型对象，直接存放在栈内存中；大类型对象，存放在堆内存中。
    // 表达式 stack_mem 为小对象的地址，表达式 heap_mem 为大对象的地址。
    // 借助 manager::manage 对保存对象进行拷贝、移动、析构等操作，实现类型擦除。
    class any {
      public:
        constexpr any() noexcept = default;

        constexpr any(const any &a) {
            if (a.has_value()) {
                auto arg = _any_manager_manage_arg{
                    .op = _any_manager_op::copy,
                    .src = &a,
                    .dst = this};
                a.m_manage(arg);
                m_manage = a.m_manage;
            }
        }

        constexpr any(any &&a) {
            if (this != &a && a.has_value()) {
                auto arg = _any_manager_manage_arg{
                    .op = _any_manager_op::move,
                    .src = &a,
                    .dst = this};
                a.m_manage(arg);
                m_manage = a.m_manage;
                a.m_manage = nullptr;
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
            any(a).swap(*this);
            return *this;
        }

        auto operator=(any &&a) -> any & {
            any(std::move(a)).swap(*this);
            return *this;
        }

        template <typename T>
            requires(std::is_copy_constructible_v<std::decay_t<T>>)
        auto operator=(T &&t) -> any & {
            any(std::forward<T>(t)).swap(*this);
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
            return *any_cast<T>(this);
        }

        template <typename T, typename U, typename... Args, typename VT = std::decay_t<T>>
            requires(std::is_copy_constructible_v<VT> && std::is_constructible_v<VT, std::initializer_list<U> &, Args...>)
        constexpr auto emplace(std::initializer_list<U> lst, Args &&...args) -> VT & {
            reset();
            _any_manager_t<VT>::construct(*this, std::move(lst), std::forward<Args>(args)...);
            m_manage = _any_manager_t<VT>::manage;
            return *any_cast<T>(this);
        }

        constexpr auto reset() noexcept -> void {
            if (has_value()) {
                auto arg = _any_manager_manage_arg{.op = _any_manager_op::destruct, .dst = this};
                m_manage(arg);
            }
        }

        constexpr auto swap(any &a) noexcept -> void {
            std::swap(m_manage, a.m_manage);
            std::swap(m_heap_mem, a.m_heap_mem); // heap_mem 和 stack_mem 占用一块内存，交换一次即可
        }

        // state
      public:
        constexpr auto has_value() const noexcept -> bool { return m_manage != nullptr; }

        constexpr auto type() const -> const std::type_info & {
            if (has_value()) {
                auto arg = _any_manager_manage_arg{.op = _any_manager_op::typeinfo};
                m_manage(arg);
                return *arg.tp;
            }
            return typeid(void);
        }

        // manager
      public:
        enum class _any_manager_op {
            access,   // 获取地址
            typeinfo, // 获取类型
            copy,     // 拷贝
            move,     // 移动
            destruct  // 销毁
        };

        struct _any_manager_manage_arg {
            _any_manager_op op;       // 输入参数
            const any *src;           // 输入参数
            any *dst;                 // 输入参数
            const std::type_info *tp; // 输出参数
            void *mem;                // 输出参数, 保存 stack_mem 或 heap_mem
        };

        using _any_manager_manage_t = void (*)(_any_manager_manage_arg &);

        // 栈内存管理器
        template <typename T>
        struct _any_stack_mem_manager {
            constexpr static auto manage(_any_manager_manage_arg &arg) -> void {
                using enum _any_manager_op;
                switch (arg.op) {
                    case access:
                        arg.mem = arg.dst->m_stack_mem;
                        break;
                    case typeinfo:
                        arg.tp = &typeid(T);
                        break;
                    case copy:
                        destroy(arg);
                        construct(*arg.dst, *reinterpret_cast<const T *>(arg.src->m_stack_mem));
                        arg.dst->m_manage = arg.src->m_manage;
                        break;
                    case move:
                        destroy(arg);
                        construct(*arg.dst, std::move(*reinterpret_cast<const T *>(arg.src->m_stack_mem)));
                        arg.dst->m_manage = arg.src->m_manage;
                        break;
                    case destruct:
                        destroy(arg);
                        break;
                }
            }

            template <typename... Args>
            constexpr static auto construct(any &a, Args &&...args) { std::construct_at(reinterpret_cast<T *>(a.m_stack_mem), std::forward<Args>(args)...); }

            constexpr static auto destroy(_any_manager_manage_arg &arg) {
                if (arg.dst->has_value()) {
                    std::destroy_at(reinterpret_cast<T *>(arg.dst->m_stack_mem));
                    arg.dst->m_manage = nullptr;
                }
            }
        };

        // 堆内存管理器
        template <typename T>
        struct _any_heap_mem_manager {
            constexpr static auto manage(_any_manager_manage_arg &arg) -> void {
                using enum _any_manager_op;
                switch (arg.op) {
                    case access:
                        arg.mem = arg.dst->m_heap_mem;
                        break;
                    case typeinfo:
                        arg.tp = &typeid(T);
                        break;
                    case copy:
                        destroy(arg);
                        construct(*arg.dst, *reinterpret_cast<T *>(arg.src->m_heap_mem));
                        arg.dst->m_manage = arg.src->m_manage;
                        break;
                    case move:
                        destroy(arg);
                        construct(*arg.dst, std::move(*reinterpret_cast<T *>(arg.src->m_heap_mem)));
                        arg.dst->m_manage = arg.src->m_manage;
                        break;
                    case destruct:
                        destroy(arg);
                        break;
                }
            }

            template <typename... Args>
            constexpr static auto construct(any &a, Args &&...args) { a.m_heap_mem = new T(std::forward<Args>(args)...); }

            constexpr static auto destroy(_any_manager_manage_arg &arg) {
                if (arg.dst->has_value()) {
                    arg.dst->m_manage = nullptr;
                    delete reinterpret_cast<T *>(arg.dst->m_heap_mem);
                }
            }
        };

        // any 的默认构造函数是不会抛出异常的，因此如果 T 的默认构造函数可能抛出异常，那么其只能存放在堆中。这样 any 默认构造时，不会立即初始化，也就不会抛出异常。
        template <typename T>
        using _any_manager_t = std::conditional_t<sizeof(T) <= sizeof(void *) && std::is_nothrow_constructible_v<T>,
                                                  _any_stack_mem_manager<T>, _any_heap_mem_manager<T>>;

      public:
        union {
            char m_stack_mem[sizeof(void *)];
            void *m_heap_mem;
        };
        _any_manager_manage_t m_manage = nullptr;
    };
} // namespace mtl

// any cast
namespace mtl {
    template <typename T>
        requires(!std::is_void_v<T>)
    constexpr auto any_cast(any *ap) -> T * {
        if (ap == nullptr || ap->type() != typeid(T) || !ap->has_value()) {
            throw bad_any_cast{};
        }
        auto arg = any::_any_manager_manage_arg{.op = any::_any_manager_op::access, .dst = ap};
        ap->m_manage(arg);
        return reinterpret_cast<T *>(arg.mem);
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
    auto make_any(Args &&...args) -> any { return any(in_place_type<T>, std::forward<Args>(args)...); }

    template <typename T, typename U, typename... Args>
    auto make_any(std::initializer_list<U> lst, Args &&...args) { return any(in_place_type<T>, lst, std::forward<Args>(args)...); }
} // namespace mtl