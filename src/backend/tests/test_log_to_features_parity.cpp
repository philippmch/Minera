// Log-to-features tables against the public capture
// (docs/log-to-features-bank.json.gz, written to
// src/backend/tests/fixtures/log_to_features_bank.txt). Seeded logs go
// through the EVENT_BASED and TRACE_BASED variants under default, explicit,
// all-extra-feature and succession parameters, and with stretched timestamps
// through TEMPORAL with weekly and daily buckets; every feature name and
// value, or the error, must match. The public trace columns follow Python set order,
// which moves with the hash seed, so columns are compared by name.
#include "minera/log_to_features.hpp"
#include "minera/event_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, separator)) parts.push_back(part);
  if (!text.empty() && text.back() == separator) parts.emplace_back();
  return parts;
}

struct Call {
  std::string config;
  std::optional<std::string> error;
  std::vector<std::string> names;
  // Trace rows; for EVENT_BASED, the event rows of each trace.
  std::vector<std::vector<std::optional<double>>> rows;
  std::vector<std::size_t> row_traces;
  std::vector<std::string> bucket_times;  // TEMPORAL bucket labels
};

// The capture stretches every minute to this many for TEMPORAL calls.
constexpr int kTemporalScale = 300;

struct Case {
  std::string name;
  EventLog log;
  EventLog temporal_log;
  std::vector<Call> calls;
};

std::string timestamp(int minute) {
  char text[32];
  std::snprintf(text, sizeof text, "2024-%02d-%02dT%02d:%02d:00Z", minute / 1440 < 31 ? 1 : 2,
                minute / 1440 < 31 ? 1 + minute / 1440 : minute / 1440 - 30, minute / 60 % 24,
                minute % 60);
  return text;
}

void add_trace(EventLog& log, const std::vector<std::string>& tokens, int scale) {
  Trace trace;
  trace.case_id = "c" + std::to_string(log.traces.size());
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.attributes.push_back({"region", tokens.at(1), AttributeKind::kString});
  trace.attributes.push_back({"size", tokens.at(2), AttributeKind::kInt});
  for (std::size_t index = 3; index < tokens.size(); ++index) {
    const auto fields = split(tokens[index], ';');
    Event event{trace.case_id, fields.at(0), timestamp(scale * std::stoi(fields.at(1))), fields.at(2),
                trace.events.size()};
    event.activity_state = FirstClassAttributeState::kPresent;
    event.timestamp_state = FirstClassAttributeState::kPresent;
    event.resource_state = FirstClassAttributeState::kPresent;
    if (fields.at(3) != "-")
      event.attributes.push_back({"amount", fields[3],
                                  fields[3].find('.') == std::string::npos ? AttributeKind::kInt
                                                                           : AttributeKind::kFloat});
    if (fields.at(4) != "-") event.attributes.push_back({"kind", fields[4], AttributeKind::kString});
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
}

std::optional<double> value(const std::string& token) {
  if (token == "-") return std::nullopt;
  return std::strtod(token.c_str(), nullptr);
}

std::vector<Case> load(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  std::vector<Case> cases;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto tag = line.substr(0, 1);
    const auto rest = line.size() > 2 ? line.substr(2) : std::string();
    if (tag == "C") {
      cases.push_back({rest, {}, {}, {}});
    } else if (tag == "T") {
      add_trace(cases.back().log, split(line, ' '), 1);
      add_trace(cases.back().temporal_log, split(line, ' '), kTemporalScale);
    } else if (tag == "F") {
      Call call;
      call.config = rest;
      cases.back().calls.push_back(std::move(call));
    } else if (tag == "X") {
      cases.back().calls.back().error = rest;
    } else if (tag == "N") {
      cases.back().calls.back().names = split(rest, ' ');
    } else if (tag == "R") {
      auto& call = cases.back().calls.back();
      auto tokens = split(rest, ' ');
      if (call.config.starts_with("event")) {
        call.row_traces.push_back(std::stoul(tokens.front()));
        tokens.erase(tokens.begin());
      }
      if (call.config.starts_with("temporal")) {
        call.bucket_times.push_back(tokens.front());
        tokens.erase(tokens.begin());
      }
      std::vector<std::optional<double>> row;
      for (const auto& token : tokens) row.push_back(value(token));
      call.rows.push_back(std::move(row));
    }
  }
  return cases;
}

LogToFeaturesOptions options_for(const std::string& config) {
  LogToFeaturesOptions options;
  options.variant = config.starts_with("event")      ? LogToFeaturesVariant::EVENT_BASED
                    : config.starts_with("temporal") ? LogToFeaturesVariant::TEMPORAL
                                                     : LogToFeaturesVariant::TRACE_BASED;
  if (config == "temporal-daily") options.grouper_freq = "D";
  if (config == "event-explicit" || config == "trace-explicit") {
    options.event_string_attributes = {"concept:name", "kind"};
    options.event_numeric_attributes = {"amount"};
    options.event_string_attributes_supplied = options.event_numeric_attributes_supplied = true;
  }
  if (config == "trace-explicit") {
    options.case_string_attributes = {"region"};
    options.case_numeric_attributes = {"size"};
    options.case_string_attributes_supplied = options.case_numeric_attributes_supplied = true;
  }
  if (config == "trace-extras") options.enable_all_extra_features = true;
  if (config == "trace-succession") {
    options.event_string_attributes = {"concept:name"};
    options.event_successor_attributes = {"org:resource"};
    options.event_string_attributes_supplied = options.event_numeric_attributes_supplied = true;
    options.case_string_attributes_supplied = options.case_numeric_attributes_supplied = true;
    options.event_successor_attributes_supplied = true;
  }
  return options;
}

