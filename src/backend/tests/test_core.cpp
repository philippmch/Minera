// Keep assertions live even in NDEBUG (Release) builds: this file *is* the
// test, so its asserts must never compile away. Must precede <cassert>.
#undef NDEBUG

#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/declare.hpp"
#include "minera/decision_mining.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/trace_variant_query.hpp"
#include "minera/pripel.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/log_conversion.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/model_decoration.hpp"
#include "minera/petri_net.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/powl.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/ptml.hpp"
#include "minera/process_tree.hpp"
#include "minera/process_tree_alignment.hpp"
#include "minera/timestamp.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/transition_system.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

void process_tree_alignment_json_carries_failure_and_moves() {
  minera::ProcessTreeAlignmentResult result;
  result.variant = minera::ProcessTreeAlignmentVariant::kMilp;
  result.computed = false;
  result.completed = false;
  result.exhausted = true;
  result.diagnostics.message = std::string("quote \"") + " and slash " +
                              std::string(1, '\\');
  minera::ProcessTreeTraceAlignment trace;
  trace.trace = {"a"};
  trace.completed = true;
  trace.timed_out = false;
  trace.moves.push_back({minera::ProcessTreeAlignmentMoveType::kSynchronous,
                         "a", "a", "a"});
  trace.moves.push_back({minera::ProcessTreeAlignmentMoveType::kLog,
                         "x", "", "x"});
  trace.moves.push_back({minera::ProcessTreeAlignmentMoveType::kModel,
                         "", "b", "b"});
  result.traces.push_back(std::move(trace));
  const auto json = minera::process_tree_alignment_json(result);
  assert(json.find("\"family\": \"PROCESS_TREE\"") != std::string::npos);
  assert(json.find("\"variant\": \"MILP\"") != std::string::npos);
  assert(json.find("\"trace\": [\"a\"]") != std::string::npos);
  assert(json.find("\"timed_out\": false") != std::string::npos);
  assert(json.find("\"type\": \"sync\"") != std::string::npos);
  assert(json.find("\"type\": \"log\"") != std::string::npos);
  assert(json.find("\"type\": \"model\"") != std::string::npos);
  assert(json.find("quote \\\" and slash") != std::string::npos);
}

minera::EventLog temporal_log(std::initializer_list<std::vector<std::string>> rows) {
  minera::EventLog log;
  std::size_t case_no = 0;
  for (const auto& row : rows) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_no++);
    for (std::size_t i = 0; i < row.size(); ++i) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = i == 0 ? "A" : (i == 1 ? "B" : "C");
      event.timestamp = row[i];
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void temporal_profile_discovers_all_pairs_and_stats() {
  const auto log = temporal_log({{"2026-01-01T00:00:00", "2026-01-01T00:00:10",
                                  "2026-01-01T00:00:30"},
                                 {"2026-01-01T00:00:00", "2026-01-01T00:00:20",
                                  "2026-01-01T00:00:40"}});
  const auto profile = minera::discover_temporal_profile(log);
  const auto* ac = profile.find("A", "C");
  assert(ac != nullptr && ac->count == 2 && ac->mean_seconds == 35.0);
  assert(std::abs(ac->standard_deviation_seconds -
                  std::sqrt(50.0)) < 1e-9);
  assert(profile.find("A", "B") != nullptr);
  assert(profile.find("B", "C") != nullptr);

  const auto graph = minera::discover_directly_follows_graph(log);
  const auto without_profile = minera::analysis_json("temporal", log, graph);
  assert(without_profile.find("\"temporal_profile\"") == std::string::npos);
  const auto json = minera::analysis_json("temporal", log, graph, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, &profile);
  assert(json.find("\"temporal_profile\": [") != std::string::npos);
  assert(json.find(
             "{\"source\": 0, \"target\": 2, \"count\": 2, \"mean_seconds\": 35") !=
         std::string::npos);
  assert(json.find("\"standard_deviation_seconds\": 7.07107") !=
         std::string::npos);
  const auto ab_position = json.find("{\"source\": 0, \"target\": 1,");
  const auto ac_position = json.find("{\"source\": 0, \"target\": 2,");
  assert(ab_position < ac_position);

  const auto expect_profile_rejected =
      [&](const minera::TemporalProfile& invalid) {
        std::ostringstream partial;
        bool threw = false;
        try {
          minera::write_analysis_json(
              partial, "temporal", log, graph,
              /*tree=*/nullptr, /*net=*/nullptr, /*heuristics=*/nullptr,
              /*conformance=*/nullptr, /*performance=*/nullptr,
              /*resources=*/nullptr, /*variant_durations=*/nullptr,
              /*eventually_follows=*/nullptr, /*decisions=*/nullptr,
              /*petri_decoration=*/nullptr, /*spectrum=*/nullptr,
              /*inductive_net=*/nullptr, /*inductive_decoration=*/nullptr,
              /*heuristic_petri=*/nullptr, /*heuristic_decoration=*/nullptr,
              &invalid);
        } catch (const std::invalid_argument&) {
          threw = true;
        }
        assert(threw);
        assert(partial.str().empty());
      };

  minera::TemporalProfile unresolved;
  unresolved.entries = {{"A", "B", 1, 1.0, 0.0},
                        {"missing", "A", 1, 1.0, 0.0}};
  expect_profile_rejected(unresolved);

  const std::vector<minera::TemporalProfileEntry> invalid_entries = {
      {"A", "B", 0, 1.0, 0.0},
      {"A", "B", 1, -1.0, 0.0},
      {"A", "B", 1, std::numeric_limits<double>::quiet_NaN(), 0.0},
      {"A", "B", 1, std::numeric_limits<double>::infinity(), 0.0},
      {"A", "B", 1, 1.0, -1.0},
      {"A", "B", 1, 1.0, std::numeric_limits<double>::infinity()}};
  for (const auto& invalid_entry : invalid_entries) {
    minera::TemporalProfile invalid;
    invalid.entries.push_back(invalid_entry);
    expect_profile_rejected(invalid);
  }
  minera::TemporalProfile duplicate;
  duplicate.entries = {{"A", "B", 1, 1.0, 0.0},
                       {"A", "B", 1, 2.0, 0.0}};
  expect_profile_rejected(duplicate);
  minera::TemporalProfile reversed;
  reversed.entries = {{"B", "C", 1, 1.0, 0.0},
                      {"A", "B", 1, 1.0, 0.0}};
  expect_profile_rejected(reversed);
}

void minimum_self_distance_discovers_distances_and_witnesses() {
  minera::EventLog log;
  log.traces = {
      {"t1", {{"", "A", "", "", 0}, {"", "B", "", "", 1},
               {"", "A", "", "", 2}, {"", "C", "", "", 3},
               {"", "A", "", "", 4}}},
      {"t2", {{"", "A", "", "", 0}, {"", "A", "", "", 1}}},
      {"t3", {{"", "X", "", "", 0}, {"", "Y", "", "", 1},
               {"", "X", "", "", 2}, {"", "Z", "", "", 3},
               {"", "X", "", "", 4}}},
      {"t4", {{"", "", "", "", 0}, {"", "ü", "", "", 1},
               {"", "", "", "", 2}, {"", "ü", "", "", 3}}},
      {"t5", {{"", "Q", "", "", 0}, {"", "R", "", "", 1},
               {"", "R", "", "", 2}, {"", "Q", "", "", 3}}},
  };
  // Explicitly distinguish a present empty activity value from a missing
  // first-class activity field.
  log.traces[3].events[0].activity_state =
      minera::FirstClassAttributeState::kPresent;
  log.traces[3].events[2].activity_state =
      minera::FirstClassAttributeState::kPresent;
  const auto result = minera::discover_minimum_self_distances(log);
  minera::MinimumSelfDistanceOptions pandas_options;
  pandas_options.variant = minera::MinimumSelfDistanceVariant::kPandas;
  const auto pandas_result =
      minera::discover_minimum_self_distances(log, pandas_options);
  minera::MinimumSelfDistanceOptions polars_options;
  polars_options.variant = minera::MinimumSelfDistanceVariant::kPolars;
  const auto polars_result =
      minera::discover_minimum_self_distances(log, polars_options);
  assert(pandas_result.size() == result.size() &&
         polars_result.size() == result.size());
  assert(result.size() == 6);
  assert(result[0].activity.empty() && result[0].distance == 1);
  assert((result[0].witnesses == std::vector<std::string>{"ü"}));
  assert(result[1].activity == "A" && result[1].distance == 0);
  assert(result[1].witnesses.empty());
  assert(result[2].activity == "Q" && result[2].distance == 2);
  assert((result[2].witnesses == std::vector<std::string>{"R"}));
  assert(result[3].activity == "R" && result[3].distance == 0);
  assert(result[3].witnesses.empty());
  assert(result[4].activity == "X" && result[4].distance == 1);
  assert((result[4].witnesses == std::vector<std::string>{"Y", "Z"}));
  assert(result[5].activity == "ü" && result[5].distance == 1);
  assert(result[5].witnesses.size() == 1 && result[5].witnesses[0].empty());
  assert(minera::discover_minimum_self_distances({}).empty());
}

void minimum_self_distance_supports_custom_activity_and_case_columns() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "native-trace";
  trace.events = {
      {"c1", "native-a", "", "", 0, {{"phase", "A"}, {"case_ref", "c1"}}},
      {"c2", "native-x", "", "", 1, {{"phase", "X"}, {"case_ref", "c2"}}},
      {"c1", "native-y", "", "", 2, {{"phase", "Y"}, {"case_ref", "c1"}}},
      {"c1", "native-a", "", "", 3, {{"phase", "A"}, {"case_ref", "c1"}}},
      {"c2", "native-q", "", "", 4, {{"phase", "Q"}, {"case_ref", "c2"}}},
      {"c1", "native-z", "", "", 5, {{"phase", "Z"}, {"case_ref", "c1"}}},
      {"c1", "native-a", "", "", 6, {{"phase", "A"}, {"case_ref", "c1"}}},
  };
  log.traces.push_back(trace);
  minera::MinimumSelfDistanceOptions custom;
  custom.activity_key = "phase";
  custom.case_id_key = "case_ref";
  const auto expected =
      minera::discover_minimum_self_distances(log, custom);
  assert(expected.size() == 1);
  assert(expected.front().activity == "A");
  assert(expected.front().distance == 1);
  assert((expected.front().witnesses ==
          std::vector<std::string>{"Y", "Z"}));
  for (const auto variant : {minera::MinimumSelfDistanceVariant::kPandas,
                             minera::MinimumSelfDistanceVariant::kPolars}) {
    custom.variant = variant;
    const auto actual = minera::discover_minimum_self_distances(log, custom);
    assert(actual.size() == expected.size());
    assert(actual.front().activity == expected.front().activity &&
           actual.front().distance == expected.front().distance &&
           actual.front().witnesses == expected.front().witnesses);
  }

  trace.events[2].attributes = {{"phase", "A"}, {"case_ref", "c1"}};
  trace.events[3].attributes = {{"phase", "A"}, {"case_ref", "c1"}};
  log.traces.front() = trace;
  custom.variant = minera::MinimumSelfDistanceVariant::kLog;
  const auto zero = minera::discover_minimum_self_distances(log, custom);
  assert(zero.size() == 1 && zero.front().distance == 0 &&
         zero.front().witnesses.empty());

  custom.activity_key = "missing";
  bool threw = false;
  try {
    (void)minera::discover_minimum_self_distances(log, custom);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void minimum_self_distance_json_resolves_ids_and_validates_before_writing() {
  minera::EventLog log;
  log.traces = {{"t", {{"", "A", "", "", 0}, {"", "B", "", "", 1},
                         {"", "A", "", "", 2}}}};
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto distances = minera::discover_minimum_self_distances(log);
  const auto json = minera::analysis_json("test", log, graph, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, &distances);
  assert(json.find("\"minimum_self_distances\": [{\"activity\": 0, \"distance\": 1, \"witnesses\": [1]}]") !=
         std::string::npos);
  assert(json.find("\"minimum_self_distance_variant\": \"log\"") !=
         std::string::npos);
  assert(minera::analysis_json("test", log, graph).find(
             "minimum_self_distances") == std::string::npos);
  const auto expect_rejected =
      [&](const minera::MinimumSelfDistances& invalid) {
        std::ostringstream stream;
        bool threw = false;
        try {
          minera::write_analysis_json(
              stream, "test", log, graph,
              /*tree=*/nullptr, /*net=*/nullptr, /*heuristics=*/nullptr,
              /*conformance=*/nullptr, /*performance=*/nullptr,
              /*resources=*/nullptr, /*variant_durations=*/nullptr,
              /*eventually_follows=*/nullptr, /*decisions=*/nullptr,
              /*petri_decoration=*/nullptr, /*spectrum=*/nullptr,
              /*inductive_net=*/nullptr, /*inductive_decoration=*/nullptr,
              /*heuristic_petri=*/nullptr, /*heuristic_decoration=*/nullptr,
              /*temporal_profile=*/nullptr, &invalid);
        } catch (const std::invalid_argument&) {
          threw = true;
        }
        assert(threw);
        assert(stream.str().empty());
      };
  expect_rejected({{"missing", 1, {"B"}}});
  expect_rejected({{"B", 1, {"A"}}, {"A", 1, {"B"}}});
  expect_rejected({{"A", 1, {"B"}}, {"A", 1, {"B"}}});
  expect_rejected({{"A", 0, {"B"}}});
  expect_rejected({{"A", 1, {}}});
  expect_rejected({{"A", 1, {"A"}}});
  expect_rejected({{"A", 1, {"missing"}}});
}

void temporal_profile_scales_to_many_activities() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "many";
  constexpr std::size_t activity_count = 130;
  trace.events.reserve(activity_count);
  for (std::size_t i = 0; i < activity_count; ++i) {
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = "activity_" + std::to_string(i);
    event.timestamp = minera::format_timestamp(static_cast<double>(i) * 60.0);
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));

  const auto profile = minera::discover_temporal_profile(log);
  assert(profile.entries.size() == activity_count * (activity_count - 1) / 2);
  const auto deviations =
      minera::check_temporal_profile_conformance(log, profile);
  assert(deviations.empty());
}

void temporal_profile_rejects_or_skips_bad_timestamps_and_checks_deviations() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case";
  trace.events = {{"case", "A", "2026-01-01T00:00:00"},
                  {"case", "B", "2026-01-01T00:00:10"},
                  {"case", "C", "not-a-time"},
                  {"case", "D", "2025-12-31T23:00:00"}};
  log.traces.push_back(trace);
  bool threw = false;
  try {
    (void)minera::discover_temporal_profile(log);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  minera::TemporalProfileOptions permissive;
  permissive.strict_attributes = false;
  const auto profile = minera::discover_temporal_profile(log, permissive);
  assert(profile.find("A", "B") != nullptr);
  assert(profile.find("A", "D") == nullptr);
  minera::TemporalProfile fixed;
  fixed.entries.push_back({"A", "B", 1, 10.0, 0.0});
  auto deviations =
      minera::check_temporal_profile_conformance(log, fixed, permissive);
  assert(deviations.empty());
  trace.events[1].timestamp = "2026-01-01T00:00:11";
  log.traces[0] = trace;
  deviations =
      minera::check_temporal_profile_conformance(log, fixed, permissive);
  assert(deviations.size() == 1 &&
         deviations[0].distance() == static_cast<double>(
             std::numeric_limits<std::int64_t>::max()));
}

void temporal_profile_supports_attributes_business_hours_and_validation() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "attrs";
  minera::Event a{"attrs", "A", "bad"};
  a.attributes.push_back({"done", "2026-01-05T07:00:00+02:00", minera::AttributeKind::kDate});
  a.attributes.push_back({"canonical", "2026-01-05T09:00:00+02:00", minera::AttributeKind::kDate});
  minera::Event b{"attrs", "B", "bad"};
  b.attributes.push_back({"started", "2026-01-05T08:00:00+02:00", minera::AttributeKind::kDate});
  b.attributes.push_back({"canonical_target", "2026-01-05T17:00:00+02:00", minera::AttributeKind::kDate});
  trace.events = {a, b};
  log.traces.push_back(trace);
  minera::TemporalProfileOptions options;
  options.source_completion_timestamp_attribute = "done";
  options.target_start_timestamp_attribute = "started";
  options.use_business_hours = true;
  const auto profile = minera::discover_temporal_profile(log, options);
  assert(profile.find("A", "B")->mean_seconds == 3600.0);
  options.source_timestamp_attribute = "canonical";
  options.target_timestamp_attribute = "canonical_target";
  const auto canonical = minera::discover_temporal_profile(log, options);
  assert(canonical.find("A", "B")->mean_seconds == 28800.0);
  options.business_hour_slots = {{0.0, 1800.0}, {1800.0, 3600.0}};
  const auto custom = minera::discover_temporal_profile(log, options);
  assert(custom.find("A", "B")->mean_seconds == 0.0);
  options.zeta = 0.0;
  const auto zero_zeta =
      minera::check_temporal_profile_conformance(log, profile, options);
  assert(zero_zeta.size() == 1);
  options.zeta = 6.0;
  options.business_hour_slots = {{-1.0, 1.0}};
  bool threw = false;
  try { (void)minera::discover_temporal_profile(log, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void temporal_profile_business_hours_handle_multiweek_and_before_epoch() {
  minera::TemporalProfileOptions options;
  options.use_business_hours = true;
  const auto monday_span = temporal_log(
      {{"2026-01-05T07:00:00", "2026-01-19T08:00:00"}});
  const auto before_epoch = temporal_log(
      {{"1969-12-22T07:00:00", "1970-01-05T08:00:00"}});
  const auto first = minera::discover_temporal_profile(monday_span, options);
  const auto second = minera::discover_temporal_profile(before_epoch, options);
  assert(first.find("A", "B")->mean_seconds == 363600.0);
  assert(second.find("A", "B")->mean_seconds == 363600.0);

  const auto minute_only =
      temporal_log({{"2026-01-05T16:00", "2026-01-05T17:00"}});
  const auto at_close =
      temporal_log({{"2026-01-05T17:00:00", "2026-01-05T18:00:00"}});
  const auto weekend =
      temporal_log({{"2026-01-10T08:00:00", "2026-01-10T09:00:00"}});
  assert(minera::discover_temporal_profile(minute_only, options)
             .find("A", "B")
             ->mean_seconds == 3600.0);
  assert(minera::discover_temporal_profile(at_close, options)
             .find("A", "B")
             ->mean_seconds == 0.0);
  assert(minera::discover_temporal_profile(weekend, options)
             .find("A", "B")
             ->mean_seconds == 0.0);
}

void temporal_profile_validates_profiles_and_zeta_boundary() {
  const auto log = temporal_log({{"2026-01-01T00:00:00", "2026-01-01T00:00:16"},
                                 {"2026-01-01T00:00:00", "2026-01-01T00:00:16.1"}});
  minera::TemporalProfile profile;
  profile.entries.push_back({"A", "B", 2, 10.0, 1.0});
  minera::TemporalProfileOptions options;
  options.zeta = 6.0;
  auto deviations = minera::check_temporal_profile_conformance(log, profile, options);
  assert(deviations.size() == 1);
  profile.entries.push_back({"A", "B", 1, 10.0, 1.0});
  bool threw = false;
  try { (void)minera::check_temporal_profile_conformance(log, profile); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  profile.entries.pop_back();
  profile.entries[0].standard_deviation_seconds = -1.0;
  threw = false;
  try { (void)minera::check_temporal_profile_conformance(log, profile); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  profile.entries[0].standard_deviation_seconds = std::numeric_limits<double>::quiet_NaN();
  threw = false;
  try { (void)minera::check_temporal_profile_conformance(log, profile); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void imported_first_class_selectors_reach_temporal_behavioral_adapters() {
  std::istringstream input(
      "case_ref,phase,occurred_at\n"
      "c1,A,1970-01-01T00:00:00Z\n"
      "c1,B,1970-01-01T00:00:10Z\n"
      "c1,A,1970-01-01T00:00:20Z\n");
  minera::CsvImportOptions import_options;
  import_options.case_id_column = "case_ref";
  import_options.activity_column = "phase";
  import_options.timestamp_column = "occurred_at";
  import_options.require_timestamp_column = true;
  const auto log = minera::import_csv_event_log(input, import_options);
  assert(log.traces.size() == 1 && log.traces.front().events.size() == 3);

  minera::MinimumSelfDistanceOptions msd_options;
  msd_options.activity_key = "phase";
  msd_options.case_id_key = "case_ref";
  const auto distances =
      minera::discover_minimum_self_distances(log, msd_options);
  assert(distances.size() == 1 && distances.front().activity == "A");
  assert(distances.front().distance == 1);
  assert((distances.front().witnesses == std::vector<std::string>{"B"}));

  minera::TransitionSystemOptions transition_options;
  transition_options.window = 1;
  transition_options.activity_attribute = "phase";
  transition_options.case_id_attribute = "case_ref";
  const auto transition =
      minera::discover_transition_system(log, transition_options);
  assert(transition.transition_count() == 3);

  minera::TemporalProfileOptions temporal_options;
  temporal_options.activity_attribute = "phase";
  temporal_options.source_timestamp_attribute = "occurred_at";
  temporal_options.target_timestamp_attribute = "occurred_at";
  const auto profile =
      minera::discover_temporal_profile_log(log, temporal_options);
  assert(profile.find("A", "B") != nullptr);
  assert(profile.find("A", "B")->mean_seconds == 10.0);

  minera::PerformanceSpectrumOptions spectrum_options;
  spectrum_options.activity_attribute = "phase";
  spectrum_options.timestamp_attribute = "occurred_at";
  spectrum_options.case_id_attribute = "case_ref";
  const auto spectrum = minera::discover_performance_spectrum_log(
      log, {"A", "B"}, spectrum_options);
  assert(!spectrum.observations.empty());
  assert(spectrum.observations.front().case_id == "c1");

  const auto table = minera::event_log_to_table(log);
  const auto table_distances = minera::MinimumSelfDistancePandasDiscovery::apply(
      table, msd_options);
  assert(table_distances == distances);
  const auto table_transition =
      minera::discover_transition_system(table, transition_options);
  assert(table_transition.transition_count() == 3);
  temporal_options.case_id_attribute = "case_ref";
  const auto table_profile =
      minera::discover_temporal_profile_dataframe(table, temporal_options);
  assert(table_profile.find("A", "B") != nullptr);
  const auto table_spectrum =
      minera::discover_performance_spectrum_dataframe(
          table, {"A", "B"}, spectrum_options);
  assert(!table_spectrum.observations.empty());
}

std::filesystem::path write_temp_log(const std::string& content) {
  auto path = std::filesystem::temp_directory_path() / "minera_core_test.csv";
  std::ofstream output(path);
  output << content;
  return path;
}

void imports_csv_and_discovers_directly_follows_graph() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:01:00\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,C,2026-01-01T00:01:00\n"
      "2,B,2026-01-01T00:02:00\n");

  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 2);
  assert(log.event_count() == 5);

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().size() == 3);
  assert(graph.edges().size() == 3);
  assert(graph.activity_count("A") == 2);
  assert(graph.activity_count("B") == 2);
  assert(graph.activity_count("C") == 1);
  assert(graph.start_activity_count("A") == 2);
  assert(graph.end_activity_count("B") == 2);
  assert(graph.edge_count("A", "B") == 1);
  assert(graph.edge_count("A", "C") == 1);
  assert(graph.edge_count("C", "B") == 1);
  assert(graph.edge_count("B", "A") == 0);
  assert(graph.variants().size() == 2);
}

void handles_quoted_csv_fields_and_timestamp_sorting() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "\"case,1\",Done,2026-01-01T00:02:00\n"
      "\"case,1\",\"Review \"\"fast\"\"\",2026-01-01T00:01:00\n"
      "\"case,1\",Start,2026-01-01T00:00:00\n");

  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.traces.front().case_id == "case,1");
  assert(log.traces.front().events[0].activity == "Start");
  assert(log.traces.front().events[1].activity == "Review \"fast\"");
  assert(log.traces.front().events[2].activity == "Done");

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("Start", "Review \"fast\"") == 1);
  assert(graph.edge_count("Review \"fast\"", "Done") == 1);
}

void aggregates_repeated_trace_variants() {
  const auto path = write_temp_log(
      "case_id,activity\n"
      "1,A\n"
      "1,B\n"
      "2,A\n"
      "2,B\n"
      "3,A\n"
      "3,C\n");

  minera::CsvImportOptions options;
  options.timestamp_column = "missing_timestamp";

  const auto log = minera::import_csv_event_log(path, options);
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 2);
  assert(graph.edge_count("A", "C") == 1);
  assert(graph.variants().size() == 2);
  assert(graph.variants()[0].count == 2);
  assert(graph.variants()[1].count == 1);
}

minera::EventLog log_from_variants(
    const std::vector<std::vector<std::string>>& variants) {
  minera::EventLog log;
  std::size_t case_index = 0;
  for (const auto& variant : variants) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& activity : variant) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::vector<std::string> activity_names(const minera::Trace& trace) {
  std::vector<std::string> names;
  for (const auto& event : trace.events) names.push_back(event.activity);
  return names;
}

// Renders a process tree to a canonical S-expression: ->(a, b), X(a, b),
// +(a, b), *(a, b), leaf activity names, and "tau" for silent steps.
std::string render(const minera::ProcessTree& tree,
                   const minera::ProcessTreeNode& node) {
  using Op = minera::ProcessTreeOperator;
  switch (node.op) {
    case Op::kActivity:
      return tree.activities()[node.activity];
    case Op::kTau:
      return "tau";
    default:
      break;
  }

  std::string symbol;
  switch (node.op) {
    case Op::kSequence: symbol = "->"; break;
    case Op::kExclusive: symbol = "X"; break;
    case Op::kParallel: symbol = "+"; break;
    case Op::kLoop: symbol = "*"; break;
    default: symbol = "?"; break;
  }

  std::string out = symbol + "(";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    out += render(tree, node.children[i]);
    if (i + 1 < node.children.size()) {
      out += ", ";
    }
  }
  out += ")";
  return out;
}

std::string mine_tree(const std::vector<std::vector<std::string>>& variants) {
  const auto log = log_from_variants(variants);
  const auto tree = minera::discover_process_tree(log);
  return render(tree, tree.root());
}

std::string mine_tree_imf(const std::vector<std::vector<std::string>>& variants,
                          double noise_threshold) {
  const auto log = log_from_variants(variants);
  minera::InductiveMinerOptions options;
  options.noise_threshold = noise_threshold;
  options.variant = minera::InductiveMinerVariant::kImf;
  const auto tree = minera::discover_process_tree(log, options);
  return render(tree, tree.root());
}

void inductive_miner_discovers_sequence() {
  assert(mine_tree({{"a", "b", "c"}}) == "->(a, b, c)");
}

void inductive_miner_discovers_exclusive_choice() {
  // a then either b or c then d.
  assert(mine_tree({{"a", "b", "d"}, {"a", "c", "d"}}) ==
         "->(a, X(b, c), d)");
}

void inductive_miner_discovers_parallel() {
  // b and c are concurrent between a and d.
  const auto tree = mine_tree({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}});
  assert(tree == "->(a, +(b, c), d)");
}

void inductive_miner_discovers_loop() {
  // a, then optionally repeat (b then a).
  const auto tree =
      mine_tree({{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}});
  assert(tree == "*(a, b)");
}

void inductive_miner_models_optional_skip() {
  // No cut applies, so the empty trace falls through to an explicit skip around
  // the rest of the behaviour (here a self-loop on the single activity).
  assert(mine_tree({{}, {"a"}, {"a", "a"}}) == "X(tau, *(a, tau))");
}

void inductive_miner_models_empty_trace_before_cut() {
  // The optional empty trace is separated before the a->b sequence cut, so it
  // remains one explicit tau branch around the complete non-empty model.
  assert(mine_tree({{}, {"a", "b"}}) == "X(tau, ->(a, b))");
}

