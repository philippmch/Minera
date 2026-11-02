#include "minera/ocel_features.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
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
using ObjectPair = std::pair<std::size_t, std::size_t>;

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

const char* variant_name(OcelFeatureVariant variant) {
  switch (variant) {
    case OcelFeatureVariant::kEvents: return "EVENTS";
    case OcelFeatureVariant::kObjects: return "OBJECTS";
    case OcelFeatureVariant::kEventsObjects: return "EVENTS_OBJECTS";
  }
  throw std::invalid_argument("OCEL feature variant is invalid");
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

std::optional<std::string> string_value(const OcelValue& value) {
  if (const auto* text = std::get_if<std::string>(&value)) return *text;
  if (const auto* boolean = std::get_if<bool>(&value))
    return *boolean ? "True" : "False";
  if (const auto* integer = std::get_if<std::int64_t>(&value))
    return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value)) {
    if (!std::isfinite(*real)) return std::nullopt;
    std::array<char, 64> buffer{};
    const auto [end, error] = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), *real,
        std::chars_format::general);
    if (error != std::errc{}) return std::nullopt;
    std::string text(buffer.data(), end);
    // Preserve the distinction between a floating-point 2.0 and the integer
    // value 2 when scalar values are stringified for categorical columns.
    if (text.find_first_of(".eE") == std::string::npos) text += ".0";
    return text;
  }
  return std::nullopt;
}

const OcelAttribute* find_attribute(const std::vector<OcelAttribute>& attrs,
                                    std::string_view key) {
  for (const auto& attr : attrs)
    if (attr.key == key) return &attr;
  return nullptr;
}

std::vector<std::string> ordered_unique(std::vector<std::string> values) {
  std::set<std::string> seen;
  std::vector<std::string> result;
  result.reserve(values.size());
  for (auto& value : values)
    if (seen.insert(value).second) result.push_back(std::move(value));
  return result;
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
  return ordered_unique(std::move(values));
}

std::vector<std::string> object_type_names(const Ocel& log) {
  std::vector<std::string> values;
  values.reserve(log.objects.size());
  for (const auto& object : log.objects) values.push_back(object.type);
  return ordered_unique(std::move(values));
}

std::set<ObjectPair> interaction_edges(
    const Ocel& log, const std::vector<std::vector<std::size_t>>& event_objects,
    OcelInteractionGraphSource source) {
  std::set<ObjectPair> edges;
  for (const auto& objects : event_objects) {
    std::vector<std::size_t> unique = objects;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    for (std::size_t left = 0; left < unique.size(); ++left)
      for (std::size_t right = left + 1; right < unique.size(); ++right)
        edges.emplace(unique[left], unique[right]);
  }
  if (source == OcelInteractionGraphSource::kEventCooccurrence) return edges;
  IndexMap ids;
  for (std::size_t i = 0; i < log.objects.size(); ++i)
    ids.emplace(log.objects[i].id, i);
  for (const auto& relation : log.o2o) {
    const auto left = ids.find(relation.source_id);
    const auto right = ids.find(relation.target_id);
    if (left != ids.end() && right != ids.end() && left->second != right->second)
      edges.insert(std::minmax(left->second, right->second));
  }
  return edges;
}

struct ObjectGraphFamilies {
  std::set<ObjectPair> interaction;
  std::set<ObjectPair> descendants;
  std::set<ObjectPair> inheritance;
  std::set<ObjectPair> cobirth;
  std::set<ObjectPair> codeath;
};

std::vector<std::vector<std::size_t>> unique_event_objects(
    const std::vector<std::vector<std::size_t>>& event_objects) {
  auto result = event_objects;
  for (auto& objects : result) {
    std::sort(objects.begin(), objects.end());
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
  }
  return result;
}

