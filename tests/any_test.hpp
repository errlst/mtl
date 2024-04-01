#pragma once
#include "utility/any.hpp"
#include "gtest/gtest.h"

using namespace mtl;

// any() 测试
TEST(any_test, case_1) {
    auto a = any();
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.type(), typeid(void));
}

// any(const any&) 测试
TEST(any_test, case_2) {
    auto a1 = any(1);
    auto a2 = any(1);
    EXPECT_EQ(any_cast<int>(a2), 1);
}

// any(any &&a) 测试
TEST(any_test, case_3) {
    auto a1 = any(1);
    auto a2 = std::move(a1);
    EXPECT_THROW(any_cast<int>(a1), bad_any_cast);
    EXPECT_EQ(any_cast<int>(a2), 1);
}

// any(T&&) 测试
TEST(any_test, case_4) {
    auto a1 = any(1);
    auto a2 = any(std::string("hello"));
    EXPECT_EQ(any_cast<int>(a1), 1);
    EXPECT_THROW(any_cast<double>(a1), bad_any_cast);
    EXPECT_EQ(any_cast<std::string>(a2), "hello");
    EXPECT_THROW(any_cast<const char *>(a2), bad_any_cast);
}

// swap() 测试
TEST(any_test, case_5) {
    auto a1 = any(1);
    auto a2 = any(2);
    a1.swap(a2);
    EXPECT_EQ(any_cast<int>(a1), 2);
    EXPECT_EQ(any_cast<int>(a2), 1);
    a1 = std::string("hello");
    a2 = std::string("world");
    a1.swap(a2);
    EXPECT_EQ(any_cast<std::string>(a1), "world");
    EXPECT_EQ(any_cast<std::string>(a2), "hello");
}