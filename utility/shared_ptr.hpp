#pragma once
#include "unique_ptr.hpp"
#include "utility.hpp"
#include <functional>
#include <mutex>

namespace mtl {
    struct bad_weak_ptr : public std::exception {};
} // namespace mtl

// storage
namespace mtl {
    template <typename T>
    struct _shared_ptr_ctlblk {
      public:
        using element_type = std::remove_extent_t<T>;
        using deleter = std::function<void(element_type *)>; // 通过 deleter::operator()(ptr) 进行删除器操作

      public:
        _shared_ptr_ctlblk()
            : del{[](element_type *p) { default_delete<T>{}(p); }} {}

        template <typename Y>
        _shared_ptr_ctlblk(Y *p) : _shared_ptr_ctlblk() { ptr = p; }

      public:
        auto inc_s() {
            auto lock = std::lock_guard{m_mut};
            ++s_count;
        }

        auto dec_s() {
            auto lock = std::lock_guard{m_mut};
            if (--s_count == 0 && ptr) {
                del(ptr);
                if (w_count == 0) {
                    delete this;
                }
            }
        }

        auto inc_w() {
            auto lock = std::lock_guard{m_mut};
            ++w_count;
        }

        auto dec_w() {
            auto lock = std::lock_guard{m_mut};
            if (--w_count == 0 && s_count == 0) {
                delete this;
            }
        }

      public:
        element_type *ptr;
        deleter del;
        size_t s_count{0};
        size_t w_count{0};
        std::mutex m_mut; // 保证 inc 和 dec 为原子操作
    };
} // namespace mtl

// weak_ptr
namespace mtl {
    template <typename T>
    class weak_ptr {
      public:
        using element_type = std::remove_extent_t<T>;

        // 构造
      public:
        constexpr weak_ptr() noexcept {};

        weak_ptr(const weak_ptr &w) noexcept : m_ctlblk(w.m_ctlblk) {
            if (m_ctlblk) {
                m_ctlblk->inc_w();
            }
        }

        template <typename U>
        weak_ptr(const weak_ptr<U> &w) noexcept : m_ctlblk(w.m_ctlblk) {
            if (m_ctlblk) {
                m_ctlblk->inc_w();
            }
        }

        template <typename U>
        weak_ptr(const shared_ptr<U> &s) noexcept : m_ctlblk(s.m_ctlblk) {
            if (m_ctlblk) {
                m_ctlblk->inc_w();
            }
        }

        weak_ptr(weak_ptr &&w) noexcept {
            if (m_ctlblk) {
                m_ctlblk->dec_w();
            }
            m_ctlblk = w.m_ctlblk;
            w.m_ctlblk = nullptr;
        }

        template <typename U>
        weak_ptr(weak_ptr<U> &&w) noexcept {
            if (m_ctlblk) {
                m_ctlblk->dec_w();
            }
            m_ctlblk = w.m_ctlblk;
            w.m_ctlblk = nullptr;
        }

        ~weak_ptr() {
            if (m_ctlblk) {
                m_ctlblk->dec_w();
            }
        }

        // assignments
      public:
        auto operator=(const weak_ptr &w) noexcept -> weak_ptr & {
            weak_ptr(w).swap(*this);
            return *this;
        }

        template <typename U>
        auto operator=(const weak_ptr<U> &w) noexcept -> weak_ptr & {
            weak_ptr(w).swap(*this);
            return *this;
        }

        template <typename U>
        auto operator=(const shared_ptr<U> &s) noexcept -> weak_ptr & {
            weak_ptr(s).swap(*this);
            return *this;
        }

        auto operator=(weak_ptr &&w) noexcept -> weak_ptr & {
            weak_ptr(std::move(w)).swap(*this);
            return *this;
        }

        template <typename U>
        auto operator=(weak_ptr<U> &&w) noexcept -> weak_ptr & {
            weak_ptr(std::move(w)).swap(*this);
            return *this;
        }

        // modfier
      public:
        auto swap(weak_ptr &w) noexcept {
            std::swap(w.m_ctlblk, m_ctlblk); // mutex 不需要且不能交换
        }

        auto reset() noexcept { weak_ptr().swap(*this); }

        // observer
      public:
        auto use_count() const noexcept -> size_t { return m_ctlblk == nullptr ? 0 : m_ctlblk->s_count; }

