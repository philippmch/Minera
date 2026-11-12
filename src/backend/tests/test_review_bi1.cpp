// Review BI1: pm4py's DataFrame filters and discoveries on an XES upload,
// whose frame keeps read_xes's file order (case ids not sorted, a case's rows
// apart when its trace name repeats): the activity split labels the cases by
// sorted id, each from its slice of the frame's rows (from its first row on,
// as many as it has), and hands the labels to the rows in row order;
// filter_between marks each case's slice in sorted id order, a later case
// replacing an earlier one's mark; the consecutive grouping merges adjacent
// rows only; the declare discovery of /api/mine reads the slices. pm4py's
// EventLog temporal profile takes statistics.mean and statistics.stdev of
// the epoch-second durations, its DataFrame profile pandas' arithmetic.
// Values are taken from pm4py 2.7.22 black-box on the same inputs
// (review-harness round-bi, bi1/mk.py and bi1/mk2.py).
#include "minera/dataframe_columns.hpp"
#include "minera/declare.hpp"
#include "minera/log_filter.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
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

std::string event(const std::string& activity, int second) {
  char time[32];
  std::snprintf(time, sizeof time, "2020-01-01T00:00:%02d+00:00", second);
  return "<event><string key=\"concept:name\" value=\"" + activity + "\"/><date key=\"time:timestamp\" value=\"" +
         time + "\"/></event>";
}

// One <trace> per row: trace names repeat apart where cases interleave.
EventLog rows(const std::vector<std::pair<std::string, std::string>>& rows) {
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  for (std::size_t i = 0; i < rows.size(); ++i)
    text += "<trace><string key=\"concept:name\" value=\"" + rows[i].first + "\"/>" +
            event(rows[i].second, static_cast<int>(i)) + "</trace>";
  return xes_upload(text + "</log>");
}

// "case:rows" per case, by case id, rows by input order.
std::string kept(const EventLog& log, const LogFilterOptions& options) {
  std::map<std::string, std::vector<std::size_t>> cases;
  for (const auto& trace : filter_event_log(log, options).traces)
    for (const auto& event : trace.events) cases[trace.case_id].push_back(event.input_order);
  std::string text;
  for (auto& [id, orders] : cases) {
    std::sort(orders.begin(), orders.end());
    text += (text.empty() ? "" : " ") + id + ":";
    for (std::size_t i = 0; i < orders.size(); ++i) text += (i ? "," : "") + std::to_string(orders[i]);
  }
  return text;
}

int check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return 0;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
  return 1;
}

int filters() {
  int failures = 0;
  // c2 (a, b), c10 (a, c), c1 (b, a) in file order; sorted c1, c10, c2.
  const auto unsorted = rows({{"c2", "a"}, {"c2", "b"}, {"c10", "a"}, {"c10", "c"}, {"c1", "b"}, {"c1", "a"}});
  LogFilterOptions split;
  split.activity_split_activities = {"a"};
  // Before: "c1##@@0:4 c1##@@1:5 c10##@@1:2,3 c2##@@1:0,1".
  failures += check("activity split (this)", kept(unsorted, split), "c1##@@0:0 c1##@@1:1 c10##@@1:2,3 c2##@@1:4,5");
  split.activity_split_cut_mode = ActivitySplitCutMode::kNext;
  failures += check("activity split (next)", kept(unsorted, split),
                    "c1##@@0:0,1 c10##@@0:2 c10##@@1:3 c2##@@0:4 c2##@@1:5");
  split.activity_split_cut_mode = ActivitySplitCutMode::kThis;
  split.top_variants = 1;
  // The variants of the split frame count its slices too.
  failures += check("activity split, top variant", kept(unsorted, split), "c2##@@1:4,5");

  // x a, y a, x b, z a, y c, x b, w a, z b, y b: x's slice is rows 0-2
  // (a, a, b), y's rows 1-3 (a, b, a); y's stretch replaces x's on rows 1, 2.
  const auto interleaved =
      rows({{"x", "a"}, {"y", "a"}, {"x", "b"}, {"z", "a"}, {"y", "c"}, {"x", "b"}, {"w", "a"}, {"z", "b"}, {"y", "b"}});
  LogFilterOptions between;
  between.between_start = "a";
  between.between_end = "b";
  // Before: "x##@@0:0,2 y##@@0:1,4,8 z##@@0:3,7".
  failures += check("between", kept(interleaved, between), "x##@@0:0 y##@@0:1,2");

  // x a, y a, x a, x a: only rows 2 and 3 are adjacent.
  const auto repeated = rows({{"x", "a"}, {"y", "a"}, {"x", "a"}, {"x", "a"}});
  LogFilterOptions grouping;
  grouping.consecutive_activity_grouping = ConsecutiveActivityGroupingOptions{};
  // Before: "x:0 y:1".
  failures += check("consecutive grouping (first)", kept(repeated, grouping), "x:0,2 y:1");
  grouping.consecutive_activity_grouping->filter_type = CaseGroupingFilterType::kLast;
  failures += check("consecutive grouping (last)", kept(repeated, grouping), "x:0,3 y:1");
  return failures;
}

