cmake_minimum_required(VERSION 3.20)

project(
  Minera
  VERSION 0.1.0
  DESCRIPTION "A fast C++ process mining core"
  LANGUAGES CXX
)

option(MINERA_BUILD_TESTS "Build Minera tests" ON)

# A mining core lives or dies by codegen: an unoptimized build is 5-10x slower
# on real event logs. Default to Release when the user picks nothing (the
# usual `cmake -S . -B build` invocation), while leaving explicit choices and
# multi-config generators untouched.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
               Debug Release RelWithDebInfo MinSizeRel)
endif()

add_library(minera_core
  src/alpha_miner.cpp
  src/bpmn.cpp
  src/batches.cpp
  src/conformance.cpp
  src/correlation_miner.cpp
  src/csv_event_log_importer.cpp
  src/declare.cpp
  src/declare_playout.cpp
  src/declare_streaming.cpp
  src/decision_mining.cpp
  src/dfg_alignment.cpp
  src/dfg_variants.cpp
  src/directly_follows_graph.cpp
  src/edit_distance_alignment.cpp
  src/event_ordering.cpp
  src/eventually_follows.cpp
  src/footprints.cpp
  src/footprints_models.cpp
  src/heuristic_miner.cpp
  src/inductive_miner.cpp
  src/json_export.cpp
  src/log_filter.cpp
  src/log_skeleton.cpp
  src/model_decoration.cpp
  src/minimum_self_distance.cpp
  src/performance.cpp
  src/performance_spectrum.cpp
  src/petri_net.cpp
  src/playout.cpp
  src/powl.cpp
  src/powl_conversion.cpp
  src/powl_discovery.cpp
  src/pnml.cpp
  src/ptml.cpp
  src/request_params.cpp
  src/resource_analysis.cpp
  src/timestamp.cpp
  src/temporal_profile.cpp
  src/transition_system.cpp
  src/xes_event_log_importer.cpp
  src/xes_export.cpp
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

add_executable(minera_playout
  tools/minera_playout.cpp
)
target_link_libraries(minera_playout PRIVATE minera::core)
target_compile_features(minera_playout PRIVATE cxx_std_20)

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

  # The tests are assert()-based; Release's -DNDEBUG would compile every
  # check away and leave the suite vacuously green. Re-enable assert for the
  # test translation units only.
  set(MINERA_TEST_OPTIONS $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
                          $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

  add_executable(minera_core_tests
    tests/test_core.cpp
  )
  target_link_libraries(minera_core_tests PRIVATE minera::core)
  target_compile_features(minera_core_tests PRIVATE cxx_std_20)
  target_compile_options(minera_core_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_core_tests COMMAND minera_core_tests)

  add_executable(minera_correlation_miner_tests
    tests/test_correlation_miner.cpp
  )
  target_link_libraries(minera_correlation_miner_tests PRIVATE minera::core)
  target_compile_features(minera_correlation_miner_tests PRIVATE cxx_std_20)
  target_compile_options(minera_correlation_miner_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_correlation_miner_tests COMMAND minera_correlation_miner_tests)

  add_executable(minera_perspective_tests
    tests/test_perspectives.cpp
  )
  target_link_libraries(minera_perspective_tests PRIVATE minera::core)
  target_compile_features(minera_perspective_tests PRIVATE cxx_std_20)
  target_compile_options(minera_perspective_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_perspective_tests COMMAND minera_perspective_tests)

  add_executable(minera_request_params_tests
    tests/test_request_params.cpp
  )
  target_link_libraries(minera_request_params_tests PRIVATE minera::core)
  target_compile_features(minera_request_params_tests PRIVATE cxx_std_20)
  target_compile_options(minera_request_params_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_request_params_tests COMMAND minera_request_params_tests)

  add_executable(minera_powl_tests
    tests/test_powl.cpp
  )
  target_link_libraries(minera_powl_tests PRIVATE minera::core)
  target_compile_features(minera_powl_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_powl_tests COMMAND minera_powl_tests)

  add_executable(minera_powl_discovery_tests
    tests/test_powl_discovery.cpp
  )
  target_link_libraries(minera_powl_discovery_tests PRIVATE minera::core)
  target_compile_features(minera_powl_discovery_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_discovery_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_powl_discovery_tests COMMAND minera_powl_discovery_tests)

  add_executable(minera_powl_conversion_tests
    tests/test_powl_conversion.cpp
  )
  target_link_libraries(minera_powl_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_powl_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_powl_conversion_tests COMMAND minera_powl_conversion_tests)

  add_executable(minera_log_skeleton_tests
    tests/test_log_skeleton.cpp
  )
  target_link_libraries(minera_log_skeleton_tests PRIVATE minera::core)
  target_compile_features(minera_log_skeleton_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_skeleton_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_log_skeleton_tests COMMAND minera_log_skeleton_tests)

  add_executable(minera_footprints_tests
    tests/test_footprints.cpp
  )
  target_link_libraries(minera_footprints_tests PRIVATE minera::core)
  target_compile_features(minera_footprints_tests PRIVATE cxx_std_20)
  target_compile_options(minera_footprints_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_footprints_tests COMMAND minera_footprints_tests)

  add_executable(minera_footprints_models_tests
    tests/test_footprints_models.cpp
  )
  target_link_libraries(minera_footprints_models_tests PRIVATE minera::core)
  target_compile_features(minera_footprints_models_tests PRIVATE cxx_std_20)
  target_compile_options(minera_footprints_models_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_footprints_models_tests COMMAND minera_footprints_models_tests)

  add_executable(minera_declare_tests
    tests/test_declare.cpp
  )
  target_link_libraries(minera_declare_tests PRIVATE minera::core)
  target_compile_features(minera_declare_tests PRIVATE cxx_std_20)
  target_compile_options(minera_declare_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_declare_tests COMMAND minera_declare_tests)

  add_executable(minera_declare_variants_tests
    tests/test_declare_variants.cpp
  )
  target_link_libraries(minera_declare_variants_tests PRIVATE minera::core)
  target_compile_features(minera_declare_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_declare_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_declare_variants_tests COMMAND minera_declare_variants_tests)

  add_executable(minera_heuristic_variants_tests
    tests/test_heuristic_variants.cpp
  )
  target_link_libraries(minera_heuristic_variants_tests PRIVATE minera::core)
  target_compile_features(minera_heuristic_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_heuristic_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_heuristic_variants_tests COMMAND minera_heuristic_variants_tests)

  add_executable(minera_dfg_variants_tests
    tests/test_dfg_variants.cpp
  )
  target_link_libraries(minera_dfg_variants_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_dfg_variants_tests COMMAND minera_dfg_variants_tests)

  add_executable(minera_temporal_behavioral_tests
    tests/test_temporal_behavioral.cpp
  )
  target_link_libraries(minera_temporal_behavioral_tests PRIVATE minera::core)
  target_compile_features(minera_temporal_behavioral_tests PRIVATE cxx_std_20)
  target_compile_options(minera_temporal_behavioral_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_temporal_behavioral_tests COMMAND minera_temporal_behavioral_tests)

  add_executable(minera_alignment_variants_tests
    tests/test_alignment_variants.cpp
  )
  target_link_libraries(minera_alignment_variants_tests PRIVATE minera::core)
  target_compile_features(minera_alignment_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alignment_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alignment_variants_tests COMMAND minera_alignment_variants_tests)

  add_executable(minera_dfg_alignment_tests
    tests/test_dfg_alignment.cpp
  )
  target_link_libraries(minera_dfg_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_alignment_tests COMMAND minera_dfg_alignment_tests)

  add_executable(minera_edit_distance_alignment_tests
    tests/test_edit_distance_alignment.cpp
  )
  target_link_libraries(minera_edit_distance_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_edit_distance_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_edit_distance_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_edit_distance_alignment_tests COMMAND minera_edit_distance_alignment_tests)
endif()
