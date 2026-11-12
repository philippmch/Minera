// Review BK1: pm4py's reading of XES booleans, Declare discovery and
// conformance on the frame's case slices for table inputs, the temporal
// profile of an event stream (read through its EventLog), the DataFrame
// temporal profile with separate completion and start columns (each case's
// rows sorted by start, then completion), the resource as the activity key,
// and get_variants on an upload whose trace names repeat apart. Values are
// taken from pm4py 2.7.22 black-box on the same inputs (review-harness
// round-bk, bk1/mk.py).
#include "minera/dataframe_columns.hpp"
#include "minera/declare.hpp"
#include "minera/event_data.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/variant_statistics.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace minera;

namespace {

EventLog xes_upload(const std::string& text) {
  std::istringstream xes(text);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  return log;
}

std::string two(int value) { return std::string(value < 10 ? "0" : "") + std::to_string(value); }

// One <trace> per row: trace names repeat apart where cases interleave.
EventLog rows(const std::vector<std::pair<std::string, std::string>>& rows) {
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  for (std::size_t i = 0; i < rows.size(); ++i)
    text += "<trace><string key=\"concept:name\" value=\"" + rows[i].first + "\"/><event><string key=\"concept:name\" value=\"" +
            rows[i].second + "\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:00:" + two(static_cast<int>(i)) +
            "+00:00\"/></event></trace>";
  return xes_upload(text + "</log>");
}

std::string number(double value) {
  char text[64];
  const auto end = std::to_chars(text, text + sizeof text, value).ptr;
  return std::string(text, end);
}

int check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return 0;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
  return 1;
}

std::optional<EventAttribute> cell(std::string key, std::string value, AttributeKind kind = AttributeKind::kString) {
  return EventAttribute{std::move(key), std::move(value), kind};
}

std::string time(int seconds, const char* fraction = "") {
  return "2020-01-01T00:" + two(seconds / 60) + ":" + two(seconds % 60) + fraction + "+00:00";
}

// "source,target:mean,deviation" per entry, sorted.
std::string entries(const TemporalProfile& profile) {
  std::vector<std::string> items;
  for (const auto& entry : profile.entries)
    items.push_back(entry.source + "," + entry.target + ":" + number(entry.mean_seconds) + "," +
                    number(entry.standard_deviation_seconds));
  std::sort(items.begin(), items.end());
  std::string text;
  for (const auto& item : items) text += (text.empty() ? "" : " ") + item;
  return text;
}

// "[source,target:elapsed,distance ...]" per case, in the result's order.
std::string deviations(const TemporalConformanceResult& result) {
  std::string text;
  for (const auto& trace : result) {
    text += "[";
    for (std::size_t i = 0; i < trace.size(); ++i)
      text += (i ? " " : "") + trace[i].source + "," + trace[i].target + ":" + number(trace[i].elapsed_seconds) + "," +
              number(trace[i].standard_deviation_distance);
    text += "]";
  }
  return text;
}

int booleans() {
  // pm4py keeps a boolean as True when its value is "true" in any case and
  // as False otherwise (1, yes, no value), and a float's leading plus goes.
  const char* values[] = {"TRUE", "1", "yes", nullptr, "false", "True", "tRuE"};
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\"><trace><string key=\"concept:name\" value=\"c\"/>";
  for (int i = 0; i < 7; ++i) {
    text += "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"" + time(i) + "\"/>";
    text += values[i] ? std::string("<boolean key=\"flag\" value=\"") + values[i] + "\"/>" : "<boolean key=\"flag\"/>";
    if (i == 0) text += "<float key=\"num\" value=\"+1.5\"/>";
    text += "</event>";
  }
  const auto log = xes_upload(text + "</trace></log>");
  std::string seen;
  for (const auto& event : log.traces.at(0).events) {
    const auto* flag = event.attribute("flag");
    seen += (seen.empty() ? "" : " ") + (flag ? *flag : std::string("-"));
  }
  const auto* num = log.traces.at(0).events.at(0).attribute("num");
  seen += " num=" + (num ? *num : std::string("-"));
  // Before, the file's spellings were kept, and the table transport refused
  // them.
  return check("xes booleans", seen, "true false false false false true true num=1.5");
}

