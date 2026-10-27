#include "minera/ocpn_reference_playout.hpp"

#include "portable_time.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>

namespace minera {
namespace {

struct TypeNet {
  std::string type;
  std::string object;
  const PetriNet* net = nullptr;
};

// One transition of the merged net: a visible activity joins one transition
// of every type net that has it; a silent transition belongs to one type.
struct MergedTransition {
  std::optional<std::string> activity;
  std::vector<std::pair<std::size_t, std::size_t>> parts;  // (type net, transition)
  std::vector<std::string> objects;
  // Variable (double) arcs of a type may bind no object of that type.
  std::vector<bool> variable;
};

struct Search {
  std::vector<TypeNet> types;
  std::vector<MergedTransition> transitions;
  std::size_t bound = 1;
  bool stop_at_first = false;
  std::vector<std::vector<std::uint32_t>> markings;
  std::vector<std::size_t> fired;
  OcpnReferenceTrace current;
  std::vector<OcpnReferenceTrace> traces;

  bool final_reached() const {
    for (std::size_t t = 0; t < types.size(); ++t) {
      const auto& net = *types[t].net;
      for (std::size_t place = 0; place < net.place_count(); ++place)
        if (markings[t][place] != (place == net.sink() ? 1u : 0u)) return false;
    }
    return true;
  }

  bool enabled_part(const MergedTransition& transition, std::size_t part) const {
    const auto& [type, index] = transition.parts[part];
    auto needed = markings[type];
    for (const auto place : types[type].net->transitions()[index].inputs) {
      if (needed[place] == 0) return false;
      --needed[place];
    }
    return true;
  }

  // The participating parts of every enabled binding: each non-variable part
  // must fire, each variable part may; at least one part fires.
  std::vector<std::vector<bool>> bindings(const MergedTransition& transition) const {
    std::vector<std::vector<bool>> out{{}};
    for (std::size_t part = 0; part < transition.parts.size(); ++part) {
      const bool can = enabled_part(transition, part);
      const bool must = !transition.variable[part];
      if (must && !can) return {};
      std::vector<std::vector<bool>> next;
      for (const auto& prefix : out) {
        if (!must) {
          next.push_back(prefix);
          next.back().push_back(false);
        }
        if (can) {
          next.push_back(prefix);
          next.back().push_back(true);
        }
      }
      out = std::move(next);
    }
    out.erase(std::remove_if(out.begin(), out.end(),
                             [](const std::vector<bool>& chosen) {
                               return std::find(chosen.begin(), chosen.end(), true) == chosen.end();
                             }),
              out.end());
    return out;
  }

  void fire(const MergedTransition& transition, const std::vector<bool>& chosen, int direction) {
    for (std::size_t part = 0; part < transition.parts.size(); ++part) {
      if (!chosen[part]) continue;
      const auto& [type, index] = transition.parts[part];
      const auto& definition = types[type].net->transitions()[index];
      for (const auto place : definition.inputs) markings[type][place] -= direction;
      for (const auto place : definition.outputs) markings[type][place] += direction;
    }
  }

