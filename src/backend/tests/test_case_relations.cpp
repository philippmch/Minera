#undef NDEBUG

#include "minera/case_relations.hpp"
#include "../tools/case_relations_typed_input.hpp"
#include "minera/json_export.hpp"
#include "case_relations_dtype_reference_cases.hpp"
#include "case_relations_index_reference_cases.hpp"
#include "case_relations_reference_cases.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using namespace minera;

Event event(std::string case_id, std::string activity, std::string timestamp) {
  Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  return result;
}

EventLog base_log() {
  Trace trace;
  trace.case_id = "L1";
  trace.events = {event("L1", "A", "2024-01-01T00:00:00Z")};
  return EventLog{{trace}};
}

EventLog incoming_log() {
  Trace trace;
  trace.case_id = "R1";
  trace.events = {event("R1", "B", "2024-01-01T00:00:01Z"),
                  event("R1", "C", "2024-01-01T00:00:02Z")};
  return EventLog{{trace}};
}

void merges_and_drops() {
  const auto merged = merge_case_relations(base_log(), incoming_log(),
                                           {{"L1", "R1"}, {"L2", "R1"}});
  assert(merged.traces.size() == 2);
  assert(merged.traces[0].case_id == "L1");
  assert(merged.traces[0].events.size() == 3);
  assert(merged.traces[0].events[1].case_id == "L1");
  assert(merged.traces[1].case_id == "L2");
  assert(merged.traces[1].events.size() == 2);
  const auto dropped = merge_case_relations(base_log(), incoming_log(), {});
  assert(dropped.traces.size() == 1 && dropped.traces[0].events.size() == 1);
}

