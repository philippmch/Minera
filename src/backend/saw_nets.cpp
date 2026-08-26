#include "minera/saw_nets.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct FlattenedTypeLog {
  EventLog log;
  std::vector<std::vector<std::string>> variants;
  std::vector<std::uint64_t> variant_counts;
};

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

FlattenedTypeLog flatten_type(const Ocel& log, const std::string& type) {
  std::map<std::string, std::string> object_types;
  std::map<std::string, const OcelEvent*> events;
  std::map<std::string, std::size_t> event_order;
  for (const auto& object : log.objects) object_types[object.id] = object.type;
  for (std::size_t index = 0; index < log.events.size(); ++index) {
    events[log.events[index].id] = &log.events[index];
    event_order[log.events[index].id] = index;
  }

  std::map<std::string, std::set<std::string>> object_event_ids;
  for (const auto& relation : log.relations) {
    const auto object_it = object_types.find(relation.object_id);
    if (object_it == object_types.end() || object_it->second != type ||
        !events.contains(relation.event_id)) {
      continue;
    }
    object_event_ids[relation.object_id].insert(relation.event_id);
  }

  FlattenedTypeLog result;
  for (const auto& [object_id, event_ids] : object_event_ids) {
    std::vector<std::string> ordered_ids(event_ids.begin(), event_ids.end());
    std::stable_sort(ordered_ids.begin(), ordered_ids.end(),
                     [&](const std::string& left, const std::string& right) {
                       const auto left_time = parse_timestamp(events.at(left)->timestamp);
                       const auto right_time = parse_timestamp(events.at(right)->timestamp);
                       if (left_time && right_time && *left_time != *right_time)
                         return *left_time < *right_time;
                       return event_order.at(left) < event_order.at(right);
                     });
    Trace trace;
    trace.case_id = object_id;
    for (const auto& event_id : ordered_ids) {
      const auto* event = events.at(event_id);
      trace.events.push_back(Event{object_id, event->activity, event->timestamp,
                                   {}, event_order.at(event_id), {}, {}});
    }
    if (trace.events.empty()) continue;
    std::vector<std::string> variant;
    variant.reserve(trace.events.size());
    for (const auto& event : trace.events) variant.push_back(event.activity);
    result.log.traces.push_back(std::move(trace));
    result.variants.push_back(std::move(variant));
  }

  std::map<std::vector<std::string>, std::size_t> variant_index;
  std::vector<std::vector<std::string>> unique_variants;
  for (const auto& variant : result.variants) {
    const auto [it, inserted] = variant_index.emplace(variant, unique_variants.size());
    if (inserted) {
      unique_variants.push_back(variant);
      result.variant_counts.push_back(1);
    } else {
      ++result.variant_counts[it->second];
    }
  }
  result.variants = std::move(unique_variants);
  return result;
}

SawNet make_saw_net(const std::string& type, const PetriNet& net,
                   const FlattenedTypeLog& flattened) {
  SawNet result;
  result.object_type = type;
  result.net = net;
  const auto firing_sequences = replay_firing_sequences(net, flattened.variants);
  for (std::size_t variant_index = 0;
       variant_index < firing_sequences.size(); ++variant_index) {
    if (!firing_sequences[variant_index]) continue;
    std::vector<std::size_t> counts(net.transitions().size(), 0);
    for (const auto& firing_group : *firing_sequences[variant_index]) {
      for (const auto transition : firing_group) {
        if (transition < counts.size()) ++counts[transition];
      }
    }
    const auto multiplicity = flattened.variant_counts[variant_index];
    for (std::size_t transition = 0; transition < counts.size(); ++transition) {
      if (counts[transition] == 0) continue;
      result.transition_weights[transition][counts[transition]] += multiplicity;
    }
  }
  return result;
}

