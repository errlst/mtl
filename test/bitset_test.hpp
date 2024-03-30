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
