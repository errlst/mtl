#pragma once
#include "utility/bitset.hpp"
#include "gtest/gtest.h"
#include <bitset>

using namespace mtl;

// bitset(ull)
TEST(bitset_test, case_1) {
    EXPECT_EQ(bitset<10>{12345}.to_ullong(), std::bitset<10>{12345}.to_ullong());
    EXPECT_EQ(bitset<10>{12345}.to_string(), std::bitset<10>{12345}.to_string());

    EXPECT_EQ(bitset<64>{123456}.to_ullong(), std::bitset<64>{123456}.to_ullong());
    EXPECT_EQ(bitset<64>{123456}.to_string(), std::bitset<64>{123456}.to_string());

    EXPECT_EQ(bitset<128>{12345678}.to_ullong(), bitset<128>{12345678}.to_ullong());
    EXPECT_EQ(bitset<128>{12345678}.to_string(), bitset<128>{12345678}.to_string());
}

// bitset(string)
TEST(bitset_test, case_2) {
    EXPECT_EQ(bitset<10>{"01011010"}.to_ullong(), std::bitset<10>{"01011010"}.to_ullong());
    EXPECT_EQ(bitset<10>{"01011010"}.to_string(), std::bitset<10>{"01011010"}.to_string());

    EXPECT_EQ(bitset<8>{"0101101011010"}.to_ullong(), std::bitset<8>{"0101101011010"}.to_ullong());
    EXPECT_EQ(bitset<8>{"0101101011010"}.to_string(), bitset<8>{"0101101011010"}.to_string());

    EXPECT_EQ(bitset<10>("ababbaba", -1, 'a', 'b').to_ullong(), std::bitset<10>("ababbaba", -1, 'a', 'b').to_ullong());
    EXPECT_EQ(bitset<10>("ababbaba", -1, 'a', 'b').to_string(), std::bitset<10>("ababbaba", -1, 'a', 'b').to_string());
}

// binary operator
TEST(bitset_test, case_3) {
    auto mask1 = bitset<10>{12345};
    auto mask2 = std::bitset<10>{12345};

    auto b1 = bitset<10>{"0101101001"};
    auto b2 = std::bitset<10>{"0101101001"};

    b1 &= mask1;
    b2 &= mask2;
    EXPECT_EQ(b1.to_ullong(), b2.to_ullong());

    b1 |= mask1;
    b2 |= mask2;
    EXPECT_EQ(b1.to_ullong(), b2.to_ullong());

    b1 ^= mask1;
    b2 ^= mask2;
    EXPECT_EQ(b1.to_ullong(), b2.to_ullong());

    EXPECT_EQ((~b1).to_ullong(), (~b2).to_ullong());
}

// shift operator
TEST(bitset_test, case_4) {
    EXPECT_EQ((bitset<8>{"0101101010"} << 4).to_ullong(), (std::bitset<8>{"0101101010"} << 4).to_ullong());
    EXPECT_EQ((bitset<32>{"010101010101010101010100101"} << 10).to_ullong(), (std::bitset<32>{"010101010101010101010100101"} << 10).to_ullong());
    EXPECT_EQ((bitset<32>{"010101010101010101010100101"} << 5).to_ullong(), (std::bitset<32>{"010101010101010101010100101"} << 5).to_ullong());

    EXPECT_EQ((bitset<8>{"0101101010"} >> 4).to_ullong(), (std::bitset<8>{"0101101010"} >> 4).to_ullong());
    EXPECT_EQ((bitset<32>{"010101010101010101010100101"} >> 10).to_ullong(), (std::bitset<32>{"010101010101010101010100101"} >> 10).to_ullong());
    EXPECT_EQ((bitset<32>{"010101010101010101010100101"} >> 5).to_ullong(), (std::bitset<32>{"010101010101010101010100101"} >> 5).to_ullong());
}

// modifier
TEST(bitset_test, case_5) {
    auto b1 = bitset<32>{12345678};
    auto b2 = std::bitset<32>{12345678};

    EXPECT_EQ(b1.set().to_ullong(), b2.set().to_ullong());

    EXPECT_EQ(b1.set(10, false).to_ullong(), b2.set(10, false).to_ullong());

    EXPECT_THROW(b1.set(100, true), std::out_of_range);

    EXPECT_EQ(b1.reset().to_ullong(), b2.reset().to_ullong());

    EXPECT_EQ(b1.set().reset(10).to_ullong(), b2.set().reset(10));

    EXPECT_EQ(b1.flip().to_ullong(), b2.flip().to_ullong());

    EXPECT_EQ(b1.flip(10).to_ullong(), b2.flip(10).to_ullong());

    EXPECT_THROW(b1.flip(100), std::out_of_range);
}