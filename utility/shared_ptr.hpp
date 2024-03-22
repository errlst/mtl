#pragma once
#include "utility.hpp"  // IWYU pragma: keep
#include "unique_ptr.hpp"
#include <atomic>
#include <functional>

namespace mtl {
    struct bad_weak_ptr : public std::exception {};
}  // namespace mtl

//  storage
namespace mtl {
    template <typename T>
    struct _shared_ptr_ctlblk {
      public:
        using element_type = std::remove_extent_t<T>;
        using deleter = std::function<void(element_type*)>;          // deleter::operator()
        using allocator = std::function<void(_shared_ptr_ctlblk*)>;  // allocator::deallocate

      public:
        _shared_ptr_ctlblk() {
            del = [](element_type* p) { default_delete<T>{}(p); };
            alc = [](_shared_ptr_ctlblk<T>* p) { delete p; };
        }

        template <typename Y>
        _shared_ptr_ctlblk(Y* p) : _shared_ptr_ctlblk() { ptr = p; }

      public:
        constexpr auto inc_s() { ++s_count; }

        constexpr auto dec_s() {
            if (--s_count == 0 && ptr) {
                del(ptr);
            }
        }

        constexpr auto inc_w() { ++w_count; }

        constexpr auto dec_w() {
            if (--w_count == 0 && s_count == 0) {
                alc(this);
            }
        }

      public:
        element_type* ptr;
        deleter del;
        allocator alc;
        std::atomic_uint64_t s_count;
        std::atomic_uint64_t w_count;
    };
}  // namespace mtl

namespace mtl {

    template <typename T>
    class shared_ptr {
      public:
        using element_type = std::remove_extent_t<T>;

        // 构造
      public:
        constexpr shared_ptr() noexcept {};

        template <typename Y>
        requires(std::is_convertible_v<Y (*)[], T*> || std::is_convertible_v<Y*, T*>)
        explicit shared_ptr(Y* p) : m_ptr(p), m_ctlblk(new _shared_ptr_ctlblk<T>(p)) {
            m_ctlblk->inc_s();
        }

        template <typename Y, typename D>
        requires(std::is_move_constructible_v<D> && requires { D{}(std::declval<Y*>()); } &&
                 (std::is_convertible_v<Y (*)[], T*> || std::is_convertible_v<Y*, T*>))
        shared_ptr(Y* p, D d) : m_ptr(p), m_ctlblk(new _shared_ptr_ctlblk<T>(p)) {
            m_ctlblk->del = [d = std::move(d)](T* p) { d(p); };
            m_ctlblk->inc_s();
        }

        template <typename Y, typename D, typename A>
        shared_ptr(Y* p, D d, A a) {
            
        }

        ~shared_ptr() {
            m_ctlblk->dec_s();
        }

        // observer
      public:
        auto get() const noexcept -> element_type* { return m_ptr; }

        auto use_count() const noexcept -> size_t { return m_ctlblk->s_count; }

      public:
        element_type* m_ptr;
        _shared_ptr_ctlblk<T>* m_ctlblk;
    };
}  // namespace mtl