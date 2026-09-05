#include "minera/alpha_miner.hpp"
#include "minera/log_conversion.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Action>
void rejects(Action action, const char* message) {
  try { action(); }
  catch (const Exception&) { return; }
  throw std::runtime_error(message);
}

EventRecord row(std::string case_id, std::string activity,
                std::string timestamp = "2024-01-01T00:00:00Z") {
  return {{{"case:concept:name", std::move(case_id)},
           {"concept:name", std::move(activity)},
           {"time:timestamp", std::move(timestamp), AttributeKind::kDate}}};
}

std::vector<std::string> sequence(const Trace& trace) {
  std::vector<std::string> result;
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

std::set<std::string> boundaries(const PetriNet& net, bool start) {
  std::set<std::string> result;
  for (const auto& transition : net.transitions()) {
    const auto& arcs = start ? transition.inputs : transition.outputs;
    if (transition.activity &&
        std::find(arcs.begin(), arcs.end(), start ? net.source() : net.sink()) !=
            arcs.end())
      result.insert(net.activities()[*transition.activity]);
  }
  return result;
}

bool connects(const PetriNet& net, const std::string& from,
               const std::string& to) {
  for (const auto& source : net.transitions()) {
    if (!source.activity || net.activities()[*source.activity] != from) continue;
    for (const auto& target : net.transitions()) {
      if (!target.activity || net.activities()[*target.activity] != to) continue;
      for (const auto place : source.outputs)
        if (std::find(target.inputs.begin(), target.inputs.end(), place) !=
            target.inputs.end()) return true;
    }
  }
  return false;
}

void stream_order_and_metadata() {
  EventStream stream;
  stream.events = {row("z", "B", "2024-01-02"), row("a", "X"),
                   row("z", "A", "2024-01-01"), row("a", "Y")};
  stream.attributes = {{"name", "flat-log"}};
  stream.metadata.properties = {{"producer", "test"}};
  const auto original = stream.events;
  for (const bool plus : {false, true}) {
    AlphaMinerOptions options;
    options.short_loops = plus;
    const auto normalized = normalize_alpha_event_log(stream, options);
    require(normalized.traces.size() == 2, "interleaved cases must group");
    require(normalized.traces[0].case_id == "z", "stream first-seen case order");
    require(sequence(normalized.traces[0]) == std::vector<std::string>{"B", "A"},
            "streams must not sort timestamps");
    require(sequence(normalized.traces[1]) == std::vector<std::string>{"X", "Y"},
            "stream per-case order");
    require(normalized.attributes == stream.attributes, "log attributes retained");
    require(normalized.metadata == stream.metadata, "log metadata retained");
    const auto net = discover_petri_net(EventDataView(stream), options);
    require(!net.transitions().empty(), "view discovery must run");
  }
  for (std::size_t i = 0; i < stream.events.size(); ++i)
    require(stream.events[i].fields == original[i].fields, "no input mutation");
}

void selector_precedence_and_types() {
  EventStream stream;
  auto stale_case = EventAttribute{"case:concept:name", "stale-case"};
  stale_case.first_class_role = FirstClassAttributeRole::kCaseId;
  stale_case.first_class_state = FirstClassAttributeState::kPresent;
  stale_case.first_class_source_key = "old-case";
  auto stale_activity = EventAttribute{"activity", "stale-activity"};
  stale_activity.first_class_role = FirstClassAttributeRole::kActivity;
  stale_activity.first_class_state = FirstClassAttributeState::kPresent;
  stale_activity.first_class_source_key = "old-label";
  EventAttribute selected_case{"new-case", "7", AttributeKind::kInt};
  selected_case.typed_value = std::int64_t{7};
  EventAttribute selected_activity{"new-label", "true", AttributeKind::kBoolean};
  selected_activity.typed_value = true;
  selected_activity.children = {{"detail", "retained"}};
  stream.events = {{{stale_case, stale_activity, selected_case, selected_activity}}};
  AlphaMinerOptions options;
  options.case_id_attribute = "new-case";
  options.activity_attribute = "new-label";
  auto log = normalize_alpha_event_log(stream, options);
  require(log.traces[0].case_id == "7", "stale case role must not override selection");
  const auto& event = log.traces[0].events[0];
  require(event.case_id == "7" && event.activity == "true", "selected projections");
  require(log.traces[0].case_id_attribute->typed_value == selected_case.typed_value,
          "selected typed case retained");
  require(event.activity_attribute->typed_value == selected_activity.typed_value &&
              event.activity_attribute->children == selected_activity.children,
          "selected typed activity and children retained");

  EventAttribute retained{"transport-label", "retained"};
  retained.first_class_source_key = "concept:name";
  stream.events = {{{{"case", "c"}, {"activity", "alias"}, retained,
                     {"concept:name", "exact"}}}};
  log = normalize_alpha_event_log(stream);
  require(log.traces[0].events[0].activity == "exact", "exact beats retained source");
  stream.events[0].fields.pop_back();
  log = normalize_alpha_event_log(stream);
  require(log.traces[0].events[0].activity == "retained", "retained source beats alias");
  stream.events[0].fields.pop_back();
  require(normalize_alpha_event_log(stream).traces[0].events[0].activity == "alias",
          "canonical alias fallback");
  stream.events[0].fields.push_back({"activity", "second-alias"});
  stream.events[0].fields.push_back({"concept:name", "winner"});
  require(normalize_alpha_event_log(stream).traces[0].events[0].activity == "winner",
          "duplicate lower-priority aliases do not defeat exact winner");
  stream.events[0].fields.back().is_null = true;
  rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(stream); },
                                "null exact winner does not fall back to aliases");
  stream.events = {{{{"case", "c"}, retained, retained}}};
  rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(stream); },
                                "duplicate retained-source selections are ambiguous");

  options.case_id_attribute = "shared";
  options.activity_attribute = "shared";
  stream.events = {{{{"shared", "same"}}}};
  log = normalize_alpha_event_log(stream, options);
  require(log.traces[0].case_id == "same" &&
              log.traces[0].events[0].activity == "same",
          "same source cell supports both projections");
}

