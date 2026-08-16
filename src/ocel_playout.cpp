#include "minera/ocel_playout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/playout.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct ObjectChoice {
  std::string type;
  std::string object_id;
  std::vector<Trace> traces;
};

struct PendingEvent {
  std::string activity;
  std::string type;
  std::string object_id;
  std::size_t local_index = 0;
  std::size_t choice_index = 0;
};

void validate_net(const PetriNet& net, const std::string& type) {
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count()) {
    throw std::invalid_argument("OCPN playout net for type " + type +
                                " has invalid workflow places");
  }
  for (const auto& transition : net.transitions()) {
    if (transition.inputs.empty())
      throw std::invalid_argument("OCPN playout net contains a source transition");
    for (const auto place : transition.inputs) {
      if (place >= net.place_count())
        throw std::invalid_argument("OCPN playout transition input is invalid");
    }
    for (const auto place : transition.outputs) {
      if (place >= net.place_count())
        throw std::invalid_argument("OCPN playout transition output is invalid");
    }
    if (transition.activity && *transition.activity >= net.activities().size())
      throw std::invalid_argument("OCPN playout transition activity is invalid");
  }
}

std::size_t visible_bound(const PetriNet& net,
                         std::size_t max_bindings_per_activity) {
  std::size_t visible = 0;
  for (const auto& transition : net.transitions())
    if (transition.activity) ++visible;
  const auto factor = std::max<std::size_t>(1, visible);
  if (max_bindings_per_activity >
      std::numeric_limits<std::size_t>::max() / factor) {
    return std::numeric_limits<std::size_t>::max();
  }
  return max_bindings_per_activity * factor;
}

void write_json_string(std::ostringstream& out, const std::string& value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << static_cast<char>(c); break;
    }
  }
  out << '"';
}

}  // namespace

const char* ocpn_event_binding_name(OcpnEventBinding binding) {
  switch (binding) {
    case OcpnEventBinding::kIndependent: return "independent";
    case OcpnEventBinding::kSharedActivity: return "shared_activity";
  }
  throw std::invalid_argument("OCPN event binding is invalid");
}

