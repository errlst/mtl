#include "utility/memory.hpp"     // IWYU pragma: keep
#include "utility/shared_ptr.hpp" // IWYU pragma: keep
#include "utility/utility.hpp"    // IWYU pragma: keep
#include <iostream>               // IWYU pragma: keep
#include <memory>                 // IWYU pragma: keep

struct T {
  T() { std::cout << "init\n"; }
  ~T() { std::cout << "dest\n"; }
};

auto main() -> int {
    
    
     return 0; }