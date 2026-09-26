#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/concept_drift.hpp"
#include "bose_log_apply_tests.hpp"
#include "bose_typed_helpers_tests.hpp"

namespace {

minera::EventAttribute floating(double value) {
  minera::EventAttribute result{"", value == 1.0 ? "1.0" : std::to_string(value),
                                minera::AttributeKind::kFloat};
  result.typed_value = value;
  return result;
}

minera::EventTable ordered_reference_table() {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "extra"};
  table.dtypes = {"str", "str", "datetime64[s, UTC]", "int64"};
  const std::vector<std::string> cases{"b", "a", "b", "a"};
  const std::vector<std::string> activities{"B", "A", "C", "D"};
  const std::vector<std::string> times{"2020-01-01T00:00:03+00:00",
                                       "2020-01-01T00:00:02+00:00",
                                       "2020-01-01T00:00:01+00:00",
                                       "2020-01-01T00:00:00+00:00"};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    minera::EventAttribute count{"extra", std::to_string(i + 1),
                                 minera::AttributeKind::kInt};
    count.typed_value = static_cast<std::int64_t>(i + 1);
    auto timestamp = minera_bose_typed_helpers_tests::date(
        times[i], "time:timestamp");
    table.rows.push_back({minera::EventAttribute{"case:concept:name", cases[i]},
                          minera::EventAttribute{"concept:name", activities[i]},
                          timestamp, count});
  }
  return table;
}

void assert_ordered_table_result(const minera::ConceptDriftTableResult& result) {
  assert(result.change_timestamps.empty() && result.p_values.empty());
  assert(result.returned_tables.size() == 1);
  const auto& frame = result.returned_tables.front();
  assert((frame.columns == std::vector<std::string>{"case:concept:name",
      "concept:name", "time:timestamp"}));
  assert((frame.dtypes == std::vector<std::string>{"str", "str", "datetime64[us, UTC]"}));
  assert(frame.rows.size() == 4);
  const std::vector<std::pair<std::string, std::string>> expected{
      {"0", "A"}, {"0", "C"}, {"1", "B"}, {"1", "A"}};
  for (std::size_t row = 0; row < expected.size(); ++row) {
    assert(frame.rows[row][0]->value == expected[row].first);
    assert(frame.rows[row][1]->value == expected[row].second);
    assert(frame.rows[row][2]->kind == minera::AttributeKind::kDate);
  }
}

void assert_log_dispatch_result(const minera::ConceptDriftTableResult& result) {
  assert((result.change_timestamps == std::vector<double>{21.0}));
  assert((result.p_values == std::vector<double>{1.0}));
  assert(result.returned_tables.size() == 2);
  for (const auto& frame : result.returned_tables) assert(frame.rows.size() == 4);
  const auto& first = result.returned_tables.front();
  assert(first.rows[0][0]->value == "0" && first.rows[0][1]->value == "A!");
  assert(first.rows[1][1]->value == "B!");
  assert(first.rows[2][0]->value == "1" && first.rows[2][1]->value == "C!");
  const auto& last = result.returned_tables.back();
  assert(last.rows[0][1]->value == "E!");
}

minera::ConceptDriftTableOptions log_options() {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = 2; options.window_size = 1;
  options.num_permutations = 1; options.threshold = 1.1;
  options.max_no_change_points = 1;
  options.activity_attribute = "selected_activity";
  options.timestamp_attribute = "selected_time";
  options.case_id_attribute = "unused_case_selector";
  return options;
}

minera::ConceptDriftTableOptions table_options() {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = 1; options.window_size = 0;
  options.num_permutations = 3; options.threshold = 1.1;
  options.max_no_change_points = 0;
  return options;
}

}  // namespace

