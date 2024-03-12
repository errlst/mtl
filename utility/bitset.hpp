/*
    https://timsong-cpp.github.io/cppwp/n4861/bitset
*/
#pragma once
#include "utility.hpp"

// byte count
namespace mtl {
    constexpr size_t BITS_PER_BYTE = 8;

    constexpr auto _bitset_floor_byte_count(size_t bit_count) -> size_t {
        return bit_count / BITS_PER_BYTE + (bit_count % BITS_PER_BYTE != 0);
    }

    constexpr auto _bitset_ceil_byte_count(size_t bit_count) -> size_t {
        return bit_count / BITS_PER_BYTE;
    }

    // bytes[0] 中有效的比特数
    constexpr auto _bitset_bits_of_byte0(size_t bit_count) -> size_t {
        if (auto _ = bit_count % BITS_PER_BYTE; _ != 0) {
            return _;
        }
        return BITS_PER_BYTE;
    }
}  // namespace mtl

// to ulong or ullong
namespace mtl {
    template <typename T>
    requires(std::is_same_v<T, unsigned long> || std::is_same_v<T, unsigned long long>)
    constexpr auto _bitset_to_unsigned(const uint8_t *bytes, size_t bit_count) -> T {
        if (bit_count == 0) {
            throw std::overflow_error{""};
        }
        auto res = T{0};
        auto byte_count = _bitset_floor_byte_count(bit_count);
        auto dst_ptr = reinterpret_cast<uint8_t *>(&res);
        auto src_ptr = bytes + byte_count - 1;
        for (size_t i = 0, iter_times = (byte_count >= sizeof(T) ? sizeof(T) : byte_count); i < iter_times; ++i) {
            *(dst_ptr++) = *(src_ptr--);
        }
        return res;
    }
}  // namespace mtl

// bitset
namespace mtl {
    template <size_t N>
    class bitset {
        static_assert(std::endian::native == std::endian::little, "bitset only work in little endian");
        static constexpr auto BYTE_COUNT = _bitset_floor_byte_count(N);

      public:
        class reference;

        // 构造
      public:
        constexpr bitset() noexcept = default;

        constexpr bitset(unsigned long long val) noexcept {
            auto bit_count = std::min(sizeof(val) * BITS_PER_BYTE, N);
            // 假设 val 对应的二进制序列为 00000000 00000000 11110000 00001111，byte_count=2
            // 则 *val_ptr=11110000，*(val_ptr-1)=00001111
            uint8_t *val_ptr = reinterpret_cast<uint8_t *>(&val) + BYTE_COUNT - 1;
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                m_bytes[i] = *(val_ptr--);
            }
            // 对于上述假设的 val，经过 for 循环后，m_bytes[0]=11110000，m_bytes[1]=00001111
            // 若 N=9，则预期 m_bytes[0]=0，m_bytes[1]=00001111
            // 若 N=15，则预期 m_bytes[0]=01110000，m_bytes[1]=00001111
            // 即如果存在零散的比特，应对 m_bytes[0] 进行高位零覆盖处理
            auto mask = 0b11111111;
            for (auto i = _bitset_bits_of_byte0(bit_count); i < BITS_PER_BYTE; ++i) {
                mask >>= 1;
                m_bytes[0] &= mask;
            }
        }

        template <typename CharT, typename Traist, typename Allocator>
        explicit bitset(const std::basic_string<CharT, Traist, Allocator> &str,
                        size_t pos = 0, size_t n = -1,
                        CharT zero = '0', CharT one = '1') {
            auto valid_bit_count = std::min(N, std::min(str.size() - pos, n));
            auto valid_byte_count = _bitset_floor_byte_count(valid_bit_count);
            auto src_iter = str.begin() + valid_bit_count - 1;
            auto dst_byte = const_cast<uint8_t *>(m_bytes + _bitset_floor_byte_count(N) - 1);
            while (valid_bit_count > 0) {
                auto i = 0, mask = 1;
                while (++i <= 8) {
                    if (*(src_iter--) == one) {
                        *dst_byte |= mask;
                    }
                    mask <<= 1, valid_bit_count -= 1;
                    if (valid_bit_count == 0) {
                        break;
                    }
                }
                dst_byte -= 1;
            }
        }

        template <typename CharT>
        explicit bitset(const CharT *str, size_t n = -1, CharT zero = '0', CharT one = '1')
            : bitset(n == -1 ? std::basic_string<CharT>(str) : std::basic_string<CharT>(str, n),
                     0, n, zero, one) {}

        // binary operator
      public:
        auto operator&=(const bitset &other) noexcept -> bitset & {
            for (auto i = 0; i < _bitset_floor_byte_count(N); ++i) {
                m_bytes[i] &= other.m_bytes[i];
            }
            return *this;
        }

        auto operator|=(const bitset &other) noexcept -> bitset & {
            for (auto i = 0; i < _bitset_floor_byte_count(N); ++i) {
                m_bytes[i] |= other.m_bytes[i];
            }
            return *this;
        }

        auto operator^=(const bitset &other) noexcept -> bitset & {
            for (auto i = 0; i < _bitset_floor_byte_count(N); ++i) {
                m_bytes[i] ^= other.m_bytes[i];
            }
        }

        auto operator~() const noexcept -> bitset {
            auto res = bitset{};
            for (auto i = 0; i < _bitset_floor_byte_count(N); ++i) {
                res.m_bytes[i] = ~m_bytes[i];
            }
            // bytes[0] 的无效比特位需要置零
            for (auto i = 0, mask = 127; i < 8 - _bitset_bits_of_byte0(N); ++i, mask >>= 1) {
                res.m_bytes[0] &= mask;
            }
            return res;
        }

