cmake_minimum_required(VERSION 3.20)

project(
  Minera
  VERSION 0.1.0
  DESCRIPTION "A fast C++ process mining core"
  LANGUAGES CXX
)

option(MINERA_BUILD_TESTS "Build Minera tests" ON)

add_library(minera_core
  src/alpha_miner.cpp
  src/conformance.cpp
  src/csv_event_log_importer.cpp
  src/directly_follows_graph.cpp
  src/inductive_miner.cpp
  src/json_export.cpp
  src/log_filter.cpp
  src/petri_net.cpp
)

add_library(minera::core ALIAS minera_core)

target_compile_features(minera_core PUBLIC cxx_std_20)
target_include_directories(minera_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(minera_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(minera_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(minera_export
  tools/minera_export.cpp
)
target_link_libraries(minera_export PRIVATE minera::core)
target_compile_features(minera_export PRIVATE cxx_std_20)

add_executable(minera_generate_sample
  tools/minera_generate_sample.cpp
)
target_compile_features(minera_generate_sample PRIVATE cxx_std_20)

find_package(Threads REQUIRED)

add_executable(minera_server
  tools/minera_server.cpp
)
target_link_libraries(minera_server PRIVATE minera::core Threads::Threads)
target_include_directories(minera_server PRIVATE third_party)
target_compile_features(minera_server PRIVATE cxx_std_20)

if(MINERA_BUILD_TESTS)
  enable_testing()

  add_executable(minera_core_tests
    tests/test_core.cpp
  )
  target_link_libraries(minera_core_tests PRIVATE minera::core)
  target_compile_features(minera_core_tests PRIVATE cxx_std_20)

  add_test(NAME minera_core_tests COMMAND minera_core_tests)
endif()