void invalid_inputs_and_caps() {
  EventStream valid;
  valid.events = {row("c", "A"), row("c", "B")};
  AlphaMinerOptions options;
  options.max_rows_to_materialize = 1;
  rejects<std::length_error>([&] { (void)normalize_alpha_event_log(valid, options); },
                             "stream cap must fail");
  const auto table = event_stream_to_table(valid);
  rejects<std::length_error>([&] { (void)normalize_alpha_event_log(table, options); },
                             "table cap must fail");
  auto lazy = LazyEventTable::from_table(table);
  rejects<std::length_error>([&] { (void)discover_petri_net(lazy, options); },
                             "lazy discovery cap must fail");
  std::size_t scanned = 0;
  LazyEventTable unknown_size(table.columns, [&](const LazyEventTable::RowConsumer& consume) {
    for (const auto& row : table.rows) {
      ++scanned;
      consume(row);
    }
  });
  rejects<std::length_error>([&] {
    (void)normalize_alpha_event_log(unknown_size, options);
  }, "unknown-size lazy source must enforce hard cap while scanning");
  require(scanned == 2, "lazy cap fails at first excess row without truncation");
  const auto grouped = normalize_alpha_event_log(valid);
  rejects<std::length_error>([&] {
    (void)normalize_alpha_event_log(EventDataView(grouped), options);
  }, "grouped view cap must fail");
  options.max_rows_to_materialize = 2;
  require(normalize_alpha_event_log(valid, options).event_count() == 2,
          "cap accepts exact boundary");
  options.max_rows_to_materialize = 0;
  require(normalize_alpha_event_log(EventStream{}, options).event_count() == 0,
          "zero cap accepts empty stream");

  for (const std::size_t field : {std::size_t{0}, std::size_t{1}}) {
    auto invalid = valid;
    invalid.events[0].fields.erase(invalid.events[0].fields.begin() + field);
    rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(invalid); },
                                  "missing required selection must fail");
    invalid = valid;
    invalid.events[0].fields[field].is_null = true;
    rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(invalid); },
                                  "null required selection must fail");
    invalid = valid;
    invalid.events[0].fields.push_back(invalid.events[0].fields[field]);
    rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(invalid); },
                                  "duplicate winning field must fail");
  }
  options = {};
  options.case_id_attribute.clear();
  rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(valid, options); },
                                "empty case selector must fail");
  options = {};
  options.activity_attribute = "unavailable";
  rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(valid, options); },
                                "unavailable custom selector must fail");

  EventLog original;
  original.traces = {{"same", {{"same", "B"}, {"same", "A"}}},
                     {"same", {}}, {"same", {{"same", "C"}}}};
  options.case_id_attribute = "irrelevant";
  options.activity_attribute.clear();
  const auto copied = normalize_alpha_event_log(EventDataView(original), options);
  require(copied.traces.size() == 3 && copied.traces[1].events.empty(),
          "grouped view retains duplicate case boundaries and empty traces");
  require(sequence(copied.traces[0]) == std::vector<std::string>{"B", "A"},
          "grouped view preserves event order");
}

