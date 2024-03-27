#include "utility/functional.hpp"
#include "utility/memory.hpp"     // IWYU pragma: keep
#include "utility/shared_ptr.hpp" // IWYU pragma: keep
#include "utility/utility.hpp"    // IWYU pragma: keep
#include <functional>
#include <iostream>

struct T {
  T() { std::cout << "con\n"; }
  T(const T &) { std::cout << "copy\n"; }
  T(T &&) { std::cout << "move\n"; }
};

auto main() -> int {

  mtl::bind_front([](int i) {std::cout << i;})(0);

  return 0;
}