        // relational operator
      public:
        auto operator==(const bitset &o) -> bool {
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                if (m_bytes[i] != o.m_bytes[i]) {
                    return false;
                }
            }
            return true;
        }

        // bit shift operator
      public:
        auto operator<<=(size_t pos) -> bitset & {
            // 基本思路：合并两个 byte 为 word，对 word 进行移位，再返回给 byte
            if (pos > BITS_PER_BYTE) {
                (*this) <<= (pos - BITS_PER_BYTE);
                pos = BITS_PER_BYTE;
            }
            uint8_t bytes[BYTE_COUNT]{0};
            auto byte = bytes + BYTE_COUNT - 1;
            auto m_byte = m_bytes + BYTE_COUNT - 1;
            auto word = uint16_t{0};
            auto word_r = reinterpret_cast<uint8_t *>(&word), word_l = word_r + 1;  // 小端，因此 word=word_r-word_l
            for (auto i = 0; i < BYTE_COUNT - 1; ++i) {
                *word_r = *m_byte;
                *word_l = *(--m_byte);
                word <<= pos;
                *(byte) |= *word_r;
                *(--byte) |= *word_l;
            }
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                m_bytes[i] = bytes[i];
            }
            return *this;
        }

        auto operator<<(size_t pos) -> bitset {
            auto res = bitset{};
            for (auto i = 0; i < BYTE_COUNT; ++i) {
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
            uint8_t bytes[BYTE_COUNT]{0};
            auto byte = bytes;
            auto m_byte = m_bytes;
            auto word = uint16_t{0};
            auto word_r = reinterpret_cast<uint16_t *>(&word), word_l = word_r + 1;
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                *word_l = *m_byte;
                *word_r = *(++m_byte);
                word >>= pos;
                *(byte) |= *word_l;
                *(++byte) |= *word_r;
            }
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                m_bytes[i] = bytes[i];
            }
            return *this;
        }

        auto operator>>(size_t pos) -> bitset {
            auto res = bitset{};
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                res.m_bytes[i] = m_bytes[i];
            }
            res >>= pos;
            return res;
        }

        // to_xxx
      public:
        template <typename CharT = char, typename Traits = std::char_traits<CharT>,
                  typename Allocator = std::allocator<CharT>>
        auto to_string(CharT zero = '0', CharT one = '1') -> std::basic_string<CharT, Traits, Allocator> {
            auto res = std::basic_string<CharT, Traits, Allocator>(N, zero);
            auto pos = res.end() - 1;
            for (auto i = BYTE_COUNT; i > 0; --i) {
                auto mask = 1;
                for (auto j = 0; j < BITS_PER_BYTE; ++j, --pos, mask <<= 1) {
                    if ((m_bytes[i] & mask) != 0) {
                        *pos = one;
                    }
                }
            }
            // 特殊处理 bytes[0]
            for (auto i = 0, mask = 1; i < _bitset_bits_of_byte0(N); ++i, --pos, mask <<= 1) {
                if ((m_bytes[0] & mask) != 0) {
                    *pos = one;
                }
            }
            return res;
        }

        auto to_ulong() const -> unsigned long { return _bitset_to_unsigned<unsigned long>(m_bytes, N); }

        auto to_ullong() const -> unsigned long long { return _bitset_to_unsigned<unsigned long long>(m_bytes, N); }

        // modifier
      public:
        auto set() noexcept -> bitset & {
            for (auto i = 0; i < BYTE_COUNT; ++i) {
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
            for (auto i = 0; i < BYTE_COUNT; ++i) {
                m_bytes[i] = 0;
            }
            return *this;
        }

        auto reset(size_t pos) { set(pos, false); }

        auto flip() noexcept -> bitset & {
            for (auto i = 0; i < BYTE_COUNT; ++i) {
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
            return reference{const_cast<uint8_t *>(m_bytes + BYTE_COUNT - 1 - _bitset_ceil_byte_count(pos)),
                             static_cast<uint8_t>(pos % BITS_PER_BYTE)};
        }

        constexpr auto operator[](size_t pos) const -> reference {
            return reference{const_cast<uint8_t *>(m_bytes + BYTE_COUNT - 1 - _bitset_ceil_byte_count(pos)),
                             static_cast<uint8_t>(pos % BITS_PER_BYTE)};
        }

        auto count() const noexcept -> size_t {
            size_t res = 0;
            for (auto i = 0; i < BYTE_COUNT; ++i) {
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
        // m_bytes[0] 存放二进制序列的高位，且对无效的比特补零
        uint8_t m_bytes[BYTE_COUNT]{0};
    };
}  // namespace mtl

// bitset::reference
namespace mtl {
    template <size_t N>
    class bitset<N>::reference {
      public:
        reference(uint8_t *byte, uint8_t offset) : m_byte{byte}, m_offset{offset} {}

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
            m_offset = r.m_offset;
            return *this;
        }

        auto operator!() const noexcept -> bool {
            return !static_cast<bool>(*this);
        }

        operator bool() const noexcept {
            return *m_byte & (1 << m_offset);
        }

        auto flip() noexcept -> reference & {
            if (static_cast<bool>(*this)) {  // 置 0
                *m_byte &= 255 ^ (1 << m_offset);
            } else {  // 置 1
                *m_byte |= 1 << m_offset;
            }
            return *this;
        }

      private:
        uint8_t *m_byte;
        uint8_t m_offset;  // 从右往左，从0开始计算；即off=0时，m_byte[off]=m_byte[7]
    };
}  // namespace mtl