void table_order_and_discovery() {
  EventStream stream;
  stream.events = {row("z", "B", "2024-01-02"), row("a", "X"),
                   row("z", "A", "2024-01-01"), row("a", "Y")};
  const auto table = event_stream_to_table(stream);
  const auto lazy = LazyEventTable::from_table(table);
  const auto eager_log = normalize_alpha_event_log(table);
  const auto lazy_log = normalize_alpha_event_log(lazy);
  require(sequence(eager_log.traces[0]) == sequence(lazy_log.traces[0]),
          "eager/lazy grouping parity");
  require(sequence(eager_log.traces[0]) == std::vector<std::string>{"B", "A"},
          "normalization preserves source order");
  const auto eager_net = discover_petri_net(table);
  const auto lazy_net = discover_petri_net(lazy);
  require(connects(eager_net, "A", "B") && !connects(eager_net, "B", "A"),
          "classic table edges sort by timestamp");
  require(connects(lazy_net, "A", "B") && connects(lazy_net, "X", "Y"),
          "lazy classic table edges match eager");
  require(boundaries(eager_net, true) == std::set<std::string>{"B", "X"} &&
              boundaries(eager_net, false) == std::set<std::string>{"A", "Y"},
          "classic table boundaries retain original row order");
  require(connects(discover_petri_net(stream), "B", "A"),
          "stream discovery retains source-order edges");
  AlphaMinerOptions options;
  options.sort_timestamp_along_case_id = false;
  require(connects(discover_petri_net(table, options), "B", "A"),
          "case-only table sort preserves per-case row order");
  options.sort_caseid_required = false;
  const auto unsorted = discover_petri_net(table, options);
  require(!connects(unsorted, "B", "A") && !connects(unsorted, "X", "Y"),
          "unsorted table footprint uses only adjacent same-case rows");
  options = {};
  options.short_loops = true;
  require(connects(discover_petri_net(table, options), "B", "A"),
          "Alpha+ tables preserve grouped row order");

  EventStream interval;
  interval.events = {row("c", "A", "2024-01-04"),
                     row("c", "B", "2024-01-03")};
  interval.events[0].fields.push_back({"began", "2024-01-01", AttributeKind::kDate});
  interval.events[1].fields.push_back({"began", "2024-01-02", AttributeKind::kDate});
  options = {};
  options.start_timestamp_attribute = "began";
  require(connects(discover_petri_net(event_stream_to_table(interval), options),
                   "A", "B"), "selected start timestamp is primary sort key");
  options.start_timestamp_attribute = "missing";
  rejects<std::invalid_argument>([&] {
    (void)discover_petri_net(event_stream_to_table(interval), options);
  }, "missing explicit start column must fail");

  EventStream untimed;
  untimed.events = {{{{"case", "c"}, {"activity", "A"}}},
                    {{{"case", "c"}, {"activity", "B"}}}};
  const auto untimed_table = event_stream_to_table(untimed);
  options = {};
  rejects<std::invalid_argument>([&] { (void)discover_petri_net(untimed_table); },
                                "classic timestamp ordering needs timestamp column");
  options.sort_timestamp_along_case_id = false;
  rejects<std::invalid_argument>([&] {
    (void)discover_petri_net(untimed_table, options);
  }, "classic table requires completion schema even when timestamp sort disabled");
  options = {};
  options.short_loops = true;
  require(connects(discover_petri_net(untimed_table, options), "A", "B"),
          "Alpha+ tables do not require timestamps");
}

