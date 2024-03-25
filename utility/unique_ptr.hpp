/*
    https://timsong-cpp.github.io/cppwp/n4861/unique.ptr#def:unique_pointer
*/
#pragma once
#include "utility.hpp" // IWYU pragma: keep

// default_delete
namespace mtl {
template <typename T>
struct default_delete {
  public:
    constexpr default_delete() = default;

    template <typename U>
        requires(std::is_convertible_v<U *, T *>)
    constexpr default_delete(const default_delete<U> &) noexcept {}

  public:
    auto operator()(T *p) const -> void {
        static_assert(is_complete<T>, "can't delete pointer to incomplete type");
        delete p;
    }
};

template <typename T>
struct default_delete<T[]> {
  public:
    constexpr default_delete() noexcept = default;

    template <typename U>
        requires(std::is_convertible_v<U (*)[], T (*)[]>)
    default_delete(const default_delete<U[]> &) noexcept {}

  public:
    template <typename U>
        requires(std::is_convertible_v<U (*)[], T (*)[]>)
    auto operator()(U *p) const {
        static_assert(is_complete<U>, "can't delete pointer to incomplete type");
        delete[] p;
    }
};
} // namespace mtl

// unique_ptr
namespace mtl {
namespace _unique_ptr_details {
template <typename T, typename D>
auto pointer_(int) -> std::remove_reference_t<D>::pointer;
template <typename T, typename D>
auto pointer_(long) -> T *;
template <typename T, typename D>
using pointer = decltype(pointer_<T, D>(0));
} // namespace _unique_ptr_details

template <typename T, typename D>
class unique_ptr {
  public:
    using pointer = _unique_ptr_details::pointer<T, D>;
    using element_type = T;
    using deleter_type = D;

  public:
    constexpr unique_ptr() noexcept
        requires(std::is_nothrow_default_constructible_v<deleter_type> && !std::is_pointer_v<deleter_type>)
        : m_ptr(new element_type) {}

    constexpr unique_ptr(std::nullptr_t) noexcept
        requires(!std::is_pointer_v<deleter_type> && std::is_default_constructible_v<deleter_type>)
        : m_ptr(nullptr) {}

    explicit unique_ptr(pointer p) noexcept
        requires(!std::is_pointer_v<deleter_type> && std::is_nothrow_default_constructible_v<deleter_type>)
        : m_ptr(p) {}

    unique_ptr(pointer p, const deleter_type &d) noexcept
        requires(std::is_constructible_v<deleter_type, decltype(d)>)
        : m_ptr(p), m_del(d) {}

    unique_ptr(pointer p, deleter_type &&d) noexcept
        requires(!std::is_rvalue_reference_v<deleter_type>)
        : m_ptr(p), m_del(std::move(d)) {}

    unique_ptr(unique_ptr &&u) noexcept
        requires(std::is_move_constructible_v<D>)
        : m_ptr(u.release()), m_del(std::move(u.m_del)) {
    }

    template <typename T_, typename D_>
        requires(std::is_convertible_v<typename unique_ptr<T_, D_>::pointer, pointer> &&
                 !std::is_array_v<T_> &&
                 std::conditional_t<std::is_rvalue_reference_v<D>, // 若 D 是引用，则需要 D_==D，否则需要 D_->D
                                    std::is_same<D_, D>,
                                    std::is_convertible<D_, D>>::value)
    unique_ptr(unique_ptr<T_, D_> &&u) noexcept
        : m_ptr(u.release()), m_del(std::forward<D_>(u.get_deleter())) {}

    ~unique_ptr() {
        if (m_ptr) {
            m_del(m_ptr);
        }
    }

    // assignment
  public:
    auto operator=(unique_ptr &&u) noexcept -> unique_ptr &
        requires(std::is_move_assignable_v<D>)
    {
        if (this != &u) {
            reset(u.release());
            m_del = std::forward<D>(u.m_del);
        }
        return *this;
    }

    template <typename T_, typename D_>
        requires(std::is_convertible_v<typename unique_ptr<T_, D_>::pointer, pointer> &&
                 !std::is_array_v<T_> && std::is_assignable_v<D &, D_ &&>)
    auto operator=(unique_ptr<T_, D_> &&u) noexcept -> unique_ptr & {
        reset(u.release());
        m_del = std::forward<D_>(u.m_del);
        return *this;
    }

    auto operator=(std::nullptr_t) noexcept -> unique_ptr & {
        reset();
        return *this;
    }

