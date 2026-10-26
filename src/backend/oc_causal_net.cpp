#include "minera/oc_causal_net.hpp"
#include "minera/oc_causal_net_reference_playout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"
#include "ocpn_projection.hpp"

namespace minera {
namespace {

struct CausalObject {
  std::string type;
  std::string id;
};

struct Obligation {
  std::string related_activity;
  std::string object_type;
  std::string object_id;

  friend bool operator<(const Obligation& left, const Obligation& right) {
    return std::tie(left.related_activity, left.object_type, left.object_id) <
           std::tie(right.related_activity, right.object_type, right.object_id);
  }
  friend bool operator==(const Obligation&, const Obligation&) = default;
};

struct CausalState {
  std::vector<std::vector<Obligation>> obligations;
  // Explicit START_<type> activities consume these per-object boundary
  // bindings.  Keeping the remaining pool in the state prevents a boundary
  // activity from reusing the same object on a later search step.
  std::vector<std::vector<CausalObject>> start_available;
  std::vector<std::size_t> activity_counts;
};

struct CausalAction {
  std::size_t activity_index = 0;
  std::string activity;
  std::vector<Obligation> consumed;
  std::vector<CausalObject> consumed_start;
  std::vector<std::pair<std::string, Obligation>> produced;
  std::vector<CausalObject> objects;
  // Marker-group actions have no Petri transition object of their own.  The
  // stable activity index is therefore used as the transition identity in
  // the optional trace projection.  Keeping one identity per bound object
  // preserves batching and shared-event semantics without losing firings.
  std::vector<OcpnFiredTransition> transitions;
};

struct MarkerSelection {
  std::vector<Obligation> consumed;
  std::vector<CausalObject> consumed_start;
  std::vector<CausalObject> objects;
};

struct OutputSelection {
  std::vector<std::pair<std::string, CausalObject>> produced;
  std::vector<CausalObject> objects;
};

std::string quote_json(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          constexpr char digits[] = "0123456789abcdef";
          out << "\\u00" << digits[c >> 4] << digits[c & 0x0f];
        } else {
          out << static_cast<char>(c);
        }
        break;
    }
  }
  out << '"';
  return out.str();
}

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

bool has_marker_groups(const OcCausalNet& net) {
  return std::any_of(net.activities.begin(), net.activities.end(),
                     [](const auto& activity) {
                       return !activity.input_marker_groups.empty() ||
                              !activity.output_marker_groups.empty();
                     });
}

std::size_t rounded_branching_limit(double cap, std::mt19937_64& rng) {
  if (cap <= 0.0) return 0;
  const auto integral = std::floor(cap);
  auto result = integral >=
                        static_cast<double>(std::numeric_limits<std::size_t>::max())
                    ? std::numeric_limits<std::size_t>::max()
                    : static_cast<std::size_t>(integral);
  const auto fraction = cap - integral;
  if (fraction > 0.0 &&
      std::uniform_real_distribution<double>(0.0, 1.0)(rng) < fraction &&
      result != std::numeric_limits<std::size_t>::max()) {
    ++result;
  }
  return result;
}

template <typename Value>
void sample_branch(std::vector<Value>& values, double cap,
                   std::mt19937_64& rng) {
  if (cap <= 0.0 || values.empty()) return;
  const auto limit = rounded_branching_limit(cap, rng);
  if (limit >= values.size()) return;
  std::shuffle(values.begin(), values.end(), rng);
  values.resize(limit);
}

std::string state_key(const CausalState& state) {
  std::ostringstream out;
  for (const auto& obligations : state.obligations) {
    out << '[';
    for (const auto& obligation : obligations) {
      out << obligation.related_activity.size() << ':'
          << obligation.related_activity << obligation.object_type.size() << ':'
          << obligation.object_type << obligation.object_id.size() << ':'
          << obligation.object_id << ';';
    }
    out << ']';
  }
  out << '|';
  for (const auto& available : state.start_available) {
    out << '[';
    for (const auto& object : available)
      out << object.type.size() << ':' << object.type << object.id.size() << ':'
          << object.id << ';';
    out << ']';
  }
  out << '|';
  for (const auto count : state.activity_counts) out << count << ',';
  return out.str();
}

bool same_object(const CausalObject& left, const CausalObject& right) {
  return left.type == right.type && left.id == right.id;
}

void add_unique_object(std::vector<CausalObject>& objects,
                       const CausalObject& object) {
  if (std::none_of(objects.begin(), objects.end(),
                   [&](const auto& existing) { return same_object(existing, object); }))
    objects.push_back(object);
}

void validate_marker(const OcCausalMarker& marker) {
  if (marker.related_activity.empty() || marker.object_type.empty() ||
      marker.min_cardinality > marker.max_cardinality)
    throw std::invalid_argument("OC causal marker is invalid");
}

