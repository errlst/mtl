/*
    https://timsong-cpp.github.io/cppwp/n4861/memory
    https://codereview.stackexchange.com/questions/220090/c17-pointer-traits-implementation pointer_traits 实现
*/
#pragma once
#include "utility.hpp"  // IWYU pragma: keep

// pointer traits
namespace mtl {
    template <typename Ptr>
    auto _pointer_traits_element_type(int) -> Ptr::element_type;

    template <template <typename, typename...> typename Ptr, typename T, typename... Args>
    auto _pointer_traits_element_type(Ptr<T, Args...>) -> T;

    template <typename Ptr>
    auto _pointer_traits_element_type(long) -> decltype(_pointer_traits_element_type(Ptr{}));

    template <typename Ptr, typename U>
    auto _pointer_traits_rebind_type(int) -> typename Ptr::template rebind<U>;

    template <typename U, template <typename, typename...> typename Ptr, typename T, typename... Args>
    auto _pointer_traits_rebind_type(Ptr<T, Args...>) -> Ptr<U, Args...>;

    template <typename Ptr, typename U>
    auto _pointer_traits_rebind_type(long) -> decltype(_pointer_traits_rebind_type<U>(Ptr{}));

    template <typename Ptr>
    struct pointer_traits {
        using pointer = Ptr;
        using element_type = decltype(_pointer_traits_element_type<Ptr>(0));
        using difference_type = default_or_t<ptrdiff_t, typename Ptr::difference_type>;

        template <typename U>
        using rebind = decltype(_pointer_traits_rebind_type<Ptr, U>(0));

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
    template <typename Allocator, typename T>
    constexpr auto _allocator_traits_rebind(int) -> typename Allocator::template rebind<T>::other;

    template <template <typename...> typename Allocator, typename T, typename... Types>
    constexpr auto _allocator_traits_rebind(Allocator<Types...>) -> Allocator<T, Types...>;

    template <typename Allocator, typename T>
    constexpr auto _allocator_traits_rebind(long) -> decltype(_allocator_traits_rebind<Allocator, T>(Allocator{}));

    template <typename Allocator>
    struct allocator_traits {
        using allocator_type = Allocator;
        using value_type = Allocator::value_type;
        using pointer = default_or_t<value_type*, typename Allocator::pointer>;
        using const_pointer = default_or_t<typename pointer_traits<pointer>::template rebind<const value_type>, typename Allocator::const_pointer>;
        using void_pointer = default_or_t<typename pointer_traits<pointer>::template rebind<void>, typename Allocator::void_pointer>;
        using const_void_pointer = default_or_t<typename pointer_traits<pointer>::template rebind<const void>, typename Allocator::const_void_pointer>;
        using difference_type = default_or_t<typename pointer_traits<pointer>::difference_type, typename Allocator::difference_type>;
        using size_type = default_or_t<std::make_unsigned_t<difference_type>, typename Allocator::size_type>;
        using propagate_on_container_copy_assignment = default_or_t<std::false_type, typename Allocator::propagate_on_container_copy_assignment>;
        using propagate_on_container_move_assignment = default_or_t<std::false_type, typename Allocator::propagate_on_container_move_assignment>;
        using propagate_on_container_swap = default_or_t<std::false_type, typename Allocator::propagate_on_container_swap>;
        using is_always_equal = default_or_t<typename std::is_empty<Allocator>::type, typename Allocator::is_always_equal>;

        template <typename T>
        using rebind_alloc = decltype(_allocator_traits_rebind<Allocator, T>(0));

      public:
        [[nodiscard]] static constexpr auto allocate(Allocator& a, size_type n) -> pointer { return a.allocate(n); }

        [[nodiscard]] static constexpr auto allocate(Allocator& a, size_t n, const_void_pointer hint) -> pointer { return a.allocate(n, hint); }

        static constexpr auto deallocate(Allocator& a, pointer p, size_type n) -> void { a.deallocate(p, n); }

        template <typename T, typename... Args>
        static constexpr auto construct(Allocator& a, T* p, Args&&... args) -> void {
            if constexpr (requires { a.construct(p, std::forward<Args>(args)...); }) {
                a.construct(p, std::forward<Args>(args)...);
            } else {
                std::construct_at(p, std::forward<Args>(args)...);
            }
        }

        template <typename T>
        static constexpr auto destroy(Allocator& a, T* p) -> void {
            if constexpr (requires { a.destroy(p); }) {
                a.destroy(p);
            } else {
                std::destroy_at(p);
            }
        }

        static constexpr auto max_size(const Allocator& a) noexcept -> size_type { return a.max_size(); }

        static constexpr auto select_on_container_copy_construction(const Allocator& a) -> Allocator {
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