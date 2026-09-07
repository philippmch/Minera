#include "minera/conformance.hpp"
#include "root_cause_trace_key.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace minera {
namespace {
using Contexts = std::map<std::size_t, RootCauseObservation>;

class TraceCohortIndex {
 public:
  explicit TraceCohortIndex(const EventLog& log) : log_(log) {}
  const std::vector<std::size_t>& matches(std::size_t source) {
    if (source >= log_.traces.size())
      throw std::out_of_range("root cause: source trace index out of range");
    if (const auto found = matches_.find(source); found != matches_.end()) return *found->second;
    if (!indexed_) {
      hashes_.reserve(log_.traces.size());
      for (std::size_t i = 0; i < log_.traces.size(); ++i) {
        const auto hash = std::hash<std::string>{}(root_cause_trace_fingerprint(log_.traces[i]));
        hashes_.push_back(hash);
        buckets_[hash].push_back(i);
      }
      indexed_ = true;
    }
    auto group = std::make_shared<std::vector<std::size_t>>();
    for (const auto candidate : buckets_.at(hashes_[source]))
      if (candidate == source || root_cause_traces_equal(log_.traces[source], log_.traces[candidate]))
        group->push_back(candidate);
    for (const auto member : *group) matches_[member] = group;
    return *group;
  }
 private:
  const EventLog& log_;
  bool indexed_ = false;
  std::vector<std::size_t> hashes_;
  std::unordered_map<std::size_t, std::vector<std::size_t>> buckets_;
  std::unordered_map<std::size_t, std::shared_ptr<std::vector<std::size_t>>> matches_;
};

Contexts indexed_contexts(TraceCohortIndex& index,
                          const std::vector<RootCauseObservation>& observations) {
  Contexts result;
  std::set<std::size_t> covered_groups;
  for (const auto& observation : observations) {
    const auto& group = index.matches(observation.trace_index);
    if (!covered_groups.insert(group.front()).second) continue;
    for (const auto source : group)
      result.try_emplace(source, RootCauseObservation{source, observation.context});
  }
  return result;
}

struct ActivityValue { bool present = false; EventAttribute value; };
ActivityValue selected_activity(EventAttribute value) {
  if (value.first_class_state == FirstClassAttributeState::kNull) value.is_null = true;
  return {true, std::move(value)};
}
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
    return selected_activity(*selected);
  }
  if (key == "concept:name" || key == "activity" ||
      (!event.activity_source_key.empty() && key == event.activity_source_key) ||
      (event.activity_attribute &&
       (key == event.activity_attribute->key ||
        (!event.activity_attribute->first_class_source_key.empty() &&
         key == event.activity_attribute->first_class_source_key)))) {
    if (event.activity_state == FirstClassAttributeState::kNull) {
      EventAttribute value;
      value.is_null = true;
      return selected_activity(std::move(value));
    }
    if (event.activity_attribute) {
      if (event.activity_state == FirstClassAttributeState::kMissing ||
          (event.activity_attribute->first_class_role != FirstClassAttributeRole::kNone &&
           event.activity_attribute->first_class_state == FirstClassAttributeState::kMissing)) return {};
      return selected_activity(*event.activity_attribute);
    }
    if (event.has_activity()) return selected_activity(EventAttribute{key, event.activity});
  }
  return {};
}

EventAttribute cohort_key(const TokenReplayUnknownRootCauseCohort& cohort) {
  auto value = cohort.activity_attribute ? selected_activity(*cohort.activity_attribute).value
      : selected_activity(EventAttribute{"", cohort.activity, AttributeKind::kString, cohort.activity_is_null}).value;
  (void)root_cause_scalar_keys_equal(value, value);
  return value;
}