EventTable declare_table() {
  // x b, y c, x a, w b, y c: x's slice is rows 0 and 1 (b, c), y's rows 1
  // and 2 (c, a), w's row 3 (b).
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  const std::pair<const char*, const char*> cells[] = {{"x", "b"}, {"y", "c"}, {"x", "a"}, {"w", "b"}, {"y", "c"}};
  for (int i = 0; i < 5; ++i)
    table.rows.push_back({cell("case:concept:name", cells[i].first), cell("concept:name", cells[i].second),
                          cell("time:timestamp", time(i), AttributeKind::kDate)});
  return table;
}

int declare() {
  const auto table = declare_table();
  const EventDataView view(table);
  int failures = 0;
  const auto discovered = discover_declare_frame_slices(view);
  std::string pairs;
  for (const auto& rule : discovered.rules.at(kDeclareNonCoexistence))
    pairs += (pairs.empty() ? "" : " ") + rule.first + "," + rule.second + ":" + std::to_string(rule.support);
  // The merged cases (x: b a, y: c c, w: b) give "b,c:3 c,b:3".
  failures += check("declare discovery (table slices)", pairs, "a,b:3 b,a:3");
  DeclareModel model;
  model.add(kDeclareExistence, DeclareConstraint{"a", "", false, 1, 1});
  std::string fitness;
  for (const auto& result : check_declare_conformance_frame_slices(view, model))
    fitness += (fitness.empty() ? "" : " ") + std::to_string(result.no_dev_total) + "/" + number(result.dev_fitness);
  // Cases x, y, w (pm4py lists them by sorted id: w, x, y); the merged
  // case x (b, a) would fit.
  failures += check("declare conformance (table slices)", fitness, "1/0 0/1 1/0");
  return failures;
}

int stream() {
  // Case 1: a at 10 s, b at 5 s, c at 20.25 s in stream order; pm4py reads
  // the stream through its EventLog and pairs the events in that order.
  EventStream stream;
  const auto event = [&](const char* id, const char* activity, const std::string& at) {
    stream.events.push_back({{EventAttribute{"case:concept:name", id, AttributeKind::kString},
                              EventAttribute{"concept:name", activity, AttributeKind::kString},
                              EventAttribute{"time:timestamp", at, AttributeKind::kDate}}});
  };
  event("1", "a", time(10));
  event("2", "a", time(0));
  event("1", "b", time(5));
  event("1", "c", time(20, ".25"));
  event("2", "c", time(30));
  int failures = 0;
  failures += check("stream temporal profile", entries(discover_temporal_profile(stream)),
                    "a,c:20.125,13.965358928434314 b,c:15.25,0");
  TemporalProfile profile;
  profile.entries = {{"a", "c", 1, 5.0, 1.0}, {"b", "c", 1, 10.0, 2.0}, {"a", "b", 1, 1.0, 1.0}};
  TemporalProfileOptions options;
  options.zeta = 0.0;
  failures += check("stream temporal conformance", deviations(conform_temporal_profile(EventDataView(stream), profile, options)),
                    "[a,c:10.25,5.25 b,c:15.25,2.625][a,c:30,25]");
  return failures;
}

int split_columns() {
  // Completion and start per row; each case's rows are sorted by start, then
  // completion, and a pair counts when the target starts no earlier than the
  // source completes.
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start"};
  const struct { const char* id; const char* activity; int completion, start; } cells[] = {
      {"p", "a", 50, 40}, {"p", "b", 20, 10}, {"p", "c", 60, 55}, {"p", "d", 45, 45},
      {"q", "a", 10, 10}, {"q", "b", 30, 10}, {"q", "c", 30, 30}};
  for (const auto& row : cells)
    table.rows.push_back({cell("case:concept:name", row.id), cell("concept:name", row.activity),
                          cell("time:timestamp", time(row.completion), AttributeKind::kDate),
                          cell("start", time(row.start), AttributeKind::kDate)});
  TemporalProfileOptions options;
  options.source_timestamp_attribute = "time:timestamp";
  options.target_timestamp_attribute = "start";
  int failures = 0;
  failures += check("split-column temporal profile (dataframe)", entries(discover_temporal_profile_dataframe(table, options)),
                    "a,b:0,0 a,c:12.5,10.606601717798213 b,a:20,0 b,c:17.5,24.748737341529164 b,d:25,0 d,c:10,0");
  TemporalProfile profile;
  for (const char* source : {"a", "b", "c", "d"})
    for (const char* target : {"a", "b", "c", "d"}) profile.entries.push_back({source, target, 1, 1.0, 1.0});
  options.zeta = 0.0;
  failures += check("split-column temporal conformance (dataframe)",
                    deviations(conform_temporal_profile_dataframe(table, profile, options)),
                    "[b,a:20,19 b,d:25,24 b,c:35,34 a,c:5,4 d,c:10,9][a,b:0,1 a,c:20,19 b,c:0,1]");
  return failures;
}

