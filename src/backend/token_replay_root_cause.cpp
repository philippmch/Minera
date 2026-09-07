#include "minera/conformance.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace minera {
namespace {
using Contexts = std::map<std::size_t, RootCauseObservation>;

Contexts indexed_contexts(const EventLog& log,
                          const std::vector<RootCauseObservation>& observations) {
  Contexts result;
  for (const auto& observation : observations) {
    if (observation.trace_index >= log.traces.size())
      throw std::out_of_range("root cause: source trace index out of range");
    result.try_emplace(observation.trace_index, observation);
  }
  return result;
}

struct ActivityValue { bool present = false; std::optional<std::string> value; };
ActivityValue activity_value(const Event& event, const std::string& key) {
  const EventAttribute* selected = nullptr;
  int priority = 0;
  std::size_t selected_count = 0;
  for (const auto& attribute : event.attributes) {
    const int candidate = attribute.key == key ? 2
        : !attribute.first_class_source_key.empty() && attribute.first_class_source_key == key ? 1 : 0;
    if (!candidate || candidate < priority) continue;
    if (candidate == priority) ++selected_count;
    else {
      selected = &attribute;
      priority = candidate;
      selected_count = 1;
    }
  }
  if (selected_count > 1)
    throw std::invalid_argument("root cause: duplicate selected activity attribute");
  if (selected) {
    if (selected->first_class_role != FirstClassAttributeRole::kNone &&
        selected->first_class_state == FirstClassAttributeState::kMissing) return {};
    return {true, selected->is_null || selected->first_class_state == FirstClassAttributeState::kNull
                      ? std::nullopt : std::optional<std::string>(selected->value)};
  }
  if (key == "concept:name" || key == "activity" ||
      (!event.activity_source_key.empty() && key == event.activity_source_key) ||
      (event.activity_attribute &&
       (key == event.activity_attribute->key ||
        (!event.activity_attribute->first_class_source_key.empty() &&
         key == event.activity_attribute->first_class_source_key)))) {
    if (event.activity_state == FirstClassAttributeState::kNull) return {true, std::nullopt};
    if (event.activity_attribute) {
      if (event.activity_state == FirstClassAttributeState::kMissing ||
          (event.activity_attribute->first_class_role != FirstClassAttributeRole::kNone &&
           event.activity_attribute->first_class_state == FirstClassAttributeState::kMissing)) return {};
      return {true, event.activity_attribute->is_null ||
                    event.activity_attribute->first_class_state == FirstClassAttributeState::kNull
                    ? std::nullopt : std::optional<std::string>(event.activity_attribute->value)};
    }
    if (event.has_activity()) return {true, event.activity};
  }
  return {};
}

bool contains_key(const Trace& trace, const std::set<std::optional<std::string>>& keys,
                  const std::string& selector) {
  for (const auto& event : trace.events) {
    const auto activity = activity_value(event, selector);
    if (activity.present && keys.contains(activity.value)) return true;
  }
  return false;
}

// Replay's context contains the latest values from examined known activities.
// Unknown events do not update it. Later known events update every observation
// of that case, including observations attached to earlier firings.
Event replay_context(const Trace& trace, const TokenReplayTraceDiagnostics& diagnostics) {
  if (diagnostics.replayed_events > trace.events.size())
    throw std::out_of_range("root cause: examined event count out of range");
  std::set<std::size_t> unknown(diagnostics.unknown_event_indices.begin(),
                                diagnostics.unknown_event_indices.end());
  Event result;
  std::map<std::string, EventAttribute> attributes;
  const auto present_projection = [](bool present, FirstClassAttributeState state,
                                      const std::optional<EventAttribute>& stored) {
    if (!present || (stored && state == FirstClassAttributeState::kMissing)) return false;
    return state == FirstClassAttributeState::kNull || !stored ||
           stored->first_class_role == FirstClassAttributeRole::kNone ||
           stored->first_class_state != FirstClassAttributeState::kMissing;
  };
  for (std::size_t i = 0; i < diagnostics.replayed_events; ++i) {
    if (unknown.contains(i)) continue;
    const auto& event = trace.events[i];
    for (const auto& attribute : event.attributes) {
      if (attribute.first_class_role != FirstClassAttributeRole::kNone &&
          attribute.first_class_state == FirstClassAttributeState::kMissing) continue;
      attributes[attribute.key] = attribute;
    }
    if (present_projection(event.has_activity(), event.activity_state, event.activity_attribute)) {
      result.activity = event.activity; result.activity_state = event.activity_state;
      result.activity_source_key = event.activity_source_key; result.activity_attribute = event.activity_attribute;
    }
    if (present_projection(event.has_timestamp(), event.timestamp_state, event.timestamp_attribute))
      copy_event_timestamp_context(event, result);
    if (present_projection(event.has_resource(), event.resource_state, event.resource_attribute)) {
      result.resource = event.resource; result.resource_state = event.resource_state;
      result.resource_source_key = event.resource_source_key; result.resource_attribute = event.resource_attribute;
    }
    if (present_projection(event.has_lifecycle(), event.lifecycle_state, event.lifecycle_attribute)) {
      result.lifecycle = event.lifecycle; result.lifecycle_state = event.lifecycle_state;
      result.lifecycle_source_key = event.lifecycle_source_key; result.lifecycle_attribute = event.lifecycle_attribute;
    }
  }
  for (auto& [key, attribute] : attributes) result.attributes.push_back(std::move(attribute));
  return result;
}
}  // namespace