void validate_group(const OcCausalMarkerGroup& group) {
  std::set<std::pair<std::string, std::string>> seen;
  for (const auto& marker : group.markers) {
    validate_marker(marker);
    if (!seen.emplace(marker.related_activity, marker.object_type).second)
      throw std::invalid_argument(
          "OC causal marker groups cannot repeat a related activity and object type");
  }
}

std::map<std::string, std::size_t> activity_indices(const OcCausalNet& net) {
  std::map<std::string, std::size_t> result;
  for (std::size_t index = 0; index < net.activities.size(); ++index) {
    const auto& name = net.activities[index].activity;
    if (name.empty() || !result.emplace(name, index).second)
      throw std::invalid_argument("OC causal activities must be unique and non-empty");
    for (const auto& group : net.activities[index].input_marker_groups)
      validate_group(group);
    for (const auto& group : net.activities[index].output_marker_groups)
      validate_group(group);
  }
  for (const auto& activity : net.activities) {
    for (const auto& group : activity.input_marker_groups)
      for (const auto& marker : group.markers)
        if (!starts_with(marker.related_activity, "START_") &&
            !result.contains(marker.related_activity))
          throw std::invalid_argument(
              "OC causal input marker names an unknown activity: " +
              marker.related_activity);
    for (const auto& group : activity.output_marker_groups)
      for (const auto& marker : group.markers)
        if (!starts_with(marker.related_activity, "END_") &&
            !result.contains(marker.related_activity))
          throw std::invalid_argument(
              "OC causal output marker names an unknown activity: " +
              marker.related_activity);
  }
  return result;
}

std::vector<std::string> causal_object_types(
    const OcCausalNet& net, const std::map<std::string, std::size_t>& indices) {
  std::vector<std::string> result = net.object_types;
  for (const auto& type : net.workflow_projection.object_types) result.push_back(type);
  for (const auto& activity : net.activities) {
    if (starts_with(activity.activity, "START_") && activity.activity.size() > 6)
      result.push_back(activity.activity.substr(6));
    for (const auto& group : activity.input_marker_groups)
      for (const auto& marker : group.markers) result.push_back(marker.object_type);
    for (const auto& group : activity.output_marker_groups)
      for (const auto& marker : group.markers) result.push_back(marker.object_type);
  }
  for (const auto& type : result)
    if (type.empty()) throw std::invalid_argument("OC causal object types cannot be empty");
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  (void)indices;
  return result;
}

std::vector<CausalObject> causal_objects(
    const std::vector<std::string>& types, const OcpnPlayoutOptions& options) {
  std::vector<CausalObject> result;
  std::set<std::string> ids;
  for (const auto& type : types) {
    const auto supplied = options.objects.find(type);
    const std::vector<std::string> ids_for_type =
        supplied == options.objects.end() ? std::vector<std::string>{type + "_1"}
                                           : supplied->second;
    for (const auto& id : ids_for_type) {
      if (id.empty() || !ids.insert(id).second)
        throw std::invalid_argument(
            "OC causal playout object ids must be non-empty and globally unique");
      result.push_back({type, id});
    }
  }
  for (const auto& [type, ids_for_type] : options.objects) {
    if (!std::binary_search(types.begin(), types.end(), type))
      throw std::invalid_argument("OC causal playout object type is not in the model: " +
                                  type);
    (void)ids_for_type;
  }
  return result;
}

bool key_constraints_hold(const std::vector<const OcCausalMarker*>& markers,
                          const std::vector<std::vector<CausalObject>>& selected) {
  for (std::size_t left = 0; left < markers.size(); ++left) {
    for (std::size_t right = left + 1; right < markers.size(); ++right) {
      if (markers[left]->marker_key == 0 || markers[right]->marker_key == 0 ||
          markers[left]->marker_key != markers[right]->marker_key ||
          markers[left]->object_type != markers[right]->object_type ||
          markers[left]->related_activity == markers[right]->related_activity)
        continue;
      for (const auto& a : selected[left])
        for (const auto& b : selected[right])
          if (same_object(a, b)) return false;
    }
  }
  return true;
}

template <typename Candidate, typename Emit>
void enumerate_combinations(const std::vector<Candidate>& candidates,
                            std::size_t min_count, std::size_t max_count,
                            Emit emit) {
  if (min_count > max_count) return;
  std::vector<Candidate> selected;
  std::function<void(std::size_t)> visit = [&](std::size_t offset) {
    if (selected.size() >= min_count && selected.size() <= max_count)
      emit(selected);
    if (selected.size() == max_count) return;
    for (std::size_t index = offset; index < candidates.size(); ++index) {
      selected.push_back(candidates[index]);
      visit(index + 1);
      selected.pop_back();
    }
  };
  visit(0);
}

