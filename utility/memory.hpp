/*
    https://timsong-cpp.github.io/cppwp/n4861/memory
    https://codereview.stackexchange.com/questions/220090/c17-pointer-traits-implementation pointer_traits 实现
*/
#pragma once
#include "utility.hpp"  // IWYU pragma: keep

// pointer traits
namespace mtl {
    namespace pointer_traits_detal {
        template <typename Ptr>
        auto ele_type_(int) -> Ptr::element_type;
        template <template <typename, typename...> typename Ptr, typename T, typename... Args>
        auto ele_type_(Ptr<T, Args...>) -> T;
        template <typename Ptr>
        auto ele_type_(long) -> decltype(ele_type_(Ptr{}));
        template <typename Ptr>
        using ele_type = decltype(ele_type_<Ptr>(0));

        template <typename Ptr>
        auto diff_type_(int) -> Ptr::difference_type;
        template <typename Ptr>
        auto diff_type_(long) -> ptrdiff_t;
        template <typename Ptr>
        using diff_type = decltype(diff_type_<Ptr>(0));

        template <typename Ptr, typename U>
        auto rebind_type_(int) -> typename Ptr::template rebind<U>;
        template <typename U, template <typename, typename...> typename Ptr, typename T, typename... Args>
        auto rebind_type_(Ptr<T, Args...>) -> Ptr<U, Args...>;
        template <typename Ptr, typename U>
        auto rebind_type_(long) -> decltype(rebind_type_<U>(Ptr{}));
        template <typename Ptr, typename U>
        using rebind_type = decltype(rebind_type_<Ptr, U>(0));

    }  // namespace pointer_traits_detal

    template <typename Ptr>
    struct pointer_traits {
        using pointer = Ptr;
        using element_type = pointer_traits_detal::ele_type<Ptr>;
        using difference_type = pointer_traits_detal::diff_type<Ptr>;
        template <typename U>
        using rebind = pointer_traits_detal::rebind_type<Ptr, U>;

      public:
        static auto pointer_to(element_type& e) -> pointer {
            return Ptr::pointer_to(e);
        }
    };

    template <typename T>
    struct pointer_traits<T*> {
        using pointer = T*;
        using element_type = T;
        using difference_type = ptrdiff_t;
        template <typename U>
        using rebind = U*;

      public:
        static constexpr auto pointer_to(element_type& e) noexcept -> pointer {
            return std::addressof(e);
        }
    };
}  // namespace mtl

// to address
namespace mtl {
    template <typename T>
    requires(!std::is_function_v<T>)
    constexpr auto to_address(T* p) noexcept -> T* {
        return p;
    }

    template <typename Ptr>
    constexpr auto _to_address_to_address(const Ptr& p, int) noexcept {
        return pointer_traits<Ptr>::to_address(p);
    }

    template <typename Ptr>
    constexpr auto _to_address_to_address(const Ptr& p, long) noexcept {
        return to_address(p.operator->());
    }

    template <typename Ptr>
    constexpr auto to_address(const Ptr& p) noexcept {
        return _to_address_to_address(p, 0);
    }
}  // namespace mtl

// allocator traits
namespace mtl {
    namespace _allocator_traits_detail {
        template <typename Alc>
        auto ptr_(int) -> Alc::pointer;
        template <typename Alc>
        auto ptr_(long) -> Alc::value_type*;
        template <typename Alc>
        using ptr = decltype(ptr_<Alc>(0));

        template <typename Alc>
        auto cptr_(int) -> Alc::const_pointer;
        template <typename Alc>
        auto cptr_(long) -> pointer_traits<ptr<Alc>>::template rebind<const typename Alc::value_type>;
        template <typename Alc>
        using cptr = decltype(cptr_<Alc>(0));

        template <typename Alc>
        auto vptr_(int) -> Alc::void_pointer;
        template <typename Alc>
        auto vptr_(long) -> pointer_traits<ptr<Alc>>::template rebind<void>;
        template <typename Alc>
        using vptr = decltype(vptr_<Alc>(0));