void inductive_miner_repairs_a_cycle_with_specialized_fallthroughs() {
  // A directly-follows cycle has no ordinary cut. Removing a reveals the
  // optional b-then-c structure, so the activity-concurrent repair isolates a
  // as the other parallel branch.
  const auto tree = mine_tree({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  assert(tree == "+(->(X(tau, b), X(tau, c)), X(tau, a))");
}

void inductive_miner_infrequent_recovers_a_noisy_sequence() {
  // A dominant a->b->c->d sequence with one rare trace that adds infrequent
  // a->c and c->a hops (and an extra a). The infrequent edges form a cycle that
  // derails the classic miner, but the IMf threshold filters them out.
  std::vector<std::vector<std::string>> variants(20, {"a", "b", "c", "d"});
  variants.push_back({"a", "c", "a", "b", "c", "d"});

  // Classic IM (threshold 0) is misled by the rare edges.
  assert(mine_tree_imf(variants, 0.0) != "->(a, b, c, d)");
  // IMf drops the rare cross-activity edges. Projected repeated activities
  // remain visible self-loops because filtering does not erase trace
  // multiplicity.
  assert(mine_tree_imf(variants, 0.2) == "->(a, b, *(c, tau), d)");
}

void inductive_miner_infrequent_leaves_a_clean_log_untouched() {
  // With no infrequent behaviour there is nothing to filter, so a non-zero
  // threshold yields exactly the same model as the classic miner.
  assert(mine_tree_imf({{"a", "b", "c"}}, 0.3) == "->(a, b, c)");
  assert(mine_tree_imf({{"a", "b", "d"}, {"a", "c", "d"}}, 0.3) ==
         "->(a, X(b, c), d)");
}

void inductive_miner_infrequent_filters_an_edge_at_the_cutoff() {
  // Node `a` has a dominant successor `b` (ten a-b-c traces) and an infrequent
  // shortcut straight to `c` (two a-c traces). IMf keeps an outgoing edge only
  // when its weight clears the threshold times the node's strongest outgoing
  // weight: 0.15 * 10 = 1.5 and 0.25 * 10 = 2.5 bracket the shortcut's weight of
  // 2. So a 0.15 threshold keeps the shortcut — b becomes skippable — while 0.25
  // drops it as noise and recovers the clean chain. This pins the direction of
  // the cutoff: raising the threshold removes infrequent behaviour, never adds
  // it. The exact integer boundary is strict: a count equal to f * max_out is
  // filtered rather than retained.
  std::vector<std::vector<std::string>> variants(10, {"a", "b", "c"});
  variants.push_back({"a", "c"});
  variants.push_back({"a", "c"});
  assert(mine_tree_imf(variants, 0.15) == "->(a, X(tau, b), c)");
  assert(mine_tree_imf(variants, 0.2) == "->(a, b, c)");
  assert(mine_tree_imf(variants, 0.25) == "->(a, b, c)");
}

void inductive_miner_infrequent_terminates_on_a_filtered_exclusive_cut() {
  // The noise filter drops a->c (1 vs a's dominant a->b at 10), so the
  // filtered graph falls apart into {a, b} and {c} and an exclusive cut
  // fires even though the a-c trace spans both groups. The split must route
  // that trace by majority and project it onto the winning group; routing it
  // whole (by its first activity) reproduces the parent sublog verbatim and
  // recursed forever.
  std::vector<std::vector<std::string>> variants(10, {"a", "b"});
  variants.push_back({"a", "c"});
  assert(mine_tree_imf(variants, 0.5) == "->(a, X(b, c))");
}

void inductive_miner_infrequent_keeps_structure_the_filter_would_destroy() {
  // a and b are genuinely parallel, but at threshold 0.5 the start filter
  // would drop the rarer branch and kill the parallel cut. IMf filters only
  // when the unfiltered relations have no cut, so the parallel structure must
  // survive a high threshold instead of collapsing to a flower.
  std::vector<std::vector<std::string>> variants(10, {"a", "b"});
  variants.push_back({"b", "a"});
  variants.push_back({"b", "a"});
  assert(mine_tree_imf(variants, 0.5) == "+(a, b)");
}

std::string mine_tree_imd(const std::vector<std::vector<std::string>>& variants,
                          double noise_threshold = 0.0) {
  const auto log = log_from_variants(variants);
  minera::InductiveMinerOptions options;
  options.noise_threshold = noise_threshold;
  options.variant = minera::InductiveMinerVariant::kDfg;
  const auto tree = minera::discover_process_tree(log, options);
  return render(tree, tree.root());
}

void inductive_miner_dfg_matches_log_recursion_on_structured_logs() {
  // On logs whose structure the directly-follows graph fully captures, the
  // graph-projection recursion must find the same tree as the log recursion.
  assert(mine_tree_imd({{"a", "b", "c"}}) == "->(a, b, c)");
  assert(mine_tree_imd({{"a", "b", "d"}, {"a", "c", "d"}}) ==
         "->(a, X(b, c), d)");
  assert(mine_tree_imd({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}) ==
         "->(a, +(b, c), d)");
  assert(mine_tree_imd({{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}}) ==
         "*(a, b)");
  // The flower fall-through is shared too.
  assert(mine_tree_imd({{"a", "b"}, {"b", "c"}, {"c", "a"}}) ==
         "*(tau, X(a, b, c))");
}

void inductive_miner_dfg_skips_empty_traces_at_the_root() {
  // A directly-follows graph cannot see empty traces, so IMd decides the skip
  // once at the root instead of riding it through every split like the log
  // recursion (which yields "->(X(tau, a), X(tau, b))" here).
  assert(mine_tree_imd({{}, {"a", "b"}}) == "X(tau, ->(a, b))");
}

void inductive_miner_dfg_ignores_the_infrequent_threshold() {
  // The graph-recursive variant has no infrequent-filtering mode. Supplying a
  // threshold therefore leaves its discovered model unchanged.
  std::vector<std::vector<std::string>> variants(20, {"a", "b", "c", "d"});
  variants.push_back({"a", "c", "a", "b", "c", "d"});
  assert(mine_tree_imd(variants, 0.2) == mine_tree_imd(variants, 0.0));
}

void inductive_miner_accepts_direct_dfg_input() {
  const auto log = log_from_variants({{"a", "b", "c"},
                                      {"a", "b", "d"},
                                      {"a", "c", "d"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  minera::InductiveMinerOptions options;
  // Graph input always uses IMd semantics, even when the scalar selector is
  // left at its log-recursive default.
  const auto tree = minera::discover_process_tree(graph, options);
  const auto expected = minera::discover_process_tree(
      log, {.variant = minera::InductiveMinerVariant::kDfg});
  assert(render(tree, tree.root()) == render(expected, expected.root()));

  const auto empty_graph =
      minera::discover_directly_follows_graph(minera::EventLog{});
  const auto empty_tree = minera::discover_process_tree(empty_graph);
  assert(empty_tree.root().op == minera::ProcessTreeOperator::kTau);
}

void inductive_miner_infrequent_drops_a_stray_edge_from_an_end_activity() {
  // The rare trace adds a->c and c->a hops. c ends twenty-one traces, so
  // ending is its dominant behaviour and the stray c->a edge (its only
  // outgoing edge) is noise relative to it. Without weighing the end count,
  // c->a could never be filtered and the unresolvable cycle would collapse
  // this log to a flower.
  std::vector<std::vector<std::string>> variants(20, {"a", "b", "c"});
  variants.push_back({"a", "c", "a", "b", "c"});
  assert(mine_tree_imf(variants, 0.2) ==
         "->(a, b, *(c, tau))");
}

void inductive_miner_infrequent_preserves_observed_end_activities() {
  std::vector<std::vector<std::string>> variants(10, {"a", "b", "a"});
  variants.push_back({"a", "b"});
  // The rare b-ending trace remains an observed endpoint. The once-per-trace
  // repair isolates b in parallel with the repeated a behaviour.
  assert(mine_tree_imf(variants, 0.5) == "+(b, *(a, tau))");
}

void inductive_miner_infrequent_uses_total_trace_weight_for_empty_noise() {
  std::vector<std::vector<std::string>> boundary(9, {"a", "b"});
  boundary.push_back({});
  // 1/10 equals the threshold and is therefore dropped.
  assert(mine_tree_imf(boundary, 0.1) == "->(a, b)");

  boundary.assign(8, {"a", "b"});
  boundary.push_back({});
  // 1/9 is above the threshold and is retained as one optional branch.
  assert(mine_tree_imf(boundary, 0.1) == "X(tau, ->(a, b))");

  // Graph recursion carries the source UVCL skip flag independently of the
  // noise threshold, so even a rare empty trace remains represented.
  boundary.assign(9, {"a", "b"});
  boundary.push_back({});
  assert(mine_tree_imd(boundary, 0.5) == "X(tau, ->(a, b))");
}

minera::ProcessTree tree_from_variants(
    const std::vector<std::vector<std::string>>& variants) {
  return minera::discover_process_tree(log_from_variants(variants));
}

void petri_net_translates_sequence() {
  const auto tree = tree_from_variants({{"a", "b", "c"}});
  const auto net = minera::to_petri_net(tree);
  // source + sink + two places between the three activities.
  assert(net.place_count() == 4);
  assert(net.transitions().size() == 3);
  for (const auto& transition : net.transitions()) {
    assert(!transition.silent());
  }
}

void petri_net_reduces_parallel_routing() {
  const auto tree = tree_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}});
  const auto net = minera::to_petri_net(tree);
  std::size_t silent = 0;
  const minera::PetriTransition* first = nullptr;
  const minera::PetriTransition* last = nullptr;
  for (const auto& transition : net.transitions()) {
    if (transition.silent()) ++silent;
    if (transition.activity && *transition.activity == 0) first = &transition;
    if (transition.activity && *transition.activity == 3) last = &transition;
  }
  // Ordinary reduction fuses the private split/join routing places into the
  // surrounding visible transitions while preserving the AND bundles.
  assert(silent == 0);
  assert(first != nullptr && first->outputs.size() == 2);
  assert(last != nullptr && last->inputs.size() == 2);
}

bool contains(const std::vector<std::size_t>& places, std::size_t place) {
  return std::find(places.begin(), places.end(), place) != places.end();
}

const minera::PetriTransition* transition_for(const minera::PetriNet& net,
                                              const std::string& activity) {
  for (std::size_t id = 0; id < net.activities().size(); ++id) {
    if (net.activities()[id] != activity) continue;
    for (const auto& transition : net.transitions()) {
      if (transition.activity && *transition.activity == id) return &transition;
    }
  }
  return nullptr;
}

void alpha_miner_discovers_a_sequence() {
  const auto net = minera::discover_petri_net(log_from_variants({{"a", "b", "c"}}));
  // source + sink + one place between each consecutive activity pair.
  assert(net.place_count() == 4);
  assert(net.transitions().size() == 3);
  const auto* a = transition_for(net, "a");
  const auto* c = transition_for(net, "c");
  assert(a != nullptr && c != nullptr);
  assert(contains(a->inputs, net.source()));   // start activity fed by source
  assert(contains(c->outputs, net.sink()));     // end activity drains to sink
  // The Alpha Miner emits only labeled transitions, never silent ones.
  for (const auto& transition : net.transitions()) assert(!transition.silent());
}

void alpha_miner_shares_a_place_for_exclusive_choice() {
  // a, then either b or c, then d. b # c, so one place routes a's token to both
  // branches and one place collects them before d.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"a", "b", "d"}, {"a", "c", "d"}}));
  // source, sink, ({a},{b,c}) and ({b,c},{d}).
  assert(net.place_count() == 4);
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  const auto* c = transition_for(net, "c");
  const auto* d = transition_for(net, "d");
  assert(a && b && c && d);
  // a's lone output place is exactly the shared entry of branches b and c.
  assert(a->outputs.size() == 1);
  const std::size_t branch_in = a->outputs.front();
  assert(contains(b->inputs, branch_in) && contains(c->inputs, branch_in));
  // b and c merge into one place that feeds d.
  assert(b->outputs.size() == 1 && b->outputs == c->outputs);
  assert(contains(d->inputs, b->outputs.front()));
}

void alpha_miner_keeps_parallel_branches_on_separate_places() {
  // b and c are concurrent between a and d (a||... no: b||c). Classic Alpha
  // represents concurrency as two independent places, never a shared one, so a
  // forks into two places and d joins from two.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}));
  // source, sink, ({a},{b}), ({a},{c}), ({b},{d}), ({c},{d}).
  assert(net.place_count() == 6);
  const auto* a = transition_for(net, "a");
  const auto* d = transition_for(net, "d");
  assert(a && d);
  assert(a->outputs.size() == 2);  // one place per concurrent branch
  assert(d->inputs.size() == 2);
  // No place is shared between b and c (that would force an order between them).
  const auto* b = transition_for(net, "b");
  const auto* c = transition_for(net, "c");
  assert(b && c);
  for (const auto place : b->inputs) assert(!contains(c->inputs, place));
}

void alpha_miner_classic_drops_short_loop_activities() {
  // A length-two loop (b after a, a after b) makes a and b parallel under the
  // footprint, so the classic algorithm cannot place either — the textbook
  // short-loop limitation. The transitions still exist but carry no arcs.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"x", "a", "b", "a", "b", "a", "y"}, {"x", "a", "y"}}),
      {.short_loops = false});
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  assert(a && b);
  assert(b->inputs.empty() && b->outputs.empty());
}

void alpha_plus_attaches_a_length_one_loop() {
  // t repeats directly between a and b, so classic Alpha sees t > t and can
  // never place it. Alpha+ mines the net without t, then creates a separate
  // self-loop place for t while retaining the original a-to-b place.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"a", "t", "t", "b"}, {"a", "b"}}),
      {.short_loops = true});
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  const auto* t = transition_for(net, "t");
  assert(a && b && t);
  // The ordinary flow and loop place are distinct components.
  assert(net.place_count() == 4);
  assert(a->outputs.size() == 1 && a->outputs == b->inputs);
  assert(t->inputs.size() == 1 && t->inputs == t->outputs);
  assert(t->inputs != a->outputs);
}

void alpha_plus_discovers_a_length_two_loop() {
  // a and b alternate (a b a b a): both "a b a" and "b a b" occur, which true
  // concurrency of single occurrences cannot produce, so Alpha+ reads causality
  // in both directions and wires the loop instead of dropping the arcs.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"x", "a", "b", "a", "b", "a", "y"}, {"x", "a", "y"}}),
      {.short_loops = true});
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  assert(a && b);
  // b loops back: its input is a's output place and its output is a's input.
  assert(a->outputs.size() == 1 && b->inputs == a->outputs);
  assert(a->inputs.size() == 1 && b->outputs == a->inputs);
}

void alpha_plus_matches_classic_without_short_loops() {
  // This structured log permits both artificial boundaries to collapse.
  // Compare wiring independent of place numbering after that cleanup.
  const auto log = log_from_variants(
      {{"a", "b", "c", "e"}, {"a", "c", "b", "e"}, {"a", "d", "e"}});
  const auto plus = minera::discover_petri_net(log, {.short_loops = true});
  const auto classic = minera::discover_petri_net(log, {.short_loops = false});
  assert(plus.place_count() == classic.place_count());
  assert(plus.transitions().size() == classic.transitions().size());
  const auto shape = [](const minera::PetriNet& net) {
    using PlaceShape = std::tuple<bool, bool, std::vector<std::size_t>,
                                 std::vector<std::size_t>>;
    std::vector<PlaceShape> result;
    for (std::size_t p = 0; p < net.place_count(); ++p) {
      std::vector<std::size_t> before;
      std::vector<std::size_t> after;
      for (const auto& transition : net.transitions()) {
        assert(transition.activity);
        if (contains(transition.outputs, p)) before.push_back(*transition.activity);
        if (contains(transition.inputs, p)) after.push_back(*transition.activity);
      }
      result.emplace_back(p == net.source(), p == net.sink(), before, after);
    }
    std::sort(result.begin(), result.end());
    return result;
  };
  assert(shape(plus) == shape(classic));
}

void alpha_miner_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto net = minera::discover_petri_net(empty);
  assert(net.place_count() == 2);  // just source and sink
  assert(net.transitions().empty());
}

void alpha_miner_accepts_direct_dfg_input() {
  const auto log = log_from_variants({{"a", "b", "c"},
                                      {"a", "b", "d"},
                                      {"a", "c", "d"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto from_log = minera::discover_petri_net(log, {.short_loops = false});
  const auto from_graph = minera::discover_petri_net(graph);
  assert(from_graph.activities() == from_log.activities());
  assert(from_graph.place_count() == from_log.place_count());
  assert(from_graph.source() == from_log.source());
  assert(from_graph.sink() == from_log.sink());
  assert(from_graph.transitions().size() == from_log.transitions().size());
  for (std::size_t i = 0; i < from_log.transitions().size(); ++i) {
    assert(from_graph.transitions()[i].activity ==
           from_log.transitions()[i].activity);
    assert(from_graph.transitions()[i].inputs ==
           from_log.transitions()[i].inputs);
    assert(from_graph.transitions()[i].outputs ==
           from_log.transitions()[i].outputs);
  }

  bool rejected_plus = false;
  try {
    (void)minera::discover_petri_net(graph, {.short_loops = true});
  } catch (const std::invalid_argument&) {
    rejected_plus = true;
  }
  assert(rejected_plus);
}

void json_export_carries_the_petri_net_contract() {
  // The frontend reads these keys by name, so guard them against renames.
  const auto log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto net = minera::discover_petri_net(log);

  const auto json = minera::analysis_json("test", log, graph, nullptr, &net);
  for (const char* key : {"\"petri_net\"", "\"place_count\"", "\"source\"",
                          "\"sink\"", "\"transitions\"", "\"inputs\"",
                          "\"outputs\""}) {
    assert(json.find(key) != std::string::npos);
  }

  // The Petri net is omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"petri_net\"") == std::string::npos);

  // The inductive and heuristic models travel as workflow nets too, each with
  // its replay decoration, so every miner's model can be drawn as a Petri net.
  const auto tree = minera::discover_process_tree(log);
  const auto inductive_net = minera::to_petri_net(tree);
  const auto heuristic_net =
      minera::to_petri_net(minera::discover_heuristics_net(log));
  const auto inductive_decoration = minera::decorate_model(log, inductive_net);
  const auto heuristic_decoration = minera::decorate_model(log, heuristic_net);
  const auto full = minera::analysis_json(
      "test", log, graph, &tree, &net, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, &inductive_net,
      &inductive_decoration, &heuristic_net, &heuristic_decoration);
  for (const char* key :
       {"\"inductive_petri_net\"", "\"inductive_petri_net_decoration\"",
        "\"heuristic_petri_net\"", "\"heuristic_petri_net_decoration\""}) {
    assert(full.find(key) != std::string::npos);
  }
  assert(without.find("\"inductive_petri_net\"") == std::string::npos);
}

void conformance_is_perfect_on_the_discovery_log() {
  const auto log = log_from_variants({{"a", "b", "d"}, {"a", "c", "d"}});
  const auto tree = minera::discover_process_tree(log);
  const auto conformance = minera::check_conformance(log, tree);

  assert(conformance.trace_count == 2);
  assert(conformance.fitting_traces == 2);
  assert(conformance.fitness > 0.999);
  assert(conformance.missing == 0);
  assert(conformance.remaining == 0);
}

void conformance_is_perfect_for_every_operator() {
  // The inductive miner guarantees a perfectly fitting model, so token replay
  // through the workflow net must report fitness 1.0 for every operator — this
  // exercises the parallel AND-join and loop redo paths end to end.
  const std::vector<std::vector<std::vector<std::string>>> logs = {
      {{"a", "b", "c"}},                              // sequence
      {{"a", "b", "d"}, {"a", "c", "d"}},             // exclusive choice
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}},   // parallel
      {{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}},  // loop
      {{}, {"a"}, {"a", "a"}},                        // optional skip + self loop
      {{"a", "b"}, {"b", "c"}, {"c", "a"}},           // flower fallback
  };
  for (const auto& variants : logs) {
    const auto log = log_from_variants(variants);
    const auto tree = minera::discover_process_tree(log);
    const auto conformance = minera::check_conformance(log, tree);
    assert(conformance.fitness > 0.9999);
    assert(conformance.missing == 0);
    assert(conformance.remaining == 0);
    assert(conformance.fitting_traces == conformance.trace_count);
  }
}

void conformance_counts_unmodeled_activities() {
  // Unknown events are not model token moves. The optional strict policy
  // marks their trace unfit without altering token accounting or fitness.
  const auto model_log = log_from_variants({{"a", "b"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto foreign = log_from_variants({{"a", "z", "b"}});
  const auto conformance = minera::check_conformance(foreign, tree);
  assert(conformance.missing == 0 && conformance.fitness == 1.0);
  assert(conformance.fitting_traces == 1);
  minera::TokenReplayOptions options;
  options.consider_activities_not_in_model_in_fitness = true;
  const auto strict = minera::check_conformance(foreign, tree, options);
  assert(strict.fitting_traces == 0 && strict.fitness == conformance.fitness);
  assert(strict.missing == conformance.missing && strict.consumed == conformance.consumed);
}

void conformance_penalizes_a_deviating_trace() {
  // Model is a strict a -> b -> c sequence; the log skips b and reorders.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto deviating = log_from_variants({{"a", "c"}});
  const auto conformance = minera::check_conformance(deviating, tree);

  assert(conformance.fitting_traces == 0);
  assert(conformance.fitness < 1.0);
  assert(conformance.missing > 0);
  // The worst-fitting variant is surfaced first.
  assert(!conformance.variants.empty());
  assert(conformance.variants.front().fitness < 1.0);
}

void backwards_replay_supports_legacy_variation() {
  const auto log = log_from_variants({{"a", "b"}, {"a", "b"}});
  const auto tree = minera::discover_process_tree(log);
  minera::TokenReplayOptions options;
  options.variant = minera::TokenReplayVariant::BACKWARDS;
  const auto result = minera::check_conformance(log, tree, options);
  assert(result.trace_count == 2);
  assert(result.fitting_traces == 2);
  assert(result.missing == 0 && result.remaining == 0);

  // A visible transition blocked behind two silent steps is found by the
  // backwards requirement search and replayed without creating a token.
  minera::PetriNet routed;
  const auto source = routed.add_place();
  const auto middle = routed.add_place();
  const auto gate = routed.add_place();
  const auto sink = routed.add_place();
  routed.set_source(source);
  routed.set_sink(sink);
  routed.set_activities({"a"});
  routed.add_transition(std::nullopt, {source}, {middle});
  routed.add_transition(std::nullopt, {middle}, {gate});
  routed.add_transition(0, {gate}, {sink});
  const auto routed_result = minera::check_conformance(
      log_from_variants({{"a"}}), routed, options);
  assert(routed_result.fitting_traces == 1);
  assert(routed_result.missing == 0 && routed_result.remaining == 0);

  // Repeated output arcs model a weighted silent transition. Reverse diffing
  // removes each arc independently, while the extra produced token remains.
  minera::PetriNet weighted;
  const auto ws = weighted.add_place();
  const auto wg = weighted.add_place();
  const auto we = weighted.add_place();
  weighted.set_source(ws);
  weighted.set_sink(we);
  weighted.set_activities({"a"});
  weighted.add_transition(std::nullopt, {ws}, {wg, wg});
  weighted.add_transition(0, {wg}, {we});
  const auto weighted_result = minera::check_conformance(
      log_from_variants({{"a"}}), weighted, options);
  assert(weighted_result.fitting_traces == 0);
  assert(weighted_result.missing == 1);
  assert(weighted_result.remaining == 1);
}

void backwards_replay_marks_unroutable_and_stops_when_requested() {
  minera::TokenReplayOptions options;
  options.variant = minera::TokenReplayVariant::BACKWARDS;

  // No silent route to the input place: the unique visible transition is
  // weak-fired and its missing input is counted.
  minera::PetriNet blocked;
  const auto source = blocked.add_place();
  const auto required = blocked.add_place();
  const auto sink = blocked.add_place();
  blocked.set_source(source);
  blocked.set_sink(sink);
  blocked.set_activities({"a"});
  blocked.add_transition(0, {required}, {sink});
  const auto blocked_result = minera::check_conformance(
      log_from_variants({{"a"}}), blocked, options);
  assert(blocked_result.fitting_traces == 0);
  assert(blocked_result.missing > 0);

  // Stopping immediately prevents later events from executing/accounting.
  minera::PetriNet stop_net;
  const auto s = stop_net.add_place();
  const auto need = stop_net.add_place();
  const auto end = stop_net.add_place();
  stop_net.set_source(s);
  stop_net.set_sink(end);
  stop_net.set_activities({"a", "b"});
  stop_net.add_transition(0, {need}, {need});  // unreachable a
  stop_net.add_transition(1, {s}, {end});      // b is initially enabled
  const auto continued = minera::check_conformance(
      log_from_variants({{"a", "b"}}), stop_net, options);
  options.stop_immediately_unfit = true;
  const auto stopped = minera::check_conformance(
      log_from_variants({{"a", "b"}}), stop_net, options);
  assert(continued.consumed > stopped.consumed);
  assert(continued.produced > stopped.produced);
}

void backwards_replay_ignores_unknown_and_interrupts_duplicate_labels() {
  minera::TokenReplayOptions options;
  options.variant = minera::TokenReplayVariant::BACKWARDS;

  const auto model = log_from_variants({{"a"}});
  const auto tree = minera::discover_process_tree(model);
  const auto unknown = log_from_variants({{"a", "unknown"}});
  const auto backwards = minera::check_conformance(unknown, tree, options);
  const auto forward = minera::check_conformance(unknown, tree);
  assert(backwards.fitting_traces == 1 && backwards.missing == 0);
  // Unknown labels do not consume or create model tokens in either variation.
  assert(forward.fitting_traces == 1 && forward.missing == 0);

  minera::PetriNet duplicate;
  const auto source = duplicate.add_place();
  const auto gate = duplicate.add_place();
  const auto sink = duplicate.add_place();
  duplicate.set_source(source);
  duplicate.set_sink(sink);
  duplicate.set_activities({"a"});
  duplicate.add_transition(std::nullopt, {source}, {gate});
  duplicate.add_transition(0, {gate}, {sink});
  duplicate.add_transition(0, {gate}, {sink});
  const auto interrupted = minera::check_conformance(
      log_from_variants({{"a"}}), duplicate, options);
  assert(interrupted.fitting_traces == 0);

  // Duplicate labels are visited in transition order against the evolving
  // marking, so a first firing may enable the next same-label transition.
  minera::PetriNet evolving;
  const auto start = evolving.add_place();
  const auto middle = evolving.add_place();
  const auto finish = evolving.add_place();
  evolving.set_source(start);
  evolving.set_sink(finish);
  evolving.set_activities({"a"});
  evolving.add_transition(0, {start}, {middle});
  evolving.add_transition(0, {middle}, {finish});
  const auto evolved = minera::check_conformance(
      log_from_variants({{"a"}}), evolving, options);
  assert(evolved.fitting_traces == 1);
}

void precision_is_high_for_a_tight_model_and_low_for_a_flower() {
  const auto sequence_log = log_from_variants({{"a", "b", "c"}});
  const auto sequence_tree = minera::discover_process_tree(sequence_log);
  const auto tight = minera::check_conformance(sequence_log, sequence_tree);
  assert(tight.precision > 0.99);

  // A directly-follows cycle yields a flower model that permits any activity at
  // any point when specialized repairs are disabled, so escaping-edges
  // precision drops sharply.
  const auto flower_log = log_from_variants({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  minera::InductiveMinerOptions flower_options;
  flower_options.disable_fallthroughs = true;
  const auto flower_tree =
      minera::discover_process_tree(flower_log, flower_options);
  const auto loose = minera::check_conformance(flower_log, flower_tree);
  assert(loose.fitness > 0.999);  // a flower still reproduces the log
  assert(loose.precision < 0.85);
  assert(loose.precision < tight.precision);
}

void generalization_rewards_a_well_exercised_model() {
  // The a -> b model has exactly two transitions, each firing once per trace.
  // A single trace leaves both barely observed (1/sqrt(1) each), the floor of
  // the measure; four traces halve each rarity term.
  const auto once = log_from_variants({{"a", "b"}});
  const auto tree = minera::discover_process_tree(once);
  const auto sparse = minera::check_conformance(once, tree);
  assert(sparse.generalization < 0.001);

  const auto often =
      log_from_variants({{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "b"}});
  const auto dense = minera::check_conformance(often, tree);
  assert(dense.generalization > 0.499 && dense.generalization < 0.501);
}

void simplicity_reflects_routing_density() {
  // A pure sequence needs no arcs beyond the two-per-node chaining baseline,
  // so it sits at the measure's ceiling.
  const auto sequence_log = log_from_variants({{"a", "b", "c"}});
  const auto sequence = minera::check_conformance(
      sequence_log, minera::discover_process_tree(sequence_log));
  assert(sequence.simplicity > 0.999);

  // The flower fallback funnels every activity through shared loop places,
  // pushing the mean degree past the baseline.
  const auto flower_log =
      log_from_variants({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  const auto flower = minera::check_conformance(
      flower_log, minera::discover_process_tree(flower_log));
  assert(flower.simplicity < sequence.simplicity);
  assert(flower.simplicity > 0.0);
}

void footprints_match_the_discovery_log() {
  // The inductive model perfectly fits its log, so every succession the log
  // shows the model also allows: footprint fitness is 1 with no log-only gaps.
  const auto log = log_from_variants(
      {{"a", "b", "c"}, {"a", "c", "b"}});  // b and c parallel
  const auto tree = minera::discover_process_tree(log);
  const auto footprints = minera::check_footprints(log, tree);
  assert(footprints.fitness > 0.999);
  assert(footprints.log_only.empty());
  assert(footprints.log_relations > 0);
  assert(footprints.shared_relations == footprints.log_relations);
}

void footprints_flag_a_forbidden_succession() {
  // Model is a strict a -> b -> c chain. A log that also puts c before b shows
  // a succession (c -> b) the model forbids, so footprint fitness drops and the
  // deviation is reported.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto log = log_from_variants({{"a", "c", "b"}});
  const auto footprints = minera::check_footprints(log, tree);
  assert(footprints.fitness < 1.0);
  bool found_cb = false;
  for (const auto& dev : footprints.log_only) {
    if (dev.source == "c" && dev.target == "b") found_cb = true;
  }
  assert(found_cb);
}

void footprints_detect_a_too_loose_model() {
  // A directly-follows cycle yields a flower model that allows every activity to
  // follow every other, so it has successions the tight log never shows:
  // footprint precision falls well below 1 and model-only pairs are reported.
  const auto log = log_from_variants({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  const auto tree = minera::discover_process_tree(log);
  const auto footprints = minera::check_footprints(log, tree);
  assert(footprints.precision < 1.0);
  assert(!footprints.model_only.empty());
}

template <typename Fn>
bool throws(Fn&& fn) {
  try {
    fn();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

void pnml_round_trip_preserves_replay_behavior() {
  // A net written by to_pnml and read back by parse_pnml must replay the log
  // identically — same token counts, same footprint relation.
  const auto log = log_from_variants(
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}, {"a", "e", "d"}});
  const auto tree = minera::discover_process_tree(log);
  const auto net = minera::to_petri_net(tree);
  const auto reread = minera::parse_pnml(minera::to_pnml(net));

  assert(reread.place_count() == net.place_count());
  assert(reread.transitions().size() == net.transitions().size());

  const auto before = minera::check_conformance(log, net);
  const auto after = minera::check_conformance(log, reread);
  assert(before.fitness == after.fitness);
  assert(before.produced == after.produced);
  assert(before.missing == after.missing);

  const auto footprints_before = minera::check_footprints(log, net);
  const auto footprints_after = minera::check_footprints(log, reread);
  assert(footprints_before.fitness == footprints_after.fitness);
  assert(footprints_before.model_relations == footprints_after.model_relations);

  assert(minera::final_marking_reachable(reread));
  assert(minera::check_alignment(log, reread).fitness == 1.0);
}

// A hand-written PNML the way other tools emit it: explicit markings, an
// invisible routing transition, an escaped label, and single quotes.
constexpr const char* kForeignPnml = R"(<?xml version="1.0"?>
<pnml xmlns="http://www.pnml.org/version-2009/grammar/pnml">
  <net id='n1' type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name><text>foreign</text></name>
    <page id="g1">
      <place id="start"><initialMarking><text>1</text></initialMarking></place>
      <place id="mid"/>
      <place id="end"/>
      <transition id="t_a"><name><text>a &amp; go</text></name></transition>
      <transition id="t_skip">
        <name><text>skip</text></name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$"/>
      </transition>
      <arc id="1" source="start" target="t_a"/>
      <arc id="2" source="t_a" target="mid"/>
      <arc id="3" source="mid" target="t_skip"/>
      <arc id="4" source="t_skip" target="end"/>
    </page>
    <finalmarkings>
      <marking><place idref="end"><text>1</text></place></marking>
    </finalmarkings>
  </net>
</pnml>)";

void parse_pnml_reads_a_foreign_net() {
  const auto net = minera::parse_pnml(kForeignPnml);
  assert(net.place_count() == 3);
  assert(net.transitions().size() == 2);
  // The toolspecific $invisible$ flag wins over the "skip" label.
  assert((net.activities() == std::vector<std::string>{"a & go"}));
  assert(!net.transitions()[0].silent());
  assert(net.transitions()[1].silent());
  assert(net.source() == 0);  // "start", the marked place
  assert(net.sink() == 2);    // "end", from <finalmarkings>

  // The single visible activity replays a matching one-event log perfectly.
  const auto log = log_from_variants({{"a & go"}});
  assert(minera::check_conformance(log, net).fitness == 1.0);

  // Against a log with a different activity, both deviations are reported:
  // "x" is unmodeled (log-only) and "a & go" is unobserved (model-only).
  const auto other = log_from_variants({{"x", "a & go"}});
  const auto footprints = minera::check_footprints(other, net);
  assert(footprints.fitness == 0.0);
  assert(footprints.log_relations == 1);
}

void parse_pnml_infers_missing_markings() {
  // No initialMarking and no finalmarkings: the unique place without incoming
  // arcs is the source, the unique place without outgoing arcs the sink.
  const auto net = minera::parse_pnml(R"(<pnml><net id="n"><page id="g">
    <place id="p0"/><place id="p1"/>
    <transition id="t"><name><text>a</text></name></transition>
    <arc id="1" source="p0" target="t"/>
    <arc id="2" source="t" target="p1"/>
  </page></net></pnml>)");
  assert(net.source() == 0);
  assert(net.sink() == 1);
  assert(minera::check_conformance(log_from_variants({{"a"}}), net).fitness ==
         1.0);
}

void parse_pnml_rejects_malformed_nets() {
  // An arc referencing an id that exists as neither place nor transition.
  assert(throws([] {
    (void)minera::parse_pnml(R"(<pnml><net id="n"><page id="g">
      <place id="p0"/><place id="p1"/>
      <transition id="t"/>
      <arc id="1" source="p0" target="ghost"/>
      <arc id="2" source="t" target="p1"/>
    </page></net></pnml>)");
  }));
  // Two initially marked places: not a workflow net.
  assert(throws([] {
    (void)minera::parse_pnml(R"(<pnml><net id="n"><page id="g">
      <place id="p0"><initialMarking><text>1</text></initialMarking></place>
      <place id="p1"><initialMarking><text>1</text></initialMarking></place>
      <place id="p2"/>
      <transition id="t"/>
      <arc id="1" source="p0" target="t"/>
      <arc id="2" source="p1" target="t"/>
      <arc id="3" source="t" target="p2"/>
    </page></net></pnml>)");
  }));
  // No net at all.
  assert(throws([] { (void)minera::parse_pnml("<pnml></pnml>"); }));
}

void bpmn_round_trip_preserves_replay_behavior() {
  const auto log = log_from_variants(
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}, {"a", "e", "d"}});
  const auto tree = minera::discover_process_tree(log);
  const auto net = minera::to_petri_net(tree);

  // The inductive model fits the log perfectly; written as BPMN by either
  // exporter (from the tree or from its net) and read back by parse_bpmn, it
  // must still fit perfectly, by replay and by optimal alignment.
  for (const auto& xml : {minera::to_bpmn(tree), minera::to_bpmn(net)}) {
    assert(minera::looks_like_bpmn(xml));
    const auto reread = minera::parse_bpmn(xml);
    assert(minera::final_marking_reachable(reread));
    assert(minera::check_conformance(log, reread).fitness == 1.0);
    assert(minera::check_footprints(log, reread).fitness == 1.0);
    assert(minera::check_alignment(log, reread).fitness == 1.0);
  }
  assert(!minera::looks_like_bpmn(minera::to_pnml(net)));

  // The other miners' nets need not fit the log (nor be sound), but the trip
  // through BPMN must preserve their language — visible in the footprint the
  // reachable markings induce.
  const auto alpha = minera::discover_petri_net(log);
  const auto heuristic =
      minera::to_petri_net(minera::discover_heuristics_net(log));
  for (const auto* model : {&alpha, &heuristic}) {
    const auto reread = minera::parse_bpmn(minera::to_bpmn(*model));
    const auto before = minera::check_footprints(log, *model);
    const auto after = minera::check_footprints(log, reread);
    assert(before.fitness == after.fitness);
    assert(before.precision == after.precision);
    assert(before.model_relations == after.model_relations);
  }
}

// A hand-written BPMN model the way modelers export it: every element
// namespace-qualified, a parallel block between two gateways, an exclusive
// choice, an escaped label, and a diagram section to ignore.
constexpr const char* kForeignBpmn = R"(<?xml version="1.0"?>
<bpmn2:definitions xmlns:bpmn2="http://www.omg.org/spec/BPMN/20100524/MODEL"
    xmlns:bpmndi="http://www.omg.org/spec/BPMN/20100524/DI" id="defs">
  <bpmn2:process id="p1" isExecutable="false">
    <bpmn2:startEvent id="start"/>
    <bpmn2:task id="reg" name="register &amp; file"/>
    <bpmn2:parallelGateway id="split"/>
    <bpmn2:userTask id="check" name="check"/>
    <bpmn2:serviceTask id="notify" name="notify"/>
    <bpmn2:parallelGateway id="join"/>
    <bpmn2:exclusiveGateway id="choice"/>
    <bpmn2:task id="approve" name="approve"/>
    <bpmn2:task id="reject" name="reject"/>
    <bpmn2:exclusiveGateway id="merge"/>
    <bpmn2:endEvent id="end"/>
    <bpmn2:sequenceFlow id="f1" sourceRef="start" targetRef="reg"/>
    <bpmn2:sequenceFlow id="f2" sourceRef="reg" targetRef="split"/>
    <bpmn2:sequenceFlow id="f3" sourceRef="split" targetRef="check"/>
    <bpmn2:sequenceFlow id="f4" sourceRef="split" targetRef="notify"/>
    <bpmn2:sequenceFlow id="f5" sourceRef="check" targetRef="join"/>
    <bpmn2:sequenceFlow id="f6" sourceRef="notify" targetRef="join"/>
    <bpmn2:sequenceFlow id="f7" sourceRef="join" targetRef="choice"/>
    <bpmn2:sequenceFlow id="f8" sourceRef="choice" targetRef="approve"/>
    <bpmn2:sequenceFlow id="f9" sourceRef="choice" targetRef="reject"/>
    <bpmn2:sequenceFlow id="f10" sourceRef="approve" targetRef="merge"/>
    <bpmn2:sequenceFlow id="f11" sourceRef="reject" targetRef="merge"/>
    <bpmn2:sequenceFlow id="f12" sourceRef="merge" targetRef="end"/>
  </bpmn2:process>
  <bpmndi:BPMNDiagram id="diagram"/>
</bpmn2:definitions>)";

void parse_bpmn_reads_a_foreign_model() {
  const auto net = minera::parse_bpmn(kForeignBpmn);
  assert((net.activities() == std::vector<std::string>{
              "register & file", "check", "notify", "approve", "reject"}));
  assert(minera::final_marking_reachable(net));

  // The parallel gateways admit both interleavings, the exclusive gateway
  // exactly one of its branches.
  const auto fitting = log_from_variants(
      {{"register & file", "check", "notify", "approve"},
       {"register & file", "notify", "check", "reject"}});
  assert(minera::check_conformance(fitting, net).fitness == 1.0);
  assert(minera::check_alignment(fitting, net).fitness == 1.0);

  // Skipping one parallel branch or running both choice branches deviates.
  const auto broken = log_from_variants(
      {{"register & file", "check", "approve"},
       {"register & file", "check", "notify", "approve", "reject"}});
  assert(minera::check_alignment(broken, net).fitness < 1.0);
}

void parse_bpmn_keeps_disconnected_tasks() {
  // An activity that is disconnected in the mined net exports as a task no
  // sequence flow reaches; reading it back must keep it a dead transition —
  // absent from replayed behaviour but still counted against the log that
  // uses it — instead of failing the whole import.
  const auto net = minera::parse_bpmn(R"(<definitions>
    <process id="p">
      <startEvent id="s"/>
      <task id="a" name="a"/>
      <task id="orphan" name="orphan"/>
      <endEvent id="e"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="a"/>
      <sequenceFlow id="f2" sourceRef="a" targetRef="e"/>
    </process>
  </definitions>)");
  assert((net.activities() == std::vector<std::string>{"a", "orphan"}));
  assert(minera::final_marking_reachable(net));
  assert(minera::check_conformance(log_from_variants({{"a"}}), net).fitness ==
         1.0);
  assert(minera::check_conformance(log_from_variants({{"a", "orphan"}}), net)
             .fitness < 1.0);
}

void parse_bpmn_reads_subprocesses() {
  // An expanded subprocess converts recursively between its own start and
  // end events; a collapsed (childless) one behaves as an atomic task.
  const auto net = minera::parse_bpmn(R"(<definitions>
    <process id="p">
      <startEvent id="s"/>
      <task id="a" name="a"/>
      <subProcess id="sub" name="handle">
        <startEvent id="ss"/>
        <task id="b" name="b"/>
        <endEvent id="se"/>
        <sequenceFlow id="g1" sourceRef="ss" targetRef="b"/>
        <sequenceFlow id="g2" sourceRef="b" targetRef="se"/>
      </subProcess>
      <subProcess id="atom" name="archive"/>
      <endEvent id="e"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="a"/>
      <sequenceFlow id="f2" sourceRef="a" targetRef="sub"/>
      <sequenceFlow id="f3" sourceRef="sub" targetRef="atom"/>
      <sequenceFlow id="f4" sourceRef="atom" targetRef="e"/>
    </process>
  </definitions>)");
  assert((net.activities() ==
          std::vector<std::string>{"a", "b", "archive"}));
  const auto log = log_from_variants({{"a", "b", "archive"}});
  assert(minera::check_conformance(log, net).fitness == 1.0);
  assert(minera::check_alignment(log, net).fitness == 1.0);
}

void parse_bpmn_reads_interrupting_boundary_events() {
  const auto net = minera::parse_bpmn(R"(<definitions><process id="p">
    <startEvent id="start"/><task id="work" name="work"/>
    <boundaryEvent id="timeout" name="timeout" attachedToRef="work"><timerEventDefinition/></boundaryEvent>
    <task id="recover" name="recover"/><endEvent id="done"/><endEvent id="failed"/>
    <sequenceFlow id="f1" sourceRef="start" targetRef="work"/>
    <sequenceFlow id="f2" sourceRef="work" targetRef="done"/>
    <sequenceFlow id="f3" sourceRef="timeout" targetRef="recover"/>
    <sequenceFlow id="f4" sourceRef="recover" targetRef="failed"/>
  </process></definitions>)");
  minera::EventLog log;
  for (const auto& activities :
       std::vector<std::vector<std::string>>{{"work"},
                                             {"timeout", "recover"},
                                             {"work", "timeout", "recover"}}) {
    minera::Trace trace;
    trace.case_id = "case_" + std::to_string(log.traces.size());
    for (const auto& activity : activities)
      trace.events.push_back({trace.case_id, activity});
    log.traces.push_back(std::move(trace));
  }
  const auto result = minera::check_conformance(log, net);
  assert(result.trace_count == 3);
  assert(result.fitting_traces == 2);

  // A collapsed subprocess is atomic too. Multiple attached events are
  // mutually exclusive alternatives to normal completion.
  const auto collapsed = minera::parse_bpmn(R"(<definitions><process id="p">
    <startEvent id="start"/><subProcess id="work" name="work"/>
    <boundaryEvent id="timeout" attachedToRef="work"/>
    <boundaryEvent id="cancel" attachedToRef="work"/>
    <task id="recover" name="recover"/><task id="cancelled" name="cancelled"/>
    <endEvent id="done"/><endEvent id="failed"/><endEvent id="aborted"/>
    <sequenceFlow id="f1" sourceRef="start" targetRef="work"/>
    <sequenceFlow id="f2" sourceRef="work" targetRef="done"/>
    <sequenceFlow id="f3" sourceRef="timeout" targetRef="recover"/>
    <sequenceFlow id="f4" sourceRef="recover" targetRef="failed"/>
    <sequenceFlow id="f5" sourceRef="cancel" targetRef="cancelled"/>
    <sequenceFlow id="f6" sourceRef="cancelled" targetRef="aborted"/>
  </process></definitions>)");
  assert(minera::check_conformance(
             log_from_variants({{"work"}, {"recover"}, {"cancelled"}}),
             collapsed)
             .fitness == 1.0);
  assert(minera::check_conformance(
             log_from_variants({{"work", "recover"},
                                {"recover", "cancelled"}}),
             collapsed)
             .fitness < 1.0);

  const auto non_interrupting = minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><task id="a" name="A"/>
      <boundaryEvent id="b" name="alert" attachedToRef="a" cancelActivity="false"/>
      <task id="handle" name="handle"/><endEvent id="e"/><endEvent id="be"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="a"/>
      <sequenceFlow id="f2" sourceRef="a" targetRef="e"/>
      <sequenceFlow id="f3" sourceRef="b" targetRef="handle"/>
      <sequenceFlow id="f4" sourceRef="handle" targetRef="be"/>
    </process></definitions>)");
  assert(minera::check_alignment(
             log_from_variants({{"A"}, {"alert", "handle", "A"},
                                {"alert", "A", "handle"},
                                {"A", "alert", "handle"}}),
             non_interrupting)
             .fitness == 1.0);
  assert(minera::check_conformance(
             log_from_variants({{"alert", "alert", "handle", "handle", "A"}}),
             non_interrupting)
             .fitting_traces == 0);

  assert(throws([] {
    (void)minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><task id="a" name="A"/>
      <boundaryEvent id="b" attachedToRef="missing"/>
      <endEvent id="e"/><sequenceFlow id="f1" sourceRef="s" targetRef="a"/>
      <sequenceFlow id="f2" sourceRef="a" targetRef="e"/>
      <sequenceFlow id="f3" sourceRef="b" targetRef="e"/>
    </process></definitions>)");
  }));

  assert(throws([] {
    (void)minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><task id="a" name="A"/>
      <boundaryEvent id="b" attachedToRef="a" cancelActivity="sometimes"/>
      <endEvent id="e"/><sequenceFlow id="f1" sourceRef="s" targetRef="a"/>
      <sequenceFlow id="f2" sourceRef="a" targetRef="e"/>
      <sequenceFlow id="f3" sourceRef="b" targetRef="e"/>
    </process></definitions>)");
  }));

  const auto expanded = minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><subProcess id="sub">
        <startEvent id="ss"/><task id="a" name="A"/><task id="b" name="B"/>
        <endEvent id="se"/>
        <sequenceFlow id="g1" sourceRef="ss" targetRef="a"/>
        <sequenceFlow id="g2" sourceRef="a" targetRef="b"/>
        <sequenceFlow id="g3" sourceRef="b" targetRef="se"/>
      </subProcess>
      <boundaryEvent id="interrupt" attachedToRef="sub"/>
      <task id="recover" name="recover"/><endEvent id="e"/><endEvent id="failed"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="sub"/>
      <sequenceFlow id="f2" sourceRef="sub" targetRef="e"/>
      <sequenceFlow id="f3" sourceRef="interrupt" targetRef="recover"/>
      <sequenceFlow id="f4" sourceRef="recover" targetRef="failed"/>
    </process></definitions>)");
  assert(expanded.has_extended_arcs());
  assert(minera::check_alignment(
             log_from_variants({{"A", "B"}, {"recover"}, {"A", "recover"}}),
             expanded)
             .fitness == 1.0);
  assert(minera::check_alignment(
             log_from_variants({{"A", "recover", "B"},
                                {"A", "B", "recover"}}),
             expanded)
             .fitness < 1.0);

  const auto expanded_non_interrupting = minera::parse_bpmn(
      R"(<definitions><process id="p"><startEvent id="s"/><subProcess id="sub">
        <startEvent id="ss"/><task id="a" name="A"/><task id="b" name="B"/>
        <endEvent id="se"/><sequenceFlow id="g1" sourceRef="ss" targetRef="a"/>
        <sequenceFlow id="g2" sourceRef="a" targetRef="b"/>
        <sequenceFlow id="g3" sourceRef="b" targetRef="se"/>
      </subProcess><boundaryEvent id="notice" name="notice" attachedToRef="sub"
        cancelActivity="false"/><task id="handle" name="handle"/>
      <endEvent id="e"/><endEvent id="ne"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="sub"/>
      <sequenceFlow id="f2" sourceRef="sub" targetRef="e"/>
      <sequenceFlow id="f3" sourceRef="notice" targetRef="handle"/>
      <sequenceFlow id="f4" sourceRef="handle" targetRef="ne"/>
    </process></definitions>)");
  assert(minera::check_alignment(
             log_from_variants({{"A", "B"},
                                {"A", "notice", "handle", "B"},
                                {"notice", "A", "B", "handle"}}),
             expanded_non_interrupting)
             .fitness == 1.0);
  assert(minera::check_conformance(
             log_from_variants({{"notice", "notice", "A", "B", "handle"}}),
             expanded_non_interrupting)
             .fitting_traces == 0);

  const auto nested = minera::parse_bpmn(R"(<definitions><process id="p">
    <startEvent id="s"/><subProcess id="outer">
      <startEvent id="os"/><subProcess id="inner">
        <startEvent id="is"/><task id="a" name="A"/><task id="b" name="B"/>
        <endEvent id="ie"/><sequenceFlow id="i1" sourceRef="is" targetRef="a"/>
        <sequenceFlow id="i2" sourceRef="a" targetRef="b"/>
        <sequenceFlow id="i3" sourceRef="b" targetRef="ie"/>
      </subProcess><endEvent id="oe"/>
      <sequenceFlow id="o1" sourceRef="os" targetRef="inner"/>
      <sequenceFlow id="o2" sourceRef="inner" targetRef="oe"/>
    </subProcess>
    <boundaryEvent id="interrupt" attachedToRef="outer"/>
    <task id="recover" name="recover"/><endEvent id="e"/><endEvent id="failed"/>
    <sequenceFlow id="f1" sourceRef="s" targetRef="outer"/>
    <sequenceFlow id="f2" sourceRef="outer" targetRef="e"/>
    <sequenceFlow id="f3" sourceRef="interrupt" targetRef="recover"/>
    <sequenceFlow id="f4" sourceRef="recover" targetRef="failed"/>
  </process></definitions>)");
  assert(minera::check_alignment(
             log_from_variants({{"A", "recover"}, {"A", "B"}}), nested)
             .fitness == 1.0);
  assert(minera::check_alignment(
             log_from_variants({{"A", "recover", "B"}}), nested)
             .fitness < 1.0);
}

void parse_bpmn_supports_inclusive_gateway_blocks() {
  // A structured inclusive split/join admits every non-empty branch subset,
  // including either serial interleaving when both branches are selected.
  const auto net = minera::parse_bpmn(R"(<definitions><process id="p">
    <startEvent id="s"/><inclusiveGateway id="split"/>
    <task id="a" name="a"/><task id="b" name="b"/>
    <inclusiveGateway id="join"/><endEvent id="e"/>
    <sequenceFlow id="f1" sourceRef="s" targetRef="split"/>
    <sequenceFlow id="f2" sourceRef="split" targetRef="a"/>
    <sequenceFlow id="f3" sourceRef="split" targetRef="b"/>
    <sequenceFlow id="f4" sourceRef="a" targetRef="join"/>
    <sequenceFlow id="f5" sourceRef="b" targetRef="join"/>
    <sequenceFlow id="f6" sourceRef="join" targetRef="e"/>
  </process></definitions>)");
  assert(minera::final_marking_reachable(net));
  const auto fitting = log_from_variants(
      {{"a"}, {"b"}, {"a", "b"}, {"b", "a"}});
  assert(minera::check_conformance(fitting, net).fitness == 1.0);
  assert(minera::check_alignment(fitting, net).fitness == 1.0);
  assert(minera::check_conformance(log_from_variants({{}}), net).fitness <
         1.0);
  assert(minera::check_alignment(log_from_variants({{"a", "a"}}), net)
             .fitness < 1.0);

  // The exporter emits the same structured shape.  A silent branch is a
  // direct split-to-join flow and still must not make the empty subset valid.
  const minera::ProcessTree tree(
      {"a", "b"},
      {minera::ProcessTreeOperator::kInclusiveChoice, 0,
       {minera::ProcessTreeNode::leaf(0),
        minera::ProcessTreeNode::tau(),
        minera::ProcessTreeNode::leaf(1)}});
  const auto xml = minera::to_bpmn(tree);
  const auto reread = minera::parse_bpmn(xml);
  assert(minera::final_marking_reachable(reread));
  const auto tree_fitting = log_from_variants(
      {{"a"}, {"b"}, {"a", "b"}, {"b", "a"}});
  assert(minera::check_conformance(tree_fitting, reread).fitness == 1.0);
  // Selecting only the silent branch is a valid non-empty subset, so the
  // visible empty trace remains in the language of this exported tree.
  // Greedy token replay may select every skip instead; use exact alignment
  // to verify the exported model's language.
  assert(minera::check_alignment(log_from_variants({{}}), reread).fitness ==
         1.0);

  // Nested inclusive blocks pair by reachability, not by gateway names.  The
  // inner block may select either activity or both while the outer block also
  // selects the sibling branch.
  const minera::ProcessTree nested(
      {"a", "b", "c"},
      {minera::ProcessTreeOperator::kInclusiveChoice, 0,
       {minera::ProcessTreeNode{
            minera::ProcessTreeOperator::kInclusiveChoice, 0,
            {minera::ProcessTreeNode::leaf(0),
             minera::ProcessTreeNode::leaf(1)}},
        minera::ProcessTreeNode::leaf(2)}});
  const auto nested_reread = minera::parse_bpmn(minera::to_bpmn(nested));
  const auto nested_fitting = log_from_variants(
      {{"a"}, {"b"}, {"a", "b"}, {"c"}, {"a", "c"}, {"c", "a"},
       {"a", "b", "c"}, {"c", "b", "a"}});
  assert(minera::check_conformance(nested_fitting, nested_reread).fitness ==
         1.0);
  assert(minera::check_alignment(nested_fitting, nested_reread).fitness ==
         1.0);
  assert(minera::check_conformance(log_from_variants({{}}), nested_reread)
             .fitness < 1.0);
}

void parse_bpmn_rejects_unsupported_models() {
  // Complex gateways remain outside the structured workflow-net contract.
  assert(throws([] {
    (void)minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><complexGateway id="g"/><endEvent id="e"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="g"/>
      <sequenceFlow id="f2" sourceRef="g" targetRef="e"/>
    </process></definitions>)");
  }));
  // A mixed-arity inclusive gateway cannot be paired without changing the
  // standard's activation and synchronization semantics.
  assert(throws([] {
    (void)minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><task id="p"/><task id="q"/>
      <inclusiveGateway id="mixed"/><task id="a"/><task id="b"/>
      <endEvent id="e"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="p"/>
      <sequenceFlow id="f2" sourceRef="s" targetRef="q"/>
      <sequenceFlow id="f3" sourceRef="p" targetRef="mixed"/>
      <sequenceFlow id="f4" sourceRef="q" targetRef="mixed"/>
      <sequenceFlow id="f5" sourceRef="mixed" targetRef="a"/>
      <sequenceFlow id="f6" sourceRef="mixed" targetRef="b"/>
      <sequenceFlow id="f7" sourceRef="a" targetRef="e"/>
      <sequenceFlow id="f8" sourceRef="b" targetRef="e"/>
    </process></definitions>)");
  }));
  // A sequence flow referencing a node that does not exist.
  assert(throws([] {
    (void)minera::parse_bpmn(R"(<definitions><process id="p">
      <startEvent id="s"/><endEvent id="e"/>
      <sequenceFlow id="f1" sourceRef="s" targetRef="ghost"/>
    </process></definitions>)");
  }));
  // A process without a start or end event cannot be replayed source-to-sink.
  assert(throws([] {
    (void)minera::parse_bpmn(R"(<definitions><process id="p">
      <task id="a" name="a"/><endEvent id="e"/>
      <sequenceFlow id="f1" sourceRef="a" targetRef="e"/>
    </process></definitions>)");
  }));
  // No process at all.
  assert(throws([] { (void)minera::parse_bpmn("<definitions/>"); }));
}

