#include "minera/attribute_statistics.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/json_export.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {

minera::Event make_event(std::string activity, std::string resource) {
  minera::Event event;
  event.activity = std::move(activity);
  event.resource = std::move(resource);
  event.attributes.push_back({"channel", event.activity,
                              minera::AttributeKind::kString});
  return event;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.attributes.push_back({"region", "north", minera::AttributeKind::kString});
  first.events = {make_event("A", "alice"), make_event("A", "alice"),
                  make_event("B", "bob")};
  minera::Trace second;
  second.case_id = "c2";
  second.attributes.push_back({"region", "south", minera::AttributeKind::kString});
  second.events = {make_event("A", "alice")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  const auto all = minera::get_attribute_values(log, "activity");
  assert(all.at("A") == 3 && all.at("B") == 1);
  const auto once = minera::get_attribute_values(log, "activity", true);
  assert(once.at("A") == 2 && once.at("B") == 1);

  minera::AttributeStatisticsOptions resource_options;
  resource_options.attribute_key = "org:resource";
  const auto resources =
      minera::discover_attribute_statistics(log, resource_options);
  assert(resources.values.at("alice") == 3);
  assert(std::string(minera::attribute_statistics_scope_name(
             resources.scope)) == "EVENTS");

  minera::AttributeStatisticsOptions trace_options;
  trace_options.scope = minera::AttributeStatisticsScope::kTraces;
  trace_options.attribute_key = "region";
  const auto regions =
      minera::discover_attribute_statistics(log, trace_options);
  // As pm4py's DataFrame get_trace_attribute_values: one count per event row,
  // and the `case:` column name selects the same trace attribute.
  assert(regions.values.at("north") == 3 && regions.values.at("south") == 1);
  trace_options.attribute_key = "case:region";
  assert(minera::discover_attribute_statistics(log, trace_options).values ==
         regions.values);
  const auto json = minera::attribute_statistics_json(regions);
  assert(json.find("\"family\":\"ATTRIBUTE_STATISTICS\"") !=
         std::string::npos);
  assert(json.find("\"scope\":\"TRACES\"") != std::string::npos);
  const auto event_attributes =
      minera::get_all_event_attributes_from_log(log);
  assert(event_attributes.contains("concept:name") &&
         event_attributes.contains("channel"));
  const auto trace_attributes =
      minera::get_all_trace_attributes_from_log(log);
  assert(trace_attributes.contains("region"));

  // Missing cells are not counted: a blank CSV resource (no first-class
  // value) and a float NaN cell, which a DataFrame reads as NaN.
  {
    auto sparse = fixture();
    sparse.traces[0].events[0].resource.clear();
    minera::EventAttribute nan{"score", "nan", minera::AttributeKind::kFloat};
    nan.typed_value = std::numeric_limits<double>::quiet_NaN();
    sparse.traces[0].events[1].attributes.push_back(nan);
    minera::EventAttribute one{"score", "1.0", minera::AttributeKind::kFloat};
    one.typed_value = 1.0;
    sparse.traces[1].events[0].attributes.push_back(one);
    auto nan_resource = sparse.traces[1].events[0];
    nan_resource.resource = "nan";
    nan_resource.resource_state = minera::FirstClassAttributeState::kPresent;
    nan_resource.resource_attribute = nan;
    nan_resource.resource_attribute->key = "org:resource";
    sparse.traces[1].events.push_back(nan_resource);
    minera::AttributeStatisticsOptions options;
    options.attribute_key = "org:resource";
    const auto counted = minera::discover_attribute_statistics(sparse, options).values;
    assert((counted == std::map<std::string, std::uint64_t>{{"alice", 2}, {"bob", 1}}));
    options.attribute_key = "score";
    assert((minera::discover_attribute_statistics(sparse, options).values ==
            std::map<std::string, std::uint64_t>{{"1.0", 2}}));
  }

  // A CSV DataFrame keeps each row's own case cell: only the filled rows
  // count, and a case whose rows differ counts each value.
  {
    using Counts = std::map<std::string, std::uint64_t>;
    const auto csv = [](const char* text) {
      std::istringstream input(text);
      return minera::import_csv_event_log(input);
    };
    minera::AttributeStatisticsOptions options;
    options.scope = minera::AttributeStatisticsScope::kTraces;
    options.attribute_key = "case:x";
    const auto blanks = csv(
        "case_id,activity,timestamp,case:x\n"
        "c1,A,2024-01-01 00:00:00,u\nc1,B,2024-01-01 00:01:00,\n"
        "c1,C,2024-01-01 00:02:00,\nc2,A,2024-01-01 00:00:00,u\n"
        "c2,B,2024-01-01 00:01:00,\n");
    assert((minera::discover_attribute_statistics(blanks, options).values == Counts{{"u", 2}}));
    const auto mixed = csv(
        "case_id,activity,timestamp,case:x\n"
        "c1,A,2024-01-01 00:00:00,\nc1,B,2024-01-01 00:01:00,u\n"
        "c2,A,2024-01-01 00:00:00,u\nc2,B,2024-01-01 00:01:00,v\n");
    assert((minera::discover_attribute_statistics(mixed, options).values ==
            Counts{{"u", 2}, {"v", 1}}));
    options.attribute_key = "x";
    assert((minera::discover_attribute_statistics(mixed, options).values ==
            Counts{{"u", 2}, {"v", 1}}));
    // The events scope reads the same per-row cells, and the case-id column.
    options.scope = minera::AttributeStatisticsScope::kEvents;
    options.attribute_key = "case:x";
    assert((minera::discover_attribute_statistics(mixed, options).values ==
            Counts{{"u", 2}, {"v", 1}}));
    options.attribute_key = "case_id";
    assert((minera::discover_attribute_statistics(mixed, options).values ==
            Counts{{"c1", 2}, {"c2", 2}}));
  }

  // Python dict-key equality, as pm4py.get_event_attribute_values and
  // get_trace_attribute_values on the read_xes DataFrame (object columns):
  // 1, 1.0 and True are one key (0, 0.0 and False too), the string '1'
  // stays apart, and each key keeps its first-seen cell. Observed results:
  // a1 events {1: 6, 0: 2, '1': 1, 2.5: 1}, once per case {1: 3, '1': 1,
  // 0: 1, 2.5: 1}, case:tx {1: 10}; a2 events {True: 3, 0.0: 2}, once per
  // case {True: 2, 0.0: 1}, case:tx {True: 5}.
  {
    using Entry = std::tuple<std::string, std::string, std::uint64_t>;
    const auto entries = [](const minera::AttributeStatisticsResult& r) {
      std::vector<Entry> out;
      for (const auto& e : r.entries) out.emplace_back(e.value, e.type, e.count);
      return out;
    };
    const auto load = [](const char* text) {
      std::istringstream input(text);
      return minera::import_xes_event_log(input);
    };
    const auto a1 = load(R"XES(<?xml version="1.0" encoding="UTF-8"?><log xes.version="1.0"><trace><string key="concept:name" value="c0"/><int key="tx" value="1"/><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:00"/><int key="x" value="1"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:01"/><float key="x" value="1.0"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:02"/><boolean key="x" value="true"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:03"/><string key="x" value="1"/></event></trace><trace><string key="concept:name" value="c1"/><boolean key="tx" value="true"/><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:00"/><boolean key="x" value="true"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:01"/><float key="x" value="2.5"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:02"/><int key="x" value="0"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:03"/><boolean key="x" value="false"/></event></trace><trace><string key="concept:name" value="c2"/><float key="tx" value="1.0"/><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:00"/><float key="x" value="1.0"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:01"/><int key="x" value="1"/></event></trace></log>)XES");
    const auto a2 = load(R"XES(<?xml version="1.0" encoding="UTF-8"?><log xes.version="1.0"><trace><string key="concept:name" value="c0"/><boolean key="tx" value="true"/><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:00"/><boolean key="x" value="true"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:01"/><int key="x" value="1"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:02"/><float key="x" value="0.0"/></event><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:03"/><int key="x" value="0"/></event></trace><trace><string key="concept:name" value="c1"/><int key="tx" value="1"/><event><string key="concept:name" value="A"/><date key="time:timestamp" value="2020-01-01T00:00:00"/><float key="x" value="1.0"/></event></trace></log>)XES");
    minera::AttributeStatisticsOptions o;
    o.attribute_key = "x";
    auto r = minera::discover_attribute_statistics(a1, o);
    // Entries follow the reference's value_counts order: descending count,
    // equal counts in first appearance.
    assert((entries(r) == std::vector<Entry>{{"1", "int", 6}, {"0", "int", 2},
                                              {"1", "str", 1}, {"2.5", "float", 1}}));
    // The text map adds keys that only share a text.
    assert((r.values == std::map<std::string, std::uint64_t>{{"0", 2}, {"1", 7}, {"2.5", 1}}));
    const auto json = minera::attribute_statistics_json(r);
    assert(json.find("\"entries\":[{\"value\":\"1\",\"type\":\"int\",\"count\":6}") !=
           std::string::npos);
    // `values` keys follow the entries, each text at its first entry.
    assert(json.find("\"values\":{\"1\":7,\"0\":2,\"2.5\":1}") != std::string::npos);
    // Once per case the ties follow the sorted [case id, value] groups:
    // c0 holds 1 and '1', c1 holds 0 and 2.5.
    o.keep_once_per_case = true;
    assert((entries(minera::discover_attribute_statistics(a1, o)) ==
            std::vector<Entry>{{"1", "int", 3}, {"1", "str", 1}, {"0", "int", 1},
                               {"2.5", "float", 1}}));
    assert((entries(minera::discover_attribute_statistics(a2, o)) ==
            std::vector<Entry>{{"true", "bool", 2}, {"0.0", "float", 1}}));
    o.keep_once_per_case = false;
    assert((entries(minera::discover_attribute_statistics(a2, o)) ==
            std::vector<Entry>{{"true", "bool", 3}, {"0.0", "float", 2}}));
    o.scope = minera::AttributeStatisticsScope::kTraces;
    o.attribute_key = "case:tx";
    assert((entries(minera::discover_attribute_statistics(a1, o)) ==
            std::vector<Entry>{{"1", "int", 10}}));
    assert((entries(minera::discover_attribute_statistics(a2, o)) ==
            std::vector<Entry>{{"true", "bool", 5}}));
  }

  // Reference value_counts order on string columns: get_event_attribute_values
  // gives {'B': 3, 'Z': 1, 'A': 1}, once per case {'B': 2, 'Z': 1, 'A': 1},
  // and {'B': 2, 'A': 1, 'Z': 1} once the case ids swap (the groups sort by
  // case id text, k1 before k10, not by row order); get_trace_attribute_values
  // counts rows: {'y': 3, 'x': 2}.
  {
    minera::EventLog ordered;
    const auto add = [&](std::string id, std::string region,
                         std::vector<std::string> activities) {
      minera::Trace trace;
      trace.case_id = std::move(id);
      trace.attributes.push_back({"region", std::move(region),
                                  minera::AttributeKind::kString});
      for (auto& activity : activities) trace.events.push_back(make_event(activity, "r"));
      ordered.traces.push_back(std::move(trace));
    };
    add("k1", "x", {"Z", "B"});
    add("k10", "y", {"A", "B", "B"});
    minera::AttributeStatisticsOptions o;
    o.attribute_key = "activity";
    const auto texts = [](const minera::AttributeStatisticsResult& r) {
      std::vector<std::pair<std::string, std::uint64_t>> out;
      for (const auto& e : r.entries) out.emplace_back(e.value, e.count);
      return out;
    };
    using Pairs = std::vector<std::pair<std::string, std::uint64_t>>;
    const auto events = minera::discover_attribute_statistics(ordered, o);
    assert((texts(events) == Pairs{{"B", 3}, {"Z", 1}, {"A", 1}}));
    assert(minera::attribute_statistics_json(events).find(
               "\"values\":{\"B\":3,\"Z\":1,\"A\":1}") != std::string::npos);
    o.keep_once_per_case = true;
    assert((texts(minera::discover_attribute_statistics(ordered, o)) ==
            Pairs{{"B", 2}, {"Z", 1}, {"A", 1}}));
    std::swap(ordered.traces[0].case_id, ordered.traces[1].case_id);
    assert((texts(minera::discover_attribute_statistics(ordered, o)) ==
            Pairs{{"B", 2}, {"A", 1}, {"Z", 1}}));
    o.keep_once_per_case = false;
    o.scope = minera::AttributeStatisticsScope::kTraces;
    o.attribute_key = "region";
    assert((texts(minera::discover_attribute_statistics(ordered, o)) ==
            Pairs{{"y", 3}, {"x", 2}}));
  }

  // Review Y2-3: the read_xes DataFrame column dtype. An int column with a
  // missing cell, or ints mixed with floats, is float64, so the reference
  // counts {1.0: 1, 2.0: 1} for x and {3.0: 2} for case:kind below; ints
  // mixed with a bool or a string stay ints (object column). With
  // dataframe_column_types off (a native log) every cell keeps its type.
  {
    using Entry = std::tuple<std::string, std::string, std::uint64_t>;
    const auto entries = [](const minera::AttributeStatisticsResult& r) {
      std::vector<Entry> out;
      for (const auto& e : r.entries) out.emplace_back(e.value, e.type, e.count);
      return out;
    };
    const auto load = [](const std::string& text) {
      std::istringstream input(text);
      return minera::import_xes_event_log(input);
    };
    const auto event = [](const std::string& activity, const std::string& extra) {
      return "<event><string key=\"concept:name\" value=\"" + activity +
             "\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:00:00\"/>" +
             extra + "</event>";
    };
    const std::string head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
    const auto missing = load(
        head + "<trace><string key=\"concept:name\" value=\"c0\"/><int key=\"kind\" value=\"3\"/>" +
        event("a", "<int key=\"x\" value=\"1\"/>") + event("b", "") + "</trace>" +
        "<trace><string key=\"concept:name\" value=\"c1\"/>" +
        event("a", "<int key=\"x\" value=\"2\"/>") + "</trace></log>");
    minera::AttributeStatisticsOptions o;
    o.attribute_key = "x";
    assert((entries(minera::discover_attribute_statistics(missing, o)) ==
            std::vector<Entry>{{"1.0", "float", 1}, {"2.0", "float", 1}}));
    assert((minera::discover_attribute_statistics(missing, o).values ==
            std::map<std::string, std::uint64_t>{{"1.0", 1}, {"2.0", 1}}));
    o.keep_once_per_case = true;
    assert((entries(minera::discover_attribute_statistics(missing, o)) ==
            std::vector<Entry>{{"1.0", "float", 1}, {"2.0", "float", 1}}));
    o.keep_once_per_case = false;
    o.dataframe_column_types = false;
    assert((entries(minera::discover_attribute_statistics(missing, o)) ==
            std::vector<Entry>{{"1", "int", 1}, {"2", "int", 1}}));
    o.dataframe_column_types = true;
    o.scope = minera::AttributeStatisticsScope::kTraces;
    o.attribute_key = "case:kind";
    assert((entries(minera::discover_attribute_statistics(missing, o)) ==
            std::vector<Entry>{{"3.0", "float", 2}}));
    o.scope = minera::AttributeStatisticsScope::kEvents;
    o.attribute_key = "x";
    // Ints mixed with floats, no missing cell: {2.5: 1, 1.0: 1}.
    const auto mixed = load(
        head + "<trace><string key=\"concept:name\" value=\"c0\"/>" +
        event("a", "<float key=\"x\" value=\"2.5\"/>") +
        event("b", "<int key=\"x\" value=\"1\"/>") + "</trace></log>");
    assert((entries(minera::discover_attribute_statistics(mixed, o)) ==
            std::vector<Entry>{{"2.5", "float", 1}, {"1.0", "float", 1}}));
    // Complete ints stay int64; a bool or string keeps the object column.
    const auto complete = load(
        head + "<trace><string key=\"concept:name\" value=\"c0\"/>" +
        event("a", "<int key=\"x\" value=\"1\"/>") +
        event("b", "<int key=\"x\" value=\"1\"/>") + "</trace></log>");
    assert((entries(minera::discover_attribute_statistics(complete, o)) ==
            std::vector<Entry>{{"1", "int", 2}}));
    const auto object = load(
        head + "<trace><string key=\"concept:name\" value=\"c0\"/>" +
        event("a", "<int key=\"x\" value=\"1\"/>") + event("b", "") +
        event("c", "<string key=\"x\" value=\"y\"/>") + "</trace></log>");
    assert((entries(minera::discover_attribute_statistics(object, o)) ==
            std::vector<Entry>{{"1", "int", 1}, {"y", "str", 1}}));
    // Once per case the value level holds the distinct keys: 1 (True merged
    // into it), 0.5 and 3 are all numbers, so {1.0: 1, 0.5: 1, 3.0: 1}; with
    // True seen first the level keeps a bool: {True: 1, 0.5: 1, 3: 1}.
    const auto merged = [&](const std::string& first, const std::string& second) {
      return load(head + "<trace><string key=\"concept:name\" value=\"c0\"/>" +
                  event("a", first) + event("b", second) +
                  "</trace><trace><string key=\"concept:name\" value=\"c1\"/>" +
                  event("a", "<float key=\"x\" value=\"0.5\"/>") +
                  event("b", "<int key=\"x\" value=\"3\"/>") + "</trace></log>");
    };
    const std::string one = "<int key=\"x\" value=\"1\"/>";
    const std::string yes = "<boolean key=\"x\" value=\"true\"/>";
    assert((entries(minera::discover_attribute_statistics(merged(one, yes), o)) ==
            std::vector<Entry>{{"1", "int", 2}, {"0.5", "float", 1}, {"3", "int", 1}}));
    o.keep_once_per_case = true;
    assert((entries(minera::discover_attribute_statistics(merged(one, yes), o)) ==
            std::vector<Entry>{{"1.0", "float", 1}, {"0.5", "float", 1}, {"3.0", "float", 1}}));
    assert((entries(minera::discover_attribute_statistics(merged(yes, one), o)) ==
            std::vector<Entry>{{"true", "bool", 1}, {"0.5", "float", 1}, {"3", "int", 1}}));
    o.keep_once_per_case = false;
    // Review Z1-2: a float64 cell holds the int rounded to a double, so
    // 2^53 + 1 and 2^53 are one key: {9007199254740992.0: 2}.
    const auto rounded = load(
        head + "<trace><string key=\"concept:name\" value=\"c0\"/>" +
        event("a", "<int key=\"x\" value=\"9007199254740993\"/>") +
        event("b", "<int key=\"x\" value=\"9007199254740992\"/>") + event("c", "") +
        "</trace></log>");
    assert((entries(minera::discover_attribute_statistics(rounded, o)) ==
            std::vector<Entry>{{"9007199254740992.0", "float", 2}}));
    assert((minera::discover_attribute_statistics(rounded, o).values ==
            std::map<std::string, std::uint64_t>{{"9007199254740992.0", 2}}));
    o.keep_once_per_case = true;
    assert((entries(minera::discover_attribute_statistics(rounded, o)) ==
            std::vector<Entry>{{"9007199254740992.0", "float", 1}}));
    // In the object column of the value level (a bool merged into 1) the
    // ints become floats and merge there: {1.0: 2, 9007199254740992.0: 2, 0.5: 1}.
    const auto level = load(
        head + "<trace><string key=\"concept:name\" value=\"c1\"/>" + event("a", one) +
        event("b", yes) + event("c", "<float key=\"x\" value=\"0.5\"/>") +
        "</trace><trace><string key=\"concept:name\" value=\"c2\"/>" +
        event("a", "<int key=\"x\" value=\"9007199254740993\"/>") +
        "</trace><trace><string key=\"concept:name\" value=\"c3\"/>" +
        event("a", "<int key=\"x\" value=\"9007199254740992\"/>") + event("b", one) +
        "</trace></log>");
    assert((entries(minera::discover_attribute_statistics(level, o)) ==
            std::vector<Entry>{{"1.0", "float", 2},
                               {"9007199254740992.0", "float", 2},
                               {"0.5", "float", 1}}));
    o.keep_once_per_case = false;
  }

  // Review AU1: a CSV upload reads the format_dataframe frame (pm4py 2.7.22
  // on read_csv + format_dataframe(case_id="cid", activity_key="act",
  // timestamp_key="ts")): an int column with a blank is float64 ({1.0: 1,
  // 2.0: 1}), a date-text column is UTC dates ({10:00 UTC: 2, 09:30 UTC: 1}),
  // a missing key is a KeyError, the trace scope reads a column of that name
  // before case:<key>, and once per case cannot group case:concept:name.
  {
    using Entry = std::tuple<std::string, std::string, std::uint64_t>;
    const auto entries = [](const minera::AttributeStatisticsResult& r) {
      std::vector<Entry> out;
      for (const auto& e : r.entries) out.emplace_back(e.value, e.type, e.count);
      return out;
    };
    std::istringstream input(
        "cid,act,ts,when,n\n"
        "c1,a,2020-01-01 10:00:00,2020-01-01 12:00:00+02:00,1\n"
        "c1,b,2020-01-01 11:00:00,2020-01-01T09:30:00Z,\n"
        "c2,a,2020-01-02 10:00:00,2020-01-01 12:00:00+02:00,2\n");
    minera::CsvImportOptions import;
    import.case_id_column = "cid";
    import.activity_column = "act";
    import.timestamp_column = "ts";
    const auto frame = minera::import_csv_event_log(input, import);
    assert(frame.dataframe_columns);
    minera::AttributeStatisticsOptions o;
    o.attribute_key = "n";
    assert((entries(minera::discover_attribute_statistics(frame, o)) ==
            std::vector<Entry>{{"1.0", "float", 1}, {"2.0", "float", 1}}));
    o.attribute_key = "when";
    const auto dates = entries(minera::discover_attribute_statistics(frame, o));
    assert(dates.size() == 2 && std::get<1>(dates[0]) == "date" && std::get<2>(dates[0]) == 2 &&
           std::get<0>(dates[0]).starts_with("2020-01-01 10:00:00") && std::get<2>(dates[1]) == 1 &&
           std::get<0>(dates[1]).starts_with("2020-01-01 09:30:00"));
    const auto error_of = [&](const minera::AttributeStatisticsOptions& options) {
      try { (void)minera::discover_attribute_statistics(frame, options); }
      catch (const std::invalid_argument& error) { return std::string(error.what()); }
      return std::string();
    };
    o.attribute_key = "zz";
    assert(error_of(o) == "attribute statistics: KeyError: 'zz'");
    o.attribute_key = "case:concept:name";
    o.keep_once_per_case = true;
    assert(error_of(o) == "attribute statistics: ValueError: cannot insert case:concept:name, already exists");
    o.keep_once_per_case = false;
    o.scope = minera::AttributeStatisticsScope::kTraces;
    o.attribute_key = "cid";
    assert((entries(minera::discover_attribute_statistics(frame, o)) ==
            std::vector<Entry>{{"c1", "str", 2}, {"c2", "str", 1}}));
    o.attribute_key = "concept:name";
    assert((entries(minera::discover_attribute_statistics(frame, o)) ==
            std::vector<Entry>{{"a", "str", 2}, {"b", "str", 1}}));
  }

  bool rejected = false;
  try {
    (void)minera::discover_attribute_statistics(log, {});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "Attribute-statistics tests passed\n";
}