ObjectGraphFamilies build_object_graph_families(
    const Ocel& log, const std::vector<std::vector<std::size_t>>& event_objects) {
  ObjectGraphFamilies graphs;
  const auto event_objects_unique = unique_event_objects(event_objects);
  for (const auto& objects : event_objects_unique)
    for (std::size_t left = 0; left < objects.size(); ++left)
      for (std::size_t right = left + 1; right < objects.size(); ++right)
        graphs.interaction.emplace(objects[left], objects[right]);

  std::vector<std::size_t> forward_events(log.events.size());
  std::iota(forward_events.begin(), forward_events.end(), 0);
  {
    std::set<std::size_t> seen;
    for (const auto event : forward_events) {
      const auto& objects = event_objects_unique[event];
      std::vector<std::size_t> unseen;
      for (const auto object : objects)
        if (!seen.contains(object)) unseen.push_back(object);
      for (const auto object : objects) {
        if (!seen.contains(object)) continue;
        for (const auto child : unseen) graphs.descendants.emplace(object, child);
      }
      for (std::size_t left = 0; left < unseen.size(); ++left)
        for (std::size_t right = left + 1; right < unseen.size(); ++right)
          graphs.cobirth.emplace(unseen[left], unseen[right]);
      seen.insert(unseen.begin(), unseen.end());
    }
  }
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
          graphs.codeath.emplace(unseen[left], unseen[right]);
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
      for (const auto target : objects)
        if (source != target && first[target] == event)
          graphs.inheritance.emplace(source, target);
    }
  }
  std::vector<ObjectPair> reciprocal;
  for (const auto& edge : graphs.inheritance)
    if (graphs.inheritance.contains({edge.second, edge.first})) reciprocal.push_back(edge);
  for (const auto& edge : reciprocal) graphs.inheritance.erase(edge);
  return graphs;
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

std::optional<int> timestamp_integer(std::string_view text, std::size_t offset,
                                     std::size_t count) {
  if (offset + count > text.size() || count == 0) return std::nullopt;
  int result = 0;
  for (std::size_t index = 0; index < count; ++index) {
    const auto value = static_cast<unsigned char>(text[offset + index]);
    if (value < '0' || value > '9') return std::nullopt;
    result = result * 10 + static_cast<int>(value - '0');
  }
  return result;
}

// Return the number of civil days relative to 1970-01-01.  This Gregorian
// calculation intentionally never consults the process timezone, so calendar
// features preserve the source timestamp's local offset.
std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const auto era = (year >= 0 ? year : year - 399) / 400;
  const auto year_of_era = static_cast<unsigned>(year - era * 400);
  const auto month_prime = month > 2 ? month - 3 : month + 9;
  const auto day_of_year = (153 * month_prime + 2) / 5 + day - 1;
  const auto day_of_era = year_of_era * 365 + year_of_era / 4 -
                          year_of_era / 100 + day_of_year;
  return static_cast<std::int64_t>(era) * 146097 +
         static_cast<std::int64_t>(day_of_era) - 719468;
}