void decorate_model_counts_firings_and_waits() {
  // Three timed cases against the (perfectly fitting) inductive net: the
  // shared place after "a" collects one wait sample per case — the gap
  // between "a" and whichever branch follows.
  minera::EventLog log;
  const auto add_trace =
      [&](const char* id,
          std::vector<std::pair<std::string, std::string>> steps) {
        minera::Trace trace;
        trace.case_id = id;
        for (auto& [activity, timestamp] : steps) {
          minera::Event event;
          event.case_id = id;
          event.activity = activity;
          event.timestamp = timestamp;
          trace.events.push_back(std::move(event));
        }
        log.traces.push_back(std::move(trace));
      };
  add_trace("c1", {{"a", "2024-01-01T10:00:00Z"},
                   {"b", "2024-01-01T10:01:00Z"}});  // 60 s
  add_trace("c2", {{"a", "2024-01-02T10:00:00Z"},
                   {"b", "2024-01-02T10:03:00Z"}});  // 180 s
  add_trace("c3", {{"a", "2024-01-03T10:00:00Z"},
                   {"c", "2024-01-03T10:02:00Z"}});  // 120 s

  const auto tree = minera::discover_process_tree(log);
  const auto net = minera::to_petri_net(tree);
  const auto decoration = minera::decorate_model(log, net);

  assert(decoration.trace_count == 3);
  assert(decoration.decorated_traces == 3);

  const auto transition_of = [&](const std::string& name) {
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& transition = net.transitions()[t];
      if (!transition.silent() &&
          net.activities()[*transition.activity] == name) {
        return t;
      }
    }
    assert(false);
    return std::size_t{0};
  };
  assert(decoration.transition_counts[transition_of("a")] == 3);
  assert(decoration.transition_counts[transition_of("b")] == 2);
  assert(decoration.transition_counts[transition_of("c")] == 1);

  // The place feeding "b" (shared with "c" by the exclusive choice) saw all
  // three tokens; its waits are the a→next gaps.
  const auto& b = net.transitions()[transition_of("b")];
  assert(b.inputs.size() == 1);
  const auto place = b.inputs.front();
  assert(decoration.place_counts[place] == 3);
  const auto& wait = decoration.place_waits[place];
  assert(wait.count == 3);
  assert(wait.min == 60.0);
  assert(wait.median == 120.0);
  assert(wait.max == 180.0);
  assert(wait.mean == 120.0);

  // The initial token is stamped with the case's first instant, so the source
  // place measures no artificial wait.
  assert(decoration.place_counts[net.source()] == 3);
  assert(decoration.place_waits[net.source()].max == 0.0);
}

void decorate_model_skips_deviating_and_untimed_traces() {
  // The net knows only {a, b}; the {a, x} trace deviates, so it contributes
  // nothing. Without timestamps the counts still tally but no waits appear.
  const auto fitting = log_from_variants({{"a", "b"}, {"a", "b"}});
  const auto tree = minera::discover_process_tree(fitting);
  const auto net = minera::to_petri_net(tree);

  const auto mixed = log_from_variants({{"a", "b"}, {"a", "x"}});
  const auto decoration = minera::decorate_model(mixed, net);
  assert(decoration.trace_count == 2);
  assert(decoration.decorated_traces == 1);
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    if (transition.silent()) continue;
    assert(decoration.transition_counts[t] == 1);  // one clean a, one clean b
  }
  for (const auto& wait : decoration.place_waits) {
    assert(wait.count == 0);  // log_from_variants carries no timestamps
  }
}

void ptml_round_trip_preserves_replay_behavior() {
  // A mined tree written as PTML and read back must accept the same language:
  // the perfectly fitting log still fits, by replay and by alignment.
  const auto log = log_from_variants(
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}, {"a", "e", "d"}});
  const auto tree = minera::discover_process_tree(log);
  const auto ptml = minera::to_ptml(tree);
  assert(minera::looks_like_ptml(ptml));
  assert(!minera::looks_like_ptml(minera::to_bpmn(tree)));

  const auto reread = minera::parse_ptml(ptml);
  const auto net = minera::to_petri_net(reread);
  assert(minera::final_marking_reachable(net));
  assert(minera::check_conformance(log, net).fitness == 1.0);
  assert(minera::check_footprints(log, net).fitness == 1.0);
  assert(minera::check_alignment(log, net).fitness == 1.0);

  // A loop with several redo alternatives survives the three-child export
  // shape (the redos come back bundled under one exclusive choice).
  minera::ProcessTreeNode loop;
  loop.op = minera::ProcessTreeOperator::kLoop;
  loop.children.push_back(minera::ProcessTreeNode::leaf(0));
  loop.children.push_back(minera::ProcessTreeNode::leaf(1));
  loop.children.push_back(minera::ProcessTreeNode::leaf(2));
  const minera::ProcessTree looped({"work", "redo", "escalate"}, loop);
  const auto reread_loop = minera::parse_ptml(minera::to_ptml(looped));
  const auto loop_net = minera::to_petri_net(reread_loop);
  const auto loop_log = log_from_variants(
      {{"work"},
       {"work", "redo", "work"},
       {"work", "escalate", "work", "redo", "work"}});
  assert(minera::check_alignment(loop_log, loop_net).fitness == 1.0);
}

// A hand-written PTML the way other tools export it: a root attribute, the
// three-child loop shape with a non-silent exit, and an escaped label.
constexpr const char* kForeignPtml = R"(<?xml version="1.0"?>
<ptml>
  <processTree id="t" name="foreign" root="n0">
    <sequence id="n0" name=""/>
    <manualTask id="n1" name="register &amp; file"/>
    <xorLoop id="n2" name=""/>
    <manualTask id="n3" name="work"/>
    <manualTask id="n4" name="redo"/>
    <manualTask id="n5" name="close"/>
    <parentsNode id="e0" sourceId="n0" targetId="n1"/>
    <parentsNode id="e1" sourceId="n0" targetId="n2"/>
    <parentsNode id="e2" sourceId="n2" targetId="n3"/>
    <parentsNode id="e3" sourceId="n2" targetId="n4"/>
    <parentsNode id="e4" sourceId="n2" targetId="n5"/>
  </processTree>
</ptml>)";

void parse_ptml_reads_a_foreign_tree() {
  const auto tree = minera::parse_ptml(kForeignPtml);
  assert((tree.activities() == std::vector<std::string>{
              "register & file", "work", "redo", "close"}));

  // The loop's non-silent exit runs once when the loop ends: register, then
  // work (redo work)*, then close.
  const auto net = minera::to_petri_net(tree);
  const auto fitting = log_from_variants(
      {{"register & file", "work", "close"},
       {"register & file", "work", "redo", "work", "close"}});
  assert(minera::check_conformance(fitting, net).fitness == 1.0);
  assert(minera::check_alignment(fitting, net).fitness == 1.0);

  // Leaving the loop without its exit deviates.
  const auto broken = log_from_variants({{"register & file", "work"}});
  assert(minera::check_alignment(broken, net).fitness < 1.0);
}

void parse_ptml_rejects_malformed_trees() {
  // Inclusive choice is supported, but it still requires at least one child.
  assert(throws([] {
    (void)minera::parse_ptml(R"(<ptml><processTree id="t" root="n0">
      <or id="n0" name=""/>
    </processTree></ptml>)");
  }));
  // An edge referencing a node that does not exist.
  assert(throws([] {
    (void)minera::parse_ptml(R"(<ptml><processTree id="t" root="n0">
      <sequence id="n0" name=""/><manualTask id="n1" name="a"/>
      <parentsNode id="e0" sourceId="n0" targetId="ghost"/>
    </processTree></ptml>)");
  }));
  // A node with two parents is not a tree.
  assert(throws([] {
    (void)minera::parse_ptml(R"(<ptml><processTree id="t" root="n0">
      <sequence id="n0" name=""/><xor id="n1" name=""/>
      <manualTask id="n2" name="a"/>
      <parentsNode id="e0" sourceId="n0" targetId="n1"/>
      <parentsNode id="e1" sourceId="n0" targetId="n2"/>
      <parentsNode id="e2" sourceId="n1" targetId="n2"/>
    </processTree></ptml>)");
  }));
  // An operator with no children.
  assert(throws([] {
    (void)minera::parse_ptml(R"(<ptml><processTree id="t" root="n0">
      <sequence id="n0" name=""/>
    </processTree></ptml>)");
  }));
  // No tree at all.
  assert(throws([] { (void)minera::parse_ptml("<ptml/>"); }));
}

void xes_export_round_trips_the_log() {
  // A log written by to_xes and read back by the XES importer must carry the
  // same cases, activities, resources, and timestamp clock fields — including
  // XML metacharacters in every field.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case <1> & \"two\"";
  minera::Event first;
  first.case_id = trace.case_id;
  first.activity = "check & <verify>";
  first.timestamp = "2024-03-01 10:00:00+02:00";  // offset form, space sep
  first.resource = "alice 'a'";
  minera::Event second;
  second.case_id = trace.case_id;
  second.activity = "ship";
  second.timestamp = "2024-03-01T09:30:00.250Z";
  trace.events = {first, second};
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log, "unit log");
  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);

  assert(reread.trace_count() == 1);
  assert(reread.traces[0].case_id == trace.case_id);
  assert(reread.traces[0].events.size() == 2);
  assert(reread.traces[0].events[0].activity == first.activity);
  assert(reread.traces[0].events[0].resource == first.resource);
  assert(reread.traces[0].events[1].activity == second.activity);
  assert(reread.traces[0].events[1].resource.empty());

  // Export retains the offset. The default reference importer attaches UTC to
  // clock fields, whereas the Rust importer preserves the absolute instant.
  assert(xes.find("2024-03-01T10:00:00+02:00") != std::string::npos);
  assert(reread.traces[0].events[0].timestamp == "2024-03-01T10:00:00.000Z");
  assert(reread.traces[0].events[1].timestamp == "2024-03-01T09:30:00.250000Z");
  std::istringstream rust_input(xes);
  minera::XesImportOptions rust_options;
  rust_options.variant = minera::XesImportVariant::RUSTXES;
  const auto rust_reread = minera::import_xes_event_log(rust_input, rust_options);
  for (std::size_t i = 0; i < 2; ++i) {
    const auto before = minera::parse_timestamp(trace.events[i].timestamp);
    const auto after =
        minera::parse_timestamp(rust_reread.traces[0].events[i].timestamp);
    assert(before && after && *before == *after);
  }
  assert(rust_reread.traces[0].events[0].timestamp == "2024-03-01T08:00:00.000Z");
}

void xes_export_keeps_unparseable_timestamps_verbatim() {
  // A timestamp that isn't a recognizable instant must not be silently
  // dropped or emitted as an invalid <date>: it survives as a string
  // attribute under the same key, and an empty timestamp is omitted.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  minera::Event odd;
  odd.case_id = "c1";
  odd.activity = "a";
  odd.timestamp = "yesterday-ish";
  minera::Event untimed;
  untimed.case_id = "c1";
  untimed.activity = "b";
  trace.events = {odd, untimed};
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("<string key=\"time:timestamp\" value=\"yesterday-ish\"/>") !=
         std::string::npos);
  assert(xes.find("<date") == std::string::npos);
  // No resource anywhere, so the org extension is not declared.
  assert(xes.find("org.xesext") == std::string::npos);

  // The importer re-orders events by timestamp (lexicographic fallback here),
  // so find them by activity rather than position.
  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  assert(reread.traces[0].events.size() == 2);
  for (const auto& event : reread.traces[0].events) {
    assert(event.timestamp == (event.activity == "a" ? "yesterday-ish" : ""));
  }
}

void graph_counts_rework_cases() {
  // a repeats in the first case only, b in the second only; the third case
  // has no repeats at all.
  const auto log = log_from_variants(
      {{"a", "a", "b"}, {"a", "b", "b", "b"}, {"a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.rework_case_count("a") == 1);
  assert(graph.rework_case_count("b") == 1);
  assert(graph.rework_case_count("unknown") == 0);
}

std::uint64_t efg_count(const std::vector<minera::EventuallyFollowsEdge>& efg,
                        const minera::DirectlyFollowsGraph& graph,
                        std::string_view source, std::string_view target) {
  const auto s = graph.activity_id(source);
  const auto t = graph.activity_id(target);
  assert(s && t);
  for (const auto& edge : efg) {
    if (edge.source == *s && edge.target == *t) return edge.count;
  }
  return 0;
}

void eventually_follows_counts_every_later_occurrence() {
  // ⟨a,b,c⟩ contributes (a,b), (a,c), (b,c); ⟨a,c⟩ contributes (a,c) — so
  // a ↝ c is 2 even though a → c is direct only once.
  const auto log = log_from_variants({{"a", "b", "c"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto efg = minera::discover_eventually_follows(log, graph);

  assert(efg.size() == 3);
  assert(efg_count(efg, graph, "a", "b") == 1);
  assert(efg_count(efg, graph, "a", "c") == 2);
  assert(efg_count(efg, graph, "b", "c") == 1);
  // Never observed in that order.
  assert(efg_count(efg, graph, "c", "a") == 0);
}

void eventually_follows_handles_repeated_activities() {
  // ⟨a,a,b⟩: the position pairs are (a,a), (a,b), (a,b).
  const auto log = log_from_variants({{"a", "a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto efg = minera::discover_eventually_follows(log, graph);

  assert(efg_count(efg, graph, "a", "a") == 1);
  assert(efg_count(efg, graph, "a", "b") == 2);
}

void alignment_precision_is_perfect_for_an_exact_model() {
  // A strict sequence model of its own log allows nothing extra at any
  // aligned state, so alignment-based precision is exactly 1.
  const auto log = log_from_variants({{"a", "b", "c"}});
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));
  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness == 1.0);
  assert(alignment.precision == 1.0);
}

void alignment_precision_flags_unused_model_behavior() {
  // The model offers a choice of b or c after a, but this log only ever takes
  // b: at the post-a state the model enables an activity the log never shows,
  // so precision drops below 1 while fitness stays perfect.
  const auto model_log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto net =
      minera::to_petri_net(minera::discover_process_tree(model_log));
  const auto narrow = log_from_variants({{"a", "b"}});
  const auto alignment = minera::check_alignment(narrow, net);
  assert(alignment.computed);
  assert(alignment.fitness == 1.0);
  assert(alignment.precision < 1.0);
  assert(alignment.precision > 0.0);
}

void alignment_precision_survives_a_deviating_log() {
  // "x" is not in the model at all; the alignment explains it as a log move
  // and precision is still measured at the genuinely reached states.
  const auto model_log = log_from_variants({{"a", "b"}});
  const auto net =
      minera::to_petri_net(minera::discover_process_tree(model_log));
  const auto deviating = log_from_variants({{"a", "x", "b"}});
  const auto alignment = minera::check_alignment(deviating, net);
  assert(alignment.computed);
  assert(alignment.fitness < 1.0);
  assert(alignment.precision > 0.0 && alignment.precision <= 1.0);

  // The JSON contract: the alignments object carries the precision by name.
  const auto graph = minera::discover_directly_follows_graph(deviating);
  const auto json = minera::alignment_json(alignment, graph);
  assert(json.find("\"precision\": ") != std::string::npos);
}

void playout_generates_perfectly_fitting_logs() {
  // A model with a choice and a parallel block. Every simulated trace must
  // replay perfectly on the net it came from, and 50 cases are plenty for
  // both parallel orders (or the choice branch) to show up.
  const auto log = log_from_variants(
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}, {"a", "e", "d"}});
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));

  minera::PlayoutOptions options;
  options.traces = 50;
  const auto generated = minera::playout(net, options);

  assert(generated.trace_count() == 50);
  assert(generated.traces.front().case_id == "case_1");
  assert(minera::check_conformance(generated, net).fitness == 1.0);
  const auto graph = minera::discover_directly_follows_graph(generated);
  assert(graph.variants().size() >= 2);

  // Timestamps parse and strictly increase within a case, so the generated
  // log survives a time-ordering importer unchanged.
  for (const auto& trace : generated.traces) {
    double previous = -1.0;
    for (const auto& event : trace.events) {
      const auto instant = minera::parse_timestamp(event.timestamp);
      assert(instant && *instant > previous);
      previous = *instant;
    }
  }

  // The same seed replays the same log.
  const auto replayed = minera::playout(net, options);
  for (std::size_t i = 0; i < generated.traces.size(); ++i) {
    assert(generated.traces[i].events.size() ==
           replayed.traces[i].events.size());
    for (std::size_t j = 0; j < generated.traces[i].events.size(); ++j) {
      assert(generated.traces[i].events[j].activity ==
             replayed.traces[i].events[j].activity);
    }
  }
}

void playout_simulates_an_imported_model() {
  // The foreign net has one visible activity and a silent router, so every
  // simulated case is exactly ⟨"a & go"⟩.
  const auto net = minera::parse_pnml(kForeignPnml);
  minera::PlayoutOptions options;
  options.traces = 5;
  const auto generated = minera::playout(net, options);
  assert(generated.trace_count() == 5);
  for (const auto& trace : generated.traces) {
    assert(trace.events.size() == 1);
    assert(trace.events[0].activity == "a & go");
  }
}

void extensive_playout_enumerates_the_language() {
  // sequence(a, xor(b, e-loop), d)-ish model: variants {a b d}, {a e d},
  // {a e e d}, … — the enumeration must list exactly the distinct traces the
  // model allows within the length bound, shortest first, each once.
  const auto log = log_from_variants(
      {{"a", "b", "d"}, {"a", "e", "d"}, {"a", "e", "e", "d"}});
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));

  minera::ExtensivePlayoutOptions options;
  options.max_length = 5;
  const auto generated = minera::extensive_playout(net, options);

  // Every enumerated trace fits, no two are alike, lengths never decrease.
  assert(minera::check_conformance(generated, net).fitness == 1.0);
  std::set<std::vector<std::string>> distinct;
  std::size_t previous_length = 0;
  for (const auto& trace : generated.traces) {
    assert(distinct.insert(activity_names(trace)).second);
    assert(trace.events.size() >= previous_length);
    assert(trace.events.size() <= options.max_length);
    previous_length = trace.events.size();
  }
  // The three observed variants are in the language, plus the deeper loop
  // unrolling {a e e e d} the log never showed.
  assert(distinct.count({"a", "b", "d"}) == 1);
  assert(distinct.count({"a", "e", "d"}) == 1);
  assert(distinct.count({"a", "e", "e", "d"}) == 1);
  assert(distinct.count({"a", "e", "e", "e", "d"}) == 1);

  // A tighter length bound trims the language accordingly.
  options.max_length = 3;
  const auto short_traces = minera::extensive_playout(net, options);
  for (const auto& trace : short_traces.traces) {
    assert(trace.events.size() <= 3);
  }
  assert(short_traces.trace_count() >= 2);  // {a b d} and {a e d}

  // The trace cap keeps an infinite language finite, shortest members first.
  options.max_length = 100;
  options.max_traces = 4;
  assert(minera::extensive_playout(net, options).trace_count() == 4);
}

