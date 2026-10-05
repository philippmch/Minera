#include "minera/conformance.hpp"
#include "minera/token_replay_duration_error.hpp"
#include "minera/token_replay_error.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "business_hours.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::optional<EventAttribute> selected_field(
    const Event& event, const std::string& selector, bool timestamp,
    bool* present = nullptr) {
  if (present) *present = true;
  const EventAttribute* selected = nullptr;
  int priority = 0;
  std::size_t matches = 0;
  for (const auto& attribute : event.attributes) {
    const int candidate = attribute.key == selector ? 3
        : !attribute.first_class_source_key.empty() &&
          attribute.first_class_source_key == selector ? 2 : 0;
    if (candidate == 0 || candidate < priority) continue;
    if (candidate > priority) {
      selected = &attribute;
      priority = candidate;
      matches = 1;
    } else {
      ++matches;
    }
  }
  if (matches > 1)
    throw std::invalid_argument("token replay duration: duplicate selected attribute");
  if (selected) {
    if (selected->first_class_role != FirstClassAttributeRole::kNone &&
        selected->first_class_state == FirstClassAttributeState::kMissing) {
      if (present) { *present = false; return std::nullopt; }
      throw TokenReplayKeyError(selector);
    }
    return *selected;
  }
  const auto& source = timestamp ? event.timestamp_source_key : event.activity_source_key;
  const auto& cell = timestamp ? event.timestamp_attribute : event.activity_attribute;
  const std::string_view projected_key = !source.empty() ? std::string_view(source)
      : cell ? !cell->first_class_source_key.empty()
          ? std::string_view(cell->first_class_source_key) : std::string_view(cell->key)
      : timestamp ? std::string_view("time:timestamp") : std::string_view("concept:name");
  if (projected_key == selector) {
    if (cell && cell->first_class_role != FirstClassAttributeRole::kNone &&
        cell->first_class_state == FirstClassAttributeState::kMissing) {
      if (present) { *present = false; return std::nullopt; }
      throw TokenReplayKeyError(selector);
    }
    if (cell) return *cell;
    const auto state = timestamp ? event.timestamp_state : event.activity_state;
    const auto& value = timestamp ? event.timestamp : event.activity;
    if (state == FirstClassAttributeState::kNull ||
        state == FirstClassAttributeState::kPresent || !value.empty()) {
      EventAttribute field{selector, value,
                           timestamp ? AttributeKind::kDate : AttributeKind::kString};
      field.is_null = state == FirstClassAttributeState::kNull;
      return field;
    }
  }
  if (present) { *present = false; return std::nullopt; }
  throw TokenReplayKeyError(selector);
}

bool is_null(const EventAttribute& field) {
  return field.is_null || field.first_class_state == FirstClassAttributeState::kNull;
}

std::optional<std::string> selected_value(
    const Event& event, const std::string& selector, bool timestamp,
    bool* present = nullptr) {
  const auto field = selected_field(event, selector, timestamp, present);
  return !field || is_null(*field) ? std::nullopt
                                 : std::optional<std::string>(field->value);
}

std::string_view endpoint_type(const EventAttribute& field) {
  if (is_null(field)) return "NoneType";
  switch (field.container) {
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kContainer: return "dict";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
    case AttributeContainerKind::kScalar: break;
  }
  if (field.scalar_wrapper || field.scalar_children_list || !field.children.empty())
    return "dict";
  if (std::holds_alternative<NativeDate>(field.typed_value)) return "datetime.datetime";
  if (std::holds_alternative<NativeNA>(field.typed_value)) return "NAType";
  if (std::holds_alternative<bool>(field.typed_value)) return "bool";
  if (std::holds_alternative<double>(field.typed_value)) return "float";
  if (std::holds_alternative<std::int64_t>(field.typed_value) ||
      std::holds_alternative<std::uint64_t>(field.typed_value)) return "int";
  switch (field.kind) {
    case AttributeKind::kDate: return "datetime.datetime";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "bool";
    case AttributeKind::kBytes: return "bytes";
    case AttributeKind::kNA: return "NAType";
    case AttributeKind::kString:
    case AttributeKind::kId: return "str";
  }
  return "str";
}

[[noreturn]] void no_total_seconds(std::string_view type) {
  throw TokenReplayDurationError("builtins.AttributeError",
      "'" + std::string(type) + "' object has no attribute 'total_seconds'");
}

