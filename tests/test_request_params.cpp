// Tests for the server's request-parsing layer (tools/minera_server feeds these
// the raw form fields). Kept httplib-free so the parsing — importer selection,
// CSV-list splitting, error escaping, and option mapping — can be exercised
// directly without standing up an HTTP server.
#undef NDEBUG

#include "minera/request_params.hpp"
#include "minera/json_export.hpp"
#include "minera/directly_follows_graph.hpp"

#include <cassert>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {

// Builds a field lookup over a fixed map, mirroring how the server resolves a
// multipart form field (absent fields read as "").
minera::FieldLookup fields(std::map<std::string, std::string> values) {
  return [values = std::move(values)](std::string_view name) -> std::string {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string{} : it->second;
  };
}

void ends_with_ci_is_case_insensitive() {
  assert(minera::ends_with_ci("LOG.XES", ".xes"));
  assert(minera::ends_with_ci("log.xes", ".xes"));
  assert(!minera::ends_with_ci("log.csv", ".xes"));
  assert(!minera::ends_with_ci("xe", ".xes"));  // shorter than the suffix
}

void looks_like_xes_uses_extension_then_content_sniff() {
  // Extension wins outright.
  assert(minera::looks_like_xes("trace.xes", "anything"));
  // A CSV name with XML content is still recognized by sniffing the preamble.
  assert(minera::looks_like_xes("trace.csv", "<?xml version=\"1.0\"?><log/>"));
  assert(minera::looks_like_xes("noext", "<log></log>"));
  // Leading whitespace before the preamble is tolerated.
  assert(minera::looks_like_xes("noext", "  \n\t<log/>"));
  // A UTF-8 BOM ahead of the preamble is skipped.
  assert(minera::looks_like_xes("noext", "\xEF\xBB\xBF<?xml ?>"));
  // Plain CSV is not XES.
  assert(!minera::looks_like_xes("trace.csv", "case,activity\n1,a\n"));
  assert(!minera::looks_like_xes("noext", "case,activity\n"));
}

void split_csv_trims_and_drops_empty_parts() {
  assert((minera::split_csv("a,b,c") ==
          std::vector<std::string>{"a", "b", "c"}));
  // Surrounding spaces are dropped on both sides; an internal space is kept.
  assert((minera::split_csv("a b, c") ==
          std::vector<std::string>{"a b", "c"}));
  assert((minera::split_csv("a , b ,c") ==
          std::vector<std::string>{"a", "b", "c"}));
  // Empty parts from doubled or trailing commas are skipped.
  assert((minera::split_csv("a,,b,") ==
          std::vector<std::string>{"a", "b"}));
  assert(minera::split_csv("").empty());
  assert(minera::split_csv(" , ").empty());
}

void error_json_escapes_quotes_and_backslashes() {
  assert(minera::error_json("plain") == "{\"error\": \"plain\"}");
  assert(minera::error_json("a\"b") == "{\"error\": \"a\\\"b\"}");
  assert(minera::error_json("a\\b") == "{\"error\": \"a\\\\b\"}");
}

void parse_mine_params_defaults_when_empty() {
  const auto params = minera::parse_mine_params(fields({}));
  // Stock importer defaults survive an empty form.
  assert(params.csv.case_id_column == "case_id");
  assert(params.csv.activity_column == "activity");
  assert(params.csv.delimiter == ',');
  assert(!params.filter.any());
  assert(params.inductive.noise_threshold == 0.0);
  assert(params.alpha.short_loops);  // Alpha+ is the default variant
  assert(params.correlation_variants.empty());
  assert(params.correlation.start_timestamp_attribute.empty());
  assert(!params.correlation.exact_time_matching);
  assert(params.correlation.sample_size == 100000);
  assert(params.spectrum_activities.empty());
  assert(params.spectrum.variant == minera::PerformanceSpectrumVariant::kConnected);
  assert(params.spectrum.sample_size == 10000);
  assert(params.spectrum.sort_log);
  assert(!params.batches);
  assert(params.batch_options.merge_distance_seconds == 900.0);
  assert(params.batch_options.min_batch_size == 2);
}

