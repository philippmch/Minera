#include "minera/conformance.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

void validate_selector(const std::string& selector) {
  if (selector.empty())
    throw std::invalid_argument("token replay duration: attribute selector must not be empty");
}

std::optional<std::string> selected_value(
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
      throw std::invalid_argument("token replay duration: selected attribute is missing");
    }
    if (selected->is_null || selected->first_class_state == FirstClassAttributeState::kNull)
      return std::nullopt;
    return selected->value;
  }
  const auto& source = timestamp ? event.timestamp_source_key : event.activity_source_key;
  const bool standard = timestamp ? selector == "time:timestamp" || selector == "timestamp"
                                  : selector == "concept:name" || selector == "activity";
  if (standard || (!source.empty() && source == selector)) {
    const auto state = timestamp ? event.timestamp_state : event.activity_state;
    const auto& value = timestamp ? event.timestamp : event.activity;
    if (state == FirstClassAttributeState::kNull) return std::nullopt;
    if (state == FirstClassAttributeState::kPresent || !value.empty()) return value;
    const auto& cell = timestamp ? event.timestamp_attribute : event.activity_attribute;
    if (cell) return cell->is_null ? std::nullopt : std::optional<std::string>(cell->value);
  }
  if (present) { *present = false; return std::nullopt; }
  throw std::invalid_argument("token replay duration: missing selected attribute '" + selector + "'");
}

double instant(const Event& event, const std::string& selector) {
  const auto value = selected_value(event, selector, true);
  const auto parsed = value ? parse_timestamp(*value) : std::nullopt;
  if (!parsed)
    throw std::invalid_argument("token replay duration: selected endpoint timestamp is null or invalid");
  return *parsed;
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
  validate_selector(timestamp_attribute);
  if (trace.events.empty())
    throw std::invalid_argument("token replay duration: empty trace has no endpoints");
  const double first = instant(trace.events.front(), timestamp_attribute);
  const double last = instant(trace.events.back(), timestamp_attribute);
  return last - first;
}

double token_replay_median_case_duration(
    const EventLog& log, const std::vector<std::size_t>& trace_indices,
    const std::string& timestamp_attribute) {
  validate_selector(timestamp_attribute);
  validate_indices(log, trace_indices);
  if (trace_indices.empty())
    throw std::invalid_argument("token replay duration: median requires at least one trace");
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
  validate_selector(options.timestamp_attribute);
  validate_selector(options.activity_attribute);
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
  validate_selector(options.timestamp_attribute);
  validate_selector(options.activity_attribute);
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
  validate_selector(options.timestamp_attribute);
  std::map<std::size_t, std::vector<std::size_t>> transition_problems;
  std::map<std::optional<std::string>, std::vector<std::size_t>> activity_problems;
  auto normalized_options = options;
  if (normalized_options.activity_attribute.empty())
    normalized_options.activity_attribute = "concept:name";
  const auto& activity_selector = normalized_options.activity_attribute;
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
      log, transition_cohorts, normalized_options);
  result.unknown_activities = token_replay_unknown_activity_duration_diagnostics(
      log, activity_cohorts, normalized_options);
  return result;
}

}  // namespace minera