// Expected values come from the typed-helper/direct, permutation, log, and table captures.
void bose_module_contract() {
  using namespace minera_bose_typed_helpers_tests;

  const auto one = integer(1, "first-int");
  const auto one_bool = boolean(true, "bool-alias");
  const auto zero_bool = boolean(false, "false");
  const auto typed_unique = minera::extract_concept_drift_typed_activities(
      {{one, one_bool, integer(2), zero_bool, integer(0)}});
  assert(typed_unique.size() == 3);
  assert(typed_unique[0].kind == minera::AttributeKind::kBoolean);
  assert(typed_unique[0].typed_value.scalar_identity() ==
         zero_bool.typed_value.scalar_identity());
  assert(typed_unique[1].kind == minera::AttributeKind::kInt);
  assert(typed_unique[1].typed_value.scalar_identity() ==
         one.typed_value.scalar_identity());
  assert(typed_unique[2].value == "2");

  const auto features = minera::extract_concept_drift_typed_features(
      {{integer(1), text("A"), boolean(true)}}, {text("A"), integer(1)});
  assert((features == std::vector<double>{1, 0, 1, 2, 0, 0}));
  const auto calendar = date("2020-01-01");
  const auto midnight = date("2020-01-01T00:00:00");
  const auto date_features = minera::extract_concept_drift_typed_features(
      {{calendar, midnight, calendar}}, {calendar, midnight});
  assert((date_features == std::vector<double>{2, 0, 0, 1, 0, 1}));

  const auto target = boolean(true);
  const auto follows = minera::compute_concept_drift_typed_follows_relation(
      {integer(1), target}, {target});
  assert(follows.size() == 1);
  assert(follows[0].first.kind == minera::AttributeKind::kInt);
  assert(follows[0].first.value == "1");
  assert(follows[0].second.kind == minera::AttributeKind::kBoolean);
  assert(follows[0].second.typed_value.scalar_identity() ==
         target.typed_value.scalar_identity());

  minera::EventLog partition_log;
  for (const char* label : {"A", "B", "C"}) {
    minera::Trace trace;
    minera::Event event;
    event.activity = label;
    event.activity_state = minera::FirstClassAttributeState::kPresent;
    trace.events.push_back(event);
    partition_log.traces.push_back(trace);
  }
  const auto default_groups = minera::split_concept_drift_sublogs(partition_log);
  assert(default_groups.size() == 1 && default_groups[0].traces.size() == 3);
  const auto partial_groups =
      minera::split_concept_drift_sublogs(partition_log, 2, false);
  assert(partial_groups.size() == 1 && partial_groups[0].traces.size() == 2);
  assert(partial_groups[0].traces[1].events[0].activity == "B");

  std::mt19937 permutation_rng(0);
  const std::vector<std::vector<double>> empty_matrix;
  const auto negative_p = minera::concept_drift_permutation_test(
      empty_matrix, empty_matrix, -3, permutation_rng);
  assert(negative_p == 0.0 && std::signbit(negative_p));
  assert(permutation_rng() == 2357136044U);
  assert(std::signbit(minera::concept_drift_permutation_test(
      empty_matrix, empty_matrix, -3, std::uint64_t{0})));

  const std::vector<std::vector<std::string>> string_traces{
      {"A"}, {"B"}, {"A"}, {"B"}};
  minera::ConceptDriftTraceOptions trace_options;
  trace_options.sub_log_size = 1;
  trace_options.window_size = 1;
  trace_options.num_permutations = 1;
  trace_options.threshold = 1.1;
  trace_options.max_no_change_points = 5;
  const auto string_result =
      minera::detect_concept_drift_traces(string_traces, trace_options);
  assert(string_result.sublogs == std::vector<std::vector<std::vector<std::string>>>(
                                     {{{"A"}}, {{"B"}}, {{"A"}}, {{"B"}}}));
  assert(string_result.change_points.size() == 3);
  for (std::size_t i = 0; i < string_result.change_points.size(); ++i) {
    assert(string_result.change_points[i].sublog_index ==
               static_cast<std::int64_t>(i + 1) &&
           string_result.change_points[i].p_value == 1.0);
  }
  auto negative_options = trace_options;
  negative_options.sub_log_size = -1;
  const auto negative_slices =
      minera::detect_concept_drift_traces(string_traces, negative_options);
  assert(negative_slices.sublogs.empty() && negative_slices.change_points.empty());

  const std::vector<std::vector<minera::EventAttribute>> boolean_traces{
      {boolean(true), integer(1), boolean(false)},
      {floating(1.0), integer(0)}, {boolean(false), boolean(true)},
      {integer(0), boolean(true)}};
  auto typed_options = trace_options;
  typed_options.num_permutations = 7;
  typed_options.max_no_change_points = 2;
  std::mt19937 typed_rng(0);
  const auto typed_result = minera::detect_concept_drift_typed_traces(
      boolean_traces, typed_options, typed_rng);
  assert(typed_result.sublogs.size() == 4);
  assert(typed_result.sublogs ==
         std::vector<std::vector<std::vector<minera::EventAttribute>>>(
             {{{boolean(true), integer(1), boolean(false)}},
              {{floating(1.0), integer(0)}},
              {{boolean(false), boolean(true)}}, {{integer(0), boolean(true)}}}));
  assert(typed_result.change_points.size() == 2);
  assert(typed_result.change_points[0].sublog_index == 1 &&
         typed_result.change_points[0].p_value == 1.0 &&
         typed_result.change_points[1].sublog_index == 2 &&
         typed_result.change_points[1].p_value == 1.0);
  assert(typed_rng() == 3488238119U);
  const auto empty_typed = minera::detect_concept_drift_typed_traces({}, trace_options);
  assert(empty_typed.sublogs.empty() && empty_typed.change_points.empty());

  auto log = minera_bose_log_apply_tests::input_log();
  const auto log_result = minera::apply_concept_drift_log(log, log_options());
  assert((log_result.change_timestamps == std::vector<double>{21.0}));
  assert((log_result.p_values == std::vector<double>{1.0}));
  assert(log_result.returned_tables.size() == 2);
  assert(log_result.returned_tables[0].rows[0][1]->value == "A!");
  assert(log_result.returned_tables[0].rows[1][1]->value == "B!");
  assert(log_result.returned_tables[1].rows[0][1]->value == "E!");

  auto table = ordered_reference_table();
  const auto table_result =
      minera::apply_concept_drift_table(table, table_options());
  assert_ordered_table_result(table_result);
}

