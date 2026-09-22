#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <locale>
#include <stdexcept>
#include <vector>

#include "minera/concept_drift.hpp"
#include "minera/json_export.hpp"
#include "minera/timestamp.hpp"

namespace {

minera::Trace trace(std::initializer_list<const char*> activities,
                    const char* id) {
  minera::Trace result;
  result.case_id = id;
  std::size_t order = 0;
  for (const char* activity : activities) {
    result.events.push_back(
        {id, activity, "2024-01-01T00:00:0" + std::to_string(order) + "Z",
         "", order++});
  }
  return result;
}

bool throws_invalid(const auto& fn) {
  try {
    fn();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace

int main() {
  // Epoch seconds and p-values must survive the JSON boundary exactly, even
  // when the caller's global locale uses a comma as the decimal separator.
  struct CommaDecimal : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
  };
  minera::ConceptDriftResult precise_result;
  const double precise_time = 1577836820.123456;
  const double precise_probability = 1.0 / 3.0;
  precise_result.sublog_start_timestamps = {precise_time};
  precise_result.change_timestamps = {precise_time};
  precise_result.change_points = {{0, precise_probability}};
  precise_result.feature_vectors = {{precise_probability}};
  const auto previous_locale = std::locale();
  std::locale::global(std::locale(previous_locale, new CommaDecimal));
  const auto precise_json = minera::concept_drift_json(precise_result);
  std::locale::global(previous_locale);
  const auto roundtrip_number = [&](const std::string& prefix) {
    const auto position = precise_json.find(prefix);
    assert(position != std::string::npos);
    return std::stod(precise_json.substr(position + prefix.size()));
  };
  assert(roundtrip_number("\"sublog_start_timestamps\":[") == precise_time);
  assert(roundtrip_number("\"change_timestamps\":[") == precise_time);
  assert(roundtrip_number("\"p_value\":") == precise_probability);
  assert(roundtrip_number("\"feature_vectors\":[[") == precise_probability);

  minera::ConceptDriftOptions time_options;
  time_options.include_timestamps = true;
  for (const auto& [text, expected] :
       std::vector<std::pair<std::string, double>>{
           {"1970-01-01T00:00:00.000001Z", 0.000001},
           {"1969-01-01T00:00:00.999999Z", -31535999.000001},
           {"2000-01-01T00:00:00.123456Z", 946684800.123456},
           {"1970-01-01T00:00:00.123456789Z", 0.123456789},
           {"1969-12-31T23:59:59.999999999Z", -0.000000001}}) {
    minera::EventLog timestamp_log;
    timestamp_log.traces = {trace({"a"}, "time")};
    timestamp_log.traces.front().events.front().timestamp = text;
    assert(minera::detect_concept_drift(timestamp_log, time_options)
               .sublog_start_timestamps.front() == expected);
  }
  minera::EventLog long_fraction_log;
  long_fraction_log.traces = {trace({"a"}, "long-fraction")};
  const std::string long_fraction = "1970-01-01T00:00:00.123456789123Z";
  long_fraction_log.traces.front().events.front().timestamp = long_fraction;
  assert(minera::detect_concept_drift(long_fraction_log, time_options)
             .sublog_start_timestamps.front() ==
         *minera::parse_timestamp(long_fraction));

  // BOSE checks only the first event of every trace, even when timestamps are
  // omitted from the result. Later-event timestamp gaps do not affect it.
  minera::EventLog missing_first_timestamp;
  missing_first_timestamp.traces = {trace({"a"}, "first-time"),
                                    trace({"b"}, "second-time")};
  missing_first_timestamp.traces[1].events.front().timestamp.clear();
  minera::ConceptDriftOptions timestamp_contract;
  timestamp_contract.sub_log_size = 2;
  timestamp_contract.include_timestamps = false;
  assert(throws_invalid([&] {
    (void)minera::detect_concept_drift(missing_first_timestamp,
                                       timestamp_contract);
  }));
  timestamp_contract.include_timestamps = true;
  assert(throws_invalid([&] {
    (void)minera::detect_concept_drift(missing_first_timestamp,
                                       timestamp_contract);
  }));

  minera::EventLog missing_later_timestamp;
  missing_later_timestamp.traces = {trace({"a", "b"}, "later-time")};
  missing_later_timestamp.traces.front().events.back().timestamp.clear();
  timestamp_contract.include_timestamps = false;
  assert(!throws_invalid([&] {
    (void)minera::detect_concept_drift(missing_later_timestamp,
                                       timestamp_contract);
  }));
  timestamp_contract.include_timestamps = true;
  const auto missing_later_result =
      minera::detect_concept_drift(missing_later_timestamp, timestamp_contract);
  assert(missing_later_result.sublog_start_timestamps.size() == 1);

  minera::EventLog custom_timestamp_log;
  custom_timestamp_log.traces = {trace({"a", "b"}, "custom-time")};
  custom_timestamp_log.traces.front().events[0].attributes.push_back(
      {"started_at", "2024-01-01T00:00:00Z"});
  custom_timestamp_log.traces.front().events[1].attributes.push_back(
      {"started_at", "1970-01-01T00:00:00Z"});
  timestamp_contract.timestamp_attribute = "started_at";
  const auto custom_timestamp_result =
      minera::detect_concept_drift(custom_timestamp_log, timestamp_contract);
  assert(custom_timestamp_result.sublog_start_timestamps.size() == 1);
  assert(custom_timestamp_result.sublog_start_timestamps.front() ==
         *minera::parse_timestamp("2024-01-01T00:00:00Z"));

  // Empty strings are labels, not absent activity attributes. Preserve them
  // in the alphabet used by both standalone extraction and drift detection.
  minera::EventLog empty_label_log;
  empty_label_log.traces = {trace({"", "a"}, "empty-label")};
  empty_label_log.traces.front().events.front().activity_state =
      minera::FirstClassAttributeState::kPresent;
  assert((minera::extract_concept_drift_activities(empty_label_log) ==
          std::vector<std::string>{"", "a"}));
  const std::vector<double> empty_label_features{0, 0, 2, 1, 0, 1};
  assert(minera::extract_concept_drift_features(empty_label_log) ==
         empty_label_features);
  const auto empty_label_result = minera::detect_concept_drift(empty_label_log);
  assert(empty_label_result.feature_vectors.front() == empty_label_features);
  for (auto& event : empty_label_log.traces.front().events) {
    event.attributes.push_back({"selected", event.activity});
    event.activity = "unused";
  }
  minera::ConceptDriftOptions empty_label_options;
  empty_label_options.activity_attribute = "selected";
  const auto selected_empty_result =
      minera::detect_concept_drift(empty_label_log, empty_label_options);
  assert(selected_empty_result.activities == empty_label_result.activities);
  assert(selected_empty_result.feature_vectors == empty_label_result.feature_vectors);

  // A missing default activity and a missing generic selector are invalid for
  // detection, while the standalone feature extractors retain their
  // historical permissive behavior.
  minera::EventLog missing_default_activity;
  missing_default_activity.traces = {trace({"a"}, "missing-default")};
  missing_default_activity.traces.front().events.front().activity.clear();
  assert(throws_invalid([&] {
    (void)minera::detect_concept_drift(missing_default_activity);
  }));

  minera::EventLog empty_trace_log;
  empty_trace_log.traces.emplace_back();
  assert(throws_invalid(
      [&] { (void)minera::detect_concept_drift(empty_trace_log); }));

  minera::EventLog log;
  log.traces = {trace({"a", "b"}, "c1"), trace({"a", "b"}, "c2"),
                trace({"a", "c"}, "c3"), trace({"a", "c"}, "c4")};
  log.attributes.push_back({"fixture", "concept-drift"});
  log.traces[0].attributes.push_back({"case:group", "before"});
  log.traces[0].events[0].attributes.push_back({"event:detail", "kept"});

  minera::ConceptDriftOptions missing_activity_options;
  missing_activity_options.activity_attribute = "missing";
  assert(throws_invalid(
      [&] { (void)minera::detect_concept_drift(log, missing_activity_options); }));

  // Generic activity cells must remain scalar strings.  Their lexical text
  // is not enough to make numeric, null, or container values valid labels.
  const auto rejects_generic_activity = [](minera::EventAttribute cell) {
    minera::EventLog candidate;
    candidate.traces = {trace({"a", "b"}, "generic-validation")};
    for (auto& event : candidate.traces.front().events) {
      cell.key = "selected";
      event.attributes.push_back(cell);
    }
    minera::ConceptDriftOptions candidate_options;
    candidate_options.activity_attribute = "selected";
    return throws_invalid([&] {
      (void)minera::detect_concept_drift(candidate, candidate_options);
    });
  };
  assert(rejects_generic_activity(
      {"selected", "", minera::AttributeKind::kString, true}));
  minera::EventAttribute numeric_activity{"selected", "1",
                                          minera::AttributeKind::kInt};
  numeric_activity.typed_value = std::int64_t{1};
  assert(rejects_generic_activity(numeric_activity));
  minera::EventAttribute container_activity{"selected", "A",
                                            minera::AttributeKind::kString};
  container_activity.container = minera::AttributeContainerKind::kList;
  assert(rejects_generic_activity(container_activity));

  // Empty strings are valid first-class labels when presence is explicit.
  minera::EventLog first_class_metadata_log;
  first_class_metadata_log.traces = {trace({"a"}, "first-class")};
  auto& first_class_event = first_class_metadata_log.traces.front().events.front();
  first_class_event.activity = "";
  first_class_event.activity_state = minera::FirstClassAttributeState::kPresent;
  first_class_event.activity_attribute =
      minera::EventAttribute{"concept:name", "", minera::AttributeKind::kString};
  assert(minera::detect_concept_drift(first_class_metadata_log)
             .activities == std::vector<std::string>{""});
  first_class_event.activity = "visible";
  first_class_event.activity_state = minera::FirstClassAttributeState::kNull;
  assert(throws_invalid([&] {
    (void)minera::detect_concept_drift(first_class_metadata_log);
  }));

  // The resource, lifecycle, and timestamp aliases use their own first-class
  // presence state and selected metadata, just like concept:name.
  const auto first_class_alias_accepts = [](std::string selector,
                                            std::string value,
                                            minera::EventAttribute metadata) {
    minera::EventLog candidate;
    candidate.traces = {trace({"unused"}, "alias")};
    auto& event = candidate.traces.front().events.front();
    if (selector == "org:resource") {
      event.resource = std::move(value);
      event.resource_state = minera::FirstClassAttributeState::kPresent;
      event.resource_attribute = std::move(metadata);
    } else if (selector == "lifecycle:transition") {
      event.lifecycle = std::move(value);
      event.lifecycle_state = minera::FirstClassAttributeState::kPresent;
      event.lifecycle_attribute = std::move(metadata);
    } else {
      event.timestamp = std::move(value);
      event.timestamp_state = minera::FirstClassAttributeState::kPresent;
      event.timestamp_attribute = std::move(metadata);
    }
    minera::ConceptDriftOptions candidate_options;
    candidate_options.activity_attribute = std::move(selector);
    return !throws_invalid([&] {
      (void)minera::detect_concept_drift(candidate, candidate_options);
    });
  };
  assert(first_class_alias_accepts(
      "org:resource", "", {"org:resource", "", minera::AttributeKind::kString}));
  assert(first_class_alias_accepts(
      "lifecycle:transition", "L", {"lifecycle:transition", "L",
                                       minera::AttributeKind::kString}));
  assert(first_class_alias_accepts(
      "time:timestamp", "2024-01-01T00:00:00Z",
      {"time:timestamp", "2024-01-01T00:00:00Z", minera::AttributeKind::kString}));

  const auto activities = minera::extract_concept_drift_activities(log);
  assert((activities == std::vector<std::string>{"a", "b", "c"}));
  // In this log a precedes b in half the cases and c in the other half.
  const auto features = minera::extract_concept_drift_features(log);
  assert(features.size() == 9);
  assert(features[0] == 0.0 && features[1] == 0.0 && features[2] == 3.0);
  assert(features[3] == 0.0 && features[4] == 1.0 && features[5] == 2.0);
  assert(features[6] == 0.0 && features[7] == 1.0 && features[8] == 2.0);

  const std::vector<minera::Trace> alphabet_traces = {
      trace({"a", "b", "c"}, "alphabet-abc"),
      trace({"a", "c"}, "alphabet-ac"),
      trace({"b", "a"}, "alphabet-ba")};
  const auto reversed_alphabet_features = minera::extract_concept_drift_features(
      alphabet_traces, {"c", "b", "a"});
  assert((reversed_alphabet_features ==
          std::vector<double>{0, 2, 1, 0, 1, 2, 0, 1, 2}));
  const auto duplicate_alphabet_features = minera::extract_concept_drift_features(
      {trace({"a", "b"}, "alphabet-duplicate")}, {"a", "b", "a"});
  assert((duplicate_alphabet_features ==
          std::vector<double>{0, 0, 3, 2, 0, 1, 0, 0, 3}));
  const std::vector<std::string> many_duplicate_labels(10'000, "a");
  const auto many_duplicate_features = minera::extract_concept_drift_features(
      {trace({"a", "a"}, "many-duplicates")}, many_duplicate_labels);
  assert(many_duplicate_features.size() == many_duplicate_labels.size() * 3);
  for (std::size_t i = 0; i < many_duplicate_features.size(); i += 3) {
    assert(many_duplicate_features[i] == 10'000.0);
    assert(many_duplicate_features[i + 1] == 0.0);
    assert(many_duplicate_features[i + 2] == 0.0);
  }
  assert(throws_invalid([&] {
    (void)minera::extract_concept_drift_features(
        {trace({"a", "missing"}, "alphabet-missing")}, {"a", "b"});
  }));
  assert(minera::extract_concept_drift_features(
             std::vector<minera::Trace>{}, std::vector<std::string>{})
             .empty());

  minera::EventLog projected = log;
  for (auto& current : projected.traces) {
    current.attributes.push_back({"case:order", "batch-" + current.case_id});
    for (auto& event : current.events) {
      event.attributes.push_back({"event:type",
                                  event.activity == "a" ? "Alpha" : "Beta"});
      event.attributes.push_back({"started_at", event.timestamp});
    }
  }
  minera::ConceptDriftOptions projected_options;
  projected_options.variant = minera::ConceptDriftVariant::BOSE;
  projected_options.sub_log_size = 2;
  projected_options.window_size = 1;
  projected_options.num_permutations = 5;
  projected_options.threshold = 0.0;
  projected_options.include_timestamps = true;
  projected_options.activity_attribute = "event:type";
  projected_options.timestamp_attribute = "started_at";
  projected_options.case_id_attribute = "case:order";
  const auto projected_result =
      minera::detect_concept_drift(projected, projected_options);
  assert((projected_result.activities ==
          std::vector<std::string>{"Alpha", "Beta"}));
  assert(projected_result.variant == minera::ConceptDriftVariant::kBose);
  assert(projected_result.activity_attribute == "event:type");
  assert(projected_result.timestamp_attribute == "started_at");
  assert(projected_result.case_id_attribute == "case:order");
  assert(projected_result.sublog_start_timestamps.size() == 2);
  const auto projected_json = minera::concept_drift_json(projected_result);
  assert(projected_json.find("\"activity_attribute\":\"event:type\"") !=
         std::string::npos);
  assert(projected_json.find("\"timestamp_attribute\":\"started_at\"") !=
         std::string::npos);
  assert(projected_json.find("\"variant\":\"bose\"") !=
         std::string::npos);

  minera::ConceptDriftOptions options;
  options.sub_log_size = 2;
  options.window_size = 1;
  options.num_permutations = 25;
  options.threshold = 1.0;
  options.max_no_change_points = 1;
  options.seed = 42;
  options.include_timestamps = true;
  const auto result = minera::detect_concept_drift(log, options);
  assert(result.variant == minera::ConceptDriftVariant::kBose);
  assert(result.sublogs.size() == 2);
  assert(result.returned_sublogs.size() == 1);
  assert(result.returned_sublogs[0].traces.size() == 4);
  assert(result.cumulative_sublogs.size() == 2);
  assert(result.cumulative_sublogs[0].traces.size() == 2);
  assert(result.cumulative_sublogs[1].traces.size() == 4);
  assert(result.sublog_start_timestamps.size() == 2);
  assert(std::isfinite(result.sublog_start_timestamps[0]));
  assert(result.change_points.empty());  // strict p < threshold; p is 1 here
  const auto result_json = minera::concept_drift_json(result);
  assert(result_json.find("\"returned_sublogs\":[{\"trace_count\":4,\"event_count\":8}]") !=
         std::string::npos);

  minera::ConceptDriftOptions boundary_options = options;
  boundary_options.sub_log_size = 1;
  boundary_options.window_size = 2;
  boundary_options.num_permutations = 100;
  boundary_options.threshold = 1.0;
  boundary_options.seed = 42;
  const auto boundary_result =
      minera::detect_concept_drift(log, boundary_options);
  assert(boundary_result.change_points.size() == 1);
  assert(boundary_result.change_points[0].sublog_index == 2);
  assert(boundary_result.returned_sublogs.size() == 2);
  assert(boundary_result.returned_sublogs[0].traces.size() == 2);
  assert(boundary_result.returned_sublogs[1].traces.size() == 2);
  assert(boundary_result.returned_sublogs[0].traces[0].case_id == "c1");
  assert(boundary_result.returned_sublogs[0].traces[1].case_id == "c2");
  assert(boundary_result.returned_sublogs[1].traces[0].case_id == "c3");
  assert(boundary_result.returned_sublogs[1].traces[1].case_id == "c4");
  assert(*boundary_result.returned_sublogs[0].attribute("fixture") ==
         "concept-drift");
  assert(*boundary_result.returned_sublogs[0].traces[0].attribute("case:group") ==
         "before");
  assert(*boundary_result.returned_sublogs[0].traces[0].events[0].attribute(
             "event:detail") == "kept");

  // The low 32-bit seed follows NumPy's legacy MT19937 permutation stream.
  // This four-trace window has a stable public p-value at 256 permutations.
  minera::ConceptDriftOptions numpy_stream_options;
  numpy_stream_options.sub_log_size = 1;
  numpy_stream_options.window_size = 2;
  numpy_stream_options.num_permutations = 256;
  numpy_stream_options.threshold = 1.0;
  numpy_stream_options.seed = 0;
  const auto numpy_stream_result =
      minera::detect_concept_drift(log, numpy_stream_options);
  assert(numpy_stream_result.change_points.size() == 1);
  assert(numpy_stream_result.change_points[0].sublog_index == 2);
  assert(numpy_stream_result.change_points[0].p_value == 0.30078125);
  const auto numpy_stream_repeat =
      minera::detect_concept_drift(log, numpy_stream_options);
  assert(numpy_stream_repeat.change_points.size() ==
         numpy_stream_result.change_points.size());
  assert(numpy_stream_repeat.change_points[0].sublog_index ==
         numpy_stream_result.change_points[0].sublog_index);
  assert(numpy_stream_repeat.change_points[0].p_value ==
         numpy_stream_result.change_points[0].p_value);

  // Values outside NumPy's scalar seed range remain deterministic through the
  // documented native 64-bit extension.
  numpy_stream_options.seed = std::numeric_limits<std::uint64_t>::max();
  const auto wide_seed_result =
      minera::detect_concept_drift(log, numpy_stream_options);
  const auto wide_seed_repeat =
      minera::detect_concept_drift(log, numpy_stream_options);
  assert(wide_seed_result.change_points.size() == 1);
  assert(wide_seed_repeat.change_points.size() ==
         wide_seed_result.change_points.size());
  assert(wide_seed_repeat.change_points[0].sublog_index ==
         wide_seed_result.change_points[0].sublog_index);
  assert(wide_seed_repeat.change_points[0].p_value ==
         wide_seed_result.change_points[0].p_value);

  options.threshold = 0.0;
  assert(minera::detect_concept_drift(log, options).change_points.empty());
  options.sub_log_size = 0;
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.sub_log_size = 2;
  options.threshold = 0.5;
  options.num_permutations = 0;
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.num_permutations = 10;
  options.threshold = std::nan("");
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));

  options.threshold = 0.5;
  options.variant = static_cast<minera::ConceptDriftVariant>(99);
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.variant = minera::ConceptDriftVariant::kBose;

  options.threshold = 0.5;
  options.activity_attribute.clear();
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.activity_attribute = "concept:name";
  options.case_id_attribute = "missing_case_key";
  const auto grouped_case_result = minera::detect_concept_drift(log, options);
  assert(grouped_case_result.case_id_attribute == "missing_case_key");

  minera::EventLog bad_timestamp;
  bad_timestamp.traces.push_back(trace({"a"}, "bad"));
  bad_timestamp.traces[0].events[0].timestamp = "invalid";
  options.threshold = 0.5;
  options.include_timestamps = true;
  assert(throws_invalid(
      [&] { (void)minera::detect_concept_drift(bad_timestamp, options); }));

  minera::EventLog oversized;
  const auto throws_materialization_limit = [](const auto& fn) {
    try {
      fn();
    } catch (const std::invalid_argument& error) {
      return std::string(error.what()).find(
                 "cumulative sub-log materialization exceeds safe bound") !=
             std::string::npos;
    }
    return false;
  };
  // The cheap trace-count guard runs before per-event validation, avoiding
  // materializing millions of events merely to exercise this resource limit.
  oversized.traces.resize(2500);
  options.include_timestamps = false;
  options.sub_log_size = 1;
  assert(throws_materialization_limit(
      [&] { (void)minera::detect_concept_drift(oversized, options); }));

  minera::EventLog oversized_sublog;
  oversized_sublog.traces.resize(1'000'001);
  options.sub_log_size = oversized_sublog.traces.size();
  assert(throws_materialization_limit(
      [&] { (void)minera::detect_concept_drift(oversized_sublog, options); }));
}
