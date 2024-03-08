#include "utility/pair.hpp"
#include "utility/tuple.hpp"
#include "utility/optional.hpp"
#include "utility/variant.hpp"
#include <variant>

auto main() -> int {
    auto v1 = mtl::variant<int, double>{10};
    auto v2 = mtl::variant<int, double>{10.1};

    mtl::visit([](int i, double d) {
        std::cout << i;
        std::cout << d;
    }, v1, v2);

    // auto v = mtl::_variant_data_value(v1.index(), v1.m_data);


    return 0;
}