std::vector<TokenReplayTransitionRootCauseDiagnostics> token_replay_transition_root_cause_diagnostics(
    const EventLog& log, const std::vector<TokenReplayTransitionRootCauseCohort>& cohorts,
    const RootCauseOptions& options) {
  std::vector<TokenReplayTransitionRootCauseDiagnostics> result;
  for (const auto& cohort : cohorts) {
    const auto fitting = indexed_contexts(log, cohort.fit_contexts);
    const auto problematic = indexed_contexts(log, cohort.problem_contexts);
    std::vector<RootCauseObservation> fit, problem;
    for (const auto& [index, observation] : fitting)
      if (!problematic.contains(index)) fit.push_back(observation);
    for (const auto& [index, observation] : problematic) problem.push_back(observation);
    if (auto model = fit_root_cause_model(fit, problem, options)) {
      TokenReplayTransitionRootCauseDiagnostics row;
      static_cast<RootCauseModel&>(row) = std::move(*model);
      row.transition_index = cohort.transition_index;
      result.push_back(std::move(row));
    }
  }
  return result;
}

std::vector<TokenReplayUnknownRootCauseDiagnostics> token_replay_unknown_root_cause_diagnostics(
    const EventLog& log, const std::vector<TokenReplayUnknownRootCauseCohort>& cohorts,
    const TokenReplayOptions& options) {
  const auto selector = options.activity_attribute.empty() ? std::string("concept:name") : options.activity_attribute;
  std::set<std::optional<std::string>> keys;
  for (const auto& cohort : cohorts)
    keys.insert(cohort.activity_is_null ? std::nullopt : std::optional<std::string>(cohort.activity));
  std::vector<TokenReplayUnknownRootCauseDiagnostics> result;
  for (const auto& cohort : cohorts) {
    const auto problematic = indexed_contexts(log, cohort.problem_contexts);
    if (problematic.empty()) continue;
    std::vector<RootCauseObservation> fit, problem;
    for (std::size_t i = 0; i < log.traces.size(); ++i) {
      if (problematic.contains(i) || contains_key(log.traces[i], keys, selector)) continue;
      if (log.traces[i].events.empty()) continue;
      fit.push_back({i, log.traces[i].events.back()});
    }
    for (const auto& [index, observation] : problematic) problem.push_back(observation);
    if (auto model = fit_root_cause_model(fit, problem, options.root_cause, "containing")) {
      TokenReplayUnknownRootCauseDiagnostics row;
      static_cast<RootCauseModel&>(row) = std::move(*model);
      row.activity = cohort.activity; row.activity_is_null = cohort.activity_is_null;
      result.push_back(std::move(row));
    }
  }
  return result;
}

TokenReplayRootCauseDiagnostics compute_token_replay_root_cause_diagnostics(
    const EventLog& log, const PetriNet& net,
    const std::vector<TokenReplayTraceDiagnostics>& traces,
    const TokenReplayOptions& options) {
  std::map<std::size_t, TokenReplayTransitionRootCauseCohort> transitions;
  std::map<std::optional<std::string>, TokenReplayUnknownRootCauseCohort> unknown;
  for (const auto& trace : traces) {
    if (trace.trace_index >= log.traces.size())
      throw std::out_of_range("root cause: diagnostic trace index out of range");
    const auto context = replay_context(log.traces[trace.trace_index], trace);
    for (const auto& event : trace.event_diagnostics) {
      if (event.event_index >= trace.replayed_events)
        throw std::out_of_range("root cause: diagnostic event index out of range");
      for (const auto transition : event.activated_transitions) {
        if (transition >= net.transitions().size())
          throw std::out_of_range("root cause: diagnostic transition index out of range");
        if (net.transitions()[transition].silent()) continue;
        auto& cohort = transitions[transition]; cohort.transition_index = transition;
        const bool problem = std::find(event.transitions_with_problems.begin(),
            event.transitions_with_problems.end(), transition) != event.transitions_with_problems.end();
        (problem ? cohort.problem_contexts : cohort.fit_contexts).push_back({trace.trace_index, context});
      }
    }
    for (std::size_t i = 0; i < trace.unknown_event_indices.size(); ++i) {
      if (trace.unknown_event_indices[i] >= trace.replayed_events || i >= trace.unknown_activities.size())
        throw std::out_of_range("root cause: unknown event identity out of range");
      const bool is_null = i < trace.unknown_activity_is_null.size() && trace.unknown_activity_is_null[i];
      const auto key = is_null ? std::nullopt : std::optional<std::string>(trace.unknown_activities[i]);
      auto& cohort = unknown[key]; cohort.activity = key.value_or(""); cohort.activity_is_null = is_null;
      cohort.problem_contexts.push_back({trace.trace_index, context});
    }
  }
  std::vector<TokenReplayTransitionRootCauseCohort> transition_cohorts;
  for (auto& [key, cohort] : transitions) transition_cohorts.push_back(std::move(cohort));
  std::vector<TokenReplayUnknownRootCauseCohort> unknown_cohorts;
  for (auto& [key, cohort] : unknown) unknown_cohorts.push_back(std::move(cohort));
  TokenReplayRootCauseDiagnostics result;
  result.options = options.root_cause;
  result.transitions = token_replay_transition_root_cause_diagnostics(log, transition_cohorts, options.root_cause);
  result.unknown_activities = token_replay_unknown_root_cause_diagnostics(log, unknown_cohorts, options);
  return result;
}
}  // namespace minera
