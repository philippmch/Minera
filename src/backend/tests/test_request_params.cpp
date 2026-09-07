// Tests for the server's request-parsing layer (src/backend/tools/minera_server
// feeds these
// the raw form fields). Kept httplib-free so the parsing — importer selection,
// CSV-list splitting, error escaping, and option mapping — can be exercised
// directly without standing up an HTTP server.
#undef NDEBUG

#include "minera/request_params.hpp"
#include "minera/json_export.hpp"
#include "minera/directly_follows_graph.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

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
  assert(minera::looks_like_xes("trace.XES.GZ", "anything"));
  assert(minera::looks_like_xes("trace.bin", "\x1f\x8bpayload"));
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
  assert(params.inductive.activity_key == "concept:name");
  assert(params.inductive.timestamp_key == "time:timestamp");
  assert(params.inductive.case_id_key == "case:concept:name");
  assert(!params.inductive.sort_events_by_timestamp);
  assert(!params.inductive.disable_strict_sequence_cut);
  assert(!params.inductive.disable_fallthroughs);
  assert(!params.inductive.multiprocessing);
  assert(!params.alpha.short_loops);  // classic Alpha is the default variant
  assert(params.alpha.activity_attribute == "concept:name");
  assert(!params.alpha.remove_unconnected);
  assert(params.powl.activity_key == "concept:name");
  assert(params.powl.timestamp_key == "time:timestamp");
  assert(params.powl.case_id_key == "case:concept:name");
  assert(!params.token_replay_activity_attribute_supplied);
  const auto explicit_plus = minera::parse_mine_params(
      fields({{"alpha_variant", "plus"}}));
  assert(explicit_plus.alpha.short_loops);
  assert(params.correlation_variants.empty());
  assert(params.correlation.activity_attribute == "concept:name");
  assert(params.correlation.timestamp_attribute == "time:timestamp");
  assert(params.correlation.start_timestamp_attribute.empty());
  assert(params.correlation.case_id_attribute == "case:concept:name");
  assert(!params.correlation.exact_time_matching);
  assert(params.correlation.sample_size == 100000);
  assert(params.spectrum_activities.empty());
  assert(params.spectrum.variant == minera::PerformanceSpectrumVariant::kConnected);
  assert(params.spectrum.sample_size == 10000);
  assert(params.spectrum.sort_log);
  assert(!params.temporal_profile.strict_attributes);
  assert(!params.batches);
  assert(params.batch_options.merge_distance_seconds == 900.0);
  assert(params.batch_options.min_batch_size == 2);
  assert(!params.ltl_enabled && !params.ltl);
  assert(!params.segment_enabled && !params.segment);
  assert(!params.path_enabled && !params.path);
}