void performance_spectrum_finds_pattern_occurrences() {
  minera::EventLog log;
  const auto add_trace =
      [&](const char* id,
          std::vector<std::pair<std::string, std::string>> steps) {
        minera::Trace trace;
        trace.case_id = id;
        for (auto& [activity, timestamp] : steps) {
          minera::Event event;
          event.case_id = id;
          event.activity = activity;
          event.timestamp = timestamp;
          trace.events.push_back(std::move(event));
        }
        log.traces.push_back(std::move(trace));
      };
  // c1 runs a→b once, with an unrelated activity in between (ignored by the
  // projection); c2 runs the pattern backwards (no match); c3 loops, so both
  // iterations count.
  add_trace("c1", {{"a", "2024-01-01T10:00:00Z"},
                   {"x", "2024-01-01T10:02:00Z"},
                   {"b", "2024-01-01T10:05:00Z"}});
  add_trace("c2", {{"b", "2024-01-02T10:00:00Z"},
                   {"a", "2024-01-02T10:05:00Z"}});
  add_trace("c3", {{"a", "2024-01-03T10:00:00Z"},
                   {"b", "2024-01-03T10:01:00Z"},
                   {"a", "2024-01-03T10:02:00Z"},
                   {"b", "2024-01-03T10:03:00Z"}});

  const auto spectrum =
      minera::compute_performance_spectrum(log, {"a", "b"});
  assert((spectrum.activities == std::vector<std::string>{"a", "b"}));
  // c1 once; c3's projection a b a b has windows [a b], [b a], [a b] — two.
  assert(spectrum.observations.size() == 3);
  assert(spectrum.observations[0].case_id == "c1");
  assert(spectrum.observations[0].instants[1] -
             spectrum.observations[0].instants[0] ==
         300.0);
  assert(spectrum.observations[1].case_id == "c3");
  assert(spectrum.observations[2].case_id == "c3");

  // A one-step pattern has no flow to show.
  assert(throws(
      [&] { (void)minera::compute_performance_spectrum(log, {"a"}); }));

  // The JSON export embeds the spectrum for the frontend/CLI contract.
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto json = minera::analysis_json(
      "t", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, &spectrum);
  assert(json.find("\"performance_spectrum\": {") != std::string::npos);
  assert(json.find("\"instants\": [") != std::string::npos);

  minera::PerformanceSpectrumOptions disconnected_options;
  disconnected_options.variant =
      minera::PerformanceSpectrumVariant::kDisconnected;
  const auto disconnected = minera::compute_performance_spectrum(
      log, {"a", "b"}, disconnected_options);
  assert(disconnected.variant ==
         minera::PerformanceSpectrumVariant::kDisconnected);
  assert(!disconnected.observations.empty());
  assert(disconnected.observations.front().activities.size() >= 2);
  const auto disconnected_json = minera::analysis_json(
      "t", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, &disconnected);
  assert(disconnected_json.find("\"variant\": \"disconnected\"") !=
         std::string::npos);
  assert(disconnected_json.find("\"activities\": [\"a\", \"b\"]") !=
         std::string::npos);

  minera::PerformanceSpectrumOptions dataframe_options;
  dataframe_options.variant = minera::PerformanceSpectrumVariant::kDataframe;
  const auto dataframe = minera::compute_performance_spectrum(
      log, {"a", "b"}, dataframe_options);
  const auto dataframe_json = minera::analysis_json(
      "t", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, &dataframe);
  assert(dataframe_json.find("\"variant\": \"dataframe\"") !=
         std::string::npos);
  assert(dataframe_json.find("\"activity_attribute\": \"activity\"") !=
         std::string::npos);
}

void json_export_carries_the_eventually_follows_contract() {
  // The frontend reads "eventually_follows" by name and expects the same
  // {source, target, count} shape as "edges".
  const auto log = log_from_variants({{"a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto efg = minera::discover_eventually_follows(log, graph);
  const auto json =
      minera::analysis_json("t", log, graph, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr, &efg);
  assert(json.find("\"eventually_follows\": [") != std::string::npos);
  assert(json.find("{\"source\": 0, \"target\": 1, \"count\": 1}") !=
         std::string::npos);
}

void json_export_carries_the_conformance_contract() {
  // The frontend reads these keys by name, so guard them against renames. The
  // "conformance" object is keyed by miner id, each carrying a token-replay and
  // an alignment result.
  const auto log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto tree = minera::discover_process_tree(log);
  const auto net = minera::to_petri_net(tree);
  const std::vector<minera::ModelConformance> conformance = {
      {"inductive", minera::final_marking_reachable(net),
       minera::check_conformance(log, net), minera::check_footprints(log, net),
       minera::check_alignment(log, net)},
  };

  const auto json =
      minera::analysis_json("test", log, graph, &tree, nullptr, nullptr,
                            &conformance);
  for (const char* key : {"\"conformance\"", "\"inductive\"", "\"sound\"",
                          "\"token_replay\"", "\"footprints\"", "\"log_only\"",
                          "\"model_only\"", "\"shared_relations\"",
                          "\"alignments\"", "\"fitness\"",
                          "\"precision\"", "\"generalization\"",
                          "\"simplicity\"", "\"fitting_traces\"",
                          "\"trace_count\"", "\"missing\"", "\"remaining\"",
                          "\"computed\"", "\"exhausted\"", "\"variants\""}) {
    assert(json.find(key) != std::string::npos);
  }

  // Conformance is omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph, &tree);
  assert(without.find("\"conformance\"") == std::string::npos);
}

void conformance_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto tree = minera::discover_process_tree(empty);
  const auto conformance = minera::check_conformance(empty, tree);
  assert(conformance.trace_count == 0);
  assert(conformance.fitting_traces == 0);
  minera::TokenReplayOptions backwards;
  backwards.variant = minera::TokenReplayVariant::BACKWARDS;
  const auto legacy = minera::check_conformance(empty, tree, backwards);
  assert(legacy.trace_count == 0);
  assert(legacy.fitting_traces == 0);
}

void temporal_behavioral_json_validates_and_serializes_batches() {
  minera::EventLog log;
  for (std::size_t i = 0; i < 10; ++i) {
    minera::Trace trace;
    trace.case_id = "c" + std::to_string(i);
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = "a";
    event.resource = "r";
    event.timestamp = std::string("1970-01-01T00:00:") +
                      (i < 10 ? "0" : "") + std::to_string(i) + "Z";
    trace.events.push_back(std::move(event));
    log.traces.push_back(std::move(trace));
  }
  // Add a second activity so spectrum activity registration can be checked.
  minera::Trace extra;
  extra.case_id = "extra";
  minera::Event extra_a;
  extra_a.case_id = "extra";
  extra_a.activity = "a";
  extra_a.timestamp = "1970-01-01T00:00:30Z";
  extra.events.push_back(std::move(extra_a));
  minera::Event extra_event;
  extra_event.case_id = "extra";
  extra_event.activity = "b";
  extra_event.timestamp = "1970-01-01T00:01:00Z";
  extra.events.push_back(std::move(extra_event));
  log.traces.push_back(std::move(extra));
  const auto graph = minera::discover_directly_follows_graph(log);

  minera::BatchDiscovery discovery;
  discovery.groups.emplace_back();
  auto& group = discovery.groups.back();
  group.activity = "a";
  group.resource = "r";
  group.batch_count = 5;
  group.event_count = 10;
  for (std::size_t batch_index = 0; batch_index < 5; ++batch_index) {
    minera::Batch batch;
    batch.type = static_cast<minera::BatchType>(batch_index);
    batch.start = static_cast<double>(batch_index * 2);
    batch.complete = batch.start + 1.5;
    for (std::size_t member = 0; member < 2; ++member) {
      const auto event_index = batch_index * 2 + member;
      double start = batch.start + member * 0.5;
      double complete = start + 0.5;
      if (batch_index == 0) {
        start = batch.start;
        complete = batch.start + 1.5;
      } else if (batch_index == 1) {
        start = batch.start;
        complete = batch.start + 1.0 + member * 0.25;
      } else if (batch_index == 2) {
        start = batch.start + member * 0.25;
        complete = batch.start + 1.5;
      } else if (batch_index == 4) {
        start = batch.start + member * 0.1;
        complete = batch.start + 1.0 + member * 0.1;
      }
      batch.events.push_back({start, complete,
                              "c" + std::to_string(event_index), event_index,
                              0});
    }
    group.batches.push_back(std::move(batch));
  }
  const auto json = minera::analysis_json("t", log, graph, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, &discovery);
  assert(json.find("\"batch_discovery\": {\"groups\"") != std::string::npos);
  assert(json.find("\"variant\": \"log\"") != std::string::npos);
  for (const auto type : {"simultaneous", "batching_on_start",
                          "batching_on_end", "sequential", "concurrent"}) {
    assert(json.find(std::string("\"type\": \"") + type + "\"") !=
           std::string::npos);
  }
  assert(json.find("\"batch_count\": 5") != std::string::npos);

  // Validation runs before any bytes are emitted.
  minera::PerformanceSpectrum malformed_spectrum;
  malformed_spectrum.activities = {"a", "b"};
  malformed_spectrum.observations.push_back({"c0", {1.0}, {"a"}});
  std::ostringstream out;
  out << "sentinel";
  bool threw = false;
  try {
    minera::write_analysis_json(out, "t", log, graph, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, &malformed_spectrum);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(out.str() == "sentinel");

  auto malformed_batch = discovery;
  malformed_batch.groups[0].event_count = 9;
  out.str("");
  out.clear();
  out << "sentinel";
  threw = false;
  try {
    minera::write_analysis_json(out, "t", log, graph, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, &malformed_batch);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(out.str() == "sentinel");

  minera::PerformanceSpectrumOptions disconnected_options;
  disconnected_options.variant =
      minera::PerformanceSpectrumVariant::kDisconnected;
  const auto disconnected = minera::compute_performance_spectrum(
      log, {"a", "b"}, disconnected_options);
  const auto rejects_before_output = [&](const auto& artifact, bool spectrum_artifact) {
    out.str("");
    out.clear();
    out << "sentinel";
    bool rejected = false;
    try {
      if constexpr (std::is_same_v<std::decay_t<decltype(artifact)>,
                                   minera::PerformanceSpectrum>) {
        minera::write_analysis_json(
            out, "t", log, graph, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            &artifact);
      } else {
        minera::write_analysis_json(
            out, "t", log, graph, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, &artifact);
      }
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
    assert(out.str() == "sentinel");
  };
  auto unknown_variant = disconnected;
  unknown_variant.variant = static_cast<minera::PerformanceSpectrumVariant>(99);
  rejects_before_output(unknown_variant, true);
  auto short_disconnected = disconnected;
  short_disconnected.observations.front().activities = {"a"};
  short_disconnected.observations.front().instants = {1.0};
  rejects_before_output(short_disconnected, true);
  auto bad_offset = disconnected;
  bad_offset.observations.front().pattern_offset = 99;
  rejects_before_output(bad_offset, true);

  auto bad_min = discovery;
  bad_min.options.min_batch_size = 3;
  rejects_before_output(bad_min, false);
  auto bad_type = discovery;
  bad_type.groups[0].batches[0].type = minera::BatchType::kConcurrent;
  rejects_before_output(bad_type, false);
  auto bad_enum = discovery;
  bad_enum.groups[0].batches[0].type =
      static_cast<minera::BatchType>(99);
  rejects_before_output(bad_enum, false);
  auto bad_tie = discovery;
  minera::BatchGroup second = bad_tie.groups[0];
  second.activity = "b";
  bad_tie.groups.push_back(std::move(second));
  rejects_before_output(bad_tie, false);
}

// Counts activity-bearing alignment moves of a given type. Silent model moves
// are routing steps rather than activity deviations.
std::size_t count_moves(const minera::VariantAlignment& variant,
                        minera::AlignmentMoveType type) {
  return static_cast<std::size_t>(std::count_if(
      variant.moves.begin(), variant.moves.end(),
      [&](const minera::AlignmentMove& move) {
        return move.type == type &&
               (type != minera::AlignmentMoveType::kModel ||
                !move.activity.empty());
      }));
}

void alignment_is_perfect_for_every_operator() {
  // The inductive miner yields a perfectly fitting model, so every trace must
  // align at cost 0 — only synchronous and silent-routing moves. This walks
  // the AND-join and loop-redo routing the same way the token-replay test does.
  const std::vector<std::vector<std::vector<std::string>>> logs = {
      {{"a", "b", "c"}},                                    // sequence
      {{"a", "b", "d"}, {"a", "c", "d"}},                   // exclusive choice
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}},         // parallel
      {{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}},  // loop
      {{}, {"a"}, {"a", "a"}},                              // optional + self loop
      {{"a", "b"}, {"b", "c"}, {"c", "a"}},                 // flower fallback
  };
  for (const auto& variants : logs) {
    const auto log = log_from_variants(variants);
    const auto tree = minera::discover_process_tree(log);
    const auto alignment = minera::check_alignment(log, tree);
    assert(alignment.fitness > 0.9999);
    assert(alignment.total_cost == 0);
    assert(alignment.fitting_traces == alignment.trace_count);
    for (const auto& variant : alignment.variants) {
      assert(variant.cost == 0);
      assert(count_moves(variant, minera::AlignmentMoveType::kLog) == 0);
      assert(count_moves(variant, minera::AlignmentMoveType::kModel) == 0);
      // Every observed event is explained by a synchronous move.
      assert(count_moves(variant, minera::AlignmentMoveType::kSynchronous) ==
             variant.activities.size());
    }
  }
}

void alignment_diagnoses_a_skipped_activity() {
  // Model is a strict a -> b -> c sequence; the trace skips b. The optimal
  // alignment keeps a and c synchronous and inserts a model move for b.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto deviating = log_from_variants({{"a", "c"}});
  const auto alignment = minera::check_alignment(deviating, tree);

  assert(alignment.fitting_traces == 0);
  assert(alignment.fitness < 1.0);
  assert(!alignment.variants.empty());
  const auto& variant = alignment.variants.front();
  assert(variant.cost == 1);  // one visible model move for the skipped b
  assert(count_moves(variant, minera::AlignmentMoveType::kModel) == 1);
  assert(count_moves(variant, minera::AlignmentMoveType::kSynchronous) == 2);
  // The model move names the activity the trace skipped.
  const auto model_move = std::find_if(
      variant.moves.begin(), variant.moves.end(),
      [](const minera::AlignmentMove& m) {
        return m.type == minera::AlignmentMoveType::kModel;
      });
  assert(model_move != variant.moves.end() && model_move->activity == "b");
}

void alignment_diagnoses_an_inserted_activity() {
  // The model knows only a -> b; the trace inserts an unmodeled z, which can
  // only be explained by a log move.
  const auto model_log = log_from_variants({{"a", "b"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto foreign = log_from_variants({{"a", "z", "b"}});
  const auto alignment = minera::check_alignment(foreign, tree);

  assert(alignment.fitting_traces == 0);
  assert(!alignment.variants.empty());
  const auto& variant = alignment.variants.front();
  assert(variant.cost == 1);  // one log move for the inserted z
  assert(count_moves(variant, minera::AlignmentMoveType::kLog) == 1);
  assert(count_moves(variant, minera::AlignmentMoveType::kSynchronous) == 2);
  const auto log_move = std::find_if(
      variant.moves.begin(), variant.moves.end(),
      [](const minera::AlignmentMove& m) {
        return m.type == minera::AlignmentMoveType::kLog;
      });
  assert(log_move != variant.moves.end() && log_move->activity == "z");
}

void json_export_carries_the_alignment_contract() {
  // A deviating trace makes the per-move alignment keys appear in the JSON, so
  // guard those against renames too.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);
  const auto net = minera::to_petri_net(tree);
  const auto deviating = log_from_variants({{"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(deviating);
  const std::vector<minera::ModelConformance> conformance = {
      {"inductive", minera::final_marking_reachable(net),
       minera::check_conformance(deviating, net),
       minera::check_footprints(deviating, net),
       minera::check_alignment(deviating, net)},
  };

  const auto json = minera::analysis_json("test", deviating, graph, &tree,
                                          nullptr, nullptr, &conformance);
  for (const char* key : {"\"alignments\"", "\"total_cost\"", "\"moves\"",
                          "\"cost\"", "\"type\"", "\"model\"", "\"sync\"",
                          "\"variant\"", "\"diagnostics\"",
                          "\"optimal_alignments\"", "\"activity_attribute\""}) {
    assert(json.find(key) != std::string::npos);
  }

  const minera::ProcessTree tau_tree({}, minera::ProcessTreeNode::tau());
  const auto tau_net = minera::to_petri_net(tau_tree);
  const auto unknown = log_from_variants({{"unknown"}});
  const auto tau_graph = minera::discover_directly_follows_graph(unknown);
  minera::AlignmentOptions tau_options;
  tau_options.variant = minera::AlignmentVariant::kStateEquationAStar;
  const auto tau_json = minera::alignment_json(
      minera::check_alignment(unknown, tau_net, tau_options), tau_graph);
  assert(tau_json.find("\"activity\": null") != std::string::npos);
  assert(tau_json.find("\"silent\": true") != std::string::npos);
}

void final_marking_reachable_distinguishes_sound_nets() {
  // A sound model's final marking is reachable; an Alpha net with concurrent end
  // activities overflows its sink and is not reachable.
  const auto sound = log_from_variants({{"a", "b", "c"}});
  assert(minera::final_marking_reachable(
      minera::to_petri_net(minera::discover_process_tree(sound))));

  const auto concurrent = log_from_variants({{"a", "b", "c"}, {"a", "c", "b"}});
  assert(!minera::final_marking_reachable(
      minera::discover_petri_net(concurrent)));
}

void alignment_scores_a_sound_alpha_net() {
  // The Alpha Miner yields a sound, perfectly-fitting net for an exclusive
  // choice (every run keeps a single token), so the log aligns at zero cost.
  // (A parallel log, by contrast, makes Alpha's shared sink unsound — both
  // branches deposit a token — which the aligner correctly refuses to score.)
  const auto log = log_from_variants({{"a", "b", "d"}, {"a", "c", "d"}});
  const auto net = minera::discover_petri_net(log);
  const auto alignment = minera::check_alignment(log, net);

  assert(alignment.computed);
  assert(alignment.fitness > 0.9999);
  assert(alignment.fitting_traces == alignment.trace_count);
}

void alignment_flags_an_unsound_net() {
  // A net whose only transition drains the source but produces nothing leaves
  // the sink unreachable — not a sound workflow net. The aligner must refuse to
  // score it rather than report a misleading number.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {source}, {});  // consumes the source, produces nothing

  const auto log = log_from_variants({{"a"}});
  const auto alignment = minera::check_alignment(log, net);
  assert(!alignment.computed);
  assert(!alignment.exhausted);  // unreachable sink, not a search-bound timeout
  assert(alignment.variants.empty());
}

void alignment_prunes_an_unbounded_net_with_dead_growth() {
  // A reachable but unbounded net: a silent transition consumes the source and
  // produces the source back plus a token on an auxiliary place, so it can fire
  // forever, growing the marking without bound. The sink stays reachable (a
  // second silent transition drains the source to it), so the net clears the
  // soundness gate. The flooded markings can never reach the final marking (the
  // auxiliary tokens have no consumer), so the marking-equation heuristic
  // prunes every one of them and the alignment computes despite the flood —
  // plain Dijkstra used to exhaust its bound here.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto aux = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({});
  net.add_transition(std::nullopt, {source}, {source, aux});  // unbounded growth
  net.add_transition(std::nullopt, {source}, {sink});          // reaches the sink

  const auto log = log_from_variants({{"x"}});
  minera::AlignmentOptions options;
  options.variant = minera::AlignmentVariant::kStateEquationAStar;
  options.max_states = 1000;
  const auto alignment = minera::check_alignment(log, net, options);
  assert(alignment.computed);
  assert(!alignment.exhausted);
  // The best alignment is one log move for the unknown "x" plus the silent
  // model run to the sink.
  assert(alignment.total_cost == 1);
}

void alignment_flags_an_exhausted_search_on_an_unbounded_net() {
  // The pathological case even the marking-equation heuristic cannot prune: a
  // token-borrowing transition (gate -> gate + gate) makes the equation
  // believe the visible "x" is reachable at zero cost from every marking (its
  // incidence can conjure gate tokens that firing never could, since the gate
  // starts empty), while a source-preserving flood transition grows the
  // marking without bound and a drain keeps the flooded markings equation-
  // feasible. The whole flood sits at estimated cost zero ahead of the real
  // cost-one goal, so the aligner must hit its bound and report `exhausted`
  // (distinct from an unsound net) rather than spin forever or emit a
  // fabricated score.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto aux = net.add_place();
  const auto gate = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"x"});
  net.add_transition(std::nullopt, {source}, {source, aux});  // unbounded growth
  net.add_transition(std::nullopt, {aux}, {});     // keeps the flood feasible
  net.add_transition(std::nullopt, {source}, {});  // balances the source
  net.add_transition(std::nullopt, {source}, {sink});  // reaches the sink
  net.add_transition(std::nullopt, {gate}, {gate, gate});  // fools the equation
  net.add_transition(0, {gate}, {sink});  // "x": never actually enabled

  const auto log = log_from_variants({{"x"}});
  minera::AlignmentOptions options;
  options.variant = minera::AlignmentVariant::kStateEquationAStar;
  options.max_states = 1000;
  const auto alignment = minera::check_alignment(log, net, options);
  assert(!alignment.computed);
  assert(alignment.exhausted);  // search bound hit, distinct from an unsound net
  assert(alignment.variants.empty());
  assert(alignment.fitting_traces == 0);
  assert(alignment.total_cost == 0);
}

void alignment_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto tree = minera::discover_process_tree(empty);
  const auto alignment = minera::check_alignment(empty, tree);
  assert(alignment.trace_count == 0);
  assert(alignment.fitting_traces == 0);
  assert(alignment.total_cost == 0);
  assert(alignment.fitness > 0.9999);
}

void alignment_treats_an_empty_log_as_perfect_for_every_model() {
  // With no traces there is nothing to align, so the result must be trivially
  // perfect for every miner — never reported as unsound just because an empty
  // net's sink is unreachable.
  const minera::EventLog empty;
  const auto alpha = minera::discover_petri_net(empty);
  const auto heuristic = minera::to_petri_net(minera::discover_heuristics_net(empty));
  for (const auto* net : {&alpha, &heuristic}) {
    const auto alignment = minera::check_alignment(empty, *net);
    assert(alignment.computed);
    assert(alignment.trace_count == 0);
    assert(alignment.total_cost == 0);
    assert(alignment.fitness > 0.9999);
  }
}

void replay_degrades_gracefully_on_an_unbounded_silent_net() {
  // Token replay routes silent (tau) transitions to enable each visible step,
  // searching the silent reachability space. This net makes that space infinite:
  // a silent transition consumes the source and produces the source back plus a
  // token on an auxiliary place, so it can fire forever. The visible activity
  // "a" needs a token on a `gate` place that nothing ever produces, so the
  // silent search can never reach it and must lean on its bound to stop. Replay
  // must return a finite, deviating result (the forced firing creates missing
  // tokens) rather than spinning forever — this test would hang without the cap.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto aux = net.add_place();
  const auto gate = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {source, aux});  // unbounded silent
  net.add_transition(0, {gate}, {sink});  // visible "a", gate never produced

  const auto log = log_from_variants({{"a"}});
  const auto replay = minera::check_conformance(log, net);
  assert(replay.trace_count == 1);
  assert(replay.missing > 0);       // the unreachable gate forces a token
  assert(replay.fitness < 1.0);     // so the trace does not fit
  assert(replay.fitting_traces == 0);
}

void heuristics_net_becomes_a_replayable_workflow_net() {
  // The Heuristic Miner emits a dependency graph; converting it to a workflow
  // net must yield a replayable model so both conformance methods work. A
  // strongly-sequential log (every dependency clears the 0.9 threshold)
  // converts to a clean chain — one place per edge, no silent routing at all —
  // that the log fits perfectly.
  const std::vector<std::vector<std::string>> sequence(12,
                                                       {"a", "b", "c", "d"});
  const auto log = log_from_variants(sequence);
  const auto heuristics = minera::discover_heuristics_net(log);
  const auto net = minera::to_petri_net(heuristics);

  // Source and sink plus a place per dependency edge.
  assert(net.place_count() == 2 + heuristics.edges.size());
  for (const auto& transition : net.transitions()) {
    assert(!transition.silent());
  }

  const auto replay = minera::check_conformance(log, net);
  assert(replay.fitness > 0.999);

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness > 0.999);
}

void heuristics_net_stays_replayable_when_it_drops_edges() {
  // With a high threshold the Heuristic Miner drops weak dependencies, so the
  // log no longer fits perfectly — but the converted net must still replay and
  // align (a finite cost rather than a refusal).
  const auto log = log_from_variants(
      {{"a", "b", "d"}, {"a", "c", "d"}, {"a", "b", "d"}});
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness >= 0.0 && alignment.fitness <= 1.0);
}

void heuristics_net_translates_an_and_split_and_join() {
  // b and c are concurrent between a and d, and the AND measure sees it: b and
  // c follow each other constantly relative to a's split. The translation must
  // wire a real AND split/join — the former one-token state machine could
  // replay only one branch, so every trace here counted as deviating.
  std::vector<std::vector<std::string>> variants(6, {"a", "b", "c", "d"});
  for (int i = 0; i < 6; ++i) variants.push_back({"a", "c", "b", "d"});
  const auto log = log_from_variants(variants);
  const auto heuristics = minera::discover_heuristics_net(log);
  const auto net = minera::to_petri_net(heuristics);

  // a fires both branch places at once; d waits for both.
  const auto* a = transition_for(net, "a");
  const auto* d = transition_for(net, "d");
  assert(a && d);
  assert(a->outputs.size() == 2);
  assert(d->inputs.size() == 2);

  assert(minera::check_conformance(log, net).fitness > 0.999);
  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness > 0.999);
}

void heuristics_net_keeps_exclusive_branches_exclusive() {
  // b and c never follow each other, so the AND measure keeps them in separate
  // bundles: after a exactly one of them may run. Both single-branch traces
  // fit; a trace running b *and* c must deviate.
  std::vector<std::vector<std::string>> variants(6, {"a", "b", "d"});
  for (int i = 0; i < 6; ++i) variants.push_back({"a", "c", "d"});
  const auto log = log_from_variants(variants);
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  assert(minera::check_conformance(log, net).fitness > 0.999);
  const auto both = log_from_variants({{"a", "b", "c", "d"}});
  assert(minera::check_conformance(both, net).fitness < 1.0);
}

void heuristics_net_handles_a_self_loop() {
  // A self-loop dependency (a after a) must route done(a) back to ready(a) so
  // the converted net can replay the repetition rather than deadlock.
  const std::vector<std::vector<std::string>> looping(
      12, {"a", "a", "a", "b"});
  const auto log = log_from_variants(looping);
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness > 0.999);
}

void importer_handles_a_header_only_log() {
  const auto path = write_temp_log("case_id,activity,timestamp\n");
  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 0);
  assert(log.event_count() == 0);

  // The downstream passes must tolerate an empty log without crashing.
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().empty());
  assert(graph.edges().empty());
}

void importer_strips_a_utf8_bom() {
  const auto path = write_temp_log(
      "\xEF\xBB\xBF"
      "case_id,activity\n"
      "1,A\n");
  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.traces.front().events.front().activity == "A");
}

