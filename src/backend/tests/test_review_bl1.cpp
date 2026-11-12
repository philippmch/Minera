// Review BL1: pm4py's DataFrame temporal profile on frames with missing
// cells (NaN case or activity, NaT start or completion), on a frame's own
// "@@index" column, on a CSV upload's rows (format_dataframe's "@@index"
// orders each case's rows by the upload timestamp; blank cells are NaT), and
// the keyed upload with a numeric activity column, a NaT start, or dates and
// numbers as activities. Values are taken from pm4py 2.7.22 black-box on the
// same inputs (review-harness round-bl, bl1/mk.py).
#include "minera/dataframe_columns.hpp"
#include "minera/event_data.hpp"
#include "minera/temporal_profile.hpp"
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

constexpr int kMissing = -1;

std::string time(int seconds) { return "2020-01-01T00:" + two(seconds / 60) + ":" + two(seconds % 60) + "+00:00"; }

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

TemporalProfile all_pairs(const std::string& activities) {
  TemporalProfile profile;
  for (const char source : activities)
    for (const char target : activities)
      profile.entries.push_back({std::string(1, source), std::string(1, target), 1, 1.0, 1.0});
  return profile;
}

struct Row {
  const char* id;
  const char* activity;
  int completion, start;
};

int missing_cells() {
  // A NaN activity row leaves the groups and the rows without a case form
  // one more case (listed at its first row); a NaT start sorts last and,
  // without an "@@index", never pairs; a NaT completion pairs only as a
  // target. The "" start reads as NaT too.
  const Row rows[] = {{"p", "a", 50, 40}, {nullptr, "a", 1, 1}, {"p", "b", 20, kMissing}, {"p", "c", kMissing, 55},
                      {"p", "d", 45, 45}, {"p", nullptr, 30, 30}, {"p", "e", 48, kMissing}, {"q", "a", 10, 10},
                      {"q", "b", 30, kMissing}, {"q", "c", kMissing, 30}, {nullptr, "c", 5, 5}};
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start"};
  for (const auto& row : rows) {
    const auto date = [](const char* key, int at) {
      return at == kMissing ? std::nullopt : cell(key, time(at), AttributeKind::kDate);
    };
    table.rows.push_back({row.id ? cell("case:concept:name", row.id) : std::nullopt,
                          row.activity ? cell("concept:name", row.activity) : std::nullopt,
                          date("time:timestamp", row.completion),
                          row.activity && std::string(row.activity) == "e" ? cell("start", "", AttributeKind::kDate)
                                                                           : date("start", row.start)});
  }
  TemporalProfileOptions options;
  options.source_timestamp_attribute = "time:timestamp";
  options.target_timestamp_attribute = "start";
  int failures = 0;
  // Before, the strict default refused the missing cells, and the lenient
  // path dropped a NaT completion's row and the rows without a case.
  failures += check("temporal profile (missing cells)", entries(discover_temporal_profile_dataframe(table, options)),
                    "a,c:9.666666666666666,8.962886439832502 d,c:10,0");
  options.zeta = 0.0;
  failures += check("temporal conformance (missing cells)",
                    deviations(conform_temporal_profile_dataframe(table, all_pairs("abcde"), options)),
                    "[a,c:5,4 d,c:10,9][a,c:4,3][a,c:20,19]");
  return failures;
}

int own_index() {
  // A frame's own "@@index" orders the pairs: a row pairs with the rows of a
  // higher index only (b with c here).
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "@@index"};
  const std::pair<const char*, const char*> rows[] = {{"a", "3"}, {"b", "1"}, {"c", "2"}, {"d", "0"}};
  for (int i = 0; i < 4; ++i)
    table.rows.push_back({cell("case:concept:name", "r"), cell("concept:name", rows[i].first),
                          cell("time:timestamp", time(10 * (i + 1)), AttributeKind::kDate),
                          cell("@@index", rows[i].second, AttributeKind::kInt)});
  int failures = 0;
  failures += check("temporal profile (own @@index)", entries(discover_temporal_profile_dataframe(table, {})), "b,c:10,0");
  TemporalProfileOptions options;
  options.zeta = 0.0;
  failures += check("temporal conformance (own @@index)",
                    deviations(conform_temporal_profile_dataframe(table, all_pairs("abcd"), options)), "[b,c:10,9]");
  return failures;
}