void adjacent_path_filter_params_contract() {
  const auto params = minera::parse_mine_params(fields({
      {"path_predecessor", "A"},
      {"path_successor", "B"},
      {"path_min_performance", "0"},
      {"path_max_performance", "2.5"},
      {"path_positive", "false"},
      {"path_activity_attribute", "phase"},
      {"path_timestamp_attribute", "occurred"},
  }));
  assert(params.path_enabled && params.path);
  assert(params.path->predecessor == "A");
  assert(params.path->successor == "B");
  assert(params.path->min_elapsed_seconds && *params.path->min_elapsed_seconds == 0.0);
  assert(params.path->max_elapsed_seconds && *params.path->max_elapsed_seconds == 2.5);
  assert(!params.path->positive);
  assert(params.path->activity_attribute == "phase");
  assert(params.path->timestamp_attribute == "occurred");
  for (const auto& item : {
           std::pair{"path_predecessor", ""},
           std::pair{"path_successor", ""},
           std::pair{"path_min_performance", "nan"},
           std::pair{"path_max_performance", "inf"},
           std::pair{"path_positive", "maybe"},
       }) {
    std::map<std::string, std::string> values{{"path_predecessor", "A"},
                                               {"path_successor", "B"}};
    values[item.first] = item.second;
    bool threw = false;
    try { (void)minera::parse_mine_params(fields(std::move(values))); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
  for (const auto name : {"path_activity_attribute", "path_timestamp_attribute"}) {
    const std::map<std::string, std::string> values{
        {"path_predecessor", "A"}, {"path_successor", "B"},
        {name, ""}};
    bool empty_threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view key) {
            return values.contains(std::string(key));
          });
    } catch (const std::invalid_argument&) { empty_threw = true; }
    assert(empty_threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"path_predecessor", "A"},
                                            {"path_successor", "B"},
                                            {"path_min_performance", "3"},
                                            {"path_max_performance", "2"}}));
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"path_predecessor", "A"},
                                            {"path_successor", "B"},
                                            {"path_max_performance", "-1"}}));
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void segment_filter_params_contract() {
  const auto params = minera::parse_mine_params(fields({
      {"segment_prefix_activity", "A"},
      {"segment_prefix_strict", "false"},
      {"segment_prefix_occurrence", "last"},
      {"segment_suffix_activity", "Z"},
      {"segment_suffix_occurrence", "first"},
      {"segment_between_starts", "A, B"},
      {"segment_between_ends", "Z"},
      {"segment_between_separator", "@@"},
  }));
  assert(params.segment_enabled && params.segment);
  assert(params.segment->use_prefix && params.segment->prefix.activity == "A");
  assert(!params.segment->prefix.strict);
  assert(params.segment->prefix.occurrence == minera::AnchorOccurrence::last);
  assert(params.segment->use_suffix && params.segment->suffix.activity == "Z");
  assert(params.segment->suffix.occurrence == minera::AnchorOccurrence::first);
  assert(params.segment->use_between);
  assert((params.segment->between.start_activities == std::vector<std::string>{"A", "B"}));
  assert(params.segment->between.case_id_separator == "@@");
  for (const auto& item : {
           std::pair{"segment_prefix_strict", "maybe"},
           std::pair{"segment_prefix_occurrence", "middle"},
           std::pair{"segment_between_starts", "A"},
           std::pair{"segment_between_separator", "@@"},
       }) {
    bool threw = false;
    try {
      std::map<std::string, std::string> values{{item.first, item.second}};
      (void)minera::parse_mine_params(fields(std::move(values)));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void ltl_filter_params_contract() {
  const auto params = minera::parse_mine_params(fields({
      {"ltl_variant", "eventually_follows"},
      {"ltl_attribute_key", "phase"},
      {"ltl_attribute_values", "A, B, C"},
      {"ltl_positive", "false"},
      {"ltl_enable_timestamp", "true"},
      {"ltl_timestamp_diff_boundaries", "0:2,3.5:9"},
      {"ltl_resource_key", "performer"},
  }));
  assert(params.ltl_enabled && params.ltl);
  assert(params.ltl->variant == minera::LtlFilterVariant::kEventuallyFollows);
  assert(params.ltl->attribute_key == "phase");
  assert((params.ltl->attribute_values ==
          std::vector<std::string>{"A", "B", "C"}));
  assert(!params.ltl->positive && params.ltl->enable_timestamp);
  assert(params.ltl->timestamp_diff_boundaries.size() == 2);
  assert(params.ltl->timestamp_diff_boundaries[1].first == 3.5);
  assert(params.ltl->resource_key == "performer");

  for (const auto& item : {
           std::pair{"ltl_variant", "unknown"},
           std::pair{"ltl_attribute_values", "A,,B"},
           std::pair{"ltl_timestamp_diff_boundaries", "2:1"},
           std::pair{"ltl_timestamp_diff_boundaries", "nan:1"},
       }) {
    bool threw = false;
    try { (void)minera::parse_mine_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"ltl_variant", "four_eyes_principle"},
        {"ltl_attribute_values", "A,B,C"}}));
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void resource_profiles_params_contract() {
  const auto p = minera::parse_resource_profiles_params(fields({
      {"resource_profiles_metric", "social_position"},
      {"resource_profiles_activity_attribute", "team:activity"},
      {"resource_profiles_resource_attribute", "team:resource"},
      {"resource_profiles_timestamp_attribute", "observed_at"},
      {"resource_profiles_start_timestamp_attribute", "started_at"},
      {"resource_profiles_case_id_attribute", "case:custom"},
      {"resource_profiles_t1", "2024-01-01"},
      {"resource_profiles_t2", "2024-01-02"},
      {"resource_profiles_resource", "alice"},
      {"resource_profiles_max_rows", "12"}}));
  assert(p.metric == minera::ResourceProfileMetric::kSocialPosition && p.resource == "alice" && p.max_rows == 12);
  assert(p.activity_attribute == "team:activity" &&
         p.resource_attribute == "team:resource" &&
         p.timestamp_attribute == "observed_at" &&
         p.start_timestamp_attribute == "started_at" &&
         p.case_id_attribute == "case:custom");
  try { (void)minera::parse_resource_profiles_params(fields({{"resource_profiles_metric", "bogus"}})); assert(false); } catch (const std::invalid_argument&) {}
  try { (void)minera::parse_resource_profiles_params(fields({{"resource_profiles_metric", "interaction_two_resources"}, {"resource_profiles_resource", "a"}})); assert(false); } catch (const std::invalid_argument&) {}
}

void clustering_params_contract() {
  const auto parsed = minera::parse_clustering_params(fields({
      {"clustering_trace_attribute", "segment"},
      {"clustering_activity_attribute", "phase"},
      {"clustering_variant", "DMM_VEC"},
      {"clustering_alpha", "0.25"},
      {"clustering_percent", "0.8"},
      {"clustering_max_groups", "12"},
      {"clustering_max_output_nodes", "24"}}));
  assert(parsed.trace_attribute == "segment");
  assert(parsed.activity_attribute == "phase");
  assert(parsed.variant == minera::TraceAttributeClusteringVariant::kDmmVec);
  assert(parsed.alpha == 0.25 && parsed.percent == 0.8);
  assert(parsed.max_groups == 12 && parsed.max_output_nodes == 24);
  for (const auto& item : {std::pair{"clustering_variant", "bad"},
                           std::pair{"clustering_alpha", "nan"},
                           std::pair{"clustering_alpha", "1.1"},
                           std::pair{"clustering_percent", "0"},
                           std::pair{"clustering_max_groups", "0"},
                           std::pair{"clustering_max_output_nodes", "0"}}) {
    bool threw = false;
    try { (void)minera::parse_clustering_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
  const auto empty_activity = fields({{"clustering_activity_attribute", ""}});
  bool empty_activity_threw = false;
  try {
    (void)minera::parse_clustering_params(
        empty_activity, [](std::string_view name) {
          return name == "clustering_activity_attribute";
        });
  } catch (const std::invalid_argument&) {
    empty_activity_threw = true;
  }
  assert(empty_activity_threw);
  bool missing = false;
  try { (void)minera::parse_clustering_params(fields({})); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
}

void profile_clustering_params_contract() {
  const auto parsed = minera::parse_profile_clustering_params(fields({
      {"profile_variant", "SKLEARN_PROFILES"},
      {"profile_activity_attribute", "phase"},
      {"profile_cluster_count", "3"},
      {"profile_max_iterations", "17"},
      {"profile_max_traces", "99"},
      {"profile_max_features", "123"},
      {"profile_seed", "42"},
      {"profile_tolerance", "0.001"},
      {"profile_include_activity", "false"},
      {"profile_include_succession", "1"},
      {"profile_trace_string_attributes", "segment,region"},
      {"profile_event_string_attributes", "phase"},
      {"profile_trace_numeric_attributes", "amount"},
      {"profile_event_numeric_attributes", "score"},
      {"profile_feature_names", "f1,f2"}}));
  assert(parsed.cluster_count == 3 && parsed.max_iterations == 17);
  assert(parsed.variant == minera::ProfileClusteringVariant::kSklearnProfiles);
  assert(parsed.activity_attribute == "phase");
  assert(parsed.max_traces == 99 && parsed.max_features == 123);
  assert(parsed.seed == 42 && parsed.tolerance == 0.001);
  assert(!parsed.include_activity_representation &&
         parsed.include_succession_representation);
  assert((parsed.trace_string_attributes ==
          std::vector<std::string>{"segment", "region"}));
  assert((parsed.event_string_attributes == std::vector<std::string>{"phase"}));
  assert((parsed.trace_numeric_attributes == std::vector<std::string>{"amount"}));
  assert((parsed.event_numeric_attributes == std::vector<std::string>{"score"}));
  assert((parsed.feature_names == std::vector<std::string>{"f1", "f2"}));

  for (const auto& item : {
           std::pair{"profile_cluster_count", "0"},
           std::pair{"profile_variant", "other"},
           std::pair{"profile_tolerance", "-0.1"},
           std::pair{"profile_include_activity", "maybe"},
           std::pair{"profile_event_string_attributes", "A,,B"}}) {
    bool threw = false;
    try { (void)minera::parse_profile_clustering_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
  const auto empty_activity = fields({{"profile_activity_attribute", ""}});
  bool empty_activity_threw = false;
  try {
    (void)minera::parse_profile_clustering_params(
        empty_activity, [](std::string_view name) {
          return name == "profile_activity_attribute";
        });
  } catch (const std::invalid_argument&) {
    empty_activity_threw = true;
  }
  assert(empty_activity_threw);
}

void chaotic_activities_params_contract() {
  const auto parsed = minera::parse_chaotic_activities_params(fields({
      {"chaotic_activity_key", "phase"},
      {"chaotic_alpha", "0.25"},
      {"chaotic_max_activities", "8"},
      {"chaotic_max_events", "100"}}));
  assert(parsed.activity_key == "phase" && parsed.alpha && *parsed.alpha == 0.25);
  assert(parsed.max_activities == 8 && parsed.max_events == 100);
  for (const auto& item : {std::pair{"chaotic_alpha", "nan"},
                           std::pair{"chaotic_alpha", "-1"},
                           std::pair{"chaotic_max_activities", "0"},
                           std::pair{"chaotic_max_events", "0"}}) {
    bool threw = false;
    try { (void)minera::parse_chaotic_activities_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
  const auto lookup = fields({{"chaotic_activity_key", ""}});
  const auto present = [](std::string_view name) {
    return name == "chaotic_activity_key";
  };
  bool empty_key_threw = false;
  try { (void)minera::parse_chaotic_activities_params(lookup, present); }
  catch (const std::invalid_argument&) { empty_key_threw = true; }
  assert(empty_key_threw);
}

void passed_time_params_contract() {
  const auto parsed = minera::parse_passed_time_params(fields({
      {"passed_time_activity", "B"},
      {"passed_time_variant", "prepost"},
      {"passed_time_activity_key", "phase"},
      {"passed_time_timestamp_key", "occurred_at"},
      {"passed_time_start_timestamp_key", "started_at"},
      {"passed_time_aggregation", "median"},
      {"passed_time_business_hours", "true"},
      {"passed_time_business_hour_slots", "0:3600,7200:10800"}}));
  assert(parsed.activity == "B");
  assert(parsed.variant == minera::PassedTimeVariant::kPrePost);
  assert(parsed.activity_key == "phase");
  assert(parsed.timestamp_key == "occurred_at");
  assert(parsed.start_timestamp_key == "started_at");
  assert(parsed.performance_aggregation ==
         minera::DfgPerformanceAggregation::kMedian);
  assert(parsed.use_business_hours && parsed.business_hour_slots.size() == 2);
  for (const auto& item : {std::pair{"passed_time_variant", "bad"},
                           std::pair{"passed_time_aggregation", "all"},
                           std::pair{"passed_time_business_hours", "maybe"}}) {
    bool threw = false;
    try {
      (void)minera::parse_passed_time_params(fields({
          {"passed_time_activity", "B"}, item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool missing = false;
  try { (void)minera::parse_passed_time_params(fields({})); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
}

void service_time_params_contract() {
  const auto parsed = minera::parse_service_time_params(fields({
      {"service_time_activity_key", "phase"},
      {"service_time_start_timestamp_key", "started_at"},
      {"service_time_timestamp_key", "finished_at"},
      {"service_time_aggregation", "sum"},
      {"service_time_business_hours", "true"},
      {"service_time_business_hour_slots", "0:3600"}}));
  assert(parsed.activity_key == "phase");
  assert(parsed.start_timestamp_key == "started_at");
  assert(parsed.timestamp_key == "finished_at");
  assert(parsed.aggregation == minera::DfgPerformanceAggregation::kSum);
  assert(parsed.use_business_hours && parsed.business_hour_slots.size() == 1);
  bool threw = false;
  try {
    (void)minera::parse_service_time_params(fields({
        {"service_time_aggregation", "raw_values"}}));
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void rework_params_contract() {
  const auto parsed = minera::parse_rework_params(fields({
      {"rework_variant", "cases"},
      {"rework_activity_key", "phase"},
      {"rework_case_id_key", "region"}}));
  assert(parsed.variant == minera::ReworkVariant::kCases);
  assert(parsed.activity_key == "phase" && parsed.case_id_key == "region");
  bool threw = false;
  try {
    (void)minera::parse_rework_params(fields({{"rework_variant", "bad"}}));
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void cycle_time_params_contract() {
  const auto options = minera::parse_cycle_time_params(fields({
      {"cycle_time_timestamp_key", "finished_at"},
      {"cycle_time_start_timestamp_key", "started_at"}}));
  assert(options.timestamp_key == "finished_at");
  assert(options.start_timestamp_key == "started_at");
  bool rejected = false;
  try {
    const auto lookup = fields({{"cycle_time_timestamp_key", ""}});
    const minera::FieldPresence present = [](std::string_view name) {
      return name == "cycle_time_timestamp_key";
    };
    (void)minera::parse_cycle_time_params(lookup, present);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void case_arrival_params_contract() {
  const auto options = minera::parse_case_arrival_params(fields({
      {"case_arrival_timestamp_key", "occurred_at"},
      {"case_arrival_business_hours", "true"},
      {"case_arrival_business_hour_slots", "28800-43200"}}));
  assert(options.timestamp_key == "occurred_at");
  assert(options.use_business_hours);
  assert(options.business_hour_slots.size() == 1);
  bool rejected = false;
  try {
    (void)minera::parse_case_arrival_params(fields({
        {"case_arrival_business_hours", "maybe"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void case_statistics_params_contract() {
  const auto options = minera::parse_case_statistics_params(fields({
      {"case_statistics_case_id_key", "case:concept:name"},
      {"case_statistics_timestamp_key", "occurred_at"},
      {"case_statistics_enable_sort", "true"},
      {"case_statistics_sort_by", "duration"},
      {"case_statistics_sort_ascending", "false"},
      {"case_statistics_max_cases", "3"},
      {"case_statistics_business_hours", "true"},
      {"case_statistics_business_hour_slots", "28800-43200"}}));
  assert(options.case_id_key == "case:concept:name");
  assert(options.timestamp_key == "occurred_at");
  assert(options.enable_sort &&
         options.sort_by == minera::CaseStatisticsSortBy::kDuration);
  assert(!options.sort_ascending && options.max_cases == 3);
  assert(options.use_business_hours && options.business_hour_slots.size() == 1);
  for (const auto& item : {
           std::pair{"case_statistics_sort_by", "bad"},
           std::pair{"case_statistics_max_cases", "-1"},
           std::pair{"case_statistics_enable_sort", "maybe"}}) {
    bool rejected = false;
    try {
      (void)minera::parse_case_statistics_params(fields({item}));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
}

void attribute_statistics_params_contract() {
  const auto options = minera::parse_attribute_statistics_params(fields({
      {"attribute_statistics_key", "region"},
      {"attribute_statistics_scope", "traces"},
      {"attribute_statistics_keep_once_per_case", "true"}}));
  assert(options.attribute_key == "region");
  assert(options.scope == minera::AttributeStatisticsScope::kTraces);
  assert(!options.keep_once_per_case);
  bool rejected = false;
  try {
    (void)minera::parse_attribute_statistics_params(fields({}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void attribute_distribution_params_contract() {
  const auto options = minera::parse_attribute_distribution_params(fields({
      {"attribute_distribution_type", "days_week"},
      {"attribute_distribution_timestamp_key", "occurred_at"}}));
  assert(options.distribution == minera::EventDistributionType::kDaysWeek);
  assert(options.timestamp_key == "occurred_at");
  bool rejected = false;
  try {
    (void)minera::parse_attribute_distribution_params(fields({
        {"attribute_distribution_type", "unknown"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void attribute_kde_params_contract() {
  const auto options = minera::parse_attribute_kde_params(fields({
      {"attribute_kde_key", "amount"},
      {"attribute_kde_variant", "date"},
      {"attribute_kde_graph_points", "17"},
      {"attribute_kde_points_to_sample", "31"}}));
  assert(options.attribute_key == "amount");
  assert(options.variant == minera::AttributeKdeVariant::kDate);
  assert(options.graph_points == 17 && options.points_to_sample == 31);
  for (const auto& item : {
           std::pair{"attribute_kde_variant", "other"},
           std::pair{"attribute_kde_graph_points", "0"},
           std::pair{"attribute_kde_points_to_sample", "-1"}}) {
    bool rejected = false;
    try {
      (void)minera::parse_attribute_kde_params(fields({
          {"attribute_kde_key", "amount"}, item}));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  bool rejected = false;
  try {
    (void)minera::parse_attribute_kde_params(fields({}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void boundary_activities_params_contract() {
  const auto options = minera::parse_boundary_activities_params(fields({
      {"boundary_activities_variant", "end"},
      {"boundary_activities_activity_key", "phase"}}));
  assert(options.variant == minera::BoundaryActivitiesVariant::kEnd);
  assert(options.activity_key == "phase");
  bool rejected = false;
  try {
    (void)minera::parse_boundary_activities_params(fields({
        {"boundary_activities_variant", "middle"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void variant_statistics_params_contract() {
  const auto options = minera::parse_variant_statistics_params(fields({
      {"variant_statistics_activity_key", "phase"},
      {"variant_statistics_max_variants", "3"}}));
  assert(options.activity_key == "phase");
  assert(options.max_variants == 3);
  bool rejected = false;
  try {
    (void)minera::parse_variant_statistics_params(fields({
        {"variant_statistics_max_variants", "-1"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void concept_drift_params_contract() {
  const auto p = minera::parse_concept_drift_params(fields({
      {"drift_variant", "BOSE"},
      {"drift_sub_log_size", "3"}, {"drift_window_size", "2"},
      {"drift_num_permutations", "5"}, {"drift_threshold", "0.25"},
      {"drift_max_no_change_points", "4"}, {"drift_seed", "9"},
      {"drift_include_timestamps", "true"},
      {"drift_activity_attribute", "event:type"},
      {"drift_timestamp_attribute", "started_at"},
      {"drift_case_id_attribute", "case:order"}}));
  assert(p.sub_log_size == 3 && p.window_size == 2 && p.num_permutations == 5);
  assert(p.variant == minera::ConceptDriftVariant::kBose);
  assert(p.threshold == 0.25 && p.max_no_change_points == 4 && p.seed == 9 && p.include_timestamps);
  assert(p.activity_attribute == "event:type" &&
         p.timestamp_attribute == "started_at" &&
         p.case_id_attribute == "case:order");
  bool threw = false;
  try { (void)minera::parse_concept_drift_params(fields({{"drift_threshold", "2"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_concept_drift_params(fields({{"drift_variant", "other"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void browser_history_params_contract() {
  const auto chrome = minera::parse_browser_history_params(fields({
      {"browser_history_variant", "chrome_history"},
      {"browser_history_path", "history.db"},
      {"browser_history_max_events", "42"}}));
  assert(chrome.variant == minera::BrowserHistoryVariant::kChrome);
  assert(chrome.history_path == "history.db" && chrome.max_events == 42);
  const auto firefox = minera::parse_browser_history_params(fields({
      {"browser_history_variant", "firefox"},
      {"browser_history_path", "profiles"}}));
  assert(firefox.variant == minera::BrowserHistoryVariant::kFirefox);
  bool threw = false;
  try { (void)minera::parse_browser_history_params(fields({})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_browser_history_params(fields({
      {"browser_history_variant", "other"},
      {"browser_history_path", "x"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void repository_history_params_contract() {
  const auto parsed = minera::parse_repository_history_params(fields({
      {"repository_owner", "acme"}, {"repository_name", "engine"},
      {"repository_api_base_url", "https://api.example.test"},
      {"repository_max_pages", "12"}, {"repository_max_issues", "34"},
      {"repository_max_events", "56"}}));
  assert(parsed.owner == "acme" && parsed.repository == "engine");
  assert(parsed.api_base_url == "https://api.example.test");
  assert(parsed.max_pages == 12 && parsed.max_issues == 34 && parsed.max_events == 56);
  bool rejected = false;
  try { (void)minera::parse_repository_history_params(fields({})); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void workflow_history_params_contract() {
  const auto parsed = minera::parse_workflow_history_params(
      fields({{"workflow_max_events", "123"}}));
  assert(parsed.max_events == 123);
  bool rejected = false;
  try { (void)minera::parse_workflow_history_params(fields({{"workflow_max_events", "0"}})); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}
void enterprise_history_params_contract() {
  const auto parsed = minera::parse_enterprise_history_params(fields({
      {"enterprise_variant", "accounting"}, {"enterprise_max_events", "45"}}));
  assert(parsed.variant == minera::EnterpriseHistoryVariant::kAccounting && parsed.max_events == 45);
  bool rejected = false;
  try { (void)minera::parse_enterprise_history_params(fields({{"enterprise_variant", "other"}})); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}
void communication_history_params_contract() {
  const auto parsed = minera::parse_communication_history_params(fields({{"communication_variant", "calendar"}, {"communication_include_reminders", "false"}, {"communication_current_time", "2024-01-01T00:00:00Z"}, {"communication_max_items", "2"}, {"communication_max_events", "8"}}));
  assert(parsed.variant == minera::CommunicationHistoryVariant::kCalendar && !parsed.include_reminders && parsed.max_items == 2 && parsed.max_events == 8);
}

void windows_event_log_params_contract() {
  const auto parsed = minera::parse_windows_event_log_params(fields({
      {"windows_events_path", "events.xml"},
      {"windows_events_max_events", "20"}}));
  assert(parsed.path == "events.xml" && parsed.max_events == 20);
  bool threw = false;
  try { (void)minera::parse_windows_event_log_params(fields({})); }
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
      {"interval_query_point", "12.5"},
      {"interval_query_start", "10"},
      {"interval_query_end", "20"},
  }));
  assert(p.activity_key == "phase");
  assert(p.start_timestamp_key == "started_at");
  assert(p.timestamp_key == "completed_at");
  assert(p.epsilon == 0.25);
  assert(p.filter_activity_couple);
  assert(p.filter_activity_couple->first == "Review, senior");
  assert(p.filter_activity_couple->second == "A\\B");
  assert(p.query_point && *p.query_point == 12.5);
  assert(p.query_range && p.query_range->first == 10.0 &&
         p.query_range->second == 20.0);

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
  threw = false;
  try {
    (void)minera::parse_interval_tree_params(
        fields({{"interval_query_start", "2"},
                {"interval_query_end", "1"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_interval_tree_params(
        fields({{"interval_query_point", "NaN"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void case_relations_params_contract() {
  const auto p = minera::parse_case_relations_params(
      fields({{"merge_variant", "pandas"},
              {"merge_timestamp_key", "completed_at"},
              {"merge_case_id_key", "case:order_id"},
              {"merge_left_suffix", "__left"},
              {"merge_right_suffix", "__right"}}));
  assert(p.variant == minera::CaseRelationsVariant::kPandas);
  assert(p.timestamp_key == "completed_at");
  assert(p.case_id_key == "case:order_id");
  assert(p.left_suffix == "__left" && p.right_suffix == "__right");
  bool threw = false;
  try {
    (void)minera::parse_case_relations_params(
        fields({{"merge_variant", "other"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void random_tree_params_contract() {
  const auto p = minera::parse_random_tree_params(fields({
      {"random_tree_variant", "ptandloggenerator"},
      {"random_tree_min_rec_depth", "2"},
      {"random_tree_max_rec_depth", "4"},
      {"random_tree_prob_leaf", "0.75"},
      {"random_tree_seed", "17"},
      {"random_tree_activities", R"(Review\, senior,A\\B)"},
      {"random_tree_mode", "5"},
      {"random_tree_min", "3"},
      {"random_tree_max", "7"},
      {"random_tree_sequence", "0.2"},
      {"random_tree_choice", "0.3"},
      {"random_tree_parallel", "0.1"},
      {"random_tree_loop", "0.4"},
      {"random_tree_or", "0"},
      {"random_tree_silent", "0.2"},
      {"random_tree_duplicate", "0.1"},
      {"random_tree_no_models", "3"},
  }));
  assert(p.variant == minera::RandomTreeVariant::kPtAndLogGenerator);
  assert(p.min_rec_depth == 2 && p.max_rec_depth == 4);
  assert(p.prob_leaf == 0.75 && p.seed == 17);
  assert((p.activities == std::vector<std::string>{"Review, senior", "A\\B"}));
  assert(p.mode == 5 && p.min == 3 && p.max == 7 && p.sequence == 0.2 &&
         p.choice == 0.3 && p.parallel == 0.1 && p.loop == 0.4 &&
         p.silent == 0.2 && p.duplicate == 0.1 && p.no_models == 3);
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
      {"correlation_activity_attribute", " event_kind "},
      {"correlation_timestamp_attribute", " finished_at "},
      {"correlation_start_timestamp_attribute", "started_at"},
      {"correlation_case_id_attribute", " case_ref "},
      {"correlation_exact_time_matching", "1"},
      {"correlation_sample_size", "42"},
  }));
  assert((params.correlation_variants ==
          std::vector<std::string>{"classic", "classic_split", "trace_based"}));
  assert(params.correlation.activity_attribute == "event_kind");
  assert(params.correlation.timestamp_attribute == "finished_at");
  assert(params.correlation.start_timestamp_attribute == "started_at");
  assert(params.correlation.case_id_attribute == "case_ref");
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
           std::pair{"correlation_activity_attribute", " "},
           std::pair{"correlation_timestamp_attribute", "\t"},
           std::pair{"correlation_case_id_attribute", ""},
           std::pair{"correlation_exact_time_matching", "yes"},
           std::pair{"correlation_sample_size", "0"},
           std::pair{"correlation_sample_size", "-1"},
           std::pair{"correlation_sample_size", "12x"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          [&](std::string_view name) {
            return name == item.first ? item.second : std::string{};
          },
          [&](std::string_view name) { return name == item.first; });
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
      {"xes_variant", "iterparse_mem_compressed"},
      {"xes_case_attribute", "case:id"},
      {"xes_activity_attribute", "event:name"},
      {"xes_timestamp_attribute", "completed_at"},
      {"xes_resource_attribute", "performer"},
      {"xes_lifecycle_attribute", "phase"},
      {"xes_require_timestamp", "true"},
      {"xes_timestamp_sort", "true"},
      {"xes_reverse_sort", "true"},
      {"xes_max_traces", "7"},
      {"xes_attributes_to_read", "cost, region"},
      {"xes_max_decompressed_bytes", "1048576"},
      {"noise_threshold", "0.25"},
      {"alpha_variant", "classic"},
      {"inductive_variant", "dfg"},
      {"inductive_activity_key", " phase "},
      {"inductive_timestamp_key", " finished_at "},
      {"inductive_case_id_key", " case_ref "},
      {"inductive_sort_events_by_timestamp", "1"},
      {"inductive_disable_strict_sequence_cut", "true"},
      {"inductive_disable_fallthroughs", "0"},
      {"inductive_multiprocessing", "true"},
      {"heuristic_variant", "plusplus"},
      {"heuristic_activity_attribute", "phase"},
      {"heuristic_timestamp_attribute", "finished_at"},
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
  assert(params.xes.variant ==
         minera::XesImportVariant::kIterparseMemoryCompressed);
  assert(params.xes.case_attribute == "case:id");
  assert(params.xes.activity_attribute == "event:name");
  assert(params.xes.timestamp_attribute == "completed_at");
  assert(params.xes.resource_attribute == "performer");
  assert(params.xes.lifecycle_attribute == "phase");
  assert(params.xes.require_timestamp && params.xes.timestamp_sort &&
         params.xes.reverse_sort);
  assert(params.xes.max_traces == 7);
  assert(params.xes.max_decompressed_bytes == 1048576);
  assert((params.xes.attributes_to_read ==
          std::vector<std::string>{"cost", "region"}));
  assert(params.inductive.noise_threshold == 0.25);
  assert(params.inductive.variant == minera::InductiveMinerVariant::kDfg);
  assert(params.inductive.activity_key == "phase");
  assert(params.inductive.timestamp_key == "finished_at");
  assert(params.inductive.case_id_key == "case_ref");
  assert(params.inductive.sort_events_by_timestamp);
  assert(params.inductive.disable_strict_sequence_cut);
  assert(!params.inductive.disable_fallthroughs);
  assert(params.inductive.multiprocessing);
  assert(!params.alpha.short_loops);
  assert(params.heuristic.variant ==
         minera::HeuristicMinerVariant::kPlusPlus);
  assert(params.heuristic.activity_attribute == "phase");
  assert(params.heuristic.timestamp_attribute == "finished_at");
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
  for (const auto* value : {"bogus", ""}) {
    const std::map<std::string, std::string> values{
        {"inductive_variant", value}};
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view requested) {
            return values.contains(std::string(requested));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
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
      {"case_duration_min", "10"},
      {"case_duration_max", "120.5"},
      {"keep_activities", "a, b ,c"},
      {"remove_activities", "x"},
      {"keep_resources", "Alice, Bob"},
      {"remove_resources", "Carol"},
      {"keep_lifecycles", "complete"},
      {"remove_lifecycles", "schedule, suspend"},
      {"start_activities", "a"},
      {"start_activities_positive", "false"},
      {"end_activities", "c"},
      {"end_activities_positive", "true"},
      {"max_cases", "4"},
      {"max_activities", "3"},
      {"top_activities_attribute_key", "channel"},
      {"relative_attribute_key", "channel"},
      {"relative_attribute_stake", "0.6"},
      {"relative_attribute_keep_once_per_case", "false"},
      {"attribute_occurrence_key", "channel"},
      {"attribute_occurrence_min", "3"},
      {"min_length", "2"},
      {"max_length", "10"},
      {"min_variant_frequency", "3"},
      {"top_variants", "5"},
      {"rework_activity", "b"},
      {"rework_min_occurrences", "3"},
      {"between_start", "a"},
      {"between_end", "c"},
      {"activity_split", "a, c"},
      {"activity_split_separator", "::"},
      {"activity_split_cut_mode", "next"},
      {"repetition_value", "web"},
      {"repetition_attribute_key", "channel"},
      {"repetition_min", "2"},
      {"repetition_max", "4"},
      {"consecutive_case_grouping", "last"},
      {"consecutive_case_grouping_activity_key", "activity"},
      {"timestamp_case_grouping", "concat"},
      {"timestamp_case_grouping_activity_key", "activity"},
      {"timestamp_case_grouping_timestamp_key", "timestamp"},
      {"timestamp_case_grouping_separator", "+"},
      {"attribute_key", "channel"},
      {"keep_attribute_values", "web, phone"},
      {"remove_attribute_values", "fax"},
      {"attribute_mode", "traces"},
      {"numeric_attribute_key", "amount"},
      {"numeric_attribute_min", "-2.5"},
      {"numeric_attribute_max", "10"},
      {"numeric_attribute_mode", "traces"},
      {"numeric_attribute_positive", "false"},
      {"case_attribute_key", "region"},
      {"keep_case_attribute_values", "north, east"},
      {"remove_case_attribute_values", "south"},
      {"keep_directly_follows", "a -> b, b->c"},
      {"remove_eventually_follows", "a -> d"},
      {"trace_patterns", "a,b,...;...,x,y"},
      {"trace_pattern_positive", "false"},
      {"admitted_variants", "a,b;[];a,b"},
      {"admitted_variants_positive", "false"},
      {"prefix_activity", "c"},
      {"prefix_strict", "false"},
      {"prefix_occurrence", "last"},
      {"suffix_activity", "a"},
      {"suffix_strict", "true"},
      {"suffix_occurrence", "last"},
      {"variant_coverage", "0.8"},
      {"variant_min_coverage", "0.2"},
      {"variant_max_coverage", "0.8"},
      {"variant_percentage", "0.6"},
  }));
  assert(params.filter.time_from == "2024-01-01");
  assert(params.filter.time_to == "2024-01-31T23:59:59");
  assert(params.filter.time_mode ==
         minera::TimeFilterMode::traces_intersecting);
  assert(params.filter.min_case_duration_seconds == 10.0);
  assert(params.filter.max_case_duration_seconds == 120.5);
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
  assert(!params.filter.start_activities_positive);
  assert((params.filter.end_activities == std::vector<std::string>{"c"}));
  assert(params.filter.end_activities_positive);
  assert(params.filter.max_cases && *params.filter.max_cases == 4);
  assert(params.filter.top_activities.has_value());
  assert(params.filter.top_activities->max_activities == 3);
  assert(params.filter.top_activities->attribute_key == "channel");
  assert(params.filter.relative_attribute_occurrence.has_value());
  assert(params.filter.relative_attribute_occurrence->attribute_key == "channel");
  assert(params.filter.relative_attribute_occurrence->min_relative_stake == 0.6);
  assert(!params.filter.relative_attribute_occurrence->keep_once_per_case);
  assert(params.filter.attribute_occurrence_threshold.has_value());
  assert(params.filter.attribute_occurrence_threshold->attribute_key == "channel");
  assert(params.filter.attribute_occurrence_threshold->min_occurrences == 3);
  assert(params.filter.min_length == 2);
  assert(params.filter.max_length == 10);
  assert(params.filter.min_variant_frequency == 3);
  assert(params.filter.top_variants == 5);
  assert(params.filter.rework_activity == "b");
  assert(params.filter.rework_min_occurrences == 3);
  assert(params.filter.between_start == "a");
  assert(params.filter.between_end == "c");
  assert((params.filter.activity_split_activities ==
          std::vector<std::string>{"a", "c"}));
  assert(params.filter.activity_split_separator == "::");
  assert(params.filter.activity_split_cut_mode ==
         minera::ActivitySplitCutMode::kNext);
  assert(params.filter.attribute_value_repetition.has_value());
  assert(params.filter.attribute_value_repetition->value == "web");
  assert(params.filter.attribute_value_repetition->attribute_key == "channel");
  assert(params.filter.attribute_value_repetition->min_repetitions == 2);
  assert(params.filter.attribute_value_repetition->max_repetitions == 4);
  assert(params.filter.consecutive_activity_grouping.has_value());
  assert(params.filter.consecutive_activity_grouping->filter_type ==
         minera::CaseGroupingFilterType::kLast);
  assert(params.filter.consecutive_activity_grouping->activity_key == "activity");
  assert(params.filter.timestamp_case_grouping.has_value());
  assert(params.filter.timestamp_case_grouping->filter_type ==
         minera::TimestampCaseGroupingType::kConcat);
  assert(params.filter.timestamp_case_grouping->activity_key == "activity");
  assert(params.filter.timestamp_case_grouping->timestamp_key == "timestamp");
  assert(params.filter.timestamp_case_grouping->concat_separator == "+");
  assert(params.filter.attribute_key == "channel");
  assert((params.filter.keep_attribute_values ==
          std::vector<std::string>{"web", "phone"}));
  assert((params.filter.remove_attribute_values ==
          std::vector<std::string>{"fax"}));
  assert(params.filter.attribute_mode == minera::AttributeFilterMode::traces);
  assert(params.filter.numeric_attribute.has_value());
  assert(params.filter.numeric_attribute->attribute_key == "amount");
  assert(params.filter.numeric_attribute->min_value == -2.5);
  assert(params.filter.numeric_attribute->max_value == 10.0);
  assert(params.filter.numeric_attribute->mode ==
         minera::AttributeFilterMode::traces);
  assert(!params.filter.numeric_attribute->positive);
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
  assert(params.filter.trace_patterns.size() == 2);
  assert((params.filter.trace_patterns[0] ==
          minera::TracePattern{"a", "b", "..."}));
  assert((params.filter.trace_patterns[1] ==
          minera::TracePattern{"...", "x", "y"}));
  assert(!params.filter.trace_pattern_positive);
  assert(params.filter.admitted_variants.size() == 3);
  assert((params.filter.admitted_variants[0] ==
          minera::TracePattern{"a", "b"}));
  assert(params.filter.admitted_variants[1].empty());
  assert(params.filter.admitted_variants[2] == params.filter.admitted_variants[0]);
  assert(!params.filter.admitted_variants_positive);
  assert(params.filter.prefix_activity == "c");
  assert(!params.filter.prefix_strict);
  assert(params.filter.prefix_occurrence ==
         minera::FilterAnchorOccurrence::last);
  assert(params.filter.suffix_activity == "a");
  assert(params.filter.suffix_strict);
  assert(params.filter.suffix_occurrence ==
         minera::FilterAnchorOccurrence::last);
  assert(params.filter.variant_coverage == 0.8);
  assert(params.filter.variant_min_coverage == 0.2);
  assert(params.filter.variant_max_coverage == 0.8);
  assert(params.filter.variant_percentage == 0.6);
  assert(params.filter.any());
}

void parse_mine_params_rejects_invalid_inductive_options() {
  for (const auto& value : {"abc", "nan", "NaN", "inf", "-inf",
                            "-0.1", "1.1", "1e9999"}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields({{"noise_threshold", value}}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  {
    const std::map<std::string, std::string> values{{"noise_threshold", ""}};
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view requested) {
            return values.contains(std::string(requested));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  for (const auto& item : {
           std::pair{"inductive_disable_strict_sequence_cut", "maybe"},
           std::pair{"inductive_disable_fallthroughs", "TRUE"},
           std::pair{"inductive_multiprocessing", "sometimes"},
           std::pair{"inductive_sort_events_by_timestamp", "yes"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  for (const auto& name : {"inductive_activity_key", "inductive_timestamp_key",
                           "inductive_case_id_key"}) {
    const std::map<std::string, std::string> values{{name, "  "}};
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view requested) {
            return values.contains(std::string(requested));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_rejects_invalid_xes_options() {
  for (const auto& item : {
           std::pair{"xes_variant", "unknown"},
           std::pair{"xes_timestamp_sort", "yes"},
           std::pair{"xes_reverse_sort", "ascending"},
           std::pair{"xes_require_timestamp", "on"},
           std::pair{"xes_max_traces", "-1"},
           std::pair{"xes_max_traces", "2x"},
           std::pair{"xes_max_decompressed_bytes", "0"},
           std::pair{"xes_max_decompressed_bytes", "2x"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  const std::map<std::string, std::string> empty = {
      {"xes_activity_attribute", ""}};
  const auto lookup = [&empty](std::string_view name) {
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

void parse_mine_params_maps_alpha_options_and_presence() {
  const auto params = minera::parse_mine_params(fields({
      {"alpha_variant", "plus"},
      {"alpha_activity_attribute", " phase "},
      {"alpha_remove_unconnected", "true"},
      {"alpha_max_rows_to_materialize", "12"},
  }));
  assert(params.alpha.short_loops);
  assert(params.alpha.activity_attribute == "phase");
  assert(params.alpha.remove_unconnected);
  assert(params.alpha.max_rows_to_materialize == 12);
  const auto zero_cap = minera::parse_mine_params(fields({
      {"alpha_max_rows_to_materialize", "0"},
  }));
  assert(zero_cap.alpha.max_rows_to_materialize == 0);
  assert(!minera::parse_mine_params(fields({})).alpha.max_rows_to_materialize);

  const std::map<std::string, std::string> explicit_empty_variant = {
      {"alpha_variant", ""}};
  bool threw = false;
  try {
    (void)minera::parse_mine_params(
        fields(explicit_empty_variant),
        [](std::string_view name) {
          return name == "alpha_variant";
        });
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  for (const auto& item : {
           std::pair{"alpha_activity_attribute", "  "},
           std::pair{"alpha_remove_unconnected", "yes"},
           std::pair{"alpha_variant", "unknown"},
           std::pair{"alpha_max_rows_to_materialize", "-1"},
           std::pair{"alpha_max_rows_to_materialize", "1.5"},
           std::pair{"alpha_max_rows_to_materialize", "18446744073709551616"},
       }) {
    const std::map<std::string, std::string> values{{item}};
    threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view name) {
            return values.contains(std::string(name));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_rejects_variant_and_endpoint_filter_errors() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"variant_min_coverage", "nan"},
           std::pair<std::string, std::string>{"variant_max_coverage", "1.1"},
           std::pair<std::string, std::string>{"variant_percentage", "-0.1"},
           std::pair<std::string, std::string>{"start_activities_positive", "maybe"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"variant_min_coverage", "0.8"},
        {"variant_max_coverage", "0.2"},
    }));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"end_activities_positive", "false"},
    }));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_invalid_attribute_occurrence_thresholds() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"attribute_occurrence_key", "channel"},
    }));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  for (const auto& value : {"-1", "1.5", "nan"}) {
    threw = false;
    try {
      (void)minera::parse_mine_params(fields({
          {"attribute_occurrence_min", value},
      }));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_rejects_invalid_case_duration_bounds() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"case_duration_min", "nan"},
           std::pair<std::string, std::string>{"case_duration_max", "-1"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"case_duration_min", "10"}, {"case_duration_max", "9"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_invalid_case_selection_bounds() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"max_cases", "-1"},
           std::pair<std::string, std::string>{"min_length", "-1"},
           std::pair<std::string, std::string>{"max_length", "not-an-int"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"min_length", "5"}, {"max_length", "4"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_invalid_frequency_attribute_options() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"max_activities", "-1"},
           std::pair<std::string, std::string>{"relative_attribute_stake", "nan"},
           std::pair<std::string, std::string>{"relative_attribute_stake", "1.1"},
           std::pair<std::string, std::string>{"relative_attribute_keep_once", "maybe"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"top_activities_attribute_key", "channel"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_invalid_numeric_attribute_ranges() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"numeric_attribute_min", "nan"},
           std::pair<std::string, std::string>{"numeric_attribute_mode", "bad"},
           std::pair<std::string, std::string>{"numeric_attribute_positive", "maybe"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({
          {"numeric_attribute_key", "amount"}, item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"numeric_attribute_key", "amount"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"numeric_attribute_key", "amount"},
        {"numeric_attribute_min", "4"},
        {"numeric_attribute_max", "3"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_invalid_prefix_suffix_options() {
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"prefix_activity", "a"}, {"prefix_strict", "maybe"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"suffix_activity", "a"}, {"suffix_occurrence", "middle"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"prefix_occurrence", "last"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_rejects_invalid_trace_patterns() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"trace_patterns", "a,,b"},
           std::pair<std::string, std::string>{"trace_pattern_positive", "maybe"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_rejects_invalid_admitted_variants() {
  for (const auto& item : {
           std::pair<std::string, std::string>{"admitted_variants", "a,,b"},
           std::pair<std::string, std::string>{"admitted_variants", "[];"},
           std::pair<std::string, std::string>{"admitted_variants_positive", "maybe"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"admitted_variants_positive", "false"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
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

void parse_mine_params_rejects_invalid_activity_split_fields() {
  for (const auto& item : std::vector<std::pair<std::string, std::string>>{
           {"activity_split_cut_mode", "unknown"},
           {"activity_split_activities", "a,,b"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  for (const auto& name : {std::string("activity_split"),
                           std::string("activity_split_separator")}) {
    const auto lookup = fields({{name, ""}});
    const auto present = [name](std::string_view requested) {
      return requested == name;
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

void parse_mine_params_maps_transition_system_options() {
  const auto params = minera::parse_mine_params(fields({
      {"transition_view", "multiset"},
      {"transition_direction", "backward"},
      {"transition_window", "0"},
      {"transition_include_data", "1"},
      {"transition_activity_attribute", " phase "},
      {"transition_case_id_attribute", " case_ref "},
  }));
  assert(params.transition_system.view == minera::TransitionView::kMultiset);
  assert(params.transition_system.direction == minera::TransitionDirection::kBackward);
  assert(params.transition_system.window == 0);
  assert(params.transition_system.include_data);
  assert(params.transition_system.activity_attribute == "phase");
  assert(params.transition_system.case_id_attribute == "case_ref");
}

void parse_mine_params_maps_powl_options() {
  const auto params = minera::parse_mine_params(fields({
      {"powl_variant", "dynamic_clustering_plain"},
      {"powl_filtering_type", "dynamic"},
      {"powl_filtering_threshold", "0.25"},
      {"powl_order_frequency_ratio", "0.8"},
      {"powl_brute_force_activity_limit", "12"},
      {"powl_activity_key", " phase "},
      {"powl_timestamp_key", " finished_at "},
      {"powl_case_id_key", " case_ref "},
  }));
  assert(params.powl.variant ==
         minera::PowlDiscoveryVariant::kDynamicClusteringPlain);
  assert(params.powl.filtering_type == minera::PowlFilteringType::kDynamic);
  assert(params.powl.filtering_threshold == 0.25);
  assert(params.powl.order_frequency_ratio == 0.8);
  assert(params.powl.brute_force_activity_limit == 12);
  assert(params.powl.activity_key == "phase");
  assert(params.powl.timestamp_key == "finished_at");
  assert(params.powl.case_id_key == "case_ref");
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
           std::pair{"powl_brute_force_activity_limit", "2x"},
           std::pair{"powl_activity_key", "   "},
           std::pair{"powl_timestamp_key", ""},
           std::pair{"powl_case_id_key", "\t"}}) {
    const std::map<std::string, std::string> values{{item.first, item.second}};
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view name) {
            return values.contains(std::string(name));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_rejects_explicit_blank_powl_scalars() {
  const std::vector<std::string> blank_values{"", " \t\r\n"};
  const std::vector<std::string> names{
      "powl_variant", "powl_filtering_type", "powl_filtering_threshold",
      "powl_order_frequency_ratio", "powl_brute_force_activity_limit"};
  for (const auto& name : names) {
    for (const auto& value : blank_values) {
      const std::map<std::string, std::string> values{{name, value}};
      bool threw = false;
      try {
        (void)minera::parse_mine_params(
            fields(values), [&](std::string_view requested) {
              return requested == name;
            });
      } catch (const std::invalid_argument&) {
        threw = true;
      }
      assert(threw);
    }
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
  for (const auto name : {"transition_activity_attribute",
                          "transition_case_id_attribute"}) {
    const std::map<std::string, std::string> values{{name, ""}};
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view key) {
            return key == name;
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_mine_params_maps_token_replay_options() {
  const auto params = minera::parse_mine_params(fields({
      {"token_replay_variant", "backwards"},
      {"token_replay_stop_immediately_unfit", "1"},
      {"token_replay_activity_attribute", " phase "},
  }));
  assert(params.token_replay.variant ==
         minera::TokenReplayVariant::kBackwards);
  assert(params.token_replay.stop_immediately_unfit);
  assert(params.token_replay.activity_attribute == "phase");
  assert(params.token_replay_activity_attribute_supplied);
}

void parse_mine_params_rejects_invalid_token_replay_fields() {
  for (const auto& item : {
           std::pair{"token_replay_variant", "backward"},
           std::pair{"token_replay_variant", "bad"},
           std::pair{"token_replay_stop_immediately_unfit", "yes"},
           std::pair{"token_replay_activity_attribute", "   "}}) {
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
      {"log_skeleton_activity_attribute", " phase "},
      {"log_skeleton_constraints",
       " always_after, equivalence, always_after,, activ_freq "},
  }));
  assert(params.log_skeleton.noise_threshold == 0.25);
  assert(params.log_skeleton.activity_key == "phase");
  assert(params.log_skeleton.considered_constraints_present);
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
      {"declare_activity_attribute", " phase "},
      {"declare_considered_activities", " B, A, B "},
      {"declare_allowed_templates", "response, existence,response"},
      {"declare_min_support_ratio", "0.25"},
      {"declare_min_confidence_ratio", "1"},
      {"declare_auto_selection_multiplier", "0.75"},
  }));
  assert(params.declare_options.considered_activities.has_value());
  assert(params.declare_options.activity_key == "phase");
  assert(params.declare_conformance_options.activity_key == "phase");
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
  assert(defaults.declare_options.activity_key == "concept:name");
  assert(defaults.declare_conformance_options.activity_key == "concept:name");
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

void parse_mine_params_preserves_explicit_empty_log_skeleton_constraints() {
  const std::map<std::string, std::string> values = {
      {"log_skeleton_constraints", ""},
  };
  const auto explicit_empty = minera::parse_mine_params(
      fields(values), [&](std::string_view key) {
        return values.contains(std::string(key));
      });
  assert(explicit_empty.log_skeleton.considered_constraints_present);
  assert(explicit_empty.log_skeleton.considered_constraints.empty());
  const auto omitted = minera::parse_mine_params(fields({}));
  assert(!omitted.log_skeleton.considered_constraints_present);
}

void parse_mine_params_rejects_blank_declarative_selectors() {
  for (const auto name : {"log_skeleton_activity_attribute",
                          "declare_activity_attribute"}) {
    const std::map<std::string, std::string> values = {{name, "  "}};
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields(values), [&](std::string_view key) {
            return values.contains(std::string(key));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
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
       "trace_by_trace, dfg,trace_by_trace,petri_reach_graph,entire_dataframe,polars_lazyframes"},
      {"footprints_model", "alpha"},
      {"footprints_conformance_variants",
       "trace_extensive,log_model,trace_extensive"},
      {"footprints_strict", "1"},
      {"footprints_enable_act_always_executed", "false"},
      {"footprints_activity_attribute", " phase "},
      {"footprints_case_id_attribute", " case_ref "},
      {"footprints_start_timestamp_attribute", " started_at "},
      {"footprints_timestamp_attribute", " finished_at "},
      {"footprints_index_attribute", " row_number "},
      {"footprints_sort_required", "false"},
      {"footprints_initial_marking", "000, 18446744073709551615"},
  }));
  assert((params.footprints.discovery_variants ==
          std::set<std::string>{"dfg", "entire_dataframe",
                                "petri_reach_graph", "polars_lazyframes",
                                "trace_by_trace"}));
  assert(params.footprints.model == "alpha");
  assert((params.footprints.conformance_variants ==
          std::set<std::string>{"log_model", "trace_extensive"}));
  assert(params.footprints.strict);
  assert(!params.footprints.enable_act_always_executed);
  assert(params.footprints.activity_attribute == "phase");
  assert(params.footprints.case_id_attribute == "case_ref");
  assert(params.footprints.start_timestamp_attribute &&
         *params.footprints.start_timestamp_attribute == "started_at");
  assert(params.footprints.timestamp_attribute == "finished_at");
  assert(params.footprints.index_attribute == "row_number");
  assert(!params.footprints.sort_required);
  const std::vector<std::uint64_t> expected_marking = {
      0, 18446744073709551615ULL};
  assert(params.footprints.initial_marking &&
         *params.footprints.initial_marking == expected_marking);

  const auto defaults = minera::parse_mine_params(fields({}));
  assert((defaults.footprints.discovery_variants ==
          std::set<std::string>{"dfg", "entire_event_log", "powl",
                                "process_tree"}));
  assert(defaults.footprints.model == "process_tree");
  assert((defaults.footprints.conformance_variants ==
          std::set<std::string>{"log_model"}));
  assert(!defaults.footprints.strict);
  assert(defaults.footprints.enable_act_always_executed);
  assert(defaults.footprints.activity_attribute == "concept:name");
  assert(defaults.footprints.case_id_attribute == "case:concept:name");
  assert(defaults.footprints.timestamp_attribute == "time:timestamp");
  assert(!defaults.footprints.start_timestamp_attribute);
  assert(defaults.footprints.index_attribute == "@@index");
  assert(defaults.footprints.sort_required);
  assert(!defaults.footprints.initial_marking);
  const std::map<std::string, std::string> empty_start = {
      {"footprints_start_timestamp_attribute", ""}};
  const auto explicit_empty = minera::parse_mine_params(
      [&](std::string_view name) {
        const auto it = empty_start.find(std::string(name));
        return it == empty_start.end() ? std::string{} : it->second;
      },
      [&](std::string_view name) {
        return empty_start.contains(std::string(name));
      });
  assert(!explicit_empty.footprints.start_timestamp_attribute);
  const std::map<std::string, std::string> empty_index = {
      {"footprints_index_attribute", "\t"}};
  const auto explicit_empty_index = minera::parse_mine_params(
      [&](std::string_view name) {
        const auto it = empty_index.find(std::string(name));
        return it == empty_index.end() ? std::string{} : it->second;
      },
      [&](std::string_view name) {
        return empty_index.contains(std::string(name));
      });
  assert(explicit_empty_index.footprints.index_attribute.empty());
}

minera::Event make_footprint_event(std::string activity,
                                   std::string timestamp,
                                   std::size_t input_order,
                                   std::string case_ref,
                                   std::string phase = {}) {
  minera::Event event;
  event.activity = std::move(activity);
  event.timestamp = std::move(timestamp);
  event.input_order = input_order;
  event.attributes.push_back(
      {"case_ref", std::move(case_ref), minera::AttributeKind::kString});
  if (!phase.empty())
    event.attributes.push_back(
        {"phase", std::move(phase), minera::AttributeKind::kString});
  return event;
}

minera::EventLog interleaved_footprint_log() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.events.push_back(
      make_footprint_event("A", "2024-01-01T00:00:00Z", 0, "g1"));
  first.events.push_back(
      make_footprint_event("B", "2024-01-01T00:00:02Z", 2, "g1"));
  minera::Trace second;
  second.case_id = "c2";
  second.events.push_back(
      make_footprint_event("X", "2024-01-01T00:00:01Z", 1, "g2"));
  second.events.push_back(
      make_footprint_event("Y", "2024-01-01T00:00:03Z", 3, "g2"));
  log.traces = {std::move(first), std::move(second)};
  return log;
}

void footprints_projection_preserves_case_and_source_order() {
  const auto log = interleaved_footprint_log();
  minera::FootprintsDiscoveryOptions options;
  options.case_id_attribute = "case_ref";
  options.sort_required = false;
  const auto table = minera::prepare_footprints_event_table(log, options);
  const auto eager = minera::discover_footprints(table, options);
  assert(eager.dfg.empty());

  const auto lazy_table = minera::prepare_footprints_lazy_table(log, options);
  const auto lazy = minera::discover_footprints(lazy_table, options);
  assert(lazy.dfg.at({"A", "B"}) == 1);
  assert(lazy.dfg.at({"X", "Y"}) == 1);

  auto collision_log = log;
  for (auto& trace : collision_log.traces)
    for (auto& event : trace.events)
      event.attributes.front().value = "g1";
  const auto collision_table =
      minera::prepare_footprints_event_table(collision_log, options);
  const auto collision =
      minera::discover_footprints(collision_table, options);
  assert(collision.dfg.at({"A", "X"}) == 1);
  assert(collision.dfg.at({"X", "B"}) == 1);
}

void footprints_projection_supports_default_csv_and_xes_schema() {
  auto csv_like = interleaved_footprint_log();
  for (auto& trace : csv_like.traces) {
    trace.case_id_source_key = "case_id";
    trace.case_id_state = minera::FirstClassAttributeState::kPresent;
    for (auto& event : trace.events) {
      event.activity_source_key = "activity";
      event.timestamp_source_key = "timestamp";
      event.activity_state = minera::FirstClassAttributeState::kPresent;
      event.timestamp_state = minera::FirstClassAttributeState::kPresent;
    }
  }
  const auto csv_table =
      minera::prepare_footprints_event_table(csv_like);
  const auto csv_result = minera::discover_footprints(
      csv_table, minera::FootprintsDiscoveryOptions{});
  assert(csv_result.dfg.at({"A", "B"}) == 1);

  auto xes_like = interleaved_footprint_log();
  for (auto& trace : xes_like.traces) {
    trace.case_id_source_key = "case:concept:name";
    trace.case_id_state = minera::FirstClassAttributeState::kPresent;
    for (auto& event : trace.events) {
      event.activity_source_key = "concept:name";
      event.timestamp_source_key = "time:timestamp";
      event.activity_state = minera::FirstClassAttributeState::kPresent;
      event.timestamp_state = minera::FirstClassAttributeState::kPresent;
    }
  }
  const auto xes_table =
      minera::prepare_footprints_event_table(xes_like);
  const auto xes_result = minera::discover_footprints(
      xes_table, minera::FootprintsDiscoveryOptions{});
  assert(xes_result.dfg.at({"A", "B"}) == 1);
}

void footprints_projection_rejects_missing_or_null_case_selector() {
  auto log = interleaved_footprint_log();
  log.traces.front().events.front().attributes.front().is_null = true;
  minera::FootprintsDiscoveryOptions options;
  options.case_id_attribute = "case_ref";
  bool threw = false;
  try {
    (void)minera::resolve_footprints_case_ids(log, options.case_id_attribute);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void footprints_json_uses_selected_case_ids_and_provenance() {
  auto log = interleaved_footprint_log();
  minera::FootprintsDiscoveryOptions options;
  options.activity_attribute = "phase";
  options.case_id_attribute = "case_ref";
  for (auto& trace : log.traces)
    for (auto& event : trace.events)
      {
        auto phase = std::find_if(
            event.attributes.begin(), event.attributes.end(),
            [](const auto& attribute) { return attribute.key == "phase"; });
        if (phase == event.attributes.end())
          event.attributes.push_back(
              {"phase", event.activity, minera::AttributeKind::kString});
        else
          phase->value = event.activity;
      }
  const auto projected = minera::prepare_footprints_event_log(log, options);
  const auto graph = minera::discover_directly_follows_graph(projected);
  minera::FootprintsAnalysis analysis;
  analysis.model = "dfg";
  analysis.activity_attribute = options.activity_attribute;
  analysis.case_id_attribute = options.case_id_attribute;
  analysis.trace_case_ids =
      minera::resolve_footprints_case_ids(log, options.case_id_attribute);
  analysis.trace_by_trace =
      minera::discover_trace_footprints(projected, options);
  analysis.model_footprint = minera::discover_dfg_footprints(graph);
  const auto json = minera::analysis_json(
      "footprints.csv", log, graph,
      /*tree=*/nullptr, /*net=*/nullptr, /*heuristics=*/nullptr,
      /*conformance=*/nullptr, /*performance=*/nullptr, /*resources=*/nullptr,
      /*variant_durations=*/nullptr, /*eventually_follows=*/nullptr,
      /*decisions=*/nullptr, /*petri_decoration=*/nullptr,
      /*spectrum=*/nullptr, /*inductive_net=*/nullptr,
      /*inductive_decoration=*/nullptr, /*heuristic_petri=*/nullptr,
      /*heuristic_decoration=*/nullptr, /*temporal_profile=*/nullptr,
      /*minimum_self_distances=*/nullptr, /*transition_system=*/nullptr,
      /*powl=*/nullptr, /*powl_petri=*/nullptr, /*powl_decoration=*/nullptr,
      /*log_skeleton=*/nullptr, /*log_skeleton_conformance=*/nullptr,
      /*declare=*/nullptr, /*declare_conformance=*/nullptr,
      /*footprints=*/&analysis);
  assert(json.find("\"case_id\": \"g1\"") != std::string::npos);
  assert(json.find("\"activity_attribute\": \"phase\"") !=
         std::string::npos);
}

void parse_mine_params_rejects_invalid_footprints_options() {
  for (const auto& item : {
           std::pair{"footprints_discovery_variants", "dfg,unknown"},
           std::pair{"footprints_model", "unknown"},
           std::pair{"footprints_conformance_variants", "future"},
           std::pair{"footprints_strict", "yes"},
           std::pair{"footprints_enable_act_always_executed", "enabled"},
           std::pair{"footprints_activity_attribute", ""},
           std::pair{"footprints_case_id_attribute", "  "},
           std::pair{"footprints_timestamp_attribute", ""},
           std::pair{"footprints_sort_required", "maybe"},
           std::pair{"footprints_initial_marking", "1,,2"},
           std::pair{"footprints_initial_marking", "18446744073709551616"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(
          fields({item}), [&](std::string_view name) {
            return name == item.first;
          });
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

void parse_mine_params_accepts_infrequent_inductive_variant() {
  const auto params = minera::parse_mine_params(
      fields({{"inductive_variant", "imf"}, {"noise_threshold", "0.2"}}));
  assert(params.inductive.variant == minera::InductiveMinerVariant::kImf);
  assert(params.inductive.noise_threshold == 0.2);
}

void parse_mine_params_rejects_invalid_repetition_fields() {
  for (const auto& item : std::vector<std::pair<std::string, std::string>>{
           {"repetition_attribute_key", "channel"},
           {"repetition_min", "2"},
           {"repetition_max", "2"},
           {"repetition_value", "web"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    if (item.first == "repetition_value") {
      assert(!threw);
    } else {
      assert(threw);
    }
  }
  bool range_threw = false;
  try {
    (void)minera::parse_mine_params(
        fields({{"repetition_value", "web"},
                {"repetition_min", "3"},
                {"repetition_max", "2"}}));
  } catch (const std::invalid_argument&) {
    range_threw = true;
  }
  assert(range_threw);
}

void parse_mine_params_rejects_invalid_case_grouping_fields() {
  for (const auto& item : std::vector<std::pair<std::string, std::string>>{
           {"consecutive_case_grouping", "bad"},
           {"timestamp_case_grouping", "bad"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  const auto lookup = fields({{"timestamp_case_grouping", "concat"},
                              {"timestamp_case_grouping_separator", ""}});
  const auto present = [](std::string_view name) {
    return name == "timestamp_case_grouping" ||
           name == "timestamp_case_grouping_separator";
  };
  bool separator_threw = false;
  try {
    (void)minera::parse_mine_params(lookup, present);
  } catch (const std::invalid_argument&) {
    separator_threw = true;
  }
  assert(separator_threw);
}

void parse_mine_params_maps_dfg_variant_options() {
  const auto params = minera::parse_mine_params(fields({
      {"dfg_statistical_variants",
       "clean_time,frequency_greedy,native,frequency,clean_time,performance"},
      {"dfg_causal_variants", "heuristic,alpha,heuristic"},
      {"dfg_window", "0"},
      {"dfg_keep_once_per_case", "true"},
      {"dfg_activity_attribute", "event:name"},
      {"dfg_timestamp_attribute", "event:when"},
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
  assert(params.dfg_statistics.activity_attribute == "event:name");
  assert(params.dfg_statistics.timestamp_attribute == "event:when");
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
  assert(defaults.dfg_statistics.activity_attribute == "concept:name");
  assert(defaults.dfg_statistics.timestamp_attribute == "time:timestamp");
  assert(defaults.dfg_statistics.performance_aggregation ==
         minera::DfgPerformanceAggregation::kMean);
  assert(defaults.dfg_statistics.start_timestamp_attribute.empty());
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

  for (const std::string name : {"dfg_activity_attribute",
                                 "dfg_timestamp_attribute"}) {
    const std::map<std::string, std::string> empty_value = {{name, ""}};
    const auto empty_lookup = [&empty_value](std::string_view key) {
      const auto it = empty_value.find(std::string(key));
      return it == empty_value.end() ? std::string{} : it->second;
    };
    const auto empty_present = [&empty_value](std::string_view key) {
      return empty_value.contains(std::string(key));
    };
    bool threw = false;
    try {
      (void)minera::parse_mine_params(empty_lookup, empty_present);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
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

void parse_mine_params_maps_dataframe_dfg_options() {
  const auto params = minera::parse_mine_params(fields({
      {"dfg_operation", "concurrent_events"},
      {"dfg_measure", "cost"},
      {"dfg_backend", "polars"},
      {"dfg_activity_attribute", "event:activity"},
      {"dfg_target_activity_attribute", "event:target"},
      {"dfg_timestamp_attribute", "event:complete"},
      {"dfg_start_timestamp_attribute", "event:start"},
      {"dfg_cost_attribute", "event:cost"},
      {"dfg_case_id_attribute", "case:external"},
      {"dfg_window", "2"},
      {"dfg_keep_once_per_case", "true"},
      {"dfg_sort_caseid_required", "false"},
      {"dfg_sort_timestamp_along_case_id", "false"},
      {"dfg_reduce_columns", "false"},
      {"dfg_workcalendar", "calendar-token"},
      {"dfg_event_index_attribute", "row-id"},
      {"dfg_max_start_column", "max-start"},
      {"dfg_min_complete_column", "min-complete"},
      {"dfg_overlap_column", "overlap"},
      {"dfg_strict", "true"},
      {"dfg_keep_first_following", "false"},
      {"dfg_business_hours", "true"},
      {"dfg_business_hour_slots", "0-3600,7200-10800"},
      {"dfg_performance_aggregation", "sum"},
  }));
  assert(params.dfg_dataframe_requested);
  assert(params.dfg_operation == minera::DfgTableOperation::kConcurrentEvents);
  assert(params.dfg_backend == minera::DfgTableBackend::kPolars);
  const auto& options = params.dfg_table_options;
  assert(options.measure == minera::DfgTableMeasure::kCost);
  assert(options.activity_attribute == "event:activity");
  assert(options.target_activity_attribute == "event:target");
  assert(options.timestamp_attribute == "event:complete");
  assert(options.start_timestamp_attribute &&
         *options.start_timestamp_attribute == "event:start");
  assert(options.cost_attribute && *options.cost_attribute == "event:cost");
  assert(options.case_id_attribute == "case:external");
  assert(options.window == 2);
  assert(options.keep_once_per_case);
  assert(!options.sort_caseid_required);
  assert(!options.sort_timestamp_along_case_id);
  assert(!options.reduce_columns);
  assert(std::any_cast<std::string>(options.workcalendar) == "calendar-token");
  assert(options.event_index_attribute == "row-id");
  assert(options.max_start_column == "max-start");
  assert(options.min_complete_column == "min-complete");
  assert(options.overlap_column == "overlap");
  assert(options.strict);
  assert(!options.keep_first_following);
  assert(options.business_hours);
  assert(options.business_hour_slots && options.business_hour_slots->size() == 2);
  assert(options.performance_aggregation ==
         minera::DfgPerformanceAggregation::kSum);
}

void parse_mine_params_preserves_dataframe_slot_presence() {
  const auto defaults = minera::parse_mine_params(fields({}));
  assert(!defaults.dfg_table_options.business_hour_slots.has_value());
  const std::map<std::string, std::string> values = {
      {"dfg_business_hour_slots", ""},
  };
  const auto lookup = [&values](std::string_view name) {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string{} : it->second;
  };
  const auto present = [&values](std::string_view name) {
    return values.contains(std::string(name));
  };
  const auto empty = minera::parse_mine_params(lookup, present);
  assert(empty.dfg_table_options.business_hour_slots.has_value());
  assert(empty.dfg_table_options.business_hour_slots->empty());

  const auto none = minera::parse_mine_params(
      fields({{"dfg_business_hour_slots", "none"}}));
  assert(none.dfg_table_options.business_hour_slots.has_value());
  assert(none.dfg_table_options.business_hour_slots->empty());
}

void parse_mine_params_rejects_invalid_dataframe_dfg_options() {
  for (const auto& item : {
           std::pair{"dfg_operation", "bad"},
           std::pair{"dfg_measure", "bad"},
           std::pair{"dfg_backend", "bad"},
           std::pair{"dfg_sort_caseid_required", "yes"},
           std::pair{"dfg_sort_timestamp_along_case_id", "yes"},
           std::pair{"dfg_reduce_columns", "yes"},
           std::pair{"dfg_strict", "yes"},
           std::pair{"dfg_keep_first_following", "yes"},
       }) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  for (const auto name : {std::string_view{"dfg_case_id_attribute"},
                          std::string_view{"dfg_cost_attribute"}}) {
    const std::map<std::string, std::string> values = {
        {std::string(name), ""},
    };
    const auto lookup = [&values](std::string_view key) {
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
  const std::map<std::string, std::string> empty_calendar = {
      {"dfg_workcalendar", ""},
  };
  const auto lookup = [&empty_calendar](std::string_view name) {
    const auto it = empty_calendar.find(std::string(name));
    return it == empty_calendar.end() ? std::string{} : it->second;
  };
  const auto present = [&empty_calendar](std::string_view name) {
    return empty_calendar.contains(std::string(name));
  };
  bool calendar_threw = false;
  try {
    (void)minera::parse_mine_params(lookup, present);
  } catch (const std::invalid_argument&) {
    calendar_threw = true;
  }
  assert(calendar_threw);

  bool missing_cost_threw = false;
  try {
    (void)minera::parse_mine_params(
        fields({{"dfg_measure", "cost"}}));
  } catch (const std::invalid_argument&) {
    missing_cost_threw = true;
  }
  assert(missing_cost_threw);
}

void parse_alignment_params_maps_variants_and_costs() {
  const auto p = minera::parse_alignment_params(fields({
      {"alignment_variant", "discounted_a_star"},
      {"alignment_activity_attribute", " phase "},
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
  assert(p.activity_attribute == "phase");
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
  threw = false;
  try { (void)minera::parse_alignment_params(fields({{"alignment_activity_attribute", "   "}})); }
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
      {"pt_alignment_activity_attribute", " phase "},
      {"pt_alignment_use_global_cache", "true"},
      {"pt_alignment_batch_size", "2"},
      {"pt_alignment_clear_cache", "false"}}));
  assert(p.variant == minera::ProcessTreeAlignmentVariant::kDynamicProgramming);
  assert(p.max_states == 0 && p.max_time_seconds == 1.5);
  assert(p.max_total_time_seconds == 0.0 && p.max_trace_length == 0);
  assert(p.max_process_tree_height == 3 && p.use_global_cache && p.batch_size == 2 && !p.clear_cache);
  assert(p.activity_attribute == "phase");
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
  for (const auto name : {"pt_alignment_activity_attribute"}) {
    const std::map<std::string, std::string> values = {{name, "  "}};
    bool threw = false;
    try {
      (void)minera::parse_process_tree_alignment_params(
          fields(values), [&](std::string_view key) {
            return values.contains(std::string(key));
          });
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_native_adapter_params_contract() {
  const auto d = minera::parse_decomposed_alignment_params(fields({
      {"variant", "recompos_maximal"}, {"max_states", "42"},
      {"activity_attribute", " phase "},
      {"threshold_border_agreement", "2.5"}, {"initial_marking", "1, 0"},
      {"tau_move_cost", "3"}, {"sync_cost", "0.5"},
      {"activity_log_costs", "A\\,B=7"}}));
  assert(d.max_states == 42 && d.initial_marking.size() == 2 &&
         d.tau_model_cost == 3.0 && d.sync_cost == 0.5 &&
         d.activity_log_costs.at("A,B") == 7.0 &&
         d.activity_attribute == "phase");
  assert(minera::parse_activity_costs("A\\=B:4", "costs").at("A=B") ==
         4.0);
  const auto a = minera::parse_petri_anti_alignment_params(fields({
      {"variant", "version_discounted_a_star"}, {"exponent", "2"},
      {"marking_limit", "10"}, {"max_states", "0"},
      {"activity_attribute", " phase "}}));
  assert(a.marking_limit == 10 && a.max_states == 0 &&
         a.activity_attribute == "phase");
  for (const auto name : {"activity_attribute"}) {
    const std::map<std::string, std::string> values = {{name, "\t"}};
    bool rejected = false;
    try {
      (void)minera::parse_decomposed_alignment_params(
          fields(values), [&](std::string_view key) {
            return values.contains(std::string(key));
          });
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  bool multi_rejected = false;
  try {
    const std::map<std::string, std::string> values = {
        {"activity_attribute", "  "}};
    (void)minera::parse_petri_multi_alignment_params(
        fields(values), [&](std::string_view key) {
          return values.contains(std::string(key));
        });
  } catch (const std::invalid_argument&) {
    multi_rejected = true;
  }
  assert(multi_rejected);
  bool threw = false;
  try { (void)minera::parse_petri_multi_alignment_params(fields({{"marking_limit", "0"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  const auto r = minera::parse_process_tree_reduction_params(fields({
      {"variant", "tree_tr_based"}, {"activity_key", "concept:name"}}));
  assert(r.activity_key == "concept:name");
  const auto net_reduction = minera::parse_petri_net_reduction_params(fields({
      {"petri_reduction_initial_marking", "1,0,2"},
      {"petri_reduction_final_marking", "0,0,1"},
      {"petri_reduction_parallel_place_fusion", "true"},
      {"petri_reduction_abstraction", "1"}}));
  assert((net_reduction.initial_marking == minera::Marking{1, 0, 2}));
  assert((net_reduction.final_marking == minera::Marking{0, 0, 1}));
  assert(net_reduction.enable_parallel_place_fusion &&
         net_reduction.enable_abstraction);
  for (const auto& invalid : {
           std::pair{"petri_reduction_initial_marking", "1,-1"},
           std::pair{"petri_reduction_parallel_place_fusion", "maybe"}}) {
    bool rejected = false;
    try {
      (void)minera::parse_petri_net_reduction_params(fields({invalid}));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  bool rejected_empty = false;
  try {
    (void)minera::parse_petri_net_reduction_params(
        fields({{"petri_reduction_abstraction", ""}}),
        [](std::string_view name) {
          return name == "petri_reduction_abstraction";
        });
  } catch (const std::invalid_argument&) {
    rejected_empty = true;
  }
  assert(rejected_empty);
}

void parse_evaluation_params_defaults_and_variants() {
  const auto defaults = minera::parse_evaluation_params(fields({}));
  assert(defaults.family == minera::EvaluationFamily::kComposite);
  assert(defaults.replay_fitness_variant == minera::ReplayFitnessVariant::AUTO);
  assert(defaults.replay_fitness_options.activity_attribute == "concept:name");
  assert(defaults.replay_fitness_options.token_replay_variant ==
         minera::TokenReplayVariant::kForward);
  assert(defaults.replay_fitness_options.alignment_options.variant ==
         minera::AlignmentVariant::kDijkstraLessMemory);
  assert(defaults.replay_fitness_options.alignment_options.cost_profile ==
         minera::AlignmentCostProfile::kStandard);
  assert(!defaults.replay_fitness_options.decomposed_alignment_options);
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
  assert(defaults.composite_options.fitness_weight == 0.25 &&
         defaults.composite_options.precision_weight == 0.25 &&
         defaults.composite_options.simplicity_weight == 0.25 &&
         defaults.composite_options.generalization_weight == 0.25);

  for (const auto& family : {"replay_fitness", "precision", "generalization",
                             "simplicity", "composite"}) {
    const auto parsed = minera::parse_evaluation_params(
        fields({{"evaluation_family", family}}));
    assert((family == std::string("replay_fitness") &&
            parsed.family == minera::EvaluationFamily::kReplayFitness) ||
           (family == std::string("precision") &&
            parsed.family == minera::EvaluationFamily::kPrecision) ||
           (family == std::string("generalization") &&
            parsed.family == minera::EvaluationFamily::kGeneralization) ||
           (family == std::string("simplicity") &&
            parsed.family == minera::EvaluationFamily::kSimplicity) ||
           (family == std::string("composite") &&
            parsed.family == minera::EvaluationFamily::kComposite));
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
      {"evaluation_activity_attribute", " phase "},
      {"evaluation_case_id_attribute", " case:order "},
      {"evaluation_max_rows_to_materialize", "78"},
      {"evaluation_initial_marking", "0, 2,0"},
      {"evaluation_final_marking", "0,0,2"},
      {"evaluation_token_replay_variant", "forward"},
      {"fitness_consider_unknown_activities", "true"},
      {"evaluation_alignment_variant", "discounted_a_star"},
      {"evaluation_alignment_max_states", "23"},
      {"evaluation_alignment_max_time", "1.5"},
      {"evaluation_alignment_max_time_trace", "0.25"},
      {"evaluation_alignment_exponent", "3"},
      {"evaluation_alignment_synchronous_dijkstra", "false"},
      {"evaluation_alignment_cost_profile", "legacy"},
      {"evaluation_alignment_trace_cost", "4"},
      {"evaluation_alignment_model_cost", "5"},
      {"evaluation_alignment_silent_model_cost", "0.5"},
      {"evaluation_alignment_sync_cost", "0"},
      {"precision_variant", "automaton_after_align"},
      {"precision_use_task_ids", "true"},
      {"precision_max_states", "1"},
      {"generalization_variant", "generalization_token"},
      {"simplicity_variant", "extended_cyclomatic"},
      {"simplicity_arc_degree_k", "-0.5"},
      {"simplicity_max_states", "7"},
      {"simplicity_initial_marking", "0, 1,2"},
      {"simplicity_final_marking", "3,4"},
      {"fitness_weight", "2"}, {"precision_weight", "1"},
      {"simplicity_weight", "0"}, {"generalization_weight", "0"}}));
  assert(all.precision_options.use_task_ids && all.precision_options.max_states == 1);
  assert(all.replay_fitness_options.activity_attribute == "phase" &&
         all.precision_options.activity_attribute == "phase" &&
         all.generalization_options.activity_attribute == "phase" &&
         all.composite_options.activity_attribute == "phase");
  assert(all.replay_fitness_options.case_id_attribute == "case:order" &&
         all.precision_options.case_id_attribute == "case:order" &&
         all.generalization_options.case_id_attribute == "case:order" &&
         all.composite_options.case_id_attribute == "case:order");
  assert(all.replay_fitness_options.max_rows_to_materialize &&
         *all.replay_fitness_options.max_rows_to_materialize == 78 &&
         all.precision_options.max_rows_to_materialize &&
         *all.precision_options.max_rows_to_materialize == 78 &&
         all.generalization_options.max_rows_to_materialize &&
         *all.generalization_options.max_rows_to_materialize == 78 &&
         all.composite_options.max_rows_to_materialize &&
         *all.composite_options.max_rows_to_materialize == 78);
  assert(all.replay_fitness_options.token_replay_variant ==
             minera::TokenReplayVariant::kForward &&
         all.precision_options.token_replay_variant ==
              minera::TokenReplayVariant::kForward);
  const std::vector<int> evaluation_initial{0, 2, 0};
  const std::vector<int> evaluation_final{0, 0, 2};
  assert(all.replay_fitness_options.initial_marking == evaluation_initial &&
         all.replay_fitness_options.final_marking == evaluation_final &&
         all.precision_options.initial_marking == evaluation_initial &&
         all.precision_options.final_marking == evaluation_final &&
         all.generalization_options.initial_marking == evaluation_initial &&
         all.generalization_options.final_marking == evaluation_final &&
         all.composite_options.initial_marking == evaluation_initial &&
         all.composite_options.final_marking == evaluation_final);
  assert(all.replay_fitness_options
             .consider_activities_not_in_model_in_fitness);
  const auto& alignment = all.replay_fitness_options.alignment_options;
  assert(alignment.variant == minera::AlignmentVariant::kDiscountedAStar &&
         alignment.max_states == 23 &&
         alignment.max_total_time_seconds == 1.5 &&
         alignment.max_time_seconds == 0.25 && alignment.exponent == 3.0 &&
         !alignment.synchronous_dijkstra &&
         alignment.cost_profile ==
             minera::AlignmentCostProfile::kLegacyUnit);
  assert(alignment.default_trace_cost == 4.0 &&
         alignment.default_model_cost == 5.0 &&
         alignment.default_silent_cost == 0.5 &&
          alignment.default_sync_cost == 0.0);
  assert(alignment.initial_marking == evaluation_initial &&
         alignment.final_marking == evaluation_final);
  assert(all.simplicity_options.arc_degree_k == -0.5 &&
         all.simplicity_options.max_states == 7);
  assert((all.simplicity_options.initial_marking == std::vector<int>{0, 1, 2}));
  assert((all.simplicity_options.final_marking == std::vector<int>{3, 4}));
  assert(all.composite_options.fitness_weight == 2.0 &&
         all.composite_options.precision_weight == 1.0 &&
         all.composite_options.simplicity_weight == 0.0 &&
         all.composite_options.generalization_weight == 0.0);

  const auto generator = minera::parse_evaluation_params(fields({
      {"evaluation_alignment_variant", "generator_dijkstra_less_memory"},
      {"evaluation_alignment_max_alignments", "7"}}));
  assert(generator.replay_fitness_options.alignment_options.variant ==
             minera::AlignmentVariant::kGeneratorDijkstraLessMemory &&
         generator.replay_fitness_options.alignment_options.max_alignments ==
             7);

  const auto backwards = minera::parse_evaluation_params(fields({
      {"evaluation_token_replay_variant", "backwards"}}));
  assert(backwards.replay_fitness_options.token_replay_variant ==
             minera::TokenReplayVariant::kBackwards &&
         backwards.precision_options.token_replay_variant ==
             minera::TokenReplayVariant::kBackwards);

  const auto decomposed = minera::parse_evaluation_params(fields({
      {"fitness_variant", "alignment_based"},
      {"evaluation_alignment_variant", "recompos_maximal"},
      {"evaluation_alignment_max_states", "17"},
      {"evaluation_alignment_max_total_states", "31"},
      {"evaluation_alignment_max_time", "2"},
      {"evaluation_alignment_max_time_trace", "0.5"},
      {"evaluation_alignment_threshold_border_agreement", "9"},
      {"evaluation_alignment_use_cache", "false"},
      {"evaluation_alignment_clear_cache", "true"},
      {"evaluation_alignment_trace_cost", "11"},
      {"evaluation_alignment_model_cost", "12"},
      {"evaluation_alignment_silent_model_cost", "1"},
      {"evaluation_alignment_sync_cost", "0"}}));
  assert(decomposed.replay_fitness_options.decomposed_alignment_options);
  const auto& recomposition =
      *decomposed.replay_fitness_options.decomposed_alignment_options;
  assert(recomposition.max_states == 17 &&
         recomposition.max_total_states == 31 &&
         recomposition.max_time_total == 2.0 &&
         recomposition.max_time_trace == 0.5 &&
         recomposition.threshold_border_agreement == 9.0 &&
         !recomposition.use_cache && recomposition.clear_cache &&
         recomposition.log_move_cost == 11.0 &&
         recomposition.model_move_cost == 12.0 &&
          recomposition.tau_model_cost == 1.0 &&
          recomposition.sync_cost == 0.0);

  const auto marked_decomposed = minera::parse_evaluation_params(fields({
      {"evaluation_alignment_variant", "recompos_maximal"},
      {"evaluation_initial_marking", "0,2,0"},
      {"evaluation_final_marking", "0,0,2"}}));
  const auto& marked_recomposition =
      *marked_decomposed.replay_fitness_options.decomposed_alignment_options;
  assert((marked_recomposition.initial_marking ==
          std::vector<std::size_t>{0, 2, 0}));
  assert((marked_recomposition.final_marking ==
          std::vector<std::size_t>{0, 0, 2}));
}

void parse_evaluation_params_rejects_invalid_and_explicit_empty_values() {
  for (const auto& item : {
           std::pair{"evaluation_family", "unknown"},
           std::pair{"fitness_variant", "token"},
           std::pair{"evaluation_token_replay_variant", "reverse"},
           std::pair{"evaluation_initial_marking", "1,,2"},
           std::pair{"evaluation_initial_marking", "-1"},
           std::pair{"evaluation_final_marking", "999999999999999999999999"},
           std::pair{"fitness_consider_unknown_activities", "yes"},
           std::pair{"evaluation_alignment_variant", "unknown"},
           std::pair{"evaluation_alignment_max_states", "0"},
           std::pair{"evaluation_alignment_max_time", "-1"},
           std::pair{"evaluation_alignment_cost_profile", "other"},
           std::pair{"precision_variant", "align"},
           std::pair{"precision_use_task_ids", "yes"},
           std::pair{"precision_max_states", "0"},
           std::pair{"precision_max_states", "184467440737095516160"},
           std::pair{"generalization_variant", "token"},
           std::pair{"simplicity_variant", "cardoso"},
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

  for (const auto& item : {std::pair{"fitness_weight", "-1"},
                           std::pair{"precision_weight", "nan"},
                           std::pair{"simplicity_weight", "inf"}}) {
    bool threw = false;
    try {
      (void)minera::parse_evaluation_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool zero_weights_threw = false;
  try {
    (void)minera::parse_evaluation_params(fields({
        {"evaluation_family", "composite"}, {"fitness_weight", "0"},
        {"precision_weight", "0"}, {"simplicity_weight", "0"},
        {"generalization_weight", "0"}}));
  } catch (const std::invalid_argument&) {
    zero_weights_threw = true;
  }
  assert(zero_weights_threw);

  for (const auto& field_name : {"evaluation_family",
                                 "evaluation_activity_attribute",
                                 "evaluation_initial_marking",
                                 "evaluation_final_marking",
                                 "evaluation_token_replay_variant",
                                 "precision_use_task_ids",
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

  for (const auto& invalid : {
           std::map<std::string, std::string>{
               {"evaluation_alignment_variant", "dijkstra_less_memory"},
               {"evaluation_alignment_max_alignments", "2"}},
           std::map<std::string, std::string>{
               {"evaluation_alignment_variant", "recompos_maximal"},
               {"evaluation_alignment_exponent", "2"}},
           std::map<std::string, std::string>{
               {"evaluation_alignment_variant", "dijkstra_less_memory"},
               {"evaluation_alignment_max_total_states", "10"}},
           std::map<std::string, std::string>{
               {"evaluation_family", "precision"},
               {"evaluation_alignment_variant", "recompos_maximal"}},
           std::map<std::string, std::string>{
               {"evaluation_token_replay_variant", "backwards"},
               {"fitness_consider_unknown_activities", "true"}},
           std::map<std::string, std::string>{
               {"precision_variant", "auto"},
               {"precision_use_task_ids", "true"}},
           std::map<std::string, std::string>{
               {"precision_variant", "align_etconformance"},
               {"precision_use_task_ids", "true"}}}) {
    bool invalid_combination_threw = false;
    try {
      (void)minera::parse_evaluation_params(
          [&invalid](std::string_view name) {
            const auto it = invalid.find(std::string(name));
            return it == invalid.end() ? std::string{} : it->second;
          },
          [&invalid](std::string_view name) {
            return invalid.contains(std::string(name));
          });
    } catch (const std::invalid_argument&) {
      invalid_combination_threw = true;
    }
    assert(invalid_combination_threw);
  }
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
      {"dfg_activity_attribute", " phase "},
      {"dfg_timestamp_attribute", " occurred_at "},
      {"dfg_model_move_cost", "A=4"}, {"dfg_log_move_cost", "B=5"},
      {"dfg_internal_log_move_cost", "A=0.25"}}));
  assert(d.max_visited_states == 0 && d.max_time_ms == 12);
  assert(d.activity_attribute == "phase");
  assert(d.timestamp_attribute == "occurred_at");
  assert(d.sync_cost.at("A") == 3.0 && d.sync_cost.at("B") == 2.0);
  assert(d.sync_cost.at("A,B") == 4.0 && d.sync_cost.at("A=B") == 5.0);
  assert(d.sync_cost.at("A:B") == 6.0 && d.sync_cost.at("A\\B") == 7.0);
  assert(d.sync_cost.at(" A") == 8.0 && d.sync_cost.at("A ") == 9.0);
  assert(d.sync_cost.at("A\tB") == 10.0);
  assert(d.model_move_cost.at("A") == 4.0 && d.log_move_cost.at("B") == 5.0);
  assert(d.internal_log_move_cost.at("A") == 0.25);
  const auto e = minera::parse_edit_distance_params(fields({
      {"edit_variant", "Variants.EDIT_DISTANCE"}, {"anti", "1"},
      {"deviation_cost", "7"}, {"edit_activity_attribute", " phase "},
      {"edit_timestamp_attribute", " occurred_at "}}));
  assert(e.perform_anti_alignment && e.deviation_cost == 7);
  assert(e.activity_attribute == "phase");
  assert(e.timestamp_attribute == "occurred_at");

  const auto expect_throw = [](std::map<std::string, std::string> values,
                               bool dfg) {
    bool threw = false;
    const auto lookup = fields(values);
    const auto present = [&values](std::string_view name) {
      return values.contains(std::string(name));
    };
    try { if (dfg) (void)minera::parse_dfg_alignment_params(lookup, present);
          else (void)minera::parse_edit_distance_params(lookup, present); }
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
  expect_throw({{"dfg_activity_attribute", "  "}}, true);
  expect_throw({{"dfg_timestamp_attribute", "  "}}, true);
  expect_throw({{"edit_activity_attribute", "  "}}, false);
  expect_throw({{"edit_timestamp_attribute", "  "}}, false);
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
           {"heuristic_activity_attribute", "   "},
           {"heuristic_timestamp_attribute", "   "},
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
      {"temporal_activity_attribute", "phase"},
      {"temporal_case_id_attribute", "case_ref"},
      {"temporal_source_timestamp_attribute", "completed_at"},
      {"temporal_target_timestamp_attribute", "started_at"},
      {"temporal_business_hours", "true"},
      {"temporal_business_hour_slots", "0-3600,7200-10800"},
      {"temporal_strict_attributes", "false"},
      {"temporal_zeta", "-1"},
      {"spectrum_activities", " A, B "},
      {"spectrum_variant", "disconnected"},
      {"spectrum_sample_size", "17"},
      {"spectrum_sort_log", "false"},
      {"spectrum_activity_key", "phase"},
      {"spectrum_timestamp_key", "occurred_at"},
      {"spectrum_case_id_key", "case_ref"},
      {"batches", "true"},
      {"batch_variant", "polars"},
      {"batch_merge_distance_seconds", "12.5"},
      {"batch_min_size", "3"},
      {"batch_activity_attribute", "phase"},
      {"batch_resource_attribute", "team"},
      {"batch_timestamp_attribute", "finished_at"},
      {"batch_case_id_attribute", "case_ref"},
      {"batch_start_timestamp_attribute", "started_at"},
      {"batch_event_id_attribute", "event_ref"},
      {"minimum_self_distance_variant", "polars"},
      {"minimum_self_distance_activity_key", "phase"},
      {"minimum_self_distance_case_id_key", "case_ref"},
  }));
  assert(params.temporal_profile.activity_attribute == "phase");
  assert(params.temporal_profile.case_id_attribute == "case_ref");
  assert(params.temporal_profile.source_timestamp_attribute == "completed_at");
  assert(params.temporal_profile.target_timestamp_attribute == "started_at");
  assert(params.temporal_profile.use_business_hours);
  assert(params.temporal_profile.business_hour_slots_explicit);
  assert(params.temporal_profile.business_hour_slots.size() == 2);
  assert(!params.temporal_profile.strict_attributes);
  assert(params.temporal_profile.zeta == -1.0);
  assert((params.spectrum_activities == std::vector<std::string>{"A", "B"}));
  assert(params.spectrum.variant == minera::PerformanceSpectrumVariant::kDisconnected);
  assert(params.spectrum.sample_size == 17);
  assert(!params.spectrum.sort_log);
  assert(params.spectrum.activity_attribute == "phase");
  assert(params.spectrum.timestamp_attribute == "occurred_at");
  assert(params.spectrum.case_id_attribute == "case_ref");
  assert(params.batches);
  assert(params.batch_options.variant == minera::BatchDiscoveryVariant::kPolars);
  assert(params.batch_options.merge_distance_seconds == 12.5);
  assert(params.batch_options.min_batch_size == 3);
  assert(params.batch_options.activity_attribute == "phase");
  assert(params.batch_options.resource_attribute == "team");
  assert(params.batch_options.timestamp_attribute == "finished_at");
  assert(params.batch_options.case_id_attribute == "case_ref");
  assert(params.batch_options.start_timestamp_attribute == "started_at");
  assert(params.batch_options.event_id_attribute == "event_ref");
  assert(params.minimum_self_distance.variant ==
         minera::MinimumSelfDistanceVariant::kPolars);
  assert(params.minimum_self_distance.activity_key == "phase");
  assert(params.minimum_self_distance.case_id_key == "case_ref");
}

void parse_mine_params_preserves_empty_temporal_schedule() {
  const std::map<std::string, std::string> values = {
      {"temporal_business_hours", "true"},
      {"temporal_business_hour_slots", "none"}};
  const auto lookup = [&values](std::string_view key) {
    const auto found = values.find(std::string(key));
    return found == values.end() ? std::string{} : found->second;
  };
  const auto present = [&values](std::string_view key) {
    return values.contains(std::string(key));
  };
  const auto params = minera::parse_mine_params(lookup, present);
  assert(params.temporal_profile.use_business_hours);
  assert(params.temporal_profile.business_hour_slots_explicit);
  assert(params.temporal_profile.business_hour_slots.empty());
}

void parse_mine_params_accepts_unbounded_temporal_zeta() {
  const auto positive = minera::parse_mine_params(
      fields({{"temporal_zeta", "Infinity"}}));
  assert(std::isinf(positive.temporal_profile.zeta));
  assert(positive.temporal_profile.zeta > 0.0);

  const auto negative = minera::parse_mine_params(
      fields({{"temporal_zeta", "-Infinity"}}));
  assert(std::isinf(negative.temporal_profile.zeta));
  assert(negative.temporal_profile.zeta < 0.0);

  bool threw = false;
  try {
    (void)minera::parse_mine_params(fields({{"temporal_zeta", "NaN"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_mine_params_maps_overlap_options() {
  const auto params = minera::parse_mine_params(fields({
      {"overlap", "true"},
      {"overlap_activity_attribute", "phase"},
      {"overlap_start_attribute", "start"},
      {"overlap_completion_attribute", "complete"},
      {"overlap_epsilon", "0.25"},
      {"overlap_strict", "1"},
  }));
  assert(params.overlap);
  assert(params.overlap_options.activity_attribute == "phase");
  assert(params.overlap_options.start_timestamp_attribute == "start");
  assert(params.overlap_options.completion_timestamp_attribute == "complete");
  assert(params.overlap_options.epsilon == 0.25);
  assert(params.overlap_options.strict);
}

void parse_mine_params_rejects_invalid_overlap_epsilon() {
  bool rejected = false;
  try {
    (void)minera::parse_mine_params(fields({{"overlap", "true"},
                                             {"overlap_epsilon", "-1"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void parse_mine_params_rejects_invalid_spectrum_and_batch_options() {
  for (const auto& item : {
           std::pair{"spectrum_variant", "other"},
           std::pair{"spectrum_activities", "only_one"},
           std::pair{"spectrum_activities", "A,,B"},
           std::pair{"spectrum_sample_size", "-1"},
           std::pair{"spectrum_sort_log", "yes"},
           std::pair{"batches", "on"},
           std::pair{"batch_variant", "other"},
           std::pair{"batch_merge_distance_seconds", "nan"},
           std::pair{"batch_merge_distance_seconds", "-1"},
           std::pair{"batch_min_size", "1"},
           std::pair{"batch_min_size", "no"},
           std::pair{"minimum_self_distance_variant", "other"}}) {
    bool threw = false;
    try {
      (void)minera::parse_mine_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  for (const auto& name : {std::string("minimum_self_distance_activity_key"),
                           std::string("minimum_self_distance_case_id_key"),
                           std::string("batch_activity_attribute"),
                           std::string("batch_resource_attribute"),
                           std::string("batch_timestamp_attribute"),
                           std::string("batch_case_id_attribute"),
                           std::string("batch_event_id_attribute")}) {
    const std::map<std::string, std::string> empty_value = {{name, ""}};
    const auto lookup = [&empty_value](std::string_view key) -> std::string {
      const auto it = empty_value.find(std::string(key));
      return it == empty_value.end() ? std::string{} : it->second;
    };
    const auto present = [&empty_value](std::string_view key) {
      return empty_value.contains(std::string(key));
    };
    bool empty_threw = false;
    try {
      (void)minera::parse_mine_params(lookup, present);
    } catch (const std::invalid_argument&) {
      empty_threw = true;
    }
    assert(empty_threw);
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

  const auto zero_sample = minera::parse_mine_params(fields({
      {"spectrum_activities", "A,B"},
      {"spectrum_variant", "lazyframe_disconnected"},
      {"spectrum_sample_size", "0"},
  }));
  assert(zero_sample.spectrum.variant ==
         minera::PerformanceSpectrumVariant::kLazyframeDisconnected);
  assert(zero_sample.spectrum.sample_size == 0);

  threw = false;
  try {
    (void)minera::parse_mine_params(fields({
        {"batches", "true"},
        {"batch_variant", "log"},
        {"batch_event_id_attribute", "event_ref"},
    }));
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
           std::pair{"batch_variant", "log"},
           std::pair{"batch_merge_distance_seconds", "1"},
           std::pair{"batch_min_size", "2"},
           std::pair{"batch_activity_attribute", "phase"},
           std::pair{"batch_resource_attribute", "team"},
           std::pair{"batch_timestamp_attribute", "finished_at"},
           std::pair{"batch_case_id_attribute", "case_ref"},
           std::pair{"batch_event_id_attribute", "event_ref"}}) {
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
      {"petri_analysis_invariant_search_limit", "77"},
      {"petri_analysis_full_bootstrap", "false"},
      {"petri_analysis_diagnostics", "true"},
      {"petri_analysis_return_asap_when_unsound", "true"}}));
  assert(parsed.family == minera::PetriAnalysisFamily::kExtendedMarkingEquation);
  assert(parsed.miner == "powl" && parsed.trace_index == 3);
  assert(parsed.initial_marking == minera::Marking({1, 0}));
  assert(parsed.final_marking == minera::Marking({0, 1}));
  assert(parsed.model_transition_costs == std::vector<double>({4, 5}));
  assert(parsed.synchronous_transition_costs == std::vector<double>({0, 1}));
  assert(parsed.log_move_costs == std::vector<double>({9, 10}));
  assert(parsed.split_indices == std::vector<std::size_t>({1, 3}));
  assert(parsed.max_k_value == 2 && parsed.max_states == 42 &&
         parsed.invariant_search_limit == 77);
  assert(!parsed.full_bootstrap_required && parsed.diagnostics &&
         parsed.return_asap_when_unsound);
}

void parse_petri_analysis_params_rejects_bad_values() {
  for (const auto& item : {
           std::pair{"petri_analysis_family", "bad"},
           std::pair{"petri_analysis_max_states", "0"},
           std::pair{"petri_analysis_invariant_search_limit", "0"},
           std::pair{"petri_analysis_model_costs", "1,-1"},
           std::pair{"petri_analysis_return_asap_when_unsound", "maybe"}}) {
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

void parse_petri_analysis_params_accepts_woflan_alias() {
  const auto parsed = minera::parse_petri_analysis_params(
      fields({{"petri_analysis_family", "woflan"},
              {"petri_analysis_miner", "alpha"}}));
  assert(parsed.family == minera::PetriAnalysisFamily::kWoflan);
}

void parse_process_tree_playout_params_maps_variants_and_limits() {
  const auto omitted = minera::parse_process_tree_playout_params(fields({}));
  assert(omitted.variant == minera::ProcessTreePlayoutVariant::kTopBottom);
  const auto parsed = minera::parse_process_tree_playout_params(fields({
      {"playout_variant", "EXTENSIVE"},
      {"playout_traces", "12"},
      {"playout_max_steps", "99"},
      {"playout_seed", "7"},
      {"playout_min_length", "2"},
      {"playout_max_length", "8"},
      {"playout_max_loop_occurrences", "4"},
      {"playout_max_trace_cap", "50"},
      {"playout_return_set_strings", "true"}}));
  assert(parsed.variant == minera::ProcessTreePlayoutVariant::kExtensive);
  assert(parsed.traces == 12 && parsed.max_steps == 99 && parsed.seed == 7);
  assert(parsed.min_length == 2 && parsed.max_length == 8);
  assert(parsed.max_loop_occurrences == 4 && parsed.max_traces == 50);
  assert(parsed.return_set_strings);
}

void parse_declare_playout_params_is_strict_and_bounded() {
  const auto parsed = minera::parse_declare_playout_params(fields({
      {"traces", "12"}, {"min_length", "2"}, {"max_length", "8"},
      {"seed", "7"}}));
  assert(parsed.n_traces == 12 && parsed.min_length == 2 &&
         parsed.max_length == 8 && parsed.seed == 7);
  for (const auto& item : {std::pair{"traces", "100001"},
                           std::pair{"min_length", "-1"},
                           std::pair{"max_length", "10001"}}) {
    bool threw = false;
    try {
      (void)minera::parse_declare_playout_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool threw = false;
  try {
    (void)minera::parse_declare_playout_params(
        fields({{"min_length", "4"}, {"max_length", "2"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
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
  threw = false;
  try {
    (void)minera::parse_process_tree_playout_params(
        fields({{"playout_return_set_strings", "true"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void parse_extensive_playout_params_maps_controls_and_rejects_zero() {
  const auto parsed = minera::parse_extensive_playout_params(fields({
      {"extensive_return_elements", "true"},
      {"extensive_max_marking_occ", "3"},
      {"extensive_max_length", "4"}}));
  assert(parsed.return_elements);
  assert(parsed.max_marking_occ && *parsed.max_marking_occ == 3);
  assert(parsed.max_length == 4);
  bool rejected = false;
  try {
    (void)minera::parse_extensive_playout_params(
        fields({{"extensive_max_marking_occ", "0"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void parse_ocpn_playout_params_maps_return_traces() {
  const auto parsed = minera::parse_ocpn_playout_params(fields({
      {"ocpn_return_traces", "true"}}));
  assert(parsed.return_traces);
  const auto http_alias = minera::parse_ocpn_playout_params(fields({
      {"return_traces", "true"}}));
  assert(http_alias.return_traces);
  const auto disabled = minera::parse_ocpn_playout_params(fields({
      {"return_traces", "false"}}));
  assert(!disabled.return_traces);
  bool rejected = false;
  try {
    (void)minera::parse_ocpn_playout_params(
        fields({{"return_traces", "maybe"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void parse_stochastic_petri_params_maps_and_rejects_bad_values() {
  const auto parsed = minera::parse_stochastic_petri_params(fields({
      {"stochastic_petri_rates", "2, 3.5"},
      {"stochastic_petri_initial_marking", "1,0"},
      {"stochastic_petri_immediate_transitions", "0,2"},
      {"stochastic_petri_max_states", "12"},
      {"stochastic_petri_analysis", "transient"},
      {"stochastic_petri_time", "1.5"},
      {"stochastic_petri_initial_state", "1"}}));
  assert(parsed.transition_rates.size() == 2);
  assert(parsed.transition_rates[1] == 3.5);
  assert(parsed.initial_marking == std::vector<std::uint32_t>({1, 0}));
  assert(parsed.immediate_transitions == std::vector<std::size_t>({0, 2}));
  assert(parsed.max_states == 12);
  assert(parsed.analysis == minera::StochasticPetriAnalysisVariant::kTransient);
  assert(parsed.analysis_time_seconds == 1.5);
  assert(parsed.analysis_initial_state && *parsed.analysis_initial_state == 1);
  for (const auto& item : {std::pair{"stochastic_petri_rates", "-1"},
                           std::pair{"stochastic_petri_rates", "nan"},
                           std::pair{"stochastic_petri_max_states", "0"},
                           std::pair{"stochastic_petri_analysis", "bad"},
                           std::pair{"stochastic_petri_time", "-1"},
                           std::pair{"stochastic_petri_time", "1000001"},
                           std::pair{"stochastic_petri_initial_probabilities", "0.2,0.2"}}) {
    bool rejected = false;
    try {
      (void)minera::parse_stochastic_petri_params(fields({item}));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  bool mismatch = false;
  try {
    (void)minera::parse_stochastic_petri_params(fields({
        {"stochastic_petri_initial_state", "0"}}));
  } catch (const std::invalid_argument&) {
    mismatch = true;
  }
  assert(mismatch);
}

void parse_stochastic_map_params_maps_and_rejects_bad_values() {
  const auto parsed = minera::parse_stochastic_map_params(fields({
      {"stochastic_map_source", "performance_dfg"},
      {"stochastic_map_distribution", "exponential"},
      {"stochastic_map_default_parameter", "2"},
      {"stochastic_map_conversion_variant", "invisibles"},
      {"stochastic_map_default_activation_weight", "0.5"},
      {"stochastic_map_frequency_activation_weights", "false"},
      {"stochastic_map_max_states", "17"},
      {"stochastic_map_initial_marking", "1,0"}}));
  assert(parsed.source == minera::StochasticMapSource::kPerformanceDfg);
  assert(parsed.performance.distribution ==
         minera::StochasticMapDistribution::kExponential);
  assert(parsed.performance.default_parameter == 2.0);
  assert(parsed.performance.conversion.variant ==
         minera::DfgPetriConversionVariant::kInvisiblesNoDuplicates);
  assert(parsed.performance.default_activation_weight == 0.5);
  assert(!parsed.performance.frequency_activation_weights);
  assert(parsed.max_states == 17 && parsed.initial_marking.size() == 2);

  bool threw = false;
  try {
    (void)minera::parse_stochastic_map_params(fields({
        {"stochastic_map_source", "other"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_stochastic_map_params(fields({
        {"stochastic_map_distribution", "exponential"},
        {"stochastic_map_default_parameter", "0"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_stochastic_map_params(fields({
        {"stochastic_map_distribution", "gamma"},
        {"stochastic_map_default_parameter", "0"}}));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::parse_stochastic_map_params(fields({
        {"stochastic_map_max_states", "0"}}));
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
      {"dfg_playout_start_time", "2026-01-01T00:00:00Z"},
      {"dfg_playout_activity_key", "event:name"},
      {"dfg_playout_timestamp_key", "event:when"},
      {"dfg_playout_case_id_key", "case:external_id"},
      {"dfg_playout_artificial_start_activity", "BEGIN"},
      {"dfg_playout_artificial_end_activity", "FINISH"}}));
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
  assert(parsed.activity_key == "event:name" &&
         parsed.timestamp_key == "event:when" &&
         parsed.case_id_key == "case:external_id");
  assert(parsed.artificial_start_activity == "BEGIN" &&
         parsed.artificial_end_activity == "FINISH");
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
  for (const auto& name : {std::string("dfg_playout_activity_key"),
                           std::string("dfg_playout_artificial_end_activity")}) {
    bool threw = false;
    try {
      (void)minera::parse_dfg_playout_params(
          fields({{name, ""}}),
          [&](std::string_view field) { return field == name; });
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
      {"stochastic_fit_from_log", "true"},
      {"stochastic_stop_at_final", "false"},
      {"stochastic_return_visited_elements", "true"},
      {"stochastic_start", "1767225600"},
      {"stochastic_case_interval", "2.5"},
      {"stochastic_event_interval", "0.25"},
      {"stochastic_initial_marking", "0, 1, 0"},
      {"stochastic_final_marking", "0, 0, 1"}}));
  assert(parsed.traces == 12 && parsed.max_steps == 99 && parsed.seed == 7);
  assert(parsed.transition_weights == std::vector<double>({0.25, 2, 0}));
  assert(parsed.fit_from_log);
  assert(!parsed.stop_at_final && parsed.return_visited_elements &&
         parsed.start == 1767225600.0);
  assert(parsed.case_interval == 2.5 && parsed.event_interval == 0.25);
  assert(parsed.initial_marking == std::vector<std::uint32_t>({0, 1, 0}));
  assert(parsed.final_marking == std::vector<std::uint32_t>({0, 0, 1}));
}

void parse_stochastic_playout_params_rejects_bad_values() {
  for (const auto& item : {
           std::pair{"stochastic_traces", "0"},
           std::pair{"stochastic_max_steps", "-1"},
           std::pair{"stochastic_transition_weights", "1,-0.1"},
           std::pair{"stochastic_transition_weights", "1,"},
           std::pair{"stochastic_fit_from_log", "maybe"},
           std::pair{"stochastic_stop_at_final", "maybe"},
           std::pair{"stochastic_return_visited_elements", "maybe"},
           std::pair{"stochastic_event_interval", "-1"},
           std::pair{"stochastic_initial_marking", "0,,1"},
           std::pair{"stochastic_final_marking", "4294967296"}}) {
    bool threw = false;
    try {
      (void)minera::parse_stochastic_playout_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_monte_carlo_params_maps_controls() {
  const auto parsed = minera::parse_monte_carlo_params(fields({
      {"monte_carlo_simulations", "12"},
      {"monte_carlo_case_arrival_seconds", "2.5"},
      {"monte_carlo_max_steps", "99"},
      {"monte_carlo_seed", "7"},
      {"monte_carlo_start", "1767225600"},
      {"monte_carlo_default_duration_seconds", "3"},
      {"monte_carlo_force_distribution", " EXPONENTIAL "},
      {"monte_carlo_default_resources_per_place", "2"},
      {"monte_carlo_transition_weights", "0.25, 2, 0"},
      {"monte_carlo_transition_durations",
       "deterministic:1, EXPONENTIAL=2, normal:3;1, uniform:0;2, "
       "lognormal:0.5;0;1, gamma:2;0;1, immediate:0"},
      {"monte_carlo_resources_per_place", "0=2, 3=4"},
      {"monte_carlo_enable_diagnostics", "false"},
      {"monte_carlo_stop_at_final", "false"}}));
  assert(parsed.simulations == 12 && parsed.case_arrival_seconds == 2.5 &&
         parsed.max_steps == 99 && parsed.seed == 7);
  assert(parsed.start == 1767225600.0 &&
         parsed.default_duration_seconds == 3.0);
  assert(parsed.force_distribution &&
         *parsed.force_distribution ==
             minera::MonteCarloDurationDistribution::kExponential);
  assert(parsed.default_resources_per_place == 2 &&
         parsed.transition_weights == std::vector<double>({0.25, 2, 0}));
  assert(parsed.transition_durations.size() == 7);
  assert(parsed.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kDeterministic);
  assert(parsed.transition_durations[1].distribution ==
         minera::MonteCarloDurationDistribution::kExponential);
  assert(parsed.transition_durations[1].value_seconds == 2.0);
  assert(parsed.transition_durations[2].distribution ==
         minera::MonteCarloDurationDistribution::kNormal);
  assert(parsed.transition_durations[2].secondary_seconds == 1.0);
  assert(parsed.transition_durations[3].distribution ==
         minera::MonteCarloDurationDistribution::kUniform);
  assert(parsed.transition_durations[3].secondary_seconds == 2.0);
  assert(parsed.transition_durations[4].distribution ==
         minera::MonteCarloDurationDistribution::kLogNormal);
  assert(parsed.transition_durations[4].tertiary_seconds == 1.0);
  assert(parsed.transition_durations[5].distribution ==
         minera::MonteCarloDurationDistribution::kGamma);
  assert(parsed.transition_durations[6].distribution ==
         minera::MonteCarloDurationDistribution::kImmediate);
  assert(parsed.resources_per_place.at(0) == 2 &&
         parsed.resources_per_place.at(3) == 4);
  assert(!parsed.enable_diagnostics && !parsed.stop_at_final);
}

void parse_monte_carlo_params_rejects_bad_values() {
  for (const auto& item : {
           std::pair{"monte_carlo_simulations", "0"},
           std::pair{"monte_carlo_case_arrival_seconds", "-1"},
           std::pair{"monte_carlo_force_distribution", "deterministic"},
           std::pair{"monte_carlo_transition_weights", "1,"},
           std::pair{"monte_carlo_transition_durations", "normal:1"},
           std::pair{"monte_carlo_transition_durations", "lognormal:1;0;0"},
           std::pair{"monte_carlo_transition_durations", "unknown:1"},
           std::pair{"monte_carlo_resources_per_place", "1=0"},
           std::pair{"monte_carlo_enable_diagnostics", "maybe"}}) {
    bool threw = false;
    try {
      (void)minera::parse_monte_carlo_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void parse_monte_carlo_params_maps_extended_contract() {
  const auto parsed = minera::parse_monte_carlo_params(fields({
      {"monte_carlo_simulations", "12"},
      {"monte_carlo_activity_key", "  activity_name  "},
      {"monte_carlo_timestamp_key", " observed_at "},
      {"monte_carlo_initial_marking", "0, 1, 2"},
      {"monte_carlo_final_marking", "3, 4, 5"},
      {"monte_carlo_case_arrival_ratio", "1.25"},
      {"monte_carlo_diagnostic_interval", "7"},
      {"monte_carlo_small_scale_factor", "0.5"},
      {"monte_carlo_max_thread_execution_time", "2.25"},
      {"monte_carlo_token_replay_variant", " backwards "},
      {"monte_carlo_provided_stochastic_map",
       "2:immediate:normal:1:0.5:0:3:-2,0:timed:lognormal:2:0.1:1:0.75:4"}}));
  assert(parsed.activity_key == "activity_name");
  assert(parsed.timestamp_key == "observed_at");
  assert((parsed.initial_marking ==
          std::vector<std::uint32_t>{0, 1, 2}));
  assert((parsed.final_marking == std::vector<std::uint32_t>{3, 4, 5}));
  assert(parsed.case_arrival_ratio.has_value() &&
         *parsed.case_arrival_ratio == 1.25);
  assert(!parsed.case_arrival_seconds.has_value());
  assert(parsed.diagnostic_interval == 7 &&
         parsed.small_scale_factor == 0.5 &&
         parsed.max_thread_execution_time == 2.25);
  assert(parsed.token_replay_variant == minera::TokenReplayVariant::kBackwards);
  assert(parsed.stochastic_map.has_value());
  assert(parsed.stochastic_map->transitions.size() == 2);
  assert(parsed.stochastic_map->transitions[0].transition == 2);
  assert(parsed.stochastic_map->transitions[0].timing ==
         minera::StochasticMapTiming::kImmediate);
  assert(parsed.stochastic_map->transitions[0].distribution ==
         minera::StochasticMapDistribution::kNormal);
  assert(parsed.stochastic_map->transitions[0].parameter == 1.0);
  assert(parsed.stochastic_map->transitions[0].secondary_parameter == 0.5);
  assert(parsed.stochastic_map->transitions[0].tertiary_parameter == 0.0);
  assert(parsed.stochastic_map->transitions[0].activation_weight == 3.0);
  assert(parsed.stochastic_map->transitions[0].priority == -2);
  assert(parsed.stochastic_map->transitions[1].transition == 0);
  assert(parsed.stochastic_map->transitions[1].timing ==
         minera::StochasticMapTiming::kTimed);
  assert(parsed.stochastic_map->transitions[1].distribution ==
         minera::StochasticMapDistribution::kLogNormal);
  assert(parsed.stochastic_map->transitions[1].tertiary_parameter == 1.0);

  const auto empty_map = minera::parse_monte_carlo_params(fields({
      {"monte_carlo_provided_stochastic_map", " empty "}}));
  assert(empty_map.stochastic_map.has_value() &&
         empty_map.stochastic_map->transitions.empty());

  const auto legacy = minera::parse_monte_carlo_params(fields({
      {"monte_carlo_case_arrival_seconds", "2.5"},
      {"monte_carlo_token_replay_variant", "token_replay"}}));
  assert(legacy.case_arrival_seconds.has_value() &&
         *legacy.case_arrival_seconds == 2.5);
  assert(!legacy.case_arrival_ratio.has_value());
  assert(legacy.token_replay_variant == minera::TokenReplayVariant::kForward);
}

void parse_monte_carlo_params_rejects_extended_bad_values() {
  for (const auto& item : {
           std::pair{"monte_carlo_case_arrival_ratio", "-1"},
           std::pair{"monte_carlo_diagnostic_interval", "0"},
           std::pair{"monte_carlo_small_scale_factor", "0"},
           std::pair{"monte_carlo_small_scale_factor", "NaN"},
           std::pair{"monte_carlo_max_thread_execution_time", "-1"},
           std::pair{"monte_carlo_activity_key", "  "},
           std::pair{"monte_carlo_timestamp_key", "\t"},
           std::pair{"monte_carlo_initial_marking", "0,,1"},
           std::pair{"monte_carlo_final_marking", "4294967296"},
           std::pair{"monte_carlo_token_replay_variant", "reverse"},
           std::pair{"monte_carlo_provided_stochastic_map", " "},
           std::pair{"monte_carlo_provided_stochastic_map", "0:timed"},
           std::pair{"monte_carlo_provided_stochastic_map",
                     "0:timed:exponential:1:nan:0:1:0"},
           std::pair{"monte_carlo_provided_stochastic_map",
                     "0:timed:exponential:1:0:0:1:0,0:timed:exponential:1:0:0:1:0"},
           std::pair{"monte_carlo_provided_stochastic_map",
                     "0:timed:unsupported:1:0:0:1:0"},
           std::pair{"monte_carlo_provided_stochastic_map",
                     "0:timed:exponential:1:0:0:1:not-an-int"}}) {
    bool threw = false;
    try {
      (void)minera::parse_monte_carlo_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  bool conflict = false;
  try {
    (void)minera::parse_monte_carlo_params(fields({
        {"monte_carlo_case_arrival_ratio", "1"},
        {"monte_carlo_case_arrival_seconds", "2"}}));
  } catch (const std::invalid_argument&) {
    conflict = true;
  }
  assert(conflict);
}

void parse_monte_carlo_params_bounds_supplied_maps_before_accumulation() {
  const auto records =
      "0:timed:deterministic:1:0:0:1:0,"
      "1:timed:deterministic:1:0:0:1:0,"
      "2:timed:deterministic:1:0:0:1:0";
  const auto values = fields({
      {"monte_carlo_provided_stochastic_map", records}});

  const auto within_limit = minera::parse_monte_carlo_params(
      values, {}, 3);
  assert(within_limit.stochastic_map.has_value());
  assert(within_limit.stochastic_map->transitions.size() == 3);

  bool threw = false;
  try {
    (void)minera::parse_monte_carlo_params(values, {}, 2);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)minera::parse_monte_carlo_params(values, {}, 0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // The explicit empty marker remains valid even when the selected net has
  // no transitions and therefore supplies a zero record limit.
  const auto empty = minera::parse_monte_carlo_params(
      fields({{"monte_carlo_provided_stochastic_map", "empty"}}), {}, 0);
  assert(empty.stochastic_map.has_value());
  assert(empty.stochastic_map->transitions.empty());

  // The ordinary overload uses the conservative request-level hard limit.
  assert(minera::kMonteCarloRequestMapMaxRecords >= 3);
  const auto default_limit = minera::parse_monte_carlo_params(values);
  assert(default_limit.stochastic_map->transitions.size() == 3);
}

void log_to_features_params_contract() {
  const std::map<std::string, std::string> explicit_empty{
      {"features_variant", "event_based"}, {"features_event_string_attributes", ""},
      {"feature_event_numeric_attributes", ""}, {"features_feature_names", ""}};
  const auto lookup = [&](std::string_view key) {
    const auto it = explicit_empty.find(std::string(key));
    return it == explicit_empty.end() ? std::string{} : it->second;
  };
  const auto present = [&](std::string_view key) { return explicit_empty.contains(std::string(key)); };
  const auto empty_lists = minera::parse_log_to_features_params(lookup, present);
  assert(empty_lists.event_string_attributes_supplied && empty_lists.event_string_attributes.empty());
  assert(empty_lists.event_numeric_attributes_supplied && empty_lists.event_numeric_attributes.empty());
  assert(empty_lists.feature_names_supplied && empty_lists.feature_names.empty());
  const auto exact_schema = minera::parse_log_to_features_params(
      [](std::string_view key) { return key == "features_variant" ? std::string("event_based") : key == "features_feature_names" ? std::string(R"(\e, event:n ,a\,b,\\e)") : std::string{}; },
      [](std::string_view key) { return key == "features_variant" || key == "features_feature_names"; });
  assert((exact_schema.feature_names == std::vector<std::string>{"", " event:n ", "a,b", R"(\e)"}));
  for (const auto& invalid : {R"(x\e)", R"(\ex)", R"(\q)", R"(x\)"}) {
    bool rejected = false;
    try {
      (void)minera::parse_log_to_features_params(
          [&](std::string_view key) { return key == "features_variant" ? std::string("event_based") : key == "features_feature_names" ? std::string(invalid) : std::string{}; },
          [](std::string_view key) { return key == "features_variant" || key == "features_feature_names"; });
    } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
  const auto trace_empty = minera::parse_log_to_features_params(
      [](std::string_view) { return std::string{}; },
      [](std::string_view key) { return key == "features_case_string_attributes" || key == "features_case_numeric_attributes" || key == "features_event_successor_attributes" || key == "features_feature_names"; });
  assert(trace_empty.variant == minera::LogToFeaturesVariant::TRACE_BASED);
  assert(trace_empty.case_id_key == "concept:name");
  const auto identifier_key = minera::parse_log_to_features_params(fields({{"features_case_id_key", "custom:id"}}));
  assert(identifier_key.case_id_key == "custom:id");
  assert(minera::parse_log_to_features_params(fields({{"feature_case_id_key", "alias:id"}})).case_id_key == "alias:id");

  assert(trace_empty.case_string_attributes_supplied && trace_empty.case_string_attributes.empty());
  assert(trace_empty.case_numeric_attributes_supplied && trace_empty.case_numeric_attributes.empty());
  assert(trace_empty.event_successor_attributes_supplied && trace_empty.event_successor_attributes.empty());
  assert(trace_empty.feature_names_supplied && trace_empty.feature_names.empty());
  const auto extra_overrides = minera::parse_log_to_features_params(fields({
      {"features_enable_all_extra_features", "true"},
      {"features_enable_case_duration", "false"},
      {"feature_enable_resource_workload", "false"},
      {"features_min_num_different_string_values", "3"},
      {"features_max_num_different_string_values", "2"}}));
  assert(extra_overrides.enable_all_extra_features);
  assert(extra_overrides.enable_case_duration_supplied && !extra_overrides.enable_case_duration);
  assert(extra_overrides.enable_resource_workload_supplied && !extra_overrides.enable_resource_workload);
  assert(!extra_overrides.enable_work_in_progress_supplied);

  const auto no_presence = minera::parse_log_to_features_params(lookup);
  assert(!no_presence.event_string_attributes_supplied && !no_presence.event_numeric_attributes_supplied && !no_presence.feature_names_supplied);
  const auto omitted = minera::parse_log_to_features_params(fields({{"features_variant", "event_based"}}));
  assert(!omitted.event_string_attributes_supplied && !omitted.event_numeric_attributes_supplied);
  const auto plural_empty = minera::parse_log_to_features_params(
      [](std::string_view key) { return key == "features_variant" ? std::string("event_based") : key == "feature_event_numeric_attributes" ? std::string("cost") : std::string{}; },
      [](std::string_view key) { return key == "features_variant" || key == "features_event_numeric_attributes" || key == "feature_event_numeric_attributes"; });
  assert(plural_empty.event_numeric_attributes_supplied && plural_empty.event_numeric_attributes.empty());

  const auto parsed = minera::parse_log_to_features_params(fields({
      {"features_variant", "temporal"},
      {"features_activity_key", "concept:name"},
      {"features_grouper_freq", "W-MON"},
      {"features_window_seconds", "60"},
      {"features_event_numeric_attributes", "cost,amount"},
      {"features_enable_all_extra_features", "true"},
      {"features_event_successor_attributes", "concept:name"},
      {"features_epsilon", "0.25"},
      {"features_min_num_different_string_values", "1"},
      {"features_max_num_different_string_values", "7"},
      {"features_feature_names", "arrivals,finishes"}}));
  assert(parsed.variant == minera::LogToFeaturesVariant::TEMPORAL);
  assert(parsed.activity_key == "concept:name" && parsed.window_seconds == 60.0 &&
         parsed.grouper_freq == "W-MON");
  assert(parsed.event_numeric_attributes.size() == 2 && parsed.feature_names.size() == 2);
  assert(parsed.enable_all_extra_features && parsed.event_successor_attributes.size() == 1 &&
         parsed.epsilon == 0.25 && parsed.max_num_different_string_values == 7);
  const auto fixed_width = minera::parse_log_to_features_params(
      fields({{"features_window_seconds", "60"}}));
  assert(fixed_width.grouper_freq.empty() && fixed_width.window_seconds == 60.0);
  bool threw = false;
  try { (void)minera::parse_log_to_features_params(fields({{"features_variant", "bogus"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_log_to_features_params(fields({{"features_window_seconds", "0"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_log_to_features_params(fields({{"features_epsilon", "-1"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_log_to_features_params(fields({{"features_variant", "event_based"}, {"features_min_num_different_string_values", "3"}, {"features_max_num_different_string_values", "2"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void embedding_params_contract() {
  const auto parsed = minera::parse_embedding_params(fields({
      {"embedding_variant", "events"},
      {"embedding_provider", "transformer"},
      {"embedding_model", "hashing-v1"},
      {"embedding_attribute_key", "concept:name"},
      {"embedding_event_id_key", "event_id"},
      {"embedding_dimensions", "32"},
      {"embedding_max_items", "17"},
      {"embedding_keep_cases", "true"}}));
  assert(parsed.variant == minera::EmbeddingVariant::EVENTS_TRANSFORMERS);
  assert(parsed.provider == minera::EmbeddingProvider::TRANSFORMER);
  assert(parsed.embedding_model == "hashing-v1" &&
         parsed.attribute_key == "concept:name" &&
         parsed.event_id_key == "event_id");
  assert(parsed.dimensions == 32 && parsed.max_items == 17 &&
         parsed.keep_cases);
  const auto inferred = minera::parse_embedding_params(fields({
      {"embedding_provider", "transformer"}, {"embedding_dimensions", "0"}}));
  assert(inferred.provider == minera::EmbeddingProvider::TRANSFORMER &&
         inferred.dimensions == 0);
  bool threw = false;
  try { (void)minera::parse_embedding_params(fields({{"embedding_variant", "bogus"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_embedding_params(fields({{"embedding_dimensions", "0"}})); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::parse_embedding_params(fields({{"embedding_provider", "bogus"}})); }
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
      {"ilp_artificial_start_activity", "<begin>"},
      {"ilp_artificial_end_activity", "<finish>"},
      {"ilp_alpha", "0.75"},
      {"ilp_causal_relation", "<begin>=>A,A=>B,B=><finish>"},
      {"ilp_noise_threshold", "0.25"},
      {"ilp_max_nodes", "17"}}));
  assert(parsed.activity_key == "concept:name");
  assert(parsed.artificial_start_activity == "<begin>");
  assert(parsed.artificial_end_activity == "<finish>");
  assert(parsed.alpha == 0.75);
  assert(parsed.causal_relation.size() == 3);
  assert(parsed.noise_threshold == 0.25);
  assert(parsed.max_nodes == 17);
  for (const auto& item : {std::pair{"ilp_noise_threshold", "-0.1"},
                           std::pair{"ilp_noise_threshold", "1.1"},
                           std::pair{"ilp_alpha", "-0.1"},
                           std::pair{"ilp_causal_relation", "A"},
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
      {"genetic_timestamp_key", "occurred_at"},
      {"genetic_case_id_key", "case_ref"},
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
  assert(parsed.timestamp_key == "occurred_at" && parsed.case_id_key == "case_ref");
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
  for (const auto& item : {std::pair{"genetic_timestamp_key", ""},
                           std::pair{"genetic_case_id_key", ""}}) {
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
      {"sna_variant", "working_together_pandas"},
      {"sna_beta", "0.25"},
      {"sna_n", "4"},
      {"sna_metric_normalization", "true"},
      {"sna_activity_attribute", "team:activity"},
      {"sna_resource_attribute", "team:resource"},
      {"sna_case_id_attribute", "case:batch"},
      {"sna_max_resources", "20"},
      {"sna_max_edges", "80"}}));
  assert(parsed.variant == minera::OrganizationalSnaVariant::kWorkingTogetherPandas);
  assert(parsed.beta == 0.25 && parsed.n == 4 && parsed.metric_normalization);
  assert(parsed.max_resources == 20 && parsed.max_edges == 80);
  assert(parsed.activity_attribute == "team:activity" &&
         parsed.resource_attribute == "team:resource");
  assert(parsed.case_id_attribute == "case:batch");
  for (const auto& [wire, expected] : {
           std::pair{"handover_pandas", minera::OrganizationalSnaVariant::kHandoverPandas},
           std::pair{"working_together_pandas", minera::OrganizationalSnaVariant::kWorkingTogetherPandas},
           std::pair{"subcontracting_pandas", minera::OrganizationalSnaVariant::kSubcontractingPandas},
           std::pair{"jointactivities_pandas", minera::OrganizationalSnaVariant::kJointActivitiesPandas}}) {
    const auto variant = minera::parse_organizational_sna_params(
        fields({{"sna_variant", wire}}));
    assert(variant.variant == expected);
  }
  for (const auto& item : {std::pair{"sna_variant", "bad"},
                           std::pair{"sna_beta", "1.1"},
                           std::pair{"sna_beta", "nan"},
                           std::pair{"sna_n", "0"},
                           std::pair{"sna_case_id_attribute", ""},
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
  bool unsupported_case_selector = false;
  try {
    (void)minera::parse_organizational_sna_params(
        fields({{"sna_variant", "jointactivities_pandas"},
                {"sna_case_id_attribute", "case:batch"}}));
  } catch (const std::invalid_argument&) {
    unsupported_case_selector = true;
  }
  assert(unsupported_case_selector);
}

void role_discovery_params_contract() {
  const auto parsed = minera::parse_role_discovery_params(fields({
      {"roles_variant", "pandas"},
      {"roles_threshold", "0.4"},
      {"roles_activity_attribute", "team:activity"},
      {"roles_resource_attribute", "team:resource"},
      {"roles_max_resources", "8"},
      {"roles_max_activities", "12"}}));
  assert(parsed.variant == minera::RoleDiscoveryVariant::kPandas);
  assert(parsed.threshold == 0.4 && parsed.max_resources == 8 &&
         parsed.max_activities == 12);
  assert(parsed.activity_attribute == "team:activity" &&
         parsed.resource_attribute == "team:resource");
  for (const auto& item : {std::pair{"roles_variant", "other"},
                           std::pair{"roles_threshold", "1.1"},
                           std::pair{"roles_threshold", "nan"},
                           std::pair{"roles_max_resources", "0"},
                           std::pair{"roles_max_activities", "0"}}) {
    bool threw = false;
    try { (void)minera::parse_role_discovery_params(fields({item})); }
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
      {"tvq_secure_random", "true"},
      {"tvq_max_output_traces", "40"},
      {"tvq_max_output_variants", "7"}}));
  assert(parsed.variant == minera::TraceVariantQueryVariant::kSacofa);
  assert(parsed.epsilon == 0.75 && parsed.max_prefix_length == 4);
  assert(parsed.prune_threshold == 2 && parsed.seed == 19);
  assert(parsed.max_output_traces == 40 && parsed.max_output_variants == 7);
  assert(parsed.secure_random);
  for (const auto& item : {std::pair{"tvq_variant", "other"},
                           std::pair{"tvq_epsilon", "0"},
                           std::pair{"tvq_k", "-1"},
                           std::pair{"tvq_p", "0"},
                           std::pair{"tvq_secure_random", "maybe"}}) {
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
      {"pripel_secure_random", "1"},
      {"pripel_max_output_traces", "20"},
      {"pripel_max_distance", "2"}}));
  assert(parsed.epsilon == 0.5);
  assert((parsed.blocklist == std::vector<std::string>{"case_id", "resource"}));
  assert(parsed.seed == 8 && parsed.max_output_traces == 20 &&
         parsed.max_distance == 2);
  assert(parsed.secure_random);
  for (const auto& item : {std::pair{"pripel_epsilon", "0"},
                           std::pair{"pripel_blocklist", "a,a"},
                           std::pair{"pripel_seed", "-1"},
                           std::pair{"pripel_max_distance", "-1"},
                           std::pair{"pripel_secure_random", "unknown"}}) {
    bool threw = false;
    try { (void)minera::parse_pripel_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void link_analysis_params_contract() {
  const auto parsed = minera::parse_link_analysis_params(fields({
      {"link_analysis_out_column", "source_id"},
      {"link_analysis_in_column", "target_id"},
      {"link_analysis_sorting_column", "started_at"},
      {"link_analysis_index_column", "row_id"},
      {"link_analysis_look_forward", "false"},
      {"link_analysis_keep_first_occurrence", "true"},
      {"link_analysis_propagate", "true"},
      {"link_analysis_max_links", "17"}}));
  assert(parsed.out_column == "source_id" && parsed.in_column == "target_id" &&
         parsed.sorting_column == "started_at" &&
         parsed.index_column == "row_id");
  assert(!parsed.look_forward && parsed.keep_first_occurrence &&
         parsed.propagate && parsed.max_links == 17);
  for (const auto& item : {
           std::pair{"link_analysis_look_forward", "maybe"},
           std::pair{"link_analysis_max_links", "0"}}) {
    bool threw = false;
    try { (void)minera::parse_link_analysis_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void timestamp_interleavings_params_contract() {
  const auto parsed = minera::parse_timestamp_interleavings_params(fields({
      {"interleavings_timestamp_key", "started_at"},
      {"interleavings_max_results", "23"}}));
  assert(parsed.timestamp_key == "started_at" &&
         parsed.max_interleavings == 23);
  for (const auto& item : {
           std::pair{"interleavings_max_results", "0"},
           std::pair{"interleavings_max_results", "nope"}}) {
    bool threw = false;
    try { (void)minera::parse_timestamp_interleavings_params(fields({item})); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void log_conversion_params_contract() {
  const auto parsed = minera::parse_log_conversion_params(fields({
      {"log_conversion_variant", "TO_NX"},
      {"log_conversion_include_case_attributes", "false"},
      {"log_conversion_case_attribute_prefix", "case_"},
      {"log_conversion_case_id_key", "case:id"},
      {"log_conversion_stream_postprocessing", "true"},
      {"log_conversion_max_rows_to_materialize", "78"},
      {"log_conversion_deep_copy", "false"},
      {"log_conversion_compress", "true"},
      {"log_conversion_lazy_output", "true"},
      {"log_conversion_graph_case_id_attribute", "case_id"},
      {"log_conversion_include_df", "0"},
      {"log_conversion_return_variants", "1"},
      {"log_conversion_max_variants_to_return", "56"},
      {"log_conversion_max_graph_nodes", "12"},
      {"log_conversion_max_graph_edges", "34"},
      {"log_conversion_case_attributes_as_nodes", "region,customer\\,id"},
      {"log_conversion_event_attributes_as_nodes", "resource\\\\name"}}));
  assert(parsed.variant == minera::LogConversionVariant::kToAttributedGraph);
  assert(!parsed.include_case_attributes);
  assert(parsed.case_attribute_prefix == "case_");
  assert(parsed.case_id_key == "case:id");
  assert(parsed.remove_null_fields);
  assert(parsed.max_rows_to_materialize &&
         *parsed.max_rows_to_materialize == 78);
  assert(!parsed.deep_copy);
  assert(parsed.compress);
  assert(parsed.lazy_output);
  assert(parsed.graph_case_id_attribute == "case_id");
  assert(!parsed.include_directly_follows);
  assert(parsed.return_variants);
  assert(parsed.max_variants_to_return &&
         *parsed.max_variants_to_return == 56);
  assert(parsed.max_graph_nodes == 12 && parsed.max_graph_edges == 34);
  assert((parsed.case_attributes_as_nodes ==
          std::vector<std::string>{"region", "customer,id"}));
  assert((parsed.event_attributes_as_nodes ==
          std::vector<std::string>{"resource\\name"}));

  // A presence callback preserves the distinction between an omitted field
  // and a meaningful explicitly empty prefix.
  const std::map<std::string, std::string> values = {
      {"log_conversion_case_attribute_prefix", ""}};
  const auto lookup = fields(values);
  const minera::FieldPresence present = [](std::string_view name) {
    return name == "log_conversion_case_attribute_prefix";
  };
  assert(minera::parse_log_conversion_params(lookup, present)
             .case_attribute_prefix.empty());
  const auto unlimited = minera::parse_log_conversion_params(fields({
      {"log_conversion_max_variants_to_return", "0"}}));
  assert(unlimited.max_variants_to_return &&
         *unlimited.max_variants_to_return == 0);

  for (const auto& item : {
           std::pair{"log_conversion_variant", "bad"},
           std::pair{"log_conversion_include_df", "yes"},
           std::pair{"log_conversion_stream_postprocessing", "yes"},
           std::pair{"log_conversion_max_rows_to_materialize", "-1"},
           std::pair{"log_conversion_deep_copy", "maybe"},
           std::pair{"log_conversion_compress", "maybe"},
           std::pair{"log_conversion_lazy_output", "maybe"},
           std::pair{"log_conversion_max_graph_nodes", "0"},
           std::pair{"log_conversion_max_graph_edges", "NaN"},
           std::pair{"log_conversion_max_variants_to_return", "-1"},
           std::pair{"log_conversion_case_id_key", "   "},
           std::pair{"log_conversion_case_attributes_as_nodes", "a,,b"},
           std::pair{"log_conversion_event_attributes_as_nodes", "a\\q"}}) {
    bool threw = false;
    try {
      (void)minera::parse_log_conversion_params(fields({item}));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

}  // namespace

void knowledge_injection_params_contract() {
  const auto options = minera::parse_knowledge_injection_params(fields({
      {"knowledge_variant", "database"},
      {"knowledge_description", "description"},
      {"knowledge_query", "query"},
      {"knowledge_database_schema", "events(id)"},
      {"knowledge_max_input_bytes", "100"},
      {"knowledge_max_prompt_bytes", "200"}}),
      [](std::string_view) { return true; });
  assert(options.variant == minera::KnowledgeInjectionVariant::kDatabase);
  assert(options.description == "description" && options.query == "query");
  assert(options.database_schema == "events(id)");
  assert(options.max_input_bytes == 100 && options.max_prompt_bytes == 200);
  bool threw = false;
  try {
    (void)minera::parse_knowledge_injection_params(
        fields({{"knowledge_max_input_bytes", "0"}}),
        [](std::string_view name) { return name == "knowledge_max_input_bytes"; });
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

int main() {
  ends_with_ci_is_case_insensitive();
  looks_like_xes_uses_extension_then_content_sniff();
  split_csv_trims_and_drops_empty_parts();
  error_json_escapes_quotes_and_backslashes();
  parse_mine_params_defaults_when_empty();
  segment_filter_params_contract();
  adjacent_path_filter_params_contract();
  ltl_filter_params_contract();
  parse_mine_params_maps_correlation_options();
  parse_mine_params_preserves_explicit_empty_correlation_timestamp();
  parse_mine_params_rejects_invalid_correlation_options();
  parse_mine_params_maps_csv_and_inductive_options();
  parse_mine_params_rejects_invalid_xes_options();
  parse_mine_params_maps_filters();
  parse_mine_params_rejects_variant_and_endpoint_filter_errors();
  parse_mine_params_rejects_invalid_attribute_occurrence_thresholds();
  parse_mine_params_rejects_invalid_case_duration_bounds();
  parse_mine_params_rejects_invalid_case_selection_bounds();
  parse_mine_params_rejects_invalid_frequency_attribute_options();
  parse_mine_params_rejects_invalid_numeric_attribute_ranges();
  parse_mine_params_rejects_invalid_prefix_suffix_options();
  parse_mine_params_rejects_invalid_trace_patterns();
  parse_mine_params_rejects_invalid_admitted_variants();
  parse_mine_params_rejects_invalid_activity_split_fields();
  parse_mine_params_rejects_invalid_repetition_fields();
  parse_mine_params_rejects_invalid_case_grouping_fields();
  parse_mine_params_rejects_an_unknown_time_mode();
  parse_mine_params_rejects_an_unknown_attribute_mode();
  parse_mine_params_rejects_a_malformed_succession();
  parse_mine_params_rejects_an_unknown_alpha_variant();
  parse_mine_params_maps_alpha_options_and_presence();
  parse_mine_params_rejects_an_unknown_inductive_variant();
  parse_mine_params_rejects_invalid_inductive_options();
  parse_mine_params_accepts_infrequent_inductive_variant();
  parse_mine_params_rejects_unknown_heuristic_settings();
  parse_mine_params_maps_spectrum_and_batch_options();
  parse_mine_params_preserves_empty_temporal_schedule();
  parse_mine_params_accepts_unbounded_temporal_zeta();
  parse_mine_params_maps_overlap_options();
  parse_mine_params_rejects_invalid_overlap_epsilon();
  parse_mine_params_rejects_invalid_spectrum_and_batch_options();
  parse_mine_params_rejects_dependent_options_without_opt_in();
  parse_petri_analysis_params_maps_all_families_and_settings();
  parse_petri_analysis_params_accepts_woflan_alias();
  parse_petri_analysis_params_rejects_bad_values();
  parse_process_tree_playout_params_maps_variants_and_limits();
  parse_declare_playout_params_is_strict_and_bounded();
  parse_process_tree_playout_params_rejects_bad_values();
  parse_extensive_playout_params_maps_controls_and_rejects_zero();
  parse_ocpn_playout_params_maps_return_traces();
  parse_stochastic_petri_params_maps_and_rejects_bad_values();
  parse_stochastic_map_params_maps_and_rejects_bad_values();
  parse_dfg_playout_params_maps_controls_and_aliases();
  parse_dfg_playout_params_rejects_invalid_bounds();
  parse_stochastic_playout_params_maps_controls();
  parse_stochastic_playout_params_rejects_bad_values();
  parse_monte_carlo_params_maps_controls();
  parse_monte_carlo_params_rejects_bad_values();
  parse_monte_carlo_params_maps_extended_contract();
  parse_monte_carlo_params_rejects_extended_bad_values();
  parse_monte_carlo_params_bounds_supplied_maps_before_accumulation();
  parse_mine_params_ignores_blank_fields();
  parse_mine_params_maps_transition_system_options();
  parse_mine_params_rejects_invalid_transition_fields();
  parse_mine_params_maps_powl_options();
  parse_mine_params_rejects_invalid_powl_fields();
  parse_mine_params_rejects_explicit_blank_powl_scalars();
  parse_mine_params_maps_token_replay_options();
  parse_mine_params_rejects_invalid_token_replay_fields();
  parse_mine_params_maps_log_skeleton_options();
  parse_mine_params_rejects_invalid_log_skeleton_fields();
  parse_mine_params_maps_declare_options();
  parse_mine_params_preserves_explicit_empty_declare_filters();
  parse_mine_params_preserves_explicit_empty_log_skeleton_constraints();
  parse_mine_params_rejects_blank_declarative_selectors();
  parse_mine_params_rejects_invalid_declare_fields();
  parse_mine_params_maps_footprints_options();
  parse_mine_params_rejects_invalid_footprints_options();
  footprints_projection_preserves_case_and_source_order();
  footprints_projection_supports_default_csv_and_xes_schema();
  footprints_projection_rejects_missing_or_null_case_selector();
  footprints_json_uses_selected_case_ids_and_provenance();
  parse_mine_params_maps_dfg_variant_options();
  parse_mine_params_preserves_explicit_empty_dfg_attributes();
  parse_mine_params_rejects_invalid_dfg_variant_options();
  parse_mine_params_maps_dataframe_dfg_options();
  parse_mine_params_preserves_dataframe_slot_presence();
  parse_mine_params_rejects_invalid_dataframe_dfg_options();
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
  browser_history_params_contract();
  repository_history_params_contract();
  workflow_history_params_contract();
  enterprise_history_params_contract();
  communication_history_params_contract();
  knowledge_injection_params_contract();
  windows_event_log_params_contract();
  log_to_target_params_contract();
  trie_params_contract();
  interval_tree_params_contract();
  case_relations_params_contract();
  random_tree_params_contract();
  log_to_features_params_contract();
  embedding_params_contract();
  label_splitting_params_contract();
  ilp_params_contract();
  genetic_params_contract();
  organizational_sna_params_contract();
  role_discovery_params_contract();
  trace_variant_query_params_contract();
  pripel_params_contract();
  link_analysis_params_contract();
  timestamp_interleavings_params_contract();
  resource_profiles_params_contract();
  clustering_params_contract();
  profile_clustering_params_contract();
  chaotic_activities_params_contract();
  passed_time_params_contract();
  service_time_params_contract();
  rework_params_contract();
  cycle_time_params_contract();
  case_arrival_params_contract();
  case_statistics_params_contract();
  attribute_statistics_params_contract();
  attribute_distribution_params_contract();
  attribute_kde_params_contract();
  boundary_activities_params_contract();
  variant_statistics_params_contract();
  log_conversion_params_contract();
  return 0;
}