void dfg_handles_single_event_traces() {
  const auto path = write_temp_log(
      "case_id,activity\n"
      "1,A\n"
      "2,A\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().size() == 1);
  assert(graph.edges().empty());  // no directly-follows pair within length-1 traces
  assert(graph.start_activity_count("A") == 2);
  assert(graph.end_activity_count("A") == 2);
}

void filter_keeps_frequent_variants() {
  const auto log = log_from_variants(
      {{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}, {"a", "d"}});

  minera::LogFilterOptions options;
  options.min_variant_frequency = 2;
  const auto filtered = minera::filter_event_log(log, options);

  assert(filtered.trace_count() == 3);
  for (const auto& trace : filtered.traces) {
    assert((activity_names(trace) == std::vector<std::string>{"a", "b"}));
  }
}

void filter_keeps_only_the_top_variant() {
  const auto log = log_from_variants(
      {{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}, {"a", "d"}});

  minera::LogFilterOptions options;
  options.top_variants = 1;
  const auto filtered = minera::filter_event_log(log, options);

  assert(filtered.trace_count() == 3);
  assert((activity_names(filtered.traces.front()) ==
          std::vector<std::string>{"a", "b"}));
}

void filter_keeps_variants_covering_a_share_of_cases() {
  // Variant counts 5 / 3 / 2 over 10 cases: half the cases are covered by the
  // top variant alone, 80% needs the second, and full coverage needs all.
  std::vector<std::vector<std::string>> variants;
  for (int i = 0; i < 5; ++i) variants.push_back({"a", "b"});
  for (int i = 0; i < 3; ++i) variants.push_back({"a", "c"});
  for (int i = 0; i < 2; ++i) variants.push_back({"a", "d"});
  const auto log = log_from_variants(variants);

  minera::LogFilterOptions options;
  options.variant_coverage = 0.5;
  assert(options.any());
  assert(minera::filter_event_log(log, options).trace_count() == 5);

  options.variant_coverage = 0.8;
  assert(minera::filter_event_log(log, options).trace_count() == 8);

  options.variant_coverage = 1.0;
  assert(minera::filter_event_log(log, options).trace_count() == 10);

  // Combined with a rank cap, a variant must satisfy both constraints.
  options.variant_coverage = 0.8;
  options.top_variants = 1;
  assert(minera::filter_event_log(log, options).trace_count() == 5);

  options.top_variants = 0;
  options.variant_coverage = 1.5;
  assert(throws([&] { (void)minera::filter_event_log(log, options); }));
}

void filter_supports_variant_percentage_and_endpoint_polarity() {
  const auto log = log_from_variants({
      {"A", "X"}, {"A", "X"}, {"A", "X"}, {"B", "X"},
      {"B", "X"}, {"C", "X"},
  });

  assert(minera::filter_variants_top_k(log, 0).trace_count() == 0);
  assert(minera::filter_variants_top_k(log, 1).trace_count() == 3);
  assert(minera::filter_variants_by_coverage_percentage(log, 0.3)
             .trace_count() == 5);
  assert(minera::filter_variants_by_maximum_coverage_percentage(log, 0.2)
             .trace_count() == 1);

  // A zero target still keeps the top variant, while variants tied at the
  // coverage boundary are retained together.
  assert(minera::filter_log_variants_percentage(log, 0.0).trace_count() == 3);
  const auto tied = log_from_variants({{"A"}, {"A"}, {"B"}, {"B"}, {"C"}});
  assert(minera::filter_log_variants_percentage(tied, 0.4).trace_count() == 4);

  const auto endpoints = log_from_variants({
      {"A", "X"}, {"B", "X"}, {"A", "Y"}, {"B", "Y"},
  });
  assert(minera::filter_start_activities(endpoints, {"A"}).trace_count() == 2);
  assert(minera::filter_start_activities(endpoints, {"A"}, false)
             .trace_count() == 2);
  assert(minera::filter_end_activities(endpoints, {"X"}, false)
             .trace_count() == 2);

  minera::LogFilterOptions combined;
  combined.start_activities = {"A"};
  combined.start_activities_positive = false;
  combined.end_activities = {"Y"};
  combined.end_activities_positive = false;
  combined.variant_percentage = 0.0;
  assert(combined.any());
  assert(minera::filter_event_log(endpoints, combined).trace_count() == 1);

  assert(throws([&] {
    (void)minera::filter_start_activities(endpoints, {}, false);
  }));
  assert(throws([&] {
    minera::LogFilterOptions invalid;
    invalid.variant_min_coverage = 0.7;
    invalid.variant_max_coverage = 0.6;
    (void)minera::filter_event_log(log, invalid);
  }));
  assert(throws([&] {
    minera::LogFilterOptions invalid;
    invalid.start_activities_positive = false;
    (void)minera::filter_event_log(log, invalid);
  }));
}

void filter_selects_cases_by_succession() {
  const auto log = log_from_variants(
      {{"a", "b", "c"}, {"a", "c", "b"}, {"a", "b", "b", "c"}});

  // Directly-follows: "a -> b" holds where b is the very next event.
  minera::LogFilterOptions keep_df;
  keep_df.keep_directly_follows = {{"a", "b"}};
  assert(keep_df.any());
  assert(minera::filter_event_log(log, keep_df).trace_count() == 2);

  minera::LogFilterOptions remove_df;
  remove_df.remove_directly_follows = {{"a", "c"}};
  assert(minera::filter_event_log(log, remove_df).trace_count() == 2);

  // Eventually-follows: "a -> b" also holds in {a, c, b}.
  minera::LogFilterOptions keep_ef;
  keep_ef.keep_eventually_follows = {{"a", "b"}};
  assert(minera::filter_event_log(log, keep_ef).trace_count() == 3);

  // A self-pair needs a second occurrence — an event never follows itself.
  minera::LogFilterOptions repeat;
  repeat.keep_eventually_follows = {{"b", "b"}};
  assert(minera::filter_event_log(log, repeat).trace_count() == 1);

  // Several pairs are alternatives: any listed succession keeps the case.
  minera::LogFilterOptions either;
  either.keep_directly_follows = {{"c", "b"}, {"b", "b"}};
  assert(minera::filter_event_log(log, either).trace_count() == 2);
}

void filter_matches_trace_patterns() {
  const auto log = log_from_variants({{"a", "b", "c"},
                                      {"x", "a", "b", "y"},
                                      {"a", "c"},
                                      {"b", "a"}});

  const auto parsed = minera::parse_trace_patterns(
      "a, b; ...,a,b,...; x,...,y");
  assert(parsed.size() == 3);
  assert(parsed[0] == minera::TracePattern({"a", "b"}));
  assert(parsed[1] == minera::TracePattern({"...", "a", "b", "..."}));

  minera::TracePatternFilterOptions exact;
  exact.patterns = {parsed[0]};
  const auto exact_log = minera::filter_trace_patterns(log, exact);
  assert(exact_log.trace_count() == 0);

  minera::TracePatternFilterOptions infix;
  infix.patterns = {parsed[1]};
  const auto infix_log = minera::filter_trace_patterns(log, infix);
  assert(infix_log.trace_count() == 2);
  assert(infix_log.traces[0].case_id == "0");
  assert(infix_log.traces[1].case_id == "1");

  minera::TracePatternFilterOptions negative;
  negative.patterns = {minera::TracePattern{"...", "a", "b", "..."}};
  negative.positive = false;
  assert(minera::filter_trace_patterns(log, negative).trace_count() == 2);

  minera::LogFilterOptions integrated;
  integrated.trace_patterns = {minera::TracePattern{"a", "b", "..."}};
  assert(minera::filter_event_log(log, integrated).trace_count() == 1);
  integrated.trace_pattern_positive = false;
  assert(minera::filter_event_log(log, integrated).trace_count() == 3);

  assert(throws([] {
    (void)minera::parse_trace_patterns("a,,b");
  }));
  assert(throws([] {
    minera::TracePatternFilterOptions invalid;
    invalid.patterns = {minera::TracePattern{}};
    (void)minera::filter_trace_patterns(log_from_variants({{"a"}}), invalid);
  }));
}

void filter_matches_exact_admitted_variants() {
  const auto log = log_from_variants({{"a", "b"}, {"a"}, {}, {"b"}});
  const auto parsed = minera::parse_admitted_variants("a,b; a,b; []");
  assert(parsed.size() == 3);
  assert(parsed[0] == minera::TracePattern({"a", "b"}));
  assert(parsed[1] == parsed[0]);
  assert(parsed[2].empty());

  minera::AdmittedVariantFilterOptions positive;
  positive.variants = parsed;
  const auto kept = minera::filter_admitted_variants(log, positive);
  assert(kept.trace_count() == 2);
  assert(kept.traces[0].case_id == "0");
  assert(kept.traces[1].case_id == "2");

  positive.positive = false;
  const auto removed = minera::filter_admitted_variants(log, positive);
  assert(removed.trace_count() == 2);
  assert(removed.traces[0].case_id == "1");
  assert(removed.traces[1].case_id == "3");

  minera::LogFilterOptions integrated;
  integrated.admitted_variants = {minera::TracePattern{}};
  const auto empty_only = minera::filter_event_log(log, integrated);
  assert(empty_only.trace_count() == 1);
  assert(empty_only.traces[0].case_id == "2");

  const auto literal = log_from_variants({{"..."}});
  minera::AdmittedVariantFilterOptions literal_filter;
  literal_filter.variants = minera::parse_admitted_variants("...");
  assert(minera::filter_admitted_variants(literal, literal_filter).trace_count() == 1);
  assert(throws([] { (void)minera::parse_admitted_variants("a,,b"); }));
  assert(throws([] { (void)minera::parse_admitted_variants("[];"); }));
}

void filter_cuts_prefixes_and_suffixes() {
  const auto log = log_from_variants({{"a", "b", "c", "d"}, {"a", "d"}});

  // Prefix: the head up to and including the first anchor; a case without
  // the anchor is dropped.
  minera::LogFilterOptions prefix;
  prefix.prefix_activity = "b";
  assert(prefix.any());
  const auto heads = minera::filter_event_log(log, prefix);
  assert(heads.trace_count() == 1);
  assert((activity_names(heads.traces.front()) ==
          std::vector<std::string>{"a", "b"}));

  // Suffix: the tail from the first anchor on.
  minera::LogFilterOptions suffix;
  suffix.suffix_activity = "c";
  const auto tails = minera::filter_event_log(log, suffix);
  assert(tails.trace_count() == 1);
  assert((activity_names(tails.traces.front()) ==
          std::vector<std::string>{"c", "d"}));

  // Both: the prefix cut applies first, the suffix is sought inside it.
  minera::LogFilterOptions both;
  both.prefix_activity = "c";
  both.suffix_activity = "b";
  const auto middles = minera::filter_event_log(log, both);
  assert(middles.trace_count() == 1);
  assert((activity_names(middles.traces.front()) ==
          std::vector<std::string>{"b", "c"}));
}

void parse_activity_pairs_reads_the_arrow_syntax() {
  const auto pairs =
      minera::parse_activity_pairs(" check ticket ->  decide ,a->b, ");
  assert(pairs.size() == 2);
  assert(pairs[0].predecessor == "check ticket");
  assert(pairs[0].successor == "decide");
  assert(pairs[1].predecessor == "a");
  assert(pairs[1].successor == "b");
  assert(minera::parse_activity_pairs("").empty());

  assert(throws([] { (void)minera::parse_activity_pairs("a, b"); }));
  assert(throws([] { (void)minera::parse_activity_pairs("a ->"); }));
  assert(throws([] { (void)minera::parse_activity_pairs("-> b"); }));
}

void filter_projects_activities() {
  const auto log = log_from_variants({{"a", "b", "c"}});

  minera::LogFilterOptions remove;
  remove.remove_activities = {"b"};
  const auto without_b = minera::filter_event_log(log, remove);
  assert((activity_names(without_b.traces.front()) ==
          std::vector<std::string>{"a", "c"}));

  minera::LogFilterOptions keep;
  keep.keep_activities = {"a", "c"};
  const auto only_ac = minera::filter_event_log(log, keep);
  assert((activity_names(only_ac.traces.front()) ==
          std::vector<std::string>{"a", "c"}));
}

void filter_projects_by_resource() {
  // Two events per case, one by Alice and one by Bob; an unattributed event
  // carries no resource at all.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "1";
  trace.events.push_back(minera::Event{"1", "a", "", "Alice", 0});
  trace.events.push_back(minera::Event{"1", "b", "", "Bob", 1});
  trace.events.push_back(minera::Event{"1", "c", "", "", 2});
  log.traces.push_back(trace);

  minera::LogFilterOptions keep;
  keep.keep_resources = {"Alice"};
  assert(keep.any());
  const auto only_alice = minera::filter_event_log(log, keep);
  assert(only_alice.trace_count() == 1);
  // Only Alice's event survives; Bob's and the unattributed event are dropped.
  assert((activity_names(only_alice.traces.front()) ==
          std::vector<std::string>{"a"}));

  minera::LogFilterOptions remove;
  remove.remove_resources = {"Bob"};
  const auto without_bob = minera::filter_event_log(log, remove);
  // Bob's event goes; the unattributed event is never dropped by a resource
  // remove-set.
  assert((activity_names(without_bob.traces.front()) ==
          std::vector<std::string>{"a", "c"}));
}

void filter_applies_length_and_endpoint_constraints() {
  const auto log = log_from_variants({{"a"}, {"a", "b", "c"}});
  minera::LogFilterOptions length;
  length.min_length = 2;
  const auto longer = minera::filter_event_log(log, length);
  assert(longer.trace_count() == 1);
  assert(longer.traces.front().events.size() == 3);

  const auto ends_log = log_from_variants({{"a", "b", "x"}, {"a", "b", "y"}});
  minera::LogFilterOptions ends;
  ends.end_activities = {"x"};
  const auto ending_x = minera::filter_event_log(ends_log, ends);
  assert(ending_x.trace_count() == 1);
  assert(ending_x.traces.front().events.back().activity == "x");
}

void filter_rework_keeps_cases_with_a_repeated_activity() {
  const auto log = log_from_variants(
      {{"a", "b", "b", "c"}, {"a", "b", "c"}, {"a", "b", "b", "b", "c"}});

  minera::LogFilterOptions rework;
  rework.rework_activity = "b";  // default minimum of 2 = any repetition
  assert(rework.any());
  const auto repeated = minera::filter_event_log(log, rework);
  assert(repeated.trace_count() == 2);

  rework.rework_min_occurrences = 3;
  const auto thrice = minera::filter_event_log(log, rework);
  assert(thrice.trace_count() == 1);
  assert(thrice.traces.front().events.size() == 5);

  // An activity the log never repeats keeps nothing.
  rework.rework_activity = "a";
  rework.rework_min_occurrences = 2;
  assert(minera::filter_event_log(log, rework).trace_count() == 0);
}

void filter_between_extracts_subtraces() {
  // One case passes a->b twice, so it yields two subcases; a case that never
  // closes a segment yields none and vanishes.
  const auto log = log_from_variants(
      {{"x", "a", "m", "b", "y", "a", "n", "b"}, {"x", "a", "m"}});

  minera::LogFilterOptions between;
  between.between_start = "a";
  between.between_end = "b";
  assert(between.any());
  const auto cut = minera::filter_event_log(log, between);
  assert(cut.trace_count() == 2);
  assert((activity_names(cut.traces[0]) ==
          std::vector<std::string>{"a", "m", "b"}));
  assert((activity_names(cut.traces[1]) ==
          std::vector<std::string>{"a", "n", "b"}));
  // Several segments from one case become distinct cases.
  assert(cut.traces[0].case_id != cut.traces[1].case_id);

  // A single segment keeps its original case id.
  const auto single = log_from_variants({{"x", "a", "m", "b", "y"}});
  const auto one = minera::filter_event_log(single, between);
  assert(one.trace_count() == 1);
  assert(one.traces.front().case_id == single.traces.front().case_id);
  assert((activity_names(one.traces.front()) ==
          std::vector<std::string>{"a", "m", "b"}));
}

void filter_between_handles_equal_endpoints_and_rejects_one_sided() {
  // start == end: each segment runs from one occurrence to the next.
  const auto log = log_from_variants({{"a", "x", "a", "y", "a"}});
  minera::LogFilterOptions between;
  between.between_start = "a";
  between.between_end = "a";
  const auto cut = minera::filter_event_log(log, between);
  assert(cut.trace_count() == 2);
  assert((activity_names(cut.traces[0]) ==
          std::vector<std::string>{"a", "x", "a"}));
  assert((activity_names(cut.traces[1]) ==
          std::vector<std::string>{"a", "y", "a"}));

  assert(throws([&] {
    minera::LogFilterOptions one_sided;
    one_sided.between_start = "a";
    (void)minera::filter_event_log(log_from_variants({{"a"}}), one_sided);
  }));
}

// A log for the time-range filter: one January case, one case straddling the
// January/February boundary, one February case, and one untimed case.
minera::EventLog timed_filter_log() {
  minera::EventLog log;
  const std::vector<std::vector<std::pair<std::string, std::string>>> cases = {
      {{"a", "2024-01-01"}, {"b", "2024-01-02"}},
      {{"a", "2024-01-30"}, {"b", "2024-02-02"}},
      {{"a", "2024-02-05"}, {"b", "2024-02-06"}},
      {{"a", ""}, {"b", ""}},
  };
  std::size_t case_index = 0;
  for (const auto& events : cases) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& [activity, timestamp] : events) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.timestamp = timestamp;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void filter_time_range_trims_events() {
  const auto log = timed_filter_log();
  minera::LogFilterOptions options;
  options.time_from = "2024-01-01";
  options.time_to = "2024-01-31";
  assert(options.any());

  // events mode keeps only the January events; the straddling case shrinks to
  // its January prefix, and the February and untimed cases vanish entirely.
  const auto filtered = minera::filter_event_log(log, options);
  assert(filtered.trace_count() == 2);
  assert((activity_names(filtered.traces[0]) ==
          std::vector<std::string>{"a", "b"}));
  assert((activity_names(filtered.traces[1]) == std::vector<std::string>{"a"}));
}

void filter_time_range_selects_whole_traces() {
  const auto log = timed_filter_log();
  minera::LogFilterOptions options;
  options.time_from = "2024-01-01";
  options.time_to = "2024-01-31";

  // Contained: only the case that lies fully inside January survives, whole.
  options.time_mode = minera::TimeFilterMode::traces_contained;
  const auto contained = minera::filter_event_log(log, options);
  assert(contained.trace_count() == 1);
  assert(contained.traces.front().case_id == "0");
  assert(contained.traces.front().events.size() == 2);

  // Intersecting: the straddling case survives too, keeping its February tail.
  options.time_mode = minera::TimeFilterMode::traces_intersecting;
  const auto intersecting = minera::filter_event_log(log, options);
  assert(intersecting.trace_count() == 2);
  assert(intersecting.traces[1].case_id == "1");
  assert(intersecting.traces[1].events.size() == 2);
}

void filter_time_range_bounds_are_inclusive_and_one_sided() {
  const auto log = timed_filter_log();

  minera::LogFilterOptions at_instant;
  at_instant.time_from = "2024-01-02";
  at_instant.time_to = "2024-01-02";
  const auto exact = minera::filter_event_log(log, at_instant);
  assert(exact.trace_count() == 1);
  assert((activity_names(exact.traces.front()) ==
          std::vector<std::string>{"b"}));

  minera::LogFilterOptions from_only;
  from_only.time_from = "2024-02-01";
  const auto tail = minera::filter_event_log(log, from_only);
  assert(tail.trace_count() == 2);  // the straddler's tail and the February case
}

void filter_time_range_rejects_a_malformed_bound() {
  assert(throws([] {
    minera::LogFilterOptions options;
    options.time_from = "not-a-date";
    (void)minera::filter_event_log(timed_filter_log(), options);
  }));
  assert((minera::parse_time_filter_mode("traces_contained") ==
          minera::TimeFilterMode::traces_contained));
  assert(!minera::parse_time_filter_mode("bogus"));
}

void filter_case_performance_uses_inclusive_trace_duration() {
  const auto log = timed_filter_log();
  minera::CasePerformanceFilterOptions exact_day;
  exact_day.min_duration_seconds = 86400.0;
  exact_day.max_duration_seconds = 86400.0;
  const auto standalone = minera::filter_case_performance(log, exact_day);
  assert(standalone.trace_count() == 2);
  assert(standalone.traces[0].case_id == "0");
  assert(standalone.traces[1].case_id == "2");

  minera::LogFilterOptions integrated;
  integrated.min_case_duration_seconds = 86400.0;
  integrated.max_case_duration_seconds = 172800.0;
  const auto filtered = minera::filter_event_log(log, integrated);
  assert(filtered.trace_count() == 2);

  minera::CasePerformanceFilterOptions maximum_only;
  maximum_only.max_duration_seconds = 86400.0;
  assert(minera::filter_case_performance(log, maximum_only).trace_count() == 2);

  assert(throws([&] {
    minera::CasePerformanceFilterOptions invalid;
    invalid.max_duration_seconds = -1.0;
    (void)minera::filter_case_performance(log, invalid);
  }));
}

void filter_case_selection_limits_cases_and_event_counts() {
  const auto log = log_from_variants({{"a"}, {"a", "b"}, {"a", "b", "c"}});

  const auto default_size = minera::filter_on_case_size(log);
  assert(default_size.trace_count() == 2);
  assert(default_size.traces.front().case_id == "1");

  const auto bounded_size = minera::filter_case_size(log, 2, 2);
  assert(bounded_size.trace_count() == 1);
  assert(bounded_size.traces.front().case_id == "1");

  assert(minera::filter_on_ncases(log, 1).trace_count() == 1);
  assert(minera::filter_on_ncases(log, 0).trace_count() == 0);

  minera::LogFilterOptions integrated;
  integrated.min_length = 2;
  integrated.max_length = 3;
  integrated.max_cases = 1;
  assert(integrated.any());
  const auto filtered = minera::filter_event_log(log, integrated);
  assert(filtered.trace_count() == 1);
  assert(filtered.traces.front().case_id == "1");

  assert(throws([&] { (void)minera::filter_case_size(log, 4, 3); }));
  assert(throws([&] {
    minera::LogFilterOptions invalid;
    invalid.min_length = 4;
    invalid.max_length = 3;
    (void)minera::filter_event_log(log, invalid);
  }));
}

void filter_numeric_attributes_support_event_and_trace_ranges() {
  minera::EventLog log;
  auto add = [&](std::string id, std::string value) {
    minera::Trace trace;
    trace.case_id = id;
    minera::Event event{id, "A", "", "", 0, {}, ""};
    if (!value.empty()) event.attributes.push_back({"score", value, {}});
    trace.events.push_back(std::move(event));
    log.traces.push_back(std::move(trace));
  };
  add("in", "3");
  add("low", "1");
  add("bad", "not-a-number");
  add("missing", "");

  minera::NumericAttributeFilterOptions range;
  range.attribute_key = "score";
  range.min_value = 2.0;
  range.max_value = 4.0;
  const auto events = minera::filter_numeric_attribute(log, range);
  assert(events.trace_count() == 1);
  assert(events.traces.front().case_id == "in");

  range.positive = false;
  const auto outside_events = minera::filter_numeric_attribute(log, range);
  assert(outside_events.trace_count() == 1);
  assert(outside_events.traces.front().case_id == "low");

  range.mode = minera::AttributeFilterMode::traces;
  range.positive = true;
  const auto matching_cases = minera::filter_numeric_attribute(log, range);
  assert(matching_cases.trace_count() == 1);
  assert(matching_cases.traces.front().case_id == "in");

  range.positive = false;
  const auto nonmatching_cases = minera::filter_numeric_attribute(log, range);
  assert(nonmatching_cases.trace_count() == 3);
  assert(nonmatching_cases.traces[0].case_id == "low");
  assert(nonmatching_cases.traces[1].case_id == "bad");
  assert(nonmatching_cases.traces[2].case_id == "missing");

  minera::LogFilterOptions integrated;
  integrated.numeric_attribute = range;
  integrated.numeric_attribute->positive = true;
  integrated.numeric_attribute->mode = minera::AttributeFilterMode::events;
  assert(integrated.any());
  assert(minera::filter_event_log(log, integrated).trace_count() == 1);

  assert(throws([&] {
    minera::NumericAttributeFilterOptions invalid;
    invalid.min_value = 2.0;
    (void)minera::filter_numeric_attribute(log, invalid);
  }));
  assert(throws([&] {
    auto invalid = range;
    invalid.min_value = 5.0;
    invalid.max_value = 4.0;
    (void)minera::filter_numeric_attribute(log, invalid);
  }));
}

void filter_frequency_attribute_variants_support_top_and_relative_occurrence() {
  const auto log = log_from_variants({
      {"A", "A", "B"},
      {"A", "C"},
      {"B", "C"},
  });

  minera::TopActivitiesFilterOptions top;
  top.max_activities = 2;
  const auto top_two = minera::filter_top_activities(log, top);
  assert(top_two.trace_count() == 3);
  assert((activity_names(top_two.traces[0]) ==
          std::vector<std::string>{"A", "A", "B"}));
  assert((activity_names(top_two.traces[1]) ==
          std::vector<std::string>{"A"}));
  assert((activity_names(top_two.traces[2]) ==
          std::vector<std::string>{"B"}));

  top.max_activities = 0;
  assert(minera::filter_log_on_max_no_activities(log, 0).trace_count() == 0);

  minera::RelativeAttributeOccurrenceFilterOptions relative;
  relative.min_relative_stake = 0.67;
  const auto case_occurrence =
      minera::filter_relative_attribute_occurrence(log, relative);
  assert(case_occurrence.trace_count() == 0);

  relative.min_relative_stake = 0.5;
  const auto half_cases =
      minera::filter_relative_attribute_occurrence(log, relative);
  assert(half_cases.trace_count() == 3);
  assert((activity_names(half_cases.traces[0]) ==
          std::vector<std::string>{"A", "A", "B"}));

  relative.keep_once_per_case = false;
  relative.min_relative_stake = 0.4;
  const auto event_occurrence =
      minera::filter_relative_attribute_occurrence(log, relative);
  assert(event_occurrence.trace_count() == 2);
  assert((activity_names(event_occurrence.traces[0]) ==
          std::vector<std::string>{"A", "A"}));
  assert((activity_names(event_occurrence.traces[1]) ==
          std::vector<std::string>{"A"}));

  minera::LogFilterOptions integrated;
  integrated.top_activities = minera::TopActivitiesFilterOptions{2, {}};
  integrated.relative_attribute_occurrence =
      minera::RelativeAttributeOccurrenceFilterOptions{"", 0.5, true};
  assert(integrated.any());
  const auto combined = minera::filter_event_log(log, integrated);
  assert(combined.trace_count() == 3);

  assert(throws([&] {
    minera::RelativeAttributeOccurrenceFilterOptions invalid;
    invalid.min_relative_stake = 1.1;
    (void)minera::filter_relative_attribute_occurrence(log, invalid);
  }));
}

void filter_absolute_attribute_occurrence_thresholds() {
  const auto log = log_from_variants({
      {"A", "B", "A"}, {"A", "C"}, {"B", "C"},
  });
  minera::AttributeOccurrenceThresholdFilterOptions threshold;
  threshold.min_occurrences = 2;
  const auto frequent =
      minera::filter_attribute_occurrence_threshold(log, threshold);
  assert(frequent.trace_count() == 3);
  assert((activity_names(frequent.traces[0]) ==
          std::vector<std::string>{"A", "B", "A"}));
  assert((activity_names(frequent.traces[1]) ==
          std::vector<std::string>{"A", "C"}));
  assert((activity_names(frequent.traces[2]) ==
          std::vector<std::string>{"B", "C"}));

  threshold.min_occurrences = 3;
  assert(minera::filter_log_by_attribute_occurrence_threshold(
             log, {}, 3)
             .trace_count() == 2);

  minera::LogFilterOptions integrated;
  integrated.attribute_occurrence_threshold =
      minera::AttributeOccurrenceThresholdFilterOptions{"", 3};
  assert(integrated.any());
  assert(minera::filter_event_log(log, integrated).trace_count() == 2);

  minera::AttributeOccurrenceThresholdFilterOptions generic;
  generic.attribute_key = "channel";
  generic.min_occurrences = 2;
  minera::EventLog attributed = log;
  attributed.traces[0].events[0].attributes.push_back(
      {"channel", "web", minera::AttributeKind::kString});
  attributed.traces[0].events[1].attributes.push_back(
      {"channel", "web", minera::AttributeKind::kString});
  attributed.traces[1].events[0].attributes.push_back(
      {"channel", "phone", minera::AttributeKind::kString});
  const auto generic_filtered =
      minera::filter_attribute_occurrence_threshold(attributed, generic);
  assert(generic_filtered.trace_count() == 1);
  assert(generic_filtered.traces[0].events.size() == 2);
  assert(throws([&] {
    (void)minera::filter_variants_variants_percentage(log, 1.5);
  }));
}

void filter_prefix_suffix_support_strictness_and_occurrence() {
  const auto log = log_from_variants({{"a", "b", "a", "c"}, {"x", "y"}});

  minera::PrefixSuffixFilterOptions prefix;
  prefix.activity = "a";
  auto first_strict = minera::filter_prefix(log, prefix);
  assert(first_strict.trace_count() == 1);
  assert((activity_names(first_strict.traces.front()) ==
          std::vector<std::string>{"a"}));

  prefix.occurrence = minera::FilterAnchorOccurrence::last;
  const auto last_strict = minera::filter_prefix(log, prefix);
  assert((activity_names(last_strict.traces.front()) ==
          std::vector<std::string>{"a", "b", "a"}));

  prefix.strict = false;
  const auto last_non_strict = minera::filter_prefix(log, prefix);
  assert((activity_names(last_non_strict.traces.front()) ==
          std::vector<std::string>{"a", "b"}));

  minera::PrefixSuffixFilterOptions suffix;
  suffix.activity = "a";
  const auto suffix_first = minera::filter_suffix(log, suffix);
  assert((activity_names(suffix_first.traces.front()) ==
          std::vector<std::string>{"a", "b", "a", "c"}));
  suffix.occurrence = minera::FilterAnchorOccurrence::last;
  const auto suffix_last = minera::filter_suffix(log, suffix);
  assert((activity_names(suffix_last.traces.front()) ==
          std::vector<std::string>{"a", "c"}));
  suffix.strict = false;
  const auto suffix_last_non_strict = minera::filter_suffix(log, suffix);
  assert((activity_names(suffix_last_non_strict.traces.front()) ==
          std::vector<std::string>{"c"}));

  minera::LogFilterOptions integrated;
  integrated.prefix_activity = "a";
  integrated.prefix_strict = false;
  integrated.prefix_occurrence = minera::FilterAnchorOccurrence::last;
  integrated.suffix_activity = "b";
  integrated.suffix_occurrence = minera::FilterAnchorOccurrence::first;
  const auto combined = minera::filter_event_log(log, integrated);
  assert(combined.trace_count() == 1);
  assert((activity_names(combined.traces.front()) ==
          std::vector<std::string>{"b"}));

  assert(throws([&] {
    minera::PrefixSuffixFilterOptions invalid;
    (void)minera::filter_prefix(log, invalid);
  }));
  assert(throws([&] {
    minera::LogFilterOptions invalid;
    invalid.prefix_strict = false;
    (void)minera::filter_event_log(log, invalid);
  }));
}

void importer_rejects_malformed_rows() {
  // Too few fields for the declared header.
  assert(throws([] {
    const auto path = write_temp_log("case_id,activity\n1\n");
    (void)minera::import_csv_event_log(path);
  }));
  // Empty activity value.
  assert(throws([] {
    const auto path = write_temp_log("case_id,activity\n1,\n");
    (void)minera::import_csv_event_log(path);
  }));
  // Unterminated quoted field.
  assert(throws([] {
    const auto path = write_temp_log("case_id,activity\n1,\"open\n");
    (void)minera::import_csv_event_log(path);
  }));
  // Missing required column.
  assert(throws([] {
    const auto path = write_temp_log("id,activity\n1,A\n");
    (void)minera::import_csv_event_log(path);
  }));
}

minera::EventLog import_xes(const std::string& xml) {
  std::istringstream input(xml);
  return minera::import_xes_event_log(input);
}

std::filesystem::path write_temp_xes(const std::string& content) {
  auto path = std::filesystem::temp_directory_path() / "minera_core_test.xes";
  std::ofstream output(path, std::ios::binary);
  output << content;
  return path;
}

void xes_importer_reads_a_file_from_disk() {
  // A UTF-8 BOM is stripped, and the file-path overload opens it in binary.
  const auto path = write_temp_xes(
      "\xEF\xBB\xBF"
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"case-1\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:00:00\"/>\n"
      "    </event>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"B\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:01:00\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  const auto log = minera::import_xes_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.event_count() == 2);
  assert(log.traces[0].case_id == "case-1");
  assert(log.traces[0].events[0].activity == "A");
  assert(log.traces[0].events[1].activity == "B");

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 1);

  // Opening a path that does not exist is an error, not an empty log.
  assert(throws([] {
    (void)minera::import_xes_event_log(
        std::filesystem::temp_directory_path() / "minera_no_such_file.xes");
  }));
}

void xes_importer_reads_traces_and_events() {
  const auto log = import_xes(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!-- a minimal XES log -->\n"
      "<log xes.version=\"1.0\">\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"case-1\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:00:00\"/>\n"
      "    </event>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"B\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:01:00\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"case-2\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  assert(log.trace_count() == 2);
  assert(log.event_count() == 3);
  assert(log.traces[0].case_id == "case-1");
  assert(log.traces[0].events.size() == 2);
  assert(log.traces[0].events[0].activity == "A");
  assert(log.traces[0].events[0].case_id == "case-1");
  assert(log.traces[0].events[1].activity == "B");
  assert(log.traces[1].case_id == "case-2");
  assert(log.traces[1].events[0].activity == "A");

  // The XES log mines just like a CSV log would.
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 1);
}

void xes_importer_variants_ordering_limits_and_projection() {
  const std::string xml =
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <string key=\"region\" value=\"west\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"B\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:02:00\"/>\n"
      "      <int key=\"cost\" value=\"2\"/>\n"
      "    </event>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:01:00\"/>\n"
      "      <string key=\"ignored\" value=\"x\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"d\"/>\n"
      "    <event><string key=\"concept:name\" value=\"C\"/>"
      "<date key=\"time:timestamp\" value=\"2025-12-31T23:59:00Z\"/></event>\n"
      "  </trace>\n"
      "</log>\n";

  // XML order is authoritative unless timestamp sorting is explicitly enabled.
  const auto document_order = import_xes(xml);
  assert(document_order.traces.size() == 2);
  assert(document_order.traces[0].events[0].activity == "B");
  assert(document_order.traces[0].events[1].activity == "A");

  minera::XesImportOptions sorted_options;
  sorted_options.timestamp_sort = true;
  std::istringstream sorted_input(xml);
  const auto sorted = minera::import_xes_event_log(sorted_input, sorted_options);
  assert(sorted.traces[0].case_id == "d");
  assert(sorted.traces[1].case_id == "c");
  assert(sorted.traces[1].events[0].activity == "A");
  assert(sorted.traces[1].events[1].activity == "B");

  sorted_options.reverse_sort = true;
  std::istringstream reversed_input(xml);
  const auto reversed =
      minera::import_xes_event_log(reversed_input, sorted_options);
  assert(reversed.traces[0].case_id == "c");
  assert(reversed.traces[0].events[0].activity == "B");

  minera::XesImportOptions projected_options;
  projected_options.max_traces = 1;
  projected_options.attributes_to_read = {"cost"};
  std::istringstream projected_input(xml);
  const auto projected =
      minera::import_xes_event_log(projected_input, projected_options);
  assert(projected.trace_count() == 1);
  assert(projected.traces[0].attribute("region") == nullptr);
  assert(*projected.traces[0].events[0].attribute("cost") == "2");
  assert(projected.traces[0].events[1].attribute("ignored") == nullptr);

  const std::vector<minera::XesImportVariant> variants = {
      minera::XesImportVariant::ITERPARSE,
      minera::XesImportVariant::LINE_BY_LINE,
      minera::XesImportVariant::ITERPARSE_MEM_COMPRESSED,
      minera::XesImportVariant::ITERPARSE_20,
      minera::XesImportVariant::CHUNK_REGEX,
      minera::XesImportVariant::RUSTXES};
  for (const auto variant : variants) {
    minera::XesImportOptions variant_options;
    variant_options.variant = variant;
    std::istringstream variant_input(xml);
    assert(minera::import_xes_event_log(variant_input, variant_options)
               .trace_count() == 2);
    assert(minera::parse_xes_import_variant(
               minera::xes_import_variant_name(variant)) == variant);
  }
  assert(minera::parse_xes_import_variant("lxml") ==
         minera::XesImportVariant::kIterparse);
  assert(minera::parse_xes_import_variant("nonstandard") ==
         minera::XesImportVariant::kLineByLine);
  assert(minera::parse_xes_import_variant("r4pm") ==
         minera::XesImportVariant::kRustXes);
  assert(throws([] { (void)minera::parse_xes_import_variant("unknown"); }));
}

void xes_importer_decodes_entities_and_ignores_nesting() {
  // Nested sub-attributes must not be mistaken for case/activity attributes,
  // and XML entities in values are decoded.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"Tom &amp; Jerry\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"Pay &lt;50&gt;\">\n"
      "        <string key=\"concept:name\" value=\"NESTED\"/>\n"
      "      </string>\n"
      "      <string key=\"org:resource\" value=\"clerk\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  assert(log.trace_count() == 1);
  assert(log.traces[0].case_id == "Tom & Jerry");
  assert(log.traces[0].events.size() == 1);
  assert(log.traces[0].events[0].activity == "Pay <50>");
}

void csv_importer_captures_extra_columns_as_attributes() {
  // Columns beyond case/activity/timestamp/resource become generic event
  // attributes keyed by their header; blank cells mean "no attribute".
  const auto path = write_temp_log(
      "case_id,activity,timestamp,resource,channel,amount\n"
      "1,A,2026-01-01T00:00:00,alice,web,100\n"
      "1,B,2026-01-01T00:01:00,bob,,250\n");

  const auto log = minera::import_csv_event_log(path);
  const auto& events = log.traces.front().events;
  assert(events[0].attributes.size() == 2);
  assert(events[0].attributes[0].key == "channel");
  assert(events[0].attributes[0].value == "web");
  assert(events[0].attributes[0].kind == minera::AttributeKind::kString);
  assert(*events[0].attribute("amount") == "100");
  // The blank channel cell leaves only the amount on the second event, and
  // the first-class fields never double as generic attributes.
  assert(events[1].attributes.size() == 1);
  assert(events[1].attribute("channel") == nullptr);
  assert(*events[1].attribute("amount") == "250");
  assert(events[1].attribute("resource") == nullptr);
  assert(events[1].resource == "bob");
}