    // observer
  public:
    auto operator*() const -> std::add_lvalue_reference_t<T> { return *get(); }

    auto operator->() const noexcept {
        static_assert(is_complete<T>, "T must be complete");
        return get();
    }

    explicit operator bool() const noexcept { return get() != nullptr; }

    auto get() const noexcept -> pointer { return m_ptr; }

    auto get_deleter() noexcept -> deleter_type & { return m_del; }

    auto get_deleter() const noexcept -> const deleter_type & { return m_del; }

    // modifier
  public:
    auto release() noexcept -> pointer {
        auto ret = m_ptr;
        m_ptr = nullptr;
        return ret;
    }

    auto reset(pointer p = pointer()) noexcept {
        // 遵守该顺序。若 m_ptr 指向 *this，调用 ~pointer() 之后，任何操作都是不安全的，因此将 m_deleter(old_ptr) 放在最后。
        auto old_ptr = m_ptr;
        m_ptr = p;
        if (old_ptr) {
            m_del(old_ptr);
        }
    }

    auto swap(unique_ptr &u) noexcept {
        static_assert(std::is_swappable_v<deleter_type>, "deleter must swappable");
        std::swap(m_ptr, u.m_ptr);
        std::swap(m_del, u.m_del);
    }

  public:
    pointer m_ptr;
    deleter_type m_del;
};

template <typename T, typename D>
class unique_ptr<T[], D> {
  public:
    using pointer = _unique_ptr_details::pointer<T, D>;
    using element_type = T;
    using deleter_type = D;
    // 构造
  public:
    constexpr unique_ptr() noexcept = default;

    template <class T_>
        requires(std::is_same_v<T_, pointer> ||
                 (std::is_same_v<pointer, element_type *> &&
                  std::is_convertible_v<std::remove_pointer_t<T_> (*)[], element_type (*)[]>))
    explicit unique_ptr(T_ p) noexcept : m_ptr(p) {}

    template <typename T_>
    unique_ptr(T_ p, const deleter_type &d) noexcept
        requires(std::is_constructible_v<deleter_type, decltype(d)> &&
                 (std::is_same_v<T_, pointer> ||
                  std::is_same_v<T_, std::nullptr_t> ||
                  (std::is_same_v<pointer, element_type *> &&
                   std::is_convertible_v<std::remove_pointer_t<T_> (*)[], element_type (*)[]>)))
        : m_ptr(p), m_del(d) {}

    template <typename T_>
        requires(!std::is_rvalue_reference_v<deleter_type> &&
                 (std::is_same_v<T_, pointer> ||
                  std::is_same_v<T_, std::nullptr_t> ||
                  (std::is_same_v<pointer, element_type *> &&
                   std::is_convertible_v<std::remove_pointer_t<T_> (*)[], element_type (*)[]>)))
    unique_ptr(T_ p, deleter_type &&d) noexcept
        : m_ptr(p), m_del(std::move(d)) {}

    template <typename T_, typename D_, typename Ptr_ = unique_ptr<T_, D_>>
        requires(std::is_array_v<T_> &&
                 std::is_same_v<pointer, element_type *> &&
                 std::is_same_v<typename Ptr_::pointer, typename Ptr_::element_type *> &&
                 std::is_convertible_v<typename Ptr_::element_type (*)[], element_type (*)[]> &&
                 ((std::is_rvalue_reference_v<D> && std::is_same_v<D_, D>) ||
                  std::is_convertible_v<D_, D>))
    unique_ptr(unique_ptr<T_, D_> &&u) noexcept
        : m_ptr(u.release()), m_del(std::move(u.m_del)) {}

    ~unique_ptr() {
        if (m_ptr) {
            m_del(m_ptr);
        }
    }

    // 删除拷贝
  public:
    unique_ptr(const unique_ptr &) = delete;

    auto operator=(const unique_ptr &) -> unique_ptr & = delete;

    // assignment
  public:
    auto operator=(unique_ptr &&u) noexcept -> unique_ptr &
        requires(std::is_move_assignable_v<D>)
    {
        if (this != &u) {
            reset(u.release());
            m_del = std::forward<D>(u.m_del);
        }
        return *this;
    }

