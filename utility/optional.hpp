/*
    参考：
        https://timsong-cpp.github.io/cppwp/n4861/optional

    TODO 为实现哈希支持
*/
#pragma once
#include "utility.hpp"

// bad opt access
namespace mtl {
    struct bad_optional_access : public std::exception {};
}  // namespace mtl

// nullopt
namespace mtl {
    // nullopt_t 不能拥有默认构造函数，因此创建一个外部不可见的类，辅助构造 nullopt
    namespace {
        struct nullopt_t_helper {};
    }  // namespace

    struct nullopt_t {
        explicit constexpr nullopt_t(nullopt_t_helper) {}
    };
    inline constexpr auto nullopt = nullopt_t{nullopt_t_helper{}};
}  // namespace mtl

// optional
namespace mtl {
    template <typename T>
    class optional {
        template <typename U>
        friend class optional;

      public:
        using value_type = T;

        // 构造
      public:
        constexpr optional() noexcept {}

        constexpr optional(std::nullptr_t) noexcept {}

        constexpr optional(const optional& o)
        requires(std::is_copy_constructible_v<T>)
            : m_val(o.has_value() ? new T(o.value()) : nullptr) {}

        constexpr optional(optional&& o) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires(std::is_move_constructible_v<T>)
            : m_val(o.has_value() ? new T(std::move(o.value())) : nullptr) {}  // 移动后 o 依然有值，但是无效

        template <typename... Args>
        requires(std::is_constructible_v<T, Args...>)
        constexpr explicit optional(in_place_t, Args... args)
            : m_val(new T(std::forward<Args>(args)...)) {}

        // 模板推导无法推导 init_list 类型，需要提供以 init_list 作为参数的重载
        template <typename U, typename... Args>
        requires(std::is_constructible_v<std::initializer_list<U>, Args...>)
        constexpr explicit optional(std::initializer_list<U> lst, Args&&... args)
            : m_val(new T(lst, std::forward<Args>(args)...)) {}

        template <typename U = T>
        requires(std::is_constructible_v<T, U> &&
                 !std::is_same_v<std::remove_cvref_t<U>, in_place_t> &&
                 !std::is_same_v<std::remove_cvref_t<U>, optional>)
        constexpr explicit(!std::is_convertible_v<U, T>)
            optional(U&& u)
            : m_val(new T(std::forward<U>(u))) {}

        // 忽略剩余的约束
        template <typename U>
        requires(std::is_constructible_v<T, const U&>)
        constexpr explicit(!std::is_convertible_v<const U&, T>)
            optional(const optional<U>& o)
            : m_val(o.has_value() ? new T(o.value()) : nullptr) {}

        template <typename U>
        requires(std::is_constructible_v<T, U>)
        constexpr explicit(!std::is_convertible_v<U, T>)
            optional(optional<U>&& o)
            : m_val(o.has_value() ? new T(std::move(o.value())) : nullptr) {}

        // 析构
      public:
        ~optional() {
            if (has_value()) {
                delete m_val;
            }
        }

        // assignments
      public:
        constexpr auto operator=(nullopt_t) noexcept -> optional& {
            if (has_value()) {
                reset();
            }
            return *this;
        }

        template <typename U = T>
        constexpr auto operator=(U&& u)
        requires(!std::is_same_v<std::remove_reference_t<U>, optional> &&
                 !std::conjunction_v<std::is_scalar<T>, std::is_same<T, std::decay_t<U>>> &&
                 std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>)
        {
            reset(std::forward<U>(u));
            return *this;
        }

        // 省略约束，且借助模板省略了两个 assignments 函数
        template <typename U = T>
        constexpr auto operator=(const optional<U>& o)
        requires(std::is_constructible_v<T, const U&> &&
                 std::is_assignable_v<T&, const U&>)
        {
            if (o.has_value()) {
                reset(o.value());
            } else {
                reset();
            }
            return *this;
        }