void enumerate_input_group(
    const OcCausalMarkerGroup& group, const CausalState& state,
    std::size_t activity_index, std::vector<MarkerSelection>& result) {
  std::vector<const OcCausalMarker*> markers;
  markers.reserve(group.markers.size());
  for (const auto& marker : group.markers) markers.push_back(&marker);
  std::vector<std::vector<std::size_t>> selected_indices(markers.size());
  std::vector<std::vector<CausalObject>> selected_objects(markers.size());
  std::function<void(std::size_t)> visit = [&](std::size_t marker_index) {
    if (marker_index == markers.size()) {
      if (!key_constraints_hold(markers, selected_objects)) return;
      MarkerSelection selection;
      for (std::size_t index = 0; index < markers.size(); ++index) {
        for (const auto obligation_index : selected_indices[index])
          selection.consumed.push_back(
              state.obligations[activity_index][obligation_index]);
        for (const auto& obligation : selected_objects[index])
          add_unique_object(selection.objects,
                           obligation);
      }
      if (!selection.consumed.empty() || !selection.objects.empty())
        result.push_back(std::move(selection));
      return;
    }
    const auto& marker = *markers[marker_index];
    std::vector<std::size_t> candidates;
    std::set<std::pair<std::string, std::string>> seen_objects;
    for (std::size_t index = 0; index < state.obligations[activity_index].size();
         ++index) {
      const auto& obligation = state.obligations[activity_index][index];
      if (obligation.related_activity == marker.related_activity &&
          obligation.object_type == marker.object_type &&
          seen_objects.emplace(obligation.object_type, obligation.object_id).second)
        candidates.push_back(index);
    }
    const auto max_count = marker.max_cardinality == kOcCausalInfiniteCardinality
                               ? candidates.size()
                               : std::min(marker.max_cardinality, candidates.size());
    if (marker.min_cardinality > max_count) return;
    enumerate_combinations<std::size_t>(
        candidates, marker.min_cardinality, max_count,
        [&](const std::vector<std::size_t>& choice) {
          selected_indices[marker_index] = choice;
          selected_objects[marker_index].clear();
          for (const auto index : choice) {
            const auto& obligation = state.obligations[activity_index][index];
            selected_objects[marker_index].push_back(
                {obligation.object_type, obligation.object_id});
          }
          visit(marker_index + 1);
        });
  };
  visit(0);
}

void enumerate_output_group(
    const OcCausalMarkerGroup& group, const MarkerSelection& input,
    std::vector<OutputSelection>& result) {
  std::map<std::string, std::vector<CausalObject>> objects_by_type;
  for (const auto& object : input.objects)
    objects_by_type[object.type].push_back(object);
  for (auto& [type, objects] : objects_by_type) {
    std::sort(objects.begin(), objects.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });
    (void)type;
  }
  std::vector<const OcCausalMarker*> markers;
  for (const auto& marker : group.markers) markers.push_back(&marker);
  std::vector<std::vector<CausalObject>> selected(markers.size());
  std::map<std::string, std::vector<std::size_t>> marker_indices_by_type;
  for (std::size_t index = 0; index < markers.size(); ++index)
    marker_indices_by_type[markers[index]->object_type].push_back(index);
  for (const auto& [type, objects] : objects_by_type)
    if (!marker_indices_by_type.contains(type) && !objects.empty()) return;
  for (const auto& [type, marker_indices] : marker_indices_by_type) {
    if (!objects_by_type.contains(type)) {
      if (std::any_of(marker_indices.begin(), marker_indices.end(), [&](const auto index) {
            return markers[index]->min_cardinality != 0;
          }))
        return;
    }
  }

  std::vector<std::string> types;
  for (const auto& [type, objects] : objects_by_type) {
    (void)objects;
    types.push_back(type);
  }
  std::function<void(std::size_t, std::size_t)> visit =
      [&](std::size_t type_index, std::size_t object_index) {
        if (type_index == types.size()) {
          if (!key_constraints_hold(markers, selected)) return;
          for (std::size_t index = 0; index < markers.size(); ++index) {
            const auto& marker = *markers[index];
            if (selected[index].size() < marker.min_cardinality ||
                (marker.max_cardinality != kOcCausalInfiniteCardinality &&
                 selected[index].size() > marker.max_cardinality))
              return;
          }
          OutputSelection output;
          for (std::size_t index = 0; index < markers.size(); ++index) {
            for (const auto& object : selected[index]) {
              output.produced.push_back({markers[index]->related_activity, object});
              add_unique_object(output.objects, object);
            }
          }
          if (!output.produced.empty() || !input.objects.empty())
            result.push_back(std::move(output));
          return;
        }
        const auto& type = types[type_index];
        const auto& objects = objects_by_type.at(type);
        const auto& marker_indices = marker_indices_by_type.at(type);
        if (object_index == objects.size()) {
          visit(type_index + 1, 0);
          return;
        }
        enumerate_combinations<std::size_t>(
            marker_indices, 1, marker_indices.size(),
            [&](const std::vector<std::size_t>& choice) {
              bool valid = true;
              for (std::size_t left = 0; left < choice.size() && valid; ++left)
                for (std::size_t right = left + 1; right < choice.size(); ++right)
                  if (markers[choice[left]]->marker_key != 0 &&
                      markers[choice[left]]->marker_key ==
                          markers[choice[right]]->marker_key &&
                      markers[choice[left]]->related_activity !=
                          markers[choice[right]]->related_activity)
                    valid = false;
              if (!valid) return;
              for (const auto index : choice)
                selected[index].push_back(objects[object_index]);
              visit(type_index, object_index + 1);
              for (const auto index : choice) selected[index].pop_back();
            });
      };
  visit(0, 0);
}

