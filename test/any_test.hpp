#pragma once
#include "utility/any.hpp"
#include "gtest/gtest.h"

using namespace mtl;

// 默认构造函数测试
TEST(any_test, construct_1) {
    auto a = any();
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.type(), typeid(void));
}

// any(T&&) 测试
TEST(any_test, construct_2) {
    auto a1 = any(1);
    auto a2 = any(std::string("hello"));
    EXPECT_EQ(any_cast<int>(a1), 1);
    EXPECT_THROW(any_cast<double>(a1), bad_any_cast);
    EXPECT_EQ(any_cast<std::string>(a2), "hello");
    EXPECT_THROW(any_cast<const char *>(a2), bad_any_cast);
}

//
