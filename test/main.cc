#include "any_test.hpp"
#include "bitset_test.hpp"
#include "functional_test.hpp"

auto main(int argc, char *argv[]) -> int {
    testing::InitGoogleTest(&argc, argv);
    std::function<void()> a;

    return RUN_ALL_TESTS();
}