std::vector<CausalAction> enabled_actions(
    const OcCausalNet& net, const std::map<std::string, std::size_t>& indices,
    const CausalState& state, const OcpnPlayoutOptions& options,
    std::mt19937_64& rng) {
  std::vector<CausalAction> result;
  std::vector<std::vector<CausalAction>> activity_groups;
  for (std::size_t activity_index = 0; activity_index < net.activities.size();
       ++activity_index) {
    const auto& activity = net.activities[activity_index];
    if (state.activity_counts[activity_index] >= options.max_bindings_per_activity)
      continue;

    std::vector<MarkerSelection> inputs;
    if (activity.input_marker_groups.empty()) {
      if (starts_with(activity.activity, "END_")) {
        inputs.push_back({{}, {}});
      } else if (starts_with(activity.activity, "START_")) {
        const auto boundary_type = activity.activity.substr(6);
        std::vector<CausalObject> available;
        if (activity_index < state.start_available.size())
          for (const auto& object : state.start_available[activity_index])
            if (object.type == boundary_type) available.push_back(object);
        enumerate_combinations<CausalObject>(
            available, 1, available.size(),
            [&](const std::vector<CausalObject>& choice) {
              MarkerSelection input;
              input.consumed_start = choice;
              input.objects = choice;
              inputs.push_back(std::move(input));
            });
      } else {
        continue;
      }
    } else {
      for (const auto& group : activity.input_marker_groups)
        enumerate_input_group(group, state, activity_index, inputs);
    }
    if (inputs.empty()) continue;

    std::vector<CausalAction> activity_actions;
    for (const auto& input : inputs) {
      std::vector<OutputSelection> outputs;
      if (starts_with(activity.activity, "END_") ||
          activity.output_marker_groups.empty()) {
        outputs.push_back({{}, {}});
      } else {
        for (const auto& group : activity.output_marker_groups)
          enumerate_output_group(group, input, outputs);
      }
      for (const auto& output : outputs) {
        CausalAction action;
        action.activity_index = activity_index;
        action.activity = activity.activity;
        action.consumed = input.consumed;
        action.consumed_start = input.consumed_start;
        for (const auto& [successor, object] : output.produced) {
          if (!starts_with(successor, "END_") && !indices.contains(successor))
            throw std::invalid_argument(
                "OC causal output marker names an unknown activity: " +
                successor);
          if (indices.contains(successor))
            action.produced.emplace_back(
                successor,
                Obligation{activity.activity, object.type, object.id});
        }
        action.objects = input.objects;
        for (const auto& object : output.objects) add_unique_object(action.objects, object);
        action.transitions.reserve(action.objects.size());
        for (const auto& object : action.objects)
          action.transitions.push_back(
              {object.type, object.id, activity_index});
        activity_actions.push_back(std::move(action));
      }
    }
    if (!activity_actions.empty())
      activity_groups.push_back(std::move(activity_actions));
  }
  sample_branch(activity_groups, options.branching_factor_activities, rng);
  for (auto& activity_actions : activity_groups) {
    sample_branch(activity_actions, options.branching_factor_bindings, rng);
    result.insert(result.end(), std::make_move_iterator(activity_actions.begin()),
                  std::make_move_iterator(activity_actions.end()));
  }
  sample_branch(result, options.branching_factor_transitions, rng);
  return result;
}

void fire_action(CausalState& state, const CausalAction& action,
                 const std::map<std::string, std::size_t>& indices) {
  auto& obligations = state.obligations[action.activity_index];
  for (const auto& consumed : action.consumed) {
    const auto found = std::find(obligations.begin(), obligations.end(), consumed);
    if (found == obligations.end())
      throw std::logic_error("OC causal action consumed a missing obligation");
    obligations.erase(found);
  }
  if (!action.consumed_start.empty()) {
    auto& available = state.start_available[action.activity_index];
    for (const auto& consumed : action.consumed_start) {
      const auto found = std::find_if(
          available.begin(), available.end(),
          [&](const auto& object) { return same_object(object, consumed); });
      if (found == available.end())
        throw std::logic_error("OC causal action consumed a missing start binding");
      available.erase(found);
    }
  }
  for (const auto& [successor, produced] : action.produced)
    state.obligations[indices.at(successor)].push_back(produced);
  for (auto& pending : state.obligations)
    std::sort(pending.begin(), pending.end());
}