int discoveries() {
  int failures = 0;
  // x a, y c, x b: x reads (a, c), y reads (c).
  const auto model = discover_declare_as_uploaded(rows({{"x", "a"}, {"y", "c"}, {"x", "b"}}));
  std::string rules;
  for (const auto& [family, entries] : model.rules)
    for (const auto& rule : entries)
      rules += family + "(" + rule.first + (rule.binary ? "," + rule.second : "") + ")=" +
               std::to_string(static_cast<int>(rule.support)) + " ";
  // Before: also the non co-existence of a and c, and no existence of c.
  failures += check("declare", rules,
                    "exactly_one(c)=2 existence(c)=2 nonchainsuccession(c,a)=2 nonsuccession(c,a)=2 ");

  // a -> b after 180.1 s, 60 s and 420.5 s, a day apart.
  EventLog log;
  const char* gaps[] = {"00:03:00.1", "00:01:00", "00:07:00.5"};
  for (int day = 0; day < 3; ++day) {
    Trace trace;
    trace.case_id = std::string(1, static_cast<char>('u' + day));
    const std::string date = "2020-01-0" + std::to_string(day + 1) + "T";
    for (const auto& [activity, clock] : {std::pair<std::string, std::string>{"a", "00:00:00"}, {"b", gaps[day]}}) {
      Event event;
      event.activity = activity;
      event.timestamp = date + clock + "+00:00";
      trace.events.push_back(event);
    }
    log.traces.push_back(trace);
  }
  const auto value = [](const TemporalProfile& profile) {
    char text[80];
    std::snprintf(text, sizeof text, "%.17g %.17g", profile.entries.at(0).mean_seconds,
                  profile.entries.at(0).standard_deviation_seconds);
    return std::string(text);
  };
  // statistics.mean and statistics.stdev; before (a running Welford mean):
  // "220.19999996821088 183.5648931681225".
  failures += check("EventLog temporal profile", value(discover_temporal_profile(log)),
                    "220.19999996821085 183.56489316812252");
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  for (int day = 0; day < 3; ++day)
    text += "<trace><string key=\"concept:name\" value=\"" + log.traces[day].case_id + "\"/>" +
            "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"" +
            log.traces[day].events[0].timestamp + "\"/></event>" +
            "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"" +
            log.traces[day].events[1].timestamp + "\"/></event></trace>";
  // pandas: Timedelta durations, the compensated mean, Welford's deviation.
  failures += check("upload temporal profile", value(discover_temporal_profile_as_uploaded(xes_upload(text + "</log>"), {})),
                    "220.20000000000002 183.56489315770594");
  return failures;
}

}  // namespace

int main() {
  const int failures = filters() + discoveries();
  if (failures != 0) {
    std::cerr << failures << " review BI1 failure(s)\n";
    return 1;
  }
  std::cout << "review BI1 tests passed\n";
  return 0;
}
