#pragma once
#include "utility/any.hpp"
#include "gtest/gtest.h"

using namespace mtl;

// 构造函数
TEST(any_test, case_1) {
    // any()
    auto a1 = any();
    EXPECT_FALSE(a1.has_value());
    EXPECT_EQ(a1.type(), typeid(void));

    // any(const any&)
    a1 = any(1);
    auto a2 = any(a1);
    EXPECT_EQ(any_cast<int>(a2), 1);

    // any(any&&)
    a1 = any(1);
    a2 = std::move(a1);
    EXPECT_THROW(any_cast<int>(a1), bad_any_cast);
    EXPECT_EQ(any_cast<int>(a2), 1);

    // any(T&&)
    a1 = any(1);
    auto a3 = any(std::string("hello"));
    EXPECT_EQ(any_cast<int>(a1), 1);
    EXPECT_THROW(any_cast<double>(a1), bad_any_cast);
    EXPECT_EQ(any_cast<std::string>(a3), "hello");
    EXPECT_THROW(any_cast<const char *>(a3), bad_any_cast);

    // any(inplace, initializerlist)
    auto a4 = mtl::any(in_place_type<std::vector<int>>, {1, 2});
    auto v = mtl::any_cast<std::vector<int>>(a4);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

// operator= 测试
TEST(any_test, case_2) {
    auto const a1 = any(1);
    auto a2 = any();
    a2 = a1;
    EXPECT_EQ(any_cast<int>(a1), any_cast<int>(a2));

    auto a3 = any(std::string("hello"));
    auto a4 = std::move(a3);
    EXPECT_THROW(any_cast<std::string>(a3), bad_any_cast);
    EXPECT_EQ(any_cast<std::string>(a4), "hello");
}

// emplace 测试
TEST(any_test, case_3) {
    auto a1 = any();
    EXPECT_EQ(a1.emplace<std::string>("hello"), "hello");
    EXPECT_EQ(any_cast<std::string>(a1), "hello");

    a1.emplace<std::vector<int>>({1, 2});
    auto v = any_cast<std::vector<int>>(a1);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

// anycast 测试
TEST(any_test, case_4) {
    auto a1 = any(1);
    EXPECT_EQ(*any_cast<int>(&a1), 1);
    *any_cast<int>(&a1) = 10;
    EXPECT_EQ(any_cast<int>(a1), 10);
    EXPECT_EQ(any_cast<int>(std::move(a1)), 10);
}

// makeany 测试
TEST(any_test, case_5) {
    auto a1 = make_any<std::vector<int>>({1, 2});
    auto v = any_cast<std::vector<int>>(a1);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

// swap() 测试
TEST(any_test, case_6) {
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

// 其它
TEST(any_test, case_7) {
    // 堆内存拷贝
    auto a1 = any(std::string("hello"));
    auto a2 = a1;
    EXPECT_EQ(any_cast<std::string>(a1), "hello");
    EXPECT_EQ(any_cast<std::string>(a2), "hello");
}