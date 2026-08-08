#include "minera/ocel_statistics.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace minera {
namespace {

struct OcelLookups {
  std::unordered_map<std::string, std::string> event_activities;
  std::unordered_map<std::string, std::string> object_types;
};

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

const char* ocel_statistics_variant_name(
    OcelStatisticsVariant variant) noexcept {
  switch (variant) {
    case OcelStatisticsVariant::kObjectTypeActivities:
      return "OBJECT_TYPE_ACTIVITIES";
    case OcelStatisticsVariant::kObjectsOtCount:
      return "OBJECTS_OT_COUNT";
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

std::string serialize_ocel_statistics_json(const Ocel& log,
                                           OcelStatisticsVariant variant) {
  switch (variant) {
    case OcelStatisticsVariant::kObjectTypeActivities:
      return serialize_activities(object_type_activities(log));
    case OcelStatisticsVariant::kObjectsOtCount:
      return serialize_counts(objects_ot_count(log));
  }
  throw std::invalid_argument("unknown OCEL statistics variant");
}

}  // namespace minera
