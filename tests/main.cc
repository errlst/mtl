#include "any_test.hpp"
#include "bitset_test.hpp"
#include "functional_test.hpp"
#include "optional_test.hpp"

auto main(int argc, char *argv[]) -> int {
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}