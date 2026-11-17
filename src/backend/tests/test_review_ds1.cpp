// Review DS1: pm4py's DataFrame temporal profile conformance on CSV uploads
// whose profile lacks some of the log's activity pairs or holds pairs the log
// never shows (tc1.py's domain: shuffled frames, repeated case ids, equal
// timestamps, zero standard deviations), and on an empty profile. pm4py
// merges the frame's row pairs with the profile's frame on the two activity
// columns; the deviations come per case (in the order of the cases' first
// rows) in the pairs' order, the same order as a full profile, and an empty
// profile's frame has no columns, so pandas raises KeyError on the activity
// key (Polars: column not found), while the EventLog conformance lists each
// case without deviations. Values are taken from pm4py 2.7.22 black-box on
// the same CSV text (review-harness round-ds, bind/gen_ds1.py; the server
// sweep tc/tc1.py agreed on 2,338 of 2,400 calls, the other 62 were empty
// profiles).
#include "minera/csv_event_log_importer.hpp"
#include "minera/event_data.hpp"
#include "minera/temporal_profile.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace minera;

namespace {

struct Call {
  const char* profile;  // "a>b:mean:stdev" entries
  double zeta;
  bool error;
  const char* expected;  // cases joined by '|', deviations "a>b:elapsed:distance" ("max" = sys.maxsize)
};

struct Case {
  const char* csv;
  std::vector<Call> calls;
};

const std::vector<Case> kCases = {
#include "review_ds1_cases.inc"
};

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream input(text);
  while (std::getline(input, part, separator)) parts.push_back(part);
  if (!text.empty() && text.back() == separator) parts.emplace_back();
  return parts;
}

TemporalProfile profile_of(const std::string& text) {
  TemporalProfile profile;
  for (const auto& item : split(text, ' ')) {
    if (item.empty()) continue;
    const auto arrow = item.find('>');
    const auto fields = split(item.substr(arrow + 1), ':');
    profile.entries.push_back({item.substr(0, arrow), fields[0], 1, std::strtod(fields[1].c_str(), nullptr),
                               std::strtod(fields[2].c_str(), nullptr)});
  }
  return profile;
}

bool close(double seen, const std::string& expected) {
  const double value = std::strtod(expected.c_str(), nullptr);
  return std::abs(seen - value) <= 1e-9 * std::max(1.0, std::abs(value));
}

// Whether the conformance matches the expected text, case by case in order.
bool matches(const TemporalConformanceResult& result, const std::string& expected) {
  // One case without deviations is the empty text.
  const auto cases = expected.empty() ? std::vector<std::string>{""} : split(expected, '|');
  if (cases.size() != result.size()) return false;
  for (std::size_t c = 0; c < cases.size(); ++c) {
    std::vector<std::string> items;
    for (const auto& item : split(cases[c], ' '))
      if (!item.empty()) items.push_back(item);
    if (items.size() != result[c].size()) return false;
    for (std::size_t d = 0; d < items.size(); ++d) {
      const auto& deviation = result[c][d];
      const auto arrow = items[d].find('>');
      const auto fields = split(items[d].substr(arrow + 1), ':');
      if (items[d].substr(0, arrow) != deviation.source || fields[0] != deviation.target) return false;
      if (!close(deviation.elapsed_seconds, fields[1])) return false;
      if (fields[2] == "max" ? !deviation.zero_variance_sentinel
                             : deviation.zero_variance_sentinel || !close(deviation.standard_deviation_distance, fields[2]))
        return false;
    }
  }
  return true;
}

std::string text_of(const TemporalConformanceResult& result) {
  std::string out;
  for (std::size_t c = 0; c < result.size(); ++c) {
    if (c) out += '|';
    for (std::size_t d = 0; d < result[c].size(); ++d) {
      const auto& deviation = result[c][d];
      if (d) out += ' ';
      std::ostringstream item;
      item.precision(17);
      item << deviation.source << '>' << deviation.target << ':' << deviation.elapsed_seconds << ':';
      if (deviation.zero_variance_sentinel) item << "max";
      else item << deviation.standard_deviation_distance;
      out += item.str();
    }
  }
  return out;
}

CsvEventData import(const char* csv) {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(csv);
  return import_csv_event_data(input, options);
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t calls = 0;
  for (std::size_t c = 0; c < kCases.size(); ++c) {
    const auto data = import(kCases[c].csv);
    for (const auto& call : kCases[c].calls) {
      ++calls;
      TemporalProfileOptions options;
      options.zeta = call.zeta;
      options.frame_index_rows = true;
      std::string seen;
      bool raised = false, ok = false;
      try {
        const auto result = conform_temporal_profile_dataframe(data.table, profile_of(call.profile), options);
        seen = text_of(result);
        ok = !call.error && matches(result, call.expected);
      } catch (const std::exception& error) {
        raised = true;
        seen = error.what();
        ok = call.error && seen == call.expected;
      }
      if (ok) continue;
      ++failures;
      std::cerr << "case " << c << " profile " << call.profile << (raised ? " (raised)" : "") << ":\n  seen     " << seen
                << "\n  expected " << call.expected << "\n";
    }
  }

  // The empty profile: pandas' KeyError on the activity key (a custom one
  // too), Polars' column error on a lazy frame, no error on the EventLog.
  const auto data = import("case,act,ts\nc0,a,2024-01-01 00:00:00\nc0,b,2024-01-01 01:00:00\n");
  const auto expect_error = [&](const char* what, const auto& run, const std::string& expected) {
    ++calls;
    try {
      run();
    } catch (const std::exception& error) {
      if (error.what() == expected) return;
      std::cerr << what << ": raised " << error.what() << "\n";
      ++failures;
      return;
    }
    std::cerr << what << ": did not raise\n";
    ++failures;
  };
  TemporalProfileOptions options;
  options.frame_index_rows = true;
  expect_error("empty profile, frame", [&] { (void)conform_temporal_profile_dataframe(data.table, {}, options); },
               "KeyError: 'concept:name'");
  expect_error("empty profile, view", [&] { (void)conform_temporal_profile(EventDataView(data.table), {}, options); },
               "KeyError: 'concept:name'");
  const auto lazy = LazyEventTable::from_table(data.table);
  expect_error("empty profile, lazy frame",
               [&] { (void)conform_temporal_profile(EventDataView(lazy), {}, options); },
               "unable to find column \"concept:name\"; valid columns: []");
  auto custom = options;
  custom.activity_attribute = "act";
  expect_error("empty profile, frame, custom key",
               [&] { (void)conform_temporal_profile_dataframe(data.table, {}, custom); }, "KeyError: 'act'");
  ++calls;
  const auto on_log = conform_temporal_profile_log(data.log, {}, {});
  if (on_log.size() != 1 || !on_log[0].empty()) {
    std::cerr << "empty profile, log: expected one case without deviations\n";
    ++failures;
  }

  std::cout << calls << " temporal conformance calls compared, " << failures << " failed\n";
  return failures == 0 && calls == 209 ? 0 : 1;
}
