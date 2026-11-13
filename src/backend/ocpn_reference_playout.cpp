#include "minera/ocpn_reference_playout.hpp"

#include "portable_time.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>

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
  const std::function<bool(const OcpnReferenceTrace&)>* visit = nullptr;
  bool stopped = false;
  std::size_t visited = 0;
  std::vector<std::vector<std::uint32_t>> markings;
  std::vector<std::size_t> fired;
  OcpnReferenceTrace current;

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

  // Dead ends: states (markings and firing counts) from which no complete
  // sequence was found. The search from a state depends on nothing else, so
  // skipping a known dead state keeps every trace and its order.
  std::unordered_set<std::string> dead;
  std::size_t dead_bytes = 0;
  static constexpr std::size_t kDeadBudget = std::size_t{256} << 20;

  static void put(std::string& key, std::uint64_t value) {
    while (value >= 0x80) {
      key.push_back(static_cast<char>((value & 0x7f) | 0x80));
      value >>= 7;
    }
    key.push_back(static_cast<char>(value));
  }

  std::string state_key() const {
    std::string key;
    for (const auto& marking : markings)
      for (const auto tokens : marking) put(key, tokens);
    for (const auto count : fired) put(key, count);
    return key;
  }

  // Returns whether this state reaches a complete sequence.
  bool run() {
    if (stopped) return true;
    if (final_reached()) {
      ++visited;
      if (!(*visit)(current)) stopped = true;
      return true;
    }
    auto key = state_key();
    if (dead.contains(key)) return false;
    bool found = false;
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
        if (run()) found = true;
        current.pop_back();
        --fired[index];
        fire(transition, chosen, -1);
        if (stopped) return true;
      }
    }
    if (!found && dead_bytes < kDeadBudget) {
      dead_bytes += key.size() + 64;
      dead.insert(std::move(key));
    }
    return found;
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

std::size_t reference_ocpn_extensive_visit(const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options,
                                           const std::function<bool(const OcpnReferenceTrace&)>& visit) {
  auto search = prepare(graph, options);
  search.visit = &visit;
  search.run();
  return search.visited;
}

std::vector<OcpnReferenceTrace> reference_ocpn_extensive_traces(const OcpnGraph& graph,
                                                                const OcpnReferencePlayoutOptions& options) {
  std::vector<OcpnReferenceTrace> traces;
  reference_ocpn_extensive_visit(graph, options, [&](const OcpnReferenceTrace& trace) {
    traces.push_back(trace);
    return true;
  });
  return traces;
}

bool reference_ocpn_trace_exists(const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options) {
  return reference_ocpn_extensive_visit(graph, options, [](const OcpnReferenceTrace&) { return false; }) != 0;
}

OcpnReferenceOcelWriter::OcpnReferenceOcelWriter(const std::map<std::string, std::string>& object_types,
                                                 bool objects_unique_per_trace)
    : object_types_(object_types), objects_unique_per_trace_(objects_unique_per_trace) {}

void OcpnReferenceOcelWriter::add(const OcpnReferenceTrace& trace) {
  for (const auto& step : trace) {
    if (!step.activity) continue;
    const auto id = "event_" + std::to_string(log_.events.size());
    log_.events.push_back({id, *step.activity, timestamp(clock_++), {}});
    for (const auto& object : step.objects) {
      const auto name = objects_unique_per_trace_ ? object + "_" + std::to_string(trace_) : object;
      log_.relations.push_back({id, name, ""});
      if (seen_.insert(name).second) log_.objects.push_back({name, object_types_.at(object), {}});
    }
  }
  ++trace_;
}

Ocel reference_ocpn_traces_to_ocel(const std::vector<OcpnReferenceTrace>& traces,
                                   const std::map<std::string, std::string>& object_types,
                                   bool objects_unique_per_trace) {
  OcpnReferenceOcelWriter writer(object_types, objects_unique_per_trace);
  for (const auto& trace : traces) writer.add(trace);
  return writer.take();
}

Ocel reference_ocpn_extensive_playout(const OcpnGraph& graph, const OcpnReferencePlayoutOptions& options) {
  std::map<std::string, std::string> object_types;
  for (const auto& [type, net] : graph.petri_nets) object_types[type + "_0"] = type;
  OcpnReferenceOcelWriter writer(object_types, options.objects_unique_per_trace);
  reference_ocpn_extensive_visit(graph, options, [&](const OcpnReferenceTrace& trace) {
    writer.add(trace);
    return true;
  });
  return writer.take();
}

}  // namespace minera
