#include "minera/ilp_miner.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/ilp_reference.hpp"

namespace minera {
namespace {

// Activity labels per trace under the configured activity key. Events without
// a label are dropped; empty traces stay (they wrap to start/end only).
std::vector<std::vector<std::string>> label_traces(const EventLog& log,
                                                   const ILPMinerOptions& options) {
  std::vector<std::vector<std::string>> traces;
  traces.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    std::vector<std::string> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      std::string label = event.activity;
      if (!options.activity_key.empty() && options.activity_key != "activity" &&
          options.activity_key != "concept:name") {
        if (options.activity_key == "resource" || options.activity_key == "org:resource") {
          if (!event.resource.empty()) label = event.resource;
        } else if (const auto* value = event.attribute(options.activity_key);
                   value != nullptr && !value->empty()) {
          label = *value;
        }
      }
      if (!label.empty()) sequence.push_back(std::move(label));
    }
    traces.push_back(std::move(sequence));
  }
  return traces;
}

// Native noise extension: keep only variants whose frequency reaches
// noise_threshold times the most frequent variant (0 keeps every trace, which
// is the reference behaviour).
void filter_noise(std::vector<std::vector<std::string>>& traces, double threshold) {
  if (threshold <= 0.0) return;
  std::map<std::vector<std::string>, std::size_t> frequency;
  std::size_t strongest = 0;
  for (const auto& trace : traces) strongest = std::max(strongest, ++frequency[trace]);
  const auto keep = [&](const std::vector<std::string>& trace) {
    return static_cast<double>(frequency.at(trace)) + 1e-12 >=
           threshold * static_cast<double>(strongest);
  };
  std::erase_if(traces, [&](const auto& trace) { return !keep(trace); });
}

}  // namespace

// Runs the reference classic ILP (`reference_ilp_net`) and lays its places and
// transitions out as a native PetriNet: the reference's "source" and "sink"
// become the workflow boundary, the artificial start/end transitions stay
// silent, and every other transition is labelled with its activity.
ILPMinerResult mine_ilp(const EventLog& event_log, const ILPMinerOptions& options) {
  if (!std::isfinite(options.noise_threshold) || options.noise_threshold < 0.0 ||
      options.noise_threshold > 1.0)
    throw std::invalid_argument("ILP miner noise_threshold must be in [0,1]");
  if (options.max_nodes == 0)
    throw std::invalid_argument("ILP miner max_nodes must be positive");
  if (event_log.traces.empty())
    throw std::invalid_argument("ILP miner requires a non-empty event log");
  if (!std::isfinite(options.alpha) || options.alpha < 0.0 || options.alpha > 1.0)
    throw std::invalid_argument("ILP miner alpha must be in [0,1]");
  if (options.artificial_start_activity.empty() || options.artificial_end_activity.empty())
    throw std::invalid_argument("ILP miner artificial boundary labels must be non-empty");
  if (options.artificial_start_activity == options.artificial_end_activity)
    throw std::invalid_argument("ILP miner artificial boundary labels must differ");

  auto traces = label_traces(event_log, options);
  std::set<std::string> names;
  for (const auto& trace : traces) names.insert(trace.begin(), trace.end());
  if (names.contains(options.artificial_start_activity) ||
      names.contains(options.artificial_end_activity))
    throw std::invalid_argument("ILP miner artificial boundary label collides with an activity");
  filter_noise(traces, options.noise_threshold);

  ReferenceIlpOptions reference;
  reference.alpha = options.alpha;
  reference.artificial_start_activity = options.artificial_start_activity;
  reference.artificial_end_activity = options.artificial_end_activity;
  if (!options.causal_relation.empty()) reference.causal_relation = options.causal_relation;

  ILPMinerResult result;
  const auto pairs = reference_ilp_causal_pairs(traces, reference);
  if (pairs.size() > options.max_nodes)
    throw std::invalid_argument("ILP miner causal pair count " + std::to_string(pairs.size()) +
                                " exceeds max_nodes");
  const auto discovered = reference_ilp_net(traces, reference);

  std::set<std::string> visible;
  for (const auto& transition : discovered.transitions)
    if (transition.label) visible.insert(*transition.label);
  std::vector<std::string> activities(visible.begin(), visible.end());
  std::map<std::string, std::size_t> activity_ids;
  for (std::size_t i = 0; i < activities.size(); ++i) activity_ids.emplace(activities[i], i);
  result.net.set_activities(activities);

  std::map<std::string, std::vector<std::size_t>> consumes, produces;
  std::optional<std::size_t> source, sink;
  for (const auto& place : discovered.places) {
    const auto id = result.net.add_place();
    if (place.name == "source") source = id;
    if (place.name == "sink") sink = id;
    for (const auto& transition : place.inputs) produces[transition].push_back(id);
    for (const auto& transition : place.outputs) consumes[transition].push_back(id);
  }
  if (!source) source = result.net.add_place();
  if (!sink) sink = result.net.add_place();
  result.net.set_source(*source);
  result.net.set_sink(*sink);
  for (const auto& transition : discovered.transitions) {
    std::optional<std::size_t> activity;
    if (transition.label) activity = activity_ids.at(*transition.label);
    result.net.add_transition(activity, consumes[transition.name], produces[transition.name]);
  }

  result.markings.initial.assign(result.net.place_count(), 0);
  result.markings.final.assign(result.net.place_count(), 0);
  result.markings.initial[*source] = 1;
  result.markings.final[*sink] = 1;
  result.diagnostics.computed = true;
  result.diagnostics.visited_nodes = pairs.size();
  result.diagnostics.lp_solved = pairs.size();
  result.diagnostics.message = "reference classic ILP: one LP relaxation per causal pair";
  return result;
}

}  // namespace minera
