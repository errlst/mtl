#pragma once
#include "utility/pair.hpp"
#include "utility/tuple.hpp"
#include "gtest/gtest.h"

using namespace mtl;

//  构造函数
TEST(pair_test, case_1) {
    //
    auto p1 = pair(1, 2);
    EXPECT_EQ(p1.first, 1);
    EXPECT_EQ(p1.second, 2);

    auto p2 = pair("hello", "world");

    //  拷贝、移动构造
    auto p3 = pair(std::string("hello"), std::string("world"));
    auto p4 = p3;
    auto p5 = std::move(p3);
    EXPECT_EQ(p4.first, "hello");
    EXPECT_EQ(p5.first, "hello");
    EXPECT_EQ(p3.first, "");

    //  tuple 构造
    auto t1 = tuple(3, 'a');
    auto t2 = tuple(4, 'b');
    auto p6 = pair<std::string, std::string>(piecewise_construct, t1, t2);
    EXPECT_EQ(p6.first, "aaa");
    EXPECT_EQ(p6.second, "bbbb");
}