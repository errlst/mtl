/*
    https://timsong-cpp.github.io/cppwp/n4861/unique.ptr#def:unique_pointer
*/
#pragma once
#include "utility.hpp"  // IWYU pragma: keep

// default_delete
namespace mtl {
    template <typename T>
    struct default_delete {
      public:
        constexpr default_delete() = default;

        template <typename U>
        requires(std::is_convertible_v<U*, T*>)
        constexpr default_delete(const default_delete<U>&) noexcept {}

      public:
        auto operator()(T* p) const -> void {
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
        default_delete(const default_delete<U[]>&) noexcept {}

      public:
        template <typename U>
        requires(std::is_convertible_v<U (*)[], T (*)[]>)
        auto operator()(U* p) const {
            static_assert(is_complete<U>, "can't delete pointer to incomplete type");
            delete[] p;
        }
    };
}  // namespace mtl

// unique_ptr
namespace mtl {
    template <typename T, typename D>
    class unique_ptr {
      public:
        using element_type = T;
        using deleter_type = D;
        using pointer = default_or_t<element_type*, typename std::remove_reference_t<D>::pointer>;

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

        unique_ptr(pointer p, const deleter_type& d) noexcept
        requires(std::is_constructible_v<deleter_type, decltype(d)>)
            : m_ptr(p), m_deleter(d) {}

        unique_ptr(pointer p, deleter_type&& d) noexcept
        requires(!std::is_rvalue_reference_v<deleter_type>)
            : m_ptr(p), m_deleter(std::move(d)) {}

        unique_ptr(unique_ptr&& u) noexcept
        requires(std::is_move_constructible_v<D>)
            : m_ptr(u.release()), m_deleter(std::move(u.m_deleter)) {
        }

        template <typename T_, typename U_>
        unique_ptr(unique_ptr<T_, U_>&& u) noexcept {
        }

        // modifier
      public:
        auto release() noexcept -> pointer {
            auto ret = m_ptr;
            m_ptr = nullptr;
            return ret;
        }

      public:
        pointer m_ptr;
        deleter_type m_deleter;
    };
}  // namespace mtl