// Dispatcher expectations come from the captured EventLog and EventTable results.
void bose_dispatch_contract() {
  auto log = minera_bose_log_apply_tests::input_log();
  const auto log_configuration = log_options();

  const auto log_default = minera::apply_concept_drift(log, log_configuration);
  assert_log_dispatch_result(log_default);
  const auto log_explicit = minera::apply_concept_drift(
      log, log_configuration, minera::ConceptDriftVariant::BOSE);
  assert_log_dispatch_result(log_explicit);
  std::mt19937 log_rng(0);
  const auto log_stateful = minera::apply_concept_drift(
      log, log_configuration, log_rng, minera::ConceptDriftVariant::kBose);
  assert_log_dispatch_result(log_stateful);
  assert(log_rng() == 2546248239U);

  const auto table = ordered_reference_table();
  const auto table_configuration = table_options();
  const auto table_default = minera::apply_concept_drift(table, table_configuration);
  assert_ordered_table_result(table_default);
  const auto table_explicit = minera::apply_concept_drift(
      table, table_configuration, minera::ConceptDriftVariant::BOSE);
  assert_ordered_table_result(table_explicit);
  std::mt19937 table_rng(0);
  const auto table_stateful = minera::apply_concept_drift(
      table, table_configuration, table_rng, minera::ConceptDriftVariant::kBose);
  assert_ordered_table_result(table_stateful);
  assert(table_rng() == 2678185683U);

  const auto unknown =
      static_cast<minera::ConceptDriftVariant>(std::uint8_t{99});
  std::mt19937 invalid_log_rng(0), untouched_log_rng(0);
  bool rejected = false;
  try {
    (void)minera::apply_concept_drift(log, log_configuration, invalid_log_rng,
                                      unknown);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected && invalid_log_rng() == untouched_log_rng());
  std::mt19937 invalid_table_rng(0), untouched_table_rng(0);
  rejected = false;
  try {
    (void)minera::apply_concept_drift(table, table_configuration, invalid_table_rng,
                                      unknown);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected && invalid_table_rng() == untouched_table_rng());
}

int main() {
  bose_module_contract();
  bose_dispatch_contract();
}
