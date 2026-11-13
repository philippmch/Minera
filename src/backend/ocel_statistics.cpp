#include "minera/ocel_statistics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <set>
#include <unordered_map>
#include <vector>

#include "minera/ocdfg_edge_performance.hpp"
#include "minera/timestamp.hpp"
#include "business_hours.hpp"

namespace minera {
namespace {

struct OcelLookups {
  std::unordered_map<std::string, std::string> event_activities;
  std::unordered_map<std::string, std::string> object_types;
};

constexpr double kWeekSeconds = 7.0 * 86400.0;

std::vector<BusinessHourSlot> default_business_slots() {
  std::vector<BusinessHourSlot> slots;
  slots.reserve(5);
  for (int day = 0; day < 5; ++day) {
    const double start = static_cast<double>(day) * 86400.0 + 7.0 * 3600.0;
    slots.push_back({start, start + 10.0 * 3600.0});
  }
  return slots;
}

std::vector<BusinessHourSlot> normalized_business_slots(
    const OcelEdgeAssociationOptions& options) {
  auto slots = options.business_hour_slots.empty()
                   ? default_business_slots()
                   : options.business_hour_slots;
  for (const auto& slot : slots) {
    if (!std::isfinite(slot.start_seconds) ||
        !std::isfinite(slot.end_seconds) || slot.start_seconds < 0.0 ||
        slot.start_seconds >= slot.end_seconds ||
        slot.end_seconds > kWeekSeconds) {
      throw std::invalid_argument(
          "OCEL edge business-hour slots must satisfy 0 <= start < end <= 604800");
    }
  }
  std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.start_seconds != rhs.start_seconds)
      return lhs.start_seconds < rhs.start_seconds;
    return lhs.end_seconds < rhs.end_seconds;
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& slot : slots) {
    if (!merged.empty() && slot.start_seconds <= merged.back().end_seconds) {
      merged.back().end_seconds =
          std::max(merged.back().end_seconds, slot.end_seconds);
    } else {
      merged.push_back(slot);
    }
  }
  return merged;
}

std::optional<double> local_wall_instant(std::string_view text) {
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end > 0 &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
  if (timezone != std::string_view::npos) end = timezone;
  return parse_timestamp(text.substr(0, end));
}

// pm4py's BusinessHours on microsecond wall clocks
// (detail::business_elapsed_seconds_microseconds); an epoch double holds its
// microsecond exactly enough for rounding to recover it (review AY1).
double business_elapsed(double source, double target,
                        const std::vector<BusinessHourSlot>& slots) {
  return detail::business_elapsed_seconds_microseconds(std::llround(source * 1e6),
                                                       std::llround(target * 1e6), slots);
}

OcelLookups validate_and_index(const Ocel& log) {
  OcelLookups lookups;
  lookups.event_activities.reserve(log.events.size());
  for (const auto& event : log.events) {
    if (!lookups.event_activities.emplace(event.id, event.activity).second) {
      throw std::invalid_argument("OCEL log contains duplicate event id");
    }
  }

  lookups.object_types.reserve(log.objects.size());
  for (const auto& object : log.objects) {
    if (!lookups.object_types.emplace(object.id, object.type).second) {
      throw std::invalid_argument("OCEL log contains duplicate object id");
    }
  }

  for (const auto& relation : log.relations) {
    if (!lookups.event_activities.contains(relation.event_id) ||
        !lookups.object_types.contains(relation.object_id)) {
      throw std::invalid_argument(
          "OCEL relation references an unknown event or object id");
    }
  }
  return lookups;
}

struct AssociationRow {
  std::string event_id;
  std::string object_id;
};

std::vector<AssociationRow> filtered_rows(
    const Ocel& log, OcelAssociationPrefilter prefilter) {
  (void)validate_and_index(log);
  std::vector<AssociationRow> rows;
  rows.reserve(log.relations.size());
  if (prefilter == OcelAssociationPrefilter::kNone) {
    for (const auto& relation : log.relations)
      rows.push_back({relation.event_id, relation.object_id});
    return rows;
  }

  // Relation order is intentional: start/end are the first/last relation
  // row for each object, independent of event table order or timestamps.
  std::map<std::string, std::size_t> selected_index;
  for (std::size_t i = 0; i < log.relations.size(); ++i) {
    const auto& relation = log.relations[i];
    if (prefilter == OcelAssociationPrefilter::kStart)
      selected_index.emplace(relation.object_id, i);
    else
      selected_index[relation.object_id] = i;
  }
  std::set<std::size_t> indexes;
  for (const auto& [object, index] : selected_index) {
    (void)object;
    indexes.insert(index);
  }
  for (const auto index : indexes) {
    const auto& relation = log.relations[index];
    rows.push_back({relation.event_id, relation.object_id});
  }
  return rows;
}

