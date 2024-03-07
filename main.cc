#include "utility/pair.hpp"
#include "utility/tuple.hpp"
#include "utility/optional.hpp"
#include "utility/variant.hpp"
#include <variant>

auto main() -> int {
    auto v1 = mtl::variant<int, double>{};

    v1.emplace<double>(20.1);

    return 0;
}