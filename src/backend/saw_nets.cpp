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
#include "ocpn_projection.hpp"

namespace minera {
namespace {

struct FlattenedTypeLog {
  EventLog log;
  std::vector<std::vector<std::string>> variants;
  // The event ids for each object trace are retained separately from the
  // activity-only variants.  An event can be related to several objects of
  // this type, so collapsing to activities alone would lose its multiplicity.
  std::vector<std::vector<std::string>> trace_event_ids;
  std::vector<std::size_t> trace_variant_indices;
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
    result.trace_event_ids.push_back(ordered_ids);
  }

  std::map<std::vector<std::string>, std::size_t> variant_index;
  std::vector<std::vector<std::string>> unique_variants;
  for (const auto& variant : result.variants) {
    const auto [it, inserted] =
        variant_index.emplace(variant, unique_variants.size());
    if (inserted) unique_variants.push_back(variant);
    result.trace_variant_indices.push_back(it->second);
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
  // Aggregate by original event id across all object traces before converting
  // counts into a distribution.  This distinguishes one shared event linked
  // to two objects (multiplicity 2) from two separate events (multiplicity 1
  // observed twice).
  std::vector<std::map<std::string, std::size_t>> event_counts(
      net.transitions().size());
  for (std::size_t trace = 0; trace < flattened.trace_event_ids.size(); ++trace) {
    if (trace >= flattened.trace_variant_indices.size()) continue;
    const auto variant_index = flattened.trace_variant_indices[trace];
    if (variant_index >= firing_sequences.size() ||
        !firing_sequences[variant_index]) {
      continue;
    }
    const auto& event_ids = flattened.trace_event_ids[trace];
    const auto& groups = *firing_sequences[variant_index];
    for (std::size_t group_index = 0; group_index < groups.size(); ++group_index) {
      if (event_ids.empty()) continue;
      // The final routing group has no event of its own and is attributed to
      // the trace's final event, matching the replay event association.
      const auto& event_id = event_ids[std::min(group_index, event_ids.size() - 1)];
      for (const auto transition : groups[group_index]) {
        if (transition < event_counts.size()) ++event_counts[transition][event_id];
      }
    }
  }
  for (std::size_t transition = 0; transition < event_counts.size(); ++transition) {
    for (const auto& [event_id, count] : event_counts[transition]) {
      (void)event_id;
      if (count != 0) result.transition_weights[transition][count]++;
    }
  }
  return result;
}

SawNet merge_saw_nets(const std::map<std::string, SawNet>& object_nets,
                     std::map<std::size_t, std::string>& place_types,
                     std::map<std::size_t, std::string>& silent_types,
                     std::map<std::string, std::string>& arc_types) {
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
        for (std::size_t arc = 0; arc < transition.inputs.size(); ++arc) {
          arc_types[std::to_string(merged) + ":input:" + std::to_string(arc)] = type;
        }
        for (std::size_t arc = 0; arc < transition.outputs.size(); ++arc) {
          arc_types[std::to_string(merged) + ":output:" + std::to_string(arc)] = type;
        }
        if (activity) {
          visible_transitions.emplace(result.net.activities()[*activity], merged);
        } else {
          silent_types[merged] = type;
        }
      } else {
        const auto input_offset = result.net.transitions()[merged].inputs.size();
        const auto output_offset = result.net.transitions()[merged].outputs.size();
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
        for (std::size_t arc = 0; arc < transition.inputs.size(); ++arc) {
          arc_types[std::to_string(merged) + ":input:" +
                    std::to_string(input_offset + arc)] = type;
        }
        for (std::size_t arc = 0; arc < transition.outputs.size(); ++arc) {
          arc_types[std::to_string(merged) + ":output:" +
                    std::to_string(output_offset + arc)] = type;
        }
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
  return discover_saw_nets(log, {});
}

SawNetDiscoveryResult discover_saw_nets(
    const Ocel& log, const SawNetDiscoveryOptions& options) {
  const Ocel projected = project_ocel_labels(
      log, options.activity_attribute, options.object_type_attribute, "SAW");
  const auto ocpn = discover_ocpn(projected);
  SawNetDiscoveryResult result;
  for (const auto& type : ocpn.object_types) {
    const auto net_it = ocpn.petri_nets.find(type);
    if (net_it == ocpn.petri_nets.end()) continue;
    result.ot_saw_nets.emplace(
        type, make_saw_net(type, net_it->second, flatten_type(projected, type)));
  }
  result.multi_saw_net = merge_saw_nets(
      result.ot_saw_nets, result.multi_place_object_types,
      result.multi_silent_transition_object_types,
      result.multi_arc_object_types);
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
  out << "},\"multi_arc_object_types\":{";
  first = true;
  for (const auto& [arc, type] : result.multi_arc_object_types) {
    if (!first) out << ',';
    first = false;
    write_json_string(out, arc);
    out << ':';
    write_json_string(out, type);
  }
  out << "}}";
  return out.str();
}

}  // namespace minera