    template <typename T_, typename D_, typename Ptr_ = unique_ptr<T_, D_>>
        requires(std::is_array_v<T_> &&
                 std::is_same_v<pointer, element_type *> &&
                 std::is_same_v<typename Ptr_::pointer, typename Ptr_::element_type *> &&
                 std::is_convertible_v<typename Ptr_::element_type (*)[], element_type (*)[]> &&
                 std::is_assignable_v<D &, D_ &&>)
    auto operator=(unique_ptr<T_, D_> &&u) noexcept {
        reset(u.release());
        m_del = std::forward<D_>(u.m_del);
        return *this;
    }

    auto operator=(std::nullptr_t) noexcept -> unique_ptr & {
        reset();
        return *this;
    }

    // observer
  public:
    auto operator[](size_t idx) const -> T & { return m_ptr[idx]; }

    auto get() const noexcept -> pointer { return m_ptr; }

    auto get_deleter() noexcept -> deleter_type & { return m_del; }

    auto get_deleter() const noexcept -> const deleter_type & { return m_del; }

    // modifier
  public:
    auto release() noexcept -> pointer {
        auto ret = m_ptr;
        m_ptr = nullptr;
        return ret;
    }

    template <typename T_>
    auto reset(T_ p) noexcept {
        auto old_ptr = m_ptr;
        m_ptr = p;
        if (old_ptr) {
            m_del(old_ptr);
        }
    }

    auto reset(std::nullptr_t p = nullptr) noexcept { reset(pointer()); }

    auto swap(unique_ptr &u) noexcept {
        static_assert(std::is_swappable_v<deleter_type>, "deleter must swappable");
        std::swap(*this, u);
    }

  public:
    pointer m_ptr;
    deleter_type m_del;
};
} // namespace mtl

// relational operation
namespace mtl {
template <typename T1, typename D1, typename T2, typename D2>
auto operator==(const unique_ptr<T1, D1> &lhs, const unique_ptr<T2, D2> &rhs) -> bool { return lhs.get() == rhs.get(); }

template <typename T1, typename D1, typename T2, typename D2,
          typename CT = std::common_type_t<typename unique_ptr<T1, D1>::pointer, typename unique_ptr<T2, D2>::pointer>>
    requires(std::is_convertible_v<typename unique_ptr<T1, D1>::pointer, CT> &&
             std::is_convertible_v<typename unique_ptr<T2, D2>::pointer, CT>)
auto operator<(const unique_ptr<T1, D1> &lhs, const unique_ptr<T2, D2> &rhs) -> bool { return std::less<CT>()(lhs.get(), rhs.get()); }

template <typename T1, typename D1, typename T2, typename D2>
auto operator>(const unique_ptr<T1, D1> &lhs, const unique_ptr<T2, D2> &rhs) -> bool { return rhs < lhs; }

template <typename T1, typename D1, typename T2, typename D2>
auto operator<=(const unique_ptr<T1, D1> &lhs, const unique_ptr<T2, D2> &rhs) -> bool { return !(lhs > rhs); }

template <typename T1, typename D1, typename T2, typename D2>
auto operator>=(const unique_ptr<T1, D1> &lhs, const unique_ptr<T2, D2> &rhs) -> bool { return !(lhs < rhs); }

template <class T1, class D1, class T2, class D2>
    requires(std::three_way_comparable_with<typename unique_ptr<T1, D1>::pointer,
                                            typename unique_ptr<T2, D2>::pointer>)
auto operator<=>(const unique_ptr<T1, D1> &lhs, const unique_ptr<T2, D2> &rhs)
    -> std::compare_three_way_result_t<typename unique_ptr<T1, D1>::pointer,
                                       typename unique_ptr<T2, D2>::pointer> {
    return std::compare_three_way()(lhs.get(), rhs.get());
}

// 针对 nullptr_t 的忽略
} // namespace mtl

// make unique
namespace mtl {
template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
auto make_unique(Args &&...args) -> unique_ptr<T> {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename T>
    requires(std::is_bounded_array_v<T>)
auto make_unique(size_t n) -> unique_ptr<T> {
    return unique_ptr<T>(new std::remove_extent_t<T>[n]());
}

template <typename T>
    requires(!std::is_array_v<T>)
auto make_unique_for_overwrite() -> unique_ptr<T> {
    return unique_ptr<T>(new T);
}

template <typename T>
    requires(std::is_bounded_array_v<T>)
auto make_unique_for_overwrite(size_t n) -> unique_ptr<T> {
    return unique_ptr<T>(new std::remove_extent_t<T>[n]);
}
} // namespace mtl