OcelActivityAssociations make_activity_associations(
    const Ocel& log, OcelAssociationPrefilter prefilter) {
  const auto lookups = validate_and_index(log);
  OcelActivityAssociations result;
  for (const auto& row : filtered_rows(log, prefilter)) {
    result[lookups.event_activities.at(row.event_id)].insert(
        {row.event_id, row.object_id});
  }
  return result;
}

void json_string(std::ostringstream& out, std::string_view value) {
  out << '"';
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          constexpr char digits[] = "0123456789abcdef";
          out << "\\u00" << digits[ch >> 4] << digits[ch & 0x0f];
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  out << '"';
}

std::string serialize_activities(const OcelObjectTypeActivities& activities) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_STATISTICS\",\"variant\":\""
      << "OBJECT_TYPE_ACTIVITIES\",\"object_type_activities\":{";
  bool first_type = true;
  for (const auto& [object_type, values] : activities) {
    if (!first_type) out << ',';
    first_type = false;
    json_string(out, object_type);
    out << ":[";
    bool first_activity = true;
    for (const auto& activity : values) {
      if (!first_activity) out << ',';
      first_activity = false;
      json_string(out, activity);
    }
    out << ']';
  }
  out << "}}";
  return out.str();
}

std::string serialize_counts(const OcelObjectsOtCount& counts) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_STATISTICS\",\"variant\":\""
      << "OBJECTS_OT_COUNT\",\"objects_ot_count\":{";
  bool first_event = true;
  for (const auto& [event_id, by_type] : counts) {
    if (!first_event) out << ',';
    first_event = false;
    json_string(out, event_id);
    out << ":{";
    bool first_type = true;
    for (const auto& [object_type, count] : by_type) {
      if (!first_type) out << ',';
      first_type = false;
      json_string(out, object_type);
      out << ':' << count;
    }
    out << '}';
  }
  out << "}}";
  return out.str();
}

void write_association_body(std::ostringstream& out,
                            const OcelActivityAssociations& associations) {
  out << '{';
  bool first_activity = true;
  for (const auto& [activity, pairs] : associations) {
    if (!first_activity) out << ',';
    first_activity = false;
    json_string(out, activity);
    out << ':';
    out << '[';
    bool first_pair = true;
    for (const auto& [event, object] : pairs) {
      if (!first_pair) out << ',';
      first_pair = false;
      out << '[';
      json_string(out, event);
      out << ',';
      json_string(out, object);
      out << ']';
    }
    out << ']';
  }
  out << '}';
}

std::string serialize_associations(const OcelActivityAssociations& associations) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_STATISTICS\",\"variant\":\"ACTIVITY_ASSOCIATIONS\",\"associations\":{";
  // The opening envelope already emits the map's opening brace.
  bool first_activity = true;
  for (const auto& [activity, pairs] : associations) {
      if (!first_activity) out << ',';
      first_activity = false;
      json_string(out, activity);
      out << ':';
      out << '[';
    bool first_pair = true;
    for (const auto& [event, object] : pairs) {
      if (!first_pair) out << ',';
      first_pair = false;
      out << '['; json_string(out, event); out << ','; json_string(out, object); out << ']';
    }
    out << ']';
  }
  out << "}}";
  return out.str();
}

std::string serialize_object_type_associations(
    const OcelObjectTypeActivityAssociations& associations) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_STATISTICS\",\"variant\":\"OBJECT_TYPE_ACTIVITY_ASSOCIATIONS\",\"associations\":{";
  bool first_type = true;
  for (const auto& [type, activities] : associations) {
    if (!first_type) out << ',';
    first_type = false;
    json_string(out, type);
    out << ':';
    write_association_body(out, activities);
  }
  out << "}}";
  return out.str();
}