        template <typename U = T>
        constexpr auto operator=(optional<U>&& o)
        requires(std::is_constructible_v<T, U> &&
                 std::is_assignable_v<T, U>)
        {
            if (o.has_value()) {
                reset(std::move(o.value()));
            } else {
                reset();
            }
            return *this;
        }

        // emplace
      public:
        template <typename... Args>
        requires(std::is_constructible_v<T, Args...>)
        constexpr auto emplace(Args&&... args) -> T& {
            try {
                if (has_value()) {
                    std::destroy_at(m_val);
                    std::construct_at(m_val, std::forward<Args>(args)...);
                } else {
                    m_val = new T(std::forward<Args>(args)...);
                }
            } catch (...) {
                if (has_value()) {
                    ::operator delete(m_val);
                    m_val = nullptr;
                }
                throw;
            }
            return value();
        }

        template <typename U, typename... Args>
        requires(std::is_constructible_v<T, std::initializer_list<U>, Args && ...>)
        auto emplace(std::initializer_list<U> lst, Args&&... args) -> T& {
            return emplace(std::move(lst), args...);
        }

        // swap
      public:
        constexpr auto swap(optional& o) noexcept(
            std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_swappable_v<T>) -> void
        requires(std::is_move_constructible_v<T> && std::is_swappable_v<T>)
        {
            if (has_value() == o.has_value()) {
                std::swap(value(), o.value());
            } else if (o.has_value()) {
                reset(std::move(o.value()));
                o.reset();
            } else {
                o.reset(std::move(value()));
                reset();
            }
        }

        // observer
      public:
        constexpr auto operator->() const -> const T* { return m_val; }

        constexpr auto operator*() & -> T& { return *m_val; }

        constexpr auto operator*() const& -> const T& { return *m_val; }

        constexpr auto operator*() && -> T&& { return std::move(*m_val); }

        constexpr auto operator*() const&& -> const T&& { return std::move(*m_val); }

        // value
      public:
        constexpr auto has_value() const noexcept -> bool {
            return m_val != nullptr;
        }

        constexpr auto value() & -> T& {
            return has_value() ? *m_val : throw bad_optional_access{};
        }

        constexpr auto value() const& -> const T& {
            return has_value() ? *m_val : throw bad_optional_access{};
        }

        constexpr auto value() && -> T&& {
            return has_value() ? std::move(*m_val) : throw bad_optional_access{};
        }

        constexpr auto value() const&& -> const T&& {
            return has_value() ? std::move(*m_val) : throw bad_optional_access{};
        }

        template <typename U>
        requires(std::is_copy_constructible_v<T> && std::is_convertible_v<U &&, T>)
        auto value_or(U&& u) const& -> T {
            return has_value() ? *m_val : static_cast<T>(std::forward<U>(u));
        }

        template <typename U>
        requires(std::is_move_constructible_v<T> && std::is_convertible_v<U &&, T>)
        auto value_or(U&& u) && -> T {
            return has_value() ? std::move(*m_val) : static_cast<T>(std::forward<U>(u));
        }

        // reset
      public:
        constexpr auto reset() noexcept {
            if (has_value()) {
                delete m_val;
                m_val = nullptr;
            }
        }

      private:
        template <typename U>
        constexpr auto reset(U&& u) noexcept {
            if (has_value()) {
                m_val = std::forward<U>(u);
            } else {
                m_val = new T(std::forward<U>(u));
            }
        }

      private:
        T* m_val = nullptr;
    };
}  // namespace mtl

// relational operator
namespace mtl {
    template <typename T, typename U>
    constexpr auto operator==(const optional<T>& lhs, const optional<U>& rhs) -> bool { return lhs.has_value() == rhs.has_value() ? lhs.value() == rhs.value() : false; }

    template <typename T, typename U>
    constexpr auto operator!=(const optional<T>& lhs, const optional<U>& rhs) -> bool { return !(lhs == rhs); }

    template <typename T, typename U>
    constexpr auto operator<(const optional<T>& lhs, const optional<U>& rhs) -> bool {
        if (lhs.has_value() && rhs.has_value()) {
            return lhs < rhs;
        } else if (rhs.has_value()) {
            return true;
        }
        return false;
    }

