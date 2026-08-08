#include "minera/ocel_filter.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

using IdSet = std::unordered_set<std::string>;

IdSet ids_of_events(const Ocel& log) {
  IdSet ids;
  ids.reserve(log.events.size());
  for (const auto& event : log.events) ids.insert(event.id);
  return ids;
}

IdSet ids_of_objects(const Ocel& log) {
  IdSet ids;
  ids.reserve(log.objects.size());
  for (const auto& object : log.objects) ids.insert(object.id);
  return ids;
}

bool contains_value(const std::vector<OcelValue>& values,
                    const OcelValue& candidate) {
  return std::find(values.begin(), values.end(), candidate) != values.end();
}

bool has_attribute_value(const std::vector<OcelAttribute>& attributes,
                         std::string_view key,
                         const std::vector<OcelValue>& values) {
  for (const auto& attribute : attributes) {
    if (attribute.key == key && contains_value(values, attribute.value)) {
      return true;
    }
  }
  return false;
}

void clear_all_tables(Ocel& log) {
  log.events.clear();
  log.objects.clear();
  log.relations.clear();
  log.o2o.clear();
  log.object_changes.clear();
}

Ocel propagate_event_filter(Ocel log) {
  const auto selected_events = ids_of_events(log);
  if (selected_events.empty()) {
    clear_all_tables(log);
    return log;
  }

  log.relations.erase(
      std::remove_if(log.relations.begin(), log.relations.end(),
                     [&](const OcelRelation& relation) {
                       return !selected_events.count(relation.event_id);
                     }),
      log.relations.end());

  IdSet selected_objects;
  for (const auto& relation : log.relations) {
    selected_objects.insert(relation.object_id);
  }
  const auto existing_objects = ids_of_objects(log);
  for (auto it = selected_objects.begin(); it != selected_objects.end();) {
    if (!existing_objects.count(*it)) {
      it = selected_objects.erase(it);
    } else {
      ++it;
    }
  }

  log.relations.erase(
      std::remove_if(log.relations.begin(), log.relations.end(),
                     [&](const OcelRelation& relation) {
                       return !selected_objects.count(relation.object_id);
                     }),
      log.relations.end());
  log.objects.erase(
      std::remove_if(log.objects.begin(), log.objects.end(),
                     [&](const OcelObject& object) {
                       return !selected_objects.count(object.id);
                     }),
      log.objects.end());
  log.object_changes.erase(
      std::remove_if(log.object_changes.begin(), log.object_changes.end(),
                     [&](const OcelObjectChange& change) {
                       return !selected_objects.count(change.object_id);
                     }),
      log.object_changes.end());
  log.o2o.erase(
      std::remove_if(log.o2o.begin(), log.o2o.end(),
                     [&](const OcelO2oRelation& relation) {
                       return !selected_objects.count(relation.source_id) ||
                              !selected_objects.count(relation.target_id);
                     }),
      log.o2o.end());
  return log;
}

Ocel propagate_object_filter(Ocel log) {
  const auto selected_objects = ids_of_objects(log);
  if (selected_objects.empty()) {
    clear_all_tables(log);
    return log;
  }

  log.relations.erase(
      std::remove_if(log.relations.begin(), log.relations.end(),
                     [&](const OcelRelation& relation) {
                       return !selected_objects.count(relation.object_id);
                     }),
      log.relations.end());

  IdSet selected_events;
  for (const auto& relation : log.relations) {
    selected_events.insert(relation.event_id);
  }
  const auto existing_events = ids_of_events(log);
  for (auto it = selected_events.begin(); it != selected_events.end();) {
    if (!existing_events.count(*it)) {
      it = selected_events.erase(it);
    } else {
      ++it;
    }
  }
  log.relations.erase(
      std::remove_if(log.relations.begin(), log.relations.end(),
                     [&](const OcelRelation& relation) {
                       return !selected_events.count(relation.event_id);
                     }),
      log.relations.end());
  log.events.erase(
      std::remove_if(log.events.begin(), log.events.end(),
                     [&](const OcelEvent& event) {
                       return !selected_events.count(event.id);
                     }),
      log.events.end());
  log.o2o.erase(
      std::remove_if(log.o2o.begin(), log.o2o.end(),
                     [&](const OcelO2oRelation& relation) {
                       return !selected_objects.count(relation.source_id) ||
                              !selected_objects.count(relation.target_id);
                     }),
      log.o2o.end());
  log.object_changes.erase(
      std::remove_if(log.object_changes.begin(), log.object_changes.end(),
                     [&](const OcelObjectChange& change) {
                       return !selected_objects.count(change.object_id);
                     }),
      log.object_changes.end());
  return log;
}