void table_selector_precedence_and_missing_cells() {
  EventStream stream;
  for (const auto& activity : {"A", "B"}) {
    auto record = row("stale", "wrong", "2024-01-01");
    record.fields[0].first_class_role = FirstClassAttributeRole::kCaseId;
    record.fields[0].first_class_state = FirstClassAttributeState::kPresent;
    record.fields[1].first_class_role = FirstClassAttributeRole::kActivity;
    record.fields[1].first_class_state = FirstClassAttributeState::kPresent;
    record.fields[2].first_class_role = FirstClassAttributeRole::kTimestamp;
    record.fields[2].first_class_state = FirstClassAttributeState::kPresent;
    record.fields.push_back({"chosen-case", "actual"});
    record.fields.push_back({"chosen-label", activity});
    record.fields.push_back({"chosen-time", activity == std::string("A")
                                                 ? "2024-01-03" : "2024-01-02",
                             AttributeKind::kDate});
    stream.events.push_back(std::move(record));
  }
  AlphaMinerOptions options;
  options.case_id_attribute = "chosen-case";
  options.activity_attribute = "chosen-label";
  options.timestamp_attribute = "chosen-time";
  auto table = event_stream_to_table(stream);
  const auto log = normalize_alpha_event_log(table, options);
  require(log.traces[0].case_id == "actual" &&
              sequence(log.traces[0]) == std::vector<std::string>{"A", "B"},
          "table custom selectors override old first-class roles");
  auto net = discover_petri_net(table, options);
  require(connects(net, "B", "A") && !connects(net, "A", "B"),
          "table custom timestamp selection overrides stale timestamp role");
  require(net.activities().size() == 2, "stale activity is not mined");
  for (const auto& key : {"chosen-case", "chosen-label"}) {
    auto missing = table;
    missing.rows[0][missing.column_index(key)].reset();
    const auto eager_missing = discover_petri_net(missing, options);
    const auto lazy_missing = discover_petri_net(LazyEventTable::from_table(missing), options);
    require(eager_missing.activities() == lazy_missing.activities(),
            "classic table missing selected cells preserve eager/lazy parity");
    require(eager_missing.activities().size() == 1 &&
                eager_missing.activities()[0] == "B",
            "missing selected case or activity does not use a stale alias");
    auto plus = options;
    plus.short_loops = true;
    rejects<std::invalid_argument>([&] { (void)discover_petri_net(missing, plus); },
                                  "Alpha+ table selection remains strict");
  }
  auto invalid_shape = table;
  invalid_shape.rows[0].pop_back();
  rejects<std::invalid_argument>([&] {
    (void)normalize_alpha_event_log(invalid_shape, options);
  }, "invalid table shape rejected");
}

void table_provenance_and_typed_dates() {
  EventStream stream;
  stream.events = {row("c", "A", "2024-01-01"),
                   row("c", "B", "2024-01-02")};
  stream.events[0].fields.push_back({"extra", "present"});
  const auto projected = normalize_alpha_event_log(event_stream_to_table(stream));
  require(std::none_of(projected.traces[0].events[1].attributes.begin(),
                       projected.traces[0].events[1].attributes.end(),
                       [](const auto& field) { return field.key == "extra"; }),
          "unselected missing table cells stay absent after normalization");
  for (auto& record : stream.events) {
    auto& label = record.fields[1];
    label.key = "transported_label";
    label.first_class_role = FirstClassAttributeRole::kActivity;
    label.first_class_state = FirstClassAttributeState::kPresent;
    label.first_class_source_key = "concept:name";
    record.fields.push_back({"activity", "wrong"});
  }
  auto table = event_stream_to_table(stream);
  table.rows[0][table.column_index("transported_label")].reset();
  for (const auto& net : {discover_petri_net(table),
                          discover_petri_net(LazyEventTable::from_table(table))})
    require(net.activities() == std::vector<std::string>{"B"},
            "missing transported activity retains source priority over alias");
  rejects<std::invalid_argument>([&] { (void)normalize_alpha_event_log(table); },
                                "strict normalization rejects missing transported selection");

  stream.events = {row("c", "A", "2024-01-01"),
                   row("c", "B", "2024-01-02")};
  for (auto& record : stream.events) {
    auto& timestamp = record.fields[2];
    timestamp.key = "transported_time";
    timestamp.first_class_source_key = "time:timestamp";
    record.fields.push_back({"timestamp", "2023-01-01", AttributeKind::kDate});
  }
  table = event_stream_to_table(stream);
  table.rows[0][table.column_index("transported_time")].reset();
  require(connects(discover_petri_net(table), "B", "A") &&
              connects(discover_petri_net(LazyEventTable::from_table(table)), "B", "A"),
          "missing transported timestamp remains null-last despite alias");

  stream.events = {row("c", "A", ""), row("c", "B", "")};
  stream.events[0].fields[2].typed_value = NativeDate{"2024-01-03"};
  stream.events[1].fields[2].typed_value = NativeDate{"2024-01-02"};
  table = event_stream_to_table(stream);
  require(connects(discover_petri_net(table), "B", "A") &&
              connects(discover_petri_net(LazyEventTable::from_table(table)), "B", "A"),
          "typed-only completion dates remain sortable");
  for (std::size_t i = 0; i < stream.events.size(); ++i) {
    EventAttribute start{"began", "", AttributeKind::kDate};
    start.typed_value = NativeDate{i == 0 ? "2024-01-01" : "2024-01-02"};
    stream.events[i].fields.push_back(std::move(start));
  }
  AlphaMinerOptions options;
  options.start_timestamp_attribute = "began";
  table = event_stream_to_table(stream);
  require(connects(discover_petri_net(table, options), "A", "B") &&
              connects(discover_petri_net(LazyEventTable::from_table(table), options), "A", "B"),
          "typed-only explicit start dates have primary ordering priority");
}