bool same(double actual, const std::optional<double>& expected) {
  if (!expected) return std::isnan(actual);
  return std::fabs(actual - *expected) <= 1e-9 * std::max(1.0, std::fabs(*expected));
}

// Epoch seconds of a public bucket label such as 2024-01-07T00:00:00+00:00.
double epoch_seconds(const std::string& label) {
  const int year = std::stoi(label.substr(0, 4)), month = std::stoi(label.substr(5, 2));
  const int day = std::stoi(label.substr(8, 2));
  const int y = year - (month <= 2), era = y / 400, yoe = y - era * 400;
  const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const long days = era * 146097L + yoe * 365L + yoe / 4 - yoe / 100 + doy - 719468;
  return days * 86400.0 + std::stoi(label.substr(11, 2)) * 3600.0 + std::stoi(label.substr(14, 2)) * 60.0 +
         std::stoi(label.substr(17, 2));
}

std::string temporal_case(const EventLog& log, const Call& call) {
  LogToFeaturesResult result;
  try {
    result = temporal_log_features(log, options_for(call.config));
  } catch (const std::exception& error) {
    return call.error ? "" : std::string("raised ") + error.what();
  }
  if (call.error) return "expected " + *call.error;
  std::vector<std::string> names(call.names.begin() + 1, call.names.end());
  if (result.feature_names != names) return "feature names differ";
  if (result.temporal_rows.size() != call.rows.size()) return "bucket count differs";
  for (std::size_t row = 0; row < call.rows.size(); ++row) {
    const auto& actual = result.temporal_rows[row];
    if (actual.timestamp != epoch_seconds(call.bucket_times[row]))
      return "bucket time differs at row " + std::to_string(row);
    for (std::size_t index = 0; index < names.size(); ++index) {
      const auto& cell = actual.values.at(index);
      if (!std::holds_alternative<double>(cell) || !same(std::get<double>(cell), call.rows[row].at(index)))
        return "value differs at row " + std::to_string(row) + " column " + names[index];
    }
  }
  return "";
}

// Empty when the native table matches the call, otherwise a description.
template <typename Compute>
std::string features_case(const Call& call, Compute compute) {
  LogToFeaturesResult result;
  try {
    result = compute();
  } catch (const std::exception& error) {
    return call.error ? "" : std::string("raised ") + error.what();
  }
  if (call.error) return "expected " + *call.error;
  auto expected_names = call.names, actual_names = result.feature_names;
  std::sort(expected_names.begin(), expected_names.end());
  std::sort(actual_names.begin(), actual_names.end());
  if (expected_names != actual_names) return "feature names differ";
  std::map<std::string, std::size_t> column;
  for (std::size_t index = 0; index < result.feature_names.size(); ++index)
    column[result.feature_names[index]] = index;

  // Public event rows are padded with zero rows up to the longest trace.
  std::vector<std::vector<double>> actual;
  std::vector<std::size_t> actual_traces;
  if (result.variant == LogToFeaturesVariant::EVENT_BASED) {
    std::size_t longest = 0;
    for (const auto& rows : result.event_rows) longest = std::max(longest, rows.size());
    for (std::size_t trace = 0; trace < result.event_rows.size(); ++trace) {
      for (std::size_t event = 0; event < longest; ++event) {
        actual.push_back(event < result.event_rows[trace].size()
                             ? result.event_rows[trace][event]
                             : std::vector<double>(result.feature_names.size(), 0.0));
        actual_traces.push_back(trace);
      }
    }
    if (actual_traces != call.row_traces) return "event row shape differs";
  } else {
    for (const auto& row : result.trace_rows) {
      std::vector<double> values;
      for (const auto& cell : row)
        values.push_back(std::holds_alternative<double>(cell) ? std::get<double>(cell) : NAN);
      actual.push_back(std::move(values));
    }
  }
  if (actual.size() != call.rows.size()) return "row count differs";
  for (std::size_t row = 0; row < actual.size(); ++row)
    for (std::size_t index = 0; index < call.names.size(); ++index)
      if (!same(actual[row].at(column.at(call.names[index])), call.rows[row].at(index)))
        return "value differs at row " + std::to_string(row) + " column " + call.names[index];
  return "";
}

std::string event_based_case(const EventLog& log, const Call& call) {
  return features_case(call, [&] { return event_based_log_features(log, options_for(call.config)); });
}

std::string trace_based_case(const EventLog& log, const Call& call) {
  return features_case(call, [&] { return trace_based_log_features(log, options_for(call.config)); });
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/log_to_features_bank.txt";
  int failures = 0;
  std::size_t compared = 0, errors = 0;
  std::map<std::string, std::size_t> failed_by_config;
  for (const auto& item : load(path)) {
    for (const auto& call : item.calls) {
      ++compared;
      if (call.error) ++errors;
      std::string detail;
      try {
        detail = call.config.starts_with("temporal") ? temporal_case(item.temporal_log, call)
                 : call.config.starts_with("event")  ? event_based_case(item.log, call)
                                                     : trace_based_case(item.log, call);
      } catch (const std::exception& error) {
        detail = std::string("threw ") + error.what();
      }
      if (!detail.empty()) {
        ++failures;
        ++failed_by_config[call.config];
        if (failures <= 40)
          std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), call.config.c_str(), detail.c_str());
      }
    }
  }
  for (const auto& [config, count] : failed_by_config)
    std::fprintf(stderr, "%s: %zu failing\n", config.c_str(), count);
  std::printf("%zu calls compared (%zu public errors)\n", compared, errors);
  if (compared < 960 || errors < 40) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
