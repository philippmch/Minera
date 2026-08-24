#include "minera/ocel_features.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

using IndexMap = std::map<std::string, std::size_t>;

struct Column {
  std::string name;
  std::vector<double> values;
};

struct Lifecycle {
  std::vector<std::size_t> event_indices;
  std::vector<std::string> activities;
  std::vector<double> timestamps;
  std::set<std::string> unique_activities;
  std::map<std::pair<std::string, std::string>, std::size_t> paths;
  std::optional<double> start;
  std::optional<double> end;
};

const char* variant_name(OcelFeatureVariant variant) noexcept {
  switch (variant) {
    case OcelFeatureVariant::kEvents: return "EVENTS";
    case OcelFeatureVariant::kObjects: return "OBJECTS";
    case OcelFeatureVariant::kEventsObjects: return "EVENTS_OBJECTS";
  }
  return "EVENTS";
}

void json_string(std::ostringstream& out, std::string_view value) {
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
          const char* hex = "0123456789abcdef";
          out << "\\u00" << hex[c >> 4] << hex[c & 15];
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

std::optional<double> numeric_value(const OcelValue& value) {
  if (const auto* integer = std::get_if<std::int64_t>(&value))
    return static_cast<double>(*integer);
  if (const auto* real = std::get_if<double>(&value))
    return std::isfinite(*real) ? std::optional<double>(*real) : std::nullopt;
  if (const auto* boolean = std::get_if<bool>(&value))
    return *boolean ? 1.0 : 0.0;
  return std::nullopt;
}

std::string string_value(const OcelValue& value) {
  if (const auto* text = std::get_if<std::string>(&value)) return *text;
  return {};
}

const OcelAttribute* find_attribute(const std::vector<OcelAttribute>& attrs,
                                    std::string_view key) {
  for (const auto& attr : attrs)
    if (attr.key == key) return &attr;
  return nullptr;
}

std::vector<std::string> sorted_unique(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

std::vector<std::string> activity_names(const Ocel& log) {
  std::vector<std::string> values;
  values.reserve(log.events.size());
  for (const auto& event : log.events) values.push_back(event.activity);
  return sorted_unique(std::move(values));
}

std::vector<std::string> object_type_names(const Ocel& log) {
  std::vector<std::string> values;
  values.reserve(log.objects.size());
  for (const auto& object : log.objects) values.push_back(object.type);
  return sorted_unique(std::move(values));
}

std::map<std::string, std::vector<std::size_t>> event_relations(
    const Ocel& log, const IndexMap& event_index, const IndexMap& object_index,
    std::vector<std::vector<std::size_t>>& event_objects,
    std::vector<std::vector<std::size_t>>& object_events) {
  std::map<std::string, std::vector<std::size_t>> result;
  event_objects.assign(log.events.size(), {});
  object_events.assign(log.objects.size(), {});
  for (const auto& relation : log.relations) {
    const auto event_it = event_index.find(relation.event_id);
    const auto object_it = object_index.find(relation.object_id);
    if (event_it == event_index.end() || object_it == object_index.end())
      throw std::invalid_argument("OCEL feature relation references an unknown id");
    event_objects[event_it->second].push_back(object_it->second);
    object_events[object_it->second].push_back(event_it->second);
    result[relation.event_id].push_back(object_it->second);
  }
  return result;
}

std::vector<std::optional<double>> event_times(const Ocel& log) {
  std::vector<std::optional<double>> times;
  times.reserve(log.events.size());
  for (const auto& event : log.events) times.push_back(parse_timestamp(event.timestamp));
  return times;
}

std::array<double, 5> calendar_features(double seconds) {
  std::tm calendar{};
  const auto epoch = static_cast<std::time_t>(std::floor(seconds));
#if defined(_WIN32)
  if (gmtime_s(&calendar, &epoch) != 0) {
    return {seconds, 0.0, 0.0, 0.0, 0.0};
  }
#else
  if (gmtime_r(&epoch, &calendar) == nullptr) {
    return {seconds, 0.0, 0.0, 0.0, 0.0};
  }
#endif
  // The table uses Monday as weekday zero, while tm_wday uses Sunday zero.
  const auto weekday = (calendar.tm_wday + 6) % 7;
  return {seconds, static_cast<double>(weekday),
          static_cast<double>(calendar.tm_hour),
          static_cast<double>(calendar.tm_mon + 1),
          static_cast<double>(calendar.tm_mday)};
}

class TableBuilder {
 public:
  TableBuilder(std::size_t rows, std::size_t max_features)
      : rows_(rows), max_features_(max_features) {}

  void add(std::string name, std::vector<double> values) {
    if (values.size() != rows_)
      throw std::invalid_argument("OCEL feature column has the wrong row count");
    if (name.empty()) throw std::invalid_argument("OCEL feature name is empty");
    if (columns_.size() >= max_features_)
      throw std::invalid_argument("OCEL feature count exceeds max_features");
    columns_.push_back({std::move(name), std::move(values)});
  }

  OcelFeatureTable finish(OcelFeatureVariant variant,
                          std::vector<std::string> row_ids,
                          std::vector<std::string> row_types) && {
    OcelFeatureTable table;
    table.variant = variant;
    table.row_ids = std::move(row_ids);
    table.row_types = std::move(row_types);
    table.feature_names.reserve(columns_.size());
    for (const auto& column : columns_)
      table.feature_names.push_back(column.name);
    table.values.assign(rows_, std::vector<double>(columns_.size(), 0.0));
    for (std::size_t column = 0; column < columns_.size(); ++column)
      for (std::size_t row = 0; row < rows_; ++row) {
        const auto value = columns_[column].values[row];
        table.values[row][column] = std::isfinite(value) ? value : 0.0;
      }
    return table;
  }

 private:
  std::size_t rows_;
  std::size_t max_features_;
  std::vector<Column> columns_;
};

void add_event_columns(TableBuilder& builder, const Ocel& log,
                       const OcelFeatureOptions& options,
                       const std::vector<std::string>& activities,
                       const std::vector<std::string>& object_types,
                       const std::vector<std::vector<std::size_t>>& event_objects,
                       const std::vector<std::optional<double>>& times,
                       const std::vector<std::string>& object_types_by_index) {
  if (options.include_activity_features) {
    for (const auto& activity : activities) {
      std::vector<double> values(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row)
        values[row] = log.events[row].activity == activity ? 1.0 : 0.0;
      builder.add("@@event_act_" + activity, std::move(values));
    }
  }
  if (options.include_timestamp_features) {
    const std::array<std::string, 5> names = {
        "@@event_timestamp", "@@event_timestamp_dayofweek",
        "@@event_timestamp_hour", "@@event_timestamp_month",
        "@@event_timestamp_day"};
    for (std::size_t field = 0; field < names.size(); ++field) {
      std::vector<double> values(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row)
        if (times[row]) values[row] = calendar_features(*times[row])[field];
      builder.add(names[field], std::move(values));
    }
  }
  if (options.include_relation_features) {
    std::vector<double> counts(log.events.size(), 0.0);
    for (std::size_t row = 0; row < log.events.size(); ++row)
      counts[row] = static_cast<double>(event_objects[row].size());
    builder.add("@@event_num_rel_objs", std::move(counts));
    for (const auto& type : object_types) {
      std::vector<double> values(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row)
        for (const auto object : event_objects[row])
          if (object_types_by_index[object] == type) values[row] += 1.0;
      builder.add("@@event_num_rel_objs_type_" + type, std::move(values));
    }
    std::map<std::string, std::size_t> event_index;
    for (std::size_t row = 0; row < log.events.size(); ++row)
      event_index.emplace(log.events[row].id, row);
    std::vector<double> outgoing(log.events.size(), 0.0);
    std::vector<double> incoming(log.events.size(), 0.0);
    for (const auto& relation : log.e2e) {
      const auto source = event_index.find(relation.source_id);
      const auto target = event_index.find(relation.target_id);
      if (source == event_index.end() || target == event_index.end())
        throw std::invalid_argument("OCEL feature E2E relation references an unknown id");
      outgoing[source->second] += 1.0;
      incoming[target->second] += 1.0;
    }
    builder.add("@@event_num_e2e_out", std::move(outgoing));
    builder.add("@@event_num_e2e_in", std::move(incoming));
  }
  if (options.include_endpoint_features) {
    for (const auto& type : object_types) {
      std::vector<double> starts(log.events.size(), 0.0);
      std::vector<double> ends(log.events.size(), 0.0);
      std::map<std::size_t, bool> seen_start, seen_end;
      for (std::size_t row = 0; row < log.events.size(); ++row) {
        for (const auto object : event_objects[row]) {
          if (object_types_by_index[object] != type) continue;
          if (!seen_start.contains(object)) {
            starts[row] = 1.0;
            seen_start[object] = true;
          }
        }
      }
      for (std::size_t row = log.events.size(); row-- > 0;) {
        for (const auto object : event_objects[row]) {
          if (object_types_by_index[object] != type) continue;
          if (!seen_end.contains(object)) {
            ends[row] = 1.0;
            seen_end[object] = true;
          }
        }
      }
      builder.add("@@event_start_" + type, std::move(starts));
      builder.add("@@event_end_" + type, std::move(ends));
    }
  }
  if (options.include_attribute_counts) {
    std::vector<double> counts(log.events.size(), 0.0);
    for (std::size_t row = 0; row < log.events.size(); ++row)
      counts[row] = static_cast<double>(log.events[row].attributes.size());
    builder.add("@@event_num_attributes", std::move(counts));
  }

  auto add_numeric_attributes = [&](const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
      std::vector<double> values(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row) {
        if (const auto* attr = find_attribute(log.events[row].attributes, key))
          if (const auto value = numeric_value(attr->value)) values[row] = *value;
      }
      builder.add("@@event_num_" + key, std::move(values));
    }
  };
  add_numeric_attributes(options.numeric_event_attributes);

  for (const auto& key : options.string_event_attributes) {
    std::vector<std::string> values;
    for (const auto& event : log.events)
      if (const auto* attr = find_attribute(event.attributes, key))
        values.push_back(string_value(attr->value));
    const auto categories = sorted_unique(std::move(values));
    for (const auto& category : categories) {
      std::vector<double> column(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row)
        if (const auto* attr = find_attribute(log.events[row].attributes, key))
          column[row] = string_value(attr->value) == category ? 1.0 : 0.0;
      builder.add("@@event_str_" + key + "_" + category, std::move(column));
    }
  }

  if (options.include_interaction_features) {
    std::set<std::pair<std::size_t, std::size_t>> seen_pairs;
    std::vector<double> new_interactions(log.events.size(), 0.0);
    for (std::size_t row = 0; row < event_objects.size(); ++row) {
      std::vector<std::size_t> objects = event_objects[row];
      std::sort(objects.begin(), objects.end());
      objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
      for (std::size_t left = 0; left < objects.size(); ++left)
        for (std::size_t right = left + 1; right < objects.size(); ++right)
          if (seen_pairs.insert({objects[left], objects[right]}).second)
            new_interactions[row] += 1.0;
    }
    builder.add("@@event_new_interactions", std::move(new_interactions));
  }
}

std::vector<Lifecycle> make_lifecycles(
    const Ocel& log, const std::vector<std::vector<std::size_t>>& object_events,
    const std::vector<std::optional<double>>& times) {
  std::vector<Lifecycle> lifecycles(log.objects.size());
  for (std::size_t object = 0; object < log.objects.size(); ++object) {
    auto& lifecycle = lifecycles[object];
    // The relation stream is the lifecycle order.  Do not reorder it by
    // timestamp: relation rows may intentionally describe a non-monotonic
    // history, and path/first/last semantics follow that stream.
    lifecycle.event_indices = object_events[object];
    for (const auto event : lifecycle.event_indices) {
      lifecycle.activities.push_back(log.events[event].activity);
      lifecycle.unique_activities.insert(log.events[event].activity);
      if (times[event]) lifecycle.timestamps.push_back(*times[event]);
      else lifecycle.timestamps.push_back(std::numeric_limits<double>::quiet_NaN());
    }
    for (std::size_t i = 1; i < lifecycle.activities.size(); ++i)
      ++lifecycle.paths[{lifecycle.activities[i - 1], lifecycle.activities[i]}];
    for (const auto timestamp : lifecycle.timestamps) {
      if (!std::isfinite(timestamp)) continue;
      if (!lifecycle.start) lifecycle.start = timestamp;
      lifecycle.end = timestamp;
    }
  }
  return lifecycles;
}

void add_object_columns(TableBuilder& builder, const Ocel& log,
                        const OcelFeatureOptions& options,
                        const std::vector<std::string>& activities,
                        const std::vector<std::string>& object_types,
                        const std::vector<std::vector<std::size_t>>& object_events,
                        const std::vector<std::vector<std::size_t>>& event_objects,
                        const std::vector<std::optional<double>>& times,
                        const std::vector<Lifecycle>& lifecycles) {
  if (options.include_activity_features) {
    for (const auto& type : object_types) {
      std::vector<double> values(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        values[row] = log.objects[row].type == type ? 1.0 : 0.0;
      builder.add("@@object_type_" + type, std::move(values));
    }
  }
  if (options.include_attribute_counts) {
    std::vector<double> values(log.objects.size(), 0.0);
    for (std::size_t row = 0; row < log.objects.size(); ++row)
      values[row] = static_cast<double>(log.objects[row].attributes.size());
    builder.add("@@object_num_attributes", std::move(values));
  }
  if (options.include_lifecycle_features) {
    if (options.include_lifecycle_length_features) {
      std::vector<double> length(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        length[row] = static_cast<double>(lifecycles[row].event_indices.size());
      builder.add("@@object_lifecycle_length", std::move(length));
    }
    if (options.include_lifecycle_unique_activity_features) {
      std::vector<double> unique(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        unique[row] = static_cast<double>(lifecycles[row].unique_activities.size());
      builder.add("@@object_lifecycle_unq_act", std::move(unique));
    }
    if (options.include_lifecycle_duration_features) {
      std::vector<double> duration(log.objects.size(), 0.0),
          start(log.objects.size(), 0.0), end(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row) {
        if (!lifecycles[row].start || !lifecycles[row].end) continue;
        duration[row] = *lifecycles[row].end - *lifecycles[row].start;
        start[row] = *lifecycles[row].start;
        end[row] = *lifecycles[row].end;
      }
      builder.add("@@object_lifecycle_duration", std::move(duration));
      builder.add("@@object_lifecycle_start_timestamp", std::move(start));
      builder.add("@@object_lifecycle_end_timestamp", std::move(end));
    }
    if (options.include_lifecycle_activity_features) {
      for (const auto& activity : activities) {
        std::vector<double> values(log.objects.size(), 0.0);
        for (std::size_t row = 0; row < log.objects.size(); ++row)
          values[row] = static_cast<double>(std::count(
              lifecycles[row].activities.begin(), lifecycles[row].activities.end(), activity));
        builder.add("@@ocel_lif_activity_" + activity, std::move(values));
      }
    }
    if (options.include_lifecycle_path_features) {
      std::set<std::pair<std::string, std::string>> paths;
      for (const auto& lifecycle : lifecycles)
        for (const auto& [path, count] : lifecycle.paths) {
          (void)count;
          paths.insert(path);
        }
      for (const auto& path : paths) {
        std::vector<double> values(log.objects.size(), 0.0);
        for (std::size_t row = 0; row < log.objects.size(); ++row) {
          const auto it = lifecycles[row].paths.find(path);
          if (it != lifecycles[row].paths.end()) values[row] = static_cast<double>(it->second);
        }
        builder.add("@@ocel_lif_path_" + path.first + "##" + path.second,
                    std::move(values));
      }
    }
    if (options.include_lifecycle_wip_features) {
      std::vector<double> wip(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row) {
        if (!lifecycles[row].start || !lifecycles[row].end) continue;
        for (std::size_t other = 0; other < log.objects.size(); ++other) {
          if (!lifecycles[other].start || !lifecycles[other].end) continue;
          if (*lifecycles[other].start <= *lifecycles[row].end &&
              *lifecycles[other].end >= *lifecycles[row].start) wip[row] += 1.0;
        }
      }
      builder.add("@@object_wip", std::move(wip));
    }
  }

  auto add_numeric_attributes = [&](const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
      std::vector<double> values(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        if (const auto* attr = find_attribute(log.objects[row].attributes, key))
          if (const auto value = numeric_value(attr->value)) values[row] = *value;
      builder.add("@@object_num_" + key, std::move(values));
    }
  };
  add_numeric_attributes(options.numeric_object_attributes);
  for (const auto& key : options.string_object_attributes) {
    std::vector<std::string> values;
    for (const auto& object : log.objects)
      if (const auto* attr = find_attribute(object.attributes, key))
        values.push_back(string_value(attr->value));
    const auto categories = sorted_unique(std::move(values));
    for (const auto& category : categories) {
      std::vector<double> column(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        if (const auto* attr = find_attribute(log.objects[row].attributes, key))
          column[row] = string_value(attr->value) == category ? 1.0 : 0.0;
      builder.add("@@object_str_" + key + "_" + category, std::move(column));
    }
  }

  const bool include_degree_centrality_features =
      options.include_degree_centrality_features.value_or(
          options.include_interaction_features);
  if (include_degree_centrality_features ||
      options.include_interaction_features ||
      options.include_interaction_by_type_features) {
    std::set<std::pair<std::size_t, std::size_t>> interactions;
    for (const auto& objects : event_objects) {
      std::vector<std::size_t> unique = objects;
      std::sort(unique.begin(), unique.end());
      unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
      for (std::size_t left = 0; left < unique.size(); ++left)
        for (std::size_t right = left + 1; right < unique.size(); ++right)
          interactions.insert({unique[left], unique[right]});
    }
    for (const auto& relation : log.o2o) {
      IndexMap ids;
      for (std::size_t i = 0; i < log.objects.size(); ++i) ids[log.objects[i].id] = i;
      const auto left = ids.find(relation.source_id), right = ids.find(relation.target_id);
      if (left != ids.end() && right != ids.end() && left->second != right->second)
        interactions.insert(std::minmax(left->second, right->second));
    }
    std::vector<std::set<std::size_t>> neighbors(log.objects.size());
    for (const auto& edge : interactions) {
      neighbors[edge.first].insert(edge.second);
      neighbors[edge.second].insert(edge.first);
    }
    if (include_degree_centrality_features) {
      std::vector<double> degree(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < neighbors.size(); ++row)
        degree[row] = static_cast<double>(neighbors[row].size());
      const auto denominator = log.objects.size() > 1
                                   ? static_cast<double>(log.objects.size() - 1)
                                   : 1.0;
      for (auto& value : degree) value /= denominator;
      builder.add("@@object_degree_centrality", std::move(degree));
    }
    if (options.include_interaction_by_type_features) {
      for (const auto& type : object_types) {
        std::vector<double> values(log.objects.size(), 0.0);
        for (std::size_t row = 0; row < neighbors.size(); ++row)
          for (const auto neighbor : neighbors[row])
            if (log.objects[neighbor].type == type) values[row] += 1.0;
        builder.add("@@object_interaction_graph_ot_" + type,
                    std::move(values));
      }
    }
  }

  const bool include_general_graph_features =
      options.include_general_interaction_features ||
      options.include_general_descendants_features ||
      options.include_general_inheritance_features ||
      options.include_cobirth_features || options.include_codeath_features;
  if (include_general_graph_features) {
    using ObjectPair = std::pair<std::size_t, std::size_t>;
    std::set<ObjectPair> interaction_graph;
    std::set<ObjectPair> descendants_graph;
    std::set<ObjectPair> inheritance_graph;
    std::set<ObjectPair> cobirth_graph;
    std::set<ObjectPair> codeath_graph;

    // Relations are deduplicated per event before constructing the graph
    // families, so repeated object-event rows do not inflate any feature.
    const auto event_objects_unique = [&]() {
      std::vector<std::vector<std::size_t>> result = event_objects;
      for (auto& objects : result) {
        std::sort(objects.begin(), objects.end());
        objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
      }
      return result;
    }();

    for (const auto& objects : event_objects_unique) {
      for (std::size_t left = 0; left < objects.size(); ++left) {
        for (std::size_t right = left + 1; right < objects.size(); ++right)
          interaction_graph.emplace(objects[left], objects[right]);
      }
    }

    std::vector<std::size_t> forward_events(log.events.size());
    std::iota(forward_events.begin(), forward_events.end(), 0);
    // Build descendants and cobirth together in the event order.
    {
      std::set<std::size_t> seen;
      for (const auto event : forward_events) {
        const auto& objects = event_objects_unique[event];
        std::vector<std::size_t> unseen;
        for (const auto object : objects)
          if (!seen.contains(object)) unseen.push_back(object);
        for (const auto object : objects) {
          if (!seen.contains(object)) continue;
          for (const auto child : unseen) descendants_graph.emplace(object, child);
        }
        for (std::size_t left = 0; left < unseen.size(); ++left)
          for (std::size_t right = left + 1; right < unseen.size(); ++right)
            cobirth_graph.emplace(unseen[left], unseen[right]);
        seen.insert(unseen.begin(), unseen.end());
      }
    }
    // Codeath is the same first-seen construction with the event order
    // traversed backwards.
    {
      std::set<std::size_t> seen;
      for (auto event = forward_events.rbegin(); event != forward_events.rend();
           ++event) {
        const auto& objects = event_objects_unique[*event];
        std::vector<std::size_t> unseen;
        for (const auto object : objects)
          if (!seen.contains(object)) unseen.push_back(object);
        for (std::size_t left = 0; left < unseen.size(); ++left)
          for (std::size_t right = left + 1; right < unseen.size(); ++right)
            codeath_graph.emplace(unseen[left], unseen[right]);
        seen.insert(unseen.begin(), unseen.end());
      }
    }

    std::vector<std::size_t> first(log.objects.size(), log.events.size());
    std::vector<std::size_t> last(log.objects.size(), log.events.size());
    for (std::size_t event = 0; event < event_objects_unique.size(); ++event)
      for (const auto object : event_objects_unique[event]) {
        first[object] = std::min(first[object], event);
        last[object] = event;
      }
    for (std::size_t event = 0; event < event_objects_unique.size(); ++event) {
      const auto& objects = event_objects_unique[event];
      for (const auto source : objects) {
        if (last[source] != event) continue;
        for (const auto target : objects) {
          if (source != target && first[target] == event)
            inheritance_graph.emplace(source, target);
        }
      }
    }
    std::set<ObjectPair> reciprocal_edges;
    for (const auto& edge : inheritance_graph)
      if (inheritance_graph.contains({edge.second, edge.first}))
        reciprocal_edges.insert(edge);
    for (const auto& edge : reciprocal_edges) inheritance_graph.erase(edge);

    const auto undirected_degrees = [&](const std::set<ObjectPair>& graph) {
      std::vector<std::set<std::size_t>> neighbors(log.objects.size());
      for (const auto& [left, right] : graph) {
        neighbors[left].insert(right);
        neighbors[right].insert(left);
      }
      std::vector<double> result(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < result.size(); ++row)
        result[row] = static_cast<double>(neighbors[row].size());
      return result;
    };
    const auto add_directed_degrees = [&](const std::set<ObjectPair>& graph,
                                          bool incoming, std::string name) {
      std::vector<double> values(log.objects.size(), 0.0);
      for (const auto& [source, target] : graph)
        values[incoming ? target : source] += 1.0;
      builder.add(std::move(name), std::move(values));
    };
    if (options.include_general_interaction_features)
      builder.add("@@object_general_interaction_graph",
                  undirected_degrees(interaction_graph));
    if (options.include_general_descendants_features) {
      add_directed_degrees(descendants_graph, true,
                           "@@object_general_descendants_graph_ascendants");
      add_directed_degrees(descendants_graph, false,
                           "@@object_general_descendants_graph_descendants");
    }
    if (options.include_general_inheritance_features) {
      add_directed_degrees(inheritance_graph, true,
                           "@@object_general_inheritance_graph_ascendants");
      add_directed_degrees(inheritance_graph, false,
                           "@@object_general_inheritance_graph_descendants");
    }
    if (options.include_cobirth_features)
      builder.add("@@object_cobirth", undirected_degrees(cobirth_graph));
    if (options.include_codeath_features)
      builder.add("@@object_codeath", undirected_degrees(codeath_graph));
  }

  if (options.include_related_events_features ||
      options.include_related_activities_features) {
    // Build the event feature vectors once, then project them through the
    // relation stream for each object.  The source table uses the same option
    // groups as the standalone event variant, including selected attributes.
    TableBuilder event_builder(log.events.size(),
                               std::numeric_limits<std::size_t>::max());
    add_event_columns(event_builder, log, options, activities, object_types,
                      event_objects, times, [&]() {
                        std::vector<std::string> types;
                        types.reserve(log.objects.size());
                        for (const auto& object : log.objects) types.push_back(object.type);
                        return types;
                      }());
    std::vector<std::string> event_ids;
    event_ids.reserve(log.events.size());
    for (const auto& event : log.events) event_ids.push_back(event.id);
    const auto event_table = std::move(event_builder).finish(
        OcelFeatureVariant::kEvents, std::move(event_ids), {});

    if (options.include_related_events_features) {
      for (std::size_t feature = 0; feature < event_table.feature_names.size();
           ++feature) {
        std::vector<double> minimum(log.objects.size(), 0.0);
        std::vector<double> maximum(log.objects.size(), 0.0);
        for (std::size_t object = 0; object < log.objects.size(); ++object) {
          bool found = false;
          double min_value = std::numeric_limits<double>::infinity();
          double max_value = -std::numeric_limits<double>::infinity();
          for (const auto event : object_events[object]) {
            const auto value = event_table.values[event][feature];
            min_value = std::min(min_value, value);
            max_value = std::max(max_value, value);
            found = true;
          }
          if (found) {
            minimum[object] = min_value;
            maximum[object] = max_value;
          }
        }
        const auto& name = event_table.feature_names[feature];
        builder.add("@@rel_eve_min_" + name, std::move(minimum));
        builder.add("@@rel_eve_max_" + name, std::move(maximum));
      }
    }

    if (options.include_related_activities_features) {
      // object_events preserves raw relation order.  Therefore the last
      // matching event here intentionally follows relation order rather than
      // timestamp order.
      std::vector<std::string> related_activities;
      for (const auto& events : object_events)
        for (const auto event : events)
          related_activities.push_back(log.events[event].activity);
      related_activities = sorted_unique(std::move(related_activities));
      std::vector<std::vector<std::size_t>> last_by_activity(
          related_activities.size(),
          std::vector<std::size_t>(log.objects.size(), log.events.size()));
      for (std::size_t activity = 0; activity < related_activities.size(); ++activity)
        for (std::size_t object = 0; object < log.objects.size(); ++object)
          for (const auto event : object_events[object])
            if (log.events[event].activity == related_activities[activity])
              last_by_activity[activity][object] = event;
      for (std::size_t feature = 0; feature < event_table.feature_names.size();
           ++feature) {
        for (std::size_t activity = 0; activity < related_activities.size(); ++activity) {
          std::vector<double> values(log.objects.size(), 0.0);
          for (std::size_t object = 0; object < log.objects.size(); ++object)
            if (last_by_activity[activity][object] != log.events.size())
              values[object] = event_table.values[
                  last_by_activity[activity][object]][feature];
          builder.add("@@ev_act_fea_" + related_activities[activity] + "_" +
                          event_table.feature_names[feature],
                      std::move(values));
        }
      }
    }
  }

  if (options.include_obj_con_in_graph_features) {
    using ObjectPair = std::pair<std::size_t, std::size_t>;
    std::set<ObjectPair> interactions;
    for (const auto& objects : event_objects) {
      std::vector<std::size_t> unique = objects;
      std::sort(unique.begin(), unique.end());
      unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
      for (std::size_t left = 0; left < unique.size(); ++left)
        for (std::size_t right = left + 1; right < unique.size(); ++right)
          interactions.emplace(unique[left], unique[right]);
    }
    IndexMap ids;
    for (std::size_t i = 0; i < log.objects.size(); ++i)
      ids.emplace(log.objects[i].id, i);
    for (const auto& relation : log.o2o) {
      const auto left = ids.find(relation.source_id), right = ids.find(relation.target_id);
      if (left != ids.end() && right != ids.end() && left->second != right->second)
        interactions.insert(std::minmax(left->second, right->second));
    }
    std::vector<std::set<std::size_t>> neighbors(log.objects.size());
    for (const auto& [left, right] : interactions) {
      neighbors[left].insert(right);
      neighbors[right].insert(left);
    }

    // Recursively obtain the ordinary object feature set, with all derived
    // relation/neighbor controls disabled to avoid self-referential columns.
    auto base_options = options;
    base_options.include_related_events_features = false;
    base_options.include_related_activities_features = false;
    base_options.include_obj_con_in_graph_features = false;
    base_options.max_features = std::numeric_limits<std::size_t>::max();
    TableBuilder base_builder(log.objects.size(), base_options.max_features);
    add_object_columns(base_builder, log, base_options, activities, object_types,
                       object_events, event_objects, times, lifecycles);
    std::vector<std::string> object_ids, object_types_for_rows;
    object_ids.reserve(log.objects.size());
    object_types_for_rows.reserve(log.objects.size());
    for (const auto& object : log.objects) {
      object_ids.push_back(object.id);
      object_types_for_rows.push_back(object.type);
    }
    const auto object_table = std::move(base_builder).finish(
        OcelFeatureVariant::kObjects, std::move(object_ids),
        std::move(object_types_for_rows));
    for (std::size_t feature = 0; feature < object_table.feature_names.size();
         ++feature) {
      std::vector<double> minimum(log.objects.size(), 0.0);
      std::vector<double> maximum(log.objects.size(), 0.0);
      for (std::size_t object = 0; object < log.objects.size(); ++object) {
        if (neighbors[object].empty()) continue;
        auto neighbor = neighbors[object].begin();
        double min_value = object_table.values[*neighbor][feature];
        double max_value = min_value;
        for (++neighbor; neighbor != neighbors[object].end(); ++neighbor) {
          min_value = std::min(min_value, object_table.values[*neighbor][feature]);
          max_value = std::max(max_value, object_table.values[*neighbor][feature]);
        }
        minimum[object] = min_value;
        maximum[object] = max_value;
      }
      const auto& name = object_table.feature_names[feature];
      builder.add("@@obj_graph_con_min_" + name, std::move(minimum));
      builder.add("@@obj_graph_con_max_" + name, std::move(maximum));
    }
  }
  (void)object_events;
  (void)times;
}

void add_related_object_columns(
    TableBuilder& builder, const Ocel& log, const OcelFeatureOptions& options,
    const std::vector<std::string>& activities,
    const std::vector<std::string>& object_types,
    const std::vector<std::vector<std::size_t>>& object_events,
    const std::vector<std::vector<std::size_t>>& event_objects,
    const std::vector<std::optional<double>>& times,
    const std::vector<Lifecycle>& lifecycles) {
  if (!options.include_related_objects_features) return;

  // Build the same deterministic object schema as the standalone OBJECTS
  // table. Keep every selected object feature family; only this aggregation
  // itself is disabled so the source schema cannot recurse.
  auto base_options = options;
  base_options.include_related_objects_features = false;
  base_options.max_features = std::numeric_limits<std::size_t>::max();
  TableBuilder object_builder(log.objects.size(), base_options.max_features);
  add_object_columns(object_builder, log, base_options, activities, object_types,
                     object_events, event_objects, times, lifecycles);
  std::vector<std::string> object_ids, object_types_for_rows;
  object_ids.reserve(log.objects.size());
  object_types_for_rows.reserve(log.objects.size());
  for (const auto& object : log.objects) {
    object_ids.push_back(object.id);
    object_types_for_rows.push_back(object.type);
  }
  const auto object_table = std::move(object_builder).finish(
      OcelFeatureVariant::kObjects, std::move(object_ids),
      std::move(object_types_for_rows));

  for (std::size_t feature = 0; feature < object_table.feature_names.size();
       ++feature) {
    std::vector<double> minimum(log.events.size(), 0.0);
    std::vector<double> maximum(log.events.size(), 0.0);
    for (std::size_t event = 0; event < log.events.size(); ++event) {
      std::vector<std::size_t> related = event_objects[event];
      std::sort(related.begin(), related.end());
      related.erase(std::unique(related.begin(), related.end()), related.end());
      if (related.empty()) continue;
      double min_value = object_table.values[related.front()][feature];
      double max_value = min_value;
      for (std::size_t index = 1; index < related.size(); ++index) {
        const auto value = object_table.values[related[index]][feature];
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
      }
      minimum[event] = min_value;
      maximum[event] = max_value;
    }
    const auto& name = object_table.feature_names[feature];
    builder.add("@@rel_obj_fea_min_" + name, std::move(minimum));
    builder.add("@@rel_obj_fea_max_" + name, std::move(maximum));
  }
}

void add_prefix_columns(
    TableBuilder& builder, const Ocel& log, const OcelFeatureOptions& options,
    const std::vector<std::string>& activities, const IndexMap& event_index,
    const IndexMap& object_index,
    const std::vector<std::optional<double>>& times) {
  if (!options.include_prefix_features) return;

  std::vector<std::vector<std::size_t>> seen_events(log.objects.size());
  std::vector<std::vector<std::size_t>> prefixes(log.relations.size());
  for (std::size_t row = 0; row < log.relations.size(); ++row) {
    const auto event = event_index.find(log.relations[row].event_id);
    const auto object = object_index.find(log.relations[row].object_id);
    if (event == event_index.end() || object == object_index.end())
      throw std::invalid_argument("OCEL feature prefix relation references an unknown id");
    auto& seen = seen_events[object->second];
    prefixes[row] = seen;
    if (std::find(seen.begin(), seen.end(), event->second) == seen.end())
      seen.push_back(event->second);
  }

  if (options.include_prefix_length_features) {
    std::vector<double> values(log.relations.size(), 0.0);
    for (std::size_t row = 0; row < values.size(); ++row)
      values[row] = static_cast<double>(prefixes[row].size());
    builder.add("@@ev_obj_pref_length", std::move(values));
  }
  if (options.include_prefix_timediff_features) {
    std::vector<double> values(log.relations.size(), 0.0);
    for (std::size_t row = 0; row < values.size(); ++row) {
      const auto event = event_index.at(log.relations[row].event_id);
      if (!prefixes[row].empty() && times[event] &&
          times[prefixes[row].front()])
        values[row] = *times[event] - *times[prefixes[row].front()];
    }
    builder.add("@@ev_obj_pref_timediff", std::move(values));
  }
  if (options.include_prefix_one_hot_features) {
    for (const auto& activity : activities) {
      std::vector<double> values(log.relations.size(), 0.0);
      for (std::size_t row = 0; row < values.size(); ++row)
        for (const auto event : prefixes[row])
          if (log.events[event].activity == activity) values[row] += 1.0;
      builder.add("@@ev_obj_pref_act_" + activity, std::move(values));
    }
  }
}

}  // namespace

const char* ocel_feature_variant_name(OcelFeatureVariant variant) noexcept {
  return variant_name(variant);
}

OcelFeatureTable extract_ocel_features(const Ocel& log,
                                       OcelFeatureVariant variant,
                                       const OcelFeatureOptions& options) {
  if (options.max_features == 0) throw std::invalid_argument("OCEL max_features must be positive");
  if (options.filter_per_type) {
    if (options.filter_per_type->empty())
      throw std::invalid_argument("OCEL filter_per_type must not be empty");
    if (variant != OcelFeatureVariant::kObjects)
      throw std::invalid_argument(
          "OCEL filter_per_type applies only to the objects variant");
  }
  const bool related_object_event_context =
      options.include_related_objects_features &&
      variant == OcelFeatureVariant::kEvents;
  if (options.include_interaction_by_type_features &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL interaction-by-type features apply only to the objects variant");
  }
  if (options.include_degree_centrality_features.has_value() &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL degree centrality features apply only to the objects variant");
  }
  if ((options.include_general_interaction_features ||
       options.include_general_descendants_features ||
       options.include_general_inheritance_features || options.include_cobirth_features ||
       options.include_codeath_features) &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL object graph features apply only to the objects variant");
  }
  if ((options.include_related_events_features ||
       options.include_related_activities_features ||
       options.include_obj_con_in_graph_features) &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL related-event and neighbor features apply only to the objects variant");
  }
  if (options.include_related_objects_features &&
      variant != OcelFeatureVariant::kEvents) {
    throw std::invalid_argument(
        "OCEL related-object features apply only to the events variant");
  }
  const bool prefix_control_disabled =
      !options.include_prefix_features ||
      !options.include_prefix_length_features ||
      !options.include_prefix_timediff_features ||
      !options.include_prefix_one_hot_features;
  if (variant != OcelFeatureVariant::kEventsObjects && prefix_control_disabled)
    throw std::invalid_argument(
        "OCEL prefix features apply only to the events_objects variant");
  const bool lifecycle_object_control_disabled =
      !options.include_lifecycle_length_features ||
      !options.include_lifecycle_unique_activity_features ||
      !options.include_lifecycle_duration_features ||
      !options.include_lifecycle_activity_features ||
      !options.include_lifecycle_path_features ||
      !options.include_lifecycle_wip_features;
  if (lifecycle_object_control_disabled &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL lifecycle metric controls apply only to the objects variant");
  }
  IndexMap event_index, object_index;
  for (std::size_t i = 0; i < log.events.size(); ++i) {
    if (log.events[i].id.empty() || !event_index.emplace(log.events[i].id, i).second)
      throw std::invalid_argument("OCEL feature events require unique non-empty ids");
  }
  for (std::size_t i = 0; i < log.objects.size(); ++i) {
    if (log.objects[i].id.empty() || !object_index.emplace(log.objects[i].id, i).second)
      throw std::invalid_argument("OCEL feature objects require unique non-empty ids");
  }
  std::vector<std::vector<std::size_t>> event_objects, object_events;
  event_relations(log, event_index, object_index, event_objects, object_events);
  const auto times = event_times(log);
  const auto activities = activity_names(log);
  const auto object_types = object_type_names(log);
  std::vector<std::string> object_types_by_index;
  object_types_by_index.reserve(log.objects.size());
  for (const auto& object : log.objects) object_types_by_index.push_back(object.type);
  const auto lifecycles = make_lifecycles(log, object_events, times);

  if (variant == OcelFeatureVariant::kEvents) {
    TableBuilder builder(log.events.size(), options.max_features);
    add_event_columns(builder, log, options, activities, object_types, event_objects,
                      times, object_types_by_index);
    add_related_object_columns(builder, log, options, activities, object_types,
                               object_events, event_objects, times, lifecycles);
    std::vector<std::string> ids;
    ids.reserve(log.events.size());
    for (const auto& event : log.events) ids.push_back(event.id);
    return std::move(builder).finish(variant, std::move(ids), {});
  }
  if (variant == OcelFeatureVariant::kObjects) {
    TableBuilder builder(log.objects.size(), options.max_features);
    add_object_columns(builder, log, options, activities, object_types, object_events,
                       event_objects, times, lifecycles);
    std::vector<std::string> ids, types;
    ids.reserve(log.objects.size());
    types.reserve(log.objects.size());
    for (const auto& object : log.objects) {
      ids.push_back(object.id);
      types.push_back(object.type);
    }
    auto table = std::move(builder).finish(variant, std::move(ids), std::move(types));
    if (options.filter_per_type) {
      std::vector<std::string> filtered_ids, filtered_types;
      std::vector<std::vector<double>> filtered_values;
      filtered_ids.reserve(table.row_ids.size());
      filtered_types.reserve(table.row_types.size());
      filtered_values.reserve(table.values.size());
      for (std::size_t row = 0; row < table.row_ids.size(); ++row) {
        if (table.row_types[row] != *options.filter_per_type) continue;
        filtered_ids.push_back(table.row_ids[row]);
        filtered_types.push_back(table.row_types[row]);
        filtered_values.push_back(std::move(table.values[row]));
      }
      table.row_ids = std::move(filtered_ids);
      table.row_types = std::move(filtered_types);
      table.values = std::move(filtered_values);
    }
    return table;
  }

  const auto row_count = log.relations.size();

  // Build the two entity tables with the same option groups used by their
  // standalone variants, then project each column onto the relation rows.
  // This keeps the exploded representation pointwise and makes newly added
  // event/object feature groups available here without a second implementation.
  TableBuilder event_builder(log.events.size(), options.max_features);
  auto events_objects_options = options;
  events_objects_options.include_related_objects_features = false;
  add_event_columns(event_builder, log, events_objects_options, activities, object_types,
                    event_objects, times, object_types_by_index);
  std::vector<std::string> event_ids;
  event_ids.reserve(log.events.size());
  for (const auto& event : log.events) event_ids.push_back(event.id);
  const auto event_table = std::move(event_builder).finish(
      OcelFeatureVariant::kEvents, std::move(event_ids), {});

  TableBuilder object_builder(log.objects.size(), options.max_features);
  add_object_columns(object_builder, log, options, activities, object_types,
                    object_events, event_objects, times, lifecycles);
  std::vector<std::string> object_ids, object_types_for_rows;
  object_ids.reserve(log.objects.size());
  object_types_for_rows.reserve(log.objects.size());
  for (const auto& object : log.objects) {
    object_ids.push_back(object.id);
    object_types_for_rows.push_back(object.type);
  }
  const auto object_table = std::move(object_builder).finish(
      OcelFeatureVariant::kObjects, std::move(object_ids),
      std::move(object_types_for_rows));

  TableBuilder builder(row_count, options.max_features);
  const auto project = [&](const OcelFeatureTable& source,
                           bool project_event) {
    for (std::size_t column = 0; column < source.feature_names.size(); ++column) {
      std::vector<double> values;
      values.reserve(row_count);
      for (const auto& relation : log.relations) {
        const auto index = project_event
                               ? event_index.at(relation.event_id)
                               : object_index.at(relation.object_id);
        values.push_back(source.values[index][column]);
      }
      builder.add(source.feature_names[column], std::move(values));
    }
  };
  project(event_table, true);
  project(object_table, false);
  add_prefix_columns(builder, log, options, activities, event_index, object_index,
                     times);
  std::vector<std::string> row_ids, row_types;
  row_ids.reserve(row_count);
  row_types.reserve(row_count);
  std::map<std::string, std::size_t> seen_ids;
  for (const auto& relation : log.relations) {
    auto id = relation.event_id + "::" + relation.object_id;
    const auto count = seen_ids[id]++;
    if (count != 0) id += "#" + std::to_string(count + 1);
    row_ids.push_back(std::move(id));
    row_types.push_back(log.objects[object_index.at(relation.object_id)].type);
  }
  return std::move(builder).finish(variant, std::move(row_ids), std::move(row_types));
}

std::string serialize_ocel_features_json(const OcelFeatureTable& table) {
  if (table.row_ids.size() != table.values.size() ||
      (!table.row_types.empty() && table.row_types.size() != table.row_ids.size()))
    throw std::invalid_argument("OCEL feature table row metadata is inconsistent");
  for (const auto& row : table.values)
    if (row.size() != table.feature_names.size())
      throw std::invalid_argument("OCEL feature table columns are inconsistent");
  std::ostringstream out;
  out << "{\"family\":\"OCEL_FEATURES\",\"variant\":\""
      << variant_name(table.variant) << "\",\"row_ids\":[";
  for (std::size_t i = 0; i < table.row_ids.size(); ++i) {
    if (i) out << ',';
    json_string(out, table.row_ids[i]);
  }
  out << "]";
  if (!table.row_types.empty()) {
    out << ",\"row_types\":[";
    for (std::size_t i = 0; i < table.row_types.size(); ++i) {
      if (i) out << ',';
      json_string(out, table.row_types[i]);
    }
    out << ']';
  }
  out << ",\"feature_names\":[";
  for (std::size_t i = 0; i < table.feature_names.size(); ++i) {
    if (i) out << ',';
    json_string(out, table.feature_names[i]);
  }
  out << "],\"values\":[" << std::setprecision(17);
  for (std::size_t row = 0; row < table.values.size(); ++row) {
    if (row) out << ',';
    out << '[';
    for (std::size_t column = 0; column < table.values[row].size(); ++column) {
      if (column) out << ',';
      out << table.values[row][column];
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