Ocel ocpn_extensive_playout(const OcpnGraph& graph,
                            const OcpnPlayoutOptions& options) {
  if (options.max_bindings_per_activity == 0)
    throw std::invalid_argument(
        "OCPN max_bindings_per_activity must be positive");
  if (options.event_binding != OcpnEventBinding::kIndependent &&
      options.event_binding != OcpnEventBinding::kSharedActivity) {
    throw std::invalid_argument(
        "OCPN event_binding must be independent or shared_activity");
  }
  if (!std::isfinite(options.start) || !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval) || options.case_interval < 0.0 ||
      options.event_interval < 0.0) {
    throw std::invalid_argument("OCPN playout timestamps must be finite and non-negative");
  }
  for (const auto& [type, object_ids] : options.objects) {
    if (!graph.petri_nets.contains(type))
      throw std::invalid_argument("OCPN playout object type is not in the model: " +
                                  type);
    for (const auto& object_id : object_ids) {
      if (object_id.empty())
        throw std::invalid_argument("OCPN playout object ids must not be empty");
    }
  }

  std::vector<ObjectChoice> choices;
  std::map<std::string, std::string> supplied_object_types;
  for (const auto& [type, net] : graph.petri_nets) {
    validate_net(net, type);
    std::vector<std::string> object_ids;
    const auto supplied = options.objects.find(type);
    if (supplied == options.objects.end()) {
      object_ids.push_back(type + "_1");
    } else {
      object_ids = supplied->second;
    }
    if (object_ids.empty()) continue;

    ExtensivePlayoutOptions trace_options;
    trace_options.max_length = visible_bound(
        net, options.max_bindings_per_activity);
    trace_options.max_traces = options.max_traces;
    trace_options.max_states = options.max_states;
    trace_options.start = 0.0;
    trace_options.case_interval = 0.0;
    trace_options.event_interval = 1.0;
    const auto traces_log = extensive_playout(net, trace_options);
    if (traces_log.traces.empty()) return {};
    for (const auto& object_id : object_ids) {
      const auto old_type = supplied_object_types.find(object_id);
      if (old_type != supplied_object_types.end())
        throw std::invalid_argument(
            "OCPN playout object ids must be unique across object types");
      supplied_object_types[object_id] = type;
      choices.push_back({type, object_id, traces_log.traces});
    }
  }

  Ocel result;
  if (choices.empty() || options.max_traces == 0) return result;
  std::map<std::string, std::string> object_types;
  std::vector<std::size_t> selected(choices.size(), 0);
  std::size_t sequence_index = 0;

  const auto emit = [&]() {
    std::vector<PendingEvent> pending;
    for (std::size_t choice = 0; choice < choices.size(); ++choice) {
      const auto& selected_trace = choices[choice].traces[selected[choice]];
      for (std::size_t index = 0; index < selected_trace.events.size(); ++index) {
        pending.push_back({selected_trace.events[index].activity,
                           choices[choice].type, choices[choice].object_id,
                           index, choice});
      }
    }
    const bool shared_activity =
        options.event_binding == OcpnEventBinding::kSharedActivity;
    std::stable_sort(pending.begin(), pending.end(),
                     [shared_activity](const PendingEvent& left,
                                       const PendingEvent& right) {
                       if (shared_activity) {
                         return std::tie(left.local_index, left.activity,
                                         left.type, left.object_id,
                                         left.choice_index) <
                                std::tie(right.local_index, right.activity,
                                         right.type, right.object_id,
                                         right.choice_index);
                       }
                       return std::tie(left.local_index, left.type, left.object_id,
                                       left.choice_index) <
                              std::tie(right.local_index, right.type,
                                       right.object_id, right.choice_index);
                     });
    const std::string suffix = options.objects_unique_per_trace
                                   ? "_" + std::to_string(sequence_index + 1)
                                   : "";
    std::map<std::string, std::string> emitted_object_ids;
    for (const auto& choice : choices) {
      const auto id = choice.object_id + suffix;
      emitted_object_ids[choice.object_id + "\n" + choice.type] = id;
      if (!object_types.contains(id)) {
        result.objects.push_back({id, choice.type, {}});
        object_types[id] = choice.type;
      }
    }
    std::size_t emitted_index = 0;
    for (std::size_t index = 0; index < pending.size();) {
      const auto& event = pending[index];
      std::size_t end = index + 1;
      if (shared_activity) {
        while (end < pending.size() &&
               pending[end].local_index == event.local_index &&
               pending[end].activity == event.activity) {
          ++end;
        }
      }
      const auto event_id = "event_" + std::to_string(result.events.size() + 1);
      const auto timestamp = format_timestamp(
          options.start + static_cast<double>(sequence_index) *
                              options.case_interval +
          static_cast<double>(emitted_index) * options.event_interval);
      result.events.push_back({event_id, event.activity, timestamp, {}});
      for (std::size_t relation = index; relation < end; ++relation) {
        const auto& related = pending[relation];
        const auto id = emitted_object_ids.at(related.object_id + "\n" +
                                              related.type);
        result.relations.push_back({event_id, id, ""});
      }
      ++emitted_index;
      index = end;
    }
    ++sequence_index;
  };

  std::function<void(std::size_t)> enumerate = [&](std::size_t index) {
    if (sequence_index >= options.max_traces) return;
    if (index == choices.size()) {
      emit();
      return;
    }
    for (std::size_t trace = 0; trace < choices[index].traces.size(); ++trace) {
      selected[index] = trace;
      enumerate(index + 1);
      if (sequence_index >= options.max_traces) return;
    }
  };
  enumerate(0);
  return result;
}

std::string serialize_ocpn_playout_json(const Ocel& log) {
  return serialize_ocpn_playout_json(log, OcpnEventBinding::kIndependent);
}

std::string serialize_ocpn_playout_json(const Ocel& log,
                                        OcpnEventBinding binding) {
  return std::string("{\"family\":\"OCEL_PLAYOUT\",\"variant\":\"EXTENSIVE\",\"event_binding\":\"") +
         ocpn_event_binding_name(binding) + "\",\"log\":" +
         serialize_ocel_json(log) + "}";
}

}  // namespace minera