void parses_relations_and_custom_timestamps() {
  std::istringstream input("case_id_LEFT,case_id_RIGHT\nL1,R1\nL1,R1\n");
  const auto relations = parse_case_relations_csv(input);
  assert((relations.size() == 2 &&
          relations[0] == CaseRelation{"L1", "R1"}));
  std::istringstream quoted("case_id_LEFT,case_id_RIGHT\n\"L,1\",\"R\"\"1\"\n");
  const auto quoted_relations = parse_case_relations_csv(quoted);
  assert((quoted_relations.size() == 1 &&
          quoted_relations[0] == CaseRelation{"L,1", "R\"1"}));
  auto base = base_log();
  auto incoming = incoming_log();
  for (auto& event : base.traces[0].events)
    event.attributes.push_back({"started", event.timestamp, AttributeKind::kDate});
  for (auto& event : incoming.traces[0].events) {
    event.attributes.push_back({"started", event.timestamp, AttributeKind::kDate});
  }
  CaseRelationsOptions options;
  options.timestamp_key = "started";
  const auto merged = merge_case_relations(base, incoming, {{"L1", "R1"}}, options);
  assert(merged.traces[0].events.size() == 3);
  bool threw = false;
  try {
    std::istringstream bad("case_id_LEFT\nL1\n");
    (void)parse_case_relations_csv(bad);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  for (const auto& malformed : {
           std::string("case_id_LEFT,case_id_RIGHT\n\"L1\"junk,R1\n"),
           std::string("case_id_LEFT,case_id_RIGHT\nL\"1,R1\n"),
           std::string("case_id_LEFT,case_id_LEFT\nL1,R1\n")}) {
    threw = false;
    try {
      std::istringstream bad(malformed);
      (void)parse_case_relations_csv(bad);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void rejects_invalid_timestamps() {
  auto base = base_log();
  base.traces[0].events[0].timestamp = "invalid";
  bool threw = false;
  try {
    (void)merge_case_relations(base, {}, {});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  base.traces[0].events[0].timestamp.clear();
  base.traces[0].events[0].timestamp_state = FirstClassAttributeState::kPresent;
  threw = false;
  try { (void)merge_case_relations(base, {}, {}); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);  // Explicit empty text is not a missing datetime cell.
}

void missing_timestamps_sort_last_with_stable_duplicates() {
  for (const int selector : {0, 1, 2}) {
    for (const bool all_null : {false, true}) {
      auto base = base_log();
      base.traces[0].events = {
          event("L1", "base-null", "stale-null-projection"),
          event("L1", "base2", "2024-01-01T00:00:00.000000002Z"),
          event("L1", "base-missing", "")};
      base.traces[0].events[0].timestamp_state = FirstClassAttributeState::kNull;
      auto incoming = incoming_log();
      incoming.traces[0].events = {
          event("R1", "incoming-null", ""),
          event("R1", "incoming1", "2024-01-01T00:00:00.000000001Z")};
      incoming.traces[0].events[0].timestamp_state = FirstClassAttributeState::kNull;
      CaseRelationsOptions options;
      if (selector) options.timestamp_key = "started";
      for (auto* log : {&base, &incoming}) {
        for (auto& value : log->traces[0].events) {
          if (all_null) value.timestamp_state = FirstClassAttributeState::kNull;
          if (!selector) continue;
          if (selector == 2) value.timestamp_source_key = "started";
          if (!value.has_timestamp()) {
            if (selector == 1) value.timestamp = "unused";
            continue;
          }
          EventAttribute attribute{"started", "stale-date-projection", AttributeKind::kDate};
          attribute.is_null = value.timestamp_state == FirstClassAttributeState::kNull;
          if (!attribute.is_null)
            attribute.typed_value = NativeDate{value.timestamp, NativeDateResolution::kNanosecond};
          if (selector == 1) value.attributes.push_back(attribute);
          else value.timestamp_attribute = attribute;
          value.timestamp = "unused";
        }
      }
      const auto base_before = case_relations_json(base);
      const auto incoming_before = case_relations_json(incoming);
      const auto merged = merge_case_relations(base, incoming,
          {{"L1", "R1"}, {"L1", "R1"}}, options);
      const std::vector<std::string> expected = all_null
          ? std::vector<std::string>{"base-null", "base2", "base-missing", "incoming-null", "incoming-null", "incoming1", "incoming1"}
          : std::vector<std::string>{"incoming1", "incoming1", "base2", "base-null", "base-missing", "incoming-null", "incoming-null"};
      assert(merged.traces[0].events.size() == expected.size());
      for (std::size_t i = 0; i < expected.size(); ++i)
        assert(merged.traces[0].events[i].activity == expected[i]);
      assert(case_relations_json(base) == base_before);
      assert(case_relations_json(incoming) == incoming_before);
    }
  }
}

void supports_custom_case_key_and_suffixes() {
  auto base = base_log();
  base.traces[0].case_id = "base-raw";
  base.traces[0].attributes.push_back(
      {"order_id", "L1", AttributeKind::kString});
  for (auto& source_event : base.traces[0].events)
    source_event.case_id = base.traces[0].case_id;

  auto incoming = incoming_log();
  incoming.traces[0].case_id = "incoming-raw";
  incoming.traces[0].attributes.push_back(
      {"order_id", "R1", AttributeKind::kString});
  for (auto& source_event : incoming.traces[0].events)
    source_event.case_id = incoming.traces[0].case_id;

  CaseRelationsOptions options;
  options.case_id_key = "order_id";
  options.left_suffix = "__left";
  options.right_suffix = "__right";
  const auto merged = merge_case_relations(
      base, incoming, {{"L1", "R1"}}, options);
  assert(merged.traces.size() == 1 && merged.traces[0].case_id == "L1");
  assert(merged.traces[0].events.size() == 3);

  auto missing = incoming;
  missing.traces[0].attributes.clear();
  bool threw = false;
  try {
    (void)merge_case_relations(base, missing, {{"L1", "R1"}}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  options.right_suffix = options.left_suffix;
  threw = false;
  try {
    (void)merge_case_relations(base, incoming, {{"L1", "R1"}}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void event_log_selectors_ignore_unused_suffixes() {
  CaseRelationsOptions options;
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  options.left_suffix.clear();
  options.right_suffix.clear();
  const auto explicit_empty = merge_case_relations(
      base_log(), incoming_log(), { {"L1", "R1"} }, options);
  assert(explicit_empty.traces.size() == 1 &&
         explicit_empty.traces[0].events.size() == 3);

  options.left_suffix = "same-unused-suffix";
  options.right_suffix = options.left_suffix;
  const auto explicit_equal = merge_case_relations(
      base_log(), incoming_log(), { {"L1", "R1"} }, options);
  assert(explicit_equal.traces.size() == 1 &&
         explicit_equal.traces[0].events.size() == 3);

  options.relation_right_column = options.relation_left_column;
  bool threw = false;
  try {
    (void)merge_case_relations(base_log(), incoming_log(),
                               { {"L1", "R1"} }, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  options.relation_left_column.clear();
  options.relation_right_column = "rhs";
  options.left_suffix.clear();
  threw = false;
  try {
    (void)merge_case_relations(base_log(), incoming_log(),
                               { {"L1", "R1"} }, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void json_shape_is_stable() {
  const auto merged = merge_case_relations(base_log(), incoming_log(),
                                           {{"L1", "R1"}});
  const auto json = case_relations_json(merged);
  assert(json.find(R"({"family":"MERGE_CASE_RELATIONS","variant":"pandas","log":)" ) !=
         std::string::npos);
  assert(json.find(R"("case_id":"L1")") != std::string::npos);
  assert(json.find(R"("activity":"B")") != std::string::npos);
}

void nanosecond_ordering_and_exact_ties_match_public_merge() {
  for (const std::string prefix : {"2024-01-01T00:00:00", "1969-12-31T23:59:59"}) {
    for (const bool custom_key : {false, true}) {
      auto base = base_log();
      base.traces[0].events = {
          event("L1", "base1", prefix + ".000000001Z"),
          event("L1", "base0", prefix + ".000000000Z"),
          event("L1", "base-last", prefix + ".999999999Z")};
      auto incoming = incoming_log();
      incoming.traces[0].events = {
          event("R1", "incoming2", prefix + ".000000002Z"),
          event("R1", "incoming1", prefix + ".000000001+00:00")};
      CaseRelationsOptions options;
      if (custom_key) {
        options.timestamp_key = "started";
        for (auto* log : {&base, &incoming}) {
          for (auto& value : log->traces[0].events) {
            value.attributes.push_back({"started", value.timestamp, AttributeKind::kDate});
            value.timestamp = "unused";
          }
        }
      }
      const auto merged = merge_case_relations(base, incoming,
          {{"L1", "R1"}, {"L1", "R1"}}, options);
      const std::vector<std::string> expected{
          "base0", "base1", "incoming1", "incoming1", "incoming2", "incoming2", "base-last"};
      assert(merged.traces.size() == 1);
      assert(merged.traces[0].events.size() == expected.size());
      for (std::size_t i = 0; i < expected.size(); ++i)
        assert(merged.traces[0].events[i].activity == expected[i]);
      assert(base.traces[0].events[0].activity == "base1");
    }
  }
  auto base = base_log();
  base.traces[0].events = {event("L1", "late", "2024-01-01T00:00:00.000000002Z")};
  auto incoming = incoming_log();
  incoming.traces[0].events = {
      event("R1", "early", "2024-01-01T05:30:00.000000001+05:30"),
      event("R1", "tie", "2023-12-31T20:00:00.000000002-04:00")};
  const auto merged = merge_case_relations(base, incoming, {{"L1", "R1"}});
  assert(merged.traces[0].events[0].activity == "early");
  assert(merged.traces[0].events[1].activity == "late");
  assert(merged.traces[0].events[2].activity == "tie");
}

void table_vector_adapter_preserves_inputs() {
  EventTable base{
      {"case_id", "when", "base_value"},
      {{EventAttribute{"case_id", "L1", AttributeKind::kString},
        EventAttribute{"when", "a", AttributeKind::kString},
        EventAttribute{"base_value", "base", AttributeKind::kString}}}};
  EventTable incoming{
      {"case_id", "when", "incoming_value"},
      {{EventAttribute{"case_id", "R1", AttributeKind::kString},
        EventAttribute{"when", "b", AttributeKind::kString},
        EventAttribute{"incoming_value", "incoming", AttributeKind::kString}}}};
  const auto base_before = base;
  const auto incoming_before = incoming;
  CaseRelationsOptions options;
  options.timestamp_key = "when";
  const auto merged = merge_case_relations(
      base, incoming, std::vector<CaseRelation>{{"L1", "R1"}}, options);
  assert((merged.columns == std::vector<std::string>{
      "case_id", "when", "base_value", "incoming_value", "case_id_LEFT",
      "case_id_RIGHT"}));
  assert(merged.rows.size() == 2);
  assert(merged.rows[0][0] && merged.rows[0][0]->value == "L1");
  assert(merged.rows[1][0] && merged.rows[1][0]->value == "L1");
  assert(!merged.rows[0][4] && !merged.rows[0][5]);
  assert(merged.rows[1][4] && merged.rows[1][4]->value == "L1");
  assert(merged.rows[1][5] && merged.rows[1][5]->value == "R1");
  assert(base.columns == base_before.columns && base.rows == base_before.rows);
  assert(incoming.columns == incoming_before.columns &&
         incoming.rows == incoming_before.rows);

  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto renamed = merge_case_relations(
      base, incoming, std::vector<CaseRelation>{{"L1", "R1"}}, options);
  assert(renamed.columns.size() == 6 && renamed.columns[4] == "lhs" &&
         renamed.columns[5] == "rhs");
}

EventTable table_for_index_metadata() {
  EventTable table{
      {"case:concept:name", "concept:name", "when"},
      {{EventAttribute{"case:concept:name", "L2", AttributeKind::kString},
        EventAttribute{"concept:name", "b2", AttributeKind::kString},
        EventAttribute{"when", "2024-01-01T00:00:00Z", AttributeKind::kString}},
       {EventAttribute{"case:concept:name", "L1", AttributeKind::kString},
        EventAttribute{"concept:name", "b1-late", AttributeKind::kString},
        EventAttribute{"when", "2024-01-01T00:00:00.000000002Z",
                       AttributeKind::kString}},
       {EventAttribute{"case:concept:name", "L1", AttributeKind::kString},
        EventAttribute{"concept:name", "b1-early", AttributeKind::kString},
        EventAttribute{"when", "2024-01-01T00:00:00.000000001Z",
                       AttributeKind::kString}}}};
  return table;
}

EventTable incoming_for_index_metadata() {
  EventTable table{
      {"case:concept:name", "concept:name", "when"},
      {{EventAttribute{"case:concept:name", "R1", AttributeKind::kString},
        EventAttribute{"concept:name", "i1", AttributeKind::kString},
        EventAttribute{"when", "2024-01-01T00:00:00Z", AttributeKind::kString}},
       {EventAttribute{"case:concept:name", "R2", AttributeKind::kString},
        EventAttribute{"concept:name", "i2", AttributeKind::kString},
        EventAttribute{"when", "2024-01-01T00:00:00.000000003Z",
                       AttributeKind::kString}}}};
  return table;
}

void implicit_index_metadata_is_reset_or_carried_as_expected() {
  auto base = table_for_index_metadata();
  auto incoming = incoming_for_index_metadata();
  EventTable relations{
      {"case:concept:name_LEFT", "case:concept:name_RIGHT"},
      {{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString},
        EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};

  CaseRelationsOptions options;
  options.case_id_key = "case:concept:name";
  options.timestamp_key = "when";
  const auto sorted_without_matches = merge_case_relations(
      base, incoming, EventTable{{"case:concept:name_LEFT",
                                  "case:concept:name_RIGHT"}, {}}, options);
  assert(sorted_without_matches.index);
  assert(sorted_without_matches.index->kind == "RangeIndex");
  assert(sorted_without_matches.index->dtype == "int64");
  assert(sorted_without_matches.rows.size() == 3);
  assert(std::get<EventAttribute>(sorted_without_matches.index->labels[0].value)
             .typed_value == NativeAttributeValue{static_cast<std::int64_t>(2)});
  assert(std::get<EventAttribute>(sorted_without_matches.index->labels[1].value)
             .typed_value == NativeAttributeValue{static_cast<std::int64_t>(1)});
  assert(std::get<EventAttribute>(sorted_without_matches.index->labels[2].value)
             .typed_value == NativeAttributeValue{static_cast<std::int64_t>(0)});

  const auto expanded = merge_case_relations(base, incoming, relations, options);
  assert(expanded.index);
  assert(expanded.index->kind == "Index");
  assert(expanded.index->dtype == "int64");
  assert(expanded.index->names == std::vector<std::optional<std::string>>{std::nullopt});
  assert(expanded.index->labels.size() == expanded.rows.size());
  assert(std::get<EventAttribute>(expanded.index->labels[0].value).typed_value ==
         NativeAttributeValue{static_cast<std::int64_t>(2)});
  assert(std::get<EventAttribute>(expanded.index->labels[1].value).typed_value ==
         NativeAttributeValue{static_cast<std::int64_t>(1)});
  // These timestamps are strings: fractional text sorts before the final Z.
  assert(std::get<EventAttribute>(expanded.index->labels[2].value).typed_value ==
         NativeAttributeValue{static_cast<std::int64_t>(0)});
}

void range_index_extrema_do_not_overflow_classification() {
  EventTable base{
      {"case:concept:name", "concept:name", "when"},
      {{EventAttribute{"case:concept:name", "L1", AttributeKind::kString},
        EventAttribute{"concept:name", "first", AttributeKind::kString},
        EventAttribute{"when", "a", AttributeKind::kString}},
       {EventAttribute{"case:concept:name", "L1", AttributeKind::kString},
        EventAttribute{"concept:name", "last", AttributeKind::kString},
        EventAttribute{"when", "b", AttributeKind::kString}}}};
  EventAttribute minimum{"", std::to_string(std::numeric_limits<std::int64_t>::min()),
                          AttributeKind::kInt};
  minimum.typed_value = std::numeric_limits<std::int64_t>::min();
  EventAttribute maximum{"", std::to_string(std::numeric_limits<std::int64_t>::max()),
                          AttributeKind::kInt};
  maximum.typed_value = std::numeric_limits<std::int64_t>::max();
  base.index = EventTableIndex{
      "RangeIndex", "int64", {std::nullopt},
      {EventTableIndexLabel{minimum}, EventTableIndexLabel{maximum}}};
  EventTable incoming{{"case:concept:name", "concept:name", "when"}, {}};
  EventTable relations{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {}};
  CaseRelationsOptions options;
  options.case_id_key = "case:concept:name";
  options.timestamp_key = "when";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.index && merged.index->kind == "RangeIndex");
  assert(merged.index->labels == base.index->labels);
}

void table_selectors_are_exact() {
  const auto event_table = [](const std::string& case_column,
                              const std::string& case_value,
                              const std::string& timestamp_column,
                              const std::string& timestamp_value) {
    EventTable table;
    table.columns = {case_column, timestamp_column};
    table.rows.push_back({
        EventAttribute{case_column, case_value, AttributeKind::kString},
        EventAttribute{timestamp_column, timestamp_value,
                       AttributeKind::kString}});
    return table;
  };
  const auto relation_table = [](const std::string& left_column,
                                 const std::string& right_column) {
    EventTable table;
    table.columns = {left_column, right_column};
    table.rows.push_back({
        EventAttribute{left_column, "L1", AttributeKind::kString},
        EventAttribute{right_column, "R1", AttributeKind::kString}});
    return table;
  };
  const auto expect_rejected = [](const EventTable& base,
                                  const EventTable& incoming,
                                  const EventTable& relations,
                                  const CaseRelationsOptions& options) {
    bool threw = false;
    try {
      (void)merge_case_relations(base, incoming, relations, options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };

  // A selected time:timestamp column does not fall back to timestamp.
  const auto timestamp_alias_base =
      event_table("case_id", "L1", "timestamp", "a");
  const auto timestamp_alias_incoming =
      event_table("case_id", "R1", "timestamp", "b");
  const auto exact_relations = relation_table("case_id_LEFT", "case_id_RIGHT");
  expect_rejected(timestamp_alias_base, timestamp_alias_incoming,
                  exact_relations, {});

  // A selected case_id column does not fall back to case:concept:name.
  const auto case_alias_base =
      event_table("case:concept:name", "L1", "time:timestamp", "a");
  const auto case_alias_incoming =
      event_table("case:concept:name", "R1", "time:timestamp", "b");
  expect_rejected(case_alias_base, case_alias_incoming, exact_relations, {});

  // Relation columns use the configured case key verbatim as well.
  const auto exact_base =
      event_table("case_id", "L1", "time:timestamp", "a");
  const auto exact_incoming =
      event_table("case_id", "R1", "time:timestamp", "b");
  const auto relation_alias =
      relation_table("case:concept:name_LEFT", "case:concept:name_RIGHT");
  expect_rejected(exact_base, exact_incoming, relation_alias, {});

  // The default case_id/time:timestamp names remain valid when present
  // exactly, with no alias translation on the table path.
  const auto merged = merge_case_relations(
      exact_base, exact_incoming, exact_relations, {});
  assert(merged.rows.size() == 2);
  assert((merged.columns == std::vector<std::string>{
      "case_id", "time:timestamp", "case_id_LEFT", "case_id_RIGHT"}));
}

void parses_relation_table_with_extra_columns() {
  std::istringstream input(
      "lhs,rhs,relation note,empty\n"
      "L1,R1,\"quoted, \"\"note\"\"\",\n");
  const auto relations = parse_case_relations_table_csv(input);
  assert((relations.columns ==
          std::vector<std::string>{"lhs", "rhs", "relation note", "empty"}));
  assert(relations.rows.size() == 1 && relations.rows[0].size() == 4);
  assert(relations.rows[0][2] &&
         relations.rows[0][2]->value == "quoted, \"note\"");
  assert(relations.rows[0][3] && relations.rows[0][3]->value.empty() &&
         !relations.rows[0][3]->is_null &&
         relations.rows[0][3]->kind == AttributeKind::kString);
  std::istringstream crlf("lhs,rhs,note\r\nL1,R1,\"quoted, note\"\r\n");
  const auto uploaded = parse_case_relations_table_csv(crlf);
  assert(uploaded.rows[0][2]->value == "quoted, note");
  assert((uploaded.dtypes == std::vector<std::string>{"str", "str", "str"}));
  std::istringstream legacy_crlf("case_id_LEFT,case_id_RIGHT\r\n\"L1\",\"R1\"\r\n");
  assert((parse_case_relations_csv(legacy_crlf) ==
          std::vector<CaseRelation>{{"L1", "R1"}}));

  EventTable base{
      {"case_id", "time:timestamp", "base_value"},
      {{EventAttribute{"case_id", "L1", AttributeKind::kString},
        EventAttribute{"time:timestamp", "a", AttributeKind::kString},
        EventAttribute{"base_value", "base", AttributeKind::kString}}}};
  EventTable incoming{
      {"case_id", "time:timestamp", "incoming_value"},
      {{EventAttribute{"case_id", "R1", AttributeKind::kString},
        EventAttribute{"time:timestamp", "b", AttributeKind::kString},
        EventAttribute{"incoming_value", "incoming", AttributeKind::kString}}}};
  CaseRelationsOptions options;
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert((merged.columns == std::vector<std::string>{
      "case_id", "time:timestamp", "base_value", "incoming_value", "lhs",
      "rhs", "relation note", "empty"}));
  assert(merged.rows.size() == 2);
  assert(merged.rows[1][4] && merged.rows[1][4]->value == "L1");
  assert(merged.rows[1][5] && merged.rows[1][5]->value == "R1");
  assert(merged.rows[1][6] &&
         merged.rows[1][6]->value == "quoted, \"note\"");
  assert(merged.rows[1][7] && merged.rows[1][7]->value.empty() &&
         !merged.rows[1][7]->is_null);
  for (const auto& suffix : {std::string{}, std::string("_SAME")}) {
    options.left_suffix = options.right_suffix = suffix;
    assert(merge_case_relations(base, incoming, relations, options).rows == merged.rows);
    const auto adapted = merge_case_relations(
        base, incoming, std::vector<CaseRelation>{{"L1", "R1"}}, options);
    assert(adapted.rows.size() == 2 && adapted.columns.back() == "rhs");
  }
  options.relation_right_column = "lhs";
  for (bool vector_adapter : {false, true}) {
    bool rejected = false;
    try {
      if (vector_adapter)
        (void)merge_case_relations(base, incoming,
            std::vector<CaseRelation>{{"L1", "R1"}}, options);
      else
        (void)merge_case_relations(base, incoming, relations, options);
    } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }

  const auto expect_bad = [](const std::string& csv) {
    bool threw = false;
    try {
      std::istringstream malformed(csv);
      (void)parse_case_relations_table_csv(malformed);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };
  // Header names follow pandas.read_csv (checked black-box against
  // read_csv + case_relations.apply): an empty name is "Unnamed: <i>", a
  // repeat "name.1", ... past names the header already holds, and the
  // selectors take the first column of a name.
  {
    std::istringstream named(
        ",lhs,rhs,note,note,lhs,note.1, \n0,L1,R1,x,y,Q,z,w\n");
    const auto table = parse_case_relations_table_csv(named);
    assert((table.columns ==
            std::vector<std::string>{"Unnamed: 0", "lhs", "rhs", "note",
                                     "note.2", "lhs.1", "note.1",
                                     "Unnamed: 7"}));
    assert(table.rows.size() == 1 && table.rows[0][5]->value == "Q");
    std::istringstream selected(
        "case_id_LEFT,case_id_RIGHT,case_id_LEFT,,x\nL1,R1,Q,w,v\n");
    assert((parse_case_relations_csv(selected) ==
            std::vector<CaseRelation>{{"L1", "R1"}}));
    std::istringstream unnamed(",case_id_LEFT,,case_id_RIGHT\n0,L1,u,R1\n");
    assert((parse_case_relations_csv(unnamed) ==
            std::vector<CaseRelation>{{"L1", "R1"}}));
    std::istringstream renamed_only("case_id_LEFT,case_id_LEFT\nL1,R1\n");
    bool missing = false;
    try {
      (void)parse_case_relations_csv(renamed_only);
    } catch (const std::invalid_argument& error) {
      missing = std::string(error.what()).find("missing column case_id_RIGHT") !=
                std::string::npos;
    }
    assert(missing);
  }
  expect_bad("lhs,rhs,extra\nL1,R1\n");
  expect_bad("lhs,rhs\n\"L1,R1\n");
}

EventAttribute uint64_cell(const std::string& key, std::uint64_t value) {
  EventAttribute cell{key, std::to_string(value), AttributeKind::kInt};
  cell.typed_value = value;
  return cell;
}

EventAttribute float_cell(const std::string& key, double value) {
  EventAttribute cell{key, "", AttributeKind::kFloat};
  cell.typed_value = value;
  cell.value = std::to_string(value);
  return cell;
}

void exact_uint64_join_and_sorting() {
  const auto high = std::numeric_limits<std::uint64_t>::max();
  EventTable base{
      {"case", "when", "label"},
      {{uint64_cell("case", high), EventAttribute{"when", "z", AttributeKind::kString},
        EventAttribute{"label", "base", AttributeKind::kString}}}};
  EventTable incoming{
      {"case", "when", "label"},
      {{uint64_cell("case", high - 1), EventAttribute{"when", "a", AttributeKind::kString},
        EventAttribute{"label", "near", AttributeKind::kString}},
       {uint64_cell("case", high), EventAttribute{"when", "b", AttributeKind::kString},
        EventAttribute{"label", "match", AttributeKind::kString}}}};
  EventTable relations{
      {"lhs", "rhs"},
      {{uint64_cell("lhs", 7), uint64_cell("rhs", high - 1)},
       {uint64_cell("lhs", 8), uint64_cell("rhs", high)}}};
  CaseRelationsOptions options;
  options.case_id_key = "case";
  options.timestamp_key = "when";
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.rows.size() == 3);
  assert(merged.rows[0][0] &&
         std::get<std::uint64_t>(merged.rows[0][0]->typed_value) == 7);
  assert(merged.rows[0][2] && merged.rows[0][2]->value == "near");
  assert(merged.rows[1][0] &&
         std::get<std::uint64_t>(merged.rows[1][0]->typed_value) == 8);
  assert(merged.rows[1][2] && merged.rows[1][2]->value == "match");
  assert(merged.rows[2][0] &&
         std::get<std::uint64_t>(merged.rows[2][0]->typed_value) == high);
  assert(merged.rows[2][2] && merged.rows[2][2]->value == "base");

  auto sortable = base;
  sortable.rows.push_back(
      {uint64_cell("case", high - 1), EventAttribute{"when", "a", AttributeKind::kString},
       EventAttribute{"label", "lower", AttributeKind::kString}});
  const auto sorted = merge_case_relations(
      sortable, EventTable{{"case", "when", "label"}, {}},
      EventTable{{"lhs", "rhs"}, {}}, options);
  assert(sorted.rows.size() == 2);
  assert(std::get<std::uint64_t>(sorted.rows[0][0]->typed_value) == high - 1);
  assert(std::get<std::uint64_t>(sorted.rows[1][0]->typed_value) == high);
}

void unknown_dtype_metadata_falls_back_without_mutating_inputs() {
  EventTable base{
      {"case", "when", "payload"},
      {{EventAttribute{"case", "L1", AttributeKind::kString},
        EventAttribute{"when", "a", AttributeKind::kString},
        EventAttribute{"payload", "base", AttributeKind::kString}}}};
  EventTable incoming{
      {"case", "when", "payload"},
      {{EventAttribute{"case", "R1", AttributeKind::kString},
        EventAttribute{"when", "b", AttributeKind::kString},
        EventAttribute{"payload", "incoming", AttributeKind::kString}}}};
  EventTable relations{
      {"lhs", "rhs"},
      {{EventAttribute{"lhs", "L1", AttributeKind::kString},
        EventAttribute{"rhs", "R1", AttributeKind::kString}}}};
  const auto base_before = base;
  const auto incoming_before = incoming;
  const auto relations_before = relations;
  CaseRelationsOptions options;
  options.case_id_key = "case";
  options.timestamp_key = "when";
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.dtypes.empty());
  assert(merged.categorical_dtypes.empty());
  assert(base.columns == base_before.columns && base.rows == base_before.rows &&
         base.dtypes == base_before.dtypes &&
         base.categorical_dtypes == base_before.categorical_dtypes);
  assert(incoming.columns == incoming_before.columns &&
         incoming.rows == incoming_before.rows);
  assert(relations.columns == relations_before.columns &&
         relations.rows == relations_before.rows);
}

void unknown_dtype_prefixes_fall_back_to_object() {
  EventTable base{
      {"case", "when", "payload"},
      {{EventAttribute{"case", "L1", AttributeKind::kString},
        EventAttribute{"when", "a", AttributeKind::kString},
        EventAttribute{"payload", "base", AttributeKind::kString}}}};
  EventTable incoming{
      {"case", "when", "payload"},
      {{EventAttribute{"case", "R1", AttributeKind::kString},
        EventAttribute{"when", "b", AttributeKind::kString},
        EventAttribute{"payload", "incoming", AttributeKind::kString}}}};
  EventTable relations{
      {"lhs", "rhs"},
      {{EventAttribute{"lhs", "L1", AttributeKind::kString},
        EventAttribute{"rhs", "R1", AttributeKind::kString}}}};
  base.dtypes = {"integer", "interval[int64, right]", "str"};
  incoming.dtypes = {"integer", "interval[int64, right]", "str"};
  relations.dtypes = {"str", "str"};
  CaseRelationsOptions options;
  options.case_id_key = "case";
  options.timestamp_key = "when";
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.dtypes.size() == merged.columns.size());
  assert(merged.dtypes[0] == "object");
  assert(merged.dtypes[1] == "object");
}

void incompatible_category_numeric_metadata_promotes_without_narrowing() {
  const auto high = std::numeric_limits<std::uint64_t>::max();
  EventTable base{
      {"case", "when", "score"},
      {{EventAttribute{"case", "L1", AttributeKind::kString},
        EventAttribute{"when", "a", AttributeKind::kString},
        float_cell("score", 1.25)}}};
  EventTable incoming{
      {"case", "when", "score"},
      {{EventAttribute{"case", "R1", AttributeKind::kString},
        EventAttribute{"when", "b", AttributeKind::kString},
        uint64_cell("score", high)}}};
  EventTable relations{
      {"lhs", "rhs"},
      {{EventAttribute{"lhs", "L1", AttributeKind::kString},
        EventAttribute{"rhs", "R1", AttributeKind::kString}}}};
  base.dtypes = {"str", "str", "category"};
  incoming.dtypes = {"str", "str", "category"};
  relations.dtypes = {"str", "str"};
  base.categorical_dtypes.assign(3, std::nullopt);
  incoming.categorical_dtypes.assign(3, std::nullopt);
  base.categorical_dtypes[2] = EventTableCategoricalDtype{
      "float64", {float_cell("category", 1.25), float_cell("category", 4.5)},
      false};
  incoming.categorical_dtypes[2] = EventTableCategoricalDtype{
      "uint64", {uint64_cell("category", high - 1),
                  uint64_cell("category", high)},
      false};
  CaseRelationsOptions options;
  options.case_id_key = "case";
  options.timestamp_key = "when";
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.dtypes[2] == "float64");
  assert(merged.rows.size() == 2);
  assert(merged.rows[1][2] &&
         std::get<double>(merged.rows[1][2]->typed_value) ==
             static_cast<double>(high));
}

void categorical_nested_null_state_is_semantic() {
  auto tuple_category = [](bool first_class_null) {
    EventAttribute tuple{"score", "", AttributeKind::kString};
    tuple.container = AttributeContainerKind::kTuple;
    EventAttribute value{"value", "x", AttributeKind::kString};
    EventAttribute missing{"missing", "", AttributeKind::kString};
    missing.is_null = !first_class_null;
    if (first_class_null)
      missing.first_class_state = FirstClassAttributeState::kNull;
    tuple.children = {value, missing};
    return tuple;
  };
  EventTable base{
      {"case", "when", "score"},
      {{EventAttribute{"case", "L1", AttributeKind::kString},
        EventAttribute{"when", "a", AttributeKind::kString},
        tuple_category(true)}}};
  EventTable incoming{
      {"case", "when", "score"},
      {{EventAttribute{"case", "R1", AttributeKind::kString},
        EventAttribute{"when", "b", AttributeKind::kString},
        tuple_category(false)}}};
  EventTable relations{
      {"lhs", "rhs"},
      {{EventAttribute{"lhs", "L1", AttributeKind::kString},
        EventAttribute{"rhs", "R1", AttributeKind::kString}}}};
  base.dtypes = {"str", "str", "category"};
  incoming.dtypes = {"str", "str", "category"};
  relations.dtypes = {"str", "str"};
  base.categorical_dtypes.assign(3, std::nullopt);
  incoming.categorical_dtypes.assign(3, std::nullopt);
  base.categorical_dtypes[2] = EventTableCategoricalDtype{
      "object", {tuple_category(true)}, false};
  incoming.categorical_dtypes[2] = EventTableCategoricalDtype{
      "object", {tuple_category(false)}, false};
  CaseRelationsOptions options;
  options.case_id_key = "case";
  options.timestamp_key = "when";
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.dtypes[2] == "category");
}

void categorical_state_null_cells_sort_last() {
  const auto category_cell = [](const std::string& key,
                                const std::string& value) {
    return EventAttribute{key, value, AttributeKind::kString};
  };
  auto state_null_cell = [](const std::string& key,
                            const std::string& stale_value) {
    EventAttribute cell{key, stale_value, AttributeKind::kString};
    cell.first_class_state = FirstClassAttributeState::kNull;
    return cell;
  };
  const EventTableCategoricalDtype case_dtype{
      "str", {category_cell("category", "B"),
              category_cell("category", "A")}, false};
  const EventTableCategoricalDtype timestamp_dtype{
      "str", {category_cell("category", "high"),
              category_cell("category", "low")}, false};
  EventTable base{
      {"case", "when", "payload"},
      {{category_cell("case", "B"), category_cell("when", "high"),
        category_cell("payload", "regular")},
       {category_cell("case", "B"), state_null_cell("when", "low"),
        category_cell("payload", "timestamp-missing")},
       {state_null_cell("case", "A"), category_cell("when", "low"),
        category_cell("payload", "case-missing")}}};
  EventTable incoming{{"case", "when", "payload"}, {}};
  EventTable relations{{"lhs", "rhs"}, {}};
  base.dtypes = {"category", "category", "str"};
  incoming.dtypes = {"category", "category", "str"};
  relations.dtypes = {"category", "category"};
  base.categorical_dtypes = {case_dtype, timestamp_dtype, std::nullopt};
  incoming.categorical_dtypes = {case_dtype, timestamp_dtype, std::nullopt};
  relations.categorical_dtypes = {case_dtype, case_dtype};
  CaseRelationsOptions options;
  options.case_id_key = "case";
  options.timestamp_key = "when";
  options.relation_left_column = "lhs";
  options.relation_right_column = "rhs";
  const auto merged = merge_case_relations(base, incoming, relations, options);
  assert(merged.rows.size() == 3);
  assert(merged.rows[0][2] && merged.rows[0][2]->value == "regular");
  assert(merged.rows[1][2] &&
         merged.rows[1][2]->value == "timestamp-missing");
  assert(merged.rows[2][2] && merged.rows[2][2]->value == "case-missing");
}



EventAttribute signed_table_cell(const std::string& key, std::int64_t value) {
  EventAttribute cell{key, std::to_string(value), AttributeKind::kInt};
  cell.typed_value = value;
  return cell;
}

void typed_table_route_retains_native_join_types_and_complete_storage() {
  const auto text = [](const std::string& key, const std::string& value) {
    return EventAttribute{key, value, AttributeKind::kString};
  };
  EventTable base{{"case_id", "time:timestamp", "activity"},
      {{text("case_id", "B"), signed_table_cell("time:timestamp", 0), text("activity", "base")}}};
  EventTable incoming{{"case_id", "time:timestamp", "activity"},
      {{signed_table_cell("case_id", 7), signed_table_cell("time:timestamp", 1), text("activity", "numeric")},
       {text("case_id", "7"), signed_table_cell("time:timestamp", 2), text("activity", "string")}}};
  EventAttribute note{"note", "true", AttributeKind::kBoolean};
  note.typed_value = true;
  EventTable relations{{"case_id_LEFT", "case_id_RIGHT", "note"},
      {{text("case_id_LEFT", "B"), signed_table_cell("case_id_RIGHT", 7), note},
       {text("case_id_LEFT", "B"), signed_table_cell("case_id_RIGHT", 7), note}}};
  base.index = EventTableIndex{"Index", "object", {std::string("source-name")},
      {EventTableIndexLabel{text("index", "base-index")}}};
  const std::string bytes("x\0\xff", 3);
  base.attributes = {text(bytes, bytes)};
  base.metadata.extensions = {{bytes, "p", bytes}};
  base.metadata.classifiers = {{bytes, {"activity", bytes}}};
  base.metadata.trace_globals = {text("trace", bytes)};
  base.metadata.event_globals = {text("event", bytes)};
  base.metadata.properties = {text("property", bytes)};
  base.metadata.globals = {{"trace", base.metadata.trace_globals}, {"event", base.metadata.event_globals}};
  base.metadata.compatibility_snapshot_valid = true;
  base.metadata.compatibility_globals_snapshot = {{"trace", {text("old-global", bytes)}}};
  base.metadata.compatibility_trace_globals_snapshot = {text("old-trace", bytes)};
  base.metadata.compatibility_event_globals_snapshot = {text("old-event", bytes)};
  const auto encode = serialize_quality_workbench_table_json;
  const auto base_json = encode(base), incoming_json = encode(incoming), relations_json = encode(relations);
  const auto decoded = parse_embedding_workbench_table_json(base_json);
  assert(decoded.attributes == base.attributes && decoded.metadata == base.metadata);
  assert(decoded.metadata.compatibility_snapshot_valid);
  assert(decoded.metadata.compatibility_globals_snapshot == base.metadata.compatibility_globals_snapshot);
  assert(decoded.metadata.compatibility_trace_globals_snapshot == base.metadata.compatibility_trace_globals_snapshot);
  assert(decoded.metadata.compatibility_event_globals_snapshot == base.metadata.compatibility_event_globals_snapshot);
  assert(encode(decoded) == base_json);
  const auto typed_incoming = parse_embedding_workbench_table_json(incoming_json);
  assert(std::holds_alternative<std::int64_t>(typed_incoming.rows[0][0]->typed_value));
  assert(typed_incoming.rows[1][0]->kind == AttributeKind::kString);
  const auto merged = merge_case_relations(base, incoming, relations);
  assert(merged.rows.size() == 3 && merged.rows[0][2]->value == "base");
  assert(merged.rows[1][2]->value == "numeric" && merged.rows[2][2]->value == "numeric");
  assert(merged.rows[1][merged.column_index("note")]->typed_value == NativeAttributeValue{true});
  assert(merged.index && merged.index->dtype == "object");
  assert(merged.index->names == std::vector<std::optional<std::string>>{std::nullopt});
  assert(std::get<EventAttribute>(merged.index->labels[0].value).value == "base-index");
  assert(std::get<EventAttribute>(merged.index->labels[1].value).typed_value == NativeAttributeValue{std::int64_t{0}});
  assert(std::get<EventAttribute>(merged.index->labels[2].value).typed_value == NativeAttributeValue{std::int64_t{1}});
  assert(merged.attributes == base.attributes && merged.metadata == base.metadata);
  assert(merged.metadata.compatibility_snapshot_valid == base.metadata.compatibility_snapshot_valid);
  assert(merged.metadata.compatibility_globals_snapshot == base.metadata.compatibility_globals_snapshot);
  assert(merged.metadata.compatibility_trace_globals_snapshot == base.metadata.compatibility_trace_globals_snapshot);
  assert(merged.metadata.compatibility_event_globals_snapshot == base.metadata.compatibility_event_globals_snapshot);
  const auto body = tools::execute_typed_case_relations_json(base_json, incoming_json, relations_json, {}, true, true);
  assert(!literal_controls_transport_error(body));
  assert(body.find("\"table\":" + encode(merged)) != std::string::npos);
  const auto snapshot = tools::typed_case_relations_snapshot(base, incoming, relations);
  assert(body.find("\"input_before\":" + snapshot) != std::string::npos);
  assert(body.find("\"input_after\":" + snapshot) != std::string::npos);
  assert(body.find("\"projected_log\":") != std::string::npos);
  assert(body.find("\"format_table\":") != std::string::npos && body.find("\"open_log\":") != std::string::npos);
  assert(encode(base) == base_json && encode(incoming) == incoming_json && encode(relations) == relations_json);
}

void typed_table_options_are_literal_and_conflicts_rejected() {
  std::map<std::string, std::string> fields;
  const FieldLookup lookup = [&](std::string_view name) {
    const auto found = fields.find(std::string(name));
    return found == fields.end() ? std::string{} : found->second;
  };
  const FieldPresence present = [&](std::string_view name) { return fields.contains(std::string(name)); };
  const auto defaults = tools::typed_case_relations_options(lookup, present);
  assert(defaults.case_id_key == "case_id" && defaults.timestamp_key == "time:timestamp");
  const std::string exact(" \0\xff ", 4);
  fields["merge_timestamp_key_json"] = serialize_embedding_workbench_string_json(exact);
  fields["merge_case_id_key"] = " case ";
  fields["merge_left_suffix"] = "";
  fields["merge_right_suffix"] = "\t";
  fields["merge_left_column"] = "lhs";
  fields["merge_right_column_json"] = "\"rhs\"";
  const auto options = tools::typed_case_relations_options(lookup, present);
  assert(options.timestamp_key == exact && options.case_id_key == " case ");
  assert(options.left_suffix.empty() && options.right_suffix == "\t");
  assert(options.relation_left_column == "lhs" && options.relation_right_column == "rhs");
  assert(!literal_controls_transport_error(tools::typed_case_relations_options_json(options)));
  const auto rejected = [&] {
    try { (void)tools::typed_case_relations_options(lookup, present); return false; }
    catch (const std::invalid_argument&) { return true; }
  };
  fields["merge_timestamp_key"] = exact;
  assert(rejected());
  fields.erase("merge_timestamp_key");
  fields["merge_timestamp_key_json"] = "1";
  assert(rejected());
  fields["merge_timestamp_key_json"] = "\"one\",\"two\"";
  assert(rejected());
  fields.erase("merge_timestamp_key_json");
  fields["merge_case_id_key"] = "";
  assert(tools::typed_case_relations_options(lookup, present).case_id_key.empty());
  fields["merge_variant"] = "other";
  assert(rejected());
}

void typed_table_route_accepts_ordinary_json_and_preserves_native_errors() {
  const std::string base = R"({"columns":["case_id","time:timestamp"],"rows":[[{"value":"B"},{"kind":"int","value":"0","typed_value":0,"integer_storage":"int64"}]]})";
  const std::string incoming = R"({"columns":["case_id","time:timestamp"],"rows":[]})";
  const std::string relations = R"({"columns":["case_id_LEFT","case_id_RIGHT"],"rows":[]})";
  const auto body = tools::execute_typed_case_relations_json(base, incoming, relations, {});
  assert(!literal_controls_transport_error(body));
  assert(body.find("\"native_schema\":\"event-table-raw-v1\"") != std::string::npos);
  assert(body.find("\"format_table\":") == std::string::npos && body.find("\"open_log\":") == std::string::npos);
  bool rejected = false;
  try { (void)tools::execute_typed_case_relations_json("{}", incoming, relations, {}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  CaseRelationsOptions invalid;
  invalid.timestamp_key = "";
  std::string native_error, route_error;
  try { (void)merge_case_relations(parse_embedding_workbench_table_json(base), parse_embedding_workbench_table_json(incoming), parse_embedding_workbench_table_json(relations), invalid); }
  catch (const std::invalid_argument& error) { native_error = error.what(); }
  try { (void)tools::execute_typed_case_relations_json(base, incoming, relations, invalid); }
  catch (const std::invalid_argument& error) { route_error = error.what(); }
  assert(!native_error.empty() && route_error == native_error);
}

}  // namespace

int main() try {
#ifdef _WIN32
  _set_error_mode(_OUT_TO_STDERR);
#endif
  typed_table_route_retains_native_join_types_and_complete_storage();
  typed_table_options_are_literal_and_conflicts_rejected();
  typed_table_route_accepts_ordinary_json_and_preserves_native_errors();
  check_public_case_relations_reference_cases();
  check_public_case_relations_dtype_reference_cases();
  check_public_case_relations_index_reference_cases();
  missing_timestamps_sort_last_with_stable_duplicates();
  nanosecond_ordering_and_exact_ties_match_public_merge();
  merges_and_drops();
  parses_relations_and_custom_timestamps();
  rejects_invalid_timestamps();
  supports_custom_case_key_and_suffixes();
  event_log_selectors_ignore_unused_suffixes();
  json_shape_is_stable();
  table_vector_adapter_preserves_inputs();
  implicit_index_metadata_is_reset_or_carried_as_expected();
  range_index_extrema_do_not_overflow_classification();
  table_selectors_are_exact();
  parses_relation_table_with_extra_columns();
  exact_uint64_join_and_sorting();
  unknown_dtype_metadata_falls_back_without_mutating_inputs();
  unknown_dtype_prefixes_fall_back_to_object();
  incompatible_category_numeric_metadata_promotes_without_narrowing();
  categorical_nested_null_state_is_semantic();
  categorical_state_null_cells_sort_last();
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
