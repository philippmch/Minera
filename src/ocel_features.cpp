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
    lifecycle.event_indices = object_events[object];
    std::stable_sort(lifecycle.event_indices.begin(), lifecycle.event_indices.end(),
                     [&](std::size_t left, std::size_t right) {
                       if (times[left] && times[right] && *times[left] != *times[right])
                         return *times[left] < *times[right];
                       return left < right;
                     });
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
      if (!lifecycle.start || timestamp < *lifecycle.start) lifecycle.start = timestamp;
      if (!lifecycle.end || timestamp > *lifecycle.end) lifecycle.end = timestamp;
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
    std::vector<double> length(log.objects.size(), 0.0), duration(log.objects.size(), 0.0),
        start(log.objects.size(), 0.0), end(log.objects.size(), 0.0), unique(log.objects.size(), 0.0);
    for (std::size_t row = 0; row < log.objects.size(); ++row) {
      length[row] = static_cast<double>(lifecycles[row].event_indices.size());
      unique[row] = static_cast<double>(lifecycles[row].unique_activities.size());
      if (lifecycles[row].start && lifecycles[row].end) {
        duration[row] = std::max(0.0, *lifecycles[row].end - *lifecycles[row].start);
        start[row] = *lifecycles[row].start;
        end[row] = *lifecycles[row].end;
      }
    }
    builder.add("@@object_lifecycle_length", std::move(length));
    builder.add("@@object_lifecycle_unq_act", std::move(unique));
    builder.add("@@object_lifecycle_duration", std::move(duration));
    builder.add("@@object_lifecycle_start_timestamp", std::move(start));
    builder.add("@@object_lifecycle_end_timestamp", std::move(end));
    for (const auto& activity : activities) {
      std::vector<double> values(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        values[row] = static_cast<double>(std::count(
            lifecycles[row].activities.begin(), lifecycles[row].activities.end(), activity));
      builder.add("@@ocel_lif_activity_" + activity, std::move(values));
    }
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

  if (options.include_interaction_features ||
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
    if (options.include_interaction_features) {
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
  (void)object_events;
  (void)times;
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
  if (options.include_interaction_by_type_features &&
      variant != OcelFeatureVariant::kObjects) {
    throw std::invalid_argument(
        "OCEL interaction-by-type features apply only to the objects variant");
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
  TableBuilder builder(row_count, options.max_features);
  // The exploded table reuses the event/object feature semantics but keeps one
  // row per relation, which is the useful join shape for downstream learners.
  for (const auto& activity : activities) {
    std::vector<double> values;
    values.reserve(row_count);
    for (const auto& relation : log.relations) {
      const auto event = event_index.at(relation.event_id);
      values.push_back(log.events[event].activity == activity ? 1.0 : 0.0);
    }
    builder.add("@@event_act_" + activity, std::move(values));
  }
  std::vector<double> event_timestamp, event_relation_count, object_lifecycle_length,
      object_lifecycle_duration;
  event_timestamp.reserve(row_count);
  event_relation_count.reserve(row_count);
  object_lifecycle_length.reserve(row_count);
  object_lifecycle_duration.reserve(row_count);
  for (const auto& relation : log.relations) {
    const auto event = event_index.at(relation.event_id);
    const auto object = object_index.at(relation.object_id);
    event_timestamp.push_back(times[event].value_or(0.0));
    event_relation_count.push_back(static_cast<double>(event_objects[event].size()));
    object_lifecycle_length.push_back(
        static_cast<double>(lifecycles[object].event_indices.size()));
    object_lifecycle_duration.push_back(
        lifecycles[object].start && lifecycles[object].end
            ? std::max(0.0, *lifecycles[object].end - *lifecycles[object].start)
            : 0.0);
  }
  builder.add("@@event_timestamp", std::move(event_timestamp));
  builder.add("@@event_num_rel_objs", std::move(event_relation_count));
  for (const auto& type : object_types) {
    std::vector<double> values;
    values.reserve(row_count);
    for (const auto& relation : log.relations) {
      const auto object = object_index.at(relation.object_id);
      values.push_back(log.objects[object].type == type ? 1.0 : 0.0);
    }
    builder.add("@@object_type_" + type, std::move(values));
  }
  builder.add("@@object_lifecycle_length", std::move(object_lifecycle_length));
  builder.add("@@object_lifecycle_duration", std::move(object_lifecycle_duration));
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
