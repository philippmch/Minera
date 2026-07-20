// Tests for the server's request-parsing layer (tools/minera_server feeds these
// the raw form fields). Kept httplib-free so the parsing — importer selection,
// CSV-list splitting, error escaping, and option mapping — can be exercised
// directly without standing up an HTTP server.
#undef NDEBUG

#include "minera/request_params.hpp"

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
  return 0;
}