bool state_is_terminal(const CausalState& state) {
  return std::all_of(state.obligations.begin(), state.obligations.end(),
                     [](const auto& obligations) { return obligations.empty(); });
}

bool has_enabled_explicit_start(const OcCausalNet& net,
                                const CausalState& state,
                                std::size_t max_bindings_per_activity) {
  for (std::size_t index = 0; index < net.activities.size(); ++index) {
    const auto& activity = net.activities[index];
    if (starts_with(activity.activity, "START_") &&
        activity.input_marker_groups.empty() &&
        state.activity_counts[index] < max_bindings_per_activity &&
        index < state.start_available.size()) {
      const auto boundary_type = activity.activity.substr(6);
      if (std::any_of(state.start_available[index].begin(),
                      state.start_available[index].end(),
                      [&](const auto& object) { return object.type == boundary_type; }))
        return true;
    }
  }
  return false;
}

void append_emitted_trace(Ocel& log, const std::vector<OcpnBinding>& sequence,
                          const std::vector<CausalObject>& objects,
                          const OcpnPlayoutOptions& options,
                          std::size_t trace_index,
                          std::vector<OcpnBinding>* returned_sequence,
                          const std::vector<OcpnTraceEvent>* source_trace,
                          std::vector<OcpnTraceEvent>* returned_trace) {
  const auto suffix = options.objects_unique_per_trace
                          ? "_" + std::to_string(trace_index + 1)
                          : std::string();
  std::map<std::pair<std::string, std::string>, std::string> emitted_ids;
  for (const auto& object : objects) {
    const auto emitted = object.id + suffix;
    emitted_ids[{object.type, object.id}] = emitted;
    if (std::none_of(log.objects.begin(), log.objects.end(),
                     [&](const auto& existing) { return existing.id == emitted; }))
      log.objects.push_back({emitted, object.type, {}});
  }

  struct EmittedEvent {
    OcpnBinding binding;
    OcpnTraceEvent trace;
  };
  std::vector<EmittedEvent> events;
  for (std::size_t source_index = 0; source_index < sequence.size();
       ++source_index) {
    const auto& binding = sequence[source_index];
    if (starts_with(binding.activity, "START_") ||
        starts_with(binding.activity, "END_"))
      continue;

    OcpnTraceEvent source_event;
    source_event.activity = binding.activity;
    source_event.objects = binding.objects;
    if (source_trace != nullptr && source_index < source_trace->size())
      source_event = (*source_trace)[source_index];

    if (options.event_binding == OcpnEventBinding::kIndependent &&
        binding.objects.size() > 1) {
      for (const auto& object : binding.objects)
        {
          EmittedEvent emitted;
          emitted.binding = {binding.activity, {object}};
          emitted.trace.activity = binding.activity;
          emitted.trace.objects = {object};
          for (const auto& transition : source_event.transitions)
            if (transition.object_type == object.first &&
                transition.object_id == object.second)
              emitted.trace.transitions.push_back(transition);
          events.push_back(std::move(emitted));
        }
    } else if (options.event_binding == OcpnEventBinding::kSharedActivity &&
               !events.empty() &&
               events.back().binding.activity == binding.activity) {
      for (const auto& object : binding.objects)
        if (std::find(events.back().binding.objects.begin(),
                      events.back().binding.objects.end(), object) ==
            events.back().binding.objects.end()) {
          events.back().binding.objects.push_back(object);
          events.back().trace.objects.push_back(object);
        }
      events.back().trace.transitions.insert(
          events.back().trace.transitions.end(), source_event.transitions.begin(),
          source_event.transitions.end());
    } else {
      events.push_back({binding, std::move(source_event)});
    }
  }
  if (returned_sequence) {
    returned_sequence->clear();
    returned_sequence->reserve(events.size());
    for (const auto& event : events) returned_sequence->push_back(event.binding);
  }
  if (returned_trace) {
    returned_trace->clear();
    returned_trace->reserve(events.size());
    for (const auto& event : events) returned_trace->push_back(event.trace);
  }
  const auto event_offset = log.events.size();
  for (std::size_t index = 0; index < events.size(); ++index) {
    const auto event_id = "event_" + std::to_string(event_offset + index + 1);
    const auto timestamp = format_timestamp(
        options.start + static_cast<double>(trace_index) * options.case_interval +
        static_cast<double>(index) * options.event_interval);
    log.events.push_back({event_id, events[index].binding.activity, timestamp, {}});
    for (const auto& [type, object_id] : events[index].binding.objects) {
      const auto found = emitted_ids.find({type, object_id});
      if (found != emitted_ids.end()) log.relations.push_back({event_id, found->second, ""});
    }
  }
}