std::string serialize_edges(const OcelEdgeAssociations& associations) {
  std::ostringstream out;
  // Durations print in full (the stream's default six digits turned
  // 1209601.123449 s into 1.2096e+06).
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  out << "{\"family\":\"OCEL_STATISTICS\",\"variant\":\"EDGE_ASSOCIATIONS\",\"edges\":{";
  bool first_type = true;
  for (const auto& [type, edges] : associations) {
    if (!first_type) out << ',';
    first_type = false;
    json_string(out, type);
    out << ':' << '{';
    bool first_edge = true;
    // "source->target" keys can repeat when activity names hold "->"; a
    // repeat gets "#2", "#3", ..., and every edge names its endpoints.
    std::set<std::string> used_keys;
    for (const auto& [activities, metric] : edges) {
      if (!first_edge) out << ',';
      first_edge = false;
      const std::string base = activities.first + "->" + activities.second;
      std::string key = base;
      for (std::size_t n = 2; !used_keys.insert(key).second; ++n)
        key = base + "#" + std::to_string(n);
      json_string(out, key);
      out << ':';
      out << "{\"event_couples\":[";
      for (std::size_t i = 0; i < metric.event_couples.size(); ++i) {
        if (i) out << ',';
        out << '['; json_string(out, metric.event_couples[i].first); out << ',';
        json_string(out, metric.event_couples[i].second); out << ']';
      }
      out << "],\"unique_objects\":[";
      for (std::size_t i = 0; i < metric.unique_objects.size(); ++i) {
        if (i) out << ',';
        json_string(out, metric.unique_objects[i]);
      }
      out << "],\"event_objects\":[";
      for (std::size_t i = 0; i < metric.event_objects.size(); ++i) {
        if (i) out << ',';
        out << '['; json_string(out, std::get<0>(metric.event_objects[i])); out << ',';
        json_string(out, std::get<1>(metric.event_objects[i])); out << ',';
        json_string(out, std::get<2>(metric.event_objects[i])); out << ']';
      }
      out << "],\"total_objects\":[";
      const auto& total_objects = metric.total_objects.empty()
                                      ? metric.event_objects
                                      : metric.total_objects;
      for (std::size_t i = 0; i < total_objects.size(); ++i) {
        if (i) out << ',';
        out << '['; json_string(out, std::get<0>(total_objects[i])); out << ',';
        json_string(out, std::get<1>(total_objects[i])); out << ',';
        json_string(out, std::get<2>(total_objects[i])); out << ']';
      }
      out << "],\"performance\":[";
      for (std::size_t i = 0; i < metric.performance.size(); ++i) {
        if (i) out << ',';
        out << metric.performance[i];
      }
      out << "],\"performance_event_couples\":[";
      for (std::size_t i = 0; i < metric.performance_event_couples.size(); ++i) {
        if (i) out << ',';
        out << metric.performance_event_couples[i];
      }
      out << "],\"performance_total_objects\":[";
      for (std::size_t i = 0; i < metric.performance_total_objects.size(); ++i) {
        if (i) out << ',';
        out << metric.performance_total_objects[i];
      }
      out << "],\"source_activity\":";
      json_string(out, activities.first);
      out << ",\"target_activity\":";
      json_string(out, activities.second);
      out << '}';
    }
    out << '}';
  }
  out << "}}";
  return out.str();
}

}  // namespace

OcelObjectTypeActivities object_type_activities(
    const Ocel& log, const OcelObjectTypeActivityOptions&) {
  const auto lookups = validate_and_index(log);
  OcelObjectTypeActivities result;
  for (const auto& relation : log.relations) {
    const auto event_it = lookups.event_activities.find(relation.event_id);
    const auto object_it = lookups.object_types.find(relation.object_id);
    // Validation above guarantees both lookups succeed.
    result[object_it->second].insert(event_it->second);
  }
  return result;
}

OcelObjectsOtCount objects_ot_count(const Ocel& log) {
  const auto lookups = validate_and_index(log);
  OcelObjectsOtCount result;
  for (const auto& relation : log.relations) {
    const auto object_it = lookups.object_types.find(relation.object_id);
    // Validation above guarantees the object lookup succeeds.
    ++result[relation.event_id][object_it->second];
  }
  return result;
}

OcelActivityAssociations activity_associations(
    const Ocel& log, OcelAssociationPrefilter prefilter) {
  return make_activity_associations(log, prefilter);
}

