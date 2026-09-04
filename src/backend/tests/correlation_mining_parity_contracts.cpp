#include "minera/correlation_miner.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/segment_filter.hpp"
#include "minera/timestamp.hpp"

namespace {

using namespace minera;

EventAttribute field(std::string key, std::string value,
                     AttributeKind kind = AttributeKind::kString,
                     bool is_null = false) {
  EventAttribute result;
  result.key = std::move(key);
  result.value = std::move(value);
  result.kind = kind;
  result.is_null = is_null;
  return result;
}

EventAttribute role_field(std::string key, std::string value,
                          FirstClassAttributeRole role,
                          AttributeKind kind = AttributeKind::kString) {
  auto result = field(std::move(key), std::move(value), kind);
  result.first_class_role = role;
  result.first_class_state = FirstClassAttributeState::kPresent;
  result.first_class_source_key = result.key;
  return result;
}

Event event(std::string activity, double complete,
            std::vector<EventAttribute> attributes = {}) {
  Event result;
  result.activity = std::move(activity);
  result.activity_state = FirstClassAttributeState::kPresent;
  result.timestamp = format_timestamp(complete);
  result.timestamp_state = FirstClassAttributeState::kPresent;
  result.attributes = std::move(attributes);
  return result;
}

Trace trace(std::string case_id, std::vector<Event> events) {
  Trace result;
  result.case_id = std::move(case_id);
  result.case_id_state = FirstClassAttributeState::kPresent;
  result.events = std::move(events);
  for (auto& item : result.events) {
    item.case_id = result.case_id;
    item.case_id_state = result.case_id_state;
  }
  return result;
}

EventRecord record(std::initializer_list<EventAttribute> fields) {
  EventRecord result;
  result.fields.assign(fields.begin(), fields.end());
  return result;
}

EventStream stream_from_rows(const std::vector<EventRecord>& rows) {
  EventStream result;
  result.events = rows;
  return result;
}

EventTable table_from_rows(const std::vector<std::string>& columns,
                          const std::vector<EventTable::Row>& rows) {
  EventTable result;
  result.columns = columns;
  result.rows = rows;
  return result;
}

EventTable custom_table() {
  const std::vector<std::string> columns = {"case_ref", "phase", "finished",
                                            "began"};
  const auto row = [&](std::string case_id, std::string activity,
                       double complete, double start) {
    return EventTable::Row{
        field("case_ref", std::move(case_id)),
        field("phase", std::move(activity)),
        field("finished", format_timestamp(complete), AttributeKind::kDate),
        field("began", format_timestamp(start), AttributeKind::kDate)};
  };
  return table_from_rows(
      columns, {row("one", "A", 10.0, 0.0), row("one", "B", 30.0, 20.0),
                row("two", "A", 50.0, 40.0), row("two", "B", 70.0, 60.0)});
}

CorrelationMinerOptions custom_options() {
  CorrelationMinerOptions options;
  options.activity_attribute = "phase";
  options.timestamp_attribute = "finished";
  options.start_timestamp_attribute = "began";
  options.case_id_attribute = "case_ref";
  return options;
}

void assert_near(double actual, double expected) {
  if (std::abs(actual - expected) >= 1.0e-9)
    std::fprintf(stderr, "correlation contract expected %.17g got %.17g\\n",
                 expected, actual);
  assert(std::abs(actual - expected) < 1.0e-9);
}

void assert_same_payload(const CorrelationMinerResult& left,
                         const CorrelationMinerResult& right) {
  assert(left.activity_counts == right.activity_counts);
  assert(left.frequency_dfg == right.frequency_dfg);
  assert(left.performance_dfg == right.performance_dfg);
  assert(left.precede_succeed_probability ==
         right.precede_succeed_probability);
  assert(left.duration_matrix == right.duration_matrix);
}

void assert_same(const CorrelationMinerResult& left,
                 const CorrelationMinerResult& right) {
  assert(left.variant == right.variant);
  assert_same_payload(left, right);
}

void selected_fields_work_for_all_eager_and_lazy_kinds() {
  const auto table = custom_table();
  const auto options = custom_options();
  const auto table_result = discover_correlation_miner(table, options);
  const auto lazy = LazyEventTable::from_table(table);
  const auto lazy_result = discover_correlation_miner(lazy, options);
  assert_same(table_result, lazy_result);

  EventStream stream;
  for (const auto& row : table.rows) {
    EventRecord item;
    for (const auto& cell : row)
      if (cell) item.fields.push_back(*cell);
    stream.events.push_back(std::move(item));
  }
  const auto stream_result = discover_correlation_miner(stream, options);
  assert_same(table_result, stream_result);
  assert_same(table_result,
             discover_correlation_miner(EventDataView(table), options));
  assert_same(table_result,
             discover_correlation_miner(EventDataView(stream), options));
  assert_same(table_result,
             discover_correlation_miner(EventDataView(lazy), options));
  assert(table_result.frequency_dfg.at({"A", "B"}) == 2);
  assert_near(table_result.performance_dfg.at({"A", "B"}), 10.0);
}

void omitted_start_follows_selected_completion() {
  auto options = custom_options();
  options.start_timestamp_attribute.clear();
  const auto implicit = discover_correlation_miner(custom_table(), options);
  options.start_timestamp_attribute = "finished";
  const auto explicit_same = discover_correlation_miner(custom_table(), options);
  assert_same(implicit, explicit_same);
}

void canonical_aliases_and_first_class_source_metadata_work() {
  EventStream stream;
  stream.events = {
      record({role_field("phase", "A", FirstClassAttributeRole::kActivity),
              role_field("finished", format_timestamp(0.0),
                         FirstClassAttributeRole::kTimestamp,
                         AttributeKind::kDate),
              role_field("case_ref", "x", FirstClassAttributeRole::kCaseId)}),
      record({role_field("phase", "B", FirstClassAttributeRole::kActivity),
              role_field("finished", format_timestamp(10.0),
                         FirstClassAttributeRole::kTimestamp,
                         AttributeKind::kDate),
              role_field("case_ref", "x", FirstClassAttributeRole::kCaseId)})};
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kTraceBased;
  // Canonical names resolve the role metadata even when the source keys are
  // custom and no canonical columns are present.
  const auto result = discover_correlation_miner(stream, options);
  assert(result.frequency_dfg.at({"A", "B"}) == 1);
  assert_near(result.precede_succeed_probability.at({"A", "B"}), 1.0);

  options.activity_attribute = "activity";
  options.timestamp_attribute = "timestamp";
  options.case_id_attribute = "case_id";
  const auto aliases_result = discover_correlation_miner(stream, options);
  assert_same(result, aliases_result);

  Event retained_projection;
  retained_projection.timestamp = format_timestamp(20.0);
  retained_projection.timestamp_state = FirstClassAttributeState::kPresent;
  retained_projection.activity_attribute = field("phase", "C");
  const auto* projected_value =
      dfg_event_field_value(retained_projection, "phase");
  assert(projected_value != nullptr && *projected_value == "C");
  EventLog projected_log;
  projected_log.traces = {trace("projected", {retained_projection})};
  CorrelationMinerOptions projected_options;
  projected_options.activity_attribute = "phase";
  const auto projected_result =
      discover_correlation_miner(projected_log, projected_options);
  assert(projected_result.activity_counts.at("C") == 1);

  EventLog resources;
  auto resource_event = event("ignored", 30.0);
  resource_event.resource = "worker";
  resource_event.resource_state = FirstClassAttributeState::kPresent;
  resource_event.resource_attribute = field("org:resource", "worker");
  resources.traces = {trace("resource-case", {resource_event})};
  projected_options.activity_attribute = "org:resource";
  const auto resource_result =
      discover_correlation_miner(resources, projected_options);
  assert(resource_result.activity_counts.at("worker") == 1);
  assert(*dfg_event_field_value(resources.traces.front().events.front(),
                                "org:resource") == "worker");

  EventLog cases;
  cases.traces = {trace("case-label", {event("ignored", 40.0)})};
  projected_options.activity_attribute = "case:id";
  const auto case_result = discover_correlation_miner(cases, projected_options);
  assert(case_result.activity_counts.at("case-label") == 1);
  assert(*dfg_event_field_value(cases.traces.front().events.front(),
                                "case:id") == "case-label");
}

void exact_generic_activity_selectors_beat_projection_aliases() {
  auto collision = event(
      "projected-activity", 0.0,
      {field("activity", "generic-activity-label"),
       field("timestamp", "generic-timestamp-label"),
       field("resource", "generic-resource-label")});
  collision.resource = "projected-resource";
  collision.resource_state = FirstClassAttributeState::kPresent;
  collision.resource_attribute = field("org:resource", "projected-resource");

  EventLog log;
  log.traces = {trace("collision", {std::move(collision)})};
  CorrelationMinerOptions options;
  options.activity_attribute = "activity";
  const auto activity = discover_correlation_miner(log, options);
  assert(activity.activity_counts.at("generic-activity-label") == 1);
  assert(!activity.activity_counts.contains("projected-activity"));

  options.activity_attribute = "timestamp";
  const auto timestamp = discover_correlation_miner(log, options);
  assert(timestamp.activity_counts.at("generic-timestamp-label") == 1);
  assert(!timestamp.activity_counts.contains(format_timestamp(0.0)));

  options.activity_attribute = "resource";
  const auto resource = discover_correlation_miner(log, options);
  assert(resource.activity_counts.at("generic-resource-label") == 1);
  assert(!resource.activity_counts.contains("projected-resource"));
}

void trace_based_groups_flat_rows_by_selected_case() {
  const std::vector<std::string> columns = {"case_ref", "phase", "finished"};
  const auto row = [&](std::string case_id, std::string activity,
                       double complete) {
    return EventTable::Row{
        field("case_ref", std::move(case_id)), field("phase", std::move(activity)),
        field("finished", format_timestamp(complete), AttributeKind::kDate)};
  };
  const auto table = table_from_rows(
      columns, {row("x", "A", 0.0), row("y", "A", 20.0),
                row("x", "B", 10.0), row("y", "B", 30.0)});
  auto options = custom_options();
  options.variant = CorrelationVariant::kTraceBased;
  options.start_timestamp_attribute.clear();
  const auto trace_result = discover_correlation_miner(table, options);
  assert_near(trace_result.precede_succeed_probability.at({"A", "B"}), 1.0);
  assert_near(trace_result.precede_succeed_probability.at({"B", "A"}), 0.0);

  auto explicit_name_table = table;
  explicit_name_table.columns[0] = "concept:name";
  options.case_id_attribute = "concept:name";
  const auto explicit_name =
      discover_correlation_miner(explicit_name_table, options);
  assert_same(trace_result, explicit_name);
}

void trace_based_accepts_blank_case_values() {
  const auto table = table_from_rows(
      {"case_ref", "phase", "finished"},
      {EventTable::Row{field("case_ref", ""), field("phase", "A"),
                       field("finished", format_timestamp(0.0),
                             AttributeKind::kDate)},
       EventTable::Row{field("case_ref", ""), field("phase", "B"),
                       field("finished", format_timestamp(10.0),
                             AttributeKind::kDate)}});
  auto options = custom_options();
  options.variant = CorrelationVariant::kTraceBased;
  options.start_timestamp_attribute.clear();
  const auto result = discover_correlation_miner(table, options);
  assert_near(result.precede_succeed_probability.at({"A", "B"}), 1.0);
}

void split_order_distinguishes_raw_flat_and_preordered_tables() {
  const auto make_record = [](std::string activity, double complete) {
    return record({field("phase", std::move(activity)),
                   field("finished", format_timestamp(complete),
                         AttributeKind::kDate)});
  };
  EventStream stream = stream_from_rows(
      {make_record("A", 100.0), make_record("A", 0.0),
       make_record("B", 10.0), make_record("B", 110.0)});
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kClassicSplit;
  options.activity_attribute = "phase";
  options.timestamp_attribute = "finished";
  options.sample_size = 2;
  const auto raw_result = discover_correlation_miner(stream, options);
  assert_near(raw_result.precede_succeed_probability.at({"A", "B"}), 0.0);

  EventTable table;
  table.columns = {"phase", "finished"};
  for (const auto& item : stream.events) {
    EventTable::Row row;
    for (const auto& value : item.fields) row.push_back(value);
    table.rows.push_back(std::move(row));
  }
  const auto table_result = discover_correlation_miner(table, options);
  assert_near(table_result.precede_succeed_probability.at({"A", "B"}), 1.0);
  assert_near(table_result.duration_matrix.at({"A", "B"}), 10.0);

  options.sample_size = std::numeric_limits<std::size_t>::max();
  const auto maximum_chunk = discover_correlation_miner(stream, options);
  options.variant = CorrelationVariant::kClassic;
  const auto classic = discover_correlation_miner(stream, options);
  assert(maximum_chunk.variant == CorrelationVariant::kClassicSplit);
  assert(classic.variant == CorrelationVariant::kClassic);
  assert_same_payload(maximum_chunk, classic);
}

void strict_required_cells_and_timestamps_fail_deterministically() {
  CorrelationMinerOptions options;
  options.activity_attribute = "phase";
  options.timestamp_attribute = "finished";

  const auto expect_invalid = [&](const EventTable& table,
                                  const CorrelationMinerOptions& settings) {
    bool threw = false;
    try {
      (void)discover_correlation_miner(table, settings);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };

  expect_invalid(table_from_rows(
      {"phase", "finished"},
      {EventTable::Row{std::nullopt,
                       field("finished", format_timestamp(0.0),
                             AttributeKind::kDate)}}),
                 options);
  expect_invalid(table_from_rows(
      {"phase", "finished"},
      {EventTable::Row{field("phase", " "),
                       field("finished", format_timestamp(0.0),
                             AttributeKind::kDate)}}),
                 options);
  expect_invalid(table_from_rows(
      {"phase", "finished"},
      {EventTable::Row{field("phase", "A"),
                       field("finished", "not-a-time", AttributeKind::kDate)}}),
                 options);
  expect_invalid(table_from_rows(
      {"phase", "finished"},
      {EventTable::Row{field("phase", "A"),
                       field("finished", "", AttributeKind::kDate, true)}}),
                 options);

  auto trace_options = options;
  trace_options.variant = CorrelationVariant::kTraceBased;
  expect_invalid(table_from_rows(
      {"phase", "finished"},
      {EventTable::Row{field("phase", "A"),
                       field("finished", format_timestamp(0.0),
                             AttributeKind::kDate)}}),
                 trace_options);
  trace_options.case_id_attribute = "case_ref";
  expect_invalid(table_from_rows(
      {"phase", "finished", "case_ref"},
      {EventTable::Row{field("phase", "A"),
                       field("finished", format_timestamp(0.0),
                             AttributeKind::kDate),
                       std::nullopt}}),
                 trace_options);

  expect_invalid(table_from_rows({"phase"}, {}), options);
  expect_invalid(table_from_rows({"finished"}, {}), options);
  expect_invalid(table_from_rows({"phase", "finished"}, {}), trace_options);

  bool lazy_threw = false;
  try {
    (void)discover_correlation_miner(
        LazyEventTable::from_table(table_from_rows({"phase"}, {})), options);
  } catch (const std::invalid_argument&) {
    lazy_threw = true;
  }
  assert(lazy_threw);

  const auto empty = discover_correlation_miner(
      table_from_rows({"phase", "finished"}, {}), options);
  assert(empty.activity_counts.empty());
  assert(empty.frequency_dfg.empty());
  assert(empty.performance_dfg.empty());
  assert(empty.precede_succeed_probability.empty());
  assert(empty.duration_matrix.empty());
}

void native_trace_boundaries_survive_custom_case_selectors() {
  EventLog log;
  auto first = event("A", 0.0, {field("case_ref", "shared")});
  auto second = event("B", 10.0, {field("case_ref", "shared")});
  log.traces = {trace("native-one", {std::move(first)}),
                trace("native-two", {std::move(second)})};
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kTraceBased;
  options.case_id_attribute = "case_ref";
  const auto result = discover_correlation_miner(log, options);
  assert_near(result.precede_succeed_probability.at({"A", "B"}), 0.0);
}

void filtered_csv_projection_preserves_rows_and_segment_case_ids() {
  std::istringstream csv(
      "case_id,activity,timestamp,group\n"
      "one,A,2024-01-01T00:00:00Z,\n"
      "one,B,2024-01-01T00:00:10Z,\n"
      "one,A,2024-01-01T00:00:20Z,\n"
      "one,B,2024-01-01T00:00:30Z,\n"
      "one,C,2024-01-01T00:00:40Z,\n");
  const auto imported = import_csv_event_data(csv);
  const auto filtered = filter_between(imported.log, "A", "B", "::");
  assert(filtered.trace_count() == 2);
  assert(filtered.event_count() == 4);

  const auto table = project_csv_event_table(imported.table, filtered);
  assert(table.rows.size() == 4);
  const auto case_index = table.column_index("case_id");
  const auto activity_index = table.column_index("activity");
  const auto group_index = table.column_index("group");
  const std::vector<std::string> expected_cases{
      "one::1", "one::1", "one::2", "one::2"};
  const std::vector<std::string> expected_activities{"A", "B", "A", "B"};
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    assert(table.rows[row][case_index]);
    assert(table.rows[row][case_index]->value == expected_cases[row]);
    assert(table.rows[row][activity_index]);
    assert(table.rows[row][activity_index]->value == expected_activities[row]);
    assert(table.rows[row][group_index]);
    assert(table.rows[row][group_index]->value.empty());
    assert(!table.rows[row][group_index]->is_null);
  }

  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kTraceBased;
  options.case_id_attribute = "case_id";
  const auto result = discover_correlation_miner(table, options);
  assert_near(result.precede_succeed_probability.at({"A", "B"}), 1.0);
  assert(!result.activity_counts.contains("C"));

  auto invalid_provenance = filtered;
  invalid_provenance.traces.front().events.front().input_order =
      imported.table.rows.size();
  bool threw = false;
  try {
    (void)project_csv_event_table(imported.table, invalid_provenance);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void lazy_inputs_restart_and_all_variants_are_deterministic() {
  const auto table = custom_table();
  const auto lazy = LazyEventTable::from_table(table);
  auto options = custom_options();
  const auto first = discover_correlation_miner(lazy, options);
  const auto second = discover_correlation_miner(lazy, options);
  assert_same(first, second);
  for (const auto variant : {CorrelationVariant::kClassic,
                             CorrelationVariant::kClassicSplit,
                             CorrelationVariant::kTraceBased}) {
    options.variant = variant;
    options.sample_size = 2;
    const auto left = discover_correlation_miner(table, options);
    const auto right = discover_correlation_miner(table, options);
    assert_same(left, right);
    assert(left.variant == variant);
  }
}

}  // namespace

int main() {
  selected_fields_work_for_all_eager_and_lazy_kinds();
  omitted_start_follows_selected_completion();
  canonical_aliases_and_first_class_source_metadata_work();
  exact_generic_activity_selectors_beat_projection_aliases();
  trace_based_groups_flat_rows_by_selected_case();
  trace_based_accepts_blank_case_values();
  split_order_distinguishes_raw_flat_and_preordered_tables();
  strict_required_cells_and_timestamps_fail_deterministically();
  native_trace_boundaries_survive_custom_case_selectors();
  filtered_csv_projection_preserves_rows_and_segment_case_ids();
  lazy_inputs_restart_and_all_variants_are_deterministic();
  return 0;
}
