#include "minera/active_event_source.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minera/dataframe_format_json.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {

using namespace minera;

ActiveSourceSelectors selectors() {
  return {"", "act", "ts", "res", "life"};
}

EventAttribute integer(std::string key, std::int64_t value) {
  EventAttribute result{std::move(key), std::to_string(value), AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

EventTable typed_table() {
  EventTable table;
  table.columns = {"", "act", "ts", "extra", "nullable", "absent", "na", "stale",
                   "__minera_private_source_row_0"};
  table.dtypes = {"object", "category", "datetime64[ns, UTC]", "object", "object", "object",
                  "object", "object", "object"};
  table.categorical_dtypes.resize(table.columns.size());
  table.categorical_dtypes[1] = EventTableCategoricalDtype{
      "object", {{"", "A"}, {"", "B"}, {"", "C"}, {"", "unused"}}, true};
  table.attributes = {{"source", "typed"}};
  table.metadata.extensions = {{"custom", "x", "urn:x"}};
  table.metadata.properties = {{"pm4py:param:case_id_key", "source-declaration"}};
  table.metadata.event_globals = {integer("global", 7)};
  table.metadata.globals = {{"event", table.metadata.event_globals}};
  table.metadata.compatibility_snapshot_valid = true;
  table.metadata.compatibility_globals_snapshot = table.metadata.globals;
  table.metadata.compatibility_event_globals_snapshot = table.metadata.event_globals;
  table.index = EventTableIndex{"MultiIndex", "object", {"part", std::nullopt}, {}};
  for (std::size_t row = 0; row < 6; ++row) {
    EventAttribute case_id = row == 1 || row == 4
                                 ? EventAttribute{"", "1"} : integer("", 1);
    case_id.first_class_role = FirstClassAttributeRole::kCaseId;
    case_id.first_class_state = row == 3 ? FirstClassAttributeState::kNull
                                     : row == 5 ? FirstClassAttributeState::kMissing
                                                : FirstClassAttributeState::kPresent;
    case_id.is_null = row == 3;
    if (case_id.is_null) case_id.typed_value = std::monostate{};
    case_id.first_class_source_key = row == 1 || row == 4 ? "text-case" : "numeric-case";
    EventAttribute timestamp{"ts", "2026-01-01T00:00:00Z", AttributeKind::kDate};
    timestamp.typed_value = NativeDate{timestamp.value, NativeDateResolution::kNanosecond, "UTC"};
    EventAttribute nested{"extra", "", AttributeKind::kString};
    nested.container = AttributeContainerKind::kList;
    nested.children = {integer("n", static_cast<std::int64_t>(row))};
    EventAttribute null{"nullable", "", AttributeKind::kString, true};
    EventAttribute stale{"stale", "old-case"};
    stale.first_class_role = FirstClassAttributeRole::kCaseId;
    stale.first_class_state = FirstClassAttributeState::kPresent;
    stale.first_class_source_key = "wrong";
    const std::string activity = row == 0 || row == 2 ? "A" : row == 1 || row == 4 ? "B" : "C";
    table.rows.push_back({case_id, EventAttribute{"act", activity}, timestamp, nested, null,
                         std::nullopt, make_native_na_attribute("na"), stale,
                         EventAttribute{"__minera_private_source_row_0", "user-value"}});
    table.index->labels.push_back({std::vector<EventTableIndexLabel>{
        {integer("", static_cast<std::int64_t>(row % 2))},
        {EventAttribute{"", "label-" + std::to_string(row)}}}});
  }
  table.validate();
  return table;
}

void assert_log_equal(const EventLog& left, const EventLog& right) {
  assert(serialize_active_source_log_json(left) == serialize_active_source_log_json(right));
}

void original_and_owned_views_preserve_complete_source() {
  const auto table = typed_table();
  const auto serialized = serialize_event_table_input_json(table);
  const std::string bytes("raw\0bytes\r\n", 11);
  auto source = ActiveEventSource::from_table(table, {"source.json", "table", bytes}, selectors());
  assert(source.file().bytes == bytes);
  assert(source.file().name == "source.json");
  assert(source.selected_kind() == EventDataKind::kEventTable);
  assert(source.preparation_options().case_id_key.empty());
  assert(!source.preparation_options().include_case_attributes);
  assert(serialize_event_table_input_json(source.original_table()) == serialized);
  assert(serialize_event_table_input_json(source.checked_table()) == serialized);
  assert(serialize_event_table_input_json(source.checked_lazy().materialize()) == serialized);
  assert(source.checked_log_preparation_error().empty());
  assert(source.checked_log().trace_count() == 4);
  const ActiveEventSource::RepresentativeRows expected{{0, 2}, {1, 4}, {3}, {5}};
  assert(source.representative_row_ordinals() == expected);
  assert(source.checked_log().traces[0].case_id == "1");
  assert(source.checked_log().traces[1].case_id == "1");
  assert(source.checked_log().traces[0].case_id_source_key == "numeric-case");
  assert(source.checked_log().traces[1].case_id_source_key == "text-case");
  assert(source.checked_log().traces[0].events[1].input_order == 1);
  assert(source.checked_log().traces[1].events[1].input_order == 1);
  assert(source.checked_log().traces[2].case_id_state == FirstClassAttributeState::kNull);
  assert(source.checked_log().traces[3].case_id_state == FirstClassAttributeState::kMissing);
  assert(source.checked_stream().events[0].field("")->first_class_role == FirstClassAttributeRole::kCaseId);
  assert(source.checked_stream().events[0].field("stale")->first_class_role == FirstClassAttributeRole::kNone);
  assert(source.checked_stream().events[0].field("__minera_private_source_row_0")->value == "user-value");
  assert(source.checked_stream().events[0].fields.size() == table.columns.size() - 1);
  assert(source.selected_view().table() == source.checked_table_if_available());
  auto mutable_copy = source.mutable_table_copy();
  mutable_copy.rows[0][1]->value = "changed";
  assert(source.checked_table().rows[0][1]->value == "A");
  auto shared_copy = source;
  assert(shared_copy.original_table_if_available() == source.original_table_if_available());
  const auto lazy = source.checked_lazy();
  source = ActiveEventSource::from_table(EventTable{}, {"empty", "table", ""});
  assert(serialize_event_table_input_json(lazy.materialize()) == serialized);
}

void log_preparation_failure_does_not_discard_flat_input() {
  EventTable table;
  table.columns = {"concept:name"};
  table.rows = {{EventAttribute{"concept:name", "A"}}};
  const auto source = ActiveEventSource::from_table(table, {"no-case.json", "table", "original"}, {},
                                                  EventDataKind::kEventLog);
  assert(!source.original_log_if_available());
  assert(!source.checked_log_if_available());
  assert(source.original_table_if_available());
  assert(source.checked_table_if_available());
  assert(source.checked_stream_if_available());
  assert(source.checked_lazy_if_available());
  assert(source.file().bytes == "original");
  assert(source.checked_log_preparation_error() == "log conversion: event stream record has no case id field");
  assert(source.checked_flat_projection_error().empty());
  bool rejected = false;
  try { (void)source.selected_view(); }
  catch (const std::invalid_argument& error) { rejected = error.what() == source.checked_log_preparation_error(); }
  assert(rejected);
  assert(source.view(EventDataKind::kEventTable).table()->rows.size() == 1);
  const auto empty = ActiveEventSource::from_table(EventTable{}, {"empty", "table", "{}"});
  assert(empty.checked_log_if_available() && empty.checked_log().event_count() == 0);
}

void selection_preserves_original_cells_and_multi_index() {
  const auto source = ActiveEventSource::from_table(typed_table(), {"typed", "table", "raw"}, selectors());
  LogFilterOptions options;
  options.keep_activities = {"B", "C"};
  const auto direct = filter_event_log(source.checked_log(), options);
  const auto filtered = source.with_filters(options);
  assert_log_equal(filtered.checked_log(), direct);
  const std::vector<std::size_t> expected{1, 3, 4, 5};
  assert(filtered.source_row_ordinals() == expected);
  assert(filtered.projection_changes().empty());
  assert(filtered.checked_table().dtypes == source.original_table().dtypes);
  assert(filtered.checked_table().categorical_dtypes == source.original_table().categorical_dtypes);
  assert(filtered.checked_table().metadata == source.original_table().metadata);
  assert(filtered.checked_table().metadata.compatibility_snapshot_valid);
  for (std::size_t row = 0; row < expected.size(); ++row) {
    assert(filtered.checked_table().rows[row] == source.original_table().rows[expected[row]]);
    assert(filtered.checked_table().index->labels[row] == source.original_table().index->labels[expected[row]]);
  }
  assert(filtered.checked_table().index->names == source.original_table().index->names);
  assert(source.checked_table().rows.size() == 6);
  assert(filtered.checked_log().traces[0].events[1].input_order == 1);
  const auto identity = source.with_filters([](const EventLog& log) { return log; },
      {"__minera_private_source_row_1", "__minera_private_source_row_2"});
  assert_log_equal(identity.checked_log(), source.checked_log());
  assert(serialize_event_table_input_json(identity.checked_table()) == serialize_event_table_input_json(source.original_table()));
}

void native_grouping_preserves_representative_and_input_order() {
  const auto source = ActiveEventSource::from_table(typed_table(), {"typed", "table", "raw"}, selectors());
  for (const auto mode : {CaseGroupingFilterType::kFirst, CaseGroupingFilterType::kLast}) {
    LogFilterOptions options;
    options.consecutive_activity_grouping = ConsecutiveActivityGroupingOptions{mode, "act"};
    const auto filtered = source.with_filters(options);
    const auto direct = filter_event_log(source.checked_log(), options);
    assert_log_equal(filtered.checked_log(), direct);
    assert(filtered.checked_log().traces[0].events[0].input_order ==
           (mode == CaseGroupingFilterType::kFirst ? 0u : 1u));
    assert(filtered.representative_row_ordinals()[0][0] ==
           (mode == CaseGroupingFilterType::kFirst ? 0u : 2u));
    assert(filtered.representative_row_ordinals()[1][0] ==
           (mode == CaseGroupingFilterType::kFirst ? 1u : 4u));
  }
  LogFilterOptions concat;
  concat.timestamp_case_grouping = TimestampCaseGroupingOptions{
      TimestampCaseGroupingType::kConcat, "act", "ts", " / "};
  const auto grouped = source.with_filters(concat);
  assert_log_equal(grouped.checked_log(), filter_event_log(source.checked_log(), concat));
  assert(grouped.checked_table().rows[0][1]->value == "A / A");
  assert(grouped.checked_table().rows[0][3] == source.original_table().rows[0][3]);
  assert(grouped.checked_table().categorical_dtypes == source.original_table().categorical_dtypes);
  assert(!grouped.projection_changes().empty());
  assert(grouped.representative_row_ordinals()[0][0] == 0);
}

void case_and_attribute_mutations_overlay_only_changed_cells() {
  const auto source = ActiveEventSource::from_table(typed_table(), {"typed", "table", "raw"}, selectors());
  const auto pipeline = [](const EventLog& log) {
    auto output = log;
    output.traces[0].case_id = "split-case";  // Event.case_id intentionally remains old.
    auto& event = output.traces[0].events[0];
    std::erase_if(event.attributes, [](const auto& attribute) { return attribute.key == "extra"; });
    event.attributes.push_back({"new-field", "new-value"});
    return output;
  };
  const auto filtered = source.with_filters(pipeline, {"extra", "new-field"});
  assert_log_equal(filtered.checked_log(), pipeline(source.checked_log()));
  const std::vector<std::size_t> order{0, 2, 1, 4, 3, 5};
  assert(filtered.source_row_ordinals() == order);
  assert(filtered.checked_table().rows[0][0]->value == "split-case");
  assert(filtered.checked_table().rows[1][0]->value == "split-case");
  assert(!filtered.checked_table().rows[0][3]);
  assert(filtered.checked_table().rows[0][4] == source.original_table().rows[0][4]);
  assert(!filtered.checked_table().rows[0][5]);
  assert(filtered.checked_table().rows[0][6] == source.original_table().rows[0][6]);
  const auto added = filtered.checked_table().column_index("new-field");
  assert(filtered.checked_table().rows[0][added]->value == "new-value");
  assert(!filtered.checked_table().rows[1][added]);
  assert(filtered.checked_table().dtypes[added] == "object");
  assert(filtered.checked_table().dtypes[1] == "category");
  const auto twice = filtered.with_filters([](const EventLog& log) { return log; });
  assert(serialize_event_table_input_json(twice.checked_table()) == serialize_event_table_input_json(filtered.checked_table()));
  assert(source.original_table().rows[0][0]->value == "1");
}

void native_case_split_overlays_actual_trace_ids() {
  const auto source = ActiveEventSource::from_table(typed_table(), {"typed", "table", "raw"}, selectors());
  LogFilterOptions options;
  options.activity_split_activities = {"A"};
  options.activity_split_separator = "::piece::";
  const auto direct = filter_event_log(source.checked_log(), options);
  const auto filtered = source.with_filters(options);
  assert_log_equal(filtered.checked_log(), direct);
  assert(filtered.checked_table_if_available());
  bool split_seen = false;
  for (std::size_t trace = 0; trace < filtered.checked_log().traces.size(); ++trace) {
    const auto& native_trace = filtered.checked_log().traces[trace];
    for (const auto row : filtered.representative_row_ordinals()[trace]) {
      assert(row);
      if (*row != 0 && *row != 2) continue;
      split_seen = split_seen || native_trace.case_id != "1";
      bool matched = false;
      for (std::size_t checked_row = 0; checked_row < filtered.source_row_ordinals().size(); ++checked_row) {
        if (filtered.source_row_ordinals()[checked_row] != *row) continue;
        assert(filtered.checked_table().rows[checked_row][0]->value == native_trace.case_id);
        assert(filtered.checked_table().rows[checked_row][3] == source.original_table().rows[*row][3]);
        matched = true;
      }
      assert(matched);
    }
  }
  assert(split_seen);
}

void duplicate_occurrences_keep_original_range_index_labels() {
  auto table = typed_table();
  table.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  for (std::size_t row = 0; row < table.rows.size(); ++row)
    table.index->labels.push_back({integer("", static_cast<std::int64_t>(10 + row))});
  const auto source = ActiveEventSource::from_table(table, {"range", "table", "raw"}, selectors());
  const auto filtered = source.with_filters([](const EventLog& log) {
    auto output = log;
    output.traces[0].events.push_back(output.traces[0].events[0]);
    return output;
  });
  assert(filtered.checked_table().index->kind == "Index");
  assert(filtered.source_row_ordinals()[0] == 0);
  assert(filtered.source_row_ordinals()[1] == 2);
  assert(filtered.source_row_ordinals()[2] == 0);
  assert(filtered.checked_table().index->labels[0] == table.index->labels[0]);
  assert(filtered.checked_table().index->labels[2] == table.index->labels[0]);
  assert(filtered.checked_log().traces[0].events[2].input_order == 0);
  table.index.reset();
  const auto implicit = ActiveEventSource::from_table(table, {"implicit", "table", "raw"}, selectors());
  LogFilterOptions options;
  options.keep_activities = {"B"};
  const auto selected = implicit.with_filters(options);
  assert(selected.checked_table().index->kind == "Index");
  assert(std::get<EventAttribute>(selected.checked_table().index->labels[0].value).value == "1");
  assert(std::get<EventAttribute>(selected.checked_table().index->labels[1].value).value == "4");
}

void marker_loss_retains_log_and_original_but_never_falls_back() {
  const auto source = ActiveEventSource::from_table(typed_table(), {"typed", "table", "raw"}, selectors());
  const auto pipeline = [](const EventLog& log) {
    auto output = log;
    Event synthesized;
    synthesized.activity = "new";
    synthesized.input_order = std::numeric_limits<std::size_t>::max();
    output.traces[0].events.push_back(synthesized);
    return output;
  };
  const auto filtered = source.with_filters(pipeline);
  assert_log_equal(filtered.checked_log(), pipeline(source.checked_log()));
  assert(filtered.checked_log_if_available());
  assert(!filtered.checked_table_if_available());
  assert(!filtered.checked_stream_if_available());
  assert(!filtered.checked_lazy_if_available());
  assert(filtered.original_table_if_available() == source.original_table_if_available());
  assert(filtered.original_log_if_available() == source.original_log_if_available());
  assert(filtered.file().bytes == "raw");
  assert(!filtered.representative_row_ordinals()[0].back());
  assert(!filtered.checked_flat_projection_error().empty());
  assert(filtered.view(EventDataKind::kEventLog).log()->event_count() == 7);
  bool rejected = false;
  try { (void)filtered.selected_view(); }
  catch (const std::invalid_argument& error) { rejected = error.what() == filtered.checked_flat_projection_error(); }
  assert(rejected);
}

void paired_csv_and_original_grouped_log_remain_native() {
  std::istringstream input("case_id,activity,timestamp,extra\n1,B,2026-01-01T00:02:00Z,b\n2,C,2026-01-01T00:00:00Z,c\n1,A,2026-01-01T00:01:00Z,a\n");
  const auto data = import_csv_event_data(input);
  const auto source = ActiveEventSource::from_csv(data, {"paired.csv", "csv", "csv-bytes"},
                                                 {"case_id", "activity", "timestamp", "resource", "lifecycle"});
  assert_log_equal(source.original_log(), data.log);
  assert(source.original_table_origin() == "paired_csv_import");
  assert(source.representative_row_ordinals()[0][0] == 2);
  assert(source.representative_row_ordinals()[0][1] == 0);
  LogFilterOptions options;
  options.keep_activities = {"B", "C"};
  const auto filtered = source.with_filters(options);
  const std::vector<std::size_t> selected{0, 1};
  assert(filtered.source_row_ordinals() == selected);
  assert_log_equal(filtered.checked_log(), filter_event_log(data.log, options));
  assert(filtered.checked_table().rows[0] == data.table.rows[0]);
  assert(filtered.checked_table().rows[1] == data.table.rows[1]);

  auto original_log = data.log;
  original_log.traces[0].events[0].input_order = 99;
  original_log.traces[1].events[0].input_order = 99;
  original_log.traces[0].properties = {{"trace-property", "retained"}};
  original_log.traces.push_back(Trace{"empty", {}});
  const auto xes = ActiveEventSource::from_log(original_log, {"original.xes", "xes", "xes-bytes"});
  assert_log_equal(xes.original_log(), original_log);
  assert(xes.original_table_origin() == "native_log_to_table");
  assert(xes.representative_row_ordinals()[0][0] == 0);
  assert(xes.representative_row_ordinals()[1][0] == 2);
  assert(xes.checked_log().traces[0].events[0].input_order == 99);
  assert(xes.checked_log().traces.back().events.empty());
  const auto identity = xes.with_filters([](const EventLog& log) { return log; });
  assert_log_equal(identity.checked_log(), original_log);
  assert(identity.checked_table().rows == xes.original_table().rows);
}

void full_snapshots_preserve_raw_states_integer_storage_and_unicode() {
  EventLog log;
  Trace trace;
  trace.case_id = "legacy-value";  // raw missing remains missing in sidecar.
  trace.case_id_source_key = "";
  trace.properties = {{"property", "retained"}};
  Event event;
  event.activity = "legacy-value";
  event.input_order = std::numeric_limits<std::size_t>::max();
  EventAttribute signed_value = integer("signed", 7);
  EventAttribute unsigned_value{"unsigned", "7", AttributeKind::kInt};
  unsigned_value.typed_value = std::uint64_t{7};
  EventAttribute maximum{"maximum", std::to_string(std::numeric_limits<std::uint64_t>::max()), AttributeKind::kInt};
  maximum.typed_value = std::numeric_limits<std::uint64_t>::max();
  EventAttribute negative_zero{"negative-zero", "-0", AttributeKind::kFloat};
  negative_zero.typed_value = -0.0;
  EventAttribute nan{"nan", "NaN", AttributeKind::kFloat};
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  event.attributes = {signed_value, unsigned_value,
      {std::string("nul\0key", 7), std::string("\xed\xa0\x80", 3)},
      maximum, negative_zero, nan};
  EventAttribute nested{"deep", ""};
  nested.container = AttributeContainerKind::kList;
  EventAttribute* cursor = &nested;
  for (unsigned depth = 0; depth < 70; ++depth) {
    cursor->children.push_back({"child", ""});
    cursor = &cursor->children.back();
    cursor->container = AttributeContainerKind::kList;
  }
  event.attributes.push_back(nested);
  event.resource_attribute = EventAttribute{"res", "", AttributeKind::kString, true};
  event.resource_state = FirstClassAttributeState::kNull;
  trace.events.push_back(event);
  log.traces.push_back(trace);
  log.metadata.compatibility_snapshot_valid = true;
  log.metadata.compatibility_globals_snapshot = {{"event", {unsigned_value}}};
  const auto snapshot = serialize_active_source_log_json(log);
  assert(snapshot.find("\"activity_state_index\":0") != std::string::npos);
  assert(snapshot.find("\"case_id_state_index\":0") != std::string::npos);
  assert(snapshot.find("\"activity_state\":\"present\"") != std::string::npos);
  assert(snapshot.find("\"integer_storage\":\"int64\"") != std::string::npos);
  assert(snapshot.find("\"integer_storage\":\"uint64\"") != std::string::npos);
  assert(snapshot.find("\"typed_value_index\":5") != std::string::npos);
  assert(snapshot.find("\"type\":\"float\",\"value\":-0") != std::string::npos);
  assert(snapshot.find("\"type\":\"float\",\"value\":\"NaN\"") != std::string::npos);
  assert(snapshot.find("\"timestamp_attribute\":null") != std::string::npos);
  assert(snapshot.find("\"compatibility_snapshot_valid\":true") != std::string::npos);
  assert(snapshot.find(std::to_string(std::numeric_limits<std::size_t>::max())) != std::string::npos);
  const std::string escaped_surrogate = R"(\ud800)";
  assert(snapshot.find(escaped_surrogate) != std::string::npos);
  assert(snapshot.find(std::string("\xed\xa0\x80", 3)) == std::string::npos);
  auto changed = log;
  changed.traces[0].events[0].activity_state = FirstClassAttributeState::kPresent;
  assert(serialize_active_source_log_json(changed) != snapshot);
  const auto stream = event_log_to_event_stream(log);
  const auto stream_snapshot = serialize_active_source_stream_json(stream);
  assert(stream_snapshot.find("\"fields\":{\"carrier\":") != std::string::npos);
  assert(stream_snapshot.find("\"integer_storage\":\"uint64\"") != std::string::npos);
  assert(stream_snapshot.find(escaped_surrogate) != std::string::npos);
  log.traces[0].events[0].attributes[1].first_class_source_key = "stored-even-with-none-role";
  const auto wider_native_storage = serialize_active_source_log_json(log);
  assert(wider_native_storage.find("stored-even-with-none-role") != std::string::npos);
  assert(wider_native_storage.find("\"carrier\":{") != std::string::npos);
  assert(wider_native_storage.find("\"first_class_role_index\":0,\"first_class_state_index\":0,\"first_class_source_key\":\"stored-even-with-none-role\"") != std::string::npos);
  assert(wider_native_storage.find("\"type\":\"uint64\",\"value\":7") != std::string::npos);
}

EventLog analysis_log() {
  EventLog log;
  for (std::size_t case_index = 0; case_index < 2; ++case_index) {
    Trace trace;
    trace.case_id = "case-" + std::to_string(case_index);
    for (std::size_t event_index = 0; event_index < 3; ++event_index) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = event_index == 1 ? "B" : "A";
      event.timestamp = "2026-01-01T00:00:0" + std::to_string(event_index + 3 * case_index) + "Z";
      event.resource = "alice";
      event.input_order = event_index;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void full_result_writers_cover_live_native_flat_consumers() {
  const auto log = analysis_log();
  const auto source = ActiveEventSource::from_table(event_log_to_table(log), {"native.json", "table", "raw"});
  for (const auto kind : {EventDataKind::kEventLog, EventDataKind::kEventStream,
                          EventDataKind::kEventTable, EventDataKind::kLazyEventTable}) {
    const auto view = source.view(kind);
    const auto skeleton = discover_log_skeleton(view);
    const auto skeleton_conformance = check_log_skeleton_conformance(view, skeleton);
    assert(!skeleton.activ_freq.empty());
    const auto skeleton_json = serialize_active_source_skeleton_json(skeleton, &skeleton_conformance);
    assert(skeleton_json.find("\"allowed_frequencies\":") != std::string::npos ||
           skeleton_json.find("\"deviations\":[]") != std::string::npos);
    assert(skeleton_json.find("\"dev_fitness\":1") != std::string::npos);
    const auto declare_model = discover_declare(view);
    const auto declare_conformance = check_declare_conformance(view, declare_model);
    const auto declare_json = serialize_active_source_declare_json(declare_model, &declare_conformance);
    assert(declare_json.find("\"rules\":") != std::string::npos);
    assert(declare_json.find("\"support\":") != std::string::npos);
    TransitionSystemOptions transition_options;
    transition_options.include_data = true;
    const auto transitions = discover_transition_system(view, transition_options);
    const auto transition_json = serialize_active_source_transition_json(transitions);
    assert(!transitions.states().empty());
    assert(transition_json.find("\"incoming_occurrences\":") != std::string::npos);
    assert(transition_json.find("\"name_order\":") != std::string::npos);
    const auto distances = discover_minimum_self_distances(view);
    const auto distance_json = serialize_active_source_distances_json(distances);
    assert(distance_json.find("\"activity\":\"A\"") != std::string::npos);
    CorrelationVariantsAnalysis correlation;
    correlation.variants.push_back({"native", discover_correlation_miner(view)});
    const auto correlation_json = serialize_active_source_correlation_json(correlation);
    assert(correlation_json.find("\"duration_matrix\":") != std::string::npos);
    assert(correlation_json.find("\"precede_succeed_probability\":") != std::string::npos);
    const auto spectrum = compute_performance_spectrum(view, {"A", "B"});
    const auto spectrum_json = serialize_active_source_spectrum_json(spectrum);
    assert(!spectrum.observations.empty());
    assert(spectrum_json.find("\"pattern_offset\":") != std::string::npos);
    const auto temporal = discover_temporal_profile(view);
    const auto temporal_json = serialize_active_source_temporal_profile_json(temporal);
    assert(!temporal.entries.empty());
    assert(temporal_json.find("\"count\":") != std::string::npos);
    const auto temporal_conformance = conform_temporal_profile(view, temporal);
    assert(temporal_conformance.size() == source.checked_log().traces.size());
    assert(serialize_active_source_temporal_conformance_json(temporal_conformance) == "[[],[]]");
  }
  for (const auto* table : {&source.checked_table(), &source.original_table()}) {
    const auto batches = discover_batches(*table);
    const auto json = serialize_active_source_batches_json(batches);
    assert(!batches.groups.empty());
    assert(json.find("\"event_id_attribute\":\"\"") != std::string::npos);
    assert(json.find("\"event_id\":\"\"") != std::string::npos);
  }
  assert(!serialize_active_source_batches_json(discover_batches(source.checked_lazy())).empty());
  assert(!serialize_active_source_batches_json(discover_batches(source.checked_log())).empty());
}

void complete_result_writers_preserve_extreme_stored_values() {
  const auto maximum = std::numeric_limits<std::size_t>::max();
  const std::string wtf("\xed\xa0\x80", 3);
  const std::string escaped_wtf = R"(\ud800)";
  LogSkeletonModel skeleton;
  skeleton.equivalence = {{"", wtf}};
  skeleton.always_after = {{"A", "B"}};
  skeleton.always_before = {{"B", "A"}};
  skeleton.never_together = {{"unused", ""}};
  skeleton.directly_follows = {{"A", "A"}};
  skeleton.activ_freq[wtf] = {0, maximum};
  std::vector<LogSkeletonConformance> skeleton_conformance{{
      {{"activ_freq", {"unused-relation", ""}, wtf, maximum, {0, maximum}}}, maximum, maximum,
      std::numeric_limits<double>::quiet_NaN(), false}};
  const auto skeleton_json = serialize_active_source_skeleton_json(skeleton, &skeleton_conformance);
  assert(skeleton_json.find(std::to_string(maximum)) != std::string::npos);
  assert(skeleton_json.find(escaped_wtf) != std::string::npos);
  assert(skeleton_json.find("\"type\":\"float\",\"value\":\"NaN\"") != std::string::npos);
  assert(serialize_active_source_skeleton_json(skeleton).find("\"conformance\":null") != std::string::npos);
  const std::vector<LogSkeletonConformance> empty;
  assert(serialize_active_source_skeleton_json(skeleton, &empty).find("\"conformance\":[]") != std::string::npos);

  DeclareModel declare_model;
  declare_model.rules.clear();
  declare_model.rules[wtf] = {{"", "latent-second", false, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}};
  std::vector<DeclareConformance> declare_conformance{{{{wtf, "", "latent-second", false}},
      std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), -0.0, false}};
  const auto declare_json = serialize_active_source_declare_json(declare_model, &declare_conformance);
  assert(declare_json.find("\"support\":" + std::to_string(std::numeric_limits<int>::min())) != std::string::npos);
  assert(declare_json.find("\"second\":\"latent-second\"") != std::string::npos);
  assert(declare_json.find("\"dev_fitness\":-0") != std::string::npos);
  assert(serialize_active_source_declare_json(declare_model).find("\"conformance\":null") != std::string::npos);

  TransitionSystemOptions transition_options;
  transition_options.direction = static_cast<TransitionDirection>(-8);
  transition_options.view = TransitionView::kMultiset;
  transition_options.window = std::numeric_limits<std::int64_t>::min();
  transition_options.activity_attribute = " ";
  TransitionSystemState state;
  state.id = maximum;
  state.payload = {std::nullopt, "", wtf, wtf};
  state.name_order = {wtf, std::nullopt, ""};
  state.start_count = maximum;
  state.end_count = maximum;
  state.incoming_occurrences = {{maximum, maximum}};
  state.outgoing_occurrences = {{0, maximum}};
  state.in_root = false;
  state.incoming = {maximum, 0};
  state.outgoing = {0, maximum};
  const TransitionSystem transitions(transition_options, {state}, {{maximum, 0, std::nullopt, maximum, {{maximum, 0}}}});
  const auto transition_json = serialize_active_source_transition_json(transitions);
  assert(transition_json.find("\"direction\":\"UNKNOWN\",\"direction_index\":-8") != std::string::npos);
  assert(transition_json.find("\"label\":null") != std::string::npos);
  assert(transition_json.find("\"in_root\":false") != std::string::npos);
  assert(transition_json.find("\"payload\":[null,\"\"") != std::string::npos);
  const auto distances_json = serialize_active_source_distances_json({{"", std::numeric_limits<std::uint64_t>::max(), {"", wtf}, true, true}});
  assert(distances_json.find("\"activity\":\"\"") != std::string::npos);
  assert(distances_json.find("\"activity_is_null\":true") != std::string::npos);

  CorrelationMinerResult correlation;
  correlation.variant = static_cast<CorrelationVariant>(37);
  correlation.activity_counts[wtf] = std::numeric_limits<std::uint64_t>::max();
  correlation.frequency_dfg[{"a:b", "c"}] = 0;
  correlation.performance_dfg[{"a", "b:c"}] = std::numeric_limits<double>::infinity();
  correlation.precede_succeed_probability[{"", wtf}] = -0.0;
  correlation.duration_matrix[{wtf, ""}] = std::nextafter(1.0, 2.0);
  const auto correlation_json = serialize_active_source_correlation_json({{{" ", correlation}}});
  assert(correlation_json.find("\"variant\":\"UNKNOWN\",\"variant_index\":37") != std::string::npos);
  assert(correlation_json.find("\"value\":\"INF\"") != std::string::npos);
  assert(correlation_json.find("1.0000000000000002") != std::string::npos);
  assert(correlation_json.find("\"source\":\"a:b\",\"target\":\"c\",\"count\":0") != std::string::npos);
  PerformanceSpectrum spectrum;
  spectrum.activities = {"", wtf};
  spectrum.observations = {{wtf, {-0.0, -std::numeric_limits<double>::infinity()}, {"", wtf}, maximum}};
  spectrum.variant = PerformanceSpectrumVariant::kLazyframeDisconnected;
  spectrum.activity_attribute = "";
  const auto spectrum_json = serialize_active_source_spectrum_json(spectrum);
  assert(spectrum_json.find("\"variant\":\"LAZYFRAME_DISCONNECTED\",\"variant_index\":7") != std::string::npos);
  assert(spectrum_json.find("\"value\":\"-INF\"") != std::string::npos);

  BatchDiscovery batches;
  batches.options.variant = BatchDiscoveryVariant::kPolars;
  batches.options.merge_distance_seconds = -0.0;
  batches.options.min_batch_size = maximum;
  batches.options.event_id_attribute = "";
  batches.variant = static_cast<BatchDiscoveryVariant>(-9);
  Batch batch;
  batch.type = static_cast<BatchType>(42);
  batch.start = std::nextafter(1.0, 2.0);
  batch.complete = std::numeric_limits<double>::quiet_NaN();
  batch.events = {{-0.0, std::numeric_limits<double>::infinity(), wtf, maximum, maximum, ""}};
  batches.groups = {{"", wtf, maximum, maximum, {batch}}};
  const auto batch_json = serialize_active_source_batches_json(batches);
  assert(batch_json.find("\"variant\":\"POLARS\",\"variant_index\":2") != std::string::npos);
  assert(batch_json.find("\"variant\":\"UNKNOWN\",\"variant_index\":-9") != std::string::npos);
  assert(batch_json.find("\"type\":\"UNKNOWN\",\"type_index\":42") != std::string::npos);
  assert(batch_json.find("\"event_id\":\"\"") != std::string::npos);
  assert(batch_json.find(escaped_wtf) != std::string::npos);
  const TemporalProfile temporal{{{wtf, "", std::numeric_limits<std::uint64_t>::max(),
      std::nextafter(1.0, 2.0), -0.0}}};
  const auto temporal_json = serialize_active_source_temporal_profile_json(temporal);
  assert(temporal_json.find("1.0000000000000002") != std::string::npos);
  assert(temporal_json.find("\"standard_deviation_seconds\":-0") != std::string::npos);
  const TemporalConformanceResult temporal_conformance{{}, {{wtf, "", " ",
      std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}}, {}};
  const auto temporal_conformance_json = serialize_active_source_temporal_conformance_json(temporal_conformance);
  assert(temporal_conformance_json.starts_with("[[],[{"));
  assert(temporal_conformance_json.ends_with("}],[]]"));
  assert(temporal_conformance_json.find("\"value\":\"INF\"") != std::string::npos);
  assert(temporal_conformance_json.find("\"value\":\"NaN\"") != std::string::npos);
}


// An XES source on pm4py's DataFrame path: its log holds the frame's cases and
// its table the frame's rows (file order, each row's own trace cells), and a
// filtered source keeps projecting those rows in file order.
void xes_frame_source_keeps_frame_rows() {
  std::istringstream input(
      "<log>"
      "<trace><string key=\"concept:name\" value=\"9\"/><string key=\"k\" value=\"first\"/>"
      "<event><string key=\"concept:name\" value=\"a\"/></event></trace>"
      "<trace><string key=\"concept:name\" value=\"10\"/>"
      "<event><string key=\"concept:name\" value=\"b\"/></event></trace>"
      "<trace><string key=\"concept:name\" value=\"empty\"/></trace>"
      "<trace><string key=\"concept:name\" value=\"9\"/><string key=\"k\" value=\"second\"/>"
      "<event><string key=\"concept:name\" value=\"c\"/></event></trace>"
      "</log>");
  const auto source = ActiveEventSource::from_xes_frame(
      import_xes_event_log(input), {"frame.xes", "xes", ""},
      {"case:concept:name", "concept:name", "time:timestamp", "org:resource", "lifecycle:transition"});
  const auto& log = source.checked_log();
  assert(log.traces.size() == 2 && log.traces[0].case_id == "9" && log.traces[0].events.size() == 2);
  const auto& table = source.checked_table();
  assert(table.rows.size() == 3);
  const auto column = [&](std::string_view name) {
    return static_cast<std::size_t>(std::find(table.columns.begin(), table.columns.end(), name) -
                                    table.columns.begin());
  };
  const auto value = [&](const EventTable& rows, std::size_t row, std::string_view name) {
    const auto& cell = rows.rows[row][column(name)];
    return cell && !cell->is_null ? cell->value : std::string("<null>");
  };
  assert(value(table, 0, "concept:name") == "a" && value(table, 1, "concept:name") == "b" &&
         value(table, 2, "concept:name") == "c");
  assert(value(table, 0, "case:k") == "first" && value(table, 2, "case:k") == "second");
  assert((source.representative_row_ordinals() ==
          ActiveEventSource::RepresentativeRows{{0, 2}, {1}}));
  LogFilterOptions keep;
  keep.case_attribute_key = "k";
  keep.keep_case_attribute_values = {"second"};
  const auto filtered = source.with_filters(keep);
  assert(filtered.checked_log().traces.size() == 1);
  const auto& rows = filtered.checked_table();
  assert(rows.rows.size() == 2 && value(rows, 0, "concept:name") == "a" &&
         value(rows, 1, "concept:name") == "c" && value(rows, 1, "case:k") == "second");
  // The table's groups by case id meet 9 before 10; pm4py lists 10 first.
  LogSkeletonOptions keys;
  assert((log_skeleton_case_order(source.view(EventDataKind::kEventTable), keys) ==
          std::vector<std::size_t>{1, 0}));
  assert((log_skeleton_case_order(source.view(EventDataKind::kEventLog), keys) ==
          std::vector<std::size_t>{1, 0}));
}
}  // namespace

int main() {
  xes_frame_source_keeps_frame_rows();
  original_and_owned_views_preserve_complete_source();
  log_preparation_failure_does_not_discard_flat_input();
  selection_preserves_original_cells_and_multi_index();
  native_grouping_preserves_representative_and_input_order();
  case_and_attribute_mutations_overlay_only_changed_cells();
  native_case_split_overlays_actual_trace_ids();
  duplicate_occurrences_keep_original_range_index_labels();
  marker_loss_retains_log_and_original_but_never_falls_back();
  paired_csv_and_original_grouped_log_remain_native();
  full_snapshots_preserve_raw_states_integer_storage_and_unicode();
  full_result_writers_cover_live_native_flat_consumers();
  complete_result_writers_preserve_extreme_stored_values();
  return 0;
}