OcelObjectTypeActivityAssociations object_type_activity_associations(
    const Ocel& log, OcelAssociationPrefilter prefilter) {
  const auto lookups = validate_and_index(log);
  OcelObjectTypeActivityAssociations result;
  for (const auto& row : filtered_rows(log, prefilter)) {
    result[lookups.object_types.at(row.object_id)]
        [lookups.event_activities.at(row.event_id)]
            .insert({row.event_id, row.object_id});
  }
  return result;
}

std::set<std::string> association_event_ids(
    const OcelActivityAssociations& associations) {
  std::set<std::string> result;
  for (const auto& [activity, pairs] : associations) {
    (void)activity;
    for (const auto& pair : pairs) result.insert(pair.first);
  }
  return result;
}

std::set<std::string> association_object_ids(
    const OcelActivityAssociations& associations) {
  std::set<std::string> result;
  for (const auto& [activity, pairs] : associations) {
    (void)activity;
    for (const auto& pair : pairs) result.insert(pair.second);
  }
  return result;
}

std::size_t association_pair_count(
    const OcelActivityAssociations& associations) {
  std::size_t result = 0;
  for (const auto& [activity, pairs] : associations) {
    (void)activity;
    result += pairs.size();
  }
  return result;
}

OcelAssociationAggregates aggregate_associations(
    const OcelActivityAssociations& associations) {
  return {association_event_ids(associations), association_object_ids(associations),
          association_pair_count(associations)};
}

OcelEdgeAssociations edge_associations(
    const Ocel& log, const OcelEdgeAssociationOptions& options) {
  const auto lookups = validate_and_index(log);
  const auto business_slots =
      options.use_business_hours || !options.business_hour_slots.empty()
          ? normalized_business_slots(options)
          : std::vector<BusinessHourSlot>{};
  // Relation rows per object and event: a repeated event_object row repeats
  // the event in the object's sequence (pm4py keeps the rows, so the
  // repeats sit next to each other and give self-edges).
  std::map<std::string, std::map<std::string, std::size_t>> object_events;
  for (const auto& relation : log.relations)
    ++object_events[relation.object_id][relation.event_id];
  std::unordered_map<std::string, const OcelEvent*> event_by_id;
  event_by_id.reserve(log.events.size());
  for (const auto& event : log.events) event_by_id.emplace(event.id, &event);

  OcelEdgeAssociations result;
  // Event table order is the canonical source order.
  for (const auto& [object_id, related_events] : object_events) {
    std::vector<const OcelEvent*> sequence;
    for (const auto& event : log.events) {
      const auto found = related_events.find(event.id);
      if (found != related_events.end()) sequence.insert(sequence.end(), found->second, &event);
    }
    if (sequence.size() < 2) continue;
    const auto object_type = lookups.object_types.at(object_id);
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      const auto& source = *sequence[i - 1];
      const auto& target = *sequence[i];
      auto& metric = result[object_type][{source.activity, target.activity}];
      metric.event_couples.emplace_back(source.id, target.id);
      metric.unique_objects.push_back(object_id);
      metric.event_objects.emplace_back(source.id, target.id, object_id);
      metric.total_objects.emplace_back(source.id, target.id, object_id);
    }
  }
  for (auto& [type, metrics] : result) {
    (void)type;
    for (auto& [activities, metric] : metrics) {
      (void)activities;
      std::sort(metric.event_couples.begin(), metric.event_couples.end());
      metric.event_couples.erase(
          std::unique(metric.event_couples.begin(), metric.event_couples.end()),
          metric.event_couples.end());
      std::sort(metric.unique_objects.begin(), metric.unique_objects.end());
      metric.unique_objects.erase(
          std::unique(metric.unique_objects.begin(), metric.unique_objects.end()),
          metric.unique_objects.end());
      std::sort(metric.event_objects.begin(), metric.event_objects.end());
      metric.event_objects.erase(
          std::unique(metric.event_objects.begin(), metric.event_objects.end()),
          metric.event_objects.end());
      std::sort(metric.total_objects.begin(), metric.total_objects.end());
      metric.total_objects.erase(
          std::unique(metric.total_objects.begin(), metric.total_objects.end()),
          metric.total_objects.end());
      metric.performance.clear();
      metric.performance_event_couples.clear();
      metric.performance_total_objects.clear();
      const auto duration = [&](const std::string& source_id,
                                const std::string& target_id)
          -> std::optional<double> {
        const auto source_it = event_by_id.find(source_id);
        const auto target_it = event_by_id.find(target_id);
        if (source_it == event_by_id.end() || target_it == event_by_id.end())
          return std::nullopt;
        const auto from = options.use_business_hours
                              ? local_wall_instant(source_it->second->timestamp)
                              : parse_timestamp(source_it->second->timestamp);
        const auto to = options.use_business_hours
                            ? local_wall_instant(target_it->second->timestamp)
                            : parse_timestamp(target_it->second->timestamp);
        if (!from || !to) return std::nullopt;
        if (!options.use_business_hours) {
          // As the reference's timedelta: whole seconds exact, then the
          // microsecond fraction (an epoch double difference loses
          // microseconds at present-day magnitudes).
          const auto exact_from = parse_timestamp_exact(source_it->second->timestamp);
          const auto exact_to = parse_timestamp_exact(target_it->second->timestamp);
          if (exact_from && exact_to) return ocdfg_elapsed_seconds(*exact_from, *exact_to);
          return *to - *from;
        }
        return business_elapsed(*from, *to, business_slots);
      };
      for (const auto& couple : metric.event_couples) {
        if (const auto value = duration(couple.first, couple.second))
          metric.performance_event_couples.push_back(*value);
      }
      for (const auto& triple : metric.total_objects) {
        if (const auto value = duration(std::get<0>(triple),
                                        std::get<1>(triple)))
          metric.performance_total_objects.push_back(*value);
      }
      // Keep the historic field as the total-object projection.
      metric.performance = metric.performance_total_objects;
      std::sort(metric.performance_event_couples.begin(),
                metric.performance_event_couples.end());
      std::sort(metric.performance_total_objects.begin(),
                metric.performance_total_objects.end());
      std::sort(metric.performance.begin(), metric.performance.end());
    }
  }
  return result;
}