        template <typename Alc>
        auto cvptr_(int) -> Alc::const_void_pointer;
        template <typename Alc>
        auto cvptr_(long) -> pointer_traits<ptr<Alc>>::template rebind<const void>;
        template <typename Alc>
        using cvptr = decltype(cvptr_<Alc>(0));

        template <typename Alc>
        auto diff_type_(int) -> Alc::difference_type;
        template <typename Alc>
        auto diff_type_(long) -> pointer_traits<ptr<Alc>>::difference_type;
        template <typename Alc>
        using diff_type = decltype(diff_type_<Alc>(0));

        template <typename Alc>
        auto size_type_(int) -> Alc::size_type;
        template <typename Alc>
        auto size_type_(long) -> std::make_unsigned_t<diff_type<Alc>>;
        template <typename Alc>
        using size_type = decltype(size_type_<Alc>(0));
    }  // namespace _allocator_traits_detail

    template <typename Alloc>
    struct allocator_traits {
      private:
      public:
        using allocator_type = Alloc;
        using value_type = Alloc::value_type;
        using pointer = _allocator_traits_detail::ptr<Alloc>;
        using const_pointer = _allocator_traits_detail::cptr<Alloc>;
        using void_pointer = _allocator_traits_detail::vptr<Alloc>;
        using const_void_pointer = _allocator_traits_detail::cvptr<Alloc>;
        using difference_type = _allocator_traits_detail::diff_type<Alloc>;
        using size_type = _allocator_traits_detail::size_type<Alloc>;
        // 剩下的不想写了
        // using propagate_on_container_copy_assignment =
        // using propagate_on_container_move_assignment =
        // using propagate_on_container_swap =
        // using is_always_equal =

        template <typename T>
        using rebind_alloc = decltype(_allocator_traits_rebind<Alloc, T>(0));

      public:
        [[nodiscard]] static constexpr auto allocate(Alloc& a, size_type n) -> pointer { return a.allocate(n); }

        [[nodiscard]] static constexpr auto allocate(Alloc& a, size_t n, const_void_pointer hint) -> pointer { return a.allocate(n, hint); }

        static constexpr auto deallocate(Alloc& a, pointer p, size_type n) -> void { a.deallocate(p, n); }

        template <typename T, typename... Args>
        static constexpr auto construct(Alloc& a, T* p, Args&&... args) -> void {
            if constexpr (requires { a.construct(p, std::forward<Args>(args)...); }) {
                a.construct(p, std::forward<Args>(args)...);
            } else {
                std::construct_at(p, std::forward<Args>(args)...);
            }
        }

        template <typename T>
        static constexpr auto destroy(Alloc& a, T* p) -> void {
            if constexpr (requires { a.destroy(p); }) {
                a.destroy(p);
            } else {
                std::destroy_at(p);
            }
        }

        static constexpr auto max_size(const Alloc& a) noexcept -> size_type { return a.max_size(); }

        static constexpr auto select_on_container_copy_construction(const Alloc& a) -> Alloc {
            if constexpr (requires { a.select_on_container_copy_construction(); }) {
                return a.select_on_container_copy_construction();
            } else {
                return a;
            }
        }
    };
}  // namespace mtl

// default allocator
namespace mtl {
    template <typename T>
    class allocator {
      public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;

      public:
        constexpr allocator() noexcept = default;

        constexpr allocator(const allocator&) noexcept = default;

        template <typename U>
        constexpr allocator(const allocator<U>&) noexcept {}

        constexpr ~allocator() = default;

      public:
        [[nodiscard]] constexpr auto allocatr(size_t n) -> T* {
            if (std::numeric_limits<size_t>::max() / sizeof(T) < n) {
                throw std::bad_array_new_length{};
            }
            return ::operator new(n * sizeof(T));
        }

        constexpr auto deallocate(T* p, size_t n) -> void {
            ::operator delete(p);
        }
    };
}  // namespace mtl