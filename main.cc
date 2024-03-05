#include <iostream>
#include "utility/variant.hpp"

auto main() -> int {
    std::cout << mtl::max_type_size<int,char>;
    
    return 0;
}