SawNet merge_saw_nets(const std::map<std::string, SawNet>& object_nets,
                     std::map<std::size_t, std::string>& place_types,
                     std::map<std::size_t, std::string>& silent_types) {
  SawNet result;
  result.object_type = "multi";
  std::set<std::string> activities;
  for (const auto& [type, saw] : object_nets) {
    (void)type;
    activities.insert(saw.net.activities().begin(), saw.net.activities().end());
  }
  result.net.set_activities({activities.begin(), activities.end()});
  std::map<std::string, std::size_t> activity_index;
  for (std::size_t index = 0; index < result.net.activities().size(); ++index)
    activity_index[result.net.activities()[index]] = index;
  std::map<std::string, std::size_t> visible_transitions;

  bool set_source = false;
  for (const auto& [type, saw] : object_nets) {
    std::vector<std::size_t> places(saw.net.place_count());
    for (std::size_t place = 0; place < places.size(); ++place) {
      places[place] = result.net.add_place();
      place_types[places[place]] = type;
    }
    if (!set_source && !places.empty()) {
      result.net.set_source(places[saw.net.source()]);
      result.net.set_sink(places[saw.net.sink()]);
      set_source = true;
    }
    for (std::size_t index = 0; index < saw.net.transitions().size(); ++index) {
      const auto& transition = saw.net.transitions()[index];
      std::optional<std::size_t> activity;
      if (transition.activity) {
        activity = activity_index.at(saw.net.activities()[*transition.activity]);
      }
      std::size_t merged = result.net.transitions().size();
      if (activity) {
        const auto label = result.net.activities()[*activity];
        const auto existing = visible_transitions.find(label);
        if (existing != visible_transitions.end()) merged = existing->second;
      }
      if (merged == result.net.transitions().size()) {
        std::vector<std::size_t> inputs, outputs;
        for (const auto place : transition.inputs) inputs.push_back(places[place]);
        for (const auto place : transition.outputs) outputs.push_back(places[place]);
        merged = result.net.add_transition(activity, std::move(inputs),
                                           std::move(outputs));
        if (activity) {
          visible_transitions.emplace(result.net.activities()[*activity], merged);
        } else {
          silent_types[merged] = type;
        }
      } else {
        std::vector<std::size_t> additional_inputs;
        std::vector<std::size_t> additional_outputs;
        for (const auto place : transition.inputs) {
          const auto mapped = places[place];
          additional_inputs.push_back(mapped);
        }
        for (const auto place : transition.outputs) {
          const auto mapped = places[place];
          additional_outputs.push_back(mapped);
        }
        result.net.append_transition_arcs(
            merged, std::move(additional_inputs), std::move(additional_outputs));
      }
      const auto weights = saw.transition_weights.find(index);
      if (weights != saw.transition_weights.end()) {
        for (const auto& [multiplicity, frequency] : weights->second)
          result.transition_weights[merged][multiplicity] += frequency;
      }
    }
  }
  if (!set_source) {
    const auto source = result.net.add_place();
    result.net.set_source(source);
    result.net.set_sink(source);
  }
  return result;
}

void write_net(std::ostringstream& out, const SawNet& saw) {
  out << "{\"object_type\":";
  write_json_string(out, saw.object_type);
  out << ",\"place_count\":" << saw.net.place_count()
      << ",\"source\":" << saw.net.source()
      << ",\"sink\":" << saw.net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < saw.net.activities().size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, saw.net.activities()[i]);
  }
  out << "],\"transitions\":[";
  for (std::size_t i = 0; i < saw.net.transitions().size(); ++i) {
    if (i != 0) out << ',';
    const auto& transition = saw.net.transitions()[i];
    out << "{\"activity\":";
    if (transition.activity) out << *transition.activity;
    else out << "null";
    out << ",\"inputs\":[";
    for (std::size_t j = 0; j < transition.inputs.size(); ++j) {
      if (j != 0) out << ',';
      out << transition.inputs[j];
    }
    out << "],\"outputs\":[";
    for (std::size_t j = 0; j < transition.outputs.size(); ++j) {
      if (j != 0) out << ',';
      out << transition.outputs[j];
    }
    out << "],\"weight_distribution\":{";
    const auto weights = saw.transition_weights.find(i);
    if (weights != saw.transition_weights.end()) {
      bool first = true;
      for (const auto& [multiplicity, frequency] : weights->second) {
        if (!first) out << ',';
        first = false;
        write_json_string(out, std::to_string(multiplicity));
        out << ':' << frequency;
      }
    }
    out << "}}";
  }
  out << "]}";
}

}  // namespace

SawNetDiscoveryResult discover_saw_nets(const Ocel& log) {
  const auto ocpn = discover_ocpn(log);
  SawNetDiscoveryResult result;
  for (const auto& type : ocpn.object_types) {
    const auto net_it = ocpn.petri_nets.find(type);
    if (net_it == ocpn.petri_nets.end()) continue;
    result.ot_saw_nets.emplace(
        type, make_saw_net(type, net_it->second, flatten_type(log, type)));
  }
  result.multi_saw_net = merge_saw_nets(
      result.ot_saw_nets, result.multi_place_object_types,
      result.multi_silent_transition_object_types);
  return result;
}

std::string saw_nets_json(const SawNetDiscoveryResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"SAW_NETS\",\"variant\":\"CLASSIC\",\"ot_saw_nets\":{";
  bool first = true;
  for (const auto& [type, saw] : result.ot_saw_nets) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, type);
    out << ':';
    write_net(out, saw);
  }
  out << "},\"multi_saw_net\":";
  write_net(out, result.multi_saw_net);
  out << ",\"multi_place_object_types\":{";
  first = true;
  for (const auto& [place, type] : result.multi_place_object_types) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, std::to_string(place));
    out << ':';
    write_json_string(out, type);
  }
  out << "},\"multi_silent_transition_object_types\":{";
  first = true;
  for (const auto& [transition, type] : result.multi_silent_transition_object_types) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, std::to_string(transition));
    out << ':';
    write_json_string(out, type);
  }
  out << "}}";
  return out.str();
}

}  // namespace minera
