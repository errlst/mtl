#pragma once
#include "utility/functional.hpp"
#include "gtest/gtest.h"

using namespace mtl;

//  invoke
TEST(functional_test, case_1) {
    struct T {
        auto call() -> void { throw std::exception(); }
        int i = 12345;
    } t;

    //  成员函数指针
    EXPECT_THROW(invoke(&T::call, t), std::exception);
    EXPECT_THROW(invoke(&T::call, std::ref(t)), std::exception);
    EXPECT_THROW(invoke(&T::call, &t), std::exception);

    //  数据成员指针
    EXPECT_EQ(invoke(&T::i, t), 12345);
    EXPECT_EQ(invoke(&T::i, std::ref(t)), 12345);
    EXPECT_EQ(invoke(&T::i, &t), 12345);

    //  其他
    EXPECT_EQ(invoke([] { return 10; }), 10);
    EXPECT_EQ(invoke([](int i) { return i; }, 20), 20);
}

// ref wrap
TEST(functional_test, case_2) {
    int i = 0;
    auto r = ref(i);

    static_cast<int &>(r) = 10;
    EXPECT_EQ(i, 10);

    r.get() = 20;
    EXPECT_EQ(i, 20);

    auto lam = [] { throw std::exception(); };
    EXPECT_THROW(ref(lam)(), std::exception);
}

//  not fn
TEST(function_test, case_3) {
    EXPECT_FALSE(not_fn([] { return true; })());
    EXPECT_TRUE(not_fn([] { return false; })());
}

//  bind front
TEST(function_test, case_4) {
    EXPECT_EQ(bind_front([] { return 1; })(), 1);
    EXPECT_EQ(bind_front([](int i) { return i; }, 1)(), 1);
    EXPECT_EQ(bind_front([](int i, int j) { return i + j; }, 1, 2)(), 3);
    EXPECT_EQ(bind_front([](int i, int j) { return i + j; }, 1)(2), 3);
}

//  function 构造
TEST(function_test, case_5) {
    //  lambda
    EXPECT_THROW(function([] { throw std::exception(); })(), std::exception);
    EXPECT_EQ(function([](int i) { return i; })(10), 10);

    struct T {
        auto call() -> int { return 123; }
        int i = 123;
    } t;

    //  成员函数、数据成员指针
    EXPECT_EQ(function<int(T &)>(&T::call)(t), 123);
    EXPECT_EQ(function<int(T &)>(&T::i)(t), 123);
}

//  function 拷贝、移动
TEST(function_test, case_6) {
    auto f1 = function([] { return 10; });
    auto f2 = f1;
    auto f3 = std::move(f1);
    EXPECT_EQ(f2(), 10);
    EXPECT_EQ(f3(), 10);
    EXPECT_THROW(f1(), bad_function_call);
}