        auto expired() const noexcept -> bool { return use_count() == 0; }

        auto lock() const noexcept -> shared_ptr<T> {
            auto lock = std::lock_guard{m_mut};
            return expired() ? shared_ptr<T>() : shared_ptr<T>(*this);
        }

        template <typename U>
        auto owner_before(const shared_ptr<U> &s) const noexcept { return s.m_ctlblk == m_ctlblk; }

        template <typename U>
        auto owner_before(const weak_ptr<U> &w) const noexcept { return w.m_ctlblk == m_ctlblk; }

      public:
        _shared_ptr_ctlblk<T> *m_ctlblk{nullptr};
        std::mutex m_mut; // 确保 lock() 原子执行
    };
} // namespace mtl

// shared_ptr，忽略 Allocator 的作用，忽略 nullptr 相关构造函数
namespace mtl {
    //  判断是否是继承自 enable_shared_from_this
    template <typename Derive>
    class is_derived_from_esft {
        template <typename T>
        static constexpr auto test(const enable_shared_from_this<T> *) -> std::true_type;
        static constexpr auto test(...) -> std::false_type;

      public:
        static constexpr bool value = decltype(test(std::declval<Derive *>()))::value;
    };

    template <typename Derive>
    constexpr bool is_derived_from_esft_v = is_derived_from_esft<Derive>::value;

    template <typename T>
    class shared_ptr {
      public:
        using element_type = std::remove_extent_t<T>;

        // 构造
      public:
        constexpr shared_ptr() noexcept {}

        template <typename U>
            requires(std::is_convertible_v<U (*)[], T *> ||
                     std::is_convertible_v<U *, T *>)
        explicit shared_ptr(U *p) : m_ptr(p), m_ctlblk(new _shared_ptr_ctlblk<T>(p)) {
            m_ctlblk->inc_s();
            // init enable_shared_from_this
            if constexpr (is_derived_from_esft_v<U>) {
                if (p != nullptr && p->m_this.expired())
                    p->m_this = shared_ptr<std::remove_cv_t<U>>(*this, const_cast<std::remove_cv_t<U> *>(p));
            }
        }

        template <typename U, typename D>
            requires(std::is_move_constructible_v<D> &&
                     requires { D{}(std::declval<U *>()); } &&
                     (std::is_convertible_v<U (*)[], T *> ||
                      std::is_convertible_v<U *, T *>))
        shared_ptr(U *p, D d) : shared_ptr(p) {
            m_ctlblk->del = [d = std::move(d)](T *p) { d(p); };
        }

        template <typename U, typename D, typename A>
        shared_ptr(U *p, D d, A a) : shared_ptr(p, d) {}

        template <typename U>
        shared_ptr(const shared_ptr<U> &s, element_type *p) noexcept : m_ptr(p), m_ctlblk(s.m_ctlblk) {
            if (m_ctlblk) {
                m_ctlblk->inc_s();
            }
        }

        template <typename U>
        shared_ptr(shared_ptr<U> &&s, element_type *p) noexcept : m_ptr(p) {
            if (m_ctlblk) {
                m_ctlblk->dec_s();
            }
            m_ctlblk = s.m_ctlblk;
            s.m_ptr = s.m_ctlblk = nullptr;
        }

        template <typename U>
        shared_ptr(const shared_ptr<U> &s) noexcept : m_ptr(s.m_ptr), m_ctlblk(s.m_ctlblk) {
            if (m_ctlblk) {
                m_ctlblk->inc_s();
            }
        }

        shared_ptr(const shared_ptr &s) noexcept : m_ptr(s.m_ptr), m_ctlblk(s.m_ctlblk) {
            if (m_ctlblk) {
                m_ctlblk->inc_s();
            }
        }

        template <typename U>
        shared_ptr(shared_ptr<U> &&s) noexcept : m_ptr(s.m_ptr) {
            if (m_ctlblk) {
                m_ctlblk->dec_s();
            }
            m_ctlblk = s.m_ctlblk;
            s.m_ptr = s.m_ctlblk = nullptr;
        }

        shared_ptr(shared_ptr &&s) noexcept : m_ptr(s.m_ptr) {
            if (m_ctlblk) {
                m_ctlblk->dec_s();
            }
            m_ctlblk = s.m_ctlblk;
            s.m_ptr = s.m_ctlblk = nullptr;
        }

