#include "minera/genetic_matrix_conversion.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

using Partition = GeneticMatrixPartition;
using PlaceKey = std::pair<Partition, Partition>;

void normalize_partitions(std::vector<Partition>& partitions,
                          std::size_t activity_count) {
  for (auto& partition : partitions) {
    std::sort(partition.begin(), partition.end());
    partition.erase(std::unique(partition.begin(), partition.end()),
                    partition.end());
    if (partition.empty())
      throw std::invalid_argument(
          "genetic matrix conversion: groups must not be empty");
    if (partition.back() >= activity_count)
      throw std::invalid_argument(
          "genetic matrix conversion: activity index is out of range");
  }
  std::sort(partitions.begin(), partitions.end());
  partitions.erase(std::unique(partitions.begin(), partitions.end()),
                   partitions.end());
}

GeneticCausalMatrix normalize(const GeneticCausalMatrix& source) {
  GeneticCausalMatrix matrix = source;
  const auto activity_count = matrix.activities.size();
  if (matrix.input_map.size() != activity_count ||
      matrix.output_map.size() != activity_count) {
    throw std::invalid_argument(
        "genetic matrix conversion: maps must match the activity count");
  }
  for (auto& groups : matrix.input_map)
    normalize_partitions(groups, activity_count);
  for (auto& groups : matrix.output_map)
    normalize_partitions(groups, activity_count);
  return matrix;
}

