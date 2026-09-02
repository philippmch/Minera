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
  src/backend/alpha_miner.cpp
  src/backend/activity_split.cpp
  src/backend/attribute_statistics.cpp
  src/backend/attribute_distribution.cpp
  src/backend/attribute_kde.cpp
  src/backend/boundary_activities.cpp
  src/backend/attribute_value_repetition.cpp
  src/backend/case_grouping.cpp
  src/backend/chaotic_activities.cpp
  src/backend/bpmn.cpp
  src/backend/browser_history.cpp
  src/backend/batches.cpp
  src/backend/batches/discovery_registry.cpp
  src/backend/case_relations.cpp
  src/backend/case_arrival.cpp
  src/backend/case_statistics.cpp
  src/backend/oc_causal_net.cpp
  src/backend/conformance.cpp
  src/backend/communication_history.cpp
  src/backend/concept_drift.cpp
  src/backend/cycle_time.cpp
  src/backend/correlation_miner.cpp
  src/backend/clustering.cpp
  src/backend/csv_event_log_importer.cpp
  src/backend/data_petri_net.cpp
  src/backend/declare.cpp
  src/backend/declare_playout.cpp
  src/backend/declare_streaming.cpp
  src/backend/decision_mining.cpp
  src/backend/decision_replay.cpp
  src/backend/decision_table.cpp
  src/backend/dfg_playout.cpp
  src/backend/dfg_alignment.cpp
  src/backend/dfg_filter.cpp
  src/backend/dfg_conversion.cpp
  src/backend/dfg_io.cpp
  src/backend/dfg_precision.cpp
  src/backend/decomposed_alignment.cpp
  src/backend/dfg_utils.cpp
  src/backend/dfg_variants.cpp
  src/backend/dfg_dataframe.cpp
  src/backend/dfg_dataframe_json.cpp
  src/backend/directly_follows_graph.cpp
  src/backend/embeddings.cpp
  src/backend/event_data.cpp
  src/backend/edit_distance_alignment.cpp
  src/backend/evaluation.cpp
  src/backend/event_ordering.cpp
  src/backend/eventually_follows.cpp
  src/backend/enterprise_history.cpp
  src/backend/footprints.cpp
  src/backend/footprints/conformance_evaluation.cpp
  src/backend/footprints/conformance_log_extensive.cpp
  src/backend/footprints/conformance_log_model.cpp
  src/backend/footprints/conformance_trace_extensive.cpp
  src/backend/footprints/conformance_tree_visualization.cpp
  src/backend/footprints/discovery_log.cpp
  src/backend/footprints/discovery_models.cpp
  src/backend/footprints/discovery_registry.cpp
  src/backend/genetic_matrix_conversion.cpp
  src/backend/genetic_miner.cpp
  src/backend/gzip_codec.cpp
  src/backend/footprints_models.cpp
  src/backend/heuristic_miner.cpp
  src/backend/organizational_mining.cpp
  src/backend/network_analysis.cpp
  src/backend/inductive_miner.cpp
  src/backend/ilp_miner.cpp
  src/backend/interval_tree.cpp
  src/backend/overlap.cpp
  src/backend/label_splitting.cpp
  src/backend/live_stream.cpp
  src/backend/link_analysis.cpp
  src/backend/json_export.cpp
  src/backend/knowledge_injection.cpp
  src/backend/log_filter.cpp
  src/backend/log_conversion.cpp
  src/backend/path_filter.cpp
  src/backend/passed_time.cpp
  src/backend/segment_filter.cpp
  src/backend/service_time.cpp
  src/backend/streaming_dfg.cpp
  src/backend/streaming_store.cpp
  src/backend/streaming_footprints.cpp
  src/backend/streaming_import.cpp
  src/backend/streaming_temporal.cpp
  src/backend/streaming_tbr.cpp
  src/backend/ltl_filter.cpp
  src/backend/log_to_features.cpp
  src/backend/log_util.cpp
  src/backend/log_util_json.cpp
  src/backend/log_to_target.cpp
  src/backend/local_diagnostics.cpp
  src/backend/log_skeleton.cpp
  src/backend/model_decoration.cpp
  src/backend/monte_carlo.cpp
  src/backend/monte_carlo_json.cpp
  src/backend/minimum_self_distance.cpp
  src/backend/ocel.cpp
  src/backend/ocel_csv.cpp
  src/backend/ocel_sqlite.cpp
  src/backend/ocel_filter.cpp
  src/backend/ocel_query.cpp
  src/backend/ocel_xml.cpp
  src/backend/ocel_features.cpp
  src/backend/ocel_statistics.cpp
  src/backend/ocel_playout.cpp
  src/backend/ocel_transform.cpp
  src/backend/performance.cpp
  src/backend/performance_spectrum.cpp
  src/backend/performance_spectrum/components.cpp
  src/backend/petri_anti_alignment.cpp
  src/backend/petri_analysis_json.cpp
  src/backend/petri_multi_alignment.cpp
  src/backend/petri_analysis.cpp
  src/backend/petri_net.cpp
  src/backend/petri_usage_comparison.cpp
  src/backend/petri_net_reduction.cpp
  src/backend/playout.cpp
  src/backend/stochastic_petri.cpp
  src/backend/stochastic_map.cpp
  src/backend/pripel.cpp
  src/backend/process_tree_alignment.cpp
  src/backend/random_tree.cpp
  src/backend/saw_nets.cpp
  src/backend/process_tree_playout.cpp
  src/backend/process_tree_reduction.cpp
  src/backend/powl.cpp
  src/backend/powl_conversion.cpp
  src/backend/powl_discovery.cpp
  src/backend/pnml.cpp
  src/backend/ptml.cpp
  src/backend/request_params.cpp
  src/backend/rework.cpp
  src/backend/repository_history.cpp
  src/backend/resource_analysis.cpp
  src/backend/resource_profiles.cpp
  src/backend/role_discovery.cpp
  src/backend/secure_random.cpp
  src/backend/timestamp.cpp
  src/backend/temporal_profile.cpp
  src/backend/timestamp_interleavings.cpp
  src/backend/transition_system.cpp
  src/backend/variant_statistics.cpp
  src/backend/trace_variant_query.cpp
  src/backend/trie.cpp
  src/backend/trie_conversion.cpp
  src/backend/process_cube.cpp
  src/backend/xes_event_log_importer.cpp
  src/backend/xes_export.cpp
  src/backend/windows_event_log.cpp
  src/backend/workflow_history.cpp
)

add_library(minera::core ALIAS minera_core)