void concept_drift_params_contract() {
  const auto p = minera::parse_concept_drift_params(fields({
      {"drift_sub_log_size", "3"}, {"drift_window_size", "2"},
      {"drift_num_permutations", "5"}, {"drift_threshold", "0.25"},
      {"drift_max_no_change_points", "4"}, {"drift_seed", "9"},
      {"drift_include_timestamps", "true"}}));
  assert(p.sub_log_size == 3 && p.window_size == 2 && p.num_permutations == 5);
  assert(p.threshold == 0.25 && p.max_no_change_points == 4 && p.seed == 9 && p.include_timestamps);
  bool threw = false;
  try { (void)minera::parse_concept_drift_params(fields({{"drift_threshold", "2"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void log_to_target_params_contract() {
  const auto p = minera::parse_log_to_target_params(fields({
      {"target_variant", "remaining_time"},
      {"target_activity_key", "concept:name"},
      {"target_timestamp_key", "started_at"},
      {"target_enable_padding", "true"},
      {"target_pad_size", "8"},
      {"target_activities", "C, A, B"},
  }));
  assert(p.variant == minera::LogToTargetVariant::REMAINING_TIME);
  assert(p.activity_key == "concept:name");
  assert(p.timestamp_key == "started_at");
  assert(p.enable_padding && p.pad_size == 8);
  assert((p.activities == std::vector<std::string>{"C", "A", "B"}));
  const auto escaped = minera::parse_log_to_target_params(fields({
      {"target_activities", R"(Review\, senior,A\\B)"}}));
  assert((escaped.activities ==
          std::vector<std::string>{"Review, senior", "A\\B"}));

  bool threw = false;
  try {
    (void)minera::parse_log_to_target_params(
        fields({{"target_variant", "unknown"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_log_to_target_params(
        fields({{"target_enable_padding", "true"}, {"target_pad_size", "bad"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void trie_params_contract() {
  const auto p = minera::parse_trie_params(fields({
      {"trie_activity_key", "phase"}, {"trie_max_path_length", "4"}}));
  assert(p.activity_key == "phase");
  assert(p.max_path_length && *p.max_path_length == 4);
  bool threw = false;
  try {
    (void)minera::parse_trie_params(
        fields({{"trie_max_path_length", "0"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void interval_tree_params_contract() {
  const auto p = minera::parse_interval_tree_params(fields({
      {"interval_activity_key", "phase"},
      {"interval_start_timestamp_key", "started_at"},
      {"interval_timestamp_key", "completed_at"},
      {"interval_epsilon", "0.25"},
      {"interval_filter_activity_couple", R"(Review\, senior,A\\B)"},
  }));
  assert(p.activity_key == "phase");
  assert(p.start_timestamp_key == "started_at");
  assert(p.timestamp_key == "completed_at");
  assert(p.epsilon == 0.25);
  assert(p.filter_activity_couple);
  assert(p.filter_activity_couple->first == "Review, senior");
  assert(p.filter_activity_couple->second == "A\\B");

  bool threw = false;
  try {
    (void)minera::parse_interval_tree_params(
        fields({{"interval_filter_activity_couple", "A"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_interval_tree_params(
        fields({{"interval_epsilon", "-0.1"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void case_relations_params_contract() {
  const auto p = minera::parse_case_relations_params(
      fields({{"merge_timestamp_key", "completed_at"}}));
  assert(p.timestamp_key == "completed_at");
  bool threw = false;
  try {
    (void)minera::parse_case_relations_params(
        fields({{"merge_timestamp_key", ""}}),
        [](std::string_view name) { return name == "merge_timestamp_key"; });
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void random_tree_params_contract() {
  const auto p = minera::parse_random_tree_params(fields({
      {"random_tree_min_rec_depth", "2"},
      {"random_tree_max_rec_depth", "4"},
      {"random_tree_prob_leaf", "0.75"},
      {"random_tree_seed", "17"},
      {"random_tree_activities", R"(Review\, senior,A\\B)"},
  }));
  assert(p.min_rec_depth == 2 && p.max_rec_depth == 4);
  assert(p.prob_leaf == 0.75 && p.seed == 17);
  assert((p.activities == std::vector<std::string>{"Review, senior", "A\\B"}));
  bool threw = false;
  try {
    (void)minera::parse_random_tree_params(
        fields({{"random_tree_prob_leaf", "2"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_maps_correlation_options() {
  const auto params = minera::parse_mine_params(fields({
      {"correlation_variants", "trace_based, classic, classic,classic_split"},
      {"correlation_start_timestamp_attribute", "started_at"},
      {"correlation_exact_time_matching", "1"},
      {"correlation_sample_size", "42"},
  }));
  assert((params.correlation_variants ==
          std::vector<std::string>{"classic", "classic_split", "trace_based"}));
  assert(params.correlation.start_timestamp_attribute == "started_at");
  assert(params.correlation.exact_time_matching);
  assert(params.correlation.sample_size == 42);
}

void parse_mine_params_preserves_explicit_empty_correlation_timestamp() {
  const std::map<std::string, std::string> values = {
      {"correlation_start_timestamp_attribute", ""}};
  const auto lookup = [&values](std::string_view name) -> std::string {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string{} : it->second;
  };
  const auto present = [&values](std::string_view name) {
    return values.contains(std::string(name));
  };
  const auto explicit_empty = minera::parse_mine_params(lookup, present);
  assert(explicit_empty.correlation.start_timestamp_attribute.empty());
  // Compatibility overload treats an empty value as omitted too.
  const auto legacy = minera::parse_mine_params(lookup);
  assert(legacy.correlation.start_timestamp_attribute.empty());
}

void parse_mine_params_rejects_invalid_correlation_options() {
  for (const auto& item : {
           std::pair{"correlation_variants", "classic,bogus"},
           std::pair{"correlation_exact_time_matching", "yes"},
           std::pair{"correlation_sample_size", "0"},
           std::pair{"correlation_sample_size", "-1"},
           std::pair{"correlation_sample_size", "12x"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  const std::map<std::string, std::string> empty = {
      {"correlation_variants", ""}};
  const auto lookup = [&empty](std::string_view name) -> std::string {
    const auto it = empty.find(std::string(name));
    return it == empty.end() ? std::string{} : it->second;
  };
  const auto present = [&empty](std::string_view name) {
    return empty.contains(std::string(name));
  };
  bool threw = false;
  try {
    (void)minera::parse_mine_params(lookup, present);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_maps_csv_and_inductive_options() {
  const auto params = minera::parse_mine_params(fields({
      {"case_column", "Case"},
      {"activity_column", "Act"},
      {"timestamp_column", "Time"},
      {"resource_column", "Who"},
      {"lifecycle_column", "Phase"},
      {"delimiter", ";"},
      {"noise_threshold", "0.25"},
      {"alpha_variant", "classic"},
      {"inductive_variant", "dfg"},
      {"heuristic_variant", "plusplus"},
      {"dependency_threshold", "0.75"},
      {"and_threshold", "0.5"},
      {"min_edge_frequency", "3"},
      {"heuristic_min_activity_count", "4"},
      {"heuristic_decoration", "performance"},
      {"heuristic_start_timestamp_attribute", "started_at"},
      {"heuristic_lifecycle_instance_attribute", "instance_id"},
      {"heuristic_dfg_pre_cleaning_noise_threshold", "0.1"},
      {"heuristic_loop_length_two_threshold", "0.6"},
      {"heuristic_keep_once_per_case", "true"},
      {"heuristic_most_common_paths", "A -> B, C -> D"},
      {"heuristic_performance_aggregation", "all"},
  }));
  assert(params.csv.case_id_column == "Case");
  assert(params.csv.activity_column == "Act");
  assert(params.csv.timestamp_column == "Time");
  assert(params.csv.resource_column == "Who");
  assert(params.csv.lifecycle_column == "Phase");
  assert(params.csv.delimiter == ';');  // only the first char is taken
  assert(params.inductive.noise_threshold == 0.25);
  assert(params.inductive.variant == minera::InductiveMinerVariant::kDfg);
  assert(!params.alpha.short_loops);
  assert(params.heuristic.variant ==
         minera::HeuristicMinerVariant::kPlusPlus);
  assert(params.heuristic.dependency_threshold == 0.75);
  assert(params.heuristic.and_threshold == 0.5);
  assert(params.heuristic.min_frequency == 3);
  assert(params.heuristic.min_activity_count == 4);
  assert(params.heuristic.decoration ==
         minera::HeuristicNetDecoration::kPerformance);
  assert(params.heuristic.start_timestamp_attribute == "started_at");
  assert(params.heuristic.lifecycle_instance_attribute == "instance_id");
  assert(params.heuristic.dfg_pre_cleaning_noise_threshold == 0.1);
  assert(params.heuristic.loop_length_two_threshold == 0.6);
  assert(params.heuristic.keep_once_per_case);
  assert(params.heuristic.most_common_paths.contains({"A", "B"}));
  assert(params.heuristic.most_common_paths.contains({"C", "D"}));
  assert(params.heuristic.performance_aggregation ==
         minera::HeuristicPerformanceAggregation::kAll);
}

void parse_mine_params_rejects_an_unknown_inductive_variant() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"inductive_variant", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_an_unknown_alpha_variant() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"alpha_variant", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_maps_filters() {
  const auto params = minera::parse_mine_params(fields({
      {"time_from", "2024-01-01"},
      {"time_to", "2024-01-31T23:59:59"},
      {"time_mode", "traces_intersecting"},
      {"keep_activities", "a, b ,c"},
      {"remove_activities", "x"},
      {"keep_resources", "Alice, Bob"},
      {"remove_resources", "Carol"},
      {"keep_lifecycles", "complete"},
      {"remove_lifecycles", "schedule, suspend"},
      {"start_activities", "a"},
      {"end_activities", "c"},
      {"min_length", "2"},
      {"max_length", "10"},
      {"min_variant_frequency", "3"},
      {"top_variants", "5"},
      {"rework_activity", "b"},
      {"rework_min_occurrences", "3"},
      {"between_start", "a"},
      {"between_end", "c"},
      {"attribute_key", "channel"},
      {"keep_attribute_values", "web, phone"},
      {"remove_attribute_values", "fax"},
      {"attribute_mode", "traces"},
      {"case_attribute_key", "region"},
      {"keep_case_attribute_values", "north, east"},
      {"remove_case_attribute_values", "south"},
      {"keep_directly_follows", "a -> b, b->c"},
      {"remove_eventually_follows", "a -> d"},
      {"prefix_activity", "c"},
      {"suffix_activity", "a"},
      {"variant_coverage", "0.8"},
  }));
  assert(params.filter.time_from == "2024-01-01");
  assert(params.filter.time_to == "2024-01-31T23:59:59");
  assert(params.filter.time_mode ==
         minera::TimeFilterMode::traces_intersecting);
  assert((params.filter.keep_activities ==
          std::vector<std::string>{"a", "b", "c"}));
  assert((params.filter.remove_activities == std::vector<std::string>{"x"}));
  assert((params.filter.keep_resources ==
          std::vector<std::string>{"Alice", "Bob"}));
  assert((params.filter.remove_resources == std::vector<std::string>{"Carol"}));
  assert((params.filter.keep_lifecycles ==
          std::vector<std::string>{"complete"}));
  assert((params.filter.remove_lifecycles ==
          std::vector<std::string>{"schedule", "suspend"}));
  assert((params.filter.start_activities == std::vector<std::string>{"a"}));
  assert((params.filter.end_activities == std::vector<std::string>{"c"}));
  assert(params.filter.min_length == 2);
  assert(params.filter.max_length == 10);
  assert(params.filter.min_variant_frequency == 3);
  assert(params.filter.top_variants == 5);
  assert(params.filter.rework_activity == "b");
  assert(params.filter.rework_min_occurrences == 3);
  assert(params.filter.between_start == "a");
  assert(params.filter.between_end == "c");
  assert(params.filter.attribute_key == "channel");
  assert((params.filter.keep_attribute_values ==
          std::vector<std::string>{"web", "phone"}));
  assert((params.filter.remove_attribute_values ==
          std::vector<std::string>{"fax"}));
  assert(params.filter.attribute_mode == minera::AttributeFilterMode::traces);
  assert(params.filter.case_attribute_key == "region");
  assert((params.filter.keep_case_attribute_values ==
          std::vector<std::string>{"north", "east"}));
  assert((params.filter.remove_case_attribute_values ==
          std::vector<std::string>{"south"}));
  assert(params.filter.keep_directly_follows.size() == 2);
  assert(params.filter.keep_directly_follows[0].predecessor == "a");
  assert(params.filter.keep_directly_follows[0].successor == "b");
  assert(params.filter.keep_directly_follows[1].predecessor == "b");
  assert(params.filter.keep_directly_follows[1].successor == "c");
  assert(params.filter.remove_eventually_follows.size() == 1);
  assert(params.filter.remove_eventually_follows[0].successor == "d");
  assert(params.filter.prefix_activity == "c");
  assert(params.filter.suffix_activity == "a");
  assert(params.filter.variant_coverage == 0.8);
  assert(params.filter.any());
}

void parse_mine_params_rejects_a_malformed_succession() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(
        fields({{"keep_directly_follows", "a, b"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_an_unknown_attribute_mode() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"attribute_mode", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_an_unknown_time_mode() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"time_mode", "bogus"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_ignores_blank_fields() {
  // A field present but empty must not override the default (the server treats
  // "" as "not supplied").
  const auto params = minera::parse_mine_params(fields({
      {"case_column", ""},
      {"delimiter", ""},
      {"min_length", ""},
  }));
  assert(params.csv.case_id_column == "case_id");
  assert(params.csv.delimiter == ',');
  assert(params.filter.min_length == 0);
}

void parse_mine_params_maps_transition_system_options() {
  const auto params = minera::parse_mine_params(fields({
      {"transition_view", "multiset"},
      {"transition_direction", "backward"},
      {"transition_window", "0"},
      {"transition_include_data", "1"},
  }));
  assert(params.transition_system.view == minera::TransitionView::kMultiset);
  assert(params.transition_system.direction == minera::TransitionDirection::kBackward);
  assert(params.transition_system.window == 0);
  assert(params.transition_system.include_data);
}

void parse_mine_params_maps_powl_options() {
  const auto params = minera::parse_mine_params(fields({
      {"powl_variant", "dynamic_clustering_plain"},
      {"powl_filtering_type", "dynamic"},
      {"powl_filtering_threshold", "0.25"},
      {"powl_order_frequency_ratio", "0.8"},
      {"powl_brute_force_activity_limit", "12"},
  }));
  assert(params.powl.variant ==
         minera::PowlDiscoveryVariant::kDynamicClusteringPlain);
  assert(params.powl.filtering_type == minera::PowlFilteringType::kDynamic);
  assert(params.powl.filtering_threshold == 0.25);
  assert(params.powl.order_frequency_ratio == 0.8);
  assert(params.powl.brute_force_activity_limit == 12);
}

void parse_mine_params_rejects_invalid_powl_fields() {
  for (const auto& item : {
           std::pair{"powl_variant", "bad"},
           std::pair{"powl_filtering_type", "bad"},
           std::pair{"powl_filtering_threshold", "-0.1"},
           std::pair{"powl_filtering_threshold", "1"},
           std::pair{"powl_filtering_threshold", "nan"},
           std::pair{"powl_order_frequency_ratio", "0.5"},
           std::pair{"powl_order_frequency_ratio", "1.1"},
           std::pair{"powl_order_frequency_ratio", "0.9x"},
           std::pair{"powl_brute_force_activity_limit", "-1"},
           std::pair{"powl_brute_force_activity_limit", "2x"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_rejects_invalid_transition_fields() {
  for (const auto& item : {std::pair{"transition_view", "bad"},
                           std::pair{"transition_direction", "bad"},
                           std::pair{"transition_window", "-1"},
                           std::pair{"transition_window", "2x"},
                           std::pair{"transition_include_data", "yes"}}) {
    bool threw = false;
    try { (void)minera::parse_mine_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void parse_mine_params_maps_token_replay_options() {
  const auto params = minera::parse_mine_params(fields({
      {"token_replay_variant", "backwards"},
      {"token_replay_stop_immediately_unfit", "1"},
  }));
  assert(params.token_replay.variant ==
         minera::TokenReplayVariant::kBackwards);
  assert(params.token_replay.stop_immediately_unfit);
}

void parse_mine_params_rejects_invalid_token_replay_fields() {
  for (const auto& item : {
           std::pair{"token_replay_variant", "backward"},
           std::pair{"token_replay_variant", "bad"},
           std::pair{"token_replay_stop_immediately_unfit", "yes"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_maps_log_skeleton_options() {
  const auto params = minera::parse_mine_params(fields({
      {"log_skeleton_noise_threshold", "0.25"},
      {"log_skeleton_constraints",
       " always_after, equivalence, always_after,, activ_freq "},
  }));
  assert(params.log_skeleton.noise_threshold == 0.25);
  assert((params.log_skeleton.considered_constraints ==
          std::vector<std::string>{"activ_freq", "always_after",
                                   "equivalence"}));
}

void parse_mine_params_rejects_invalid_log_skeleton_fields() {
  for (const auto& item : {
           std::pair{"log_skeleton_noise_threshold", "-0.1"},
           std::pair{"log_skeleton_noise_threshold", "1.1"},
           std::pair{"log_skeleton_noise_threshold", "nan"},
           std::pair{"log_skeleton_constraints", "equivalence,unknown"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_maps_declare_options() {
  const auto params = minera::parse_mine_params(fields({
      {"declare_considered_activities", " B, A, B "},
      {"declare_allowed_templates", "response, existence,response"},
      {"declare_min_support_ratio", "0.25"},
      {"declare_min_confidence_ratio", "1"},
      {"declare_auto_selection_multiplier", "0.75"},
  }));
  assert(params.declare_options.considered_activities.has_value());
  assert((params.declare_options.considered_activities.value() ==
          std::vector<std::string>{"B", "A", "B"}));
  assert((params.declare_options.allowed_templates.value() ==
          std::vector<std::string>{"existence", "response"}));
  assert(params.declare_options.has_min_support_ratio);
  assert(params.declare_options.min_support_ratio == 0.25);
  assert(params.declare_options.has_min_confidence_ratio);
  assert(params.declare_options.min_confidence_ratio == 1.0);
  assert(params.declare_options.auto_selection_multiplier == 0.75);
  const auto defaults = minera::parse_mine_params(fields({
      {"declare_considered_activities", ""},
      {"declare_allowed_templates", ""},
  }));
  assert(!defaults.declare_options.considered_activities.has_value());
  assert(!defaults.declare_options.allowed_templates.has_value());
  assert(!defaults.declare_options.has_min_support_ratio);
  assert(!defaults.declare_options.has_min_confidence_ratio);
}

void parse_mine_params_preserves_explicit_empty_declare_filters() {
  const std::map<std::string, std::string> values = {
      {"declare_considered_activities", ""},
      {"declare_allowed_templates", ""},
  };
  const auto lookup = [&values](std::string_view name) -> std::string {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string{} : it->second;
  };
  const auto present = [&values](std::string_view name) {
    return values.contains(std::string(name));
  };
  const auto explicit_empty = minera::parse_mine_params(lookup, present);
  assert(explicit_empty.declare_options.considered_activities.has_value());
  assert(explicit_empty.declare_options.considered_activities->empty());
  assert(explicit_empty.declare_options.allowed_templates.has_value());
  assert(explicit_empty.declare_options.allowed_templates->empty());

  // The compatibility overload keeps the historical absent-or-empty behavior.
  const auto legacy = minera::parse_mine_params(lookup);
  assert(!legacy.declare_options.considered_activities.has_value());
  assert(!legacy.declare_options.allowed_templates.has_value());

  const auto absent = minera::parse_mine_params(
      fields({}), [](std::string_view) { return false; });
  assert(!absent.declare_options.considered_activities.has_value());
  assert(!absent.declare_options.allowed_templates.has_value());
}

void parse_mine_params_rejects_invalid_declare_fields() {
  for (const auto& item : {
           std::pair{"declare_allowed_templates", "response,unknown"},
           std::pair{"declare_min_support_ratio", "-0.1"},
           std::pair{"declare_min_support_ratio", "1.1"},
           std::pair{"declare_min_support_ratio", "nan"},
           std::pair{"declare_min_confidence_ratio", "inf"},
           std::pair{"declare_auto_selection_multiplier", "2"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_maps_footprints_options() {
  const auto params = minera::parse_mine_params(fields({
      {"footprints_discovery_variants",
       "trace_by_trace, dfg,trace_by_trace,petri_reach_graph"},
      {"footprints_model", "alpha"},
      {"footprints_conformance_variants",
       "trace_extensive,log_model,trace_extensive"},
      {"footprints_strict", "1"},
      {"footprints_enable_act_always_executed", "false"},
  }));
  assert((params.footprints.discovery_variants ==
          std::set<std::string>{"dfg", "petri_reach_graph",
                                "trace_by_trace"}));
  assert(params.footprints.model == "alpha");
  assert((params.footprints.conformance_variants ==
          std::set<std::string>{"log_model", "trace_extensive"}));
  assert(params.footprints.strict);
  assert(!params.footprints.enable_act_always_executed);

  const auto defaults = minera::parse_mine_params(fields({}));
  assert((defaults.footprints.discovery_variants ==
          std::set<std::string>{"dfg", "entire_event_log", "powl",
                                "process_tree"}));
  assert(defaults.footprints.model == "process_tree");
  assert((defaults.footprints.conformance_variants ==
          std::set<std::string>{"log_model"}));
  assert(!defaults.footprints.strict);
  assert(defaults.footprints.enable_act_always_executed);
}

void parse_mine_params_rejects_invalid_footprints_options() {
  for (const auto& item : {
           std::pair{"footprints_discovery_variants", "dfg,unknown"},
           std::pair{"footprints_model", "unknown"},
           std::pair{"footprints_conformance_variants", "future"},
           std::pair{"footprints_strict", "yes"},
           std::pair{"footprints_enable_act_always_executed", "enabled"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  const std::map<std::string, std::string> empty_lists = {
      {"footprints_discovery_variants", ""},
      {"footprints_conformance_variants", ""},
  };
  const auto lookup = [&empty_lists](std::string_view name) -> std::string {
    const auto it = empty_lists.find(std::string(name));
    return it == empty_lists.end() ? std::string{} : it->second;
  };
  const auto present = [&empty_lists](std::string_view name) {
    return empty_lists.contains(std::string(name));
  };
  bool threw = false;
  try {
    (void)minera::parse_mine_params(lookup, present);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"footprints_model", "dfg"},
        {"footprints_discovery_variants", "dfg,petri_reach_graph"},
    }));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_maps_dfg_variant_options() {
  const auto params = minera::parse_mine_params(fields({
      {"dfg_statistical_variants",
       "clean_time,frequency_greedy,native,frequency,clean_time,performance"},
      {"dfg_causal_variants", "heuristic,alpha,heuristic"},
      {"dfg_window", "0"},
      {"dfg_keep_once_per_case", "true"},
      {"dfg_performance_aggregation", "all"},
      {"dfg_start_timestamp_attribute", "start:timestamp"},
      {"dfg_business_hours", "1"},
      {"dfg_business_hour_slots", "0-3600,7200:10800"},
      {"dfg_case_attributes", "region,concept:name,region"},
      {"dfg_return_node_attributes", "true"},
  }));
  assert((params.dfg_statistical_variants ==
          std::vector<std::string>{"native", "frequency",
                                   "frequency_greedy", "performance",
                                   "clean_time"}));
  assert((params.dfg_causal_variants ==
          std::vector<std::string>{"alpha", "heuristic"}));
  assert(params.dfg_statistics.window == 0);
  assert(params.dfg_statistics.keep_once_per_case);
  assert(params.dfg_statistics.performance_aggregation ==
         minera::DfgPerformanceAggregation::kAll);
  assert(params.dfg_statistics.start_timestamp_attribute ==
         "start:timestamp");
  assert(params.dfg_statistics.use_business_hours);
  assert(params.dfg_statistics.business_hour_slots.size() == 2);
  assert(params.dfg_statistics.business_hour_slots[0].start_seconds == 0.0);
  assert(params.dfg_statistics.business_hour_slots[0].end_seconds == 3600.0);
  assert(params.dfg_statistics.business_hour_slots[1].start_seconds == 7200.0);
  assert(params.dfg_statistics.business_hour_slots[1].end_seconds == 10800.0);
  assert((params.dfg_statistics.case_attributes ==
          std::vector<std::string>{"concept:name", "region"}));
  assert(params.dfg_statistics.return_node_attributes);

  const auto defaults = minera::parse_mine_params(fields({}));
  assert(defaults.dfg_statistical_variants.empty());
  assert(defaults.dfg_causal_variants.empty());
  assert(defaults.dfg_statistics.window == 1);
  assert(!defaults.dfg_statistics.keep_once_per_case);
  assert(defaults.dfg_statistics.performance_aggregation ==
         minera::DfgPerformanceAggregation::kMean);
  assert(defaults.dfg_statistics.start_timestamp_attribute ==
         "start_timestamp");
  assert(!defaults.dfg_statistics.use_business_hours);
  assert(defaults.dfg_statistics.business_hour_slots.empty());
  assert((defaults.dfg_statistics.case_attributes ==
          std::vector<std::string>{"concept:name"}));
  assert(!defaults.dfg_statistics.return_node_attributes);
}

void parse_mine_params_preserves_explicit_empty_dfg_attributes() {
  const std::map<std::string, std::string> values = {
      {"dfg_start_timestamp_attribute", ""},
      {"dfg_business_hour_slots", ""},
      {"dfg_case_attributes", ""},
  };
  const auto lookup = [&values](std::string_view name) -> std::string {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string{} : it->second;
  };
  const auto present = [&values](std::string_view name) {
    return values.contains(std::string(name));
  };
  const auto params = minera::parse_mine_params(lookup, present);
  assert(params.dfg_statistics.start_timestamp_attribute.empty());
  assert(params.dfg_statistics.business_hour_slots.empty());
  assert(params.dfg_statistics.case_attributes.empty());
}

void parse_mine_params_rejects_invalid_dfg_variant_options() {
  for (const auto& item : {
           std::pair{"dfg_statistical_variants", "native,unknown"},
           std::pair{"dfg_causal_variants", "alpha,future"},
           std::pair{"dfg_window", "-1"},
           std::pair{"dfg_window", "2x"},
           std::pair{"dfg_keep_once_per_case", "yes"},
           std::pair{"dfg_performance_aggregation", "average"},
           std::pair{"dfg_business_hours", "on"},
           std::pair{"dfg_business_hour_slots", "0-0"},
           std::pair{"dfg_business_hour_slots", "0-604801"},
           std::pair{"dfg_business_hour_slots", "nan-10"},
           std::pair{"dfg_return_node_attributes", "yes"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  for (const std::string name : {"dfg_statistical_variants",
                                 "dfg_causal_variants"}) {
    const std::map<std::string, std::string> values = {{name, ""}};
    const auto lookup = [&values](std::string_view key) -> std::string {
      const auto it = values.find(std::string(key));
      return it == values.end() ? std::string{} : it->second;
    };
    const auto present = [&values](std::string_view key) {
      return values.contains(std::string(key));
    };
    bool threw = false;
    try {
      (void)minera::parse_mine_params(lookup, present);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_alignment_params_maps_variants_and_costs() {
  const auto p = minera::parse_alignment_params(fields({
      {"alignment_variant", "discounted_a_star"},
      {"max_states", "17"}, {"max_align_time", "1.5"},
      {"max_align_time_trace", "0.5"}, {"exponent", "2.5"},
      {"synchronous_dijkstra", "true"}, {"cost_profile", "standard"},
      {"trace_cost", "3"}, {"model_cost", "4"},
      {"silent_model_cost", "2"}, {"sync_cost", "0.2"}}));
  assert(p.variant == minera::AlignmentVariant::kDiscountedAStar);
  assert(p.max_states == 17 && p.max_total_time_seconds == 1.5);
  assert(p.max_time_seconds == 0.5 && p.exponent == 2.5);
  assert(p.synchronous_dijkstra && p.default_trace_cost.value() == 3.0);
  assert(p.default_model_cost.value() == 4.0 && p.default_silent_cost.value() == 2.0);
}

void parse_alignment_params_rejects_invalid_and_dependent_values() {
  bool threw = false;
  try { (void)minera::parse_alignment_params(fields({{"max_states", "0"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_alignment_params(fields({{"exponent", "1"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_alignment_params(fields({{"alignment_variant", "dijkstra_no_heuristics"}, {"max_alignments", "2"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_alignment_params(fields({{"alignment_variant", "discounted_a_star"}, {"exponent", "2"}, {"theta", "3"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void parse_process_tree_alignment_params_maps_variants_and_limits() {
  for (const auto& value : {"SEARCH_GRAPH_PT", "search_graph_pt",
                            "Variants.SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP",
                            "Variants.APPROXIMATED_ORIGINAL"}) {
    const auto p = minera::parse_process_tree_alignment_params(
        fields({{"pt_alignment_variant", value}}));
    (void)p;
  }
  const auto p = minera::parse_process_tree_alignment_params(fields({
      {"pt_alignment_variant", "dynamic-programming"},
      {"pt_alignment_max_states", "0"},
      {"pt_alignment_max_time_ms", "1500"},
      {"pt_alignment_max_total_time_ms", "0"},
      {"pt_alignment_max_trace_length", "0"},
      {"pt_alignment_max_process_tree_height", "3"},
      {"pt_alignment_use_global_cache", "true"},
      {"pt_alignment_batch_size", "2"},
      {"pt_alignment_clear_cache", "false"}}));
  assert(p.variant == minera::ProcessTreeAlignmentVariant::kDynamicProgramming);
  assert(p.max_states == 0 && p.max_time_seconds == 1.5);
  assert(p.max_total_time_seconds == 0.0 && p.max_trace_length == 0);
  assert(p.max_process_tree_height == 3 && p.use_global_cache && p.batch_size == 2 && !p.clear_cache);
}

void parse_process_tree_alignment_params_rejects_bad_values() {
  for (const auto& item : {std::pair{"pt_alignment_batch_size", "0"},
                           std::pair{"pt_alignment_max_time_ms", "-1"},
                           std::pair{"pt_alignment_max_time_ms", "nan"},
                           std::pair{"pt_alignment_variant", "unknown"}}) {
    bool threw = false;
    try { (void)minera::parse_process_tree_alignment_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void parse_native_adapter_params_contract() {
  const auto d = minera::parse_decomposed_alignment_params(fields({
      {"variant", "recompos_maximal"}, {"max_states", "42"},
      {"threshold_border_agreement", "2.5"}, {"initial_marking", "1, 0"},
      {"tau_move_cost", "3"}, {"sync_cost", "0.5"},
      {"activity_log_costs", "A\\,B=7"}}));
  assert(d.max_states == 42 && d.initial_marking.size() == 2 &&
         d.tau_model_cost == 3.0 && d.sync_cost == 0.5 &&
         d.activity_log_costs.at("A,B") == 7.0);
  assert(minera::parse_activity_costs("A\\=B:4", "costs").at("A=B") ==
         4.0);
  const auto a = minera::parse_petri_anti_alignment_params(fields({
      {"variant", "version_discounted_a_star"}, {"exponent", "2"},
      {"marking_limit", "10"}, {"max_states", "0"}}));
  assert(a.marking_limit == 10 && a.max_states == 0);
  bool threw = false;
  try { (void)minera::parse_petri_multi_alignment_params(fields({{"marking_limit", "0"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  const auto r = minera::parse_process_tree_reduction_params(fields({
      {"variant", "tree_tr_based"}, {"activity_key", "concept:name"}}));
  assert(r.activity_key == "concept:name");
}

void parse_evaluation_params_defaults_and_variants() {
  const auto defaults = minera::parse_evaluation_params(fields({}));
  assert(defaults.family == minera::EvaluationFamily::kReplayFitness);
  assert(defaults.replay_fitness_variant == minera::ReplayFitnessVariant::AUTO);
  assert(defaults.precision_variant == minera::PrecisionVariant::AUTO);
  assert(!defaults.precision_options.use_task_ids);
  assert(defaults.precision_options.max_states == 100000);
  assert(defaults.generalization_variant ==
         minera::GeneralizationVariant::GENERALIZATION_TOKEN);
  assert(defaults.simplicity_variant == minera::SimplicityVariant::ARC_DEGREE);
  assert(defaults.simplicity_options.arc_degree_k == 2.0);
  assert(defaults.simplicity_options.max_states == 100000);
  assert(defaults.simplicity_options.initial_marking.empty());
  assert(defaults.simplicity_options.final_marking.empty());

  for (const auto& family : {"replay_fitness", "precision", "generalization",
                             "simplicity"}) {
    const auto parsed = minera::parse_evaluation_params(
        fields({{"evaluation_family", family}}));
    assert((family == std::string("replay_fitness") &&
            parsed.family == minera::EvaluationFamily::kReplayFitness) ||
           (family == std::string("precision") &&
            parsed.family == minera::EvaluationFamily::kPrecision) ||
           (family == std::string("generalization") &&
            parsed.family == minera::EvaluationFamily::kGeneralization) ||
           (family == std::string("simplicity") &&
            parsed.family == minera::EvaluationFamily::kSimplicity));
  }
  for (const auto& value : {"auto", "token_based", "alignment_based"}) {
    const auto parsed = minera::parse_evaluation_params(
        fields({{"fitness_variant", value}}));
    assert((value == std::string("auto") &&
            parsed.replay_fitness_variant == minera::ReplayFitnessVariant::AUTO) ||
           (value == std::string("token_based") &&
            parsed.replay_fitness_variant == minera::ReplayFitnessVariant::TOKEN_BASED) ||
           (value == std::string("alignment_based") &&
            parsed.replay_fitness_variant == minera::ReplayFitnessVariant::ALIGNMENT_BASED));
  }
  for (const auto& value : {"auto", "etconformance_token", "align_etconformance",
                            "automaton_after_align"}) {
    const auto parsed = minera::parse_evaluation_params(
        fields({{"precision_variant", value}}));
    assert((value == std::string("auto") &&
            parsed.precision_variant == minera::PrecisionVariant::AUTO) ||
           (value == std::string("etconformance_token") &&
            parsed.precision_variant == minera::PrecisionVariant::ETCONFORMANCE_TOKEN) ||
           (value == std::string("align_etconformance") &&
            parsed.precision_variant == minera::PrecisionVariant::ALIGN_ETCONFORMANCE) ||
           (value == std::string("automaton_after_align") &&
            parsed.precision_variant == minera::PrecisionVariant::AUTOMATON_AFTER_ALIGN));
  }
  for (const auto& value : {"arc_degree", "extended_cardoso",
                            "extended_cyclomatic"}) {
    const auto parsed = minera::parse_evaluation_params(
        fields({{"simplicity_variant", value}}));
    assert((value == std::string("arc_degree") &&
            parsed.simplicity_variant == minera::SimplicityVariant::ARC_DEGREE) ||
           (value == std::string("extended_cardoso") &&
            parsed.simplicity_variant == minera::SimplicityVariant::EXTENDED_CARDOSO) ||
           (value == std::string("extended_cyclomatic") &&
            parsed.simplicity_variant == minera::SimplicityVariant::EXTENDED_CYCLOMATIC));
  }
  const auto all = minera::parse_evaluation_params(fields({
      {"evaluation_family", "simplicity"},
      {"fitness_variant", "alignment_based"},
      {"precision_variant", "automaton_after_align"},
      {"precision_use_task_ids", "true"},
      {"precision_max_states", "1"},
      {"generalization_variant", "generalization_token"},
      {"simplicity_variant", "extended_cyclomatic"},
      {"simplicity_arc_degree_k", "0"},
      {"simplicity_max_states", "7"},
      {"simplicity_initial_marking", "0, 1,2"},
      {"simplicity_final_marking", "3,4"}}));
  assert(all.precision_options.use_task_ids && all.precision_options.max_states == 1);
  assert(all.simplicity_options.arc_degree_k == 0.0 &&
         all.simplicity_options.max_states == 7);
  assert((all.simplicity_options.initial_marking == std::vector<int>{0, 1, 2}));
  assert((all.simplicity_options.final_marking == std::vector<int>{3, 4}));
}

void parse_evaluation_params_rejects_invalid_and_explicit_empty_values() {
  for (const auto& item : {
           std::pair{"evaluation_family", "unknown"},
           std::pair{"fitness_variant", "token"},
           std::pair{"precision_variant", "align"},
           std::pair{"precision_use_task_ids", "yes"},
           std::pair{"precision_max_states", "0"},
           std::pair{"precision_max_states", "184467440737095516160"},
           std::pair{"generalization_variant", "token"},
           std::pair{"simplicity_variant", "cardoso"},
           std::pair{"simplicity_arc_degree_k", "-0.1"},
           std::pair{"simplicity_arc_degree_k", "nan"},
           std::pair{"simplicity_arc_degree_k", "inf"},
           std::pair{"simplicity_max_states", "0"},
           std::pair{"simplicity_max_states", "not-an-integer"},
           std::pair{"simplicity_initial_marking", "1,,2"},
           std::pair{"simplicity_initial_marking", "-1"},
           std::pair{"simplicity_initial_marking", "999999999999999999999999"},
           std::pair{"simplicity_final_marking", " , "}}) {
    bool threw = false;
    try {
      (void)minera::parse_evaluation_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  for (const auto& field_name : {"evaluation_family", "precision_use_task_ids",
                                 "simplicity_initial_marking"}) {
    const std::map<std::string, std::string> explicit_empty = {
        {field_name, ""}};
    const auto lookup = [&explicit_empty](std::string_view key) -> std::string {
      const auto it = explicit_empty.find(std::string(key));
      return it == explicit_empty.end() ? std::string{} : it->second;
    };
    const auto present = [&explicit_empty](std::string_view key) {
      return explicit_empty.contains(std::string(key));
    };
    bool threw = false;
    try {
      (void)minera::parse_evaluation_params(lookup, present);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  // Validation is independent of the selected family.
  bool threw = false;
  try {
    (void)minera::parse_evaluation_params(fields({
        {"evaluation_family", "precision"},
        {"simplicity_arc_degree_k", "nan"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void native_json_contracts() {
  minera::DecomposedAlignmentResult d; d.diagnostics.note = "ok";
  minera::DecomposedAlignmentTrace dt;
  dt.activities = {"A"}; dt.complete = true; dt.cost = 1.0; dt.fitness = 0.9;
  dt.best_worst_cost = 2.0;
  dt.alignment.push_back({std::string("A"), std::string("A"), 0});
  d.traces.push_back(dt);
  const auto dj = minera::decomposed_alignment_json(d);
  assert(dj.find("\"algorithm\":\"decomposed_alignment\"") != std::string::npos &&
         dj.find("\"diagnostics\"") != std::string::npos && dj.find("\"moves\"") != std::string::npos);
  minera::PetriAntiAlignmentResult a; a.activities = {"A"}; a.transitions = {2};
  const auto aj = minera::petri_anti_alignment_json(a);
  assert(aj.find("petri_anti_alignment") != std::string::npos && aj.find("precision") != std::string::npos);
  minera::PetriMultiAlignmentResult m; m.activities = {"A"};
  const auto mj = minera::petri_multi_alignment_json(m);
  assert(mj.find("petri_multi_alignment") != std::string::npos && mj.find("max_distance_to_log") != std::string::npos);
  minera::DirectlyFollowsGraph graph;
  minera::ProcessTree tree({}, minera::ProcessTreeNode::tau());
  const auto rj = minera::process_tree_reduction_json(tree, graph, 3);
  assert(rj.find("process_tree_reduction") != std::string::npos);
  assert(rj.find("\"trace_index\":3") != std::string::npos && rj.find("\"process_tree\"") != std::string::npos);
  minera::EventLog log;
  log.traces.push_back({"case-1", {{"case-1", "A", "", "", 0, {}, ""}}, {}});
  minera::LabelSplittingResult split;
  split.log = log;
  const auto sj = minera::label_splitting_json(split);
  assert(sj.find("\"family\":\"LABEL_SPLITTING\"") != std::string::npos &&
         sj.find("\"log\":") != std::string::npos);
}

void parse_dfg_and_edit_alignment_params_contract() {
  const auto d = minera::parse_dfg_alignment_params(fields({
      {"dfg_variant", "CLASSIC"}, {"dfg_max_states", "0"},
      {"dfg_max_time_ms", "12"}, {"dfg_sync_cost", R"(A=1,B:2,A=3,A\,B=4,A\=B=5,A\:B=6,A\\B=7,\sA=8,A\s=9,A\tB=10)"},
      {"dfg_model_move_cost", "A=4"}, {"dfg_log_move_cost", "B=5"},
      {"dfg_internal_log_move_cost", "A=0.25"}}));
  assert(d.max_visited_states == 0 && d.max_time_ms == 12);
  assert(d.sync_cost.at("A") == 3.0 && d.sync_cost.at("B") == 2.0);
  assert(d.sync_cost.at("A,B") == 4.0 && d.sync_cost.at("A=B") == 5.0);
  assert(d.sync_cost.at("A:B") == 6.0 && d.sync_cost.at("A\\B") == 7.0);
  assert(d.sync_cost.at(" A") == 8.0 && d.sync_cost.at("A ") == 9.0);
  assert(d.sync_cost.at("A\tB") == 10.0);
  assert(d.model_move_cost.at("A") == 4.0 && d.log_move_cost.at("B") == 5.0);
  assert(d.internal_log_move_cost.at("A") == 0.25);
  const auto e = minera::parse_edit_distance_params(fields({
      {"edit_variant", "Variants.EDIT_DISTANCE"}, {"anti", "1"},
      {"deviation_cost", "7"}}));
  assert(e.perform_anti_alignment && e.deviation_cost == 7);

  const auto expect_throw = [](std::map<std::string, std::string> values,
                               bool dfg) {
    bool threw = false;
    try { if (dfg) (void)minera::parse_dfg_alignment_params(fields(std::move(values)));
          else (void)minera::parse_edit_distance_params(fields(std::move(values))); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  };
  expect_throw({{"dfg_variant", "bogus"}}, true);
  expect_throw({{"dfg_sync_cost", "=1"}}, true);
  expect_throw({{"dfg_sync_cost", "A=-1"}}, true);
  expect_throw({{"dfg_sync_cost", "A=nan"}}, true);
  expect_throw({{"dfg_sync_cost", "A=inf"}}, true);
  expect_throw({{"dfg_sync_cost", R"(A\q=1)"}}, true);
  expect_throw({{"dfg_sync_cost", R"(A\)"}}, true);
  expect_throw({{"edit_variant", "EDIT_DISTANCE"}, {"anti", "maybe"}}, false);
  expect_throw({{"deviation_cost", "0"}}, false);
  expect_throw({{"deviation_cost", "-2"}}, false);
  const std::map<std::string, std::string> empty{{"dfg_log_move_cost", ""}};
  const auto lookup = fields(empty);
  bool threw = false;
  try { (void)minera::parse_dfg_alignment_params(lookup, [](std::string_view n) { return n == "dfg_log_move_cost"; }); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void parse_mine_params_rejects_unknown_heuristic_settings() {
  for (const auto& [name, value] :
       std::vector<std::pair<std::string, std::string>>{
           {"heuristic_variant", "bogus"},
           {"heuristic_decoration", "bogus"},
           {"heuristic_performance_aggregation", "bogus"},
           {"heuristic_keep_once_per_case", "maybe"},
           {"heuristic_most_common_paths", "A-B"},
           {"dependency_threshold", "not-a-number"},
           {"min_edge_frequency", "-1"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({{name, value}}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_maps_spectrum_and_batch_options() {
  const auto params = minera::parse_mine_params(fields({
      {"spectrum_activities", " A, B "},
      {"spectrum_variant", "disconnected"},
      {"spectrum_sample_size", "17"},
      {"spectrum_sort_log", "false"},
      {"batches", "true"},
      {"batch_merge_distance_seconds", "12.5"},
      {"batch_min_size", "3"},
      {"batch_start_timestamp_attribute", "started_at"},
  }));
  assert((params.spectrum_activities == std::vector<std::string>{"A", "B"}));
  assert(params.spectrum.variant == minera::PerformanceSpectrumVariant::kDisconnected);
  assert(params.spectrum.sample_size == 17);
  assert(!params.spectrum.sort_log);
  assert(params.batches);
  assert(params.batch_options.merge_distance_seconds == 12.5);
  assert(params.batch_options.min_batch_size == 3);
  assert(params.batch_options.start_timestamp_attribute == "started_at");
}

void parse_mine_params_rejects_invalid_spectrum_and_batch_options() {
  for (const auto& item : {
           std::pair{"spectrum_variant", "other"},
           std::pair{"spectrum_activities", "only_one"},
           std::pair{"spectrum_sample_size", "0"},
           std::pair{"spectrum_sort_log", "yes"},
           std::pair{"batches", "on"},
           std::pair{"batch_merge_distance_seconds", "nan"},
           std::pair{"batch_merge_distance_seconds", "-1"},
           std::pair{"batch_min_size", "1"},
           std::pair{"batch_min_size", "no"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  const std::map<std::string, std::string> empty = {{"spectrum_activities", ""}};
  const auto lookup = [&empty](std::string_view key) -> std::string {
    const auto it = empty.find(std::string(key));
    return it == empty.end() ? std::string{} : it->second;
  };
  const auto present = [&empty](std::string_view key) {
    return empty.contains(std::string(key));
  };
  bool threw = false;
  try {
    (void)minera::parse_mine_params(lookup, present);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_dependent_options_without_opt_in() {
  bool threw = false;
  for (const auto& item : {
           std::pair{"spectrum_variant", "connected"},
           std::pair{"spectrum_sample_size", "10"},
           std::pair{"spectrum_sort_log", "true"},
           std::pair{"batch_merge_distance_seconds", "1"},
           std::pair{"batch_min_size", "2"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  const std::map<std::string, std::string> explicit_empty = {
      {"batch_start_timestamp_attribute", ""}};
  const auto lookup = [&explicit_empty](std::string_view key) -> std::string {
    const auto it = explicit_empty.find(std::string(key));
    return it == explicit_empty.end() ? std::string{} : it->second;
  };
  const auto present = [&explicit_empty](std::string_view key) {
    return explicit_empty.contains(std::string(key));
  };
  threw = false;
  try {
    (void)minera::parse_mine_params(lookup, present);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"batches", "false"}, {"batch_min_size", "2"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_petri_analysis_params_maps_all_families_and_settings() {
  const auto parsed = minera::parse_petri_analysis_params(fields({
      {"petri_analysis_family", "extended_marking_equation"},
      {"petri_analysis_miner", "powl"},
      {"petri_analysis_trace_index", "3"},
      {"petri_analysis_initial_marking", "1, 0"},
      {"petri_analysis_final_marking", "0, 1"},
      {"petri_analysis_model_costs", "4,5"},
      {"petri_analysis_sync_costs", "0,1"},
      {"petri_analysis_log_costs", "9,10"},
      {"petri_analysis_split_indices", "1, 3"},
      {"petri_analysis_max_k", "2"},
      {"petri_analysis_max_states", "42"},
      {"petri_analysis_full_bootstrap", "false"},
      {"petri_analysis_diagnostics", "true"}}));
  assert(parsed.family == minera::PetriAnalysisFamily::kExtendedMarkingEquation);
  assert(parsed.miner == "powl" && parsed.trace_index == 3);
  assert(parsed.initial_marking == minera::Marking({1, 0}));
  assert(parsed.final_marking == minera::Marking({0, 1}));
  assert(parsed.model_transition_costs == std::vector<double>({4, 5}));
  assert(parsed.synchronous_transition_costs == std::vector<double>({0, 1}));
  assert(parsed.log_move_costs == std::vector<double>({9, 10}));
  assert(parsed.split_indices == std::vector<std::size_t>({1, 3}));
  assert(parsed.max_k_value == 2 && parsed.max_states == 42);
  assert(!parsed.full_bootstrap_required && parsed.diagnostics);
}

void parse_petri_analysis_params_rejects_bad_values() {
  for (const auto& item : {
           std::pair{"petri_analysis_family", "bad"},
           std::pair{"petri_analysis_max_states", "0"},
           std::pair{"petri_analysis_model_costs", "1,-1"}}) {
    bool threw = false;
    try {
      (void)minera::parse_petri_analysis_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  const std::map<std::string, std::string> empty_split = {
      {"petri_analysis_split_indices", ""}};
  bool threw = false;
  try {
    const auto lookup = [&empty_split](std::string_view key) {
      const auto it = empty_split.find(std::string(key));
      return it == empty_split.end() ? std::string{} : it->second;
    };
    const auto present = [&empty_split](std::string_view key) {
      return empty_split.contains(std::string(key));
    };
    (void)minera::parse_petri_analysis_params(lookup, present);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_process_tree_playout_params_maps_variants_and_limits() {
  const auto parsed = minera::parse_process_tree_playout_params(fields({
      {"playout_variant", "TOPBOTTOM"},
      {"playout_traces", "12"},
      {"playout_max_steps", "99"},
      {"playout_seed", "7"},
      {"playout_min_length", "2"},
      {"playout_max_length", "8"},
      {"playout_max_loop_occurrences", "4"},
      {"playout_max_trace_cap", "50"}}));
  assert(parsed.variant == minera::ProcessTreePlayoutVariant::kTopBottom);
  assert(parsed.traces == 12 && parsed.max_steps == 99 && parsed.seed == 7);
  assert(parsed.min_length == 2 && parsed.max_length == 8);
  assert(parsed.max_loop_occurrences == 4 && parsed.max_traces == 50);
}

void parse_process_tree_playout_params_rejects_bad_values() {
  for (const auto& item : {
           std::pair{"playout_variant", "not-a-variant"},
           std::pair{"playout_traces", "0"},
           std::pair{"playout_max_steps", "-1"},
           std::pair{"playout_max_trace_cap", "0"}}) {
    bool threw = false;
    try {
      (void)minera::parse_process_tree_playout_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_process_tree_playout_params(fields({
        {"playout_min_length", "4"}, {"playout_max_length", "2"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_dfg_playout_params_maps_controls_and_aliases() {
  const auto parsed = minera::parse_dfg_playout_params(fields({
      {"dfg_playout_variant", "performance"},
      {"dfg_playout_max_variants", "12"},
      {"dfg_playout_min_weighted_probability", "0.25"},
      {"dfg_playout_max_occurrences_per_activity", "4"},
      {"dfg_playout_interrupt_when_dfg_complete", "true"},
      {"dfg_playout_add_trace_if_takes_new_edges", "1"},
      {"dfg_playout_max_execution_time_ms", "500"},
      {"dfg_playout_return_variants", "true"},
      {"dfg_playout_return_only_if_complete", "false"},
      {"dfg_playout_min_variant_occurrences", "2"},
      {"dfg_playout_traces", "20"},
      {"dfg_playout_case_arrival_rate", "1.5"},
      {"dfg_playout_seed", "7"},
      {"dfg_playout_max_steps", "99"},
      {"dfg_playout_start_time", "2026-01-01T00:00:00Z"}}));
  assert(parsed.variant == minera::DfgPlayoutVariant::kPerformance);
  assert(parsed.max_variants == 12 && parsed.min_weighted_probability == 0.25);
  assert(parsed.max_occurrences_per_activity == 4 &&
         parsed.interrupt_when_dfg_complete &&
         parsed.add_trace_if_takes_new_edges);
  assert(parsed.max_execution_time_ms == 500 && parsed.return_variants &&
         !parsed.return_only_if_complete && parsed.min_variant_occurrences == 2);
  assert(parsed.traces == 20 && parsed.case_arrival_rate == 1.5 &&
         parsed.seed == 7 && parsed.max_events_per_trace == 99);
  assert(parsed.start_time == "2026-01-01T00:00:00Z");
}

void parse_dfg_playout_params_rejects_invalid_bounds() {
  for (const auto& item : {std::pair{"dfg_playout_variant", "other"},
                           std::pair{"dfg_playout_traces", "0"},
                           std::pair{"dfg_playout_max_variants", "0"},
                           std::pair{"dfg_playout_min_weighted_probability", "2"},
                           std::pair{"dfg_playout_case_arrival_rate", "-1"},
                           std::pair{"dfg_playout_return_variants", "maybe"}}) {
    bool threw = false;
    try {
      (void)minera::parse_dfg_playout_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_stochastic_playout_params_maps_controls() {
  const auto parsed = minera::parse_stochastic_playout_params(fields({
      {"stochastic_traces", "12"},
      {"stochastic_max_steps", "99"},
      {"stochastic_seed", "7"},
      {"stochastic_transition_weights", "0.25, 2, 0"},
      {"stochastic_stop_at_final", "false"},
      {"stochastic_start", "1767225600"},
      {"stochastic_case_interval", "2.5"},
      {"stochastic_event_interval", "0.25"}}));
  assert(parsed.traces == 12 && parsed.max_steps == 99 && parsed.seed == 7);
  assert(parsed.transition_weights == std::vector<double>({0.25, 2, 0}));
  assert(!parsed.stop_at_final && parsed.start == 1767225600.0);
  assert(parsed.case_interval == 2.5 && parsed.event_interval == 0.25);
}

void parse_stochastic_playout_params_rejects_bad_values() {
  for (const auto& item : {
           std::pair{"stochastic_traces", "0"},
           std::pair{"stochastic_max_steps", "-1"},
           std::pair{"stochastic_transition_weights", "1,-0.1"},
           std::pair{"stochastic_transition_weights", "1,"},
           std::pair{"stochastic_stop_at_final", "maybe"},
           std::pair{"stochastic_event_interval", "-1"}}) {
    bool threw = false;
    try {
      (void)minera::parse_stochastic_playout_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void log_to_features_params_contract() {
  const auto parsed = minera::parse_log_to_features_params(fields({
      {"features_variant", "temporal"},
      {"features_activity_key", "concept:name"},
      {"features_window_seconds", "60"},
      {"features_event_numeric_attributes", "cost,amount"},
      {"features_feature_names", "arrivals,finishes"}}));
  assert(parsed.variant == minera::LogToFeaturesVariant::TEMPORAL);
  assert(parsed.activity_key == "concept:name" && parsed.window_seconds == 60.0);
  assert(parsed.event_numeric_attributes.size() == 2 && parsed.feature_names.size() == 2);
  bool threw = false;
  try { (void)minera::parse_log_to_features_params(fields({{"features_variant", "bogus"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_log_to_features_params(fields({{"features_window_seconds", "0"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void label_splitting_params_contract() {
  const auto parsed = minera::parse_label_splitting_params(fields({
      {"label_splitting_variant", "contextual"},
      {"label_splitting_activity_key", "concept:name"},
      {"label_splitting_target_key", "split"},
      {"label_splitting_prefix_length", "3"},
      {"label_splitting_suffix_length", "4"},
      {"label_splitting_min_edge_weight", "0.25"},
      {"label_splitting_activities_suffix", "#"},
      {"label_splitting_target_activities", "A,B"}}));
  assert(parsed.variant == minera::LabelSplittingVariant::CONTEXTUAL);
  assert(parsed.activity_key == "concept:name" && parsed.target_key == "split");
  assert(parsed.prefix_length == 3 && parsed.suffix_length == 4);
  assert(parsed.min_edge_weight == 0.25 && parsed.activities_suffix == "#");
  assert((parsed.target_activities == std::vector<std::string>{"A", "B"}));
  bool threw = false;
  try { (void)minera::parse_label_splitting_params(fields({{"label_splitting_variant", "other"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_label_splitting_params(fields({{"label_splitting_prefix_length", "-1"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_label_splitting_params(fields({{"label_splitting_target_activities", "A,A"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void ilp_params_contract() {
  const auto parsed = minera::parse_ilp_miner_params(fields({
      {"ilp_activity_key", "concept:name"},
      {"ilp_noise_threshold", "0.25"},
      {"ilp_max_nodes", "17"}}));
  assert(parsed.activity_key == "concept:name");
  assert(parsed.noise_threshold == 0.25);
  assert(parsed.max_nodes == 17);
  for (const auto& item : {std::pair{"ilp_noise_threshold", "-0.1"},
                           std::pair{"ilp_noise_threshold", "1.1"},
                           std::pair{"ilp_max_nodes", "0"},
                           std::pair{"ilp_activity_key", ""}}) {
    bool threw = false;
    const auto lookup = fields({item});
    const auto present = [name = item.first](std::string_view key) {
      return key == name;
    };
    try { (void)minera::parse_ilp_miner_params(lookup, present); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void genetic_params_contract() {
  const auto parsed = minera::parse_genetic_miner_params(fields({
      {"genetic_activity_key", "concept:name"},
      {"genetic_seed", "19"},
      {"genetic_population_size", "12"},
      {"genetic_generations", "7"},
      {"genetic_elitism_rate", "0.2"},
      {"genetic_crossover_rate", "0.8"},
      {"genetic_mutation_rate", "0.05"},
      {"genetic_elitism_min_sample", "4"},
      {"genetic_max_activities", "9"},
      {"genetic_max_states", "77"}}));
  assert(parsed.activity_key == "concept:name" && parsed.seed == 19);
  assert(parsed.population_size == 12 && parsed.generations == 7);
  assert(parsed.elitism_rate == 0.2 && parsed.crossover_rate == 0.8);
  assert(parsed.mutation_rate == 0.05 && parsed.elitism_min_sample == 4);
  assert(parsed.max_activities == 9 && parsed.max_states == 77);
  for (const auto& item : {
           std::pair{"genetic_population_size", "1"},
           std::pair{"genetic_generations", "-1"},
           std::pair{"genetic_elitism_rate", "nan"},
           std::pair{"genetic_crossover_rate", "1.1"},
           std::pair{"genetic_mutation_rate", "-0.1"},
           std::pair{"genetic_elitism_min_sample", "32"},
           std::pair{"genetic_max_activities", "0"},
           std::pair{"genetic_max_states", "0"},
           std::pair{"genetic_seed", "-1"}}) {
    bool threw = false;
    const auto lookup = fields({item});
    const auto present = [name = item.first](std::string_view key) {
      return key == name;
    };
    try { (void)minera::parse_genetic_miner_params(lookup, present); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void organizational_sna_params_contract() {
  const auto parsed = minera::parse_organizational_sna_params(fields({
      {"sna_variant", "working_together"},
      {"sna_beta", "0.25"},
      {"sna_n", "4"},
      {"sna_metric_normalization", "true"},
      {"sna_max_resources", "20"},
      {"sna_max_edges", "80"}}));
  assert(parsed.variant == minera::OrganizationalSnaVariant::kWorkingTogether);
  assert(parsed.beta == 0.25 && parsed.n == 4 && parsed.metric_normalization);
  assert(parsed.max_resources == 20 && parsed.max_edges == 80);
  for (const auto& item : {std::pair{"sna_variant", "bad"},
                           std::pair{"sna_beta", "1.1"},
                           std::pair{"sna_beta", "nan"},
                           std::pair{"sna_n", "0"},
                           std::pair{"sna_metric_normalization", "maybe"},
                           std::pair{"sna_max_edges", "0"}}) {
    bool threw = false;
    const auto lookup = fields({item});
    const auto present = [name = item.first](std::string_view key) {
      return key == name;
    };
    try { (void)minera::parse_organizational_sna_params(lookup, present); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void trace_variant_query_params_contract() {
  const auto parsed = minera::parse_trace_variant_query_params(fields({
      {"tvq_variant", "sacofa"},
      {"tvq_epsilon", "0.75"},
      {"tvq_k", "4"},
      {"tvq_p", "2"},
      {"tvq_seed", "19"},
      {"tvq_max_output_traces", "40"},
      {"tvq_max_output_variants", "7"}}));
  assert(parsed.variant == minera::TraceVariantQueryVariant::kSacofa);
  assert(parsed.epsilon == 0.75 && parsed.max_prefix_length == 4);
  assert(parsed.prune_threshold == 2 && parsed.seed == 19);
  assert(parsed.max_output_traces == 40 && parsed.max_output_variants == 7);
  for (const auto& item : {std::pair{"tvq_variant", "other"},
                           std::pair{"tvq_epsilon", "0"},
                           std::pair{"tvq_k", "-1"},
                           std::pair{"tvq_p", "0"}}) {
    bool threw = false;
    try { (void)minera::parse_trace_variant_query_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void pripel_params_contract() {
  const auto parsed = minera::parse_pripel_params(fields({
      {"pripel_epsilon", "0.5"},
      {"pripel_blocklist", "case_id,resource"},
      {"pripel_seed", "8"},
      {"pripel_max_output_traces", "20"},
      {"pripel_max_distance", "2"}}));
  assert(parsed.epsilon == 0.5);
  assert((parsed.blocklist == std::vector<std::string>{"case_id", "resource"}));
  assert(parsed.seed == 8 && parsed.max_output_traces == 20 &&
         parsed.max_distance == 2);
  for (const auto& item : {std::pair{"pripel_epsilon", "0"},
                           std::pair{"pripel_blocklist", "a,a"},
                           std::pair{"pripel_seed", "-1"},
                           std::pair{"pripel_max_distance", "-1"}}) {
    bool threw = false;
    try { (void)minera::parse_pripel_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

}  // namespace

int main() {
  ends_with_ci_is_case_insensitive();
  looks_like_xes_uses_extension_then_content_sniff();
  split_csv_trims_and_drops_empty_parts();
  error_json_escapes_quotes_and_backslashes();
  parse_mine_params_defaults_when_empty();
  parse_mine_params_maps_correlation_options();
  parse_mine_params_preserves_explicit_empty_correlation_timestamp();
  parse_mine_params_rejects_invalid_correlation_options();
  parse_mine_params_maps_csv_and_inductive_options();
  parse_mine_params_maps_filters();
  parse_mine_params_rejects_an_unknown_time_mode();
  parse_mine_params_rejects_an_unknown_attribute_mode();
  parse_mine_params_rejects_a_malformed_succession();
  parse_mine_params_rejects_an_unknown_alpha_variant();
  parse_mine_params_rejects_an_unknown_inductive_variant();
  parse_mine_params_rejects_unknown_heuristic_settings();
  parse_mine_params_maps_spectrum_and_batch_options();
  parse_mine_params_rejects_invalid_spectrum_and_batch_options();
  parse_mine_params_rejects_dependent_options_without_opt_in();
  parse_petri_analysis_params_maps_all_families_and_settings();
  parse_petri_analysis_params_rejects_bad_values();
  parse_process_tree_playout_params_maps_variants_and_limits();
  parse_process_tree_playout_params_rejects_bad_values();
  parse_dfg_playout_params_maps_controls_and_aliases();
  parse_dfg_playout_params_rejects_invalid_bounds();
  parse_stochastic_playout_params_maps_controls();
  parse_stochastic_playout_params_rejects_bad_values();
  parse_mine_params_ignores_blank_fields();
  parse_mine_params_maps_transition_system_options();
  parse_mine_params_rejects_invalid_transition_fields();
  parse_mine_params_maps_powl_options();
  parse_mine_params_rejects_invalid_powl_fields();
  parse_mine_params_maps_token_replay_options();
  parse_mine_params_rejects_invalid_token_replay_fields();
  parse_mine_params_maps_log_skeleton_options();
  parse_mine_params_rejects_invalid_log_skeleton_fields();
  parse_mine_params_maps_declare_options();
  parse_mine_params_preserves_explicit_empty_declare_filters();
  parse_mine_params_rejects_invalid_declare_fields();
  parse_mine_params_maps_footprints_options();
  parse_mine_params_rejects_invalid_footprints_options();
  parse_mine_params_maps_dfg_variant_options();
  parse_mine_params_preserves_explicit_empty_dfg_attributes();
  parse_mine_params_rejects_invalid_dfg_variant_options();
  parse_alignment_params_maps_variants_and_costs();
  parse_alignment_params_rejects_invalid_and_dependent_values();
  parse_dfg_and_edit_alignment_params_contract();
  parse_process_tree_alignment_params_maps_variants_and_limits();
  parse_process_tree_alignment_params_rejects_bad_values();
  parse_native_adapter_params_contract();
  parse_evaluation_params_defaults_and_variants();
  parse_evaluation_params_rejects_invalid_and_explicit_empty_values();
  native_json_contracts();
  concept_drift_params_contract();
  log_to_target_params_contract();
  trie_params_contract();
  interval_tree_params_contract();
  case_relations_params_contract();
  random_tree_params_contract();
  log_to_features_params_contract();
  label_splitting_params_contract();
  ilp_params_contract();
  genetic_params_contract();
  organizational_sna_params_contract();
  trace_variant_query_params_contract();
  pripel_params_contract();
  return 0;
}