        template <typename U>
        shared_ptr(const weak_ptr<U> &w) {
            if (w.expired()) {
                throw bad_weak_ptr();
            }
            m_ctlblk = w.m_ctlblk;
            m_ptr = m_ctlblk->ptr;
            m_ctlblk->inc_s();
        }

        template <typename U, typename D>
            requires(std::is_convertible_v<typename unique_ptr<U, D>::pointer, element_type *>)
        shared_ptr(unique_ptr<U, D> &&u) {
            if (u.get()) {
                if constexpr (std::is_lvalue_reference_v<D>) {
                    shared_ptr(u.release(), std::ref(u.get_deleter())).swap(*this);
                } else {
                    shared_ptr(u.release(), u.get_deleter()).swap(*this);
                }
            }
        }

        ~shared_ptr() {
            if (m_ctlblk) {
                m_ctlblk->dec_s();
            }
        }

        // assignment
      public:
        auto operator=(const shared_ptr &s) noexcept -> shared_ptr & {
            shared_ptr(s).swap(*this);
            return *this;
        }

        template <typename U>
        auto operator=(const shared_ptr<U> &s) noexcept -> shared_ptr & {
            shared_ptr(s).swap(*this);
            return *this;
        }

        auto operator=(shared_ptr &&s) noexcept -> shared_ptr & {
            shared_ptr(std::move(s)).swap(*this);
            return *this;
        }

        template <typename U>
        auto operator=(shared_ptr<U> &&s) noexcept -> shared_ptr & {
            shared_ptr(std::move(s)).swap(*this);
            return *this;
        }

        template <typename U, typename D>
        auto operator=(unique_ptr<U, D> &&u) -> shared_ptr & {
            shared_ptr(std::move(u)).swap(*this);
            return *this;
        }

        // observer
      public:
        auto operator*() const noexcept { return *get(); }

        auto operator->() const noexcept
            requires(!std::is_array_v<T>)
        { return get(); }

        auto operator[](std::ptrdiff_t i) const -> element_type &
            requires(std::is_array_v<T>)
        { return get()[i]; }

        explicit operator bool() const noexcept { return get(); }

        auto get() const noexcept -> element_type * { return m_ptr; }

        auto use_count() const noexcept -> size_t { return m_ctlblk == nullptr ? 0 : m_ctlblk->s_count; }

        template <typename U>
        auto owner_before(const shared_ptr<U> &s) const noexcept -> bool { return m_ctlblk == s.m_ctlblk; }

        template <typename U>
        auto owner_before(const weak_ptr<U> &w) const noexcept -> bool { return m_ctlblk == w.m_ctlblk; }

        // modifier
      public:
        auto swap(shared_ptr &s) noexcept {
            std::swap(m_ptr, s.m_ptr);
            std::swap(m_ctlblk, s.m_ctlblk);
        }

        auto reset() noexcept -> void { shared_ptr().swap(*this); }

        template <typename U>
        auto reset(U *p) -> void { shared_ptr(p).swap(*this); }

        template <typename U, typename D>
        auto reset(U *p, D d) -> void { shared_ptr(p, d).swap(*this); }

        template <typename U, typename D, typename A>
        auto reset(U *p, D d, A a) -> void { shared_ptr(p, d, a).swap(*this); }

      public:
        element_type *m_ptr{nullptr};
        _shared_ptr_ctlblk<T> *m_ctlblk{nullptr};
    };
} // namespace mtl

// enable_shared_from_this
namespace mtl {
    template <typename T>
    class enable_shared_from_this {
      protected:
        constexpr enable_shared_from_this() noexcept {}

        enable_shared_from_this(const enable_shared_from_this &) noexcept {}

        auto operator=(const enable_shared_from_this &) noexcept -> enable_shared_from_this & { return *this; }

        ~enable_shared_from_this() {}

      public:
        auto shared_from_this() -> shared_ptr<T> { return {m_this}; }

        auto shared_from_this() const -> shared_ptr<const T> { return {m_this}; }

        auto weak_from_this() noexcept -> weak_ptr<T> { return m_this; }

        auto weak_from_this() const noexcept -> weak_ptr<const T> { return m_this; }

      public:
        mutable weak_ptr<T> m_this;
    };
} // namespace mtl