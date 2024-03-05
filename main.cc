#include <iostream>
#include <variant>
#include "utility/pair.hpp"
#include "utility/tuple.hpp"
#include "utility/optional.hpp"
#include "utility/variant.hpp"

auto main() -> int {
    []<size_t... Idxs>(mtl::index_sequence<Idxs...>) {
        ((std::cout << Idxs), ...);
    }(mtl::make_index_sequence<10>());

    return 0;
}