std::vector<std::size_t> flatten(
    const std::vector<Partition>& partitions) {
  std::vector<std::size_t> values;
  for (const auto& partition : partitions)
    values.insert(values.end(), partition.begin(), partition.end());
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

bool contains_partition(const std::vector<Partition>& partitions,
                        const Partition& value) {
  return std::binary_search(partitions.begin(), partitions.end(), value);
}

bool partition_subset(const Partition& subset, const Partition& superset) {
  return std::includes(superset.begin(), superset.end(), subset.begin(),
                       subset.end());
}

bool partition_intersects(const Partition& first, const Partition& second) {
  return std::any_of(first.begin(), first.end(), [&](const auto value) {
    return std::binary_search(second.begin(), second.end(), value);
  });
}

std::vector<std::size_t> boundary_activities(
    const GeneticMatrixMap& map) {
  std::vector<std::vector<std::size_t>> components;
  for (std::size_t activity = 0; activity < map.size(); ++activity) {
    auto component = std::find_if(
        components.begin(), components.end(), [&](const auto& values) {
          return std::find(values.begin(), values.end(), activity) !=
                 values.end();
        });
    if (component == components.end()) {
      components.push_back({activity});
      component = std::prev(components.end());
    }
    const auto adjacent = flatten(map[activity]);
    component->insert(component->end(), adjacent.begin(), adjacent.end());
    for (const auto next : adjacent) {
      for (std::size_t other = 0; other < components.size(); ++other) {
        if (&components[other] == &*component || components[other].empty() ||
            components[other].front() != next)
          continue;
        component->insert(component->end(), components[other].begin(),
                          components[other].end());
        components.erase(components.begin() +
                         static_cast<std::ptrdiff_t>(other));
        component = std::find_if(
            components.begin(), components.end(), [&](const auto& values) {
              return std::find(values.begin(), values.end(), activity) !=
                     values.end();
            });
        break;
      }
    }
  }
  std::vector<std::size_t> result;
  result.reserve(components.size());
  for (const auto& component : components)
    if (!component.empty()) result.push_back(component.front());
  return result;
}

PetriNet build_net(const GeneticCausalMatrix& matrix) {
  const auto activity_count = matrix.activities.size();
  PetriNet net;
  net.set_activities(matrix.activities);
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  std::vector<Partition> all_inputs;
  std::vector<Partition> all_outputs;
  for (const auto& groups : matrix.input_map)
    all_inputs.insert(all_inputs.end(), groups.begin(), groups.end());
  for (const auto& groups : matrix.output_map)
    all_outputs.insert(all_outputs.end(), groups.begin(), groups.end());
  std::sort(all_inputs.begin(), all_inputs.end());
  all_inputs.erase(std::unique(all_inputs.begin(), all_inputs.end()),
                   all_inputs.end());
  std::sort(all_outputs.begin(), all_outputs.end());
  all_outputs.erase(std::unique(all_outputs.begin(), all_outputs.end()),
                    all_outputs.end());

  std::vector<PlaceKey> complete_places;
  for (const auto& predecessors : all_inputs) {
    for (const auto& successors : all_outputs) {
      const bool output_agreement = std::all_of(
          predecessors.begin(), predecessors.end(),
          [&](const auto predecessor) {
            return contains_partition(matrix.output_map[predecessor],
                                      successors);
          });
      const bool input_agreement = output_agreement && std::all_of(
          successors.begin(), successors.end(), [&](const auto successor) {
            return contains_partition(matrix.input_map[successor],
                                      predecessors);
          });
      if (input_agreement)
        complete_places.emplace_back(predecessors, successors);
    }
  }
  std::sort(complete_places.begin(), complete_places.end());
  complete_places.erase(
      std::unique(complete_places.begin(), complete_places.end()),
      complete_places.end());

  std::vector<PlaceKey> routed_places;
  for (std::size_t activity = 0; activity < activity_count; ++activity) {
    for (const auto& predecessors : matrix.input_map[activity])
      routed_places.emplace_back(predecessors, Partition{activity});
    for (const auto& successors : matrix.output_map[activity])
      routed_places.emplace_back(Partition{activity}, successors);
  }
  std::sort(routed_places.begin(), routed_places.end());
  routed_places.erase(
      std::unique(routed_places.begin(), routed_places.end()),
      routed_places.end());
  routed_places.erase(
      std::remove_if(
          routed_places.begin(), routed_places.end(),
          [&](const PlaceKey& edge) {
            return std::any_of(
                complete_places.begin(), complete_places.end(),
                [&](const PlaceKey& place) {
                  return partition_subset(edge.first, place.first) &&
                         partition_subset(edge.second, place.second);
                });
          }),
      routed_places.end());

  std::vector<PlaceKey> pending = complete_places;
  complete_places.clear();
  while (!pending.empty()) {
    auto candidate = std::move(pending.back());
    pending.pop_back();
    const bool conflicts = std::any_of(
        routed_places.begin(), routed_places.end(),
        [&](const PlaceKey& edge) {
          return partition_intersects(candidate.first, edge.first) ||
                 partition_intersects(candidate.second, edge.second);
        });
    if (!conflicts) {
      complete_places.push_back(std::move(candidate));
      continue;
    }
    routed_places.push_back(candidate);
    for (const auto& accepted : complete_places) {
      if (partition_intersects(accepted.first, candidate.first) ||
          partition_intersects(accepted.second, candidate.second))
        pending.push_back(accepted);
    }
    complete_places.erase(
        std::remove_if(
            complete_places.begin(), complete_places.end(),
            [&](const PlaceKey& accepted) {
              return partition_intersects(accepted.first, candidate.first) ||
                     partition_intersects(accepted.second, candidate.second);
            }),
        complete_places.end());
  }
  std::sort(complete_places.begin(), complete_places.end());
  complete_places.erase(
      std::unique(complete_places.begin(), complete_places.end()),
      complete_places.end());
  std::sort(routed_places.begin(), routed_places.end());
  routed_places.erase(
      std::unique(routed_places.begin(), routed_places.end()),
      routed_places.end());

  std::map<PlaceKey, std::size_t> places;
  for (const auto& key : complete_places)
    places.emplace(key, net.add_place());

  std::vector<std::size_t> visible(activity_count);
  for (std::size_t activity = 0; activity < activity_count; ++activity)
    visible[activity] = net.add_transition(activity, {}, {});

  const auto starts = boundary_activities(matrix.output_map);
  const auto ends = boundary_activities(matrix.input_map);
  const bool cyclic_boundary =
      activity_count > 1 && starts.size() == 1 && ends.size() == 1 &&
      starts.front() == ends.front();
  std::vector<std::size_t> cycle_entry_inputs;
  std::vector<std::size_t> cycle_exit_outputs;
  if (!cyclic_boundary) {
    for (const auto activity : starts)
      net.add_arc(visible[activity], source);
    for (const auto activity : ends)
      net.add_output_arc(visible[activity], sink);
  }

  for (const auto& [key, place] : places) {
    for (const auto activity : key.first) {
      if (cyclic_boundary && activity == activity_count - 1)
        cycle_exit_outputs.push_back(place);
      else
        net.add_output_arc(visible[activity], place);
    }
    for (const auto activity : key.second) {
      if (cyclic_boundary && activity == 0)
        cycle_entry_inputs.push_back(place);
      else
        net.add_arc(visible[activity], place);
    }
  }

  std::map<std::pair<std::size_t, std::size_t>, std::size_t>
      silent_transitions;
  const auto silent_transition = [&](std::size_t from, std::size_t to) {
    const auto key = std::pair{from, to};
    const auto found = silent_transitions.find(key);
    if (found != silent_transitions.end()) return found->second;
    const auto created = net.add_transition(std::nullopt, {}, {});
    silent_transitions.emplace(key, created);
    return created;
  };
  std::set<std::size_t> routed_inputs;
  std::set<std::size_t> routed_outputs;
  for (const auto& edge : routed_places) {
    routed_inputs.insert(edge.first.begin(), edge.first.end());
    routed_outputs.insert(edge.second.begin(), edge.second.end());
  }
  for (const auto from : routed_inputs) {
    for (const auto& successors : matrix.output_map[from]) {
      const auto place = net.add_place();
      if (cyclic_boundary && from == activity_count - 1)
        cycle_exit_outputs.push_back(place);
      else
        net.add_output_arc(visible[from], place);
      for (const auto to : successors)
        net.add_arc(silent_transition(from, to), place);
    }
  }
  for (const auto to : routed_outputs) {
    for (const auto& predecessors : matrix.input_map[to]) {
      const auto place = net.add_place();
      if (cyclic_boundary && to == 0)
        cycle_entry_inputs.push_back(place);
      else
        net.add_arc(visible[to], place);
      for (const auto from : predecessors)
        net.add_output_arc(silent_transition(from, to), place);
    }
  }

  if (cyclic_boundary) {
    const auto entry_hub = net.add_place();
    net.add_arc(visible.front(), entry_hub);
    net.add_transition(std::nullopt, {source}, {entry_hub});
    if (!cycle_entry_inputs.empty())
      net.add_transition(std::nullopt, std::move(cycle_entry_inputs),
                         {entry_hub});

    const auto exit_hub = net.add_place();
    net.add_output_arc(visible.back(), exit_hub);
    if (!cycle_exit_outputs.empty())
      net.add_transition(std::nullopt, {exit_hub},
                         std::move(cycle_exit_outputs));
    net.add_transition(std::nullopt, {exit_hub}, {sink});
  }
  return net;
}

std::string json_escape(std::string_view value) {
  std::string escaped;
  for (const unsigned char character : value) {
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (character < 0x20) {
          static constexpr char hex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped.push_back(hex[character >> 4]);
          escaped.push_back(hex[character & 0x0f]);
        } else {
          escaped.push_back(static_cast<char>(character));
        }
    }
  }
  return escaped;
}

