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
  src/backend/alpha_relations.cpp
  src/backend/alpha_event_data.cpp
  src/backend/alpha_result_json.cpp
  src/backend/activity_split.cpp
  src/backend/attribute_statistics.cpp
  src/backend/attribute_distribution.cpp
  src/backend/attribute_kde.cpp
  src/backend/boundary_activities.cpp
  src/backend/filtering_boundary_common.cpp
  src/backend/log_trace_filter.cpp
  src/backend/log_segments_filter.cpp
  src/backend/log_boundary_filter.cpp
  src/backend/log_rework_filter.cpp
  src/backend/log_attribute_repetition_filter.cpp
  src/backend/log_attribute_repetition_controls.cpp
  src/backend/log_between_filter.cpp
  src/backend/pandas_between_filter.cpp
  src/backend/polars_between_filter.cpp
  src/backend/table_between_filter_workbench.cpp
  src/backend/case_filter_controls.cpp
  src/backend/log_case_filter.cpp
  src/backend/pandas_case_filter.cpp
  src/backend/polars_case_filter.cpp
  src/backend/table_case_filter_workbench.cpp
  src/backend/paths_filter_controls.cpp
  src/backend/log_paths_filter.cpp
  src/backend/pandas_paths_filter.cpp
  src/backend/polars_paths_filter.cpp
  src/backend/table_paths_filter_workbench.cpp
  src/backend/attributes_filter_controls.cpp
  src/backend/log_attributes_filter.cpp
  src/backend/pandas_attributes_filter.cpp
  src/backend/polars_attributes_filter.cpp
  src/backend/log_attributes_filter_workbench.cpp
  src/backend/log_variants_filter_workbench.cpp
  src/backend/log_ltl_filter.cpp
  src/backend/pandas_ltl_filter.cpp
  src/backend/polars_ltl_filter.cpp
  src/backend/ltl_filter_workbench.cpp
  src/backend/table_attributes_filter_workbench.cpp
  src/backend/variants_filter_controls.cpp
  src/backend/log_variants_filter.cpp
  src/backend/pandas_variants_filter.cpp
  src/backend/polars_variants_filter.cpp
  src/backend/table_variants_filter_workbench.cpp
  src/backend/log_timestamp_filter_workbench.cpp
  src/backend/log_timestamp_filter.cpp
  src/backend/pandas_timestamp_filter.cpp
  src/backend/polars_timestamp_filter.cpp
  src/backend/table_timestamp_filter_workbench.cpp
  src/backend/pandas_attribute_repetition_filter.cpp
  src/backend/polars_attribute_repetition_filter.cpp
  src/backend/ocel_exact_filter_controls.cpp
  src/backend/reference_log_filter_workbench.cpp
  src/backend/table_attribute_repetition_workbench.cpp
  src/backend/pripel_trace_distance.cpp
  src/backend/attribute_value_repetition.cpp
  src/backend/case_grouping.cpp
  src/backend/chaotic_activities.cpp
  src/backend/bpmn.cpp
  src/backend/browser_history.cpp
  src/backend/source_acquisition.cpp
  src/backend/windows_event_source.cpp
  src/backend/database_history_source.cpp
  src/backend/outlook_source.cpp
  src/backend/batches.cpp
  src/backend/batches/discovery_registry.cpp
  src/backend/case_relations.cpp
  src/backend/case_arrival.cpp
  src/backend/case_statistics.cpp
  src/backend/oc_causal_net.cpp
  src/backend/conformance.cpp
  src/backend/token_replay_duration.cpp
  src/backend/replay_duration_workbench_json.cpp
  src/backend/token_replay_event_data.cpp
  src/backend/communication_history.cpp
  src/backend/concept_drift.cpp
  src/backend/concept_drift_table.cpp
  src/backend/cycle_time.cpp
  src/backend/correlation_miner.cpp
  src/backend/temporal_profile_json.cpp
  src/backend/precomputed_replay_json.cpp
  src/backend/clustering.cpp
  src/backend/csv_event_log_importer.cpp
  src/backend/dataframe_validation.cpp
  src/backend/dataframe_format.cpp
  src/backend/dataframe_format_json.cpp
  src/backend/workflow_input_result_json.cpp
  src/backend/graph_alignment_workbench_json.cpp
  src/backend/knowledge_exact_json.cpp
  src/backend/tree_reduction_workbench_json.cpp
  src/backend/original_features_workbench_json.cpp
  src/backend/original_label_splitting_workbench_json.cpp
  src/backend/original_log_query_workbench_json.cpp
  src/backend/exact_log_query_json.cpp
  src/backend/original_model_context.cpp
  src/backend/case_description.cpp
  src/backend/case_description_workbench_json.cpp
  src/backend/original_model_context_workbench_json.cpp
  src/backend/otg_threshold_json.cpp
  src/backend/saved_dfg_projection_json.cpp
  src/backend/inductive_input_json.cpp
  src/backend/process_tree_alignment_workbench_json.cpp
  src/backend/petri_anti_multi_workbench_json.cpp
  src/backend/model_conversion_workbench_json.cpp
  src/backend/active_event_source.cpp
  src/backend/monte_carlo_workbench_json.cpp
  src/backend/playout_workbench_json.cpp
  src/backend/dfg_playout_input_json.cpp
  src/backend/decision_workbench_json.cpp
  src/backend/declare_workbench_json.cpp
  src/backend/log_skeleton_workbench_json.cpp
  src/backend/footprints_workbench_json.cpp
  src/backend/genetic_matrix_workbench_json.cpp
  src/backend/embedding_workbench_json.cpp
  src/backend/emd_workbench_json.cpp
  src/backend/object_centric_playout_workbench_json.cpp
  src/backend/literal_controls_json.cpp
  src/backend/edit_distance_workbench_json.cpp
  src/backend/bose_workbench_json.cpp
  src/backend/discovery_workbench_json.cpp
  src/backend/petri_analysis_workbench_json.cpp
  src/backend/root_cause_workbench_json.cpp
  src/backend/anonymization_workbench_json.cpp
  src/backend/quality_workbench_json.cpp
  src/backend/clustering_workbench_json.cpp
  src/backend/organizational_workbench_json.cpp
  src/backend/temporal_workbench_json.cpp
  src/backend/timing_workbench_json.cpp
  src/backend/time_analysis_workbench_json.cpp
  src/backend/structural_inputs_json.cpp
  src/backend/petri_net_literal_storage.cpp
  src/backend/data_petri_net.cpp
  src/backend/declare.cpp
  src/backend/declare_playout.cpp
  src/backend/declare_streaming.cpp
  src/backend/declare_streaming_io.cpp
  src/backend/decision_mining.cpp
  src/backend/root_cause.cpp
  src/backend/root_cause_trace_key.cpp
  src/backend/token_replay_root_cause.cpp
  src/backend/token_replay_result_json.cpp
  src/backend/decision_replay.cpp
  src/backend/decision_table.cpp
  src/backend/dfg_playout.cpp
  src/backend/dfg_alignment.cpp
  src/backend/dfg_filter.cpp
  src/backend/dfg_filtering.cpp
  src/backend/filtering_common.cpp
  src/backend/pandas_sequence_filters.cpp
  src/backend/polars_sequence_filters.cpp
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
  src/backend/petri_trace_alignment.cpp
  src/backend/alignment_log_enrichment.cpp
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
  src/backend/ilp_reference.cpp
  src/backend/genetic_reference.cpp
  src/backend/interval_tree.cpp
  src/backend/overlap.cpp
  src/backend/label_splitting.cpp
  src/backend/llm_injection.cpp
  src/backend/llm_abstractions.cpp
  src/backend/llm_sql_utils.cpp
  src/backend/trace_variant_query_reference.cpp
  src/backend/pripel_reference.cpp
  src/backend/process_tree_search_graph_reference.cpp
  src/backend/process_tree_milp_reference.cpp
  src/backend/process_tree_approximated_reference.cpp
  src/backend/decomposed_reference.cpp
  src/backend/connector_reference.cpp
  src/backend/llm_connector_reference.cpp
  src/backend/embedding_reference.cpp
  src/backend/temporal_lazy_reference.cpp
  src/backend/lle_reference.cpp
  src/backend/trace_attribute_reference.cpp
  src/backend/live_stream.cpp
  src/backend/streaming_session.cpp
  src/backend/link_analysis.cpp
  src/backend/json_export.cpp
  src/backend/knowledge_injection.cpp
  src/backend/log_query.cpp
  src/backend/llm_context.cpp
  src/backend/log_filter.cpp
  src/backend/infix_to_regex.cpp
  src/backend/log_conversion.cpp
  src/backend/path_filter.cpp
  src/backend/passed_time.cpp
  src/backend/segment_filter.cpp
  src/backend/service_time.cpp
  src/backend/streaming_dfg.cpp
  src/backend/streaming_store.cpp
  src/backend/streaming_footprints.cpp
  src/backend/streaming_import.cpp
  src/backend/reader_stream_session.cpp
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
  src/backend/ocdfg_edge_performance.cpp
  src/backend/ocdfg_performance_controls.cpp
  src/backend/ocdfg_edge_metrics.cpp
  src/backend/ocdfg_edge_metrics_json.cpp
  src/backend/ocel_csv.cpp
  src/backend/ocel_sqlite.cpp
  src/backend/ocel_filter.cpp
  src/backend/ocel_activity_type_matching.cpp
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
  src/backend/reference_tree_net.cpp
  src/backend/saw_nets.cpp
  src/backend/process_tree_playout.cpp
  src/backend/process_tree_reduction.cpp
  src/backend/powl.cpp
  src/backend/powl_conversion.cpp
  src/backend/powl_discovery.cpp
  src/backend/pnml.cpp
  src/backend/ptml.cpp
  src/backend/exact_filter_discovery_json.cpp
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
  src/backend/interleavings_merge.cpp
  src/backend/organizational_util.cpp
  src/backend/precision_utils.cpp
  src/backend/ocpn_reference_playout.cpp
  src/backend/oc_causal_net_reference_playout.cpp
  src/backend/process_tree_reference_alignment.cpp
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

