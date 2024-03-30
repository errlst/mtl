#pragma once
#include "utility/bitset.hpp"
#include "gtest/gtest.h"
#include <bitset>

using namespace mtl;

// bitset(ull)
TEST(bitset_test, case_1) {
    std::cout << mtl::bitset<12>{0b10100101'00001111}.to_ullong() << "\n";
}