Ocel propagate_relation_filter(Ocel log) {
  IdSet selected_events;
  IdSet selected_objects;
  for (const auto& relation : log.relations) {
    selected_events.insert(relation.event_id);
    selected_objects.insert(relation.object_id);
  }
  if (selected_events.empty() || selected_objects.empty()) {
    clear_all_tables(log);
    return log;
  }

  const auto existing_events = ids_of_events(log);
  const auto existing_objects = ids_of_objects(log);
  for (auto it = selected_events.begin(); it != selected_events.end();) {
    if (!existing_events.count(*it)) {
      it = selected_events.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = selected_objects.begin(); it != selected_objects.end();) {
    if (!existing_objects.count(*it)) {
      it = selected_objects.erase(it);
    } else {
      ++it;
    }
  }
  if (selected_events.empty() || selected_objects.empty()) {
    clear_all_tables(log);
    return log;
  }

  log.events.erase(
      std::remove_if(log.events.begin(), log.events.end(),
                     [&](const OcelEvent& event) {
                       return !selected_events.count(event.id);
                     }),
      log.events.end());
  log.objects.erase(
      std::remove_if(log.objects.begin(), log.objects.end(),
                     [&](const OcelObject& object) {
                       return !selected_objects.count(object.id);
                     }),
      log.objects.end());
  log.relations.erase(
      std::remove_if(log.relations.begin(), log.relations.end(),
                     [&](const OcelRelation& relation) {
                       return !selected_events.count(relation.event_id) ||
                              !selected_objects.count(relation.object_id);
                     }),
      log.relations.end());
  log.o2o.erase(
      std::remove_if(log.o2o.begin(), log.o2o.end(),
                     [&](const OcelO2oRelation& relation) {
                       return !selected_objects.count(relation.source_id) ||
                              !selected_objects.count(relation.target_id);
                     }),
      log.o2o.end());
  log.object_changes.erase(
      std::remove_if(log.object_changes.begin(), log.object_changes.end(),
                     [&](const OcelObjectChange& change) {
                       return !selected_objects.count(change.object_id);
                     }),
      log.object_changes.end());
  return log;
}

template <typename Record, typename Predicate>
void filter_records(std::vector<Record>& records, Predicate&& keep) {
  records.erase(std::remove_if(records.begin(), records.end(),
                               [&](const Record& record) {
                                 return !keep(record);
                               }),
                records.end());
}

IdSet string_set(const std::vector<std::string>& values) {
  return {values.begin(), values.end()};
}

std::map<std::string, std::string> object_types_by_id(const Ocel& log) {
  std::map<std::string, std::string> result;
  for (const auto& object : log.objects) result.emplace(object.id, object.type);
  return result;
}

std::map<std::string, std::set<std::string>> objects_by_event(
    const Ocel& log, const std::map<std::string, std::string>& object_types) {
  std::map<std::string, std::set<std::string>> result;
  for (const auto& relation : log.relations) {
    if (object_types.contains(relation.object_id)) {
      result[relation.event_id].insert(relation.object_id);
    }
  }
  return result;
}

std::vector<IdSet> object_interaction_components(const Ocel& log) {
  const auto object_types = object_types_by_id(log);
  const auto related = objects_by_event(log, object_types);
  std::map<std::string, std::set<std::string>> adjacency;
  for (const auto& [event_id, objects] : related) {
    (void)event_id;
    for (auto left = objects.begin(); left != objects.end(); ++left) {
      auto right = left;
      ++right;
      for (; right != objects.end(); ++right) {
        adjacency[*left].insert(*right);
        adjacency[*right].insert(*left);
      }
    }
  }

  std::vector<IdSet> result;
  IdSet seen;
  for (const auto& [start, neighbours] : adjacency) {
    (void)neighbours;
    if (seen.count(start)) continue;
    IdSet component;
    std::vector<std::string> stack{start};
    seen.insert(start);
    while (!stack.empty()) {
      const auto current = stack.back();
      stack.pop_back();
      component.insert(current);
      const auto adjacency_it = adjacency.find(current);
      if (adjacency_it == adjacency.end()) continue;
      for (const auto& next : adjacency_it->second) {
        if (seen.insert(next).second) stack.push_back(next);
      }
    }
    if (!component.empty()) result.push_back(std::move(component));
  }
  return result;
}

IdSet expand_object_ids(const Ocel& log, IdSet selected, std::size_t level) {
  if (level <= 1 || selected.empty()) return selected;
  const auto object_types = object_types_by_id(log);
  const auto related = objects_by_event(log, object_types);
  std::map<std::string, std::set<std::string>> adjacency;
  for (const auto& [event_id, objects] : related) {
    (void)event_id;
    for (const auto& left : objects) {
      auto& neighbours = adjacency[left];
      neighbours.insert(objects.begin(), objects.end());
      neighbours.erase(left);
    }
  }
  for (std::size_t depth = 1; depth < level; ++depth) {
    const IdSet frontier = selected;
    for (const auto& object_id : frontier) {
      const auto adjacency_it = adjacency.find(object_id);
      if (adjacency_it == adjacency.end()) continue;
      selected.insert(adjacency_it->second.begin(), adjacency_it->second.end());
    }
  }
  return selected;
}

Ocel filter_object_id_set(const Ocel& log, const IdSet& selected) {
  return filter_ocel_objects(
      log, std::vector<std::string>(selected.begin(), selected.end()), true, 1);
}

}  // namespace