class ActivityKeyIndex {
 public:
  std::optional<std::size_t> find(const EventAttribute& value) const {
    const auto fingerprint = root_cause_scalar_key_fingerprint(value);
    const auto bucket = buckets_.find(fingerprint);
    if (bucket == buckets_.end()) return std::nullopt;
    for (const auto index : bucket->second)
      if (root_cause_scalar_keys_equal(values_[index], value)) return index;
    return std::nullopt;
  }
  std::size_t insert(EventAttribute value) {
    if (const auto found = find(value)) return *found;
    const auto index = values_.size();
    buckets_[root_cause_scalar_key_fingerprint(value)].push_back(index);
    values_.push_back(std::move(value));
    return index;
  }
 private:
  std::vector<EventAttribute> values_;
  std::unordered_map<std::string, std::vector<std::size_t>> buckets_;
};

bool contains_key(const Trace& trace, const ActivityKeyIndex& keys,
                  const std::string& selector) {
  for (const auto& event : trace.events) {
    const auto activity = activity_value(event, selector);
    if (activity.present && root_cause_key_is_hashable(activity.value))
      if (keys.find(activity.value)) return true;
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
  TraceCohortIndex index(log);
  for (const auto& cohort : cohorts) {
    const auto fitting = indexed_contexts(index, cohort.fit_contexts);
    const auto problematic = indexed_contexts(index, cohort.problem_contexts);
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
  ActivityKeyIndex keys;
  for (const auto& cohort : cohorts)
    keys.insert(cohort_key(cohort));
  std::vector<TokenReplayUnknownRootCauseDiagnostics> result;
  std::vector<std::optional<bool>> baseline_eligible(log.traces.size());
  TraceCohortIndex index(log);
  for (const auto& cohort : cohorts) {
    const auto problematic = indexed_contexts(index, cohort.problem_contexts);
    if (problematic.empty()) continue;
    std::vector<RootCauseObservation> fit, problem;
    for (std::size_t i = 0; i < log.traces.size(); ++i) {
      if (problematic.contains(i)) continue;
      if (!baseline_eligible[i].has_value())
        baseline_eligible[i] = !log.traces[i].events.empty() && !contains_key(log.traces[i], keys, selector);
      if (!*baseline_eligible[i]) continue;
      fit.push_back({i, log.traces[i].events.back()});
    }
    for (const auto& [index, observation] : problematic) problem.push_back(observation);
    if (auto model = fit_root_cause_model(fit, problem, options.root_cause, "containing")) {
      TokenReplayUnknownRootCauseDiagnostics row;
      static_cast<RootCauseModel&>(row) = std::move(*model);
      const auto key = cohort_key(cohort);
      row.activity = cohort.activity_attribute ? key.is_null ? "" : root_cause_attribute_category(key) : cohort.activity;
      row.activity_is_null = key.is_null;
      row.activity_attribute = cohort.activity_attribute ? std::optional<EventAttribute>(key) : std::nullopt;
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
  std::vector<TokenReplayUnknownRootCauseCohort> unknown;
  ActivityKeyIndex unknown_index;
  const auto selector = options.activity_attribute.empty() ? std::string("concept:name") : options.activity_attribute;
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
      auto selected = activity_value(log.traces[trace.trace_index].events[trace.unknown_event_indices[i]], selector);
      if (!selected.present)
        selected = selected_activity(EventAttribute{selector, trace.unknown_activities[i], AttributeKind::kString, is_null});
      const auto group = unknown_index.insert(selected.value);
      if (group == unknown.size()) {
        TokenReplayUnknownRootCauseCohort cohort;
        cohort.activity = selected.value.is_null ? "" : root_cause_attribute_category(selected.value);
        cohort.activity_is_null = selected.value.is_null;
        cohort.activity_attribute = std::move(selected.value);
        unknown.push_back(std::move(cohort));
      }
      unknown[group].problem_contexts.push_back({trace.trace_index, context});
    }
  }
  std::vector<TokenReplayTransitionRootCauseCohort> transition_cohorts;
  for (auto& [key, cohort] : transitions) transition_cohorts.push_back(std::move(cohort));
  TokenReplayRootCauseDiagnostics result;
  result.options = options.root_cause;
  result.transitions = token_replay_transition_root_cause_diagnostics(log, transition_cohorts, options.root_cause);
  result.unknown_activities = token_replay_unknown_root_cause_diagnostics(log, unknown, options);
  return result;
}
}  // namespace minera