target_compile_features(minera_core PUBLIC cxx_std_20)
target_include_directories(minera_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(WIN32)
  target_link_libraries(minera_core PRIVATE bcrypt winsqlite3 PUBLIC ws2_32)
  target_compile_definitions(minera_core PUBLIC MINERA_HAS_SQLITE=1)
else()
  find_package(SQLite3 QUIET)
  if(SQLite3_FOUND)
    target_link_libraries(minera_core PRIVATE SQLite::SQLite3)
    target_compile_definitions(minera_core PUBLIC MINERA_HAS_SQLITE=1)
  endif()
endif()

if(MSVC)
  target_compile_options(minera_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(minera_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(minera_export
  src/backend/tools/minera_export.cpp
)
target_link_libraries(minera_export PRIVATE minera::core)
target_include_directories(minera_export PRIVATE third_party)
target_compile_features(minera_export PRIVATE cxx_std_20)

add_executable(minera_ocel
  src/backend/tools/minera_ocel.cpp
)
target_link_libraries(minera_ocel PRIVATE minera::core)
target_compile_features(minera_ocel PRIVATE cxx_std_20)

add_executable(minera_process_cube
  src/backend/tools/minera_process_cube.cpp
)
target_link_libraries(minera_process_cube PRIVATE minera::core)
target_compile_features(minera_process_cube PRIVATE cxx_std_20)

add_executable(minera_log_util src/backend/tools/minera_log_util.cpp)
target_link_libraries(minera_log_util PRIVATE minera::core)
target_compile_features(minera_log_util PRIVATE cxx_std_20)

add_executable(minera_knowledge src/backend/tools/minera_knowledge.cpp)
target_link_libraries(minera_knowledge PRIVATE minera::core)
target_compile_features(minera_knowledge PRIVATE cxx_std_20)

add_executable(minera_dfg
  src/backend/tools/minera_dfg.cpp
)
target_link_libraries(minera_dfg PRIVATE minera::core)
target_compile_features(minera_dfg PRIVATE cxx_std_20)

add_executable(minera_dfg_precision
  src/backend/tools/minera_dfg_precision.cpp
)
target_link_libraries(minera_dfg_precision PRIVATE minera::core)
target_compile_features(minera_dfg_precision PRIVATE cxx_std_20)

add_executable(minera_streaming_dfg
  src/backend/tools/minera_streaming_dfg.cpp
)
target_link_libraries(minera_streaming_dfg PRIVATE minera::core)
target_compile_features(minera_streaming_dfg PRIVATE cxx_std_20)

add_executable(minera_streaming_store
  src/backend/tools/minera_streaming_store.cpp
)
target_link_libraries(minera_streaming_store PRIVATE minera::core)
target_compile_features(minera_streaming_store PRIVATE cxx_std_20)

add_executable(minera_streaming_footprints
  src/backend/tools/minera_streaming_footprints.cpp
)
target_link_libraries(minera_streaming_footprints PRIVATE minera::core)
target_compile_features(minera_streaming_footprints PRIVATE cxx_std_20)

add_executable(minera_streaming_temporal
  src/backend/tools/minera_streaming_temporal.cpp
)
target_link_libraries(minera_streaming_temporal PRIVATE minera::core)
target_compile_features(minera_streaming_temporal PRIVATE cxx_std_20)

add_executable(minera_streaming_tbr
  src/backend/tools/minera_streaming_tbr.cpp
)
target_link_libraries(minera_streaming_tbr PRIVATE minera::core)
target_compile_features(minera_streaming_tbr PRIVATE cxx_std_20)

add_executable(minera_live_stream
  src/backend/tools/minera_live_stream.cpp
)
target_link_libraries(minera_live_stream PRIVATE minera::core)
target_compile_features(minera_live_stream PRIVATE cxx_std_20)

add_executable(minera_data_petri
  src/backend/tools/minera_data_petri.cpp
)
target_link_libraries(minera_data_petri PRIVATE minera::core)
target_compile_features(minera_data_petri PRIVATE cxx_std_20)

add_executable(minera_decision_table
  src/backend/tools/minera_decision_table.cpp
)
target_link_libraries(minera_decision_table PRIVATE minera::core)
target_compile_features(minera_decision_table PRIVATE cxx_std_20)

add_executable(minera_stream_import
  src/backend/tools/minera_stream_import.cpp
)
target_link_libraries(minera_stream_import PRIVATE minera::core)
target_compile_features(minera_stream_import PRIVATE cxx_std_20)

add_executable(minera_playout
  src/backend/tools/minera_playout.cpp
)
target_link_libraries(minera_playout PRIVATE minera::core)
target_compile_features(minera_playout PRIVATE cxx_std_20)

add_executable(minera_generate_sample
  src/backend/tools/minera_generate_sample.cpp
)
target_compile_features(minera_generate_sample PRIVATE cxx_std_20)

find_package(Threads REQUIRED)

add_executable(minera_server
  src/backend/tools/minera_server.cpp
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
    src/backend/tests/test_core.cpp
  )
  target_link_libraries(minera_core_tests PRIVATE minera::core)
  target_compile_features(minera_core_tests PRIVATE cxx_std_20)
  target_compile_options(minera_core_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_core_tests COMMAND minera_core_tests)

  add_executable(minera_inductive_miner_tests
    src/backend/tests/test_inductive_miner.cpp
  )
  target_link_libraries(minera_inductive_miner_tests PRIVATE minera::core)
  target_compile_features(minera_inductive_miner_tests PRIVATE cxx_std_20)
  target_compile_options(minera_inductive_miner_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_inductive_miner_tests COMMAND minera_inductive_miner_tests)

  add_executable(minera_inductive_parity_contracts_tests
    src/backend/tests/test_inductive_parity_contracts.cpp
  )
  target_link_libraries(minera_inductive_parity_contracts_tests PRIVATE minera::core)
  target_include_directories(minera_inductive_parity_contracts_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
  )
  target_compile_features(minera_inductive_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_inductive_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_inductive_parity_contracts_tests
           COMMAND minera_inductive_parity_contracts_tests)

  add_executable(minera_xes_io_tests
    src/backend/tests/test_xes_io.cpp
  )
  target_link_libraries(minera_xes_io_tests PRIVATE minera::core)
  target_compile_features(minera_xes_io_tests PRIVATE cxx_std_20)
  target_compile_options(minera_xes_io_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_xes_io_tests COMMAND minera_xes_io_tests)

  add_executable(minera_event_data_tests
    src/backend/tests/test_event_data.cpp
  )
  target_link_libraries(minera_event_data_tests PRIVATE minera::core)
  target_compile_features(minera_event_data_tests PRIVATE cxx_std_20)
  target_compile_options(minera_event_data_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_event_data_tests COMMAND minera_event_data_tests)

  add_executable(minera_ocel_tests
    src/backend/tests/test_ocel.cpp
  )
  target_link_libraries(minera_ocel_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_tests COMMAND minera_ocel_tests)

  add_executable(minera_ocel_csv_tests
    src/backend/tests/test_ocel_csv.cpp
  )
  target_link_libraries(minera_ocel_csv_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_csv_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_csv_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_csv_tests COMMAND minera_ocel_csv_tests)

  add_executable(minera_ocel_sqlite_tests
    src/backend/tests/test_ocel_sqlite.cpp
  )
  target_link_libraries(minera_ocel_sqlite_tests PRIVATE minera::core)
  if(WIN32)
    target_link_libraries(minera_ocel_sqlite_tests PRIVATE winsqlite3)
  elseif(SQLite3_FOUND)
    target_link_libraries(minera_ocel_sqlite_tests PRIVATE SQLite::SQLite3)
  endif()
  target_compile_features(minera_ocel_sqlite_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_sqlite_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_sqlite_tests COMMAND minera_ocel_sqlite_tests)

  add_executable(minera_ocel_xml_tests
    src/backend/tests/test_ocel_xml.cpp
  )
  target_link_libraries(minera_ocel_xml_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_xml_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_xml_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_xml_tests COMMAND minera_ocel_xml_tests)

  add_executable(minera_ocel_filter_tests
    src/backend/tests/test_ocel_filter.cpp
  )
  target_link_libraries(minera_ocel_filter_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_filter_tests COMMAND minera_ocel_filter_tests)

  add_executable(minera_ocel_query_tests
    src/backend/tests/test_ocel_query.cpp
  )
  target_link_libraries(minera_ocel_query_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_query_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_query_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_query_tests COMMAND minera_ocel_query_tests)

  add_executable(minera_ocel_statistics_tests
    src/backend/tests/test_ocel_statistics.cpp
  )
  target_link_libraries(minera_ocel_statistics_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_statistics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_statistics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_statistics_tests COMMAND minera_ocel_statistics_tests)

  add_executable(minera_dfg_filter_tests
    src/backend/tests/test_dfg_filter.cpp
  )
  target_link_libraries(minera_dfg_filter_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_filter_tests COMMAND minera_dfg_filter_tests)

  add_executable(minera_dfg_conversion_tests
    src/backend/tests/test_dfg_conversion.cpp
  )
  target_link_libraries(minera_dfg_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_conversion_tests COMMAND minera_dfg_conversion_tests)

  add_executable(minera_activity_split_tests
    src/backend/tests/test_activity_split.cpp
  )
  target_link_libraries(minera_activity_split_tests PRIVATE minera::core)
  target_compile_features(minera_activity_split_tests PRIVATE cxx_std_20)
  target_compile_options(minera_activity_split_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_activity_split_tests COMMAND minera_activity_split_tests)

  add_executable(minera_attribute_value_repetition_tests
    src/backend/tests/test_attribute_value_repetition.cpp
  )
  target_link_libraries(minera_attribute_value_repetition_tests PRIVATE minera::core)
  target_compile_features(minera_attribute_value_repetition_tests PRIVATE cxx_std_20)
  target_compile_options(minera_attribute_value_repetition_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_attribute_value_repetition_tests COMMAND minera_attribute_value_repetition_tests)

  add_executable(minera_case_grouping_tests
    src/backend/tests/test_case_grouping.cpp
  )
  target_link_libraries(minera_case_grouping_tests PRIVATE minera::core)
  target_compile_features(minera_case_grouping_tests PRIVATE cxx_std_20)
  target_compile_options(minera_case_grouping_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_case_grouping_tests COMMAND minera_case_grouping_tests)

  add_executable(minera_ltl_filter_tests
    src/backend/tests/test_ltl_filter.cpp
  )
  target_link_libraries(minera_ltl_filter_tests PRIVATE minera::core)
  target_compile_features(minera_ltl_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ltl_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ltl_filter_tests COMMAND minera_ltl_filter_tests)

  add_executable(minera_path_filter_tests
    src/backend/tests/test_path_filter.cpp
  )
  target_link_libraries(minera_path_filter_tests PRIVATE minera::core)
  target_compile_features(minera_path_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_path_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_path_filter_tests COMMAND minera_path_filter_tests)

  add_executable(minera_segment_filter_tests
    src/backend/tests/test_segment_filter.cpp
  )
  target_link_libraries(minera_segment_filter_tests PRIVATE minera::core)
  target_compile_features(minera_segment_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_segment_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_segment_filter_tests COMMAND minera_segment_filter_tests)

  add_executable(minera_embeddings_tests
    src/backend/tests/test_embeddings.cpp
  )
  target_link_libraries(minera_embeddings_tests PRIVATE minera::core)
  target_compile_features(minera_embeddings_tests PRIVATE cxx_std_20)
  target_compile_options(minera_embeddings_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_embeddings_tests COMMAND minera_embeddings_tests)

  add_executable(minera_embedding_runtime_tests
    src/backend/tests/test_embedding_runtime.cpp
  )
  target_link_libraries(minera_embedding_runtime_tests PRIVATE minera::core)
  target_include_directories(minera_embedding_runtime_tests PRIVATE third_party src/backend/tools)
  target_compile_features(minera_embedding_runtime_tests PRIVATE cxx_std_20)
  target_compile_options(minera_embedding_runtime_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_embedding_runtime_tests COMMAND minera_embedding_runtime_tests)

  add_executable(minera_chaotic_activities_tests
    src/backend/tests/test_chaotic_activities.cpp
  )
  target_link_libraries(minera_chaotic_activities_tests PRIVATE minera::core)
  target_compile_features(minera_chaotic_activities_tests PRIVATE cxx_std_20)
  target_compile_options(minera_chaotic_activities_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_chaotic_activities_tests COMMAND minera_chaotic_activities_tests)

  add_executable(minera_passed_time_tests
    src/backend/tests/test_passed_time.cpp
  )
  target_link_libraries(minera_passed_time_tests PRIVATE minera::core)
  target_compile_features(minera_passed_time_tests PRIVATE cxx_std_20)
  target_compile_options(minera_passed_time_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_passed_time_tests COMMAND minera_passed_time_tests)

  add_executable(minera_service_time_tests
    src/backend/tests/test_service_time.cpp
  )
  target_link_libraries(minera_service_time_tests PRIVATE minera::core)
  target_compile_features(minera_service_time_tests PRIVATE cxx_std_20)
  target_compile_options(minera_service_time_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_service_time_tests COMMAND minera_service_time_tests)

  add_executable(minera_rework_tests
    src/backend/tests/test_rework.cpp
  )
  target_link_libraries(minera_rework_tests PRIVATE minera::core)
  target_compile_features(minera_rework_tests PRIVATE cxx_std_20)
  target_compile_options(minera_rework_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_rework_tests COMMAND minera_rework_tests)

  add_executable(minera_ilp_miner_tests
    src/backend/tests/test_ilp_miner.cpp
  )
  target_link_libraries(minera_ilp_miner_tests PRIVATE minera::core)
  target_compile_features(minera_ilp_miner_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ilp_miner_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ilp_miner_tests COMMAND minera_ilp_miner_tests)

  add_executable(minera_trace_variant_query_tests
    src/backend/tests/test_trace_variant_query.cpp
  )
  target_link_libraries(minera_trace_variant_query_tests PRIVATE minera::core)
  target_compile_features(minera_trace_variant_query_tests PRIVATE cxx_std_20)
  target_compile_options(minera_trace_variant_query_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_trace_variant_query_tests COMMAND minera_trace_variant_query_tests)

  add_executable(minera_pripel_tests
    src/backend/tests/test_pripel.cpp
  )
  target_link_libraries(minera_pripel_tests PRIVATE minera::core)
  target_compile_features(minera_pripel_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pripel_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pripel_tests COMMAND minera_pripel_tests)

  add_executable(minera_genetic_miner_tests
    src/backend/tests/test_genetic_miner.cpp
  )
  target_link_libraries(minera_genetic_miner_tests PRIVATE minera::core)
  target_compile_features(minera_genetic_miner_tests PRIVATE cxx_std_20)
  target_compile_options(minera_genetic_miner_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_genetic_miner_tests COMMAND minera_genetic_miner_tests)

  add_executable(minera_genetic_matrix_conversion_tests
    src/backend/tests/test_genetic_matrix_conversion.cpp
  )
  target_link_libraries(minera_genetic_matrix_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_genetic_matrix_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_genetic_matrix_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_genetic_matrix_conversion_tests COMMAND minera_genetic_matrix_conversion_tests)

  add_executable(minera_role_discovery_tests
    src/backend/tests/test_role_discovery.cpp
  )
  target_link_libraries(minera_role_discovery_tests PRIVATE minera::core)
  target_compile_features(minera_role_discovery_tests PRIVATE cxx_std_20)
  target_compile_options(minera_role_discovery_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_role_discovery_tests COMMAND minera_role_discovery_tests)

  add_executable(minera_organizational_mining_tests
    src/backend/tests/test_organizational_mining.cpp
  )
  target_link_libraries(minera_organizational_mining_tests PRIVATE minera::core)
  target_compile_features(minera_organizational_mining_tests PRIVATE cxx_std_20)
  target_compile_options(minera_organizational_mining_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_organizational_mining_tests COMMAND minera_organizational_mining_tests)
  add_executable(minera_network_analysis_tests
    src/backend/tests/test_network_analysis.cpp
  )
  target_link_libraries(minera_network_analysis_tests PRIVATE minera::core)
  target_compile_features(minera_network_analysis_tests PRIVATE cxx_std_20)
  target_compile_options(minera_network_analysis_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_network_analysis_tests COMMAND minera_network_analysis_tests)

  add_executable(minera_resource_profiles_tests
    src/backend/tests/test_resource_profiles.cpp
  )
  target_link_libraries(minera_resource_profiles_tests PRIVATE minera::core)
  target_compile_features(minera_resource_profiles_tests PRIVATE cxx_std_20)
  target_compile_options(minera_resource_profiles_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_resource_profiles_tests COMMAND minera_resource_profiles_tests)

  add_executable(minera_local_diagnostics_tests
    src/backend/tests/test_local_diagnostics.cpp
  )
  target_link_libraries(minera_local_diagnostics_tests PRIVATE minera::core)
  target_compile_features(minera_local_diagnostics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_local_diagnostics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_local_diagnostics_tests COMMAND minera_local_diagnostics_tests)

  add_executable(minera_clustering_tests src/backend/tests/test_clustering.cpp)
  target_link_libraries(minera_clustering_tests PRIVATE minera::core)
  target_compile_features(minera_clustering_tests PRIVATE cxx_std_20)
  target_compile_options(minera_clustering_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_clustering_tests COMMAND minera_clustering_tests)

  add_executable(minera_process_cube_tests src/backend/tests/test_process_cube.cpp)
  target_link_libraries(minera_process_cube_tests PRIVATE minera::core)
  target_compile_features(minera_process_cube_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_cube_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_cube_tests COMMAND minera_process_cube_tests)

  add_executable(minera_cycle_time_tests src/backend/tests/test_cycle_time.cpp)
  target_link_libraries(minera_cycle_time_tests PRIVATE minera::core)
  target_compile_features(minera_cycle_time_tests PRIVATE cxx_std_20)
  target_compile_options(minera_cycle_time_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_cycle_time_tests COMMAND minera_cycle_time_tests)

  add_executable(minera_case_arrival_tests src/backend/tests/test_case_arrival.cpp)
  target_link_libraries(minera_case_arrival_tests PRIVATE minera::core)
  target_compile_features(minera_case_arrival_tests PRIVATE cxx_std_20)
  target_compile_options(minera_case_arrival_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_case_arrival_tests COMMAND minera_case_arrival_tests)

  add_executable(minera_attribute_statistics_tests src/backend/tests/test_attribute_statistics.cpp)
  target_link_libraries(minera_attribute_statistics_tests PRIVATE minera::core)
  target_compile_features(minera_attribute_statistics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_attribute_statistics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_attribute_statistics_tests COMMAND minera_attribute_statistics_tests)

  add_executable(minera_attribute_distribution_tests src/backend/tests/test_attribute_distribution.cpp)
  target_link_libraries(minera_attribute_distribution_tests PRIVATE minera::core)
  target_compile_features(minera_attribute_distribution_tests PRIVATE cxx_std_20)
  target_compile_options(minera_attribute_distribution_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_attribute_distribution_tests COMMAND minera_attribute_distribution_tests)

  add_executable(minera_attribute_kde_tests src/backend/tests/test_attribute_kde.cpp)
  target_link_libraries(minera_attribute_kde_tests PRIVATE minera::core)
  target_compile_features(minera_attribute_kde_tests PRIVATE cxx_std_20)
  target_compile_options(minera_attribute_kde_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_attribute_kde_tests COMMAND minera_attribute_kde_tests)

  add_executable(minera_boundary_activities_tests src/backend/tests/test_boundary_activities.cpp)
  target_link_libraries(minera_boundary_activities_tests PRIVATE minera::core)
  target_compile_features(minera_boundary_activities_tests PRIVATE cxx_std_20)
  target_compile_options(minera_boundary_activities_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_boundary_activities_tests COMMAND minera_boundary_activities_tests)

  add_executable(minera_variant_statistics_tests src/backend/tests/test_variant_statistics.cpp)
  target_link_libraries(minera_variant_statistics_tests PRIVATE minera::core)
  target_compile_features(minera_variant_statistics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_variant_statistics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_variant_statistics_tests COMMAND minera_variant_statistics_tests)

  add_executable(minera_case_statistics_tests src/backend/tests/test_case_statistics.cpp)
  target_link_libraries(minera_case_statistics_tests PRIVATE minera::core)
  target_compile_features(minera_case_statistics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_case_statistics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_case_statistics_tests COMMAND minera_case_statistics_tests)

  add_executable(minera_streaming_dfg_tests src/backend/tests/test_streaming_dfg.cpp)
  target_link_libraries(minera_streaming_dfg_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_dfg_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_dfg_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_dfg_tests COMMAND minera_streaming_dfg_tests)

  add_executable(minera_streaming_store_tests src/backend/tests/test_streaming_store.cpp)
  target_link_libraries(minera_streaming_store_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_store_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_store_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_store_tests COMMAND minera_streaming_store_tests)

  add_executable(minera_streaming_footprints_tests src/backend/tests/test_streaming_footprints.cpp)
  target_link_libraries(minera_streaming_footprints_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_footprints_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_footprints_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_footprints_tests COMMAND minera_streaming_footprints_tests)

  add_executable(minera_streaming_temporal_tests src/backend/tests/test_streaming_temporal.cpp)
  target_link_libraries(minera_streaming_temporal_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_temporal_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_temporal_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_temporal_tests COMMAND minera_streaming_temporal_tests)

  add_executable(minera_streaming_tbr_tests src/backend/tests/test_streaming_tbr.cpp)
  target_link_libraries(minera_streaming_tbr_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_tbr_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_tbr_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_tbr_tests COMMAND minera_streaming_tbr_tests)

  add_executable(minera_live_stream_tests src/backend/tests/test_live_stream.cpp)
  target_link_libraries(minera_live_stream_tests PRIVATE minera::core)
  target_compile_features(minera_live_stream_tests PRIVATE cxx_std_20)
  target_compile_options(minera_live_stream_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_live_stream_tests COMMAND minera_live_stream_tests)

  add_executable(minera_streaming_import_tests src/backend/tests/test_streaming_import.cpp)
  target_link_libraries(minera_streaming_import_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_import_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_import_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_import_tests COMMAND minera_streaming_import_tests)

  add_executable(minera_data_petri_net_tests src/backend/tests/test_data_petri_net.cpp)
  target_link_libraries(minera_data_petri_net_tests PRIVATE minera::core)
  target_compile_features(minera_data_petri_net_tests PRIVATE cxx_std_20)
  target_compile_options(minera_data_petri_net_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_data_petri_net_tests COMMAND minera_data_petri_net_tests)

  add_executable(minera_decision_table_tests src/backend/tests/test_decision_table.cpp)
  target_link_libraries(minera_decision_table_tests PRIVATE minera::core)
  target_compile_features(minera_decision_table_tests PRIVATE cxx_std_20)
  target_compile_options(minera_decision_table_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_decision_table_tests COMMAND minera_decision_table_tests)

  add_executable(minera_process_tree_playout_tests
    src/backend/tests/test_process_tree_playout.cpp
  )
  target_link_libraries(minera_process_tree_playout_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_playout_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_playout_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_playout_tests COMMAND minera_process_tree_playout_tests)

  add_executable(minera_playout_tests
    src/backend/tests/test_playout.cpp
  )
  target_link_libraries(minera_playout_tests PRIVATE minera::core)
  target_compile_features(minera_playout_tests PRIVATE cxx_std_20)
  target_compile_options(minera_playout_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_playout_tests COMMAND minera_playout_tests)

  add_executable(minera_stochastic_petri_tests
    src/backend/tests/test_stochastic_petri.cpp
  )
  target_link_libraries(minera_stochastic_petri_tests PRIVATE minera::core)
  target_compile_features(minera_stochastic_petri_tests PRIVATE cxx_std_20)
  target_compile_options(minera_stochastic_petri_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_stochastic_petri_tests COMMAND minera_stochastic_petri_tests)

  add_executable(minera_stochastic_map_tests
    src/backend/tests/test_stochastic_map.cpp
  )
  target_link_libraries(minera_stochastic_map_tests PRIVATE minera::core)
  target_compile_features(minera_stochastic_map_tests PRIVATE cxx_std_20)
  target_compile_options(minera_stochastic_map_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_stochastic_map_tests COMMAND minera_stochastic_map_tests)

  add_executable(minera_monte_carlo_tests
    src/backend/tests/test_monte_carlo.cpp
  )
  target_link_libraries(minera_monte_carlo_tests PRIVATE minera::core)
  target_compile_features(minera_monte_carlo_tests PRIVATE cxx_std_20)
  target_compile_options(minera_monte_carlo_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_monte_carlo_tests COMMAND minera_monte_carlo_tests)

  add_executable(minera_correlation_miner_tests
    src/backend/tests/test_correlation_miner.cpp
  )
  target_link_libraries(minera_correlation_miner_tests PRIVATE minera::core)
  target_compile_features(minera_correlation_miner_tests PRIVATE cxx_std_20)
  target_compile_options(minera_correlation_miner_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_correlation_miner_tests COMMAND minera_correlation_miner_tests)

  add_executable(minera_perspective_tests
    src/backend/tests/test_perspectives.cpp
  )
  target_link_libraries(minera_perspective_tests PRIVATE minera::core)
  target_compile_features(minera_perspective_tests PRIVATE cxx_std_20)
  target_compile_options(minera_perspective_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_perspective_tests COMMAND minera_perspective_tests)

  add_executable(minera_request_params_tests
    src/backend/tests/test_request_params.cpp
  )
  target_link_libraries(minera_request_params_tests PRIVATE minera::core)
  target_compile_features(minera_request_params_tests PRIVATE cxx_std_20)
  target_compile_options(minera_request_params_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_request_params_tests COMMAND minera_request_params_tests)

  add_executable(minera_powl_tests
    src/backend/tests/test_powl.cpp
  )
  target_link_libraries(minera_powl_tests PRIVATE minera::core)
  target_compile_features(minera_powl_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_powl_tests COMMAND minera_powl_tests)

  add_executable(minera_powl_discovery_tests
    src/backend/tests/test_powl_discovery.cpp
  )
  target_link_libraries(minera_powl_discovery_tests PRIVATE minera::core)
  target_compile_features(minera_powl_discovery_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_discovery_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_powl_discovery_tests COMMAND minera_powl_discovery_tests)

  add_executable(minera_powl_parity_contracts_tests
    src/backend/tests/test_powl_parity_contracts.cpp
  )
  target_link_libraries(minera_powl_parity_contracts_tests PRIVATE minera::core)
  target_include_directories(minera_powl_parity_contracts_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
  )
  target_compile_features(minera_powl_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_powl_parity_contracts_tests
           COMMAND minera_powl_parity_contracts_tests)

  add_executable(minera_powl_conversion_tests
    src/backend/tests/test_powl_conversion.cpp
  )
  target_link_libraries(minera_powl_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_powl_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_powl_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_powl_conversion_tests COMMAND minera_powl_conversion_tests)

  add_executable(minera_log_skeleton_tests
    src/backend/tests/test_log_skeleton.cpp
  )
  target_link_libraries(minera_log_skeleton_tests PRIVATE minera::core)
  target_compile_features(minera_log_skeleton_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_skeleton_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_log_skeleton_tests COMMAND minera_log_skeleton_tests)

  add_executable(minera_log_to_target_tests
    src/backend/tests/test_log_to_target.cpp
  )
  target_link_libraries(minera_log_to_target_tests PRIVATE minera::core)
  target_compile_features(minera_log_to_target_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_to_target_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_to_target_tests COMMAND minera_log_to_target_tests)

  add_executable(minera_log_to_features_tests
    src/backend/tests/test_log_to_features.cpp
  )
  target_link_libraries(minera_log_to_features_tests PRIVATE minera::core)
  target_compile_features(minera_log_to_features_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_to_features_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_to_features_tests COMMAND minera_log_to_features_tests)

  add_executable(minera_log_util_tests src/backend/tests/test_log_util.cpp)
  target_link_libraries(minera_log_util_tests PRIVATE minera::core)
  target_compile_features(minera_log_util_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_util_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_util_tests COMMAND minera_log_util_tests)

  add_executable(minera_log_conversion_tests
    src/backend/tests/test_log_conversion.cpp
  )
  target_link_libraries(minera_log_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_log_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_conversion_tests COMMAND minera_log_conversion_tests)

  add_executable(minera_label_splitting_tests
    src/backend/tests/test_label_splitting.cpp
  )
  target_link_libraries(minera_label_splitting_tests PRIVATE minera::core)
  target_compile_features(minera_label_splitting_tests PRIVATE cxx_std_20)
  target_compile_options(minera_label_splitting_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_label_splitting_tests COMMAND minera_label_splitting_tests)

  add_executable(minera_footprints_tests
    src/backend/tests/test_footprints.cpp
  )
  target_link_libraries(minera_footprints_tests PRIVATE minera::core)
  target_include_directories(minera_footprints_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
  )
  target_compile_features(minera_footprints_tests PRIVATE cxx_std_20)
  target_compile_options(minera_footprints_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_footprints_tests COMMAND minera_footprints_tests)

  add_executable(minera_footprints_models_tests
    src/backend/tests/test_footprints_models.cpp
  )
  target_link_libraries(minera_footprints_models_tests PRIVATE minera::core)
  target_compile_features(minera_footprints_models_tests PRIVATE cxx_std_20)
  target_compile_options(minera_footprints_models_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_footprints_models_tests COMMAND minera_footprints_models_tests)

  add_executable(minera_footprints_discovery_parity_contracts_tests
    src/backend/tests/test_footprints_discovery_parity_contracts.cpp
  )
  target_link_libraries(minera_footprints_discovery_parity_contracts_tests PRIVATE minera::core)
  target_include_directories(minera_footprints_discovery_parity_contracts_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
  )
  target_compile_features(minera_footprints_discovery_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_footprints_discovery_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_footprints_discovery_parity_contracts_tests
           COMMAND minera_footprints_discovery_parity_contracts_tests)

  add_executable(minera_footprints_conformance_parity_contracts_tests
    src/backend/tests/test_footprints_conformance_parity_contracts.cpp
  )
  target_link_libraries(minera_footprints_conformance_parity_contracts_tests PRIVATE minera::core)
  target_include_directories(minera_footprints_conformance_parity_contracts_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
  )
  target_compile_features(minera_footprints_conformance_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_footprints_conformance_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_footprints_conformance_parity_contracts_tests
           COMMAND minera_footprints_conformance_parity_contracts_tests)

  add_executable(minera_declare_tests
    src/backend/tests/test_declare.cpp
  )
  target_link_libraries(minera_declare_tests PRIVATE minera::core)
  target_compile_features(minera_declare_tests PRIVATE cxx_std_20)
  target_compile_options(minera_declare_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_declare_tests COMMAND minera_declare_tests)

  add_executable(minera_declare_variants_tests
    src/backend/tests/test_declare_variants.cpp
  )
  target_link_libraries(minera_declare_variants_tests PRIVATE minera::core)
  target_compile_features(minera_declare_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_declare_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_declare_variants_tests COMMAND minera_declare_variants_tests)

  add_executable(minera_heuristic_variants_tests
    src/backend/tests/test_heuristic_variants.cpp
  )
  target_link_libraries(minera_heuristic_variants_tests PRIVATE minera::core)
  target_compile_features(minera_heuristic_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_heuristic_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_heuristic_variants_tests COMMAND minera_heuristic_variants_tests)

  add_executable(minera_dfg_variants_tests
    src/backend/tests/test_dfg_variants.cpp
  )
  target_link_libraries(minera_dfg_variants_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_dfg_variants_tests COMMAND minera_dfg_variants_tests)

  add_executable(minera_dfg_parity_contracts_tests
    src/backend/tests/test_dfg_parity_contracts.cpp
  )
  target_link_libraries(minera_dfg_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_parity_contracts_tests COMMAND minera_dfg_parity_contracts_tests)

  add_executable(minera_dfg_utils_tests
    src/backend/tests/test_dfg_utils.cpp
  )
  target_link_libraries(minera_dfg_utils_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_utils_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_utils_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_utils_tests COMMAND minera_dfg_utils_tests)

  add_executable(minera_dfg_dataframe_tests
    src/backend/tests/test_dfg_dataframe.cpp
  )
  target_link_libraries(minera_dfg_dataframe_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_dataframe_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_dataframe_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_dataframe_tests COMMAND minera_dfg_dataframe_tests)

  add_executable(minera_dfg_dataframe_json_tests
    src/backend/tests/test_dfg_dataframe_json.cpp
  )
  target_link_libraries(minera_dfg_dataframe_json_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_dataframe_json_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_dataframe_json_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_dataframe_json_tests
           COMMAND minera_dfg_dataframe_json_tests)

  add_executable(minera_dfg_playout_tests
    src/backend/tests/test_dfg_playout.cpp
  )
  target_link_libraries(minera_dfg_playout_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_playout_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_playout_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_playout_tests COMMAND minera_dfg_playout_tests)

  add_executable(minera_dfg_io_tests
    src/backend/tests/test_dfg_io.cpp
  )
  target_link_libraries(minera_dfg_io_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_io_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_io_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_io_tests COMMAND minera_dfg_io_tests)

  add_executable(minera_dfg_precision_tests src/backend/tests/test_dfg_precision.cpp)
  target_link_libraries(minera_dfg_precision_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_precision_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_precision_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_precision_tests COMMAND minera_dfg_precision_tests)

  add_executable(minera_temporal_behavioral_tests
    src/backend/tests/test_temporal_behavioral.cpp
  )
  target_link_libraries(minera_temporal_behavioral_tests PRIVATE minera::core)
  target_compile_features(minera_temporal_behavioral_tests PRIVATE cxx_std_20)
  target_compile_options(minera_temporal_behavioral_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_temporal_behavioral_tests COMMAND minera_temporal_behavioral_tests)

  add_executable(minera_temporal_profile_parity_contracts_tests
    src/backend/tests/test_temporal_profile_parity_contracts.cpp
  )
  target_link_libraries(minera_temporal_profile_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_temporal_profile_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_temporal_profile_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_temporal_profile_parity_contracts_tests
           COMMAND minera_temporal_profile_parity_contracts_tests)

  add_executable(minera_transition_system_parity_contracts_tests
    src/backend/tests/transition_system_parity_contracts.cpp
  )
  target_link_libraries(minera_transition_system_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_transition_system_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_transition_system_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_transition_system_parity_contracts_tests
           COMMAND minera_transition_system_parity_contracts_tests)

  add_executable(minera_minimum_self_distance_parity_contracts_tests
    src/backend/tests/minimum_self_distance_parity_contracts.cpp
  )
  target_link_libraries(minera_minimum_self_distance_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_minimum_self_distance_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_minimum_self_distance_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_minimum_self_distance_parity_contracts_tests
           COMMAND minera_minimum_self_distance_parity_contracts_tests)

  add_executable(minera_performance_spectrum_parity_contracts_tests
    src/backend/tests/test_performance_spectrum_parity_contracts.cpp
  )
  target_link_libraries(minera_performance_spectrum_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_performance_spectrum_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_performance_spectrum_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_performance_spectrum_parity_contracts_tests
           COMMAND minera_performance_spectrum_parity_contracts_tests)

  add_executable(minera_batches_parity_contracts_tests
    src/backend/tests/test_batches_parity_contracts.cpp
  )
  target_link_libraries(minera_batches_parity_contracts_tests PRIVATE minera::core)
  target_include_directories(minera_batches_parity_contracts_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
  )
  target_compile_features(minera_batches_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_batches_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_batches_parity_contracts_tests
           COMMAND minera_batches_parity_contracts_tests)

  add_executable(minera_concept_drift_tests
    src/backend/tests/test_concept_drift.cpp
  )
  target_link_libraries(minera_concept_drift_tests PRIVATE minera::core)
  target_compile_features(minera_concept_drift_tests PRIVATE cxx_std_20)
  target_compile_options(minera_concept_drift_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_concept_drift_tests COMMAND minera_concept_drift_tests)

  add_executable(minera_browser_history_tests
    src/backend/tests/test_browser_history.cpp
  )
  target_link_libraries(minera_browser_history_tests PRIVATE minera::core)
  if(WIN32)
    target_link_libraries(minera_browser_history_tests PRIVATE winsqlite3)
  elseif(SQLite3_FOUND)
    target_link_libraries(minera_browser_history_tests PRIVATE SQLite::SQLite3)
  endif()
  target_compile_features(minera_browser_history_tests PRIVATE cxx_std_20)
  target_compile_options(minera_browser_history_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_browser_history_tests COMMAND minera_browser_history_tests)

  add_executable(minera_windows_event_log_tests
    src/backend/tests/test_windows_event_log.cpp
  )
  target_link_libraries(minera_windows_event_log_tests PRIVATE minera::core)
  target_compile_features(minera_windows_event_log_tests PRIVATE cxx_std_20)
  target_compile_options(minera_windows_event_log_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_windows_event_log_tests COMMAND minera_windows_event_log_tests)

  add_executable(minera_repository_history_tests
    src/backend/tests/test_repository_history.cpp
  )
  target_link_libraries(minera_repository_history_tests PRIVATE minera::core)
  target_compile_features(minera_repository_history_tests PRIVATE cxx_std_20)
  target_compile_options(minera_repository_history_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_repository_history_tests COMMAND minera_repository_history_tests)

  add_executable(minera_workflow_history_tests
    src/backend/tests/test_workflow_history.cpp
  )
  target_link_libraries(minera_workflow_history_tests PRIVATE minera::core)
  target_compile_features(minera_workflow_history_tests PRIVATE cxx_std_20)
  target_compile_options(minera_workflow_history_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_workflow_history_tests COMMAND minera_workflow_history_tests)

  add_executable(minera_enterprise_history_tests
    src/backend/tests/test_enterprise_history.cpp
  )
  target_link_libraries(minera_enterprise_history_tests PRIVATE minera::core)
  target_compile_features(minera_enterprise_history_tests PRIVATE cxx_std_20)
  target_compile_options(minera_enterprise_history_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_enterprise_history_tests COMMAND minera_enterprise_history_tests)

  add_executable(minera_communication_history_tests
    src/backend/tests/test_communication_history.cpp
  )
  target_link_libraries(minera_communication_history_tests PRIVATE minera::core)
  target_compile_features(minera_communication_history_tests PRIVATE cxx_std_20)
  target_compile_options(minera_communication_history_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_communication_history_tests COMMAND minera_communication_history_tests)

  add_executable(minera_alignment_variants_tests
    src/backend/tests/test_alignment_variants.cpp
  )
  target_link_libraries(minera_alignment_variants_tests PRIVATE minera::core)
  target_compile_features(minera_alignment_variants_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alignment_variants_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alignment_variants_tests COMMAND minera_alignment_variants_tests)

  add_executable(minera_process_tree_alignment_tests
    src/backend/tests/test_process_tree_alignment.cpp
  )
  target_link_libraries(minera_process_tree_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_alignment_tests COMMAND minera_process_tree_alignment_tests)

  add_executable(minera_process_tree_reduction_tests
    src/backend/tests/test_process_tree_reduction.cpp
  )
  target_link_libraries(minera_process_tree_reduction_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_reduction_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_reduction_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_reduction_tests COMMAND minera_process_tree_reduction_tests)

  add_executable(minera_petri_net_conversion_tests
    src/backend/tests/test_petri_net_conversion.cpp
  )
  target_link_libraries(minera_petri_net_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_petri_net_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_net_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_net_conversion_tests COMMAND minera_petri_net_conversion_tests)

  add_executable(minera_petri_net_reduction_tests
    src/backend/tests/test_petri_net_reduction.cpp
  )
  target_link_libraries(minera_petri_net_reduction_tests PRIVATE minera::core)
  target_compile_features(minera_petri_net_reduction_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_net_reduction_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_net_reduction_tests COMMAND minera_petri_net_reduction_tests)

  add_executable(minera_dfg_alignment_tests
    src/backend/tests/test_dfg_alignment.cpp
  )
  target_link_libraries(minera_dfg_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_alignment_tests COMMAND minera_dfg_alignment_tests)

  add_executable(minera_edit_distance_alignment_tests
    src/backend/tests/test_edit_distance_alignment.cpp
  )
  target_link_libraries(minera_edit_distance_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_edit_distance_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_edit_distance_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_edit_distance_alignment_tests COMMAND minera_edit_distance_alignment_tests)

  add_executable(minera_decomposed_alignment_tests
    src/backend/tests/test_decomposed_alignment.cpp
  )
  target_link_libraries(minera_decomposed_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_decomposed_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_decomposed_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_decomposed_alignment_tests COMMAND minera_decomposed_alignment_tests)

  add_executable(minera_petri_anti_multi_alignment_tests
    src/backend/tests/test_petri_anti_multi_alignment.cpp
  )
  target_link_libraries(minera_petri_anti_multi_alignment_tests PRIVATE minera::core)
  target_compile_features(minera_petri_anti_multi_alignment_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_anti_multi_alignment_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_anti_multi_alignment_tests COMMAND minera_petri_anti_multi_alignment_tests)

  add_executable(minera_evaluation_tests
    src/backend/tests/test_evaluation.cpp
  )
  target_link_libraries(minera_evaluation_tests PRIVATE minera::core)
  target_compile_features(minera_evaluation_tests PRIVATE cxx_std_20)
  target_compile_options(minera_evaluation_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_evaluation_tests COMMAND minera_evaluation_tests)

  add_executable(minera_petri_analysis_tests
    src/backend/tests/test_petri_analysis.cpp
  )
  target_link_libraries(minera_petri_analysis_tests PRIVATE minera::core)
  target_compile_features(minera_petri_analysis_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_analysis_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_analysis_tests COMMAND minera_petri_analysis_tests)

  add_executable(minera_analysis_parity_contracts_tests
    src/backend/tests/test_analysis_parity_contracts.cpp
  )
  target_link_libraries(minera_analysis_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_analysis_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_analysis_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_analysis_parity_contracts_tests COMMAND minera_analysis_parity_contracts_tests)

  add_executable(minera_organizational_parity_contracts_tests
    src/backend/tests/test_organizational_parity_contracts.cpp
  )
  target_link_libraries(minera_organizational_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_organizational_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_organizational_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_organizational_parity_contracts_tests COMMAND minera_organizational_parity_contracts_tests)

  add_executable(minera_petri_extended_arcs_tests
    src/backend/tests/test_petri_extended_arcs.cpp
  )
  target_link_libraries(minera_petri_extended_arcs_tests PRIVATE minera::core)
  target_compile_features(minera_petri_extended_arcs_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_extended_arcs_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_extended_arcs_tests COMMAND minera_petri_extended_arcs_tests)

  add_executable(minera_petri_usage_comparison_tests
    src/backend/tests/test_petri_usage_comparison.cpp
  )
  target_link_libraries(minera_petri_usage_comparison_tests PRIVATE minera::core)
  target_compile_features(minera_petri_usage_comparison_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_usage_comparison_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_usage_comparison_tests COMMAND minera_petri_usage_comparison_tests)

  add_executable(minera_trie_tests
    src/backend/tests/test_trie.cpp
  )
  target_link_libraries(minera_trie_tests PRIVATE minera::core)
  target_compile_features(minera_trie_tests PRIVATE cxx_std_20)
  target_compile_options(minera_trie_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_trie_tests COMMAND minera_trie_tests)

  add_executable(minera_trie_conversion_tests
    src/backend/tests/test_trie_conversion.cpp
  )
  target_link_libraries(minera_trie_conversion_tests PRIVATE minera::core)
  target_compile_features(minera_trie_conversion_tests PRIVATE cxx_std_20)
  target_compile_options(minera_trie_conversion_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_trie_conversion_tests COMMAND minera_trie_conversion_tests)

  add_executable(minera_interval_tree_tests
    src/backend/tests/test_interval_tree.cpp
  )
  target_link_libraries(minera_interval_tree_tests PRIVATE minera::core)
  target_compile_features(minera_interval_tree_tests PRIVATE cxx_std_20)
  target_compile_options(minera_interval_tree_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_interval_tree_tests COMMAND minera_interval_tree_tests)

  add_executable(minera_case_relations_tests
    src/backend/tests/test_case_relations.cpp
  )
  target_link_libraries(minera_case_relations_tests PRIVATE minera::core)
  target_compile_features(minera_case_relations_tests PRIVATE cxx_std_20)
  target_compile_options(minera_case_relations_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_case_relations_tests COMMAND minera_case_relations_tests)

  add_executable(minera_random_tree_tests
    src/backend/tests/test_random_tree.cpp
  )
  target_link_libraries(minera_random_tree_tests PRIVATE minera::core)
  target_compile_features(minera_random_tree_tests PRIVATE cxx_std_20)
  target_compile_options(minera_random_tree_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_random_tree_tests COMMAND minera_random_tree_tests)

  add_executable(minera_saw_nets_tests
    src/backend/tests/test_saw_nets.cpp
  )
  target_link_libraries(minera_saw_nets_tests PRIVATE minera::core)
  target_compile_features(minera_saw_nets_tests PRIVATE cxx_std_20)
  target_compile_options(minera_saw_nets_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_saw_nets_tests COMMAND minera_saw_nets_tests)

  add_executable(minera_ocel_transform_tests
    src/backend/tests/test_ocel_transform.cpp
  )
  target_link_libraries(minera_ocel_transform_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_transform_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_transform_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_transform_tests COMMAND minera_ocel_transform_tests)

  add_executable(minera_ocel_playout_tests
    src/backend/tests/test_ocel_playout.cpp
  )
  target_link_libraries(minera_ocel_playout_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_playout_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_playout_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_playout_tests COMMAND minera_ocel_playout_tests)

  add_executable(minera_ocel_features_tests
    src/backend/tests/test_ocel_features.cpp
  )
  target_link_libraries(minera_ocel_features_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_features_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_features_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_features_tests COMMAND minera_ocel_features_tests)

  add_executable(minera_ocel_feature_components_tests
    src/backend/tests/test_ocel_feature_components.cpp
  )
  target_link_libraries(minera_ocel_feature_components_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_feature_components_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_feature_components_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_feature_components_tests
           COMMAND minera_ocel_feature_components_tests)

  add_test(NAME minera_ocel_features_cli_lifecycle_paths
           COMMAND minera_ocel
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/ocel_split.json"
                   --features objects
                   --features-lifecycle-paths)
  set_tests_properties(minera_ocel_features_cli_lifecycle_paths PROPERTIES
    PASS_REGULAR_EXPRESSION "@@ocel_lif_path_Create##Pack")

  add_executable(minera_oc_causal_net_tests
    src/backend/tests/test_oc_causal_net.cpp
  )
  target_link_libraries(minera_oc_causal_net_tests PRIVATE minera::core)
  target_compile_features(minera_oc_causal_net_tests PRIVATE cxx_std_20)
  target_compile_options(minera_oc_causal_net_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_oc_causal_net_tests COMMAND minera_oc_causal_net_tests)

  add_executable(minera_link_analysis_tests
    src/backend/tests/test_link_analysis.cpp
  )
  target_link_libraries(minera_link_analysis_tests PRIVATE minera::core)
  target_compile_features(minera_link_analysis_tests PRIVATE cxx_std_20)
  target_compile_options(minera_link_analysis_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_link_analysis_tests COMMAND minera_link_analysis_tests)

  add_executable(minera_timestamp_interleavings_tests
    src/backend/tests/test_timestamp_interleavings.cpp
  )
  target_link_libraries(minera_timestamp_interleavings_tests PRIVATE minera::core)
  target_compile_features(minera_timestamp_interleavings_tests PRIVATE cxx_std_20)
  target_compile_options(minera_timestamp_interleavings_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_timestamp_interleavings_tests COMMAND minera_timestamp_interleavings_tests)

  add_executable(minera_overlap_tests
    src/backend/tests/test_overlap.cpp
  )
  target_link_libraries(minera_overlap_tests PRIVATE minera::core)
  target_compile_features(minera_overlap_tests PRIVATE cxx_std_20)
  target_compile_options(minera_overlap_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_overlap_tests COMMAND minera_overlap_tests)

  add_executable(minera_overlap_json_tests
    src/backend/tests/test_overlap_json.cpp
  )
  target_link_libraries(minera_overlap_json_tests PRIVATE minera::core)
  target_compile_features(minera_overlap_json_tests PRIVATE cxx_std_20)
  target_compile_options(minera_overlap_json_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_overlap_json_tests COMMAND minera_overlap_json_tests)

  add_executable(minera_knowledge_injection_tests
    src/backend/tests/test_knowledge_injection.cpp
  )
  target_link_libraries(minera_knowledge_injection_tests PRIVATE minera::core)
  target_compile_features(minera_knowledge_injection_tests PRIVATE cxx_std_20)
  target_compile_options(minera_knowledge_injection_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_knowledge_injection_tests
           COMMAND minera_knowledge_injection_tests)
endif()
