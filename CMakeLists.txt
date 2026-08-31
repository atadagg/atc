cmake_minimum_required(VERSION 3.28)
project(atc CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(atc_core
    src/rng.cpp
    src/world.cpp
    src/ledger.cpp
    src/intervention.cpp
    src/scenario.cpp
    src/network.cpp
    src/firm_decisions.cpp
    src/household_decisions.cpp
    src/daily.cpp
    src/month_close.cpp
    src/scheduler.cpp
    src/recorder.cpp
    src/identity.cpp
    src/identity_lexicon.cpp
    src/identity_glyphs.cpp
    src/identity_svg.cpp
)
target_include_directories(atc_core PUBLIC src)

add_library(viewer_core
    viewer/cell.cpp
    viewer/sim.cpp
    viewer/ring_recorder.cpp
    viewer/histogram.cpp
    viewer/model_agents.cpp
    viewer/model_dist.cpp
    viewer/model_time.cpp
    viewer/model_steering.cpp
    viewer/model_identity.cpp
    viewer/app.cpp
)
target_link_libraries(viewer_core PUBLIC atc_core)
target_include_directories(viewer_core PUBLIC viewer)

enable_testing()
add_executable(atc_tests
    tests/test_main.cpp
    tests/test_rng.cpp
    tests/test_world.cpp
    tests/test_ledger.cpp
    tests/test_summary_journal.cpp
    tests/test_params.cpp
    tests/test_intervention.cpp
    tests/test_scenario.cpp
    tests/test_network.cpp
    tests/test_firm_decisions.cpp
    tests/test_household_decisions.cpp
    tests/test_daily.cpp
    tests/test_month_close.cpp
    tests/test_scheduler.cpp
    tests/test_recorder.cpp
    tests/test_identity.cpp
    tests/test_identity_names.cpp
    tests/test_identity_svg.cpp
    tests/test_viewer_identity.cpp
    tests/test_viewer_sim.cpp
    tests/test_viewer_ring.cpp
    tests/test_viewer_histogram.cpp
    tests/test_viewer_cell.cpp
    tests/test_viewer_model_agents.cpp
    tests/test_viewer_model_views.cpp
    tests/test_viewer_model_steering.cpp
    tests/test_viewer_app.cpp
)
target_link_libraries(atc_tests PRIVATE atc_core viewer_core)
target_include_directories(atc_tests PRIVATE third_party)
add_test(NAME atc_tests COMMAND atc_tests)

add_executable(atc src/main.cpp)
target_link_libraries(atc PRIVATE atc_core)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
endif()

if(NOT DEFINED SDL3_FOUND)
  set(SDL3_FOUND OFF)
endif()

option(ATC_BUILD_VIEWER "Build the interactive viewer" ${SDL3_FOUND})

if(ATC_BUILD_VIEWER)
  if(NOT SDL3_FOUND)
    message(FATAL_ERROR "ATC_BUILD_VIEWER is ON but SDL3 was not found")
  endif()

  add_library(imgui STATIC
      third_party/imgui/imgui.cpp
      third_party/imgui/imgui_draw.cpp
      third_party/imgui/imgui_tables.cpp
      third_party/imgui/imgui_widgets.cpp
      third_party/imgui/backends/imgui_impl_sdl3.cpp
      third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(imgui SYSTEM PUBLIC third_party/imgui third_party/imgui/backends)
  target_link_libraries(imgui PUBLIC PkgConfig::SDL3)
  # Vendored third-party code does not have to satisfy our warning settings.
  target_compile_options(imgui PRIVATE -w)

  add_executable(atc_viewer
      viewer/main.cpp
      viewer/panel_agents.cpp
      viewer/panel_dist.cpp
      viewer/panel_time.cpp
      viewer/panel_steering.cpp
      viewer/identity_textures.cpp
  )
  target_link_libraries(atc_viewer PRIVATE viewer_core imgui)
endif()