Ocel filter_ocel_event_attribute(const Ocel& log, std::string_view attribute_key,
                                 const std::vector<OcelValue>& values,
                                 bool positive) {
  if (attribute_key.empty()) {
    throw std::invalid_argument("OCEL event attribute key must not be empty");
  }
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    const bool matches =
        has_attribute_value(event.attributes, attribute_key, values);
    return positive ? matches : !matches;
  });
  return propagate_event_filter(std::move(filtered));
}

Ocel filter_ocel_object_attribute(const Ocel& log,
                                  std::string_view attribute_key,
                                  const std::vector<OcelValue>& values,
                                  bool positive) {
  if (attribute_key.empty()) {
    throw std::invalid_argument("OCEL object attribute key must not be empty");
  }
  Ocel filtered = log;
  filter_records(filtered.objects, [&](const OcelObject& object) {
    const bool matches =
        has_attribute_value(object.attributes, attribute_key, values);
    return positive ? matches : !matches;
  });
  return propagate_object_filter(std::move(filtered));
}

Ocel filter_ocel_object_types(const Ocel& log,
                              const std::vector<std::string>& types,
                              bool positive, std::size_t level) {
  const auto selected = string_set(types);
  if (level > 1) {
    IdSet object_ids;
    for (const auto& object : log.objects) {
      if (selected.count(object.type)) object_ids.insert(object.id);
    }
    return filter_ocel_objects(
        log, std::vector<std::string>(object_ids.begin(), object_ids.end()),
        positive, level);
  }
  Ocel filtered = log;
  filter_records(filtered.objects, [&](const OcelObject& object) {
    const bool matches = selected.count(object.type) != 0;
    return positive ? matches : !matches;
  });
  return propagate_object_filter(std::move(filtered));
}

Ocel filter_ocel_objects(const Ocel& log,
                         const std::vector<std::string>& object_ids,
                         bool positive, std::size_t level) {
  auto selected = expand_object_ids(log, string_set(object_ids), level);
  Ocel filtered = log;
  filter_records(filtered.objects, [&](const OcelObject& object) {
    const bool matches = selected.count(object.id) != 0;
    return positive ? matches : !matches;
  });
  return propagate_object_filter(std::move(filtered));
}

