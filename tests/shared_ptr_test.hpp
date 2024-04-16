#include "utility/shared_ptr.hpp"
#include "gtest/gtest.h"

using namespace mtl;

// 测试 is_derived_from_esft
TEST(shared_ptr_test, case_1) {
    struct T1 {};
    EXPECT_FALSE(is_derived_from_esft_v<T1>);

    struct T2 : public enable_shared_from_this<T2> {};
    EXPECT_TRUE(is_derived_from_esft_v<T2>);

    struct T3 : public enable_shared_from_this<int> {};
    EXPECT_TRUE(is_derived_from_esft_v<T3>);
}

struct Case2T1 {
    ~Case2T1() { del_times += 1; }
    inline static int del_times = 0;
};
// 测试 shared_ptr 构造函数
TEST(shared_ptr_test, case_2) {
    // shared_ptr(p)
    auto p1 = shared_ptr<Case2T1>(new Case2T1());
    EXPECT_EQ(p1.use_count(), 1);
    auto p2 = p1;
    EXPECT_EQ(p2.use_count(), 2);
    p1.reset();
    EXPECT_EQ(p1.use_count(), 0);
    EXPECT_EQ(p2.use_count(), 1);
    p2.reset();
    EXPECT_EQ(p2.use_count(), 0);
    EXPECT_EQ(Case2T1::del_times, 1);
    auto p3 = shared_ptr<Case2T1[]>(new Case2T1[10]);
    p3.reset();
    EXPECT_EQ(Case2T1::del_times, 11);

    // shared_ptr(p,del)
    auto p4 = shared_ptr<Case2T1>(new Case2T1(), [](Case2T1 *p) {
        Case2T1::del_times = 0;
        delete p;
    });
    EXPECT_EQ(Case2T1::del_times, 11);
    p4.reset();
    EXPECT_EQ(Case2T1::del_times, 1);
}