int csv_rows() {
  // A CSV upload's rows in file order, blanks as "": format_dataframe drops
  // s's row without a timestamp and orders each case by the timestamp, then
  // file order; a row pairs with the rows after it there, whatever their
  // start (s: c 5, b 12, a 30, d 40 by completion).
  const Row rows[] = {{"s", "a", 30, 25}, {"t", "b", 10, kMissing}, {"s", "b", 12, 2}, {"s", "c", 5, 20},
                      {"t", "a", 4, 4}, {"s", "d", 40, 40}, {"s", "e", kMissing, 50}, {"t", "c", 40, 30}};
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start"};
  for (const auto& row : rows) {
    const auto text = [](int at) {
      return at == kMissing ? std::string() : "2020-01-01 00:" + two(at / 60) + ":" + two(at % 60);
    };
    table.rows.push_back({cell("case:concept:name", row.id), cell("concept:name", row.activity),
                          cell("time:timestamp", text(row.completion)), cell("start", text(row.start))});
  }
  TemporalProfileOptions options;
  options.source_timestamp_attribute = "time:timestamp";
  options.target_timestamp_attribute = "start";
  options.frame_index_rows = true;
  int failures = 0;
  // Before, each case's rows paired by their start order only.
  failures += check("temporal profile (CSV rows)", entries(discover_temporal_profile_dataframe(table, options)),
                    "a,c:26,0 a,d:10,0 b,a:13,0 b,c:20,0 b,d:28,0 c,a:20,0 c,d:35,0");
  options.zeta = 0.0;
  failures += check("temporal conformance (CSV rows)",
                    deviations(conform_temporal_profile_dataframe(table, all_pairs("abcde"), options)),
                    "[b,a:13,12 b,d:28,27 c,a:20,19 c,d:35,34 a,d:10,9][a,c:26,25 b,c:20,19]");
  // The upload's timestamp in "ts" beside another "time:timestamp" column:
  // format_dataframe copies "ts" over it, so "ts" orders the frame. Before,
  // the "time:timestamp" header did.
  EventTable alias;
  alias.columns = {"case:concept:name", "concept:name", "ts", "start", "time:timestamp"};
  for (std::size_t k = 0; k < table.rows.size(); ++k) {
    auto upload = table.rows[k][2];
    upload->key = "ts";
    upload->first_class_role = FirstClassAttributeRole::kTimestamp;
    alias.rows.push_back({table.rows[k][0], table.rows[k][1], upload, table.rows[k][3],
                          cell("time:timestamp", "2020-01-01 00:" + two(59 - static_cast<int>(k)) + ":00")});
  }
  options.source_timestamp_attribute = "ts";
  failures += check("temporal profile (CSV rows, upload timestamp)",
                    entries(discover_temporal_profile_dataframe(alias, options)),
                    "a,c:26,0 a,d:10,0 b,a:13,0 b,c:20,0 b,d:28,0 c,a:20,0 c,d:35,0");
  return failures;
}

std::string xes_log(const std::string& traces) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" + traces + "</log>";
}

int keyed_numbers() {
  // org:resource as ints with one event without (a float column, so pm4py's
  // keys are 1.0, 2.0, 10.0) and an event without a start.
  struct Event {
    const char* resource;
    int completion, start;
  };
  const std::vector<std::pair<const char*, std::vector<Event>>> traces = {
      {"x", {{"1", 0, 0}, {"2", 10, 5}, {nullptr, 12, 12}, {"1", 30, kMissing}, {"10", 40, 35}}},
      {"y", {{"2", 3, 1}, {"1", 20, 20}}}};
  std::string text;
  for (const auto& [id, events] : traces) {
    text += std::string("<trace><string key=\"concept:name\" value=\"") + id + "\"/>";
    for (const auto& event : events) {
      text += "<event><string key=\"concept:name\" value=\"a\"/>";
      if (event.resource) text += std::string("<int key=\"org:resource\" value=\"") + event.resource + "\"/>";
      text += "<date key=\"time:timestamp\" value=\"" + time(event.completion) + "\"/>";
      if (event.start != kMissing) text += "<date key=\"start\" value=\"" + time(event.start) + "\"/>";
      text += "</event>";
    }
    text += "</trace>";
  }
  const auto log = xes_upload(xes_log(text));
  TemporalProfileOptions options;
  options.activity_attribute = "org:resource";
  options.source_timestamp_attribute = "time:timestamp";
  options.target_timestamp_attribute = "start";
  // Before, the EventLog algorithm answered for the numbers.
  return check("temporal profile (keyed numbers)", entries(discover_temporal_profile_as_uploaded(log, options)),
               "1.0,10.0:35,0 1.0,2.0:5,0 2.0,1.0:17,0 2.0,10.0:25,0");
}

int keyed_dates() {
  const auto log_of = [](const std::vector<std::pair<const char*, const char*>>& values) {
    std::string text = "<trace><string key=\"concept:name\" value=\"z\"/>";
    for (std::size_t i = 0; i < values.size(); ++i)
      text += std::string("<event><") + values[i].first + " key=\"mix\" value=\"" + values[i].second +
              "\"/><date key=\"time:timestamp\" value=\"" + time(10 * static_cast<int>(i)) + "\"/></event>";
    return xes_upload(xes_log(text + "</trace>"));
  };
  TemporalProfileOptions options;
  options.activity_attribute = "mix";
  int failures = 0;
  // One date beside one number: each level of the pairs' groups sorts.
  failures += check("temporal profile (keyed date and number)",
                    entries(discover_temporal_profile_as_uploaded(
                        log_of({{"date", "2020-02-02T00:00:00+00:00"}, {"int", "1"}}), options)),
                    "2020-02-02 00:00:00+00:00,1:10,0");
  // A date and a number among the sources: pm4py raises (a TypeError), and
  // the EventLog algorithm answers.
  const auto mixed = log_of({{"date", "2020-02-02T00:00:00+00:00"}, {"int", "1"}, {"float", "2.5"}});
  failures += check("temporal profile (keyed dates beside numbers)",
                    entries(discover_temporal_profile_as_uploaded(mixed, options)),
                    entries(discover_temporal_profile(mixed, options)));
  return failures;
}

}  // namespace

int main() {
  const int failures = missing_cells() + own_index() + csv_rows() + keyed_numbers() + keyed_dates();
  if (failures != 0) {
    std::cerr << failures << " review BL1 failure(s)\n";
    return 1;
  }
  std::cout << "review BL1 tests passed\n";
  return 0;
}