double endpoint_difference(const EventAttribute& last, const EventAttribute& first) {
  const auto left = endpoint_type(last);
  const auto right = endpoint_type(first);
  if (left == "datetime.datetime" && right == "datetime.datetime") {
    const auto parse = [](const EventAttribute& field) {
      const auto* date = std::get_if<NativeDate>(&field.typed_value);
      const auto value = parse_timestamp_exact(date ? date->value : field.value);
      if (!value)
        throw std::invalid_argument("token replay duration: invalid native date endpoint");
      return *value;
    };
    const auto end = parse(last);
    const auto start = parse(first);
    if (end.timezone_aware != start.timezone_aware)
      throw TokenReplayDurationError("builtins.TypeError",
          "can't subtract offset-naive and offset-aware datetimes");
    const auto microseconds = [](const ParsedExactTimestamp& value) {
      return value.seconds * detail::kMicrosecondsPerSecond + value.nanoseconds / 1000;
    };
    return detail::epoch_seconds_from_microseconds(
        microseconds(end) - microseconds(start));
  }
  const auto numeric = [](std::string_view type) {
    return type == "int" || type == "float" || type == "bool";
  };
  if (numeric(left) && numeric(right))
    no_total_seconds(left == "float" || right == "float" ? "float" : "int");
  const auto na_operand = [&](std::string_view type) {
    return numeric(type) || type == "str" || type == "bytes" ||
           type == "datetime.datetime" || type == "NAType";
  };
  if ((left == "NAType" && na_operand(right)) ||
      (right == "NAType" && na_operand(left))) no_total_seconds("NAType");
  const auto set = [](std::string_view type) {
    return type == "set" || type == "frozenset";
  };
  if (set(left) && set(right)) no_total_seconds(left);
  throw TokenReplayDurationError("builtins.TypeError",
      "unsupported operand type(s) for -: '" + std::string(left) +
      "' and '" + std::string(right) + "'");
}

void validate_indices(const EventLog& log, const std::vector<std::size_t>& indices) {
  for (auto index : indices)
    if (index >= log.traces.size())
      throw std::out_of_range("token replay duration: trace index out of range");
}

std::vector<std::size_t> unique_indices(
    const EventLog& log, const std::vector<std::size_t>& indices) {
  validate_indices(log, indices);
  std::set<std::size_t> unique(indices.begin(), indices.end());
  return {unique.begin(), unique.end()};
}

bool contains_activity(const Trace& trace, const std::optional<std::string>& activity,
                       const std::string& selector) {
  for (const auto& event : trace.events) {
    bool present = false;
    const auto value = selected_value(event, selector, false, &present);
    if (present && value == activity) return true;
  }
  return false;
}

TokenReplayDurationComparison compare(
    const EventLog& log, std::vector<std::size_t> fit,
    std::vector<std::size_t> problem, const std::string& selector) {
  TokenReplayDurationComparison result;
  result.fit_trace_indices = std::move(fit);
  result.problem_trace_indices = std::move(problem);
  result.fit_count = result.fit_trace_indices.size();
  result.problem_count = result.problem_trace_indices.size();
  result.fit_median_seconds = token_replay_median_case_duration(
      log, result.fit_trace_indices, selector);
  result.problem_median_seconds = token_replay_median_case_duration(
      log, result.problem_trace_indices, selector);
  result.relative_throughput = result.fit_median_seconds > 0
      ? result.problem_median_seconds / result.fit_median_seconds : 0;
  return result;
}

}  // namespace

double token_replay_case_duration(const Trace& trace,
                                 const std::string& timestamp_attribute) {
  if (trace.events.empty())
    throw TokenReplayDurationError("builtins.IndexError", "list index out of range");
  const auto last = selected_field(trace.events.back(), timestamp_attribute, true);
  const auto first = selected_field(trace.events.front(), timestamp_attribute, true);
  return endpoint_difference(*last, *first);
}

double token_replay_median_case_duration(
    const EventLog& log, const std::vector<std::size_t>& trace_indices,
    const std::string& timestamp_attribute) {
  validate_indices(log, trace_indices);
  if (trace_indices.empty())
    throw TokenReplayDurationError("statistics.StatisticsError", "no median for empty data");
  std::vector<double> values;
  values.reserve(trace_indices.size());
  for (auto index : trace_indices)
    values.push_back(token_replay_case_duration(log.traces[index], timestamp_attribute));
  std::sort(values.begin(), values.end());
  const auto middle = values.size() / 2;
  return values.size() % 2 ? values[middle]
                          : values[middle - 1] / 2.0 + values[middle] / 2.0;
}