Ocel filter_ocel_events(const Ocel& log,
                        const std::vector<std::string>& event_ids,
                        bool positive) {
  const auto selected = string_set(event_ids);
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    const bool matches = selected.count(event.id) != 0;
    return positive ? matches : !matches;
  });
  return propagate_event_filter(std::move(filtered));
}

Ocel filter_ocel_events_timestamp(const Ocel& log, std::string_view from,
                                  std::string_view to) {
  const auto parse_bound = [](std::string_view text, double fallback) {
    if (text.empty()) return fallback;
    const auto parsed = parse_timestamp(text);
    if (!parsed) throw std::invalid_argument("invalid OCEL timestamp bound");
    return *parsed;
  };
  const double lower =
      parse_bound(from, -std::numeric_limits<double>::infinity());
  const double upper =
      parse_bound(to, std::numeric_limits<double>::infinity());
  if (lower > upper) {
    throw std::invalid_argument("OCEL timestamp bounds are reversed");
  }

  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    const auto instant = parse_timestamp(event.timestamp);
    return instant && *instant >= lower && *instant <= upper;
  });
  return propagate_event_filter(std::move(filtered));
}

Ocel filter_ocel_activity_type_matching(
    const Ocel& log,
    const std::map<std::string, std::vector<std::string>>& allowed) {
  std::unordered_map<std::string, std::string> event_activities;
  for (const auto& event : log.events) {
    event_activities.emplace(event.id, event.activity);
  }
  std::unordered_map<std::string, std::string> object_types;
  for (const auto& object : log.objects) {
    object_types.emplace(object.id, object.type);
  }

  Ocel filtered = log;
  filter_records(filtered.relations, [&](const OcelRelation& relation) {
    const auto event_it = event_activities.find(relation.event_id);
    const auto object_it = object_types.find(relation.object_id);
    if (event_it == event_activities.end() || object_it == object_types.end()) {
      return false;
    }
    const auto allowed_it = allowed.find(object_it->second);
    if (allowed_it == allowed.end()) return false;
    return std::find(allowed_it->second.begin(), allowed_it->second.end(),
                     event_it->second) != allowed_it->second.end();
  });
  return propagate_relation_filter(std::move(filtered));
}

std::map<std::string, std::map<std::string, std::size_t>>
ocel_event_object_type_counts(const Ocel& log) {
  std::unordered_map<std::string, std::string> object_types;
  object_types.reserve(log.objects.size());
  for (const auto& object : log.objects) {
    object_types.emplace(object.id, object.type);
  }

  std::map<std::string, std::map<std::string, std::size_t>> result;
  for (const auto& relation : log.relations) {
    const auto object_it = object_types.find(relation.object_id);
    if (object_it == object_types.end()) continue;
    ++result[relation.event_id][object_it->second];
  }
  return result;
}

Ocel filter_ocel_events_object_type_counts(
    const Ocel& log,
    const std::map<std::string, std::size_t>& minimum_counts) {
  const auto counts = ocel_event_object_type_counts(log);
  IdSet selected_events;
  for (const auto& [event_id, by_type] : counts) {
    bool keep = true;
    for (const auto& [object_type, minimum] : minimum_counts) {
      const auto type_it = by_type.find(object_type);
      if (type_it == by_type.end() || type_it->second < minimum) {
        keep = false;
        break;
      }
    }
    if (keep) selected_events.insert(event_id);
  }

  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    return selected_events.count(event.id) != 0;
  });
  return propagate_event_filter(std::move(filtered));
}

namespace {

Ocel filter_ocel_lifecycle_events_per_object_type(const Ocel& log,
                                                   std::string_view object_type,
                                                   bool first) {
  std::unordered_map<std::string, std::string> object_types;
  object_types.reserve(log.objects.size());
  for (const auto& object : log.objects) {
    object_types.emplace(object.id, object.type);
  }

  std::unordered_map<std::string, std::string> selected_by_object;
  for (const auto& relation : log.relations) {
    const auto type_it = object_types.find(relation.object_id);
    if (type_it == object_types.end() || type_it->second != object_type) {
      continue;
    }
    if (first) {
      selected_by_object.emplace(relation.object_id, relation.event_id);
    } else {
      selected_by_object[relation.object_id] = relation.event_id;
    }
  }

  IdSet selected_events;
  for (const auto& [object_id, event_id] : selected_by_object) {
    (void)object_id;
    selected_events.insert(event_id);
  }
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    return selected_events.count(event.id) != 0;
  });
  return propagate_event_filter(std::move(filtered));
}

}  // namespace