  void run() {
    if (stop_at_first && !traces.empty()) return;
    if (final_reached()) {
      traces.push_back(current);
      return;
    }
    for (std::size_t index = 0; index < transitions.size(); ++index) {
      const auto& transition = transitions[index];
      if (fired[index] >= bound) continue;
      for (const auto& chosen : bindings(transition)) {
        fire(transition, chosen, 1);
        ++fired[index];
        OcpnReferenceStep step;
        step.activity = transition.activity;
        if (!transition.activity) {
          step.object_type = types[transition.parts.front().first].type;
          step.transition_index = transition.parts.front().second;
        }
        for (std::size_t part = 0; part < chosen.size(); ++part)
          if (chosen[part]) step.objects.push_back(types[transition.parts[part].first].object);
        std::sort(step.objects.begin(), step.objects.end());
        current.push_back(std::move(step));
        run();
        current.pop_back();
        --fired[index];
        fire(transition, chosen, -1);
        if (stop_at_first && !traces.empty()) return;
      }
      if (stop_at_first && !traces.empty()) return;
    }
  }
};

Search prepare(const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options) {
  Search search;
  search.bound = options.max_bindings_per_activity;
  for (const auto& [type, net] : graph.petri_nets) {
    if (net.place_count() == 0 || net.source() >= net.place_count() || net.sink() >= net.place_count())
      throw std::invalid_argument("OCPN playout net for type " + type + " has no workflow places");
    search.types.push_back({type, type + "_0", &net});
  }
  std::vector<std::string> activities;
  for (std::size_t t = 0; t < search.types.size(); ++t) {
    const auto& net = *search.types[t].net;
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      if (!transition.activity) {
        search.transitions.push_back({std::nullopt, {{t, index}}, {search.types[t].object}, {false}});
        continue;
      }
      const auto& activity = net.activities().at(*transition.activity);
      auto merged = std::find_if(search.transitions.begin(), search.transitions.end(),
                                 [&](const MergedTransition& entry) { return entry.activity == activity; });
      if (merged == search.transitions.end()) {
        search.transitions.push_back({activity, {}, {}, {}});
        merged = std::prev(search.transitions.end());
      }
      merged->parts.push_back({t, index});
      merged->objects.push_back(search.types[t].object);
      const auto arcs = graph.double_arcs_on_activity.find(search.types[t].type);
      bool variable = false;
      if (arcs != graph.double_arcs_on_activity.end()) {
        const auto found = arcs->second.find(activity);
        variable = found != arcs->second.end() && found->second;
      }
      merged->variable.push_back(variable);
    }
  }
  for (const auto& type : search.types) {
    std::vector<std::uint32_t> marking(type.net->place_count(), 0);
    marking[type.net->source()] = 1;
    search.markings.push_back(std::move(marking));
  }
  search.fired.assign(search.transitions.size(), 0);
  return search;
}

std::string timestamp(long long seconds) {
  const std::time_t value = static_cast<std::time_t>(seconds);
  std::tm parts{};
  utc_fields(value, parts);
  char buffer[32];
  std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &parts);
  return buffer;
}

}  // namespace

std::vector<OcpnReferenceTrace> reference_ocpn_extensive_traces(const OcpnGraph& graph,
                                                                const OcpnReferencePlayoutOptions& options) {
  auto search = prepare(graph, options);
  search.run();
  return std::move(search.traces);
}

bool reference_ocpn_trace_exists(const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options) {
  auto search = prepare(graph, options);
  search.stop_at_first = true;
  search.run();
  return !search.traces.empty();
}

Ocel reference_ocpn_traces_to_ocel(const std::vector<OcpnReferenceTrace>& traces,
                                   const std::map<std::string, std::string>& object_types,
                                   bool objects_unique_per_trace) {
  Ocel log;
  std::set<std::string> seen;
  long long clock = 10000001;
  for (std::size_t trace = 0; trace < traces.size(); ++trace) {
    for (const auto& step : traces[trace]) {
      if (!step.activity) continue;
      const auto id = "event_" + std::to_string(log.events.size());
      log.events.push_back({id, *step.activity, timestamp(clock++), {}});
      for (const auto& object : step.objects) {
        const auto name = objects_unique_per_trace ? object + "_" + std::to_string(trace) : object;
        log.relations.push_back({id, name, ""});
        if (seen.insert(name).second) log.objects.push_back({name, object_types.at(object), {}});
      }
    }
  }
  return log;
}

Ocel reference_ocpn_extensive_playout(const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options) {
  std::map<std::string, std::string> object_types;
  for (const auto& [type, net] : graph.petri_nets) object_types[type + "_0"] = type;
  return reference_ocpn_traces_to_ocel(reference_ocpn_extensive_traces(graph, options), object_types,
                                       options.objects_unique_per_trace);
}

}  // namespace minera