void xes_importer_captures_generic_event_attributes() {
  // Scalar event attributes beyond the standard activity/timestamp/resource
  // keys are kept with their XES type; nested sub-attributes and trace-level
  // attributes are not event attributes.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <string key=\"customer\" value=\"trace-level\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <string key=\"org:resource\" value=\"clerk\"/>\n"
      "      <string key=\"channel\" value=\"web\">\n"
      "        <string key=\"nested\" value=\"ignored\"/>\n"
      "      </string>\n"
      "      <int key=\"amount\" value=\"100\"/>\n"
      "      <boolean key=\"approved\" value=\"true\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  const auto& event = log.traces.front().events.front();
  assert(event.activity == "A");
  assert(event.resource == "clerk");
  assert(event.attributes.size() == 3);
  assert(event.attributes[0].key == "channel");
  assert(event.attributes[0].kind == minera::AttributeKind::kString);
  assert(event.attributes[1].key == "amount");
  assert(event.attributes[1].value == "100");
  assert(event.attributes[1].kind == minera::AttributeKind::kInt);
  assert(event.attributes[2].key == "approved");
  assert(event.attributes[2].kind == minera::AttributeKind::kBoolean);
  assert(event.attribute("nested") == nullptr);
  assert(event.attribute("customer") == nullptr);
}

void xes_export_round_trips_generic_attributes() {
  // Attributes survive an export/import cycle with their XES types, so an
  // <int> stays an <int>; explicitly typed invalid dates are rejected.
  minera::EventLog log;
  log.attributes.push_back(
      {"@@aligned_variant", "a,b", minera::AttributeKind::kString});
  minera::Trace trace;
  trace.case_id = "c1";
  minera::Event event;
  event.case_id = "c1";
  event.activity = "a";
  event.attributes.push_back({"amount", "100", minera::AttributeKind::kInt});
  event.attributes.push_back(
      {"channel <&>", "web \"x\"", minera::AttributeKind::kString});
  event.attributes.push_back(
      {"due", "2026-03-01T10:00:00Z", minera::AttributeKind::kDate});
  trace.events.push_back(event);
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("<string key=\"@@aligned_variant\" value=\"a,b\"/>") !=
         std::string::npos);
  assert(xes.find("<int key=\"amount\" value=\"100\"/>") != std::string::npos);
  assert(xes.find("<date key=\"due\" value=\"2026-03-01T10:00:00+00:00\"/>") !=
         std::string::npos);

  auto invalid = log;
  invalid.traces.front().events.front().attributes.push_back(
      {"odd-date", "someday", minera::AttributeKind::kDate});
  bool invalid_date_threw = false;
  try {
    (void)minera::to_xes(invalid);
  } catch (const std::invalid_argument&) {
    invalid_date_threw = true;
  }
  assert(invalid_date_threw);

  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  assert(*reread.attribute("@@aligned_variant") == "a,b");
  const auto& back = reread.traces.front().events.front();
  assert(back.attributes.size() == 3);
  assert(back.attributes[0].kind == minera::AttributeKind::kInt);
  assert(back.attributes[0].value == "100");
  assert(*back.attribute("channel <&>") == "web \"x\"");
  assert(back.attributes[2].kind == minera::AttributeKind::kDate);
}

// A three-event trace with a channel attribute on two events: web on a,
// phone on b, nothing on c.
minera::EventLog attribute_filter_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "1";
  trace.events.push_back(
      minera::Event{"1", "a", "", "", 0, {{"channel", "web", {}}}});
  trace.events.push_back(
      minera::Event{"1", "b", "", "", 1, {{"channel", "phone", {}}}});
  trace.events.push_back(minera::Event{"1", "c", "", "", 2});
  log.traces.push_back(trace);
  return log;
}

void filter_attribute_events_mode_keeps_and_removes() {
  const auto log = attribute_filter_log();

  minera::LogFilterOptions keep;
  keep.attribute_key = "channel";
  keep.keep_attribute_values = {"web"};
  assert(keep.any());
  const auto only_web = minera::filter_event_log(log, keep);
  // Only the web event survives a keep-set; the attribute-less event drops.
  assert((activity_names(only_web.traces.front()) ==
          std::vector<std::string>{"a"}));

  minera::LogFilterOptions remove;
  remove.attribute_key = "channel";
  remove.remove_attribute_values = {"phone"};
  const auto no_phone = minera::filter_event_log(log, remove);
  // The phone event goes; the attribute-less event is never dropped by a
  // remove-set.
  assert((activity_names(no_phone.traces.front()) ==
          std::vector<std::string>{"a", "c"}));

  minera::LogFilterOptions carriers;
  carriers.attribute_key = "channel";
  const auto with_key = minera::filter_event_log(log, carriers);
  // A bare key keeps only the events that carry the attribute at all.
  assert((activity_names(with_key.traces.front()) ==
          std::vector<std::string>{"a", "b"}));
}

void filter_attribute_traces_mode_selects_cases() {
  auto log = attribute_filter_log();
  minera::Trace other;
  other.case_id = "2";
  other.events.push_back(
      minera::Event{"2", "a", "", "", 0, {{"channel", "phone", {}}}});
  log.traces.push_back(other);
  minera::Trace plain;
  plain.case_id = "3";
  plain.events.push_back(minera::Event{"3", "a", "", "", 0});
  log.traces.push_back(plain);

  minera::LogFilterOptions keep;
  keep.attribute_key = "channel";
  keep.keep_attribute_values = {"web"};
  keep.attribute_mode = minera::AttributeFilterMode::traces;
  const auto web_cases = minera::filter_event_log(log, keep);
  // Case 1 has a web event somewhere, so it survives whole.
  assert(web_cases.trace_count() == 1);
  assert(web_cases.traces.front().events.size() == 3);

  minera::LogFilterOptions remove;
  remove.attribute_key = "channel";
  remove.remove_attribute_values = {"phone"};
  remove.attribute_mode = minera::AttributeFilterMode::traces;
  const auto no_phone_cases = minera::filter_event_log(log, remove);
  // Both cases touching phone drop; the attribute-less case survives.
  assert(no_phone_cases.trace_count() == 1);
  assert(no_phone_cases.traces.front().case_id == "3");

  minera::LogFilterOptions carriers;
  carriers.attribute_key = "channel";
  carriers.attribute_mode = minera::AttributeFilterMode::traces;
  const auto with_key = minera::filter_event_log(log, carriers);
  // A bare key keeps the cases where the attribute appears at all.
  assert(with_key.trace_count() == 2);
}

void filter_attribute_values_require_a_key() {
  assert(throws([] {
    minera::LogFilterOptions options;
    options.keep_attribute_values = {"web"};
    assert(options.any());
    (void)minera::filter_event_log(attribute_filter_log(), options);
  }));
}

void json_export_carries_event_attributes() {
  auto log = attribute_filter_log();
  minera::EventAttribute payload{"payload", "",
                                 minera::AttributeKind::kString};
  payload.container = minera::AttributeContainerKind::kContainer;
  minera::EventAttribute nested{"score", "7", minera::AttributeKind::kInt};
  nested.typed_value = std::int64_t{7};
  payload.children.push_back(std::move(nested));
  log.traces[0].events[0].attributes.push_back(std::move(payload));
  minera::EventAttribute non_finite{"ratio", "NaN",
                                    minera::AttributeKind::kFloat};
  non_finite.typed_value = std::numeric_limits<double>::quiet_NaN();
  log.traces[0].events[0].attributes.push_back(std::move(non_finite));
  log.traces[0].case_id_source_key = "business.case";
  log.traces[0].events[0].activity_source_key = "business.activity";
  log.metadata.globals.push_back({"custom", {{"ordered", "yes"}}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto json = minera::analysis_json("test", log, graph);

  // Per-event attributes ride along in the traces block.
  assert(json.find("\"attributes\": {\"channel\": \"web\"") !=
         std::string::npos);
  // The summary lists the key with its event count and value frequencies.
  assert(json.find("\"event_attributes\": [") != std::string::npos);
  assert(json.find("{\"key\": \"channel\", \"events\": 2, "
                   "\"distinct_values\": 2, \"values\": [") !=
         std::string::npos);
  assert(json.find("{\"value\": \"phone\", \"count\": 1}") !=
         std::string::npos);
  assert(json.find("\"log_metadata\":") != std::string::npos);
  assert(json.find("\"globals\":[{\"scope\":\"custom\"") !=
         std::string::npos);
  assert(json.find("\"attribute_details\": [") != std::string::npos);
  assert(json.find("\"container\":\"container\"") !=
         std::string::npos);
  assert(json.find("\"typed_value\":7") != std::string::npos);
  assert(json.find("\"typed_value\":\"NaN\"") != std::string::npos);
  assert(json.find("\"case_id_source_key\": \"business.case\"") !=
         std::string::npos);
  assert(json.find("\"activity_source_key\": \"business.activity\"") !=
         std::string::npos);
  assert(json.find("\"activity_state\": \"present\"") !=
         std::string::npos);

  // A log without generic attributes omits the field entirely.
  const auto plain = log_from_variants({{"a", "b"}});
  const auto plain_graph = minera::discover_directly_follows_graph(plain);
  const auto plain_json = minera::analysis_json("test", plain, plain_graph);
  assert(plain_json.find("event_attributes") == std::string::npos);
}

void importers_read_lifecycle_transitions() {
  // The CSV `lifecycle` column and the XES lifecycle:transition attribute
  // both land on the first-class lifecycle field, never on the generic
  // event attributes.
  const auto path = write_temp_log(
      "case_id,activity,timestamp,lifecycle,channel\n"
      "1,A,2026-01-01T00:00:00,start,web\n"
      "1,A,2026-01-01T00:00:30,complete,web\n");
  const auto csv_log = minera::import_csv_event_log(path);
  const auto& csv_events = csv_log.traces.front().events;
  assert(csv_events[0].lifecycle == "start");
  assert(csv_events[1].lifecycle == "complete");
  assert(csv_events[0].attribute("lifecycle") == nullptr);
  assert(*csv_events[0].attribute("channel") == "web");

  const auto xes_log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <string key=\"lifecycle:transition\" value=\"start\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");
  const auto& xes_event = xes_log.traces.front().events.front();
  assert(xes_event.lifecycle == "start");
  assert(xes_event.attribute("lifecycle:transition") == nullptr);
}

void xes_export_round_trips_lifecycle() {
  // Lifecycle transitions survive an export/import cycle, and the export
  // declares the Lifecycle extension only when the log uses one.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  trace.events.push_back(minera::Event{"c1", "a", "", "", 0, {}, "start"});
  trace.events.push_back(minera::Event{"c1", "a", "", "", 1, {}, "complete"});
  log.traces.push_back(trace);

  minera::XesExportOptions options;
  options.infer_metadata = true;
  const auto xes = minera::serialize_xes(log, options);
  assert(xes.find("lifecycle.xesext") != std::string::npos);
  assert(xes.find("<string key=\"lifecycle:transition\" value=\"start\"/>") !=
         std::string::npos);

  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  assert(reread.traces.front().events[0].lifecycle == "start");
  assert(reread.traces.front().events[1].lifecycle == "complete");

  // An atomic log declares no Lifecycle extension.
  const auto atomic =
      minera::serialize_xes(log_from_variants({{"a"}}), options);
  assert(atomic.find("lifecycle.xesext") == std::string::npos);
}

void filter_projects_by_lifecycle() {
  // One case: a start, a complete, and an atomic b without a lifecycle.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "1";
  trace.events.push_back(minera::Event{"1", "a", "", "", 0, {}, "start"});
  trace.events.push_back(minera::Event{"1", "a", "", "", 1, {}, "complete"});
  trace.events.push_back(minera::Event{"1", "b", "", "", 2});
  log.traces.push_back(trace);

  minera::LogFilterOptions keep;
  keep.keep_lifecycles = {"complete"};
  assert(keep.any());
  const auto completes = minera::filter_event_log(log, keep);
  // Only the complete survives; the lifecycle-less event is never kept by a
  // keep-set — this is the projection that makes a start/complete log atomic.
  assert(completes.traces.front().events.size() == 1);
  assert(completes.traces.front().events.front().lifecycle == "complete");

  minera::LogFilterOptions remove;
  remove.remove_lifecycles = {"start"};
  const auto no_starts = minera::filter_event_log(log, remove);
  // The start goes; the lifecycle-less event is never dropped by a remove-set.
  assert((activity_names(no_starts.traces.front()) ==
          std::vector<std::string>{"a", "b"}));
}

void alignment_summary_tallies_moves_per_activity() {
  // Model: a → b → c. Log: three clean traces, two skipping b (a model move
  // each), one inserting an unknown x (a log move). The aggregate must weigh
  // each variant by its trace count and rank deviating activities first.
  const auto tree =
      minera::discover_process_tree(log_from_variants({{"a", "b", "c"}}));
  const auto net = minera::to_petri_net(tree);
  const auto log = log_from_variants({{"a", "b", "c"},
                                      {"a", "b", "c"},
                                      {"a", "b", "c"},
                                      {"a", "c"},
                                      {"a", "c"},
                                      {"a", "b", "x", "c"}});
  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);

  const auto summary = minera::summarize_alignment_moves(alignment);
  assert(summary.size() == 4);
  // b: skipped twice, otherwise synchronous — the most deviating activity.
  assert(summary[0].activity == "b");
  assert(summary[0].synchronous == 4);
  assert(summary[0].log_moves == 0);
  assert(summary[0].model_moves == 2);
  // x: inserted once; the model has no step for it at all.
  assert(summary[1].activity == "x");
  assert(summary[1].synchronous == 0);
  assert(summary[1].log_moves == 1);
  assert(summary[1].model_moves == 0);
  // a and c never deviate; equal tallies fall back to alphabetical order.
  assert(summary[2].activity == "a");
  assert(summary[2].synchronous == 6);
  assert(summary[3].activity == "c");
  assert(summary[3].synchronous == 6);
  assert(summary[2].log_moves + summary[2].model_moves == 0);

  // The aggregate retains both document activity ids and lossless labels.
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto json = minera::alignment_json(alignment, graph);
  assert(json.find("\"activity_deviations\": [") != std::string::npos);
  const std::string b_entry =
      "{\"activity\": " + std::to_string(*graph.activity_id("b")) +
      ", \"activity_name\": \"b\", \"synchronous\": 4, \"log_moves\": 0, "
      "\"model_moves\": 2}";
  assert(json.find(b_entry) != std::string::npos);

  // An uncomputable alignment aggregates to nothing.
  assert(minera::summarize_alignment_moves(minera::AlignmentResult{
                                               false, false})
             .empty());
}

void replay_firing_sequences_walk_fitting_variants() {
  const auto tree = minera::discover_process_tree(log_from_variants({{"a", "b"}}));
  const auto net = minera::to_petri_net(tree);
  const auto sequences =
      minera::replay_firing_sequences(net, {{"a", "b"}, {"a", "x"}, {"b"}});
  assert(sequences.size() == 3);

  // The fitting variant yields one entry per event plus the finish routing,
  // each event entry ending with its visible transition.
  assert(sequences[0]);
  assert(sequences[0]->size() == 3);
  for (std::size_t i = 0; i < 2; ++i) {
    const auto& fired = (*sequences[0])[i];
    assert(!fired.empty());
    const auto& last = net.transitions()[fired.back()];
    assert(!last.silent());
    assert(net.activities()[*last.activity] == (i == 0 ? "a" : "b"));
  }

  // An unknown activity and a missing-token replay both disqualify.
  assert(!sequences[1]);
  assert(!sequences[2]);
}

// Register → (Approve | Reject) → Notify, with a numeric case attribute
// `amount` deciding the branch: 12 approvals at 500, 8 rejections at 2000.
minera::EventLog decision_amount_log() {
  minera::EventLog log;
  for (std::size_t c = 0; c < 20; ++c) {
    const bool approve = c < 12;
    minera::Trace trace;
    trace.case_id = std::to_string(c + 1);
    trace.attributes.push_back({"amount", approve ? "500" : "2000",
                                minera::AttributeKind::kInt});
    std::size_t order = 0;
    for (const char* activity :
         {"Register", approve ? "Approve" : "Reject", "Notify"}) {
      trace.events.push_back(
          minera::Event{trace.case_id, activity, "", "", order++});
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

minera::PetriNet decision_activity_selector_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto choice = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"Register", "Approve", "Reject"});
  net.add_transition(0, {source}, {choice});
  net.add_transition(1, {choice}, {sink});
  net.add_transition(2, {choice}, {sink});
  return net;
}

minera::EventLog decision_activity_selector_log() {
  minera::EventLog log;
  for (std::size_t c = 0; c < 20; ++c) {
    const bool approve = c < 12;
    minera::Trace trace;
    trace.case_id = std::to_string(c + 1);
    minera::Event first{trace.case_id, "recorded", "", "", 0};
    first.attributes.push_back(
        {"stage", "Register", minera::AttributeKind::kString});
    minera::Event branch{trace.case_id, "recorded", "", "", 1};
    branch.attributes.push_back(
        {"stage", approve ? "Approve" : "Reject",
         minera::AttributeKind::kString});
    trace.events.push_back(std::move(first));
    trace.events.push_back(std::move(branch));
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void decision_mining_supports_a_custom_activity_field() {
  const auto log = decision_activity_selector_log();
  const auto net = decision_activity_selector_net();

  const auto default_result = minera::mine_decisions(log, net);
  assert(default_result.activity_attribute == "concept:name");

  minera::DecisionMiningOptions selected;
  selected.activity_attribute = "stage";
  const auto result = minera::mine_decisions(log, net, selected);
  assert(result.activity_attribute == "stage");
  assert(result.replayed_traces == 20);
  assert(result.aligned_traces == 0);
  assert(result.alignment_failures == 0);
  assert(result.decision_points.size() == 1);
  const auto& point = result.decision_points.front();
  assert(point.observations == 20);
  assert(point.branches.size() == 2);
  const auto approve = std::find(point.branches.begin(), point.branches.end(),
                                 "Approve");
  const auto reject = std::find(point.branches.begin(), point.branches.end(),
                                "Reject");
  assert(approve != point.branches.end() && reject != point.branches.end());
  const auto& counts = point.nodes.front().branch_counts;
  assert(counts[static_cast<std::size_t>(approve - point.branches.begin())] ==
         12);
  assert(counts[static_cast<std::size_t>(reject - point.branches.begin())] == 8);

  bool threw = false;
  try {
    minera::DecisionMiningOptions missing;
    missing.activity_attribute = "does-not-exist";
    (void)minera::mine_decisions(log, net, missing);
  } catch (const std::invalid_argument& error) {
    threw = std::string(error.what()).find("activity attribute") !=
            std::string::npos;
  }
  assert(threw);
}

void decision_mining_recovers_a_numeric_rule() {
  const auto log = decision_amount_log();
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));
  const auto decisions = minera::mine_decisions(log, net);

  // The inductive model fits its own log perfectly: every trace contributes.
  assert(decisions.observed_traces == 20);
  assert(decisions.trace_count == 20);
  assert(decisions.decision_points.size() == 1);

  const auto& point = decisions.decision_points.front();
  assert(point.observations == 20);
  assert(point.branches.size() == 2);
  const bool approve_first = point.branches[0] == "Approve";
  assert(point.branches[approve_first ? 0 : 1] == "Approve");
  assert(point.branches[approve_first ? 1 : 0] == "Reject");

  // The mined rule is a threshold on the amount, separating 500 from 2000.
  const auto& root = point.nodes.front();
  assert(root.then_child >= 0);
  assert(root.split.key == "amount");
  assert(root.split.numeric);
  assert(root.split.threshold > 500.0 && root.split.threshold < 2000.0);
  assert(point.accuracy == 1.0);
  assert(point.majority_share == 0.6);
}

void decision_mining_uses_prior_event_attributes_only() {
  // The channel is written by Register — one step before the choice — so it
  // is in scope and separates the branches. A second run tags the *choice
  // event itself* instead: information that does not exist yet when the
  // token leaves the decision place, so no rule may use it.
  const auto build = [](bool on_register) {
    minera::EventLog log;
    for (std::size_t c = 0; c < 20; ++c) {
      const bool web = c < 12;
      minera::Trace trace;
      trace.case_id = std::to_string(c + 1);
      std::size_t order = 0;
      minera::Event reg{trace.case_id, "Register", "", "", order++};
      minera::Event check{trace.case_id,
                          web ? "AutoCheck" : "ManualCheck", "", "", order++};
      minera::EventAttribute channel{
          "channel", web ? "web" : "phone", minera::AttributeKind::kString};
      (on_register ? reg : check).attributes.push_back(channel);
      trace.events.push_back(std::move(reg));
      trace.events.push_back(std::move(check));
      trace.events.push_back(
          minera::Event{trace.case_id, "Done", "", "", order++});
      log.traces.push_back(std::move(trace));
    }
    return log;
  };

  const auto in_scope = build(true);
  const auto net = minera::to_petri_net(minera::discover_process_tree(in_scope));
  const auto found = minera::mine_decisions(in_scope, net);
  assert(found.decision_points.size() == 1);
  const auto& rule = found.decision_points.front();
  assert(rule.nodes.front().then_child >= 0);
  assert(rule.nodes.front().split.key == "channel");
  assert(!rule.nodes.front().split.numeric);
  assert(rule.nodes.front().split.value == "web");
  assert(rule.accuracy == 1.0);

  const auto too_late = build(false);
  const auto blind = minera::mine_decisions(too_late, net);
  assert(blind.decision_points.size() == 1);
  // Single-leaf tree: the attribute arrives with the choice, not before it.
  assert(blind.decision_points.front().nodes.size() == 1);
  assert(blind.decision_points.front().accuracy ==
         blind.decision_points.front().majority_share);
}

void decision_mining_options_bound_tree_and_scope_attributes() {
  const auto log = decision_amount_log();
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));

  minera::DecisionMiningOptions shallow;
  shallow.max_depth = 0;
  const auto shallow_result = minera::mine_decisions(log, net, shallow);
  assert(shallow_result.decision_points.size() == 1);
  assert(shallow_result.decision_points.front().nodes.size() == 1);

  minera::DecisionMiningOptions no_trace_attributes;
  no_trace_attributes.include_trace_attributes = false;
  no_trace_attributes.attributes = {"amount"};
  const auto without_context =
      minera::mine_decisions(log, net, no_trace_attributes);
  assert(without_context.decision_points.size() == 1);
  assert(without_context.decision_points.front().nodes.size() == 1);

  bool threw = false;
  try {
    minera::DecisionMiningOptions invalid;
    invalid.max_depth = 65;
    (void)minera::mine_decisions(log, net, invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void decision_mining_uses_alignment_for_deviating_traces() {
  auto log = decision_amount_log();
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));

  // A trace with an activity the model lacks is aligned: the inserted model
  // branch still gives a deterministic decision observation.
  minera::Trace rogue;
  rogue.case_id = "rogue";
  rogue.events.push_back(minera::Event{"rogue", "Register", "", "", 0});
  rogue.events.push_back(minera::Event{"rogue", "Zap", "", "", 1});
  log.traces.push_back(std::move(rogue));

  const auto decisions = minera::mine_decisions(log, net);
  assert(decisions.trace_count == 21);
  assert(decisions.observed_traces == 21);
  assert(decisions.replayed_traces == 20);
  assert(decisions.aligned_traces == 1);
  assert(decisions.alignment_failures == 0);
  assert(decisions.decision_points.size() == 1);
  assert(decisions.decision_points.front().observations == 21);
}

void json_export_carries_the_decisions_contract() {
  const auto log = decision_amount_log();
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));
  auto decisions = minera::mine_decisions(log, net);
  decisions.miner = "inductive";

  const auto json = minera::analysis_json(
      "test", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, &decisions);
  for (const char* key :
      {"\"decisions\"", "\"miner\": \"inductive\"", "\"observed_traces\": 20",
        "\"replayed_traces\": 20", "\"aligned_traces\": 0",
        "\"activity_attribute\": \"concept:name\"",
        "\"decision_points\"", "\"branches\"", "\"tree\"",
        "\"key\": \"amount\"", "\"threshold\"", "\"branch\"",
        "\"accuracy\"", "\"majority_share\""}) {
    assert(json.find(key) != std::string::npos);
  }
  // Omitted entirely when not supplied.
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"decisions\"") == std::string::npos);

  // The standalone writer serves the same object for the server endpoint.
  const auto standalone = minera::decisions_json(decisions);
  assert(standalone.find("\"decision_points\"") != std::string::npos);
}

void csv_importer_maps_case_columns_to_case_attributes() {
  // A `case:`-prefixed column is a case attribute: stored once on the trace
  // under the unprefixed key (first value wins, blanks skipped), and never
  // doubled as an event attribute.
  const auto path = write_temp_log(
      "case_id,activity,timestamp,case:region,amount\n"
      "1,A,2026-01-01T00:00:00,,100\n"
      "1,B,2026-01-01T00:01:00,north,250\n"
      "1,C,2026-01-01T00:02:00,south,300\n"
      "2,A,2026-01-01T00:03:00,east,\n");

  const auto log = minera::import_csv_event_log(path);
  const auto& first = log.traces[0];
  // The blank first cell is skipped; the first non-empty value sticks and the
  // later conflicting one is ignored.
  assert(first.attributes.size() == 1);
  assert(first.attributes[0].key == "region");
  assert(*first.attribute("region") == "north");
  assert(first.attributes[0].kind == minera::AttributeKind::kString);
  assert(*log.traces[1].attribute("region") == "east");
  // The column feeds the trace, not the events; other columns still do.
  assert(first.events[0].attribute("region") == nullptr);
  assert(first.events[0].attribute("case:region") == nullptr);
  assert(*first.events[0].attribute("amount") == "100");
}

void xes_importer_captures_case_attributes() {
  // Scalar trace attributes beyond concept:name are case attributes, kept
  // with their XES type; nested sub-attributes are ignored and the case id
  // itself is never doubled.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <int key=\"AMOUNT_REQ\" value=\"20000\"/>\n"
      "    <string key=\"customer\" value=\"acme\">\n"
      "      <string key=\"nested\" value=\"ignored\"/>\n"
      "    </string>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  const auto& trace = log.traces.front();
  assert(trace.case_id == "c");
  assert(trace.attributes.size() == 2);
  assert(trace.attributes[0].key == "AMOUNT_REQ");
  assert(trace.attributes[0].value == "20000");
  assert(trace.attributes[0].kind == minera::AttributeKind::kInt);
  assert(*trace.attribute("customer") == "acme");
  assert(trace.attribute("nested") == nullptr);
  assert(trace.attribute("concept:name") == nullptr);
  // The trace-level attributes never leak onto the events.
  assert(trace.events.front().attributes.empty());
}

void xes_export_round_trips_case_attributes() {
  // Case attributes survive an export/import cycle with their XES types,
  // exactly like the generic event attributes.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  trace.attributes.push_back(
      {"AMOUNT_REQ", "20000", minera::AttributeKind::kInt});
  trace.attributes.push_back(
      {"customer <&>", "acme \"x\"", minera::AttributeKind::kString});
  trace.events.push_back(minera::Event{"c1", "a", "", "", 0});
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("<int key=\"AMOUNT_REQ\" value=\"20000\"/>") !=
         std::string::npos);

  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  const auto& back = reread.traces.front();
  assert(back.attributes.size() == 2);
  assert(back.attributes[0].kind == minera::AttributeKind::kInt);
  assert(*back.attribute("AMOUNT_REQ") == "20000");
  assert(*back.attribute("customer <&>") == "acme \"x\"");
}

// Three one-event cases: region north on 1, south on 2, none on 3.
minera::EventLog case_attribute_filter_log() {
  minera::EventLog log;
  for (const auto& [id, region] :
       std::vector<std::pair<std::string, std::string>>{
           {"1", "north"}, {"2", "south"}, {"3", ""}}) {
    minera::Trace trace;
    trace.case_id = id;
    if (!region.empty()) {
      trace.attributes.push_back(
          {"region", region, minera::AttributeKind::kString});
    }
    trace.events.push_back(minera::Event{id, "a", "", "", 0});
    trace.events.push_back(minera::Event{id, "b", "", "", 1});
    log.traces.push_back(trace);
  }
  return log;
}

void filter_case_attribute_selects_cases() {
  const auto log = case_attribute_filter_log();

  minera::LogFilterOptions keep;
  keep.case_attribute_key = "region";
  keep.keep_case_attribute_values = {"north"};
  assert(keep.any());
  const auto north = minera::filter_event_log(log, keep);
  // Only the north case survives a keep-set; the attribute-less case drops.
  assert(north.trace_count() == 1);
  assert(north.traces.front().case_id == "1");
  // The surviving trace still carries its case attributes.
  assert(*north.traces.front().attribute("region") == "north");

  minera::LogFilterOptions remove;
  remove.case_attribute_key = "region";
  remove.remove_case_attribute_values = {"south"};
  const auto no_south = minera::filter_event_log(log, remove);
  // The south case goes; the attribute-less case is never dropped by a
  // remove-set.
  assert(no_south.trace_count() == 2);
  assert(no_south.traces[0].case_id == "1");
  assert(no_south.traces[1].case_id == "3");

  minera::LogFilterOptions carriers;
  carriers.case_attribute_key = "region";
  const auto with_key = minera::filter_event_log(log, carriers);
  // A bare key keeps only the cases that carry the attribute at all.
  assert(with_key.trace_count() == 2);

  // Case attributes ride through the other filters: an activity projection
  // rewrites the trace but keeps its attributes.
  minera::LogFilterOptions project;
  project.keep_activities = {"a"};
  const auto projected = minera::filter_event_log(log, project);
  assert(projected.trace_count() == 3);
  assert(*projected.traces.front().attribute("region") == "north");
  assert(projected.traces.front().events.size() == 1);
}

void filter_case_attribute_values_require_a_key() {
  assert(throws([] {
    minera::LogFilterOptions options;
    options.keep_case_attribute_values = {"north"};
    assert(options.any());
    (void)minera::filter_event_log(case_attribute_filter_log(), options);
  }));
}

void filter_preserves_typed_first_class_projection_context() {
  const std::string xml =
      "<log><trace><int key=\"case.number\" value=\"42\"/>"
      "<event><string key=\"concept:name\" value=\"A\"/>"
      "<date key=\"when.value\" value=\"2026-01-01T00:00:00Z\"/>"
      "</event><event><string key=\"concept:name\" value=\"B\"/>"
      "<date key=\"when.value\" value=\"2026-01-01T00:01:00Z\"/>"
      "</event></trace></log>";
  minera::XesImportOptions import_options;
  import_options.case_attribute = "case.number";
  import_options.timestamp_attribute = "when.value";
  std::istringstream input(xml);
  const auto imported = minera::import_xes_event_log(input, import_options);
  assert(imported.traces.size() == 1);
  assert(imported.traces.front().case_id_state ==
         minera::FirstClassAttributeState::kPresent);
  assert(imported.traces.front().case_id_source_key == "case.number");
  assert(imported.traces.front().case_id_attribute &&
         imported.traces.front().case_id_attribute->kind ==
             minera::AttributeKind::kInt);

  minera::LogFilterOptions options;
  options.keep_activities = {"A"};
  const auto filtered = minera::filter_event_log(imported, options);
  const auto& trace = filtered.traces.front();
  assert(trace.case_id == "42");
  assert(trace.case_id_state == minera::FirstClassAttributeState::kPresent);
  assert(trace.case_id_source_key == "case.number");
  assert(trace.case_id_attribute &&
         trace.case_id_attribute->kind == minera::AttributeKind::kInt &&
         std::get<std::int64_t>(trace.case_id_attribute->typed_value) == 42);
  assert(trace.events.size() == 1);
  const auto& event = trace.events.front();
  assert(event.timestamp == "2026-01-01T00:00:00.000Z");
  assert(event.timestamp_state == minera::FirstClassAttributeState::kPresent);
  assert(event.timestamp_source_key == "when.value");
  assert(event.timestamp_attribute &&
         event.timestamp_attribute->kind == minera::AttributeKind::kDate);

  const auto exported = minera::to_xes(filtered);
  assert(exported.find("<int key=\"case.number\" value=\"42\"/>") !=
         std::string::npos);
  assert(exported.find(
             "<date key=\"when.value\" value=\"2026-01-01T00:00:00+00:00\"/>") !=
         std::string::npos);
  std::istringstream round_input(exported);
  const auto round = minera::import_xes_event_log(round_input, import_options);
  assert(round.traces.front().case_id_attribute &&
         round.traces.front().case_id_attribute->kind ==
             minera::AttributeKind::kInt);
  assert(round.traces.front().events.front().timestamp_attribute &&
         round.traces.front().events.front().timestamp_attribute->kind ==
             minera::AttributeKind::kDate);
}

void json_export_carries_case_attributes() {
  const auto log = case_attribute_filter_log();
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto json = minera::analysis_json("test", log, graph);

  // Per-case attributes ride along in the traces block.
  assert(json.find("\"attributes\": {\"region\": \"north\"}") !=
         std::string::npos);
  // The summary lists the key with its case count and value frequencies.
  assert(json.find("\"case_attributes\": [") != std::string::npos);
  assert(json.find("{\"key\": \"region\", \"cases\": 2, "
                   "\"distinct_values\": 2, \"values\": [") !=
         std::string::npos);
  assert(json.find("{\"value\": \"north\", \"count\": 1}") !=
         std::string::npos);

  // A log without case attributes omits the field entirely.
  const auto plain = log_from_variants({{"a", "b"}});
  const auto plain_graph = minera::discover_directly_follows_graph(plain);
  const auto plain_json = minera::analysis_json("test", plain, plain_graph);
  assert(plain_json.find("case_attributes") == std::string::npos);
}

void xes_importer_preserves_missing_fields_and_rejects_malformed_xml() {
  // Missing first-class fields remain distinguishable from present empty
  // strings. They do not make an otherwise valid XES document malformed.
  const auto missing_case = import_xes(
      "<log><trace><event>"
      "<string key=\"concept:name\" value=\"A\"/>"
      "</event></trace></log>");
  assert(missing_case.traces[0].case_id_state ==
         minera::FirstClassAttributeState::kMissing);
  assert(missing_case.traces[0].events[0].activity == "A");

  const auto missing_activity = import_xes(
      "<log><trace>"
      "<string key=\"concept:name\" value=\"c\"/>"
      "<event><string key=\"org:resource\" value=\"x\"/></event>"
      "</trace></log>");
  assert(missing_activity.traces[0].case_id == "c");
  assert(missing_activity.traces[0].events[0].activity_state ==
         minera::FirstClassAttributeState::kMissing);
  // Unterminated tag.
  assert(throws([] { (void)import_xes("<log><trace"); }));
  // An empty stream yields an empty log rather than throwing.
  assert(import_xes("").trace_count() == 0);
}