add_executable(minera_ltl_filter src/backend/tools/minera_ltl_filter.cpp)
target_link_libraries(minera_ltl_filter PRIVATE minera::core)
target_compile_features(minera_ltl_filter PRIVATE cxx_std_20)

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

add_executable(minera_format_dataframe src/backend/tools/minera_format_dataframe.cpp)
target_link_libraries(minera_format_dataframe PRIVATE minera::core)
target_compile_features(minera_format_dataframe PRIVATE cxx_std_20)

add_executable(minera_repetition_batch src/backend/tools/minera_repetition_batch.cpp)
target_link_libraries(minera_repetition_batch PRIVATE minera::core)
target_compile_features(minera_repetition_batch PRIVATE cxx_std_20)

add_executable(minera_knowledge src/backend/tools/minera_knowledge.cpp)
add_executable(minera_model_context src/backend/tools/minera_model_context.cpp)
target_link_libraries(minera_model_context PRIVATE minera::core)
target_compile_features(minera_model_context PRIVATE cxx_std_20)
add_executable(minera_tree_reduction src/backend/tools/minera_tree_reduction.cpp)
target_link_libraries(minera_tree_reduction PRIVATE minera::core)
target_compile_features(minera_tree_reduction PRIVATE cxx_std_20)
add_executable(minera_label_splitting src/backend/tools/minera_label_splitting.cpp)
target_link_libraries(minera_label_splitting PRIVATE minera::core)
target_compile_features(minera_label_splitting PRIVATE cxx_std_20)
add_executable(minera_features src/backend/tools/minera_features.cpp)
target_link_libraries(minera_features PRIVATE minera::core)
target_compile_features(minera_features PRIVATE cxx_std_20)
add_executable(minera_query src/backend/tools/minera_query.cpp)
add_executable(minera_cluster src/backend/tools/minera_cluster.cpp)
add_executable(minera_context src/backend/tools/minera_context.cpp)
target_link_libraries(minera_context PRIVATE minera::core)
target_compile_features(minera_context PRIVATE cxx_std_20)
target_link_libraries(minera_cluster PRIVATE minera::core)
target_include_directories(minera_cluster PRIVATE third_party)
target_compile_features(minera_cluster PRIVATE cxx_std_20)
target_link_libraries(minera_query PRIVATE minera::core)
target_include_directories(minera_query PRIVATE third_party)
target_compile_features(minera_query PRIVATE cxx_std_20)
target_include_directories(minera_knowledge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
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
add_executable(minera_streaming_declare src/backend/tools/minera_streaming_declare.cpp)
target_link_libraries(minera_streaming_declare PRIVATE minera::core)
target_compile_features(minera_streaming_declare PRIVATE cxx_std_20)
add_executable(minera_reader_stream src/backend/tools/minera_reader_stream.cpp)
target_link_libraries(minera_reader_stream PRIVATE minera::core)
target_compile_features(minera_reader_stream PRIVATE cxx_std_20)

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

add_executable(minera_original_log_query src/backend/tools/minera_original_log_query.cpp)
target_link_libraries(minera_original_log_query PRIVATE minera::core)
target_compile_features(minera_original_log_query PRIVATE cxx_std_20)

find_package(Threads REQUIRED)

add_executable(minera_server
  src/backend/tools/minera_server.cpp
)
target_link_libraries(minera_server PRIVATE minera::core Threads::Threads)
target_include_directories(minera_server PRIVATE third_party)
target_compile_features(minera_server PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(minera_server PRIVATE /bigobj)
endif()
add_executable(minera_acquire_source src/backend/tools/minera_acquire_source.cpp)
target_link_libraries(minera_acquire_source PRIVATE minera::core Threads::Threads)
target_include_directories(minera_acquire_source PRIVATE third_party src/backend/tools)
target_compile_features(minera_acquire_source PRIVATE cxx_std_20)
if(WIN32)
  target_link_libraries(minera_core PUBLIC wevtapi)
  target_link_libraries(minera_core PUBLIC odbc32)
  target_link_libraries(minera_core PUBLIC ole32 oleaut32)
  target_link_libraries(minera_server PRIVATE winhttp)
  target_link_libraries(minera_export PRIVATE winhttp)
  target_link_libraries(minera_acquire_source PRIVATE winhttp)
else()
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    target_compile_definitions(minera_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(minera_server PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(minera_acquire_source PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(minera_acquire_source PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()

if(MINERA_BUILD_TESTS)
  enable_testing()

  # The tests are assert()-based; Release's -DNDEBUG would compile every
  # check away and leave the suite vacuously green. Re-enable assert for the
  # test translation units only.
  set(MINERA_TEST_OPTIONS $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
                          $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

  add_executable(minera_root_cause_tests src/backend/tests/test_root_cause.cpp)
  target_link_libraries(minera_root_cause_tests PRIVATE minera::core)
  target_compile_features(minera_root_cause_tests PRIVATE cxx_std_20)
  target_compile_options(minera_root_cause_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_root_cause_tests COMMAND minera_root_cause_tests)

  add_executable(minera_token_replay_root_cause_tests src/backend/tests/test_token_replay_root_cause.cpp)
  target_link_libraries(minera_token_replay_root_cause_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_root_cause_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_root_cause_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_root_cause_tests COMMAND minera_token_replay_root_cause_tests)

  add_executable(minera_core_tests
    src/backend/tests/test_core.cpp
  )
  target_link_libraries(minera_core_tests PRIVATE minera::core)
  target_compile_features(minera_core_tests PRIVATE cxx_std_20)
  target_compile_options(minera_core_tests PRIVATE ${MINERA_TEST_OPTIONS})

  add_test(NAME minera_core_tests COMMAND minera_core_tests)

  add_executable(minera_weighted_token_replay_tests src/backend/tests/test_weighted_token_replay.cpp)
  target_link_libraries(minera_weighted_token_replay_tests PRIVATE minera::core)
  target_compile_features(minera_weighted_token_replay_tests PRIVATE cxx_std_20)
  target_compile_options(minera_weighted_token_replay_tests PRIVATE ${MINERA_TEST_OPTIONS}
                         $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
  add_test(NAME minera_weighted_token_replay_tests COMMAND minera_weighted_token_replay_tests)

  add_executable(minera_token_replay_result_tests src/backend/tests/test_token_replay_result.cpp)
  target_link_libraries(minera_token_replay_result_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_result_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_result_tests PRIVATE ${MINERA_TEST_OPTIONS}
                         $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
  add_test(NAME minera_token_replay_result_tests COMMAND minera_token_replay_result_tests)

  add_executable(minera_token_replay_diagnostics_tests
    src/backend/tests/test_token_replay_diagnostics.cpp
  )
  target_link_libraries(minera_token_replay_diagnostics_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_diagnostics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_diagnostics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_diagnostics_tests COMMAND minera_token_replay_diagnostics_tests)

  add_executable(minera_token_replay_transport_tests
    src/backend/tests/test_token_replay_transport.cpp
  )
  target_link_libraries(minera_token_replay_transport_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_transport_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_transport_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_transport_tests COMMAND minera_token_replay_transport_tests)

  add_executable(minera_token_replay_duration_tests
    src/backend/tests/test_token_replay_duration.cpp
  )
  target_link_libraries(minera_token_replay_duration_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_duration_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_duration_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_duration_tests COMMAND minera_token_replay_duration_tests)

  add_executable(minera_token_replay_event_data_tests
    src/backend/tests/test_token_replay_event_data.cpp
  )
  target_link_libraries(minera_token_replay_event_data_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_event_data_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_event_data_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_event_data_tests COMMAND minera_token_replay_event_data_tests)

  add_executable(minera_alpha_miner_tests
    src/backend/tests/test_alpha_miner.cpp
  )
  target_link_libraries(minera_alpha_miner_tests PRIVATE minera::core)
  target_compile_features(minera_alpha_miner_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alpha_miner_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alpha_miner_tests COMMAND minera_alpha_miner_tests)

  add_executable(minera_alpha_event_data_tests
    src/backend/tests/test_alpha_event_data.cpp
  )
  target_link_libraries(minera_alpha_event_data_tests PRIVATE minera::core)
  target_compile_features(minera_alpha_event_data_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alpha_event_data_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alpha_event_data_tests COMMAND minera_alpha_event_data_tests)

  add_executable(minera_alpha_parity_contracts_tests
    src/backend/tests/test_alpha_parity_contracts.cpp
  )
  target_link_libraries(minera_alpha_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_alpha_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alpha_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alpha_parity_contracts_tests COMMAND minera_alpha_parity_contracts_tests)

  add_executable(minera_attribute_repetition_parity_contracts_tests
    src/backend/tests/test_attribute_repetition_parity_contracts.cpp
  )
  target_link_libraries(minera_attribute_repetition_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_attribute_repetition_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_attribute_repetition_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_attribute_repetition_parity_contracts_tests COMMAND minera_attribute_repetition_parity_contracts_tests)

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

  add_executable(minera_conformance_parity_contracts_tests
    src/backend/tests/test_conformance_parity_contracts.cpp
  )
  target_link_libraries(minera_conformance_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_conformance_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_conformance_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_conformance_parity_contracts_tests
           COMMAND minera_conformance_parity_contracts_tests)

  add_executable(minera_process_tree_alignment_parity_contracts_tests
    src/backend/tests/test_process_tree_alignment_parity_contracts.cpp
  )
  target_link_libraries(minera_process_tree_alignment_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_alignment_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_alignment_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_alignment_parity_contracts_tests
           COMMAND minera_process_tree_alignment_parity_contracts_tests)

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

  add_executable(minera_native_na_tests
    src/backend/tests/test_native_na.cpp
  )
  target_link_libraries(minera_native_na_tests PRIVATE minera::core)
  target_compile_features(minera_native_na_tests PRIVATE cxx_std_20)
  target_compile_options(minera_native_na_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_native_na_tests COMMAND minera_native_na_tests)

  add_executable(minera_token_replay_dataframe_diagnostics_tests
    src/backend/tests/test_token_replay_dataframe_diagnostics.cpp
  )
  target_link_libraries(minera_token_replay_dataframe_diagnostics_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_dataframe_diagnostics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_dataframe_diagnostics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_dataframe_diagnostics_tests COMMAND minera_token_replay_dataframe_diagnostics_tests)

  add_executable(minera_dataframe_validation_tests
    src/backend/tests/test_dataframe_validation.cpp
  )
  target_link_libraries(minera_dataframe_validation_tests PRIVATE minera::core)
  target_compile_features(minera_dataframe_validation_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dataframe_validation_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dataframe_validation_tests COMMAND minera_dataframe_validation_tests)

  add_executable(minera_dataframe_format_tests
    src/backend/tests/test_dataframe_format.cpp
  )
  target_link_libraries(minera_dataframe_format_tests PRIVATE minera::core)
  target_compile_features(minera_dataframe_format_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dataframe_format_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dataframe_format_tests COMMAND minera_dataframe_format_tests)

  add_executable(minera_dataframe_format_json_tests src/backend/tests/test_dataframe_format_json.cpp)
  target_link_libraries(minera_dataframe_format_json_tests PRIVATE minera::core)
  target_compile_features(minera_dataframe_format_json_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dataframe_format_json_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dataframe_format_json_tests COMMAND minera_dataframe_format_json_tests)

  foreach(workflow_input_test original_log_query_workbench_json exact_log_query_json original_label_splitting_workbench_json original_features_workbench_json exact_filter_discovery_json tree_reduction_workbench_json knowledge_exact_json otg_threshold_json saved_dfg_projection_json replay_duration_workbench_json dfg_graph_input workflow_input_alpha_result workflow_discovery_input_json workflow_input_result_json graph_alignment_workbench_json inductive_input_json process_tree_alignment_workbench_json petri_anti_multi_workbench_json model_conversion_workbench_json active_event_source monte_carlo_workbench_json playout_workbench_json dfg_playout_input_json decision_workbench_json declare_workbench_json log_skeleton_workbench_json footprints_workbench_json genetic_matrix_workbench_json embedding_workbench_json emd_workbench_json object_centric_playout_workbench_json literal_controls_json edit_distance_workbench_json bose_workbench_json discovery_workbench_json petri_analysis_workbench_json root_cause_workbench_json anonymization_workbench_json quality_workbench_json clustering_workbench_json organizational_workbench_json temporal_workbench_json timing_workbench_json time_analysis_workbench_json structural_inputs_json)
    add_executable(minera_${workflow_input_test}_tests src/backend/tests/test_${workflow_input_test}.cpp)
    target_link_libraries(minera_${workflow_input_test}_tests PRIVATE minera::core)
    target_compile_features(minera_${workflow_input_test}_tests PRIVATE cxx_std_20)
    target_compile_options(minera_${workflow_input_test}_tests PRIVATE ${MINERA_TEST_OPTIONS})
    add_test(NAME minera_${workflow_input_test}_tests COMMAND minera_${workflow_input_test}_tests)
  endforeach()

  add_executable(minera_original_model_context_tests
    src/backend/tests/test_original_model_context.cpp
  )
  target_link_libraries(minera_original_model_context_tests PRIVATE minera::core)
  target_compile_features(minera_original_model_context_tests PRIVATE cxx_std_20)
  target_compile_options(minera_original_model_context_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_original_model_context_tests COMMAND minera_original_model_context_tests)
  target_include_directories(minera_knowledge_exact_json_tests PRIVATE third_party)

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

  add_executable(minera_ilp_discovery_parity_tests
    src/backend/tests/test_ilp_discovery_parity.cpp
  )
  target_link_libraries(minera_ilp_discovery_parity_tests PRIVATE minera::core)
  target_compile_features(minera_ilp_discovery_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ilp_discovery_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ilp_discovery_parity_tests COMMAND minera_ilp_discovery_parity_tests)

  add_executable(minera_genetic_discovery_parity_tests
    src/backend/tests/test_genetic_discovery_parity.cpp
  )
  target_link_libraries(minera_genetic_discovery_parity_tests PRIVATE minera::core)
  target_compile_features(minera_genetic_discovery_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_genetic_discovery_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_genetic_discovery_parity_tests COMMAND minera_genetic_discovery_parity_tests)

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

  add_executable(minera_filtering_boundary_common_tests
    src/backend/tests/test_filtering_boundary_common.cpp
  )
  target_link_libraries(minera_filtering_boundary_common_tests PRIVATE minera::core)
  target_compile_features(minera_filtering_boundary_common_tests PRIVATE cxx_std_20)
  target_compile_options(minera_filtering_boundary_common_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_filtering_boundary_common_tests
           COMMAND minera_filtering_boundary_common_tests)

  add_executable(minera_infix_to_regex_tests src/backend/tests/test_infix_to_regex.cpp)
  target_link_libraries(minera_infix_to_regex_tests PRIVATE minera::core)
  target_compile_features(minera_infix_to_regex_tests PRIVATE cxx_std_20)
  target_compile_options(minera_infix_to_regex_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_infix_to_regex_tests COMMAND minera_infix_to_regex_tests)

  add_executable(minera_log_trace_filter_tests src/backend/tests/test_log_trace_filter.cpp)
  target_link_libraries(minera_log_trace_filter_tests PRIVATE minera::core)
  target_compile_features(minera_log_trace_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_trace_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_trace_filter_tests COMMAND minera_log_trace_filter_tests)

  add_executable(minera_log_segments_filter_tests src/backend/tests/test_log_segments_filter.cpp)
  target_link_libraries(minera_log_segments_filter_tests PRIVATE minera::core)
  target_compile_features(minera_log_segments_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_segments_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_segments_filter_tests COMMAND minera_log_segments_filter_tests)

  add_executable(minera_pripel_trace_distance_tests src/backend/tests/test_pripel_trace_distance.cpp)
  target_link_libraries(minera_pripel_trace_distance_tests PRIVATE minera::core)
  target_compile_features(minera_pripel_trace_distance_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pripel_trace_distance_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pripel_trace_distance_tests COMMAND minera_pripel_trace_distance_tests)

  add_executable(minera_log_boundary_filter_tests src/backend/tests/test_log_boundary_filter.cpp)
  target_link_libraries(minera_log_boundary_filter_tests PRIVATE minera::core)
  target_compile_features(minera_log_boundary_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_boundary_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_boundary_filter_tests COMMAND minera_log_boundary_filter_tests)

  add_executable(minera_polars_attribute_repetition_filter_tests src/backend/tests/test_polars_attribute_repetition_filter.cpp)
  target_link_libraries(minera_polars_attribute_repetition_filter_tests PRIVATE minera::core)
  target_compile_features(minera_polars_attribute_repetition_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_polars_attribute_repetition_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_polars_attribute_repetition_filter_tests COMMAND minera_polars_attribute_repetition_filter_tests)

  add_executable(minera_pandas_attribute_repetition_module_tests src/backend/tests/test_pandas_attribute_repetition_module.cpp)
  target_link_libraries(minera_pandas_attribute_repetition_module_tests PRIVATE minera::core)
  target_compile_features(minera_pandas_attribute_repetition_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pandas_attribute_repetition_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pandas_attribute_repetition_module_tests COMMAND minera_pandas_attribute_repetition_module_tests)

  add_executable(minera_pandas_between_filter_tests src/backend/tests/test_pandas_between_filter.cpp)
  target_link_libraries(minera_pandas_between_filter_tests PRIVATE minera::core)
  target_compile_features(minera_pandas_between_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pandas_between_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  if(MSVC)
    target_compile_options(minera_pandas_between_filter_tests PRIVATE /bigobj)
  endif()
  add_test(NAME minera_pandas_between_filter_tests COMMAND minera_pandas_between_filter_tests)

  add_executable(minera_pandas_between_ordinary_tests src/backend/tests/test_pandas_between_ordinary.cpp)
  target_link_libraries(minera_pandas_between_ordinary_tests PRIVATE minera::core)
  target_compile_features(minera_pandas_between_ordinary_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pandas_between_ordinary_tests PRIVATE ${MINERA_TEST_OPTIONS})
  if(MSVC)
    target_compile_options(minera_pandas_between_ordinary_tests PRIVATE /bigobj)
  endif()
  add_test(NAME minera_pandas_between_ordinary_tests COMMAND minera_pandas_between_ordinary_tests)

  add_executable(minera_polars_between_filter_tests src/backend/tests/test_polars_between_filter.cpp)
  target_link_libraries(minera_polars_between_filter_tests PRIVATE minera::core)
  target_compile_features(minera_polars_between_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_polars_between_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  if(MSVC)
    target_compile_options(minera_polars_between_filter_tests PRIVATE /bigobj)
  endif()
  add_test(NAME minera_polars_between_filter_tests COMMAND minera_polars_between_filter_tests)

  add_executable(minera_polars_categorical_table_metadata_tests src/backend/tests/test_polars_categorical_table_metadata.cpp)
  target_link_libraries(minera_polars_categorical_table_metadata_tests PRIVATE minera::core)
  target_compile_features(minera_polars_categorical_table_metadata_tests PRIVATE cxx_std_20)
  target_compile_options(minera_polars_categorical_table_metadata_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_polars_categorical_table_metadata_tests COMMAND minera_polars_categorical_table_metadata_tests)

  foreach(unit log_ltl_filter pandas_ltl_filter polars_ltl_filter log_ltl_workbench_public pandas_ltl_workbench_public ltl_filter_workbench ltl_filter_route log_attributes_filter pandas_attributes_filter polars_attributes_filter log_attributes_filter_workbench log_attributes_filter_route log_attributes_workbench_public table_attributes_filter_workbench table_attributes_filter_route pandas_attributes_workbench_public pandas_attributes_original_call_controls polars_attributes_original_call_controls polars_attributes_workbench_public log_variants_filter log_variants_filter_workbench log_variants_filter_route log_variants_original_call_controls log_variants_workbench_public pandas_variants_filter pandas_variants_original_call_controls pandas_variants_original_call_workbench pandas_variants_original_call_route pandas_variants_workbench_public polars_variants_filter polars_variants_original_call_controls polars_variants_original_call_workbench polars_variants_original_call_route polars_variants_workbench_public table_variants_filter_workbench table_variants_filter_route log_timestamp_filter log_timestamp_original_call_controls log_timestamp_filter_workbench log_timestamp_filter_route log_timestamp_workbench_public pandas_timestamp_filter pandas_timestamp_original_call_controls pandas_timestamp_workbench_public polars_timestamp_filter polars_timestamp_original_call_controls polars_timestamp_workbench_public table_timestamp_filter_workbench table_timestamp_filter_route log_paths_filter pandas_paths_filter polars_paths_filter table_paths_filter_workbench table_paths_filter_route log_case_filter pandas_case_filter polars_case_filter table_case_filter_workbench table_case_filter_route)
    add_executable(minera_${unit}_tests src/backend/tests/test_${unit}.cpp)
    target_link_libraries(minera_${unit}_tests PRIVATE minera::core Threads::Threads)
    target_include_directories(minera_${unit}_tests PRIVATE third_party)
    target_compile_features(minera_${unit}_tests PRIVATE cxx_std_20)
    target_compile_options(minera_${unit}_tests PRIVATE ${MINERA_TEST_OPTIONS})
    if(MSVC)
      target_compile_options(minera_${unit}_tests PRIVATE /bigobj)
    endif()
    if(MSVC AND unit MATCHES "^((log|pandas|polars)_ltl_filter|(log|pandas)_ltl_workbench_public|(log|pandas|polars)_attributes_workbench_public|(log|pandas|polars)_variants_workbench_public|(log|pandas|polars)_timestamp_workbench_public)$")
      target_compile_options(minera_${unit}_tests PRIVATE /Od)
    endif()
    add_test(NAME minera_${unit}_tests COMMAND minera_${unit}_tests)
  endforeach()

  add_executable(minera_table_between_filter_workbench_tests src/backend/tests/test_table_between_filter_workbench.cpp)
  target_link_libraries(minera_table_between_filter_workbench_tests PRIVATE minera::core)
  target_compile_features(minera_table_between_filter_workbench_tests PRIVATE cxx_std_20)
  target_compile_options(minera_table_between_filter_workbench_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_table_between_filter_workbench_tests COMMAND minera_table_between_filter_workbench_tests)

  add_executable(minera_table_between_filter_route_tests src/backend/tests/test_table_between_filter_route.cpp)
  target_link_libraries(minera_table_between_filter_route_tests PRIVATE minera::core Threads::Threads)
  target_include_directories(minera_table_between_filter_route_tests PRIVATE third_party)
  target_compile_features(minera_table_between_filter_route_tests PRIVATE cxx_std_20)
  target_compile_options(minera_table_between_filter_route_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_table_between_filter_route_tests COMMAND minera_table_between_filter_route_tests)

  add_executable(minera_log_between_filter_tests src/backend/tests/test_log_between_filter.cpp)
  target_link_libraries(minera_log_between_filter_tests PRIVATE minera::core)
  target_compile_features(minera_log_between_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_between_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  if(MSVC)
    target_compile_options(minera_log_between_filter_tests PRIVATE /bigobj)
  endif()
  add_test(NAME minera_log_between_filter_tests COMMAND minera_log_between_filter_tests)

  add_executable(minera_log_attribute_repetition_ordinary_tests src/backend/tests/test_log_attribute_repetition_ordinary.cpp)
  target_link_libraries(minera_log_attribute_repetition_ordinary_tests PRIVATE minera::core)
  target_compile_features(minera_log_attribute_repetition_ordinary_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_attribute_repetition_ordinary_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_attribute_repetition_ordinary_tests COMMAND minera_log_attribute_repetition_ordinary_tests)

  add_executable(minera_pandas_attribute_repetition_literal_errors_tests src/backend/tests/test_pandas_attribute_repetition_literal_errors.cpp)
  target_link_libraries(minera_pandas_attribute_repetition_literal_errors_tests PRIVATE minera::core)
  target_compile_features(minera_pandas_attribute_repetition_literal_errors_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pandas_attribute_repetition_literal_errors_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pandas_attribute_repetition_literal_errors_tests COMMAND minera_pandas_attribute_repetition_literal_errors_tests)

  add_executable(minera_log_attribute_repetition_module_tests src/backend/tests/test_log_attribute_repetition_module.cpp)
  target_link_libraries(minera_log_attribute_repetition_module_tests PRIVATE minera::core)
  target_compile_features(minera_log_attribute_repetition_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_attribute_repetition_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_attribute_repetition_module_tests COMMAND minera_log_attribute_repetition_module_tests)

  add_executable(minera_log_rework_filter_tests src/backend/tests/test_log_rework_filter.cpp)
  target_link_libraries(minera_log_rework_filter_tests PRIVATE minera::core)
  target_compile_features(minera_log_rework_filter_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_rework_filter_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_rework_filter_tests COMMAND minera_log_rework_filter_tests)

  add_executable(minera_case_description_tests src/backend/tests/test_case_description.cpp)
  target_link_libraries(minera_case_description_tests PRIVATE minera::core)
  target_compile_features(minera_case_description_tests PRIVATE cxx_std_20)
  target_compile_options(minera_case_description_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_case_description_tests COMMAND minera_case_description_tests)

  add_executable(minera_table_attribute_repetition_workbench_tests src/backend/tests/test_table_attribute_repetition_workbench.cpp)
  target_link_libraries(minera_table_attribute_repetition_workbench_tests PRIVATE minera::core)
  target_compile_features(minera_table_attribute_repetition_workbench_tests PRIVATE cxx_std_20)
  target_compile_options(minera_table_attribute_repetition_workbench_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_table_attribute_repetition_workbench_tests COMMAND minera_table_attribute_repetition_workbench_tests)

  add_executable(minera_table_attribute_repetition_route_tests src/backend/tests/test_table_attribute_repetition_route.cpp)
  target_link_libraries(minera_table_attribute_repetition_route_tests PRIVATE minera::core Threads::Threads)
  target_include_directories(minera_table_attribute_repetition_route_tests PRIVATE third_party)
  target_compile_features(minera_table_attribute_repetition_route_tests PRIVATE cxx_std_20)
  target_compile_options(minera_table_attribute_repetition_route_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_table_attribute_repetition_route_tests COMMAND minera_table_attribute_repetition_route_tests)

  add_executable(minera_reference_log_filter_workbench_tests src/backend/tests/test_reference_log_filter_workbench.cpp)
  target_link_libraries(minera_reference_log_filter_workbench_tests PRIVATE minera::core)
  target_compile_features(minera_reference_log_filter_workbench_tests PRIVATE cxx_std_20)
  target_compile_options(minera_reference_log_filter_workbench_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_reference_log_filter_workbench_tests COMMAND minera_reference_log_filter_workbench_tests)

  add_executable(minera_reference_log_filter_route_tests src/backend/tests/test_reference_log_filter_route.cpp)
  target_link_libraries(minera_reference_log_filter_route_tests PRIVATE minera::core Threads::Threads)
  target_include_directories(minera_reference_log_filter_route_tests PRIVATE third_party)
  target_compile_features(minera_reference_log_filter_route_tests PRIVATE cxx_std_20)
  target_compile_options(minera_reference_log_filter_route_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_reference_log_filter_route_tests COMMAND minera_reference_log_filter_route_tests)

  add_executable(minera_quality_json_numbers_tests src/backend/tests/test_quality_json_numbers.cpp)
  target_link_libraries(minera_quality_json_numbers_tests PRIVATE minera::core)
  target_compile_features(minera_quality_json_numbers_tests PRIVATE cxx_std_20)
  target_compile_options(minera_quality_json_numbers_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_quality_json_numbers_tests COMMAND minera_quality_json_numbers_tests)

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
  add_executable(minera_streaming_session_tests src/backend/tests/test_streaming_session.cpp)
  target_link_libraries(minera_streaming_session_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_session_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_session_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_session_tests COMMAND minera_streaming_session_tests)
  add_executable(minera_declare_streaming_io_tests src/backend/tests/test_declare_streaming_io.cpp)
  target_link_libraries(minera_declare_streaming_io_tests PRIVATE minera::core)
  target_compile_features(minera_declare_streaming_io_tests PRIVATE cxx_std_20)
  target_compile_options(minera_declare_streaming_io_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_declare_streaming_io_tests COMMAND minera_declare_streaming_io_tests)

  add_executable(minera_streaming_import_tests src/backend/tests/test_streaming_import.cpp)
  target_link_libraries(minera_streaming_import_tests PRIVATE minera::core)
  target_compile_features(minera_streaming_import_tests PRIVATE cxx_std_20)
  target_compile_options(minera_streaming_import_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_streaming_import_tests COMMAND minera_streaming_import_tests)
  add_executable(minera_reader_stream_session_tests src/backend/tests/test_reader_stream_session.cpp)
  target_link_libraries(minera_reader_stream_session_tests PRIVATE minera::core)
  target_compile_features(minera_reader_stream_session_tests PRIVATE cxx_std_20)
  target_compile_options(minera_reader_stream_session_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_reader_stream_session_tests COMMAND minera_reader_stream_session_tests)

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

  add_executable(minera_stochastic_fit_probe
    src/backend/tests/stochastic_fit_probe.cpp
  )
  target_link_libraries(minera_stochastic_fit_probe PRIVATE minera::core)
  target_compile_features(minera_stochastic_fit_probe PRIVATE cxx_std_20)

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

  add_executable(minera_correlation_mining_parity_contracts_tests
    src/backend/tests/correlation_mining_parity_contracts.cpp
  )
  target_link_libraries(minera_correlation_mining_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_correlation_mining_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_correlation_mining_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_correlation_mining_parity_contracts_tests
           COMMAND minera_correlation_mining_parity_contracts_tests)

  add_test(NAME minera_correlation_cli_projected_labels
           COMMAND minera_export
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/correlation_case_activity.csv"
                   --correlation-variants classic
                   --correlation-activity-attribute case:id)
  set_tests_properties(minera_correlation_cli_projected_labels PROPERTIES
    PASS_REGULAR_EXPRESSION "\"activity_counts\"")

  add_test(NAME minera_correlation_cli_custom_case
           COMMAND minera_export
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/correlation_custom_case.csv"
                   --correlation-variants trace_based
                   --correlation-case-id-attribute group)
  set_tests_properties(minera_correlation_cli_custom_case PROPERTIES
    PASS_REGULAR_EXPRESSION
      "\"precede_succeed_probability\".*\"source\": 0, \"target\": 1, \"value\": 1")

  add_test(NAME minera_correlation_cli_classic_split_source_order
           COMMAND minera_export
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/correlation_classic_split_source_order.csv"
                   --correlation-variants classic_split
                   --correlation-sample-size 2)
  set_tests_properties(minera_correlation_cli_classic_split_source_order PROPERTIES
    PASS_REGULAR_EXPRESSION
      "\"precede_succeed_probability\".*\"source\": 0, \"target\": 1, \"value\": 1")

  add_test(NAME minera_correlation_cli_blank_case
           COMMAND minera_export
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/correlation_blank_case.csv"
                   --correlation-variants trace_based
                   --correlation-case-id-attribute group)
  set_tests_properties(minera_correlation_cli_blank_case PROPERTIES
    PASS_REGULAR_EXPRESSION
      "\"precede_succeed_probability\".*\"source\": 0, \"target\": 1, \"value\": 1")

  add_test(NAME minera_dfg_causal_cli_custom_activity
           COMMAND minera_export
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/dfg_causal_custom_activity.csv"
                   --dfg-causal-variants alpha
                   --dfg-activity-attribute causal_label)
  set_tests_properties(minera_dfg_causal_cli_custom_activity PROPERTIES
    PASS_REGULAR_EXPRESSION "\"name\": \"begin\"")

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

  add_executable(minera_bose_parity_contract_tests
    src/backend/tests/test_bose_parity_contracts.cpp
  )
  target_link_libraries(minera_bose_parity_contract_tests PRIVATE minera::core)
  target_compile_features(minera_bose_parity_contract_tests PRIVATE cxx_std_20)
  target_compile_options(minera_bose_parity_contract_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_bose_parity_contract_tests COMMAND minera_bose_parity_contract_tests)

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
  add_executable(minera_source_acquisition_tests src/backend/tests/test_source_acquisition.cpp)
  target_link_libraries(minera_source_acquisition_tests PRIVATE minera::core)
  target_compile_features(minera_source_acquisition_tests PRIVATE cxx_std_20)
  target_compile_options(minera_source_acquisition_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_source_acquisition_tests COMMAND minera_source_acquisition_tests)
  add_executable(minera_windows_event_source_tests src/backend/tests/test_windows_event_source.cpp)
  target_link_libraries(minera_windows_event_source_tests PRIVATE minera::core)
  target_compile_features(minera_windows_event_source_tests PRIVATE cxx_std_20)
  target_compile_options(minera_windows_event_source_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_windows_event_source_tests COMMAND minera_windows_event_source_tests)
  add_executable(minera_database_history_source_tests src/backend/tests/test_database_history_source.cpp)
  target_link_libraries(minera_database_history_source_tests PRIVATE minera::core)
  target_compile_features(minera_database_history_source_tests PRIVATE cxx_std_20)
  target_compile_options(minera_database_history_source_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_database_history_source_tests COMMAND minera_database_history_source_tests)
  add_executable(minera_outlook_source_tests src/backend/tests/test_outlook_source.cpp)
  target_link_libraries(minera_outlook_source_tests PRIVATE minera::core)
  target_compile_features(minera_outlook_source_tests PRIVATE cxx_std_20)
  target_compile_options(minera_outlook_source_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_outlook_source_tests COMMAND minera_outlook_source_tests)
  if(WIN32)
    add_executable(minera_outlook_workspace_fixture src/backend/tests/outlook_workspace_fixture.cpp)
    target_link_libraries(minera_outlook_workspace_fixture PRIVATE minera::core $<TARGET_PROPERTY:minera_server,LINK_LIBRARIES>)
    target_include_directories(minera_outlook_workspace_fixture PRIVATE $<TARGET_PROPERTY:minera_server,INCLUDE_DIRECTORIES>)
    target_compile_definitions(minera_outlook_workspace_fixture PRIVATE $<TARGET_PROPERTY:minera_server,COMPILE_DEFINITIONS>)
    target_compile_features(minera_outlook_workspace_fixture PRIVATE cxx_std_20)
    target_compile_options(minera_outlook_workspace_fixture PRIVATE ${MINERA_TEST_OPTIONS} $<TARGET_PROPERTY:minera_server,COMPILE_OPTIONS> $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
  endif()

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

  # Line-oriented driver for the public/native DFG parity audit script.
  add_executable(minera_dfg_alignment_probe
    src/backend/tests/dfg_alignment_probe.cpp
  )
  target_link_libraries(minera_dfg_alignment_probe PRIVATE minera::core)
  target_compile_features(minera_dfg_alignment_probe PRIVATE cxx_std_20)

  add_executable(minera_alpha_result_json_tests src/backend/tests/test_alpha_result_json.cpp)
  target_link_libraries(minera_alpha_result_json_tests PRIVATE minera::core)
  target_compile_features(minera_alpha_result_json_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alpha_result_json_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alpha_result_json_tests COMMAND minera_alpha_result_json_tests)

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

  add_executable(minera_evaluation_parity_contracts_tests
    src/backend/tests/test_evaluation_parity_contracts.cpp
  )
  target_link_libraries(minera_evaluation_parity_contracts_tests PRIVATE minera::core)
  target_compile_features(minera_evaluation_parity_contracts_tests PRIVATE cxx_std_20)
  target_compile_options(minera_evaluation_parity_contracts_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_evaluation_parity_contracts_tests
           COMMAND minera_evaluation_parity_contracts_tests)

  add_executable(minera_evaluation_reference_equivalence_tests
    src/backend/tests/test_evaluation_reference_equivalence.cpp
  )
  target_link_libraries(minera_evaluation_reference_equivalence_tests PRIVATE minera::core)
  target_compile_features(minera_evaluation_reference_equivalence_tests PRIVATE cxx_std_20)
  target_compile_options(minera_evaluation_reference_equivalence_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_evaluation_reference_equivalence_tests
           COMMAND minera_evaluation_reference_equivalence_tests)

  add_executable(minera_token_replay_arc_order_parity_tests
    src/backend/tests/test_token_replay_arc_order_parity.cpp
  )
  target_link_libraries(minera_token_replay_arc_order_parity_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_arc_order_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_arc_order_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_arc_order_parity_tests
           COMMAND minera_token_replay_arc_order_parity_tests)

  add_executable(minera_dfg_alignment_dispatcher_parity_tests
    src/backend/tests/test_dfg_alignment_dispatcher_parity.cpp
  )
  target_link_libraries(minera_dfg_alignment_dispatcher_parity_tests PRIVATE minera::core)
  target_compile_features(minera_dfg_alignment_dispatcher_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_dfg_alignment_dispatcher_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_dfg_alignment_dispatcher_parity_tests
           COMMAND minera_dfg_alignment_dispatcher_parity_tests)

  add_executable(minera_alignment_log_enrichment_parity_tests
    src/backend/tests/test_alignment_log_enrichment_parity.cpp
  )
  target_link_libraries(minera_alignment_log_enrichment_parity_tests PRIVATE minera::core)
  target_compile_features(minera_alignment_log_enrichment_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_alignment_log_enrichment_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_alignment_log_enrichment_parity_tests
           COMMAND minera_alignment_log_enrichment_parity_tests)

  add_executable(minera_petri_alignment_dispatcher_parity_tests
    src/backend/tests/test_petri_alignment_dispatcher_parity.cpp
  )
  target_link_libraries(minera_petri_alignment_dispatcher_parity_tests PRIVATE minera::core)
  target_compile_features(minera_petri_alignment_dispatcher_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_petri_alignment_dispatcher_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_petri_alignment_dispatcher_parity_tests
           COMMAND minera_petri_alignment_dispatcher_parity_tests)

  add_executable(minera_token_replay_pltr_parity_tests
    src/backend/tests/test_token_replay_pltr_parity.cpp
  )
  target_link_libraries(minera_token_replay_pltr_parity_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_pltr_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_pltr_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_pltr_parity_tests
           COMMAND minera_token_replay_pltr_parity_tests)

  add_executable(minera_token_replay_root_cause_parity_tests
    src/backend/tests/test_token_replay_root_cause_parity.cpp
  )
  target_link_libraries(minera_token_replay_root_cause_parity_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_root_cause_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_root_cause_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_root_cause_parity_tests
           COMMAND minera_token_replay_root_cause_parity_tests)

  add_executable(minera_token_replay_dispatcher_parity_tests
    src/backend/tests/test_token_replay_dispatcher_parity.cpp
  )
  target_link_libraries(minera_token_replay_dispatcher_parity_tests PRIVATE minera::core)
  target_compile_features(minera_token_replay_dispatcher_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_token_replay_dispatcher_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_token_replay_dispatcher_parity_tests
           COMMAND minera_token_replay_dispatcher_parity_tests)

  add_executable(minera_heuristics_plusplus_parity_tests
    src/backend/tests/test_heuristics_plusplus_parity.cpp
  )
  target_link_libraries(minera_heuristics_plusplus_parity_tests PRIVATE minera::core)
  target_compile_features(minera_heuristics_plusplus_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_heuristics_plusplus_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_heuristics_plusplus_parity_tests
           COMMAND minera_heuristics_plusplus_parity_tests)

  add_executable(minera_ocel_graphs_parity_tests
    src/backend/tests/test_ocel_graphs_parity.cpp
  )
  target_link_libraries(minera_ocel_graphs_parity_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_graphs_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_graphs_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_graphs_parity_tests
           COMMAND minera_ocel_graphs_parity_tests)

  add_executable(minera_ocel_nets_parity_tests
    src/backend/tests/test_ocel_nets_parity.cpp
  )
  target_link_libraries(minera_ocel_nets_parity_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_nets_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_nets_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_nets_parity_tests
           COMMAND minera_ocel_nets_parity_tests)

  add_executable(minera_ocel_links_parity_tests
    src/backend/tests/test_ocel_links_parity.cpp
  )
  target_link_libraries(minera_ocel_links_parity_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_links_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_links_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_links_parity_tests
           COMMAND minera_ocel_links_parity_tests)

  add_executable(minera_extended_cardoso_module_tests
    src/backend/tests/test_extended_cardoso_module.cpp
  )
  target_link_libraries(minera_extended_cardoso_module_tests PRIVATE minera::core)
  target_compile_features(minera_extended_cardoso_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_extended_cardoso_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_extended_cardoso_module_tests
           COMMAND minera_extended_cardoso_module_tests)

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
  if(MSVC)
    # The generated public dtype matrix contains hundreds of independent cases.
    target_compile_options(minera_case_relations_tests PRIVATE /bigobj)
  endif()
  add_test(NAME minera_case_relations_tests COMMAND minera_case_relations_tests)

  add_executable(minera_case_relations_parity_tests
    src/backend/tests/test_case_relations_parity.cpp
  )
  target_link_libraries(minera_case_relations_parity_tests PRIVATE minera::core)
  target_compile_features(minera_case_relations_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_case_relations_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  if(MSVC)
    target_compile_options(minera_case_relations_parity_tests PRIVATE /bigobj)
  endif()
  add_test(NAME minera_case_relations_parity_tests COMMAND minera_case_relations_parity_tests)

  add_executable(minera_label_splitting_parity_tests
    src/backend/tests/test_label_splitting_parity.cpp
  )
  target_link_libraries(minera_label_splitting_parity_tests PRIVATE minera::core)
  target_compile_features(minera_label_splitting_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_label_splitting_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_label_splitting_parity_tests COMMAND minera_label_splitting_parity_tests)

  add_executable(minera_llm_injection_parity_tests
    src/backend/tests/test_llm_injection_parity.cpp
  )
  target_link_libraries(minera_llm_injection_parity_tests PRIVATE minera::core)
  target_compile_features(minera_llm_injection_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_injection_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_injection_parity_tests COMMAND minera_llm_injection_parity_tests)

  add_executable(minera_trace_attribute_clustering_parity_tests
    src/backend/tests/test_trace_attribute_clustering_parity.cpp
  )
  target_link_libraries(minera_trace_attribute_clustering_parity_tests PRIVATE minera::core)
  target_compile_features(minera_trace_attribute_clustering_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_trace_attribute_clustering_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_trace_attribute_clustering_parity_tests COMMAND minera_trace_attribute_clustering_parity_tests)

  add_executable(minera_decision_mining_parity_tests
    src/backend/tests/test_decision_mining_parity.cpp
  )
  target_link_libraries(minera_decision_mining_parity_tests PRIVATE minera::core)
  target_compile_features(minera_decision_mining_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_decision_mining_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_decision_mining_parity_tests COMMAND minera_decision_mining_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/decision_mining_bank.txt")

  add_executable(minera_log_to_features_parity_tests
    src/backend/tests/test_log_to_features_parity.cpp
  )
  target_link_libraries(minera_log_to_features_parity_tests PRIVATE minera::core)
  target_compile_features(minera_log_to_features_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_to_features_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_to_features_parity_tests COMMAND minera_log_to_features_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/log_to_features_bank.txt")

  add_executable(minera_profile_clustering_parity_tests
    src/backend/tests/test_profile_clustering_parity.cpp
  )
  target_link_libraries(minera_profile_clustering_parity_tests PRIVATE minera::core)
  target_compile_features(minera_profile_clustering_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_profile_clustering_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_profile_clustering_parity_tests COMMAND minera_profile_clustering_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/profile_clustering_bank.txt")

  add_executable(minera_log_filter_bank_parity_tests
    src/backend/tests/test_log_filter_bank_parity.cpp
  )
  target_link_libraries(minera_log_filter_bank_parity_tests PRIVATE minera::core)
  target_compile_features(minera_log_filter_bank_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_filter_bank_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_filter_bank_parity_tests COMMAND minera_log_filter_bank_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/log-filter-domain-bank.json.gz")
  set_tests_properties(minera_log_filter_bank_parity_tests PROPERTIES ENVIRONMENT "TZ=Europe/Berlin")

  add_executable(minera_llm_abstractions_parity_tests
    src/backend/tests/test_llm_abstractions_parity.cpp
  )
  target_link_libraries(minera_llm_abstractions_parity_tests PRIVATE minera::core)
  target_compile_features(minera_llm_abstractions_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_abstractions_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_abstractions_parity_tests COMMAND minera_llm_abstractions_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/llm_abstractions_bank.txt")

  add_executable(minera_llm_ocel_abstractions_parity_tests
    src/backend/tests/test_llm_ocel_abstractions_parity.cpp
  )
  target_link_libraries(minera_llm_ocel_abstractions_parity_tests PRIVATE minera::core)
  target_compile_features(minera_llm_ocel_abstractions_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_ocel_abstractions_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_ocel_abstractions_parity_tests COMMAND minera_llm_ocel_abstractions_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/llm_ocel_abstractions_bank.txt")

  add_executable(minera_filtering_common_bank_parity_tests
    src/backend/tests/test_filtering_common_bank_parity.cpp
  )
  target_link_libraries(minera_filtering_common_bank_parity_tests PRIVATE minera::core)
  target_compile_features(minera_filtering_common_bank_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_filtering_common_bank_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_filtering_common_bank_parity_tests COMMAND minera_filtering_common_bank_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/filtering-common-bank.json.gz")
  set_tests_properties(minera_filtering_common_bank_parity_tests PROPERTIES ENVIRONMENT "TZ=Europe/Berlin")

  add_executable(minera_monte_carlo_numpy_parity_tests
    src/backend/tests/test_monte_carlo_numpy_parity.cpp
  )
  target_link_libraries(minera_monte_carlo_numpy_parity_tests PRIVATE minera::core)
  target_compile_features(minera_monte_carlo_numpy_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_monte_carlo_numpy_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_monte_carlo_numpy_parity_tests COMMAND minera_monte_carlo_numpy_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/tests/fixtures/monte_carlo_numpy_bank.txt")

  add_executable(minera_pandas_sequence_filter_bank_parity_tests
    src/backend/tests/test_pandas_sequence_filter_bank_parity.cpp
  )
  target_link_libraries(minera_pandas_sequence_filter_bank_parity_tests PRIVATE minera::core)
  target_compile_features(minera_pandas_sequence_filter_bank_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pandas_sequence_filter_bank_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pandas_sequence_filter_bank_parity_tests COMMAND minera_pandas_sequence_filter_bank_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/pandas-sequence-filter-bank.json.gz")
  set_tests_properties(minera_pandas_sequence_filter_bank_parity_tests PROPERTIES ENVIRONMENT "TZ=Europe/Berlin")

  add_executable(minera_polars_sequence_filter_bank_parity_tests
    src/backend/tests/test_polars_sequence_filter_bank_parity.cpp
  )
  target_link_libraries(minera_polars_sequence_filter_bank_parity_tests PRIVATE minera::core)
  target_compile_features(minera_polars_sequence_filter_bank_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_polars_sequence_filter_bank_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_polars_sequence_filter_bank_parity_tests COMMAND minera_polars_sequence_filter_bank_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/polars-sequence-filter-bank.json.gz")
  set_tests_properties(minera_polars_sequence_filter_bank_parity_tests PROPERTIES ENVIRONMENT "TZ=Europe/Berlin")

  add_executable(minera_polars_filter_bank_parity_tests
    src/backend/tests/test_polars_filter_bank_parity.cpp
  )
  target_link_libraries(minera_polars_filter_bank_parity_tests PRIVATE minera::core)
  target_compile_features(minera_polars_filter_bank_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_polars_filter_bank_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_polars_filter_bank_parity_tests COMMAND minera_polars_filter_bank_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/polars-filter-bank.json.gz")
  set_tests_properties(minera_polars_filter_bank_parity_tests PROPERTIES ENVIRONMENT "TZ=Europe/Berlin")

  add_executable(minera_interleavings_merge_parity_tests
    src/backend/tests/test_interleavings_merge_parity.cpp
  )
  target_link_libraries(minera_interleavings_merge_parity_tests PRIVATE minera::core)
  target_compile_features(minera_interleavings_merge_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_interleavings_merge_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_interleavings_merge_parity_tests COMMAND minera_interleavings_merge_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/interleavings-merge-bank.txt.gz")

  add_executable(minera_tree_reduction_parity_tests
    src/backend/tests/test_tree_reduction_parity.cpp
  )
  target_link_libraries(minera_tree_reduction_parity_tests PRIVATE minera::core)
  target_compile_features(minera_tree_reduction_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_tree_reduction_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_tree_reduction_parity_tests COMMAND minera_tree_reduction_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/tree-reduction-bank.txt.gz")

  add_executable(minera_organizational_util_parity_tests
    src/backend/tests/test_organizational_util_parity.cpp
  )
  target_link_libraries(minera_organizational_util_parity_tests PRIVATE minera::core)
  target_compile_features(minera_organizational_util_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_organizational_util_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_organizational_util_parity_tests COMMAND minera_organizational_util_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/organizational-util-bank.txt.gz")

  add_executable(minera_precision_utils_parity_tests
    src/backend/tests/test_precision_utils_parity.cpp
  )
  target_link_libraries(minera_precision_utils_parity_tests PRIVATE minera::core)
  target_compile_features(minera_precision_utils_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_precision_utils_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_precision_utils_parity_tests COMMAND minera_precision_utils_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/precision-utils-bank.txt.gz")

  add_executable(minera_element_usage_comparison_parity_tests
    src/backend/tests/test_element_usage_comparison_parity.cpp
  )
  target_link_libraries(minera_element_usage_comparison_parity_tests PRIVATE minera::core)
  target_compile_features(minera_element_usage_comparison_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_element_usage_comparison_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_element_usage_comparison_parity_tests COMMAND minera_element_usage_comparison_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/element-usage-bank.txt.gz")

  add_executable(minera_ocpn_playout_parity_tests
    src/backend/tests/test_ocpn_playout_parity.cpp
  )
  target_link_libraries(minera_ocpn_playout_parity_tests PRIVATE minera::core)
  target_compile_features(minera_ocpn_playout_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocpn_playout_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocpn_playout_parity_tests COMMAND minera_ocpn_playout_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/ocpn-playout-bank.txt.gz")

  add_executable(minera_oc_causal_net_playout_parity_tests
    src/backend/tests/test_oc_causal_net_playout_parity.cpp
  )
  target_link_libraries(minera_oc_causal_net_playout_parity_tests PRIVATE minera::core)
  target_compile_features(minera_oc_causal_net_playout_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_oc_causal_net_playout_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_oc_causal_net_playout_parity_tests COMMAND minera_oc_causal_net_playout_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/oc-causal-net-playout-bank.txt.gz")

  add_executable(minera_process_tree_reference_alignment_parity_tests
    src/backend/tests/test_process_tree_reference_alignment_parity.cpp
  )
  target_link_libraries(minera_process_tree_reference_alignment_parity_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_reference_alignment_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_reference_alignment_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_reference_alignment_parity_tests
           COMMAND minera_process_tree_reference_alignment_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/process-tree-dp-alignment-bank.txt.gz")

  add_executable(minera_process_tree_search_graph_parity_tests
    src/backend/tests/test_process_tree_search_graph_parity.cpp
  )
  target_link_libraries(minera_process_tree_search_graph_parity_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_search_graph_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_search_graph_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_search_graph_parity_tests
           COMMAND minera_process_tree_search_graph_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/process-tree-search-graph-bank.txt.gz")

  add_executable(minera_process_tree_milp_parity_tests
    src/backend/tests/test_process_tree_milp_parity.cpp
  )
  target_link_libraries(minera_process_tree_milp_parity_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_milp_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_milp_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_milp_parity_tests
           COMMAND minera_process_tree_milp_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/process-tree-milp-bank.txt.gz")

  add_executable(minera_process_tree_approximated_parity_tests
    src/backend/tests/test_process_tree_approximated_parity.cpp
  )
  target_link_libraries(minera_process_tree_approximated_parity_tests PRIVATE minera::core)
  target_compile_features(minera_process_tree_approximated_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_process_tree_approximated_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_process_tree_approximated_parity_tests
           COMMAND minera_process_tree_approximated_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/process-tree-approximated-bank.txt.gz")

  add_executable(minera_decomposed_parity_tests
    src/backend/tests/test_decomposed_parity.cpp
  )
  target_link_libraries(minera_decomposed_parity_tests PRIVATE minera::core)
  target_compile_features(minera_decomposed_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_decomposed_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_decomposed_parity_tests
           COMMAND minera_decomposed_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/decomposed-bank.txt.gz")

  if(SQLite3_FOUND)
    add_executable(minera_connector_parity_tests
      src/backend/tests/test_connector_parity.cpp
    )
    target_link_libraries(minera_connector_parity_tests PRIVATE minera::core SQLite::SQLite3)
    target_compile_features(minera_connector_parity_tests PRIVATE cxx_std_20)
    target_compile_options(minera_connector_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
    add_test(NAME minera_connector_parity_tests
             COMMAND minera_connector_parity_tests
             "${CMAKE_CURRENT_SOURCE_DIR}/docs/connector-bank.txt.gz")
  endif()

  add_executable(minera_llm_connector_parity_tests
    src/backend/tests/test_llm_connector_parity.cpp
  )
  target_link_libraries(minera_llm_connector_parity_tests PRIVATE minera::core)
  target_compile_features(minera_llm_connector_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_connector_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_connector_parity_tests
           COMMAND minera_llm_connector_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/llm-connector-bank.txt.gz")

  add_executable(minera_embedding_parity_tests
    src/backend/tests/test_embedding_parity.cpp
  )
  target_link_libraries(minera_embedding_parity_tests PRIVATE minera::core)
  target_compile_features(minera_embedding_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_embedding_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_embedding_parity_tests
           COMMAND minera_embedding_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/embedding-bank.txt.gz")
  add_executable(minera_temporal_lazy_parity_tests
    src/backend/tests/test_temporal_lazy_parity.cpp
  )
  target_link_libraries(minera_temporal_lazy_parity_tests PRIVATE minera::core)
  target_compile_features(minera_temporal_lazy_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_temporal_lazy_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_temporal_lazy_parity_tests
           COMMAND minera_temporal_lazy_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/temporal-lazy-bank.txt.gz")
  add_executable(minera_lle_parity_tests
    src/backend/tests/test_lle_parity.cpp
  )
  target_link_libraries(minera_lle_parity_tests PRIVATE minera::core)
  target_compile_features(minera_lle_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_lle_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_lle_parity_tests
           COMMAND minera_lle_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/lle-bank.txt.gz")

  add_executable(minera_pandas_filter_bank_parity_tests
    src/backend/tests/test_pandas_filter_bank_parity.cpp
  )
  target_link_libraries(minera_pandas_filter_bank_parity_tests PRIVATE minera::core)
  target_compile_features(minera_pandas_filter_bank_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pandas_filter_bank_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pandas_filter_bank_parity_tests COMMAND minera_pandas_filter_bank_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/pandas-filter-bank.json.gz")
  set_tests_properties(minera_pandas_filter_bank_parity_tests PROPERTIES ENVIRONMENT "TZ=Europe/Berlin")

  add_executable(minera_llm_sql_utils_parity_tests
    src/backend/tests/test_llm_sql_utils_parity.cpp
  )
  target_link_libraries(minera_llm_sql_utils_parity_tests PRIVATE minera::core)
  target_compile_features(minera_llm_sql_utils_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_sql_utils_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_sql_utils_parity_tests COMMAND minera_llm_sql_utils_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/llm-sql-utils-bank.txt.gz")

  add_executable(minera_trace_variant_query_parity_tests
    src/backend/tests/test_trace_variant_query_parity.cpp
  )
  target_link_libraries(minera_trace_variant_query_parity_tests PRIVATE minera::core)
  target_compile_features(minera_trace_variant_query_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_trace_variant_query_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_trace_variant_query_parity_tests COMMAND minera_trace_variant_query_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/trace-variant-query-bank.txt.gz")

  add_executable(minera_pripel_reference_parity_tests
    src/backend/tests/test_pripel_reference_parity.cpp
  )
  target_link_libraries(minera_pripel_reference_parity_tests PRIVATE minera::core)
  target_compile_features(minera_pripel_reference_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_pripel_reference_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_pripel_reference_parity_tests COMMAND minera_pripel_reference_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/pripel-reference-bank.txt")

  add_executable(minera_trace_attribute_helpers_parity_tests
    src/backend/tests/test_trace_attribute_helpers_parity.cpp
  )
  target_link_libraries(minera_trace_attribute_helpers_parity_tests PRIVATE minera::core)
  target_compile_features(minera_trace_attribute_helpers_parity_tests PRIVATE cxx_std_20)
  target_compile_options(minera_trace_attribute_helpers_parity_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_trace_attribute_helpers_parity_tests COMMAND minera_trace_attribute_helpers_parity_tests
           "${CMAKE_CURRENT_SOURCE_DIR}/docs/trace-attribute-helpers-bank.txt.gz")

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

  add_executable(minera_ocel_interaction_module_tests
    src/backend/tests/test_ocel_interaction_module.cpp
  )
  target_link_libraries(minera_ocel_interaction_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_interaction_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_interaction_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_interaction_module_tests
           COMMAND minera_ocel_interaction_module_tests)

  add_executable(minera_ocel_cobirth_module_tests
    src/backend/tests/test_ocel_cobirth_module.cpp
  )
  target_link_libraries(minera_ocel_cobirth_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_cobirth_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_cobirth_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_cobirth_module_tests
           COMMAND minera_ocel_cobirth_module_tests)

  add_executable(minera_ocel_codeath_module_tests src/backend/tests/test_ocel_codeath_module.cpp)
  target_link_libraries(minera_ocel_codeath_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_codeath_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_codeath_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_codeath_module_tests COMMAND minera_ocel_codeath_module_tests)

  add_executable(minera_ocel_descendants_module_tests src/backend/tests/test_ocel_descendants_module.cpp)
  target_link_libraries(minera_ocel_descendants_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_descendants_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_descendants_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_descendants_module_tests COMMAND minera_ocel_descendants_module_tests)

  add_executable(minera_ocel_inheritance_module_tests src/backend/tests/test_ocel_inheritance_module.cpp)
  target_link_libraries(minera_ocel_inheritance_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_inheritance_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_inheritance_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_inheritance_module_tests COMMAND minera_ocel_inheritance_module_tests)

  add_executable(minera_ocel20_computation_module_tests src/backend/tests/test_ocel20_computation_module.cpp)
  target_link_libraries(minera_ocel20_computation_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel20_computation_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel20_computation_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel20_computation_module_tests COMMAND minera_ocel20_computation_module_tests)

  add_executable(minera_ocel_ot_endpoints_module_tests src/backend/tests/test_ocel_ot_endpoints_module.cpp)
  target_link_libraries(minera_ocel_ot_endpoints_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_ot_endpoints_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_ot_endpoints_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_ot_endpoints_module_tests COMMAND minera_ocel_ot_endpoints_module_tests)

  add_executable(minera_ocel_objects_ot_count_module_tests src/backend/tests/test_ocel_objects_ot_count_module.cpp)
  target_link_libraries(minera_ocel_objects_ot_count_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_objects_ot_count_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_objects_ot_count_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_objects_ot_count_module_tests COMMAND minera_ocel_objects_ot_count_module_tests)

  add_executable(minera_ocdfg_edge_metrics_tests src/backend/tests/test_ocdfg_edge_metrics.cpp)
  target_link_libraries(minera_ocdfg_edge_metrics_tests PRIVATE minera::core)
  target_compile_features(minera_ocdfg_edge_metrics_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocdfg_edge_metrics_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocdfg_edge_metrics_tests COMMAND minera_ocdfg_edge_metrics_tests)

  add_executable(minera_ocdfg_edge_metrics_controls_tests src/backend/tests/test_ocdfg_edge_metrics_controls.cpp)
  target_link_libraries(minera_ocdfg_edge_metrics_controls_tests PRIVATE minera::core Threads::Threads)
  target_include_directories(minera_ocdfg_edge_metrics_controls_tests PRIVATE third_party)
  target_compile_features(minera_ocdfg_edge_metrics_controls_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocdfg_edge_metrics_controls_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocdfg_edge_metrics_controls_tests COMMAND minera_ocdfg_edge_metrics_controls_tests)

  add_executable(minera_ocel_conformance_json_numbers_tests src/backend/tests/test_ocel_conformance_json_numbers.cpp)
  target_link_libraries(minera_ocel_conformance_json_numbers_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_conformance_json_numbers_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_conformance_json_numbers_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_conformance_json_numbers_tests COMMAND minera_ocel_conformance_json_numbers_tests)

  add_executable(minera_ocel_object_attributes_module_tests src/backend/tests/test_ocel_object_attributes_module.cpp)
  target_link_libraries(minera_ocel_object_attributes_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_object_attributes_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_object_attributes_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_object_attributes_module_tests COMMAND minera_ocel_object_attributes_module_tests)

  add_executable(minera_ocel_activity_type_matching_module_tests src/backend/tests/test_ocel_activity_type_matching_module.cpp)
  target_link_libraries(minera_ocel_activity_type_matching_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_activity_type_matching_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_activity_type_matching_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_activity_type_matching_module_tests COMMAND minera_ocel_activity_type_matching_module_tests)

  add_executable(minera_ocel_event_nat_membership_tests src/backend/tests/test_ocel_event_nat_membership.cpp)
  target_link_libraries(minera_ocel_event_nat_membership_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_event_nat_membership_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_event_nat_membership_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_event_nat_membership_tests COMMAND minera_ocel_event_nat_membership_tests)

  add_executable(minera_ocdfg_edge_performance_tests src/backend/tests/test_ocdfg_edge_performance.cpp)
  target_link_libraries(minera_ocdfg_edge_performance_tests PRIVATE minera::core)
  target_compile_features(minera_ocdfg_edge_performance_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocdfg_edge_performance_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocdfg_edge_performance_tests COMMAND minera_ocdfg_edge_performance_tests)

  add_executable(minera_ocdfg_performance_controls_tests src/backend/tests/test_ocdfg_performance_controls.cpp)
  target_link_libraries(minera_ocdfg_performance_controls_tests PRIVATE minera::core Threads::Threads)
  target_include_directories(minera_ocdfg_performance_controls_tests PRIVATE third_party)
  target_compile_features(minera_ocdfg_performance_controls_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocdfg_performance_controls_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocdfg_performance_controls_tests COMMAND minera_ocdfg_performance_controls_tests)

  add_executable(minera_ocel_event_attributes_module_tests src/backend/tests/test_ocel_event_attributes_module.cpp)
  target_link_libraries(minera_ocel_event_attributes_module_tests PRIVATE minera::core)
  target_compile_features(minera_ocel_event_attributes_module_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_event_attributes_module_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_event_attributes_module_tests COMMAND minera_ocel_event_attributes_module_tests)

  add_executable(minera_ocel_exact_filter_controls_tests src/backend/tests/test_ocel_exact_filter_controls.cpp)
  target_link_libraries(minera_ocel_exact_filter_controls_tests PRIVATE minera::core Threads::Threads)
  target_include_directories(minera_ocel_exact_filter_controls_tests PRIVATE third_party)
  target_compile_features(minera_ocel_exact_filter_controls_tests PRIVATE cxx_std_20)
  target_compile_options(minera_ocel_exact_filter_controls_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_ocel_exact_filter_controls_tests COMMAND minera_ocel_exact_filter_controls_tests)

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

  add_executable(minera_replay_input_options_tests
    src/backend/tests/test_replay_input_options.cpp
  )
  target_link_libraries(minera_replay_input_options_tests PRIVATE minera::core)
  target_include_directories(minera_replay_input_options_tests PRIVATE src/backend/tools)
  target_compile_features(minera_replay_input_options_tests PRIVATE cxx_std_20)
  target_compile_options(minera_replay_input_options_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_replay_input_options_tests COMMAND minera_replay_input_options_tests)

  add_executable(minera_knowledge_injection_tests
    src/backend/tests/test_knowledge_injection.cpp
  )
  target_link_libraries(minera_knowledge_injection_tests PRIVATE minera::core)
  target_compile_features(minera_knowledge_injection_tests PRIVATE cxx_std_20)
  target_compile_options(minera_knowledge_injection_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_knowledge_injection_tests
           COMMAND minera_knowledge_injection_tests)
  add_executable(minera_llm_runtime_tests src/backend/tests/test_llm_runtime.cpp)
  target_link_libraries(minera_llm_runtime_tests PRIVATE minera::core)
  target_include_directories(minera_llm_runtime_tests PRIVATE third_party)
  target_compile_features(minera_llm_runtime_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_runtime_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_runtime_tests COMMAND minera_llm_runtime_tests)
  add_executable(minera_log_query_tests src/backend/tests/test_log_query.cpp)
  target_link_libraries(minera_log_query_tests PRIVATE minera::core)
  target_compile_features(minera_log_query_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_query_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_query_tests COMMAND minera_log_query_tests)
  add_executable(minera_log_cluster_runtime_tests src/backend/tests/test_log_cluster_runtime.cpp)
  target_link_libraries(minera_log_cluster_runtime_tests PRIVATE minera::core)
  target_include_directories(minera_log_cluster_runtime_tests PRIVATE third_party src/backend/tools)
  target_compile_features(minera_log_cluster_runtime_tests PRIVATE cxx_std_20)
  target_compile_options(minera_log_cluster_runtime_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_log_cluster_runtime_tests COMMAND minera_log_cluster_runtime_tests)
  add_executable(minera_llm_context_tests src/backend/tests/test_llm_context.cpp)
  target_link_libraries(minera_llm_context_tests PRIVATE minera::core)
  target_compile_features(minera_llm_context_tests PRIVATE cxx_std_20)
  target_compile_options(minera_llm_context_tests PRIVATE ${MINERA_TEST_OPTIONS})
  add_test(NAME minera_llm_context_tests COMMAND minera_llm_context_tests)
endif()
