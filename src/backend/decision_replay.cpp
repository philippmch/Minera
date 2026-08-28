#include "decision_replay.hpp"

#include <string>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace minera::detail {
namespace {

std::string variant_key(const std::vector<std::string>& activities) {
  std::string key;
  for (const auto& activity : activities) {
    key.push_back('\x1f');
    key += activity;
  }
  return key;
}

bool standard_activity_attribute(std::string_view key) {
  return key.empty() || key == "concept:name" || key == "activity";
}

const std::string& activity_value(const Event& event,
                                  std::string_view activity_attribute) {
  if (standard_activity_attribute(activity_attribute)) return event.activity;
  if (const auto* value = event.attribute(activity_attribute)) return *value;
  throw std::invalid_argument(
      "decision mining: event is missing activity attribute '" +
      std::string(activity_attribute) + "'");
}

std::vector<AlignmentMove> exact_path(
    const PetriNet& net,
    const std::vector<std::vector<std::size_t>>& firing) {
  std::vector<AlignmentMove> moves;
  for (const auto& step : firing) {
    for (const std::size_t transition : step) {
      if (transition >= net.transitions().size()) continue;
      const auto& model_transition = net.transitions()[transition];
      if (model_transition.silent()) {
        moves.push_back(
            {AlignmentMoveType::kModel, {}, transition});
      } else {
        moves.push_back({AlignmentMoveType::kSynchronous,
                         net.activities()[*model_transition.activity],
                         transition});
      }
    }
  }
  return moves;
}

}  // namespace

DecisionReplayPaths collect_decision_replay_paths(const EventLog& log,
                                                  const PetriNet& net,
                                                  std::string_view activity_attribute) {
  DecisionReplayPaths result;
  result.variant_of.resize(log.traces.size());
  std::vector<std::uint64_t> variant_counts;
  std::unordered_map<std::string, std::size_t> variant_ids;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    std::vector<std::string> activities;
    activities.reserve(log.traces[trace_index].events.size());
    for (const auto& event : log.traces[trace_index].events)
      activities.push_back(activity_value(event, activity_attribute));
    const auto [it, inserted] =
        variant_ids.emplace(variant_key(activities), result.variants.size());
    if (inserted) {
      result.variants.push_back(activities);
      variant_counts.push_back(1);
    } else {
      ++variant_counts[it->second];
    }
    result.variant_of[trace_index] = it->second;
  }

  result.paths.resize(result.variants.size());
  const auto firing = replay_firing_sequences(net, result.variants);
  std::vector<std::size_t> deviating;
  for (std::size_t variant = 0; variant < result.variants.size(); ++variant) {
    if (firing[variant]) {
      result.paths[variant].available = true;
      result.paths[variant].exact = true;
      result.paths[variant].moves = exact_path(net, *firing[variant]);
    } else {
      deviating.push_back(variant);
    }
  }

  // Align only variants that clean replay rejected. This preserves the cheap
  // exact path while giving deviating variants the same model-choice evidence
  // used by alignment-based conformance.
  if (!deviating.empty()) {
    EventLog deviating_log;
    deviating_log.traces.reserve(deviating.size());
    for (const std::size_t variant : deviating) {
      Trace trace;
      trace.case_id = "decision-variant-" + std::to_string(variant);
      for (const auto& activity : result.variants[variant]) {
        Event event;
        event.case_id = trace.case_id;
        event.activity = activity;
        trace.events.push_back(std::move(event));
      }
      deviating_log.traces.push_back(std::move(trace));
    }
    const auto alignment = check_alignment(deviating_log, net);
    if (alignment.computed) {
      std::unordered_map<std::string, const VariantAlignment*> by_variant;
      by_variant.reserve(alignment.variants.size());
      for (const auto& item : alignment.variants)
        by_variant.emplace(variant_key(item.activities), &item);
      for (const std::size_t variant : deviating) {
        const auto it = by_variant.find(variant_key(result.variants[variant]));
        if (it == by_variant.end()) continue;
        result.paths[variant].available = true;
        result.paths[variant].exact = it->second->cost <= 1e-9;
        result.paths[variant].moves =
            it->second->complete_moves.empty() ? it->second->moves
                                                : it->second->complete_moves;
      }
    }
  }

  for (std::size_t variant = 0; variant < result.paths.size(); ++variant) {
    const auto count = variant_counts[variant];
    if (!result.paths[variant].available) {
      result.alignment_failures += count;
    } else if (result.paths[variant].exact) {
      result.replayed_traces += count;
    } else {
      result.aligned_traces += count;
    }
  }
  return result;
}

}  // namespace minera::detail