    template <typename T, typename U>
    constexpr auto operator>(const optional<T>& lhs, const optional<U>& rhs) -> bool { return !(lhs < rhs) && !(lhs == rhs); }

    template <typename T, typename U>
    constexpr auto operator<=(const optional<T>& lhs, const optional<U>& rhs) -> bool { return (lhs < rhs) || (lhs == rhs); }

    template <typename T, typename U>
    constexpr auto operator>=(const optional<T>& lhs, const optional<U>& rhs) -> bool { return (lhs > rhs) || (lhs == rhs); }

    template <typename T, std::three_way_comparable_with<T> U>
    constexpr auto operator<=>(const optional<T>& lhs, const optional<U>& rhs) -> std::compare_three_way_result_t<T, U> { return lhs.has_value() && rhs.has_value() ? lhs <=> rhs : static_cast<bool>(lhs) <=> static_cast<bool>(rhs); }

    template <typename T>
    constexpr auto operator==(const optional<T>& o, nullopt_t) noexcept -> bool { return !o; }

    template <typename T>
    constexpr auto operator<=>(const optional<T>& o, nullopt_t) noexcept -> std::strong_ordering { return static_cast<bool>(o) <=> false; }

    template <typename T, typename U>
    constexpr auto operator==(const optional<T>& o, const U& u) -> bool { return o.has_value() ? o.value() == u : false; }

    template <typename T, typename U>
    constexpr auto operator==(const U& u, const optional<T>& o) -> bool { return o == u; }

    template <typename T, typename U>
    constexpr auto operator!=(const optional<T>& u, const U& o) -> bool { return !(o == u); }

    template <typename T, typename U>
    constexpr auto operator!=(const U& u, const optional<T>& o) -> bool { return o != u; }

    template <typename T, typename U>
    constexpr auto operator<(const optional<T>& o, const U& u) -> bool { return o.has_value() ? o.value() < u : false; }

    template <typename T, typename U>
    constexpr auto operator<(const U& u, const optional<T>& o) -> bool { return o >= u; }

    template <typename T, typename U>
    constexpr auto operator>(const optional<T>& o, const U& u) -> bool { return !(o < u) && !(o == u); }

    template <typename T, typename U>
    constexpr auto operator>(const U& u, const optional<T>& o) -> bool { return o <= u; }

    template <typename T, typename U>
    constexpr auto operator<=(const optional<T>& o, const U& u) -> bool { return o < u || o == u; }

    template <typename T, typename U>
    constexpr auto operator<=(const U& u, const optional<T>& o) -> bool { return o > u; }

    template <typename T, typename U>
    constexpr auto operator>=(const optional<T>& o, const U& u) -> bool { return o > u || o == u; }

    template <typename T, typename U>
    constexpr auto operator>=(const U& u, const optional<T>& o) -> bool { return o < u; }

    template <typename T, std::three_way_comparable_with<T> U>
    constexpr auto operator<=>(const optional<T>& o, const U& u) -> std::compare_three_way_result_t<T, U> { return o.has_value() ? o.value() <=> u : std::strong_ordering::less; }
}  // namespace mtl

// swap
namespace mtl {
    template <typename T>
    auto swap(optional<T>& lhs, optional<T>& rhs) noexcept(noexcept(lhs.swap(rhs))) -> void
    requires(std::is_move_constructible_v<T> && std::is_swappable_v<T>)
    { lhs.swap(rhs); }
}  // namespace mtl

// make
namespace mtl {
    template <typename T>
    constexpr auto make_optional(T&& t) -> optional<std::decay_t<T>> { return {std::forward<T>(t)}; }

    template <typename T, typename... Args>
    constexpr auto make_optional(Args&&... args) -> optional<T> { return {in_place, std::forward<Args>(args)...}; }

    template <typename T, typename U, typename... Args>
    constexpr auto make_optional(std::initializer_list<U> lst, Args&&... args) -> optional<T> { return {in_place, lst, std::forward<Args>(args)...}; }
}  // namespace mtl