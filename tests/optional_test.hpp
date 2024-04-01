#pragma once
#include "utility/optional.hpp"
#include "gtest/gtest.h"

using namespace mtl;

//  构造函数测试
TEST(optional_test, case_1) {
    //  默认构造
    auto o1 = optional<int>();
    EXPECT_FALSE(o1.has_value());
    EXPECT_THROW(o1.value(), bad_optional_access);

    //  nullopt
    auto o2 = optional<int>(nullopt);
    EXPECT_FALSE(o2.has_value());
    EXPECT_THROW(o2.value(), bad_optional_access);

    //  拷贝
    auto o3 = optional<int>(10);
    auto o4 = o3;
    EXPECT_EQ(o3.value(), 10);
    EXPECT_EQ(o4.value(), 10);

    //  移动
    auto o5 = optional<std::string>("hello");
    auto o6 = std::move(o5);
    EXPECT_EQ(o5.value(), "");
    EXPECT_EQ(o6.value(), "hello");

    //  
    auto o7 = optional<int>(10);
    auto o8 = optional<double>(o7);
    EXPECT_EQ(o8.value(), 10);
}