void write_partition_map(std::ostream& output,
                         const GeneticMatrixMap& map) {
  output << '[';
  for (std::size_t activity = 0; activity < map.size(); ++activity) {
    if (activity != 0) output << ',';
    output << '[';
    for (std::size_t group = 0; group < map[activity].size(); ++group) {
      if (group != 0) output << ',';
      output << '[';
      for (std::size_t member = 0; member < map[activity][group].size();
           ++member) {
        if (member != 0) output << ',';
        output << map[activity][group][member];
      }
      output << ']';
    }
    output << ']';
  }
  output << ']';
}

void write_marking(std::ostream& output, const Marking& marking) {
  output << '[';
  for (std::size_t i = 0; i < marking.size(); ++i) {
    if (i != 0) output << ',';
    output << marking[i];
  }
  output << ']';
}

}  // namespace

GeneticMatrixConversionVariant parse_genetic_matrix_conversion_variant(
    std::string_view value) {
  if (value == "to_petri_net" || value == "TO_PETRI_NET" ||
      value == "petri_net")
    return GeneticMatrixConversionVariant::kToPetriNet;
  throw std::invalid_argument(
      "genetic matrix conversion variant: expected to_petri_net");
}

std::string_view genetic_matrix_conversion_variant_name(
    GeneticMatrixConversionVariant variant) {
  switch (variant) {
    case GeneticMatrixConversionVariant::kToPetriNet:
      return "TO_PETRI_NET";
  }
  throw std::invalid_argument(
      "genetic matrix conversion: unsupported variant");
}