std::array<double, 5> calendar_features(std::string_view text, double seconds) {
  const auto year = timestamp_integer(text, 0, 4);
  const auto month = timestamp_integer(text, 5, 2);
  const auto day = timestamp_integer(text, 8, 2);
  if (!year || !month || !day || text.size() < 10 || text[4] != '-' ||
      text[7] != '-')
    throw std::invalid_argument("OCEL feature timestamp is invalid");

  int hour = 0;
  if (text.size() > 10) {
    if (text.size() < 16 ||
        (text[10] != 'T' && text[10] != 't' && text[10] != ' ') ||
        text[13] != ':')
      throw std::invalid_argument("OCEL feature timestamp is invalid");
    const auto parsed_hour = timestamp_integer(text, 11, 2);
    const auto parsed_minute = timestamp_integer(text, 14, 2);
    if (!parsed_hour || !parsed_minute)
      throw std::invalid_argument("OCEL feature timestamp is invalid");
    hour = *parsed_hour;
  }
  const auto weekday = days_from_civil(*year, static_cast<unsigned>(*month),
                                       static_cast<unsigned>(*day));
  const auto monday_zero = static_cast<int>(((weekday + 3) % 7 + 7) % 7);
  return {seconds, static_cast<double>(monday_zero), static_cast<double>(hour),
          static_cast<double>(*month), static_cast<double>(*day)};
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
                       const std::vector<std::vector<std::size_t>>& object_events,
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
        if (times[row])
          values[row] = calendar_features(log.events[row].timestamp,
                                          *times[row])[field];
      builder.add(names[field], std::move(values));
    }
  }
  const bool include_event_num_rel_objs =
      options.include_event_num_rel_objs_features.value_or(
          options.include_relation_features);
  const bool include_event_num_rel_objs_type =
      options.include_event_num_rel_objs_type_features.value_or(
          options.include_relation_features);
  if (include_event_num_rel_objs || include_event_num_rel_objs_type ||
      options.include_relation_features) {
    if (include_event_num_rel_objs) {
      std::vector<double> counts(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row)
        counts[row] = static_cast<double>(event_objects[row].size());
      builder.add("@@event_num_rel_objs", std::move(counts));
    }
    if (include_event_num_rel_objs_type) {
      for (const auto& type : object_types) {
        std::vector<double> values(log.events.size(), 0.0);
        for (std::size_t row = 0; row < log.events.size(); ++row) {
          std::set<std::size_t> seen_objects;
          for (const auto object : event_objects[row])
            if (object_types_by_index[object] == type &&
                seen_objects.insert(object).second)
              values[row] += 1.0;
        }
        builder.add("@@event_num_rel_objs_type_" + type, std::move(values));
      }
    }
    if (options.include_relation_features) {
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
  }
  const bool include_event_start_ot = options.include_event_start_ot_features.value_or(
      options.include_endpoint_features);
  const bool include_event_end_ot = options.include_event_end_ot_features.value_or(
      options.include_endpoint_features);
  const bool compatibility_names =
      options.feature_name_style == OcelFeatureNameStyle::kCompatibility;
  if ((include_event_start_ot || include_event_end_ot) && compatibility_names) {
    // Reference event_start_ot/event_end_ot quirk: the endpoint event id of
    // every object is iterated as a string, so the per-type set holds the
    // id's characters (code points); an event scores 1 only when its whole
    // id is one of them. Both blocks are named "@@event_start_<type>", start
    // block first, types in sorted (groupby) order. Objects without
    // relations are skipped (the reference raises KeyError on them).
    auto sorted_types = object_types;
    std::sort(sorted_types.begin(), sorted_types.end());
    const auto add_quirk_block = [&](bool start) {
      for (const auto& type : sorted_types) {
        std::set<std::string> characters;
        for (std::size_t object = 0; object < object_events.size(); ++object) {
          if (object_types_by_index[object] != type || object_events[object].empty())
            continue;
          const auto& id = log.events[start ? object_events[object].front()
                                            : object_events[object].back()].id;
          for (std::size_t at = 0; at < id.size();) {
            const auto lead = static_cast<unsigned char>(id[at]);
            std::size_t length = lead < 0x80 ? 1 : lead >= 0xF0 ? 4
                                 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
            if (at + length > id.size()) length = 1;
            characters.insert(id.substr(at, length));
            at += length;
          }
        }
        std::vector<double> values(log.events.size(), 0.0);
        for (std::size_t row = 0; row < log.events.size(); ++row)
          values[row] = characters.contains(log.events[row].id) ? 1.0 : 0.0;
        builder.add("@@event_start_" + type, std::move(values));
      }
    };
    if (include_event_start_ot) add_quirk_block(true);
    if (include_event_end_ot) add_quirk_block(false);
  } else if (include_event_start_ot || include_event_end_ot) {
    for (const auto& type : object_types) {
      if (include_event_start_ot) {
        std::vector<double> starts(log.events.size(), 0.0);
        std::map<std::size_t, bool> seen_start;
        for (std::size_t object = 0; object < object_events.size(); ++object) {
          if (object_types_by_index[object] != type || object_events[object].empty())
            continue;
          const auto event = object_events[object].front();
          if (seen_start.emplace(object, true).second) starts[event] = 1.0;
        }
        builder.add("@@event_start_" + type, std::move(starts));
      }
      if (include_event_end_ot) {
        std::vector<double> ends(log.events.size(), 0.0);
        std::map<std::size_t, bool> seen_end;
        for (std::size_t object = 0; object < object_events.size(); ++object) {
          if (object_types_by_index[object] != type || object_events[object].empty())
            continue;
          const auto event = object_events[object].back();
          if (seen_end.emplace(object, true).second) ends[event] = 1.0;
        }
        builder.add("@@event_end_" + type, std::move(ends));
      }
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
        if (const auto value = string_value(attr->value)) values.push_back(*value);
    const auto categories = sorted_unique(std::move(values));
    for (const auto& category : categories) {
      std::vector<double> column(log.events.size(), 0.0);
      for (std::size_t row = 0; row < log.events.size(); ++row)
        if (const auto* attr = find_attribute(log.events[row].attributes, key))
          if (const auto value = string_value(attr->value))
            column[row] = *value == category ? 1.0 : 0.0;
      const auto prefix = options.feature_name_style ==
                                  OcelFeatureNameStyle::kCompatibility
                              ? "@@event_attr_value_"
                              : "@@event_str_";
      builder.add(prefix + key + "_" + category, std::move(column));
    }
  }

  const bool include_new_interactions = options.include_new_interactions_features.value_or(
      options.include_interaction_features);
  if (include_new_interactions) {
    std::set<std::pair<std::size_t, std::size_t>> seen_pairs;
    std::vector<double> new_interactions(log.events.size(), 0.0);
    // Reference new_interactions quirk: its "seen" set is never updated, so
    // every event counts all ordered relation pairs with a lexically smaller
    // first object id (relation multiplicity included).
    for (std::size_t row = 0; compatibility_names && row < event_objects.size(); ++row)
      for (const auto left : event_objects[row])
        for (const auto right : event_objects[row])
          if (log.objects[left].id < log.objects[right].id)
            new_interactions[row] += 1.0;
    for (std::size_t row = 0; !compatibility_names && row < event_objects.size(); ++row) {
      std::vector<std::size_t> objects = event_objects[row];
      std::sort(objects.begin(), objects.end());
      objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
      for (std::size_t left = 0; left < objects.size(); ++left)
        for (std::size_t right = left + 1; right < objects.size(); ++right)
          if (seen_pairs.insert({objects[left], objects[right]}).second)
            new_interactions[row] += 1.0;
    }
    builder.add(options.feature_name_style == OcelFeatureNameStyle::kCompatibility
                    ? "@@ev_new_interactions"
                    : "@@event_new_interactions",
                std::move(new_interactions));
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

OcelFeatureOptions complete_object_preset(const OcelFeatureOptions& options) {
  auto result = options;
  // Neighbor and related-object projections aggregate this stable source
  // schema.  It intentionally contains the complete object feature families
  // but none of the projections that would recurse or add selected/native
  // extension columns.
  result.include_activity_features = false;
  result.include_attribute_counts = false;
  result.include_lifecycle_features = true;
  result.include_lifecycle_length_features = true;
  result.include_lifecycle_unique_activity_features = true;
  result.include_lifecycle_duration_features = true;
  result.include_lifecycle_activity_features = true;
  result.include_lifecycle_path_features = false;
  result.include_lifecycle_wip_features = false;
  // Explicit attribute selectors remain part of the source schema.  The
  // broad preset contributes the standard lifecycle/graph families; it does
  // not erase a caller's deliberate numeric or string attribute request.
  result.include_all_object_features = true;
  result.include_degree_centrality_features.reset();
  result.include_interaction_by_type_features.reset();
  result.include_general_interaction_features.reset();
  result.include_general_descendants_features.reset();
  result.include_general_inheritance_features.reset();
  result.include_cobirth_features.reset();
  result.include_codeath_features.reset();
  result.include_interaction_features = false;
  result.interaction_graph_source = OcelInteractionGraphSource::kEventCooccurrence;
  result.include_related_events_features = false;
  result.include_related_activities_features = false;
  result.include_related_objects_features = false;
  result.include_obj_con_in_graph_features = false;
  result.filter_per_type.reset();
  result.max_features = std::numeric_limits<std::size_t>::max();
  return result;
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
  if (options.include_lifecycle_features.value_or(true)) {
    if (options.include_lifecycle_length_features.value_or(true)) {
      std::vector<double> length(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        length[row] = static_cast<double>(lifecycles[row].event_indices.size());
      builder.add("@@object_lifecycle_length", std::move(length));
    }
    if (options.include_lifecycle_unique_activity_features.value_or(true)) {
      std::vector<double> unique(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        unique[row] = static_cast<double>(lifecycles[row].unique_activities.size());
      builder.add("@@object_lifecycle_unq_act", std::move(unique));
    }
    if (options.include_lifecycle_duration_features.value_or(true)) {
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
    if (options.include_lifecycle_activity_features.value_or(true)) {
      for (const auto& activity : activities) {
        std::vector<double> values(log.objects.size(), 0.0);
        for (std::size_t row = 0; row < log.objects.size(); ++row)
          values[row] = static_cast<double>(std::count(
              lifecycles[row].activities.begin(), lifecycles[row].activities.end(), activity));
        builder.add("@@ocel_lif_activity_" + activity, std::move(values));
      }
    }
    if (options.include_lifecycle_path_features.value_or(false)) {
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
    if (options.include_lifecycle_wip_features.value_or(false)) {
      std::vector<double> wip(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        if (lifecycles[row].event_indices.empty()) wip[row] = 1.0;
      // Sort valid lifecycles by their start/end boundary, then count the
      // contiguous suffix whose start is within the current end boundary.
      // The feature is intentionally directional: an earlier lifecycle can
      // see later starts, while a later nested lifecycle does not look back.
      // Restore the input object order after computing the window counts.
      std::vector<std::size_t> ordered;
      ordered.reserve(log.objects.size());
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        if (lifecycles[row].start && lifecycles[row].end)
          ordered.push_back(row);
      std::stable_sort(ordered.begin(), ordered.end(), [&](std::size_t left,
                                                            std::size_t right) {
        if (*lifecycles[left].start != *lifecycles[right].start)
          return *lifecycles[left].start < *lifecycles[right].start;
        return *lifecycles[left].end < *lifecycles[right].end;
      });
      for (std::size_t position = 0; position < ordered.size(); ++position) {
        const auto row = ordered[position];
        const double end = *lifecycles[row].end;
        std::size_t next = position + 1;
        while (next < ordered.size() &&
               *lifecycles[ordered[next]].start <= end)
          ++next;
        wip[row] = static_cast<double>(next - position);
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
        if (const auto value = string_value(attr->value)) values.push_back(*value);
    const auto categories = sorted_unique(std::move(values));
    for (const auto& category : categories) {
      std::vector<double> column(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        if (const auto* attr = find_attribute(log.objects[row].attributes, key))
          if (const auto value = string_value(attr->value))
            column[row] = *value == category ? 1.0 : 0.0;
      const auto prefix = options.feature_name_style ==
                                  OcelFeatureNameStyle::kCompatibility
                              ? "@@object_attr_value_"
                              : "@@object_str_";
      builder.add(prefix + key + "_" + category, std::move(column));
    }
  }

  const bool include_all_object_features =
      options.include_all_object_features.value_or(false);
  const bool include_degree_centrality_features =
      options.include_degree_centrality_features.value_or(
          include_all_object_features || options.include_interaction_features);
  const bool include_interaction_by_type_features =
      options.include_interaction_by_type_features.value_or(
          include_all_object_features);
  if (include_degree_centrality_features ||
      options.include_interaction_features ||
      include_interaction_by_type_features) {
    const auto graph_source = options.interaction_graph_source.value_or(
        OcelInteractionGraphSource::kEventCooccurrence);
    const auto interactions = interaction_edges(
        log, event_objects, graph_source);
    std::vector<std::set<std::size_t>> neighbors(log.objects.size());
    std::set<std::size_t> active_objects;
    for (const auto& objects : event_objects) {
      std::set<std::size_t> unique(objects.begin(), objects.end());
      active_objects.insert(unique.begin(), unique.end());
    }
    if (graph_source == OcelInteractionGraphSource::kEventAndO2O) {
      IndexMap object_ids;
      for (std::size_t row = 0; row < log.objects.size(); ++row)
        object_ids.emplace(log.objects[row].id, row);
      for (const auto& relation : log.o2o) {
        if (const auto source = object_ids.find(relation.source_id);
            source != object_ids.end())
          active_objects.insert(source->second);
        if (const auto target = object_ids.find(relation.target_id);
            target != object_ids.end())
          active_objects.insert(target->second);
      }
    }
    for (const auto& edge : interactions) {
      neighbors[edge.first].insert(edge.second);
      neighbors[edge.second].insert(edge.first);
    }
    if (include_degree_centrality_features) {
      std::vector<double> degree(log.objects.size(), 0.0);
      for (std::size_t row = 0; row < neighbors.size(); ++row)
        if (active_objects.contains(row))
          degree[row] = static_cast<double>(neighbors[row].size());
      // The reference graph holds only objects with an interaction, and
      // scales by the reciprocal of n - 1, so values such as 3 * 0.2 keep
      // its rounding.
      std::size_t nodes = 0;
      for (std::size_t row = 0; row < neighbors.size(); ++row)
        if (active_objects.contains(row) && !neighbors[row].empty()) ++nodes;
      const auto scale = nodes > 1 ? 1.0 / static_cast<double>(nodes - 1) : 1.0;
      for (std::size_t row = 0; row < degree.size(); ++row)
        if (active_objects.contains(row)) degree[row] *= scale;
      builder.add("@@object_degree_centrality", std::move(degree));
    }
    if (include_interaction_by_type_features) {
      for (const auto& type : object_types) {
        std::vector<double> values(log.objects.size(), 0.0);
        for (std::size_t row = 0; row < neighbors.size(); ++row)
          for (const auto neighbor : neighbors[row])
            if (log.objects[neighbor].type == type) values[row] += 1.0;
        const auto prefix = options.feature_name_style ==
                                    OcelFeatureNameStyle::kCompatibility
                                ? "@@object_interaction_graph_"
                                : "@@object_interaction_graph_ot_";
        builder.add(prefix + type,
                    std::move(values));
      }
    }
  }

  const bool include_general_interaction_features =
      options.include_general_interaction_features.value_or(
          include_all_object_features);
  const bool include_general_descendants_features =
      options.include_general_descendants_features.value_or(
          include_all_object_features);
  const bool include_general_inheritance_features =
      options.include_general_inheritance_features.value_or(
          include_all_object_features);
  const bool include_cobirth_features =
      options.include_cobirth_features.value_or(include_all_object_features);
  const bool include_codeath_features =
      options.include_codeath_features.value_or(include_all_object_features);
  const bool include_general_graph_features =
      include_general_interaction_features || include_general_descendants_features ||
      include_general_inheritance_features || include_cobirth_features ||
      include_codeath_features;
  if (include_general_graph_features) {
    const auto graphs = build_object_graph_families(log, event_objects);

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
    if (include_general_interaction_features)
      builder.add("@@object_general_interaction_graph",
                  undirected_degrees(graphs.interaction));
    if (include_general_descendants_features) {
      add_directed_degrees(graphs.descendants, true,
                           "@@object_general_descendants_graph_ascendants");
      add_directed_degrees(graphs.descendants, false,
                           "@@object_general_descendants_graph_descendants");
    }
    if (include_general_inheritance_features) {
      add_directed_degrees(graphs.inheritance, true,
                           "@@object_general_inheritance_graph_ascendants");
      add_directed_degrees(graphs.inheritance, false,
                           "@@object_general_inheritance_graph_descendants");
    }
    if (include_cobirth_features)
      builder.add("@@object_cobirth", undirected_degrees(graphs.cobirth));
    if (include_codeath_features)
      builder.add("@@object_codeath", undirected_degrees(graphs.codeath));
  }

  if (options.include_related_events_features ||
      options.include_related_activities_features) {
    // Build the event feature vectors once, then project them through the
    // relation stream for each object.  The source table uses the same option
    // groups as the standalone event variant, including selected attributes.
    TableBuilder event_builder(log.events.size(),
                               std::numeric_limits<std::size_t>::max());
    add_event_columns(event_builder, log, options, activities, object_types,
                      event_objects, object_events, times, [&]() {
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
      related_activities = ordered_unique(std::move(related_activities));
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
    std::set<ObjectPair> selected_graph;
    switch (options.object_neighbor_graph) {
      case OcelObjectNeighborGraph::kInteraction:
        selected_graph = interaction_edges(
            log, event_objects,
            options.interaction_graph_source.value_or(
                OcelInteractionGraphSource::kEventCooccurrence));
        break;
      case OcelObjectNeighborGraph::kCobirth:
        selected_graph = build_object_graph_families(log, event_objects).cobirth;
        break;
      case OcelObjectNeighborGraph::kCodeath:
        selected_graph = build_object_graph_families(log, event_objects).codeath;
        break;
      default:
        throw std::invalid_argument("OCEL object_neighbor_graph is invalid");
    }
    std::vector<std::set<std::size_t>> neighbors(log.objects.size());
    for (const auto& [left, right] : selected_graph) {
      neighbors[left].insert(right);
      neighbors[right].insert(left);
    }

    // Obtain the complete ordinary object feature set, with all derived
    // relation/neighbor controls disabled to avoid self-referential columns.
    auto base_options = complete_object_preset(options);
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

  // Build the complete deterministic object schema used by the standalone
  // reference-compatible object preset. The aggregation itself is disabled
  // so the source schema cannot recurse.
  auto base_options = complete_object_preset(options);
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
  const bool include_length = options.include_prefix_length_features.value_or(
      options.include_prefix_features);
  const bool include_timediff = options.include_prefix_timediff_features.value_or(
      options.include_prefix_features);
  const bool include_one_hot = options.include_prefix_one_hot_features.value_or(
      options.include_prefix_features);
  if (!include_length && !include_timediff && !include_one_hot) return;

  std::vector<std::vector<std::size_t>> seen_events(log.objects.size());
  std::vector<std::vector<std::size_t>> prefixes(log.relations.size());
  std::map<std::pair<std::size_t, std::size_t>, std::vector<std::size_t>>
      duplicate_prefixes;
  for (std::size_t row = 0; row < log.relations.size(); ++row) {
    const auto event = event_index.find(log.relations[row].event_id);
    const auto object = object_index.find(log.relations[row].object_id);
    if (event == event_index.end() || object == object_index.end())
      throw std::invalid_argument("OCEL feature prefix relation references an unknown id");
    auto& seen = seen_events[object->second];
    const auto identity = std::make_pair(event->second, object->second);
    if (const auto duplicate = duplicate_prefixes.find(identity);
        duplicate != duplicate_prefixes.end()) {
      // Duplicate relation rows represent the same event/object identity. A
      // duplicate keeps the canonical prefix snapshot while the first row is
      // still the only row that advances the object-local history.
      prefixes[row] = duplicate->second;
      continue;
    }
    prefixes[row] = seen;
    duplicate_prefixes.emplace(identity, prefixes[row]);
    if (std::find(seen.begin(), seen.end(), event->second) == seen.end())
      seen.push_back(event->second);
  }

  if (include_length) {
    std::vector<double> values(log.relations.size(), 0.0);
    for (std::size_t row = 0; row < values.size(); ++row)
      values[row] = static_cast<double>(prefixes[row].size());
    builder.add("@@ev_obj_pref_length", std::move(values));
  }
  if (include_timediff) {
    std::vector<double> values(log.relations.size(), 0.0);
    for (std::size_t row = 0; row < values.size(); ++row) {
      const auto event = event_index.at(log.relations[row].event_id);
      if (!prefixes[row].empty() && times[event] &&
          times[prefixes[row].front()])
        values[row] = *times[event] - *times[prefixes[row].front()];
    }
    builder.add("@@ev_obj_pref_timediff", std::move(values));
  }
  if (include_one_hot) {
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

const char* ocel_feature_variant_name(OcelFeatureVariant variant) {
  return variant_name(variant);
}

OcelFeatureTable extract_ocel_features(const Ocel& log,
                                       OcelFeatureVariant variant,
                                       const OcelFeatureOptions& options) {
  if (options.max_features == 0) throw std::invalid_argument("OCEL max_features must be positive");
  if (options.feature_name_style == OcelFeatureNameStyle::kCompatibility &&
      variant == OcelFeatureVariant::kEventsObjects) {
    throw std::invalid_argument(
        "OCEL compatibility feature names apply only to events or objects variants");
  }
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
  if (options.interaction_graph_source.has_value() &&
      *options.interaction_graph_source !=
          OcelInteractionGraphSource::kEventAndO2O &&
      *options.interaction_graph_source !=
          OcelInteractionGraphSource::kEventCooccurrence)
    throw std::invalid_argument("OCEL interaction_graph_source is invalid");
  if (options.interaction_graph_source.has_value() &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context)
    throw std::invalid_argument(
        "OCEL interaction_graph_source applies only to objects or related-object events");
  const bool event_control_supplied =
      options.include_event_num_rel_objs_features.has_value() ||
      options.include_event_num_rel_objs_type_features.has_value() ||
      options.include_event_start_ot_features.has_value() ||
      options.include_event_end_ot_features.has_value() ||
      options.include_new_interactions_features.has_value();
  if (event_control_supplied && variant != OcelFeatureVariant::kEvents) {
    throw std::invalid_argument(
        "OCEL independent event feature controls apply only to the events variant");
  }
  const bool object_graph_control_supplied =
      options.include_interaction_by_type_features.has_value() ||
      options.include_general_interaction_features.has_value() ||
      options.include_general_descendants_features.has_value() ||
      options.include_general_inheritance_features.has_value() ||
      options.include_cobirth_features.has_value() ||
      options.include_codeath_features.has_value();
  if (object_graph_control_supplied &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL object graph feature controls apply only to the objects variant");
  }
  if (options.include_degree_centrality_features.has_value() &&
      variant != OcelFeatureVariant::kObjects && !related_object_event_context) {
    throw std::invalid_argument(
        "OCEL degree centrality features apply only to the objects variant");
  }
  if (options.include_all_object_features.has_value() &&
      variant != OcelFeatureVariant::kObjects) {
    throw std::invalid_argument(
        "OCEL all-object features apply only to the objects variant");
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
  const bool pointwise_control_supplied =
      options.include_event_pointwise_features.has_value() ||
      options.include_object_pointwise_features.has_value();
  if (pointwise_control_supplied &&
      variant != OcelFeatureVariant::kEventsObjects) {
    throw std::invalid_argument(
        "OCEL pointwise feature controls apply only to the events_objects variant");
  }
  const bool prefix_control_disabled =
      !options.include_prefix_features ||
      options.include_prefix_length_features.has_value() ||
      options.include_prefix_timediff_features.has_value() ||
      options.include_prefix_one_hot_features.has_value();
  if (variant != OcelFeatureVariant::kEventsObjects && prefix_control_disabled)
    throw std::invalid_argument(
        "OCEL prefix features apply only to the events_objects variant");
  const bool lifecycle_object_control_disabled =
      options.include_lifecycle_features.has_value() ||
      options.include_lifecycle_length_features.has_value() ||
      options.include_lifecycle_unique_activity_features.has_value() ||
      options.include_lifecycle_duration_features.has_value() ||
      options.include_lifecycle_activity_features.has_value() ||
      options.include_lifecycle_path_features.has_value() ||
      options.include_lifecycle_wip_features.has_value();
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
  if (options.include_timestamp_features) {
    for (std::size_t event = 0; event < log.events.size(); ++event)
      if (!log.events[event].timestamp.empty() && !times[event])
        throw std::invalid_argument("OCEL feature timestamp is invalid");
  }
  const auto activities = activity_names(log);
  const auto object_types = object_type_names(log);
  std::vector<std::string> object_types_by_index;
  object_types_by_index.reserve(log.objects.size());
  for (const auto& object : log.objects) object_types_by_index.push_back(object.type);
  const auto lifecycles = make_lifecycles(log, object_events, times);

  if (variant == OcelFeatureVariant::kEvents) {
    TableBuilder builder(log.events.size(), options.max_features);
    add_event_columns(builder, log, options, activities, object_types, event_objects,
                      object_events, times, object_types_by_index);
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

  TableBuilder builder(row_count, options.max_features);
  const auto project = [&](const OcelFeatureTable& source, bool project_event) {
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
  if (options.include_event_pointwise_features.value_or(true)) {
    // The standalone source is built only when its projection is selected;
    // this keeps disabled pointwise families out of feature-cap accounting.
    TableBuilder event_builder(log.events.size(), options.max_features);
    auto events_objects_options = options;
    events_objects_options.include_related_objects_features = false;
    add_event_columns(event_builder, log, events_objects_options, activities,
                      object_types, event_objects, object_events, times,
                      object_types_by_index);
    std::vector<std::string> event_ids;
    event_ids.reserve(log.events.size());
    for (const auto& event : log.events) event_ids.push_back(event.id);
    const auto event_table = std::move(event_builder).finish(
        OcelFeatureVariant::kEvents, std::move(event_ids), {});
    project(event_table, true);
  }
  if (options.include_object_pointwise_features.value_or(true)) {
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
    project(object_table, false);
  }
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
  (void)variant_name(table.variant);
  if (table.row_ids.size() != table.values.size() ||
      (!table.row_types.empty() && table.row_types.size() != table.row_ids.size()))
    throw std::invalid_argument("OCEL feature table row metadata is inconsistent");
  std::set<std::string> row_ids;
  for (const auto& row_id : table.row_ids)
    if (row_id.empty() || !row_ids.insert(row_id).second)
      throw std::invalid_argument("OCEL feature table row ids must be unique and non-empty");
  // Names may repeat: the reference-compatible event table names both the
  // start and the end endpoint blocks "@@event_start_<type>". Columns are
  // positional in this envelope.
  for (const auto& feature_name : table.feature_names)
    if (feature_name.empty())
      throw std::invalid_argument("OCEL feature table names must be non-empty");
  for (const auto& row : table.values) {
    if (row.size() != table.feature_names.size())
      throw std::invalid_argument("OCEL feature table columns are inconsistent");
    if (std::any_of(row.begin(), row.end(),
                    [](double value) { return !std::isfinite(value); }))
      throw std::invalid_argument("OCEL feature table values must be finite");
  }
  std::ostringstream out;
  out << "{\"family\":\"OCEL_FEATURES\",\"variant\":\""
      << variant_name(table.variant) << "\",\"row_count\":"
      << table.row_ids.size() << ",\"feature_count\":"
      << table.feature_names.size() << ",\"row_ids\":[";
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