const char* ocel_statistics_variant_name(
    OcelStatisticsVariant variant) noexcept {
  switch (variant) {
    case OcelStatisticsVariant::kObjectTypeActivities:
      return "OBJECT_TYPE_ACTIVITIES";
    case OcelStatisticsVariant::kObjectsOtCount:
      return "OBJECTS_OT_COUNT";
    case OcelStatisticsVariant::kActivityAssociations:
      return "ACTIVITY_ASSOCIATIONS";
    case OcelStatisticsVariant::kObjectTypeActivityAssociations:
      return "OBJECT_TYPE_ACTIVITY_ASSOCIATIONS";
    case OcelStatisticsVariant::kEdgeAssociations:
      return "EDGE_ASSOCIATIONS";
  }
  return "UNKNOWN";
}

std::string serialize_ocel_statistics_json(
    const OcelObjectTypeActivities& activities) {
  return serialize_activities(activities);
}

std::string serialize_ocel_statistics_json(const OcelObjectsOtCount& counts) {
  return serialize_counts(counts);
}

std::string serialize_ocel_statistics_json(
    const OcelActivityAssociations& associations) {
  return serialize_associations(associations);
}

std::string serialize_ocel_statistics_json(
    const OcelObjectTypeActivityAssociations& associations) {
  return serialize_object_type_associations(associations);
}

std::string serialize_ocel_statistics_json(const OcelEdgeAssociations& associations) {
  return serialize_edges(associations);
}

std::string serialize_ocel_statistics_json(const Ocel& log,
                                           OcelStatisticsVariant variant) {
  switch (variant) {
    case OcelStatisticsVariant::kObjectTypeActivities:
      return serialize_activities(object_type_activities(log));
    case OcelStatisticsVariant::kObjectsOtCount:
      return serialize_counts(objects_ot_count(log));
    case OcelStatisticsVariant::kActivityAssociations:
      return serialize_associations(activity_associations(log));
    case OcelStatisticsVariant::kObjectTypeActivityAssociations:
      return serialize_object_type_associations(
          object_type_activity_associations(log));
    case OcelStatisticsVariant::kEdgeAssociations:
      return serialize_edges(edge_associations(log));
  }
  throw std::invalid_argument("unknown OCEL statistics variant");
}

std::string serialize_ocel_statistics_json(
    const Ocel& log, OcelStatisticsVariant variant,
    const OcelEdgeAssociationOptions& edge_options) {
  if (variant != OcelStatisticsVariant::kEdgeAssociations)
    return serialize_ocel_statistics_json(log, variant);
  return serialize_edges(edge_associations(log, edge_options));
}

}  // namespace minera