std::vector<TokenReplayTransitionDurationDiagnostics>
token_replay_transition_duration_diagnostics(
    const EventLog& log, const std::vector<TokenReplayTransitionDurationCohort>& cohorts,
    const TokenReplayOptions& options) {
  std::vector<TokenReplayTransitionDurationDiagnostics> result;
  for (const auto& cohort : cohorts) {
    auto problem = unique_indices(log, cohort.problem_trace_indices);
    if (problem.empty()) continue;
    std::vector<std::size_t> fit;
    for (std::size_t index = 0; index < log.traces.size(); ++index) {
      if (!std::binary_search(problem.begin(), problem.end(), index) &&
          contains_activity(log.traces[index], cohort.activity, options.activity_attribute))
        fit.push_back(index);
    }
    if (fit.empty()) continue;
    TokenReplayTransitionDurationDiagnostics row;
    static_cast<TokenReplayDurationComparison&>(row) =
        compare(log, std::move(fit), std::move(problem), options.timestamp_attribute);
    row.transition_index = cohort.transition_index;
    result.push_back(std::move(row));
  }
  return result;
}

std::vector<TokenReplayUnknownActivityDurationDiagnostics>
token_replay_unknown_activity_duration_diagnostics(
    const EventLog& log, const std::vector<TokenReplayUnknownActivityDurationCohort>& cohorts,
    const TokenReplayOptions& options) {
  std::vector<std::optional<std::string>> activities;
  for (const auto& cohort : cohorts) {
    validate_indices(log, cohort.problem_trace_indices);
    activities.push_back(cohort.activity_is_null
        ? std::nullopt : std::optional<std::string>(cohort.activity));
  }
  std::vector<std::size_t> baseline;
  for (std::size_t index = 0; index < log.traces.size(); ++index) {
    bool contains_unknown = false;
    for (const auto& activity : activities) {
      if (contains_activity(log.traces[index], activity, options.activity_attribute)) {
        contains_unknown = true;
        break;
      }
    }
    if (!contains_unknown) baseline.push_back(index);
  }
  std::vector<TokenReplayUnknownActivityDurationDiagnostics> result;
  for (const auto& cohort : cohorts) {
    auto problem = unique_indices(log, cohort.problem_trace_indices);
    if (problem.empty()) continue;
    std::vector<std::size_t> fit;
    for (auto index : baseline)
      if (!std::binary_search(problem.begin(), problem.end(), index)) fit.push_back(index);
    if (fit.empty()) continue;
    TokenReplayUnknownActivityDurationDiagnostics row;
    static_cast<TokenReplayDurationComparison&>(row) =
        compare(log, std::move(fit), std::move(problem), options.timestamp_attribute);
    row.activity = cohort.activity;
    row.activity_is_null = cohort.activity_is_null;
    result.push_back(std::move(row));
  }
  return result;
}

TokenReplayDurationDiagnostics compute_token_replay_duration_diagnostics(
    const EventLog& log, const PetriNet& net,
    const std::vector<TokenReplayTraceDiagnostics>& traces,
    const TokenReplayOptions& options) {
  std::map<std::size_t, std::vector<std::size_t>> transition_problems;
  std::map<std::optional<std::string>, std::vector<std::size_t>> activity_problems;
  const auto& activity_selector = options.activity_attribute;
  for (const auto& trace : traces) {
    if (trace.trace_index >= log.traces.size())
      throw std::out_of_range("token replay duration: diagnostic trace index out of range");
    for (const auto& event : trace.event_diagnostics) {
      if (event.event_index >= log.traces[trace.trace_index].events.size())
        throw std::out_of_range("token replay duration: diagnostic event index out of range");
      for (auto transition : event.transitions_with_problems) {
        if (transition >= net.transitions().size())
          throw std::out_of_range("token replay duration: diagnostic transition index out of range");
        transition_problems[transition].push_back(trace.trace_index);
      }
    }
    for (auto event_index : trace.unknown_event_indices) {
      if (event_index >= log.traces[trace.trace_index].events.size())
        throw std::out_of_range("token replay duration: unknown event index out of range");
      const auto activity = selected_value(log.traces[trace.trace_index].events[event_index],
                                           activity_selector, false);
      activity_problems[activity].push_back(trace.trace_index);
    }
  }
  std::vector<TokenReplayTransitionDurationCohort> transition_cohorts;
  for (auto& [index, problem] : transition_problems) {
    const auto activity = net.transitions()[index].activity;
    transition_cohorts.push_back({index,
        activity ? std::optional<std::string>(net.activities().at(*activity)) : std::nullopt,
        std::move(problem)});
  }
  std::vector<TokenReplayUnknownActivityDurationCohort> activity_cohorts;
  for (auto& [activity, problem] : activity_problems)
    activity_cohorts.push_back({activity.value_or(""), !activity, std::move(problem)});
  TokenReplayDurationDiagnostics result;
  result.timestamp_attribute = options.timestamp_attribute;
  result.transitions = token_replay_transition_duration_diagnostics(
      log, transition_cohorts, options);
  result.unknown_activities = token_replay_unknown_activity_duration_diagnostics(
      log, activity_cohorts, options);
  return result;
}

}  // namespace minera
