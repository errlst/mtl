cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

project(mtl)

add_compile_options(--coverage)
add_link_options(--coverage)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/googletest)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)