OcpnPlayoutResult causal_playout(
    const OcCausalNet& net, const std::map<std::string, std::size_t>& indices,
    const std::vector<std::string>& types, const OcpnPlayoutOptions& options) {
  if (options.max_bindings_per_activity == 0)
    throw std::invalid_argument("OC causal max_bindings_per_activity must be positive");
  if (options.max_states == 0)
    throw std::invalid_argument("OC causal max_states must be positive");
  if (!std::isfinite(options.branching_factor_transitions) ||
      options.branching_factor_transitions < 0.0 ||
      !std::isfinite(options.branching_factor_bindings) ||
      options.branching_factor_bindings < 0.0 ||
      !std::isfinite(options.branching_factor_activities) ||
      options.branching_factor_activities < 0.0)
    throw std::invalid_argument("OC causal branching factors must be finite and non-negative");
  if (!std::isfinite(options.start) || !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval) || options.case_interval < 0.0 ||
      options.event_interval < 0.0)
    throw std::invalid_argument("OC causal playout timestamps must be finite and non-negative");
  if (options.event_binding != OcpnEventBinding::kIndependent &&
      options.event_binding != OcpnEventBinding::kSharedActivity &&
      options.event_binding != OcpnEventBinding::kSynchronized)
    throw std::invalid_argument("OC causal event_binding is invalid");

  const auto objects = causal_objects(types, options);
  CausalState initial;
  initial.obligations.resize(net.activities.size());
  initial.start_available.resize(net.activities.size());
  initial.activity_counts.assign(net.activities.size(), 0);
  for (std::size_t activity_index = 0; activity_index < net.activities.size();
       ++activity_index) {
    const auto& activity = net.activities[activity_index];
    if (starts_with(activity.activity, "START_") &&
        activity.input_marker_groups.empty()) {
      const auto boundary_type = activity.activity.substr(6);
      for (const auto& object : objects)
        if (object.type == boundary_type)
          initial.start_available[activity_index].push_back(object);
    }
    for (const auto& group : net.activities[activity_index].input_marker_groups) {
      for (const auto& marker : group.markers) {
        if (!starts_with(marker.related_activity, "START_")) continue;
        if (indices.contains(marker.related_activity)) continue;
        for (const auto& object : objects)
          if (object.type == marker.object_type)
            initial.obligations[activity_index].push_back(
                {marker.related_activity, object.type, object.id});
      }
    }
    std::sort(initial.obligations[activity_index].begin(),
              initial.obligations[activity_index].end());
    initial.obligations[activity_index].erase(
        std::unique(initial.obligations[activity_index].begin(),
                    initial.obligations[activity_index].end()),
        initial.obligations[activity_index].end());
  }

  OcpnPlayoutResult result;
  result.exists_trace_requested = options.exists_trace;
  result.return_sequences_requested = options.return_sequences;
  result.return_traces_requested = options.return_traces;
  if (!options.exists_trace && options.max_traces == 0) return result;

  std::vector<std::vector<OcpnBinding>> traces;
  std::vector<std::vector<OcpnTraceEvent>> transition_traces;
  std::set<std::string> visited;
  std::mt19937_64 rng(options.seed);
  std::size_t states = 0;
  std::function<void(const CausalState&, std::vector<OcpnBinding>&,
                     std::vector<OcpnTraceEvent>&)>
      visit = [&](const CausalState& state, std::vector<OcpnBinding>& sequence,
                  std::vector<OcpnTraceEvent>& transition_trace) {
        if (states++ >= options.max_states) return;
        if (state_is_terminal(state) &&
            !has_enabled_explicit_start(net, state,
                                        options.max_bindings_per_activity)) {
          result.trace_exists = true;
          if (options.max_traces != 0 && traces.size() < options.max_traces)
            traces.push_back(sequence);
          if (options.max_traces != 0 &&
              transition_traces.size() < options.max_traces)
            transition_traces.push_back(transition_trace);
          return;
        }
        if (options.exists_trace &&
            !visited.insert(state_key(state)).second)
          return;
        auto actions = enabled_actions(net, indices, state, options, rng);
        for (const auto& action : actions) {
          auto next = state;
          fire_action(next, action, indices);
          ++next.activity_counts[action.activity_index];
          sequence.push_back({action.activity, {}});
          for (const auto& object : action.objects)
            sequence.back().objects.emplace_back(object.type, object.id);
          if (options.return_traces) {
            OcpnTraceEvent trace_event;
            trace_event.activity = action.activity;
            trace_event.objects = sequence.back().objects;
            trace_event.transitions = action.transitions;
            transition_trace.push_back(std::move(trace_event));
          }
          visit(next, sequence, transition_trace);
          if (options.return_traces) transition_trace.pop_back();
          sequence.pop_back();
          if ((options.exists_trace && result.trace_exists) ||
              (options.max_traces != 0 && traces.size() >= options.max_traces))
            return;
        }
      };
  std::vector<OcpnBinding> sequence;
  std::vector<OcpnTraceEvent> transition_trace;
  visit(initial, sequence, transition_trace);

  if (options.exists_trace || !result.trace_exists) return result;
  for (std::size_t index = 0; index < traces.size(); ++index) {
    std::vector<OcpnBinding> returned;
    std::vector<OcpnTraceEvent> returned_trace;
    append_emitted_trace(result.log, traces[index], objects, options, index,
                         options.return_sequences ? &returned : nullptr,
                         options.return_traces ? &transition_traces[index]
                                                : nullptr,
                         options.return_traces ? &returned_trace : nullptr);
    if (options.return_sequences) result.sequences.push_back(std::move(returned));
    if (options.return_traces) result.traces.push_back(std::move(returned_trace));
  }
  return result;
}