void classic_table_null_contracts() {
  EventStream stream;
  stream.events = {row("c", "A", "2024-01-01"),
                   row("c", "gap", "2024-01-02"),
                   row("c", "B", "2024-01-03")};
  stream.events[1].fields[1].is_null = true;
  auto table = event_stream_to_table(stream);
  auto net = discover_petri_net(table);
  require(!connects(net, "A", "B"), "null activity does not bridge adjacency");
  require(boundaries(net, true) == std::set<std::string>{"A"} &&
              boundaries(net, false) == std::set<std::string>{"B"},
          "null activity excluded from per-case endpoints");
  table.rows[1][table.column_index("concept:name")]->is_null = false;
  table.rows[1][table.column_index("case:concept:name")]->is_null = true;
  net = discover_petri_net(table);
  require(connects(net, "A", "B") && net.activities().size() == 2,
          "null-case row sorts outside valid case and contributes no activity");

  stream.events = {row("c", "A", "2024-01-01"),
                   row("c", "B", "2024-01-03")};
  stream.events[0].fields.push_back({"start_timestamp", "", AttributeKind::kDate, true});
  stream.events[1].fields.push_back({"start_timestamp", "2024-01-02", AttributeKind::kDate});
  table = event_stream_to_table(stream);
  net = discover_petri_net(table);
  require(connects(net, "B", "A") && !connects(net, "A", "B"),
          "null start sorts last without completion fallback");
  require(connects(discover_petri_net(LazyEventTable::from_table(table)), "B", "A"),
          "lazy null start uses same ordering");
  AlphaMinerOptions options;
  options.start_timestamp_attribute = "absent";
  options.sort_timestamp_along_case_id = false;
  rejects<std::invalid_argument>([&] { (void)discover_petri_net(table, options); },
                                "explicit missing start rejects even without sorting");

  // An existing exact column with a missing cell must not fall back to alias.
  table.columns.push_back("activity");
  for (auto& values : table.rows) values.push_back(EventAttribute{"activity", "wrong"});
  table.rows[0][table.column_index("concept:name")].reset();
  net = discover_petri_net(table);
  require(net.activities().size() == 1 && net.activities()[0] == "B",
          "missing exact activity column cell does not use alias");
  options = {};
  options.short_loops = true;
  rejects<std::invalid_argument>([&] { (void)discover_petri_net(table, options); },
                                "missing exact activity remains strict for Alpha+");

  stream.events = {row("c", "unused", "2024-01-01"),
                   row("c", "A", "2024-01-02"),
                   row("c", "B", "2024-01-03"),
                   row("c", "unused", "2024-01-04")};
  stream.events.front().fields[1].is_null = true;
  stream.events.back().fields[1].is_null = true;
  net = discover_petri_net(event_stream_to_table(stream));
  require(connects(net, "A", "B") &&
              boundaries(net, true) == std::set<std::string>{"A"} &&
              boundaries(net, false) == std::set<std::string>{"B"},
          "endpoint scan skips null boundary activities");
}
}  // namespace

int main() {
  try {
    stream_order_and_metadata();
    selector_precedence_and_types();
    invalid_inputs_and_caps();
    table_order_and_discovery();
    table_selector_precedence_and_missing_cells();
    classic_table_null_contracts();
    table_provenance_and_typed_dates();
    std::cout << "Alpha event-data tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