GeneticMatrixConversionResult convert_genetic_matrix(
    const GeneticCausalMatrix& source,
    GeneticMatrixConversionVariant variant) {
  (void)genetic_matrix_conversion_variant_name(variant);
  GeneticMatrixConversionResult result;
  result.variant = variant;
  result.matrix = normalize(source);
  result.net = build_net(result.matrix);
  result.markings = default_workflow_markings(result.net);
  return result;
}

std::string genetic_matrix_conversion_json(
    const GeneticMatrixConversionResult& result) {
  const auto normalized = normalize(result.matrix);
  const auto variant = genetic_matrix_conversion_variant_name(result.variant);
  if (!result.net.has_source() || !result.net.has_sink() ||
      result.net.source() >= result.net.place_count() ||
      result.net.sink() >= result.net.place_count() ||
      result.markings.initial.size() != result.net.place_count() ||
      result.markings.final.size() != result.net.place_count()) {
    throw std::invalid_argument(
        "genetic matrix conversion: result model is invalid");
  }

  std::ostringstream output;
  output << "{\"family\":\"GENETIC_MATRIX_CONVERSION\",\"variant\":\""
         << variant << "\",\"matrix\":{\"activities\":[";
  for (std::size_t i = 0; i < normalized.activities.size(); ++i) {
    if (i != 0) output << ',';
    output << '\"' << json_escape(normalized.activities[i]) << '\"';
  }
  output << "],\"input_map\":";
  write_partition_map(output, normalized.input_map);
  output << ",\"output_map\":";
  write_partition_map(output, normalized.output_map);
  output << "},\"net\":{\"place_count\":" << result.net.place_count()
         << ",\"source\":" << result.net.source() << ",\"sink\":"
         << result.net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < result.net.activities().size(); ++i) {
    if (i != 0) output << ',';
    output << '\"' << json_escape(result.net.activities()[i]) << '\"';
  }
  output << "],\"transitions\":[";
  for (std::size_t i = 0; i < result.net.transitions().size(); ++i) {
    if (i != 0) output << ',';
    const auto& transition = result.net.transitions()[i];
    output << "{\"activity\":";
    if (transition.activity)
      output << *transition.activity;
    else
      output << "null";
    output << ",\"inputs\":[";
    for (std::size_t j = 0; j < transition.inputs.size(); ++j) {
      if (j != 0) output << ',';
      output << transition.inputs[j];
    }
    output << "],\"outputs\":[";
    for (std::size_t j = 0; j < transition.outputs.size(); ++j) {
      if (j != 0) output << ',';
      output << transition.outputs[j];
    }
    output << "]}";
  }
  output << "]},\"markings\":{\"initial\":";
  write_marking(output, result.markings.initial);
  output << ",\"final\":";
  write_marking(output, result.markings.final);
  output << "}}";
  return output.str();
}

}  // namespace minera