struct EventContext {
  std::string activity;
  std::map<std::pair<std::string, std::string>, std::size_t> inputs;
  std::map<std::pair<std::string, std::string>, std::size_t> outputs;
};

std::string context_key(
    const std::map<std::pair<std::string, std::string>, std::size_t>& values) {
  std::ostringstream out;
  for (const auto& [key, count] : values)
    out << key.first << '\n' << key.second << '\n' << count << '|';
  return out.str();
}

OcCausalNet discover_marker_model(const Ocel& log,
                                  const OcpnDiscoveryOptions& options) {
  OcCausalNet result;
  result.workflow_projection = discover_ocpn(log, options);
  const Ocel projected = project_ocpn_labels(log, options);
  result.object_types = result.workflow_projection.object_types;
  std::map<std::string, std::vector<std::pair<std::string, std::string>>> by_object;
  std::map<std::string, std::string> object_types;
  for (const auto& object : projected.objects) {
    object_types[object.id] = object.type;
    by_object[object.id] = {};
  }
  std::map<std::string, const OcelEvent*> event_by_id;
  for (const auto& event : projected.events) event_by_id[event.id] = &event;
  std::map<std::string, std::vector<std::string>> event_objects;
  for (const auto& relation : projected.relations)
    if (event_by_id.contains(relation.event_id) && object_types.contains(relation.object_id))
      event_objects[relation.event_id].push_back(relation.object_id);
  for (auto& [event_id, objects] : event_objects) {
    std::sort(objects.begin(), objects.end());
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
  }
  for (const auto& event : projected.events) {
    for (const auto& object_id : event_objects[event.id])
      by_object[object_id].push_back({event.id, event.activity});
  }
  for (auto& [object_id, events] : by_object) {
    std::stable_sort(events.begin(), events.end(), [&](const auto& left, const auto& right) {
      const auto& a = *event_by_id.at(left.first);
      const auto& b = *event_by_id.at(right.first);
      const auto left_time = parse_timestamp(a.timestamp);
      const auto right_time = parse_timestamp(b.timestamp);
      if (left_time && right_time && *left_time != *right_time)
        return *left_time < *right_time;
      if (left_time.has_value() != right_time.has_value())
        return left_time.has_value();
      return std::tie(a.timestamp, a.id) < std::tie(b.timestamp, b.id);
    });
    (void)object_id;
  }

  std::map<std::string, std::map<std::string, EventContext>> contexts;
  for (const auto& event : log.events) {
    EventContext context;
    context.activity = event.activity;
    for (const auto& object_id : event_objects[event.id]) {
      const auto& object_type = object_types.at(object_id);
      const auto& events = by_object.at(object_id);
      const auto found = std::find_if(events.begin(), events.end(),
                                      [&](const auto& entry) { return entry.first == event.id; });
      if (found == events.end()) continue;
      const auto index = static_cast<std::size_t>(found - events.begin());
      const auto previous = index == 0 ? "START_" + object_type : events[index - 1].second;
      const auto next = index + 1 == events.size() ? "END_" + object_type
                                                    : events[index + 1].second;
      ++context.inputs[{object_type, previous}];
      ++context.outputs[{object_type, next}];
    }
    contexts[event.activity][context_key(context.inputs) + "#" +
                            context_key(context.outputs)] = std::move(context);
  }

  for (const auto& activity : result.workflow_projection.activities) {
    OcCausalActivity model;
    model.activity = activity;
    for (const auto& [key, context] : contexts[activity]) {
      (void)key;
      OcCausalMarkerGroup input;
      OcCausalMarkerGroup output;
      std::int64_t marker_key = 1;
      for (const auto& [marker_key_value, count] : context.inputs) {
        input.markers.push_back({marker_key_value.second, marker_key_value.first,
                                 count, count, marker_key++});
      }
      marker_key = 1;
      for (const auto& [marker_key_value, count] : context.outputs) {
        output.markers.push_back({marker_key_value.second, marker_key_value.first,
                                  count, count, marker_key++});
      }
      if (!input.markers.empty()) model.input_marker_groups.push_back(std::move(input));
      if (!output.markers.empty()) model.output_marker_groups.push_back(std::move(output));
    }
    result.activities.push_back(std::move(model));
  }
  return result;
}

}  // namespace