int resource_key() {
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\"><trace><string key=\"concept:name\" value=\"c\"/>";
  const std::pair<const char*, int> events[] = {{"ann", 0}, {"bob", 7}, {"ann", 10}};
  for (const auto& [resource, at] : events)
    text += std::string("<event><string key=\"concept:name\" value=\"a\"/><string key=\"org:resource\" value=\"") + resource +
            "\"/><date key=\"time:timestamp\" value=\"" + time(at) + "\"/></event>";
  std::istringstream xes(text + "</trace></log>");
  const auto log = import_xes_dataframe_log(xes);
  TemporalProfileOptions options;
  options.activity_attribute = "org:resource";
  // Before, the first-class resource was not found among the event's
  // attributes.
  return check("temporal profile (resource activity)", entries(discover_temporal_profile(log, options)),
               "ann,ann:10,0 ann,bob:7,0 bob,ann:3,0");
}

int keyed_upload() {
  // y a, x b, y b, x a, y c with completion and start; pm4py's DataFrame
  // discovery sorts x as a (start 4), b (start 5) and y as b, a, c.
  const struct { const char* id; const char* activity; int completion, start; } cells[] = {
      {"y", "a", 30, 25}, {"x", "b", 10, 5}, {"y", "b", 12, 2}, {"x", "a", 4, 4}, {"y", "c", 40, 40}};
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  for (const auto& row : cells)
    text += std::string("<trace><string key=\"concept:name\" value=\"") + row.id + "\"/><event><string key=\"concept:name\" value=\"" +
            row.activity + "\"/><date key=\"time:timestamp\" value=\"" + time(row.completion) + "\"/><date key=\"start\" value=\"" +
            time(row.start) + "\"/></event></trace>";
  const auto log = xes_upload(text + "</log>");
  TemporalProfileOptions options;
  options.source_timestamp_attribute = "time:timestamp";
  options.target_timestamp_attribute = "start";
  // Each trace in file order (pm4py's EventLog algorithm) gives
  // "a,c:10,0 b,c:28,0".
  return check("temporal profile (keyed upload)", entries(discover_temporal_profile_as_uploaded(log, options)),
               "a,b:1,0 a,c:10,0 b,a:13,0 b,c:28,0");
}

int variants() {
  // x b, y c, x a, w b, w c, y a: slices x (b, c), y (c, a), w (b, c).
  const auto log = rows({{"x", "b"}, {"y", "c"}, {"x", "a"}, {"w", "b"}, {"w", "c"}, {"y", "a"}});
  std::string text;
  for (const auto& entry : discover_variant_statistics_as_uploaded(log).variants) {
    std::string activities;
    for (const auto& activity : entry.activities) activities += (activities.empty() ? "" : ",") + activity;
    text += (text.empty() ? "" : " ") + activities + ":" + std::to_string(entry.count);
  }
  // The merged cases give "c,a:1 b,c:1 b,a:1".
  return check("variant statistics (upload slices)", text, "b,c:2 c,a:1");
}

}  // namespace

int main() {
  const int failures = booleans() + declare() + stream() + split_columns() + resource_key() + keyed_upload() + variants();
  if (failures != 0) {
    std::cerr << failures << " review BK1 failure(s)\n";
    return 1;
  }
  std::cout << "review BK1 tests passed\n";
  return 0;
}
