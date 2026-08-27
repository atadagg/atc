cmake_minimum_required(VERSION 3.28)
project(economy CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(economy_core
    src/rng.cpp
    src/world.cpp
    src/network.cpp
    src/firm_decisions.cpp
    src/household_decisions.cpp
    src/daily.cpp
    src/month_close.cpp
    src/scheduler.cpp
    src/recorder.cpp
)
target_include_directories(economy_core PUBLIC src)

add_library(viewer_core
    viewer/sim.cpp
    viewer/ring_recorder.cpp
    viewer/histogram.cpp
)
target_link_libraries(viewer_core PUBLIC economy_core)
target_include_directories(viewer_core PUBLIC viewer)

enable_testing()
add_executable(economy_tests
    tests/test_main.cpp
    tests/test_rng.cpp
    tests/test_world.cpp
    tests/test_network.cpp
    tests/test_firm_decisions.cpp
    tests/test_household_decisions.cpp
    tests/test_daily.cpp
    tests/test_month_close.cpp
    tests/test_scheduler.cpp
    tests/test_recorder.cpp
    tests/test_viewer_sim.cpp
    tests/test_viewer_ring.cpp
    tests/test_viewer_histogram.cpp
)
target_link_libraries(economy_tests PRIVATE economy_core viewer_core)
target_include_directories(economy_tests PRIVATE third_party)
add_test(NAME economy_tests COMMAND economy_tests)

add_executable(economy src/main.cpp)
target_link_libraries(economy PRIVATE economy_core)
