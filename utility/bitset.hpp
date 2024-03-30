#pragma once
#include "utility.hpp"

//  辅助函数
namespace mtl {
    constexpr size_t BITS_PER_BYTE = 8;

    //  向上取整计算字节数
    constexpr auto _bitset_floor_byte_count(size_t bit_count) -> size_t {
        return bit_count / BITS_PER_BYTE + (bit_count % BITS_PER_BYTE != 0);
    }

    //  向下取整计算字节数
    constexpr auto _bitset_ceil_byte_count(size_t bit_count) -> size_t {
        return bit_count / BITS_PER_BYTE;
    }

    //  零散比特数量
    constexpr auto _bitset_scattered_bit_count(size_t bit_count) -> size_t {
        return bit_count % BITS_PER_BYTE;
    }
} // namespace mtl

//  转换到无符号
namespace mtl {

} // namespace mtl

// bitset
namespace mtl {
    //  bitset 使用小端存储的方式，将比特序列存放在数组中，即：对于比特序列 0011 00001111
    //      bytes[0] == 0011，bytes[1] == 00001111
    //  此实现只能运行在小端存储的平台上。
    template <size_t N>
    class bitset {
        static_assert(std::endian::native == std::endian::little, "bitset only work in little endian");
        static constexpr auto m_bytes_size = _bitset_floor_byte_count(N);

      public:
        class reference;

        // 构造
      public:
        constexpr bitset() noexcept = default;

        //  假设：
        //      val 仅为32位，且对应的二进制序列为 00001111 11110000 10100101 01011010
        //      val 的内存模型为  (低地址 [01011010] [10100101] [11110000] [00001111] 高地址)
        //      若 N=9，则预期 m_bytes 的内存模型为 ([00000001] [01011010])
        //      若 N=15，则预期 m_bytes 的内存模型为 ([00000000] [10100101] [01011010])
        //  构造函数的实现流程为：
        //      1. 对比特数量向上取整，得到有效字节数量
        //      2. 将有效字节数量的数据直接拷贝到 m_bytes 对应内存中
        //      3. 如果存在零散比特位，对 m_bytes 中的最后一个进行高位零覆盖处理。
        constexpr bitset(unsigned long long val) noexcept {
            auto bit_count = std::min(sizeof(val) * BITS_PER_BYTE, N);
            auto val_ptr = reinterpret_cast<uint8_t *>(&val);
            auto byte_ptr = &(m_bytes[m_bytes_size - 1]);
            //  有效字节
            for (auto i = 0; i < _bitset_floor_byte_count(bit_count); ++i) {
                *(byte_ptr--) = *(val_ptr++);
            }
            //  零覆盖最后一个有效字节中的无效比特
            ++byte_ptr;
            for (auto i = 0, mask = 0b11111111; i < BITS_PER_BYTE - _bitset_scattered_bit_count(bit_count); ++i) {
                mask >>= 1;
                *byte_ptr &= mask;
            }
        }

        //
        template <typename CharT, typename Traits, typename Allocator>
        explicit bitset(const std::basic_string<CharT, Traits, Allocator> &str,
                        size_t pos = 0, size_t n = -1,
                        CharT zero = '0', CharT one = '1') {
            auto valid_bit_count = std::min(N, std::min(N, std::min(n, str.size() - pos)));
            auto str_iter = str.begin() + valid_bit_count - 1;
            auto byte = const_cast<uint8_t *>(&m_bytes[m_bytes_size] - 1);
            while (valid_bit_count > 0) {
                auto i = 0, mask = 1;
                while (++i <= 8) {
                    if (Traits::eq(*(str_iter--), one)) {
                        *byte |= mask;
                    }
                    mask <<= 1, valid_bit_count -= 1;
                    if (valid_bit_count == 0) {
                        break;
                    }
                }
                --byte;
            }
        }

        template <typename CharT>
        explicit bitset(const CharT *str, size_t n = -1, CharT zero = '0', CharT one = '1')
            : bitset(n == -1 ? std::basic_string<CharT>(str) : std::basic_string<CharT>(str, n), 0, n, zero, one) {}

