cmake_minimum_required(VERSION 3.10)

project(mtl_test)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} test_srcs)

add_executable(${PROJECT_NAME} ${test_srcs})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/3rd/googletest/googletest/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/3rd/googletest/googlemock/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE gtest gtest_main)
add_dependencies(${PROJECT_NAME} gtest gtest_main)