std::size_t net_activity(const minera::HeuristicsNet& net, const std::string& name) {
  for (std::size_t i = 0; i < net.activities.size(); ++i) {
    if (net.activities[i] == name) return i;
  }
  assert(false);
  return 0;
}

const minera::HeuristicDependency* net_edge(
    const minera::HeuristicsNet& net,
    const std::string& source,
    const std::string& target) {
  const auto s = net_activity(net, source);
  const auto t = net_activity(net, target);
  for (const auto& edge : net.edges) {
    if (edge.source == s && edge.target == t) return &edge;
  }
  return nullptr;
}

minera::EventLog log_of(const std::vector<std::vector<std::string>>& traces) {
  minera::EventLog log;
  std::size_t order = 0;
  for (std::size_t i = 0; i < traces.size(); ++i) {
    minera::Trace trace;
    trace.case_id = std::to_string(i);
    for (const auto& activity : traces[i]) {
      trace.events.push_back(
          minera::Event{trace.case_id, activity, "", "", order++});
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void heuristic_miner_keeps_a_clean_sequence() {
  // A pure A -> B -> C sequence seen often enough to clear the 0.9 threshold.
  const auto log = log_of(std::vector<std::vector<std::string>>(
      10, {"A", "B", "C"}));
  const auto net = minera::discover_heuristics_net(log);

  assert(net_edge(net, "A", "B") != nullptr);
  assert(net_edge(net, "B", "C") != nullptr);
  // No spurious reverse edges.
  assert(net_edge(net, "B", "A") == nullptr);
  assert(net_edge(net, "C", "B") == nullptr);
  assert(net_edge(net, "A", "B")->frequency == 10);
  assert(net_edge(net, "A", "B")->dependency > 0.9);
  assert(net.start_activities.size() == 1);
  assert(net.start_activities[0] == net_activity(net, "A"));
  assert(net.end_activities.size() == 1);
  assert(net.end_activities[0] == net_activity(net, "C"));
}

void heuristic_miner_filters_noisy_reverse_edges() {
  // A is mostly followed by B, with a single reversed occurrence of noise. The
  // directly-follows graph keeps both directions; the Heuristic Miner does not.
  auto traces = std::vector<std::vector<std::string>>(5, {"A", "B"});
  traces.push_back({"B", "A"});
  const auto log = log_of(traces);

  const auto dfg = minera::discover_directly_follows_graph(log);
  assert(dfg.edge_count("A", "B") == 5);
  assert(dfg.edge_count("B", "A") == 1);

  minera::HeuristicMinerOptions options;
  options.dependency_threshold = 0.5;
  const auto net = minera::discover_heuristics_net(log, options);
  assert(net_edge(net, "A", "B") != nullptr);
  assert(net_edge(net, "B", "A") == nullptr);
}

void heuristic_miner_detects_self_loops() {
  minera::HeuristicMinerOptions options;
  options.dependency_threshold = 0.5;
  const auto log = log_of(std::vector<std::vector<std::string>>(
      3, {"A", "A", "A", "B"}));
  const auto net = minera::discover_heuristics_net(log, options);

  const auto* loop = net_edge(net, "A", "A");
  assert(loop != nullptr);
  assert(loop->frequency == 6);  // two A>A per trace, three traces
  assert(net_edge(net, "A", "B") != nullptr);
}

void heuristic_miner_handles_an_empty_log() {
  const auto net = minera::discover_heuristics_net(minera::EventLog{});
  assert(net.activities.empty());
  assert(net.edges.empty());
  assert(net.start_activities.empty());
  assert(net.end_activities.empty());
}

void heuristic_miner_and_threshold_controls_split_semantics() {
  // b and c are concurrent between a and d: they follow each other as often as
  // a splits to them, giving an AND measure of 12/(6+6+1) ≈ 0.923.
  // A threshold at 0.9 keeps the AND bundle; 0.95 breaks it into XOR branches.
  std::vector<std::vector<std::string>> variants(6, {"a", "b", "c", "d"});
  for (int i = 0; i < 6; ++i) variants.push_back({"a", "c", "b", "d"});
  const auto log = log_of(variants);

  minera::HeuristicMinerOptions and_opts;
  and_opts.and_threshold = 0.9;  // below 0.923 — should produce an AND bundle
  const auto and_net = minera::discover_heuristics_net(log, and_opts);
  const auto a_id_and = net_activity(and_net, "a");
  // Under AND semantics a fires into two parallel bundles.
  assert(and_net.output_bundles[a_id_and].size() == 1);
  assert(and_net.output_bundles[a_id_and][0].size() == 2);

  minera::HeuristicMinerOptions xor_opts;
  xor_opts.and_threshold = 0.95;  // above 0.923 — should produce separate XOR bundles
  const auto xor_net = minera::discover_heuristics_net(log, xor_opts);
  const auto a_id_xor = net_activity(xor_net, "a");
  assert(xor_net.output_bundles[a_id_xor].size() == 2);
  for (const auto& bundle : xor_net.output_bundles[a_id_xor]) {
    assert(bundle.size() == 1);
  }
}

void heuristic_miner_min_frequency_drops_rare_edges() {
  // A has one dominant successor B (10 traces) and one rare successor C (1
  // trace). Both have a positive dependency measure, but with min_frequency=2
  // the single A->C occurrence falls below the cutoff and is dropped.
  std::vector<std::vector<std::string>> traces(10, {"A", "B"});
  traces.push_back({"A", "C"});
  const auto log = log_of(traces);

  minera::HeuristicMinerOptions opts;
  opts.dependency_threshold = 0.0;
  opts.min_frequency = 2;
  const auto net = minera::discover_heuristics_net(log, opts);

  // A->B cleared the frequency cutoff via the threshold pass.
  assert(net_edge(net, "A", "B") != nullptr);
  assert(net_edge(net, "A", "B")->frequency == 10);

  // Discovery creates nodes from accepted endpoints only when at least one edge
  // survives, so the isolated C remains in activities but is not active.
  assert(net_edge(net, "A", "C") == nullptr);
  const auto c = net_activity(net, "C");
  assert(std::find(net.active_activities.begin(), net.active_activities.end(),
                   c) == net.active_activities.end());
}

void heuristic_miner_connect_all_false_may_leave_gaps() {
  // With only 5 traces the dependency A->B equals 5/6 ≈ 0.833, which falls
  // below the default 0.9 threshold, so A gets no accepted outgoing edge.
  // A is a start activity but never an end activity, so connect_all is
  // permitted to force an edge for it.
  const auto log = log_of(std::vector<std::vector<std::string>>(
      5, {"A", "B", "C"}));

  minera::HeuristicMinerOptions opts;
  opts.dependency_threshold = 0.9;
  opts.connect_all = false;  // nothing forced
  const auto net = minera::discover_heuristics_net(log, opts);

  const auto a = net_activity(net, "A");
  std::size_t a_out = 0;
  for (const auto& e : net.edges) { if (e.source == a) ++a_out; }
  assert(a_out == 0);

  // With connect_all enabled A must get at least its strongest outgoing edge.
  opts.connect_all = true;
  const auto connected = minera::discover_heuristics_net(log, opts);
  const auto a2 = net_activity(connected, "A");
  std::size_t a2_out = 0;
  for (const auto& e : connected.edges) { if (e.source == a2) ++a2_out; }
  assert(a2_out == 1);
}

// Transition-system discovery: compatible views plus Minera occurrence
// data. Kept as one isolated section so its contract is easy to audit.
minera::EventLog transition_test_log(
    std::initializer_list<std::vector<std::string>> traces) {
  minera::EventLog log;
  std::size_t trace_index = 0;
  for (const auto& activities : traces) {
    minera::Trace trace;
    trace.case_id = std::to_string(trace_index);
    for (std::size_t i = 0; i < activities.size(); ++i) {
      trace.events.push_back({trace.case_id, activities[i], "", "", i});
    }
    log.traces.push_back(std::move(trace));
    ++trace_index;
  }
  return log;
}

void json_export_carries_recursive_powl_contract() {
  const auto log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto powl = minera::PowlNode::loop(
      minera::PowlNode::strict_partial_order(
          {minera::PowlNode::transition("a"),
           minera::PowlNode::transition(
               "b", minera::PowlFrequency{0, std::nullopt})},
          {{0, 1}}),
      minera::PowlNode::xor_node(
          {minera::PowlNode::transition("c"), minera::PowlNode::silent()}));
  const auto powl_net = minera::to_petri_net(powl);
  const auto decoration = minera::decorate_model(log, powl_net);
  const auto json = minera::analysis_json(
      "test", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, &powl, &powl_net,
      &decoration);
  for (const char* fragment :
       {"\"powl\"", "\"kind\": \"loop\"",
        "\"kind\": \"strict_partial_order\"", "\"kind\": \"xor\"",
        "\"kind\": \"silent\"", "\"frequency\"",
        "\"minimum\": 0", "\"maximum\": null",
        "\"order\": [{\"source\": 0, \"target\": 1}]",
        "\"powl_petri_net\"", "\"powl_petri_net_decoration\""}) {
    assert(json.find(fragment) != std::string::npos);
  }
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"powl\"") == std::string::npos);
  assert(without.find("\"powl_petri_net\"") == std::string::npos);
}

void json_export_carries_log_skeleton_contract() {
  const auto log = transition_test_log({{"a", "b"}, {"a"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  minera::LogSkeletonOptions options;
  options.considered_constraints = {"equivalence", "activ_freq"};
  const auto model = minera::discover_log_skeleton(log, options);
  auto constrained_model = model;
  constrained_model.activ_freq["a"] = {2};
  const auto conformance =
      minera::check_log_skeleton_conformance(log, constrained_model, options);
  const auto json = minera::analysis_json(
      "test", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      &constrained_model, &conformance);
  for (const char* fragment :
       {"\"log_skeleton\"", "\"equivalence\"", "\"activ_freq\"",
        "\"log_skeleton_conformance\"", "\"case_id\": \"0\"",
        "\"constraint\": \"activ_freq\"", "\"observed_frequency\"",
        "\"allowed_frequencies\""}) {
    assert(json.find(fragment) != std::string::npos);
  }
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"log_skeleton\"") == std::string::npos);
  assert(without.find("\"log_skeleton_conformance\"") ==
         std::string::npos);

  const auto expect_rejected = [&](const minera::LogSkeletonModel& invalid_model,
                                   const std::vector<minera::LogSkeletonConformance>& invalid_conformance) {
    std::ostringstream stream;
    bool threw = false;
    try {
      minera::write_analysis_json(
          stream, "test", log, graph,
          /*tree=*/nullptr, /*net=*/nullptr, /*heuristics=*/nullptr,
          /*conformance=*/nullptr, /*performance=*/nullptr,
          /*resources=*/nullptr, /*variant_durations=*/nullptr,
          /*eventually_follows=*/nullptr, /*decisions=*/nullptr,
          /*petri_decoration=*/nullptr, /*spectrum=*/nullptr,
          /*inductive_net=*/nullptr, /*inductive_decoration=*/nullptr,
          /*heuristic_petri=*/nullptr, /*heuristic_decoration=*/nullptr,
          /*temporal_profile=*/nullptr, /*minimum_self_distances=*/nullptr,
          /*transition_system=*/nullptr, /*powl=*/nullptr,
          /*powl_petri=*/nullptr, /*powl_decoration=*/nullptr,
          &invalid_model, &invalid_conformance);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
    assert(stream.str().empty());
  };
  auto unknown_activity_model = model;
  unknown_activity_model.activ_freq["missing"] = {1};
  expect_rejected(unknown_activity_model, conformance);
  expect_rejected(model, {});  // shorter than the two-trace event log
  auto too_many = conformance;
  too_many.push_back(conformance.front());
  expect_rejected(model, too_many);

  auto unknown_constraint = conformance;
  unknown_constraint[0].deviations.push_back(
      {"not_a_constraint", {}, {}, 0, {}});
  expect_rejected(model, unknown_constraint);
}

void json_export_carries_declare_contract() {
  auto log = transition_test_log({{"a", "b"}, {"b"}});
  log.traces[0].case_id = "duplicate";
  log.traces[1].case_id = "duplicate";
  auto graph = minera::discover_directly_follows_graph(log);
  graph.ensure_activity("missing");  // considered-activity style registry entry
  const auto empty_activity = graph.ensure_activity("");
  minera::DeclareModel model;
  model.add(minera::kDeclareExistence, {"missing", {}, false, 2, 2});
  model.add(minera::kDeclareExistence, {"", {}, false, 2, 0});
  model.add(minera::kDeclareResponse, {"a", "b", true, 2, 1});
  model.add(minera::kDeclareResponse, {"a", "", true, 1, 0});
  auto conformance = minera::check_declare_conformance(log, model);
  const auto json = minera::analysis_json(
      "test", log, graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, &model, &conformance);
  for (const char* fragment :
       {"\"declare\"", "\"existence\"", "\"response\"",
        "\"activity\": 2", "\"source\": 0", "\"target\": 1",
        "\"declare_conformance\"", "\"trace_index\": 0",
        "\"trace_index\": 1", "\"case_id\": \"duplicate\""}) {
    assert(json.find(fragment) != std::string::npos);
  }
  const auto empty_id = std::to_string(empty_activity);
  assert(json.find("\"activity\": " + empty_id) != std::string::npos);
  assert(json.find("\"target\": " + empty_id) != std::string::npos);
  assert(json.find("\"template\": \"existence\", \"activity\": " +
                   empty_id) != std::string::npos);
  assert(json.find("\"template\": \"response\", \"source\": 0, \"target\": " +
                   empty_id) != std::string::npos);
  assert(json.find("\"declare\": {\"existence\"") != std::string::npos);
  assert(minera::analysis_json("test", log, graph).find("\"declare\"") ==
         std::string::npos);

  const auto expect_rejected = [&](const minera::DeclareModel* invalid_model,
                                   const std::vector<minera::DeclareConformance>* invalid_conf) {
    std::ostringstream stream;
    bool threw = false;
    try {
      minera::write_analysis_json(
          stream, "test", log, graph,
          nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr, invalid_model, invalid_conf);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
    assert(stream.str().empty());
  };
  auto unknown = model;
  unknown.rules[minera::kDeclareExistence].front().first = "not_registered";
  expect_rejected(&unknown, &conformance);
  auto too_short = conformance;
  too_short.pop_back();
  expect_rejected(&model, &too_short);
  auto bad_fitness = conformance;
  bad_fitness.front().dev_fitness = std::numeric_limits<double>::quiet_NaN();
  expect_rejected(&model, &bad_fitness);
  auto bad_shape = model;
  bad_shape.rules[minera::kDeclareExistence].front().binary = true;
  expect_rejected(&bad_shape, &conformance);
  auto bad_binary_activity = model;
  bad_binary_activity.rules[minera::kDeclareResponse].front().second =
      "not_registered";
  expect_rejected(&bad_binary_activity, &conformance);
  auto bad_deviation_activity = conformance;
  bool changed_binary = false;
  for (auto& result : bad_deviation_activity) {
    for (auto& deviation : result.deviations) {
      if (deviation.binary) {
        deviation.second = "not_registered";
        changed_binary = true;
        break;
      }
    }
    if (changed_binary) break;
  }
  assert(changed_binary);
  expect_rejected(&model, &bad_deviation_activity);
}

void json_export_carries_footprints_contract() {
  auto log = transition_test_log({{"a", "b"}, {"b", "a"}});
  log.traces[0].case_id = "duplicate";
  log.traces[1].case_id = "duplicate";
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto tree = minera::discover_process_tree(log);
  const auto powl = minera::discover_powl(log);
  const auto net = minera::to_petri_net(tree);

  minera::FootprintsAnalysis analysis;
  analysis.entire_event_log = minera::discover_footprints(log);
  analysis.entire_dataframe = *analysis.entire_event_log;
  analysis.polars_lazyframes = *analysis.entire_event_log;
  analysis.trace_by_trace = minera::discover_trace_footprints(log);
  analysis.dfg = minera::discover_dfg_footprints(graph);
  analysis.process_tree = minera::discover_footprints(tree);
  analysis.powl = minera::discover_footprints(powl);
  analysis.petri_reach_graph = minera::discover_footprints(net);
  analysis.petri_reach_graph->start_activities_present = false;
  analysis.petri_reach_graph->end_activities_present = false;
  analysis.petri_reach_graph->skippable = false;
  analysis.petri_reach_graph->activities_always_happening =
      std::set<std::string>{"a"};
  analysis.petri_reach_graph->max_trace_length_without_loops = 2;
  analysis.petri_reach_graph->trace = std::vector<std::string>{"a", "b"};
  analysis.model = "process_tree";
  analysis.strict = true;
  analysis.model_footprint = *analysis.process_tree;
  analysis.log_model = minera::check_footprints_log_model(
      *analysis.entire_event_log, analysis.model_footprint);
  analysis.log_extensive = minera::check_footprints_log_extensive(
      *analysis.entire_event_log, analysis.model_footprint);
  analysis.trace_extensive = minera::check_footprints_trace_extensive(
      *analysis.trace_by_trace, analysis.model_footprint);
  analysis.precision =
      minera::fp_precision(*analysis.entire_event_log,
                           analysis.model_footprint);
  const auto fitness = minera::fp_fitness(
      *analysis.trace_by_trace, analysis.model_footprint,
      *analysis.trace_extensive);
  analysis.log_fitness = fitness.log_fitness;
  analysis.perc_fit_traces = 50.0;

  const auto json = minera::analysis_json(
      "test", log, graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      &analysis);
  for (const char* fragment :
       {"\"footprints\"", "\"discovery\"",
        "\"entire_event_log\"", "\"entire_dataframe\"",
        "\"polars_lazyframes\"", "\"trace_by_trace\"", "\"dfg\"",
        "\"process_tree\"", "\"powl\"", "\"petri_reach_graph\"",
        "\"sequence\":", "\"parallel\":", "\"source\": 0",
        "\"target\": 1", "\"conformance\": {\"model\": \"process_tree\"",
        "\"log_model\": {\"strict\": true, \"violations\": [], "
        "\"is_fit\": true}",
        "\"log_extensive\"",
        "\"trace_extensive\": [{\"trace_index\": 0, \"case_id\": "
        "\"duplicate\", \"footprints\":",
        "\"metrics\"", "\"precision\"", "\"log_fitness\"",
        "\"perc_fit_traces\": 50", "\"trace_index\": 0",
        "\"trace_index\": 1", "\"case_id\": \"duplicate\""}) {
    assert(json.find(fragment) != std::string::npos);
  }
  assert(json.find("\"result\":") == std::string::npos);
  // The DFG discovery variant intentionally omits its inner DFG and minimum
  // length fields while retaining the presence-defined boundary fields.
  const auto dfg_pos = json.find("\"dfg\": {");
  const auto process_tree_pos = json.find("\"process_tree\":", dfg_pos);
  assert(dfg_pos != std::string::npos);
  assert(process_tree_pos != std::string::npos);
  const auto dfg_json = json.substr(dfg_pos, process_tree_pos - dfg_pos);
  assert(dfg_json.find("\"dfg\": [") == std::string::npos);
  assert(dfg_json.find("\"min_trace_length\"") == std::string::npos);
  assert(dfg_json.find("\"start_activities\"") != std::string::npos);
  const auto petri_pos = json.find("\"petri_reach_graph\":");
  const auto conformance_pos = json.find("\"conformance\":", petri_pos);
  assert(petri_pos != std::string::npos);
  assert(conformance_pos != std::string::npos);
  const auto petri_json =
      json.substr(petri_pos, conformance_pos - petri_pos);
  assert(petri_json.find("\"start_activities\"") == std::string::npos);
  assert(petri_json.find("\"end_activities\"") == std::string::npos);
  assert(petri_json.find("\"skippable\": false") != std::string::npos);
  assert(petri_json.find("\"activities_always_happening\": [0]") !=
         std::string::npos);
  assert(petri_json.find("\"max_trace_length_without_loops\": 2") !=
         std::string::npos);
  assert(petri_json.find("\"trace\": [0, 1]") != std::string::npos);

  minera::FootprintsAnalysis only_log;
  only_log.entire_event_log = *analysis.entire_event_log;
  only_log.model_footprint = analysis.model_footprint;
  const auto selective = minera::analysis_json(
      "test", log, graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      &only_log);
  assert(selective.find("\"entire_event_log\"") != std::string::npos);
  assert(selective.find("\"trace_by_trace\"") == std::string::npos);
  assert(selective.find("\"petri_reach_graph\"") == std::string::npos);
  assert(minera::analysis_json("test", log, graph).find("\"footprints\"") ==
         std::string::npos);

  auto non_strict = analysis;
  non_strict.strict = false;
  const auto non_strict_json = minera::analysis_json(
      "test", log, graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      &non_strict);
  assert(non_strict_json.find(
             "\"log_model\": {\"strict\": false, \"violations\": [], "
             "\"is_fit\": true}") !=
         std::string::npos);

  const auto expect_rejected =
      [&](const minera::FootprintsAnalysis& invalid) {
        std::ostringstream stream;
        bool threw = false;
        try {
          minera::write_analysis_json(
              stream, "test", log, graph,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              &invalid);
        } catch (const std::invalid_argument&) {
          threw = true;
        }
        assert(threw);
        assert(stream.str().empty());
      };
  auto unknown_activity = analysis;
  unknown_activity.entire_event_log->activities.insert("not_registered");
  expect_rejected(unknown_activity);
  auto short_traces = analysis;
  short_traces.trace_by_trace->pop_back();
  expect_rejected(short_traces);
  auto short_conformance = analysis;
  short_conformance.trace_extensive->pop_back();
  expect_rejected(short_conformance);
  auto bad_metric = analysis;
  bad_metric.log_fitness = std::numeric_limits<double>::quiet_NaN();
  expect_rejected(bad_metric);
}

void json_export_carries_dfg_variants_contract() {
  const auto log = transition_test_log({{"a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  minera::DfgVariantsAnalysis variants;

  minera::DfgStatisticalAnalysis frequency;
  frequency.variant = minera::DfgStatisticalVariant::kNative;
  frequency.frequency_dfg[{"a", "b"}] = 2;
  variants.statistical.push_back({"frequency_greedy", frequency});

  minera::DfgStatisticalAnalysis triples;
  triples.variant = minera::DfgStatisticalVariant::kFrequencyTriples;
  triples.frequency_triples[{"a", "b", "a"}] = 3;
  variants.statistical.push_back({"frequency_triples", triples});

  minera::DfgStatisticalAnalysis performance;
  performance.variant = minera::DfgStatisticalVariant::kPerformance;
  minera::DfgPerformanceStatistics performance_edge;
  performance_edge.summary = {2, 3.0, 3.0, 2.0, 4.0,
                              std::sqrt(2.0), 6.0};
  performance_edge.raw_values = {2.0, 4.0};
  performance_edge.value = performance_edge.summary;
  performance.performance_dfg[{"a", "b"}] = performance_edge;
  variants.statistical.push_back({"performance_greedy", performance});

  minera::DfgStatisticalAnalysis case_attributes;
  case_attributes.variant = minera::DfgStatisticalVariant::kCaseAttributes;
  case_attributes.case_attributes.edges[{"a", "b"}]["concept:name"]["c1"] =
      1;
  case_attributes.case_attributes.nodes["a"]["region"]["EU"] = 2;
  variants.statistical.push_back({"case_attributes", case_attributes});

  minera::DfgStatisticalAnalysis clean;
  clean.variant = minera::DfgStatisticalVariant::kCleanPolars;
  clean.clean.graph[{"a", "b"}] = 1;
  clean.clean.start_activities["a"] = 1;
  clean.clean.end_activities["b"] = 1;
  variants.statistical.push_back({"clean_polars", clean});

  minera::DfgStatisticalAnalysis clean_time;
  clean_time.variant = minera::DfgStatisticalVariant::kCleanTime;
  clean_time.clean_time["b"] = 5.5;
  variants.statistical.push_back({"clean_time", clean_time});

  minera::DfgCausalAnalysis causal;
  causal.variant = minera::DfgCausalVariant::kHeuristic;
  causal.relations[{"a", "b"}] = 0.12345678901234566;
  causal.relations[{"b", "a"}] = -0.12345678901234566;
  variants.causal.push_back({"heuristic", causal});

  const auto json = minera::analysis_json(
      "test", log, graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, &variants);
  for (const char* fragment :
       {"\"dfg_variants\"", "\"statistical\"",
        "\"variant\": \"frequency_greedy\"",
        "\"frequency_dfg\": [{\"source\": 0, \"target\": 1, "
        "\"count\": 2}]",
        "\"frequency_triples\": [{\"first\": 0, \"middle\": 1, "
        "\"last\": 0, \"count\": 3}]",
        "\"variant\": \"performance_greedy\"", "\"raw_values\": [2, 4]",
        "\"value\": {\"count\": 2", "\"case_attributes\"",
        "\"attribute\": \"concept:name\"", "\"value\": \"c1\"",
        "\"variant\": \"clean_polars\"", "\"start_activities\"",
        "\"mean_seconds_from_case_start\": 5.5", "\"causal\"",
        "\"variant\": \"heuristic\"",
        "\"value\": 0.12345678901234566",
        "\"value\": -0.12345678901234566"}) {
    assert(json.find(fragment) != std::string::npos);
  }
  assert(minera::analysis_json("test", log, graph).find("\"dfg_variants\"") ==
         std::string::npos);

  auto invalid = variants;
  invalid.statistical.front().analysis.frequency_dfg.clear();
  invalid.statistical.front().analysis.frequency_dfg[{"a", "missing"}] = 1;
  std::ostringstream stream;
  bool threw = false;
  try {
    minera::write_analysis_json(
        stream, "test", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(stream.str().empty());

  auto statistical_mismatched = variants;
  statistical_mismatched.statistical.front().name = "performance";
  std::ostringstream statistical_mismatch_stream;
  threw = false;
  try {
    minera::write_analysis_json(
        statistical_mismatch_stream, "test", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &statistical_mismatched);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(statistical_mismatch_stream.str().empty());

  auto statistical_unknown = variants;
  statistical_unknown.statistical.front().name = "future";
  std::ostringstream statistical_unknown_stream;
  threw = false;
  try {
    minera::write_analysis_json(
        statistical_unknown_stream, "test", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &statistical_unknown);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(statistical_unknown_stream.str().empty());

  auto mismatched = variants;
  mismatched.causal.front().name = "alpha";
  std::ostringstream mismatch_stream;
  threw = false;
  try {
    minera::write_analysis_json(
        mismatch_stream, "test", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &mismatched);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(mismatch_stream.str().empty());

  auto unknown = variants;
  unknown.causal.front().name = "future";
  std::ostringstream unknown_stream;
  threw = false;
  try {
    minera::write_analysis_json(
        unknown_stream, "test", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &unknown);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  assert(unknown_stream.str().empty());

  const auto expect_oversized_dfg_count_rejected =
      [&](const minera::DfgVariantsAnalysis& oversized) {
        std::ostringstream oversized_stream;
        bool oversized_threw = false;
        try {
          minera::write_analysis_json(
              oversized_stream, "test", log, graph,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, &oversized);
        } catch (const std::invalid_argument&) {
          oversized_threw = true;
        }
        assert(oversized_threw);
        assert(oversized_stream.str().empty());
      };
  constexpr std::uint64_t kFirstUnsafeJsonInteger = 9007199254740992ULL;

  auto oversized_frequency = variants;
  oversized_frequency.statistical[0].analysis.frequency_dfg[{"a", "b"}] =
      kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_frequency);

  auto oversized_triples = variants;
  oversized_triples.statistical[1].analysis.frequency_triples[
      {"a", "b", "a"}] = kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_triples);

  auto oversized_performance_summary = variants;
  oversized_performance_summary.statistical[2]
      .analysis.performance_dfg[{"a", "b"}]
      .summary.count = kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_performance_summary);

  auto oversized_selected_summary = variants;
  oversized_selected_summary.statistical[2]
      .analysis.performance_dfg[{"a", "b"}]
      .value = minera::DfgPerformanceSummary{
          kFirstUnsafeJsonInteger, 3.0, 3.0, 2.0, 4.0, std::sqrt(2.0), 6.0};
  expect_oversized_dfg_count_rejected(oversized_selected_summary);

  auto oversized_case_attribute_edge = variants;
  oversized_case_attribute_edge.statistical[3]
      .analysis.case_attributes.edges[{"a", "b"}]["concept:name"]["c1"] =
      kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_case_attribute_edge);

  auto oversized_case_attribute_node = variants;
  oversized_case_attribute_node.statistical[3]
      .analysis.case_attributes.nodes["a"]["region"]["EU"] =
      kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_case_attribute_node);

  auto oversized_clean_graph = variants;
  oversized_clean_graph.statistical[4].analysis.clean.graph[{"a", "b"}] =
      kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_clean_graph);

  auto oversized_clean_start = variants;
  oversized_clean_start.statistical[4].analysis.clean.start_activities["a"] =
      kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_clean_start);

  auto oversized_clean_end = variants;
  oversized_clean_end.statistical[4].analysis.clean.end_activities["b"] =
      kFirstUnsafeJsonInteger;
  expect_oversized_dfg_count_rejected(oversized_clean_end);
}

void json_export_carries_correlation_mining_contract() {
  const auto log = transition_test_log({{"a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  minera::CorrelationMinerResult result;
  result.variant = minera::CorrelationVariant::kClassicSplit;
  result.activity_counts = {{"a", 1}, {"b", 1}};
  result.frequency_dfg[{"a", "b"}] = 1;
  result.frequency_dfg[{"b", "a"}] = 1;
  result.performance_dfg[{"a", "b"}] = 4.5;
  result.performance_dfg[{"b", "a"}] = 0.0;
  for (const auto& source : {"a", "b"}) {
    for (const auto& target : {"a", "b"}) {
      result.precede_succeed_probability[{source, target}] =
          source == target ? 0.0 : 0.75;
      result.duration_matrix[{source, target}] =
          source == target ? 0.0 : 4.5;
    }
  }
  minera::CorrelationVariantsAnalysis analyses;
  analyses.variants.push_back({"classic_split", result});

  const auto json = minera::analysis_json(
      "test", log, graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, &analyses);
  for (const char* fragment :
       {"\"correlation_mining\"", "\"variants\"",
        "\"variant\": \"classic_split\"",
        "\"frequency_dfg\": [{\"source\": 0, \"target\": 1, "
        "\"count\": 1}",
        "\"performance_dfg\": [{\"source\": 0, \"target\": 1, "
        "\"seconds\": 4.5}",
        "\"precede_succeed_probability\"",
        "\"duration_matrix\"", "\"value\": 0.75"}) {
    assert(json.find(fragment) != std::string::npos);
  }

  auto projected_graph = graph;
  minera::CorrelationMinerResult projected_result;
  projected_result.variant = minera::CorrelationVariant::kClassicSplit;
  projected_result.activity_counts = {{"x", 1}, {"y", 1}};
  projected_result.frequency_dfg[{"x", "y"}] = 1;
  projected_result.frequency_dfg[{"y", "x"}] = 1;
  projected_result.performance_dfg[{"x", "y"}] = 2.0;
  projected_result.performance_dfg[{"y", "x"}] = 3.0;
  for (const auto& source : {"x", "y"}) {
    for (const auto& target : {"x", "y"}) {
      projected_result.precede_succeed_probability[{source, target}] = 0.0;
      projected_result.duration_matrix[{source, target}] = 0.0;
    }
  }
  minera::CorrelationVariantsAnalysis projected_analyses;
  projected_analyses.variants.push_back(
      {"classic_split", std::move(projected_result)});
  minera::register_correlation_activities(projected_graph,
                                          projected_analyses);
  assert(projected_graph.activity_count("x") == 0);
  assert(projected_graph.activity_count("y") == 0);
  assert(projected_graph.activity_count("a") == 1);
  const auto projected_json = minera::analysis_json(
      "test", log, projected_graph,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, &projected_analyses);
  assert(projected_json.find("\"name\": \"x\", \"count\": 0") !=
         std::string::npos);
  assert(projected_json.find(
             "\"activity_counts\": [{\"activity\": 2, \"count\": 1") !=
         std::string::npos);
  assert(projected_json.find(
             "\"frequency_dfg\": [{\"source\": 2, \"target\": 3") !=
         std::string::npos);
  assert(minera::analysis_json("test", log, graph).find(
             "\"correlation_mining\"") == std::string::npos);

  const auto rejects =
      [&](const minera::CorrelationVariantsAnalysis& invalid) {
        std::ostringstream stream;
        bool threw = false;
        try {
          minera::write_analysis_json(
              stream, "test", log, graph,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, &invalid);
        } catch (const std::invalid_argument&) {
          threw = true;
        }
        assert(threw);
        assert(stream.str().empty());
      };

  auto invalid_activity = analyses;
  invalid_activity.variants.front().analysis.frequency_dfg.clear();
  invalid_activity.variants.front().analysis.performance_dfg.clear();
  invalid_activity.variants.front().analysis.frequency_dfg[
      {"a", "missing"}] = 1;
  invalid_activity.variants.front().analysis.performance_dfg[
      {"a", "missing"}] = 2.0;
  rejects(invalid_activity);

  auto unknown_name = analyses;
  unknown_name.variants.front().name = "classic_fast";
  rejects(unknown_name);

  auto mismatched_name = analyses;
  mismatched_name.variants.front().name = "classic";
  rejects(mismatched_name);

  auto missing_probability = analyses;
  missing_probability.variants.front()
      .analysis.precede_succeed_probability.erase({"a", "b"});
  rejects(missing_probability);

  auto missing_duration = analyses;
  missing_duration.variants.front().analysis.duration_matrix.erase(
      {"b", "a"});
  rejects(missing_duration);

  minera::CorrelationMinerResult isolated;
  isolated.variant = minera::CorrelationVariant::kClassicSplit;
  isolated.activity_counts = {{"a", 1}};
  isolated.frequency_dfg[{"a", "a"}] = 1;
  isolated.performance_dfg[{"a", "a"}] = 0.0;
  minera::CorrelationVariantsAnalysis missing_isolated_matrices;
  missing_isolated_matrices.variants.push_back(
      {"classic_split", std::move(isolated)});
  rejects(missing_isolated_matrices);
}

void transition_system_forward_backward_views() {
  const auto log = transition_test_log({{"a", "b", "a"}});
  const auto forward = minera::discover_transition_system(log);
  assert(forward.states().size() == 4);
  // States are sorted by canonical payload, not by first occurrence.
  assert((forward.states()[0].payload == std::vector<std::string>{}));
  assert((forward.states()[1].payload == std::vector<std::string>{"a"}));
  assert((forward.states()[2].payload == std::vector<std::string>{"a", "b"}));
  assert((forward.states()[3].payload == std::vector<std::string>{"b", "a"}));
  assert(forward.transitions().size() == 3);

  minera::TransitionSystemOptions options;
  options.direction = minera::Direction::kBackward;
  const auto backward = minera::discover_transition_system(log, options);
  assert((backward.states()[0].payload == std::vector<std::string>{}));
  assert((backward.states()[1].payload == std::vector<std::string>{"a"}));
  assert((backward.states()[2].payload == std::vector<std::string>{"a", "b"}));
  assert((backward.states()[3].payload == std::vector<std::string>{"b", "a"}));
  assert(backward.transitions().size() == 3);
  assert(backward.transitions()[0].source == 0);
  assert(backward.transitions()[0].target == 1);
  assert(backward.transitions()[0].label == "a");
  assert(backward.transitions()[1].source == 1);
  assert(backward.transitions()[1].target == 2);
  assert(backward.transitions()[1].label == "b");
  assert(backward.transitions()[2].source == 2);
  assert(backward.transitions()[2].target == 3);
  assert(backward.transitions()[2].label == "a");
}

void transition_system_views_counts_and_data() {
  const auto log = transition_test_log({{"a", "a", "b"}, {"a", "a", "b"}});
  minera::TransitionSystemOptions set_options;
  set_options.view = minera::View::kSet;
  const auto set = minera::discover_transition_system(log, set_options);
  minera::TransitionSystemOptions multiset_options;
  multiset_options.view = minera::View::kMultiset;
  const auto multiset = minera::discover_transition_system(log, multiset_options);
  assert(set.states().size() == multiset.states().size());
  const auto has_payload = [](const minera::TransitionSystem& system,
                              const std::vector<std::string>& payload) {
    return std::any_of(
        system.states().begin(), system.states().end(),
        [&](const auto& state) { return state.payload == payload; });
  };
  assert(has_payload(set, {"a"}));
  assert(!has_payload(set, {"a", "a"}));
  assert(!has_payload(multiset, {"a"}));
  assert(has_payload(multiset, {"a", "a"}));

  minera::TransitionSystemOptions data_options;
  data_options.window = 0;
  data_options.include_data = true;
  const auto data = minera::discover_transition_system(
      transition_test_log({{"a", "a"}}), data_options);
  assert(data.states().size() == 1 && data.transitions().size() == 1);
  assert(data.transitions()[0].count == 2);
  assert(data.transitions()[0].occurrences.size() == 2);
  assert(data.states()[0].incoming_occurrences.size() == 2);
  assert(data.states()[0].outgoing_occurrences.size() == 2);
  assert(data.states()[0].start_count == 1 && data.states()[0].end_count == 1);
}

void transition_system_empty_and_deterministic() {
  minera::EventLog empty;
  empty.traces.push_back({"empty", {}});
  const auto no_states = minera::discover_transition_system(empty);
  assert(no_states.states().empty() && no_states.transitions().empty());
  const auto empty_graph = minera::discover_directly_follows_graph(empty);
  const auto empty_json = minera::analysis_json(
      "empty", empty, empty_graph,
      /*tree=*/nullptr, /*net=*/nullptr, /*heuristics=*/nullptr,
      /*conformance=*/nullptr, /*performance=*/nullptr,
      /*resources=*/nullptr, /*variant_durations=*/nullptr,
      /*eventually_follows=*/nullptr, /*decisions=*/nullptr,
      /*petri_decoration=*/nullptr, /*spectrum=*/nullptr,
      /*inductive_net=*/nullptr, /*inductive_decoration=*/nullptr,
      /*heuristic_petri=*/nullptr, /*heuristic_decoration=*/nullptr,
      /*temporal_profile=*/nullptr, /*minimum_self_distances=*/nullptr,
      &no_states);
  assert(empty_json.find("\"states\": []") != std::string::npos);
  assert(empty_json.find("\"transitions\": []") != std::string::npos);
  const auto first = minera::discover_transition_system(
      transition_test_log({{"b", "a"}, {"a", "b"}}));
  const auto second = minera::discover_transition_system(
      transition_test_log({{"a", "b"}, {"b", "a"}}));
  assert(first.states().size() == second.states().size());
  for (std::size_t i = 0; i < first.states().size(); ++i) {
    assert(first.states()[i].payload == second.states()[i].payload);
  }
  assert(first.transitions().size() == second.transitions().size());
}

void transition_system_positional_options_and_invalid_enums() {
  const auto log = transition_test_log({{"a", "b", "a"}});
  const auto positional = minera::discover_transition_system(
      log, minera::TransitionDirection::kBackward, 1,
      minera::TransitionView::kSet, true);
  assert(positional.options().direction ==
         minera::TransitionDirection::kBackward);
  assert(positional.options().window == 1);
  assert(positional.options().view == minera::TransitionView::kSet);
  assert(positional.options().include_data);
  assert(!positional.transitions().empty());
  assert(!positional.transitions()[0].occurrences.empty());

  for (const auto invalid_direction :
       {static_cast<minera::TransitionDirection>(-1),
        static_cast<minera::TransitionDirection>(42)}) {
    minera::TransitionSystemOptions options;
    options.direction = invalid_direction;
    bool threw = false;
    try {
      (void)minera::discover_transition_system(log, options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  for (const auto invalid_view : {static_cast<minera::TransitionView>(-1),
                                  static_cast<minera::TransitionView>(42)}) {
    minera::TransitionSystemOptions options;
    options.view = invalid_view;
    bool threw = false;
    try {
      (void)minera::discover_transition_system(log, options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

}  // namespace

void json_export_carries_dfg_and_edit_alignment_contract() {
  minera::EventLog log;
  minera::Trace t; t.case_id = "c1";
  t.events.push_back({"c1", "A", "2026-01-01T00:00:00Z"});
  t.events.push_back({"c1", "C", "2026-01-01T00:00:01Z"});
  log.traces.push_back(t); log.attributes.push_back({"log_tag", "x", minera::AttributeKind::kString});
  minera::DfgFrequency dfg{{{"A", "B"}, 1}};
  std::map<std::string, std::uint64_t> starts{{"A",1}}, ends{{"B",1}};
  const auto d = minera::align_log_on_dfg(log, dfg, starts, ends);
  const auto metrics = minera::project_alignments_on_dfg(log, dfg, starts, ends);
  const auto projected = minera::project_log_on_dfg(log, dfg, starts, ends);
  const auto dj = minera::dfg_alignment_json(d, &metrics, &projected);
  assert(dj.find("\"family\": \"DFG\"") != std::string::npos);
  assert(dj.find("\"variant\": \"CLASSIC\"") != std::string::npos);
  assert(dj.find("\"unique_variants\": 1") != std::string::npos);
  assert(dj.find("\"computed\": true") != std::string::npos);
  assert(dj.find("log_tag") == std::string::npos);
  assert(dj.find("\"cost\"") != std::string::npos && dj.find("\"visited_states\"") != std::string::npos);
  assert(dj.find("\"projected_dfg\"") != std::string::npos && dj.find("\"projected_log\"") != std::string::npos);
  auto unavailable = d;
  unavailable.alignments.front().computed = false;
  const auto absent_json = minera::dfg_alignment_json(unavailable);
  assert(absent_json == "{\"family\": \"DFG\", \"variant\": \"CLASSIC\", \"unique_variants\": 1, \"alignments\": [{\"computed\": false}]}");
  assert(throws([&] { (void)minera::dfg_alignment_json(unavailable, &metrics); }));
  assert(throws([&] { (void)minera::dfg_alignment_json(unavailable, nullptr, &projected); }));
  auto invalid = d;
  invalid.unique_variants = 2;
  assert(throws([&] { (void)minera::dfg_alignment_json(invalid); }));
  invalid = d;
  invalid.alignments.front().cost = std::numeric_limits<double>::infinity();
  assert(throws([&] { (void)minera::dfg_alignment_json(invalid); }));
  invalid = d;
  invalid.alignments.front().fitness = std::numeric_limits<double>::quiet_NaN();
  assert(throws([&] { (void)minera::dfg_alignment_json(invalid); }));
  invalid = d;
  invalid.alignments.front().internal_cost = -1;
  assert(throws([&] { (void)minera::dfg_alignment_json(invalid); }));
  minera::EventLog target; minera::Trace tt; tt.case_id = "r"; tt.events.push_back({"r", "B"}); target.traces.push_back(tt);
  const auto e = minera::align_edit_distance(log, target);
  const auto ej = minera::edit_distance_json(e, false);
  assert(ej.find("\"family\": \"EDIT_DISTANCE\"") != std::string::npos);
  assert(ej.find("\"source_count\": 1") != std::string::npos && ej.find("\"target_index\"") != std::string::npos);
  assert(ej.find("\"source_event_count\"") != std::string::npos && ej.find("\"target_event_count\"") != std::string::npos);
  assert(ej.find("\"log\": \">>\"") != std::string::npos || ej.find("\"model\": \">>\"") != std::string::npos);
}

void json_export_carries_evaluation_contract() {
  minera::ReplayFitnessResult fitness;
  fitness.variant = minera::ReplayFitnessVariant::ALIGNMENT_BASED;
  fitness.fitness = 0.75;
  fitness.log_fitness = 0.7;
  fitness.average_trace_fitness = 0.8;
  fitness.percentage_fit_traces = 50.0;
  fitness.fitting_traces = 1;
  fitness.trace_count = 2;
  fitness.total_cost = 3.0;
  fitness.error = "bad \"quote\" \\\"path";
  const auto fj = minera::replay_fitness_json(fitness);
  assert(fj.find("\"variant\": \"alignment_based\"") != std::string::npos);
  assert(fj.find("\"percentage_of_fitting_traces\": 50") != std::string::npos);
  assert(fj.find("\"percFitTraces\": 50") != std::string::npos);
  assert(fj.find("\"averageFitness\": 0.8") != std::string::npos);
  assert(fj.find("bad \\\"quote\\\"") != std::string::npos);
  assert(fj.find("token_replay") == std::string::npos);

  minera::PrecisionResult precision;
  precision.variant = minera::PrecisionVariant::AUTOMATON_AFTER_ALIGN;
  precision.precision = 0.25;
  const auto pj = minera::precision_json(precision);
  assert(pj.find("\"variant\": \"automaton_after_align\"") != std::string::npos);
  assert(pj.find("\"precision\": 0.25") != std::string::npos);

  minera::GeneralizationResult generalization;
  generalization.generalization = 0.5;
  generalization.executed_transitions = 2;
  generalization.transition_count = 3;
  generalization.transition_frequencies = {2, 1, 0};
  const auto gj = minera::generalization_json(generalization);
  assert(gj.find("\"score\": 0.5") != std::string::npos);
  assert(gj.find("\"transition_frequencies\": [2, 1, 0]") != std::string::npos);

  minera::SimplicityResult simplicity;
  simplicity.variant = minera::SimplicityVariant::EXTENDED_CARDOSO;
  simplicity.simplicity = 0.4;
  simplicity.complexity = 3.0;
  simplicity.value = 7.0;
  simplicity.vertices = 4;
  simplicity.edges = 5;
  simplicity.strongly_connected_components = 2;
  const auto sj = minera::simplicity_json(simplicity);
  assert(sj.find("\"variant\": \"extended_cardoso\"") != std::string::npos);
  assert(sj.find("\"strongly_connected_components\": 2") != std::string::npos);

  minera::EmdResult emd;
  emd.distance = std::numeric_limits<double>::quiet_NaN();
  emd.computed = false;
  emd.normalized = true;
  emd.error = "distance unavailable";
  const auto ej = minera::emd_json(emd);
  assert(ej.find("\"variant\": \"pyemd\"") != std::string::npos);
  assert(ej.find("\"distance\": null") != std::string::npos);
  assert(ej.find("\"computed\": false") != std::string::npos);
  assert(ej.find("\"normalized\": true") != std::string::npos);
  assert(ej.find("\"use_fast_emd\": true") != std::string::npos);

  minera::ModelEvaluation model;
  model.miner = "miner \"one\"";
  model.replay_fitness = fitness;
  const auto mj = minera::model_evaluation_json(model);
  assert(mj.find("\"miner\": \"miner \\\"one\\\"\"") != std::string::npos);
  assert(mj.find("replay_fitness") != std::string::npos);

  minera::CompositeEvaluationResult composite;
  composite.fitness = 0.9;
  composite.precision = 0.8;
  composite.generalization = 0.7;
  composite.simplicity = 0.6;
  composite.metrics_average_weight = 0.75;
  composite.fscore = 0.8;
  composite.fitness_details.fitness = 0.9;
  composite.fitness_details.log_fitness = 0.9;
  composite.fitness_details.average_trace_fitness = 0.9;
  composite.fitness_details.percentage_fit_traces = 100.0;
  composite.fitness_details.fitting_traces = 2;
  composite.fitness_details.trace_count = 2;
  composite.fitness_score = 0.9;
  composite.fitness_weight = 0.25;
  composite.precision_weight = 0.25;
  composite.simplicity_weight = 0.25;
  composite.generalization_weight = 0.25;
  minera::ModelEvaluation composite_model;
  composite_model.miner = "composite";
  composite_model.composite = composite;
  const auto composite_json =
      minera::model_evaluation_json(composite_model);
  assert(composite_json.find("\"fitness\": {\"variant\": \"token_based\"") !=
         std::string::npos);
  assert(composite_json.find("\"fitness_score\": 0.9") !=
         std::string::npos);
  assert(composite_json.find("\"metrics_average_weight\": 0.75") !=
         std::string::npos);
  assert(composite_json.find("\"metricsAverageWeight\": 0.75") !=
         std::string::npos);

  minera::EventLog log;
  log.traces.push_back({"case", {{"case", "A"}}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto alignment_detail = minera::replay_fitness_json(fitness, &graph);
  assert(alignment_detail.find("\"alignments\":") != std::string::npos);
  assert(alignment_detail.find("\"token_replay\":") == std::string::npos);
  fitness.variant = minera::ReplayFitnessVariant::TOKEN_BASED;
  const auto token_detail = minera::replay_fitness_json(fitness, &graph);
  assert(token_detail.find("\"token_replay\":") != std::string::npos);
  assert(token_detail.find("\"perc_fit_traces\": 50") !=
         std::string::npos);
  assert(token_detail.find("\"alignments\":") == std::string::npos);
  precision.variant = minera::PrecisionVariant::ETCONFORMANCE_TOKEN;
  const auto token_precision = minera::precision_json(precision, &graph);
  assert(token_precision.find("\"token_replay\":") != std::string::npos);
  assert(token_precision.find("\"alignments\":") == std::string::npos);
  precision.variant = minera::PrecisionVariant::ALIGN_ETCONFORMANCE;
  const auto alignment_precision = minera::precision_json(precision, &graph);
  assert(alignment_precision.find("\"alignments\":") == std::string::npos);
  assert(alignment_precision.find("\"token_replay\":") == std::string::npos);
  precision.alignment_detail_present = true;
  const auto populated_alignment_precision =
      minera::precision_json(precision, &graph);
  assert(populated_alignment_precision.find("\"alignments\":") !=
         std::string::npos);
  const std::vector<minera::ModelEvaluation> evaluations{model};
  const auto embedded = minera::analysis_json(
      "evaluation", log, graph,
      /*tree*/ nullptr, /*net*/ nullptr, /*heuristics*/ nullptr,
      /*conformance*/ nullptr, /*performance*/ nullptr, /*resources*/ nullptr,
      /*variant_durations*/ nullptr, /*eventually_follows*/ nullptr,
      /*decisions*/ nullptr, /*petri_decoration*/ nullptr, /*spectrum*/ nullptr,
      /*inductive_net*/ nullptr, /*inductive_decoration*/ nullptr,
      /*heuristic_petri*/ nullptr, /*heuristic_decoration*/ nullptr,
      /*temporal_profile*/ nullptr, /*minimum_self_distances*/ nullptr,
      /*transition_system*/ nullptr, /*powl*/ nullptr, /*powl_petri*/ nullptr,
      /*powl_decoration*/ nullptr, /*log_skeleton*/ nullptr,
      /*log_skeleton_conformance*/ nullptr, /*declare*/ nullptr,
      /*declare_conformance*/ nullptr, /*footprints*/ nullptr,
      /*dfg_variants*/ nullptr, /*correlation_mining*/ nullptr,
      /*batch_discovery*/ nullptr, /*dfg_alignment*/ nullptr,
      /*edit_distance*/ nullptr, /*edit_anti*/ false,
      /*dfg_projection*/ nullptr, /*dfg_projected_log*/ nullptr,
      /*edit_projected_log*/ nullptr, /*process_tree_alignment*/ nullptr,
      /*decomposed_alignment*/ nullptr, /*petri_anti_alignment*/ nullptr,
      /*petri_multi_alignment*/ nullptr, /*reduced_process_tree*/ nullptr,
      /*reduced_trace_index*/ 0, /*evaluation*/ &evaluations,
      /*earth_movers_distance*/ &emd);
  assert(embedded.find("\"evaluation\":") != std::string::npos);
  assert(embedded.find("\"earth_movers_distance\":") != std::string::npos);
  const auto omitted = minera::analysis_json("evaluation", log, graph);
  assert(omitted.find("\"evaluation\":") == std::string::npos);
  assert(omitted.find("\"earth_movers_distance\":") == std::string::npos);

  auto duplicate = model;
  duplicate.precision = precision;
  const std::vector<minera::ModelEvaluation> duplicate_evaluations{
      model, duplicate};
  bool rejected_duplicate = false;
  try {
    (void)minera::analysis_json(
        "evaluation", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        0, &duplicate_evaluations);
  } catch (const std::invalid_argument&) {
    rejected_duplicate = true;
  }
  assert(rejected_duplicate);

  auto unnamed = model;
  unnamed.miner.clear();
  const std::vector<minera::ModelEvaluation> unnamed_evaluations{unnamed};
  bool rejected_unnamed = false;
  try {
    (void)minera::analysis_json(
        "evaluation", log, graph,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        0, &unnamed_evaluations);
  } catch (const std::invalid_argument&) {
    rejected_unnamed = true;
  }
  assert(rejected_unnamed);
}

void ilp_json_is_standalone_and_deterministic() {
  std::istringstream input("case_id,activity\nc1,A\nc1,B\nc2,A\nc2,B\n");
  const auto log = minera::import_csv_event_log(input);
  const auto result = minera::mine_ilp(log);
  const auto first = minera::ilp_miner_json(result);
  const auto second = minera::ilp_miner_json(result);
  assert(first == second);
  assert(first.find("\"family\":\"ILP\"") != std::string::npos);
  assert(first.find("\"net\":") != std::string::npos);
  assert(first.find("\"markings\":") != std::string::npos);
  assert(first.find("\"diagnostics\":") != std::string::npos);
}

void genetic_json_is_standalone_and_deterministic() {
  std::istringstream input("case_id,activity\nc1,A\nc1,B\nc2,A\nc2,B\n");
  const auto log = minera::import_csv_event_log(input);
  minera::GeneticMinerOptions options;
  options.seed = 11;
  options.population_size = 6;
  options.generations = 2;
  options.max_states = 32;
  const auto result = minera::mine_genetic(log, options);
  const auto first = minera::genetic_miner_json(result);
  const auto second = minera::genetic_miner_json(result);
  assert(first == second);
  assert(first.find("\"family\":\"GENETIC\"") != std::string::npos);
  assert(first.find("\"net\":") != std::string::npos);
  assert(first.find("\"markings\":") != std::string::npos);
  assert(first.find("\"best_fitness\":") != std::string::npos);
}

void organizational_sna_json_is_standalone_and_deterministic() {
  std::istringstream input(
      "case_id,activity,resource\n"
      "c1,A,alice\n"
      "c1,B,bob\n"
      "c2,A,alice\n"
      "c2,B,bob\n");
  const auto log = minera::import_csv_event_log(input);
  minera::OrganizationalSnaOptions options;
  options.variant = minera::OrganizationalSnaVariant::kWorkingTogether;
  const auto result = minera::discover_organizational_sna(log, options);
  const auto first = minera::organizational_sna_json(result);
  const auto second = minera::organizational_sna_json(result);
  assert(first == second);
  assert(first.find("\"family\":\"ORGANIZATIONAL_SNA\"") != std::string::npos);
  assert(first.find("\"directed\":false") != std::string::npos);
  assert(first.find("\"resources\":[\"alice\",\"bob\"]") != std::string::npos);
  assert(first.find("\"edges\":") != std::string::npos);
}

void trace_variant_query_json_is_standalone_and_deterministic() {
  minera::EventLog log;
  log.traces.push_back({"c1", {{"c1", "A", "", "", 0, {}, ""},
                              {"c1", "B", "", "", 1, {}, ""}}});
  minera::TraceVariantQueryOptions options;
  options.variant = minera::TraceVariantQueryVariant::kLaplace;
  options.max_prefix_length = 2;
  options.epsilon = 2.0;
  options.seed = 4;
  options.max_output_traces = 20;
  const auto result = minera::anonymize_trace_variant_query(log, options);
  const auto first = minera::trace_variant_query_json(result);
  const auto second = minera::trace_variant_query_json(result);
  assert(first == second);
  assert(first.find("\"family\":\"TRACE_VARIANT_QUERY\"") != std::string::npos);
  assert(first.find("\"variant\":\"laplace\"") != std::string::npos);
  assert(first.find("\"secure_random\":false") != std::string::npos);
  assert(first.find("\"selected_violation_count\":") != std::string::npos);
  assert(first.find("\"frequencies\":") != std::string::npos);
  assert(first.find("\"log\":") != std::string::npos);
}

void pripel_json_is_standalone_and_escapes_diagnostics() {
  minera::PripelResult result;
  result.diagnostics.epsilon = 1.0;
  result.diagnostics.message = "ok \"quoted\"";
  const auto json = minera::pripel_json(result);
  assert(json.find("\"family\":\"PRIPEL\"") != std::string::npos);
  assert(json.find("ok \\\"quoted\\\"") != std::string::npos);
  assert(json.find("\"secure_random\":false") != std::string::npos);
  assert(json.find("\"log\":{") != std::string::npos);
  assert(json.find("\"traces\":[]") != std::string::npos);
}

void csv_case_aliases_are_opt_in_and_exact_columns_win() {
  const std::string qualified = "case:concept:name,activity\ncase-1,A\n";
  bool rejected = false;
  try {
    std::istringstream input(qualified);
    (void)minera::import_csv_event_log(input);
  } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  minera::CsvImportOptions options;
  options.allow_case_id_aliases = true;
  std::istringstream input(qualified);
  const auto imported = minera::import_csv_event_log(input, options);
  assert(imported.traces.size() == 1 && imported.traces[0].case_id == "case-1");
  assert(imported.traces[0].case_id_source_key == "case:concept:name");
  options.case_id_column = "case:concept:name";
  std::istringstream short_header("case_id,activity\nshort-case,A\n");
  assert(minera::import_csv_event_log(short_header, options).traces[0].case_id == "short-case");
  const std::string both = "case_id,case:concept:name,activity\nx,joined,A\ny,joined,B\n";
  std::istringstream exact_qualified(both);
  assert(minera::import_csv_event_log(exact_qualified, options).traces.size() == 1);
  options.case_id_column = "case_id";
  std::istringstream exact_short(both);
  assert(minera::import_csv_event_log(exact_short, options).traces.size() == 2);
  options.case_id_column = "order_id";
  std::istringstream missing_custom(both);
  rejected = false;
  try { (void)minera::import_csv_event_log(missing_custom, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

int main() {
  csv_case_aliases_are_opt_in_and_exact_columns_win();
  ilp_json_is_standalone_and_deterministic();
  trace_variant_query_json_is_standalone_and_deterministic();
  pripel_json_is_standalone_and_escapes_diagnostics();
  process_tree_alignment_json_carries_failure_and_moves();
  temporal_profile_discovers_all_pairs_and_stats();
  minimum_self_distance_discovers_distances_and_witnesses();
  minimum_self_distance_supports_custom_activity_and_case_columns();
  minimum_self_distance_json_resolves_ids_and_validates_before_writing();
  temporal_profile_scales_to_many_activities();
  temporal_profile_rejects_or_skips_bad_timestamps_and_checks_deviations();
  temporal_profile_supports_attributes_business_hours_and_validation();
  temporal_profile_business_hours_handle_multiweek_and_before_epoch();
  temporal_profile_validates_profiles_and_zeta_boundary();
  imported_first_class_selectors_reach_temporal_behavioral_adapters();
  imports_csv_and_discovers_directly_follows_graph();
  handles_quoted_csv_fields_and_timestamp_sorting();
  aggregates_repeated_trace_variants();
  inductive_miner_discovers_sequence();
  inductive_miner_discovers_exclusive_choice();
  inductive_miner_discovers_parallel();
  inductive_miner_discovers_loop();
  inductive_miner_models_optional_skip();
  inductive_miner_models_empty_trace_before_cut();
  inductive_miner_repairs_a_cycle_with_specialized_fallthroughs();
  inductive_miner_infrequent_recovers_a_noisy_sequence();
  inductive_miner_infrequent_leaves_a_clean_log_untouched();
  inductive_miner_infrequent_filters_an_edge_at_the_cutoff();
  inductive_miner_infrequent_terminates_on_a_filtered_exclusive_cut();
  inductive_miner_infrequent_keeps_structure_the_filter_would_destroy();
  inductive_miner_infrequent_drops_a_stray_edge_from_an_end_activity();
  inductive_miner_infrequent_preserves_observed_end_activities();
  inductive_miner_infrequent_uses_total_trace_weight_for_empty_noise();
  inductive_miner_dfg_matches_log_recursion_on_structured_logs();
  inductive_miner_dfg_skips_empty_traces_at_the_root();
  inductive_miner_dfg_ignores_the_infrequent_threshold();
  inductive_miner_accepts_direct_dfg_input();
  petri_net_translates_sequence();
  petri_net_reduces_parallel_routing();
  alpha_miner_discovers_a_sequence();
  alpha_miner_shares_a_place_for_exclusive_choice();
  alpha_miner_keeps_parallel_branches_on_separate_places();
  alpha_miner_classic_drops_short_loop_activities();
  alpha_plus_attaches_a_length_one_loop();
  alpha_plus_discovers_a_length_two_loop();
  alpha_plus_matches_classic_without_short_loops();
  alpha_miner_handles_an_empty_log();
  alpha_miner_accepts_direct_dfg_input();
  json_export_carries_the_petri_net_contract();
  json_export_carries_recursive_powl_contract();
  json_export_carries_log_skeleton_contract();
  json_export_carries_dfg_and_edit_alignment_contract();
  conformance_is_perfect_on_the_discovery_log();
  conformance_is_perfect_for_every_operator();
  conformance_counts_unmodeled_activities();
  conformance_penalizes_a_deviating_trace();
  backwards_replay_supports_legacy_variation();
  backwards_replay_marks_unroutable_and_stops_when_requested();
  backwards_replay_ignores_unknown_and_interrupts_duplicate_labels();
  precision_is_high_for_a_tight_model_and_low_for_a_flower();
  generalization_rewards_a_well_exercised_model();
  simplicity_reflects_routing_density();
  footprints_match_the_discovery_log();
  footprints_flag_a_forbidden_succession();
  footprints_detect_a_too_loose_model();
  pnml_round_trip_preserves_replay_behavior();
  parse_pnml_reads_a_foreign_net();
  parse_pnml_infers_missing_markings();
  parse_pnml_rejects_malformed_nets();
  bpmn_round_trip_preserves_replay_behavior();
  parse_bpmn_reads_a_foreign_model();
  parse_bpmn_keeps_disconnected_tasks();
  parse_bpmn_reads_subprocesses();
  parse_bpmn_reads_interrupting_boundary_events();
  parse_bpmn_supports_inclusive_gateway_blocks();
  parse_bpmn_rejects_unsupported_models();
  decorate_model_counts_firings_and_waits();
  decorate_model_skips_deviating_and_untimed_traces();
  ptml_round_trip_preserves_replay_behavior();
  parse_ptml_reads_a_foreign_tree();
  parse_ptml_rejects_malformed_trees();
  xes_export_round_trips_the_log();
  xes_export_keeps_unparseable_timestamps_verbatim();
  graph_counts_rework_cases();
  eventually_follows_counts_every_later_occurrence();
  eventually_follows_handles_repeated_activities();
  alignment_precision_is_perfect_for_an_exact_model();
  alignment_precision_flags_unused_model_behavior();
  alignment_precision_survives_a_deviating_log();
  playout_generates_perfectly_fitting_logs();
  playout_simulates_an_imported_model();
  extensive_playout_enumerates_the_language();
  performance_spectrum_finds_pattern_occurrences();
  temporal_behavioral_json_validates_and_serializes_batches();
  json_export_carries_the_eventually_follows_contract();
  json_export_carries_the_conformance_contract();
  conformance_handles_an_empty_log();
  alignment_is_perfect_for_every_operator();
  alignment_diagnoses_a_skipped_activity();
  alignment_diagnoses_an_inserted_activity();
  json_export_carries_the_alignment_contract();
  final_marking_reachable_distinguishes_sound_nets();
  alignment_scores_a_sound_alpha_net();
  alignment_flags_an_unsound_net();
  alignment_prunes_an_unbounded_net_with_dead_growth();
  alignment_flags_an_exhausted_search_on_an_unbounded_net();
  alignment_handles_an_empty_log();
  alignment_treats_an_empty_log_as_perfect_for_every_model();
  replay_degrades_gracefully_on_an_unbounded_silent_net();
  heuristics_net_becomes_a_replayable_workflow_net();
  heuristics_net_stays_replayable_when_it_drops_edges();
  heuristics_net_translates_an_and_split_and_join();
  heuristics_net_keeps_exclusive_branches_exclusive();
  heuristics_net_handles_a_self_loop();
  importer_handles_a_header_only_log();
  importer_strips_a_utf8_bom();
  dfg_handles_single_event_traces();
  importer_rejects_malformed_rows();
  filter_keeps_frequent_variants();
  filter_keeps_only_the_top_variant();
  filter_keeps_variants_covering_a_share_of_cases();
  filter_supports_variant_percentage_and_endpoint_polarity();
  filter_selects_cases_by_succession();
  filter_matches_trace_patterns();
  filter_matches_exact_admitted_variants();
  filter_cuts_prefixes_and_suffixes();
  parse_activity_pairs_reads_the_arrow_syntax();
  filter_projects_activities();
  filter_applies_length_and_endpoint_constraints();
  filter_rework_keeps_cases_with_a_repeated_activity();
  filter_between_extracts_subtraces();
  filter_between_handles_equal_endpoints_and_rejects_one_sided();
  filter_projects_by_resource();
  filter_time_range_trims_events();
  filter_time_range_selects_whole_traces();
  filter_time_range_bounds_are_inclusive_and_one_sided();
  filter_time_range_rejects_a_malformed_bound();
  filter_case_performance_uses_inclusive_trace_duration();
  filter_case_selection_limits_cases_and_event_counts();
  filter_numeric_attributes_support_event_and_trace_ranges();
  filter_frequency_attribute_variants_support_top_and_relative_occurrence();
  filter_absolute_attribute_occurrence_thresholds();
  filter_prefix_suffix_support_strictness_and_occurrence();
  csv_importer_captures_extra_columns_as_attributes();
  xes_importer_captures_generic_event_attributes();
  xes_export_round_trips_generic_attributes();
  filter_attribute_events_mode_keeps_and_removes();
  filter_attribute_traces_mode_selects_cases();
  filter_attribute_values_require_a_key();
  json_export_carries_event_attributes();
  alignment_summary_tallies_moves_per_activity();
  replay_firing_sequences_walk_fitting_variants();
  decision_mining_supports_a_custom_activity_field();
  decision_mining_recovers_a_numeric_rule();
  decision_mining_uses_prior_event_attributes_only();
  decision_mining_options_bound_tree_and_scope_attributes();
  decision_mining_uses_alignment_for_deviating_traces();
  json_export_carries_the_decisions_contract();
  importers_read_lifecycle_transitions();
  xes_export_round_trips_lifecycle();
  filter_projects_by_lifecycle();
  csv_importer_maps_case_columns_to_case_attributes();
  xes_importer_captures_case_attributes();
  xes_export_round_trips_case_attributes();
  filter_case_attribute_selects_cases();
  filter_case_attribute_values_require_a_key();
  filter_preserves_typed_first_class_projection_context();
  json_export_carries_case_attributes();
  xes_importer_reads_traces_and_events();
  xes_importer_reads_a_file_from_disk();
  xes_importer_variants_ordering_limits_and_projection();
  xes_importer_decodes_entities_and_ignores_nesting();
  xes_importer_preserves_missing_fields_and_rejects_malformed_xml();
  heuristic_miner_keeps_a_clean_sequence();
  heuristic_miner_filters_noisy_reverse_edges();
  heuristic_miner_detects_self_loops();
  heuristic_miner_handles_an_empty_log();
  heuristic_miner_and_threshold_controls_split_semantics();
  heuristic_miner_min_frequency_drops_rare_edges();
  heuristic_miner_connect_all_false_may_leave_gaps();
  transition_system_forward_backward_views();
  transition_system_views_counts_and_data();
  transition_system_empty_and_deterministic();
  transition_system_positional_options_and_invalid_enums();
  json_export_carries_declare_contract();
  json_export_carries_footprints_contract();
  json_export_carries_dfg_variants_contract();
  json_export_carries_correlation_mining_contract();
  json_export_carries_evaluation_contract();
  genetic_json_is_standalone_and_deterministic();
  organizational_sna_json_is_standalone_and_deterministic();
  return 0;
}