        // binary operator
      public:
        auto operator&=(const bitset &other) noexcept -> bitset & {
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] &= other.m_bytes[i];
            }
            return *this;
        }

        auto operator|=(const bitset &other) noexcept -> bitset & {
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] |= other.m_bytes[i];
            }
            return *this;
        }

        auto operator^=(const bitset &other) noexcept -> bitset & {
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] ^= other.m_bytes[i];
            }
        }

        auto operator~() const noexcept -> bitset {
            auto res = bitset{};
            for (auto i = 0; i < m_bytes_size; ++i) {
                res.m_bytes[i] = ~m_bytes[i];
            }
            // bytes[0] 的无效比特位需要置零
            for (auto i = 0, mask = 127; i < 8 - _bitset_scattered_bit_count(N); ++i, mask >>= 1) {
                res.m_bytes[0] &= mask;
            }
            return res;
        }

        // relational operator
      public:
        auto operator==(const bitset &o) -> bool {
            for (auto i = 0; i < m_bytes_size; ++i) {
                if (m_bytes[i] != o.m_bytes[i]) {
                    return false;
                }
            }
            return true;
        }

        //  二进制位移操作的基本思路为：将两个 byte 合并为一个 word，并对 word 进行有限的位移操作（一次最多只能移动8位），再将移位后的结果返回给 byte
        //  假设：
        //      m_bytes[0]==00001111，m_bytes[1]==11110000
        //      进行 >>4 操作：
        //          1. 将 [0]和[1] 合并为 word，word==00001111'11110000
        //          2. 对 word 进行 >>4 操作，word==00000000'11111111
        //          3. [0]=word_h==00000000，[1]=word_h==11111111
        //  如果直接使用 m_bytes 接受 word 移位后的结果，会导致后续的移位操作的操作数是移位后的结果，因此需要将结果暂存到临时量上，并在移位完成后拷贝
      public:
        auto operator<<=(size_t pos) -> bitset & {
            if (pos > BITS_PER_BYTE) {
                (*this) <<= (pos - BITS_PER_BYTE);
                pos = BITS_PER_BYTE;
            }
            uint8_t bytes_tmp[m_bytes_size]{0};
            auto byte_tmp = &bytes_tmp[m_bytes_size - 1];
            auto m_byte = &m_bytes[m_bytes_size - 1];
            auto word = uint16_t{0};
            auto word_l = reinterpret_cast<uint8_t *>(&word), word_h = word_l + 1; // word 低字节和高字节
            for (auto i = 0; i < m_bytes_size - 1; ++i) {
                *word_l = *m_byte;
                *word_h = *(--m_byte);
                word <<= pos;
                *(byte_tmp) |= *word_l;
                *(--byte_tmp) |= *word_h;
            }
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] = bytes_tmp[i];
            }
            return *this;
        }

        auto operator<<(size_t pos) -> bitset {
            auto res = bitset{};
            for (auto i = 0; i < m_bytes_size; ++i) {
                res.m_bytes[i] = m_bytes[i];
            }
            res <<= pos;
            return res;
        }

        auto operator>>=(size_t pos) -> bitset & {
            if (pos > BITS_PER_BYTE) {
                (*this) >>= (pos - BITS_PER_BYTE);
                pos = 8;
            }
            uint8_t bytes[m_bytes_size]{0};
            auto byte = bytes;
            auto m_byte = m_bytes;
            auto word = uint16_t{0};
            auto word_r = reinterpret_cast<uint16_t *>(&word), word_l = word_r + 1;
            for (auto i = 0; i < m_bytes_size; ++i) {
                *word_l = *m_byte;
                *word_r = *(++m_byte);
                word >>= pos;
                *(byte) |= *word_l;
                *(++byte) |= *word_r;
            }
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] = bytes[i];
            }
            return *this;
        }

        auto operator>>(size_t pos) -> bitset {
            auto res = bitset{};
            for (auto i = 0; i < m_bytes_size; ++i) {
                res.m_bytes[i] = m_bytes[i];
            }
            res >>= pos;
            return res;
        }

        //  to_xxx
      public:
        template <typename CharT = char, typename Traits = std::char_traits<CharT>,
                  typename Allocator = std::allocator<CharT>>
        auto to_string(CharT zero = '0', CharT one = '1') -> std::basic_string<CharT, Traits, Allocator> {
            auto res = std::basic_string<CharT, Traits, Allocator>(N, zero);
            auto res_iter = res.end() - 1;
            auto byte_ptr = &m_bytes[m_bytes_size - 1];
            //  处理完整字节
            for (auto i = 0; i < _bitset_ceil_byte_count(N); ++i, --byte_ptr) {
                for (auto j = 0, mask = 1; j < BITS_PER_BYTE; ++j, --res_iter, mask <<= 1) {
                    if ((*byte_ptr & mask) != 0) {
                        *res_iter = one;
                    }
                }
            }
            //  处理零散比特
            for (auto i = 0, mask = 1; i < _bitset_scattered_bit_count(N); ++i, --res_iter, mask <<= 1) {
                if ((*byte_ptr & mask) != 0) {
                    *res_iter = one;
                }
            }
            return res;
        }

        auto to_ulong() const -> unsigned long { return to_unsigned<unsigned long>(); }

        auto to_ullong() const -> unsigned long long { return to_unsigned<unsigned long long>(); }

      private:
        template <std::unsigned_integral T>
        constexpr auto to_unsigned() const -> T {
            if (N == 0) {
                throw std::overflow_error{""};
            }
            auto res = T{0};
            auto res_ptr = reinterpret_cast<uint8_t *>(&res);
            auto byte = &m_bytes[m_bytes_size - 1];
            for (size_t i = 0; i < std::min(sizeof(T), m_bytes_size); ++i) {
                *(res_ptr++) = *(byte--);
            }
            return res;
        }

        // modifier
      public:
        auto set() noexcept -> bitset & {
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] = 255;
            }
            return *this;
        }

        auto set(size_t pos, bool val = true) -> bitset & {
            if (pos >= N) {
                throw std::out_of_range{""};
            }
            (*this)[pos] = val;
            return *this;
        }

        auto reset() noexcept -> bitset & {
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] = 0;
            }
            return *this;
        }

        auto reset(size_t pos) { set(pos, false); }

        auto flip() noexcept -> bitset & {
            for (auto i = 0; i < m_bytes_size; ++i) {
                m_bytes[i] = ~m_bytes[i];
            }
            return *this;
        }

        auto flip(size_t pos) -> bitset & {
            if (pos >= N) {
                throw std::out_of_range{""};
            }
            (*this)[pos].flip();
            return *this;
        }

        // access
      public:
        auto operator[](size_t pos) -> reference {
            return reference{const_cast<uint8_t *>(m_bytes + m_bytes_size - 1 - _bitset_ceil_byte_count(pos)),
                             static_cast<uint8_t>(pos % BITS_PER_BYTE)};
        }

        constexpr auto operator[](size_t pos) const -> reference {
            return reference{const_cast<uint8_t *>(m_bytes + m_bytes_size - 1 - _bitset_ceil_byte_count(pos)),
                             static_cast<uint8_t>(pos % BITS_PER_BYTE)};
        }

        auto count() const noexcept -> size_t {
            size_t res = 0;
            for (auto i = 0; i < m_bytes_size; ++i) {
                auto byte = m_bytes[i];
                for (auto j = 0; j < 8; ++j, byte >>= 1) {
                    res += ((byte & 1) != 0);
                }
            }
            return res;
        }

        constexpr auto size() const noexcept -> size_t { return N; }

        constexpr auto test(size_t pos) const -> bool {
            if (pos >= N) {
                throw std::out_of_range{""};
            }
            return (*this)[pos];
        }

        auto all() const noexcept { return count() == size(); }

        auto any() const noexcept { return count() != 0; }

        auto none() const noexcept { return count() == 0; }

      public:
        uint8_t m_bytes[m_bytes_size]{0};
    };
} // namespace mtl

// bitset::reference
namespace mtl {
    //  通过 <byte,pos> 表示某个比特，pos 表示该比特在该字节中，从低位开始计算的位置。
    //  即 <byte,0> 表示 byte 中的第0个比特。
    template <size_t N>
    class bitset<N>::reference {
      public:
        reference(uint8_t *byte, uint8_t offset) : m_byte{byte}, m_pos{offset} {}

        reference(const reference &) = default;

        ~reference() {}

      public:
        auto operator=(bool b) noexcept -> reference & {
            if (b != *this) {
                flip();
            }
            return *this;
        }

        auto operator=(const reference &r) noexcept -> reference & {
            m_byte = r.m_byte;
            m_pos = r.m_pos;
            return *this;
        }

        auto operator!() const noexcept -> bool { return !*this; }

        operator bool() const noexcept { return *m_byte & (1 << m_pos); }

        auto flip() noexcept -> reference & {
            if (*this) {
                *m_byte &= 255 ^ (1 << m_pos);
            } else {
                *m_byte |= 1 << m_pos;
            }
            return *this;
        }

      private:
        uint8_t *m_byte;
        uint8_t m_pos; // 从右往左，从0开始计算；即off=0时，m_byte[off]=m_byte[7]
    };
} // namespace mtl