Ocel filter_ocel_start_events_per_object_type(const Ocel& log,
                                              std::string_view object_type) {
  return filter_ocel_lifecycle_events_per_object_type(log, object_type, true);
}

Ocel filter_ocel_end_events_per_object_type(const Ocel& log,
                                            std::string_view object_type) {
  return filter_ocel_lifecycle_events_per_object_type(log, object_type, false);
}

Ocel filter_ocel_activities_connected_object_type(
    const Ocel& log, std::string_view object_type) {
  const auto object_types = object_types_by_id(log);
  std::unordered_map<std::string, std::string> event_activities;
  event_activities.reserve(log.events.size());
  for (const auto& event : log.events) {
    event_activities.emplace(event.id, event.activity);
  }

  std::unordered_set<std::string> activities;
  for (const auto& relation : log.relations) {
    const auto object_it = object_types.find(relation.object_id);
    const auto event_it = event_activities.find(relation.event_id);
    if (object_it != object_types.end() && event_it != event_activities.end() &&
        object_it->second == object_type) {
      activities.insert(event_it->second);
    }
  }

  Ocel filtered = log;
  filter_records(filtered.relations, [&](const OcelRelation& relation) {
    const auto event_it = event_activities.find(relation.event_id);
    return event_it != event_activities.end() &&
           activities.count(event_it->second) != 0;
  });
  return propagate_relation_filter(std::move(filtered));
}

Ocel filter_ocel_cc_object(const Ocel& log, std::string_view object_id) {
  IdSet selected{std::string(object_id)};
  for (const auto& component : object_interaction_components(log)) {
    if (component.count(std::string(object_id)) != 0) {
      selected = component;
      break;
    }
  }
  return filter_object_id_set(log, selected);
}

Ocel filter_ocel_cc_length(const Ocel& log, std::size_t minimum_size,
                           std::size_t maximum_size) {
  if (minimum_size > maximum_size) {
    throw std::invalid_argument(
        "OCEL connected-component size bounds are reversed");
  }
  IdSet selected;
  for (const auto& component : object_interaction_components(log)) {
    if (component.size() >= minimum_size && component.size() <= maximum_size) {
      selected.insert(component.begin(), component.end());
    }
  }
  return filter_object_id_set(log, selected);
}

Ocel filter_ocel_cc_object_type(const Ocel& log, std::string_view object_type,
                                bool positive) {
  const auto object_types = object_types_by_id(log);
  IdSet seeds;
  for (const auto& [object_id, type] : object_types) {
    const bool matches = type == object_type;
    if (positive ? matches : !matches) seeds.insert(object_id);
  }

  IdSet selected;
  for (const auto& component : object_interaction_components(log)) {
    bool intersects = false;
    for (const auto& object_id : component) {
      if (seeds.count(object_id) != 0) {
        intersects = true;
        break;
      }
    }
    if (intersects) selected.insert(component.begin(), component.end());
  }
  return filter_object_id_set(log, selected);
}

Ocel filter_ocel_cc_activity(const Ocel& log, std::string_view activity) {
  std::unordered_map<std::string, std::string> event_activities;
  event_activities.reserve(log.events.size());
  for (const auto& event : log.events) {
    event_activities.emplace(event.id, event.activity);
  }
  IdSet seeds;
  for (const auto& relation : log.relations) {
    const auto event_it = event_activities.find(relation.event_id);
    if (event_it != event_activities.end() && event_it->second == activity) {
      seeds.insert(relation.object_id);
    }
  }

  IdSet selected;
  for (const auto& component : object_interaction_components(log)) {
    bool intersects = false;
    for (const auto& object_id : component) {
      if (seeds.count(object_id) != 0) {
        intersects = true;
        break;
      }
    }
    if (intersects) selected.insert(component.begin(), component.end());
  }
  return filter_object_id_set(log, selected);
}

}  // namespace minera