OcCausalNet discover_oc_causal_net(const Ocel& log,
                                   const OcpnDiscoveryOptions& options) {
  return discover_marker_model(log, options);
}

std::string serialize_oc_causal_net_json(const OcCausalNet& net) {
  std::ostringstream out;
  out << "{\"family\":\"OC_CAUSAL_NET\",\"variant\":\"MARKER_GROUPS\",\"model\":"
      << serialize_ocpn_json(net.workflow_projection) << ",\"object_types\":[";
  for (std::size_t index = 0; index < net.object_types.size(); ++index) {
    if (index) out << ',';
    out << quote_json(net.object_types[index]);
  }
  out << "],\"activities\":[";
  for (std::size_t index = 0; index < net.activities.size(); ++index) {
    if (index) out << ',';
    const auto& activity = net.activities[index];
    out << "{\"activity\":" << quote_json(activity.activity) <<
        ",\"input_marker_groups\":[";
    auto write_groups = [&](const std::vector<OcCausalMarkerGroup>& groups) {
      for (std::size_t group_index = 0; group_index < groups.size(); ++group_index) {
        if (group_index) out << ',';
        const auto& group = groups[group_index];
        out << "{\"support_count\":" << group.support_count << ",\"markers\":[";
        for (std::size_t marker_index = 0; marker_index < group.markers.size(); ++marker_index) {
          if (marker_index) out << ',';
          const auto& marker = group.markers[marker_index];
          out << "{\"related_activity\":" << quote_json(marker.related_activity)
              << ",\"object_type\":" << quote_json(marker.object_type)
              << ",\"min\":" << marker.min_cardinality << ",\"max\":"
              << marker.max_cardinality << ",\"key\":" << marker.marker_key << '}';
        }
        out << "]}";
      }
    };
    write_groups(activity.input_marker_groups);
    out << "],\"output_marker_groups\":[";
    write_groups(activity.output_marker_groups);
    out << "]}";
  }
  out << "]}";
  return out.str();
}

Ocel oc_causal_net_extensive_playout(const OcCausalNet& net,
                                     const OcpnPlayoutOptions& options) {
  return oc_causal_net_extensive_playout_result(net, options).log;
}

OcpnPlayoutResult oc_causal_net_extensive_playout_result(
    const OcCausalNet& net, const OcpnPlayoutOptions& options) {
  if (options.event_binding == OcpnEventBinding::kReference) {
    validate_reference_playout_controls(options);
    if (options.exists_trace || options.return_sequences || options.return_traces)
      throw std::invalid_argument(
          "reference causal-net playout returns the log only; exists_trace and "
          "sequence/trace output need another event_binding");
    std::map<std::string, std::vector<std::string>> objects = options.objects;
    if (objects.empty())
      for (const auto& type : net.object_types) objects[type] = {type + "_0"};
    OcCausalReferencePlayoutOptions reference;
    reference.max_bindings_per_activity = options.max_bindings_per_activity;
    reference.objects_unique_per_sequence = options.objects_unique_per_trace;
    OcpnPlayoutResult result;
    result.log = reference_oc_causal_net_playout(net, objects, reference);
    result.trace_exists = !result.log.events.empty();
    return result;
  }
  if (!has_marker_groups(net))
    return ocpn_extensive_playout_result(net.workflow_projection, options);
  const auto indices = activity_indices(net);
  const auto types = causal_object_types(net, indices);
  return causal_playout(net, indices, types, options);
}

std::string serialize_oc_causal_net_playout_json(const Ocel& log) {
  return serialize_oc_causal_net_playout_json(log,
                                              OcpnEventBinding::kIndependent);
}

std::string serialize_oc_causal_net_playout_json(const Ocel& log,
                                                 OcpnEventBinding binding) {
  return std::string(
             "{\"family\":\"OC_CAUSAL_NET_PLAYOUT\",\"variant\":\"EXTENSIVE\",\"event_binding\":\"") +
         ocpn_event_binding_name(binding) + "\",\"log\":" +
         serialize_ocel_json(log) + "}";
}

std::string serialize_oc_causal_net_playout_json(
    const OcpnPlayoutResult& result, OcpnEventBinding binding) {
  auto json = serialize_ocpn_playout_json(result, binding);
  const auto family = std::string("\"family\":\"OCEL_PLAYOUT\"");
  const auto replacement = std::string("\"family\":\"OC_CAUSAL_NET_PLAYOUT\"");
  const auto position = json.find(family);
  if (position != std::string::npos) json.replace(position, family.size(), replacement);
  return json;
}

}  // namespace minera
