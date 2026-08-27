cmake_minimum_required(VERSION 3.28)
project(economy CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(economy_core
    src/rng.cpp
    src/world.cpp
    src/network.cpp
)
target_include_directories(economy_core PUBLIC src)

enable_testing()
add_executable(economy_tests
    tests/test_main.cpp
    tests/test_rng.cpp
    tests/test_world.cpp
    tests/test_network.cpp
)
target_link_libraries(economy_tests PRIVATE economy_core)
target_include_directories(economy_tests PRIVATE third_party)
add_test(NAME economy_tests COMMAND economy_tests)
