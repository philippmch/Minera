#include "minera/ocel_filter.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
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

// The reference selects with pandas isin, which compares like Python ==:
// booleans, integers and floats meet numerically (1 == 1.0 == True), while
// strings only equal strings and a missing cell (NaN) or None matches nothing
// but the same kind.
bool ocel_python_equal(const OcelValue& left, const OcelValue& right) {
  if (left.index() == right.index()) return left == right;
  const auto number = [](const OcelValue& value) -> std::optional<long double> {
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? 1.0L : 0.0L;
    if (const auto* integer = std::get_if<std::int64_t>(&value))
      return static_cast<long double>(*integer);
    if (const auto* real = std::get_if<double>(&value)) return static_cast<long double>(*real);
    return std::nullopt;
  };
  const auto a = number(left), b = number(right);
  if (!a || !b) return false;
  // An int64 and a double are equal only when the double is that exact
  // integer; long double holds every int64 exactly on the supported targets,
  // and the explicit check keeps the comparison exact elsewhere too.
  const auto* integer = std::get_if<std::int64_t>(&left);
  const auto* real = std::get_if<double>(&right);
  if (!integer) { integer = std::get_if<std::int64_t>(&right); real = std::get_if<double>(&left); }
  if (integer && real)
    return std::isfinite(*real) && *real >= -0x1p63 && *real < 0x1p63 &&
           std::trunc(*real) == *real && *integer == static_cast<std::int64_t>(*real);
  return *a == *b;
}

bool contains_value(const std::vector<OcelValue>& values,
                    const OcelValue& candidate) {
  return std::any_of(values.begin(), values.end(), [&](const OcelValue& value) {
    return ocel_python_equal(value, candidate);
  });
}

// The pandas dtype of the reference frame's column for an attribute, as far
// as it changes how a large integer compares (review Y1-2): a column of
// integers only is int64, and isin casts it to float64 against a float
// value (2^53 + 1 then equals 2.0^53); a missing cell (a record without the
// key, or null) or a float makes it float64, which holds every integer
// rounded to a double; a string or boolean makes it an object column, which
// compares like Python ==.
enum class OcelNumericColumn { kObject, kInteger, kFloat };

template <class Records>
OcelNumericColumn ocel_numeric_column(const Records& records, std::string_view key) {
  bool missing = false, real = false;
  for (const auto& record : records) {
    const OcelValue* value = nullptr;
    for (const auto& attribute : record.attributes)
      if (attribute.key == key) {
        value = &attribute.value;
        break;
      }
    if (!value || std::holds_alternative<std::nullptr_t>(*value)) missing = true;
    else if (std::holds_alternative<double>(*value)) real = true;
    else if (!std::holds_alternative<std::int64_t>(*value)) return OcelNumericColumn::kObject;
  }
  return missing || real ? OcelNumericColumn::kFloat : OcelNumericColumn::kInteger;
}

bool ocel_cell_equal(const OcelValue& cell, const OcelValue& value, OcelNumericColumn column,
                     bool object_list) {
  if (const auto* integer = std::get_if<std::int64_t>(&cell)) {
    if (column == OcelNumericColumn::kFloat)
      return ocel_python_equal(value, OcelValue(static_cast<double>(*integer)));
    if (column == OcelNumericColumn::kInteger && !object_list)
      if (const auto* real = std::get_if<double>(&value))
        return static_cast<double>(*integer) == *real;
  }
  return ocel_python_equal(value, cell);
}

// The values as isin holds them (review Z1-3): a list of ints and floats
// only is float64, so against a numeric column its ints are rounded to
// doubles (2^53 + 1 next to 1.5 is 2.0^53); a list holding a bool, string or
// null is an object array, compared with Python == even against an int64
// column (2^53 + 1 does not equal 2.0^53 there); an object column compares
// every list as given.
struct OcelIsinValues {
  std::vector<OcelValue> values;
  bool object_list = false;
};

OcelIsinValues isin_values(const std::vector<OcelValue>& values, OcelNumericColumn column) {
  OcelIsinValues result{values, false};
  bool integer = false, real = false;
  for (const auto& value : values) {
    if (std::holds_alternative<std::int64_t>(value)) integer = true;
    else if (std::holds_alternative<double>(value)) real = true;
    else result.object_list = true;
  }
  if (result.object_list || !integer || !real || column == OcelNumericColumn::kObject)
    return result;
  for (auto& value : result.values)
    if (const auto* number = std::get_if<std::int64_t>(&value))
      value = static_cast<double>(*number);
  return result;
}

bool has_attribute_value(const std::vector<OcelAttribute>& attributes,
                         std::string_view key,
                         const OcelIsinValues& values,
                         OcelNumericColumn column) {
  for (const auto& attribute : attributes) {
    if (attribute.key != key) continue;
    for (const auto& value : values.values)
      if (ocel_cell_equal(attribute.value, value, column, values.object_list)) return true;
  }
  return false;
}

void clear_all_tables(Ocel& log) {
  log.events.clear();
  log.objects.clear();
  log.relations.clear();
  log.o2o.clear();
  log.e2e.clear();
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
  log.e2e.erase(
      std::remove_if(log.e2e.begin(), log.e2e.end(),
                     [&](const OcelE2eRelation& relation) {
                       return !selected_events.count(relation.source_id) ||
                              !selected_events.count(relation.target_id);
                     }),
      log.e2e.end());
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
  log.e2e.erase(
      std::remove_if(log.e2e.begin(), log.e2e.end(),
                     [&](const OcelE2eRelation& relation) {
                       return !selected_events.count(relation.source_id) ||
                              !selected_events.count(relation.target_id);
                     }),
      log.e2e.end());
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
  log.e2e.erase(
      std::remove_if(log.e2e.begin(), log.e2e.end(),
                     [&](const OcelE2eRelation& relation) {
                       return !selected_events.count(relation.source_id) ||
                              !selected_events.count(relation.target_id);
                     }),
      log.e2e.end());
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
    bool changed = false;
    for (const auto& object_id : frontier) {
      const auto adjacency_it = adjacency.find(object_id);
      if (adjacency_it == adjacency.end()) continue;
      for (const auto& neighbour : adjacency_it->second) {
        changed = selected.insert(neighbour).second || changed;
      }
    }
    if (!changed) break;
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
  const auto column = ocel_numeric_column(log.events, attribute_key);
  const auto column_values = isin_values(values, column);
  filter_records(filtered.events, [&](const OcelEvent& event) {
    // The id and activity are columns of the reference events frame, so
    // filter_ocel_event_attribute selects on them (ocel:timestamp holds
    // datetimes, which no given value equals, as a missing key matches none).
    const bool matches =
        attribute_key == "ocel:eid" ? contains_value(values, OcelValue(event.id))
        : attribute_key == "ocel:activity" ? contains_value(values, OcelValue(event.activity))
        : has_attribute_value(event.attributes, attribute_key, column_values, column);
    return positive ? matches : !matches;
  });
  return propagate_event_filter(std::move(filtered));
}

namespace {
bool event_filter_column_exists(const Ocel& log, std::string_view key) {
  if (key == "ocel:eid" || key == "ocel:activity" || key == "ocel:timestamp")
    return true;
  if (log.table_columns.events)
    return std::find(log.table_columns.events->begin(),
                     log.table_columns.events->end(), key) !=
           log.table_columns.events->end();
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes)
      if (attribute.key == key) return true;
  return false;
}

OcelValue event_filter_scalar(const OcelEvent& event, std::string_view key) {
  if (key == "ocel:eid") return event.id;
  if (key == "ocel:activity") return event.activity;
  if (key == "ocel:timestamp") return event.timestamp;
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return attribute.value;
  // An absent cell in a declared dataframe column is NaN, not explicit None.
  return std::numeric_limits<double>::quiet_NaN();
}

bool event_filter_numeric_integer_equal(std::int64_t integer, double real) {
  return std::isfinite(real) && real >= -0x1p63 && real < 0x1p63 &&
         std::trunc(real) == real && integer == static_cast<std::int64_t>(real);
}

bool event_filter_scalar_equal(const OcelValue& left,
                              const OcelEventAttributeFilterValue& right) {
  if (right.index() >= std::variant_size_v<OcelValue>) return false;
  if (left.index() == right.index()) {
    if (const auto* value = std::get_if<double>(&left)) {
      const double other = std::get<double>(right);
      return *value == other || (std::isnan(*value) && std::isnan(other));
    }
    return std::visit([&](const auto& value) {
      using Value = std::decay_t<decltype(value)>;
      const auto* other = std::get_if<Value>(&right);
      return other && value == *other;
    }, left);
  }
  const auto integer = [](const auto& value) -> std::optional<std::int64_t> {
    if (const auto* number = std::get_if<std::int64_t>(&value)) return *number;
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? 1 : 0;
    return std::nullopt;
  };
  const auto left_integer = integer(left), right_integer = integer(right);
  if (left_integer && right_integer) return *left_integer == *right_integer;
  if (left_integer) {
    const auto* real = std::get_if<double>(&right);
    return real && event_filter_numeric_integer_equal(*left_integer, *real);
  }
  if (right_integer) {
    const auto* real = std::get_if<double>(&left);
    return real && event_filter_numeric_integer_equal(*right_integer, *real);
  }
  return false;
}

bool event_filter_instant_equal(const ParsedExactTimestamp& left,
                                const ParsedExactTimestamp& right) {
  return left.timezone_aware == right.timezone_aware &&
         left.seconds == right.seconds && left.nanoseconds == right.nanoseconds;
}

bool object_filter_column_exists(const Ocel& log, std::string_view key) {
  if (key == "ocel:oid" || key == "ocel:type") return true;
  if (log.table_columns.objects)
    return std::find(log.table_columns.objects->begin(),
                     log.table_columns.objects->end(), key) !=
           log.table_columns.objects->end();
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes)
      if (attribute.key == key) return true;
  return false;
}

OcelValue object_filter_scalar(const OcelObject& object, std::string_view key) {
  if (key == "ocel:oid") return object.id;
  if (key == "ocel:type") return object.type;
  for (const auto& attribute : object.attributes)
    if (attribute.key == key) return attribute.value;
  return std::numeric_limits<double>::quiet_NaN();
}

bool object_filter_timestamp_column(const Ocel& log, const OcelObject& object,
                                    std::string_view key,
                                    OcelObjectAttributeColumnKind kind) {
  if (kind != OcelObjectAttributeColumnKind::kAutomatic)
    return kind == OcelObjectAttributeColumnKind::kTimestamp;
  const auto type = log.object_type_attributes.find(object.type);
  if (type == log.object_type_attributes.end()) return false;
  const auto attribute = type->second.find(std::string(key));
  return attribute != type->second.end() &&
         (attribute->second.find("date") != std::string::npos ||
          attribute->second.find("time") != std::string::npos);
}

bool attribute_filter_datetime_equal(
    const OcelValue& candidate, const OcelEventAttributeFilterValue& value) {
  const auto* text = std::get_if<std::string>(&candidate);
  if (std::holds_alternative<OcelAttributeNaT>(value)) {
    if (std::holds_alternative<std::nullptr_t>(candidate)) return true;
    if (const auto* real = std::get_if<double>(&candidate)) return std::isnan(*real);
    return text && *text == "NaT";
  }
  const auto* other = std::get_if<ParsedExactTimestamp>(&value);
  const auto instant = text ? parse_timestamp_exact(*text) : std::nullopt;
  return other && instant && event_filter_instant_equal(*instant, *other);
}

bool event_filter_timestamp_column(const Ocel& log, const OcelEvent& event,
                                   std::string_view key,
                                   OcelEventAttributeColumnKind kind) {
  if (kind != OcelEventAttributeColumnKind::kAutomatic)
    return kind == OcelEventAttributeColumnKind::kTimestamp;
  if (key == "ocel:timestamp") return true;
  const auto type = log.event_type_attributes.find(event.activity);
  if (type == log.event_type_attributes.end()) return false;
  const auto attribute = type->second.find(std::string(key));
  return attribute != type->second.end() &&
         (attribute->second.find("date") != std::string::npos ||
          attribute->second.find("time") != std::string::npos);
}

ParsedExactTimestamp event_filter_timestamp_bound(
    const OcelEventTimestampBound& bound) {
  if (const auto* datetime = std::get_if<ParsedExactTimestamp>(&bound)) {
    auto result = *datetime;
    // The public filter replaces tzinfo with UTC without converting the local
    // clock. Reconstruct that clock from the parsed instant's stored offset.
    const auto delta = std::int64_t{result.offset_minutes} * 60;
    if ((delta > 0 && result.seconds > std::numeric_limits<std::int64_t>::max() - delta) ||
        (delta < 0 && result.seconds < std::numeric_limits<std::int64_t>::min() - delta))
      throw std::invalid_argument("OCEL timestamp bound is out of range");
    result.seconds += delta;
    result.timezone_aware = true;
    result.offset_minutes = 0;
    return result;
  }
  const auto& text = std::get<std::string>(bound);
  const auto invalid = [&]() -> ParsedExactTimestamp {
    throw std::invalid_argument("time data '" + text +
        "' does not match format '%Y-%m-%d %H:%M:%S'");
  };
  std::istringstream input{text};
  input.imbue(std::locale::classic());
  std::tm calendar{};
  calendar.tm_year = std::numeric_limits<int>::min();
  calendar.tm_mon = calendar.tm_mday = calendar.tm_hour =
      calendar.tm_min = calendar.tm_sec = -1;
  input >> std::get_time(&calendar, "%Y-%m-%d %H:%M:%S");
  if (input.fail() || calendar.tm_year == std::numeric_limits<int>::min() ||
      calendar.tm_mon < 0 || calendar.tm_mday < 0 || calendar.tm_hour < 0 ||
      calendar.tm_min < 0 || calendar.tm_sec < 0) return invalid();
  if (input.peek() != std::char_traits<char>::eof()) {
    std::string remaining;
    std::getline(input, remaining, '\0');
    throw std::invalid_argument("unconverted data remains: " + remaining);
  }
  std::ostringstream normalized;
  normalized.imbue(std::locale::classic());
  normalized << std::setfill('0') << std::setw(4) << calendar.tm_year + 1900
             << '-' << std::setw(2) << calendar.tm_mon + 1
             << '-' << std::setw(2) << calendar.tm_mday << 'T'
             << std::setw(2) << calendar.tm_hour << ':'
             << std::setw(2) << calendar.tm_min << ':'
             << std::setw(2) << calendar.tm_sec << 'Z';
  const auto parsed = parse_timestamp_exact(normalized.str());
  return parsed ? *parsed : invalid();
}

bool event_filter_instant_less(const ParsedExactTimestamp& left,
                               const ParsedExactTimestamp& right) {
  return std::tie(left.seconds, left.nanoseconds) <
         std::tie(right.seconds, right.nanoseconds);
}
}  // namespace

Ocel filter_ocel_event_attribute_exact(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelEventAttributeFilterValue>& values, bool positive,
    OcelEventAttributeColumnKind column_kind) {
  if (!event_filter_column_exists(log, attribute_key))
    throw std::out_of_range("'" + std::string(attribute_key) + "'");
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    bool matches = false;
    const auto candidate = event_filter_scalar(event, attribute_key);
    if (event_filter_timestamp_column(log, event, attribute_key, column_kind)) {
      for (const auto& value : values)
        matches = matches || attribute_filter_datetime_equal(candidate, value);
    } else {
      for (const auto& value : values)
        matches = matches || event_filter_scalar_equal(candidate, value);
    }
    return matches == positive;
  });
  return propagate_event_filter(std::move(filtered));
}

Ocel filter_ocel_events_timestamp_exact(
    const Ocel& log, const OcelEventTimestampBound& from,
    const OcelEventTimestampBound& to, std::string_view timestamp_key) {
  const auto lower = event_filter_timestamp_bound(from);
  const auto upper = event_filter_timestamp_bound(to);
  if (!event_filter_column_exists(log, timestamp_key))
    throw std::out_of_range("'" + std::string(timestamp_key) + "'");
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    const auto value = event_filter_scalar(event, timestamp_key);
    const auto* text = std::get_if<std::string>(&value);
    const auto instant = text ? parse_timestamp_exact(*text) : std::nullopt;
    if (!instant) return false;
    if (!instant->timezone_aware)
      throw std::invalid_argument("OCEL timestamp comparison requires timezone-aware source values");
    return !event_filter_instant_less(*instant, lower) &&
           !event_filter_instant_less(upper, *instant);
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
  const auto column = ocel_numeric_column(log.objects, attribute_key);
  const auto column_values = isin_values(values, column);
  filter_records(filtered.objects, [&](const OcelObject& object) {
    // The id and type are columns of the reference objects frame.
    const bool matches =
        attribute_key == "ocel:oid" ? contains_value(values, OcelValue(object.id))
        : attribute_key == "ocel:type" ? contains_value(values, OcelValue(object.type))
        : has_attribute_value(object.attributes, attribute_key, column_values, column);
    return positive ? matches : !matches;
  });
  return propagate_object_filter(std::move(filtered));
}

Ocel filter_ocel_object_attribute_exact(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelObjectAttributeFilterValue>& values, bool positive,
    OcelObjectAttributeColumnKind column_kind) {
  if (!object_filter_column_exists(log, attribute_key))
    throw std::out_of_range("'" + std::string(attribute_key) + "'");
  Ocel filtered = log;
  filter_records(filtered.objects, [&](const OcelObject& object) {
    const auto candidate = object_filter_scalar(object, attribute_key);
    const bool datetime =
        object_filter_timestamp_column(log, object, attribute_key, column_kind);
    const bool matches = std::any_of(values.begin(), values.end(), [&](const auto& value) {
      return datetime ? attribute_filter_datetime_equal(candidate, value)
                      : event_filter_scalar_equal(candidate, value);
    });
    return matches == positive;
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
  // Reversed bounds select no event, so the result is an empty OCEL, as in
  // the reference.
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
bool meets_object_type_count(std::size_t count,
                             const OcelObjectTypeCountThreshold& minimum) {
  if (const auto* integer = std::get_if<TokenInteger>(&minimum))
    return TokenInteger{count} >= *integer;
  const double threshold = std::get<double>(minimum);
  if (std::isnan(threshold)) return false;
  if (threshold <= 0.0) return true;
  // Comparing through double(count) would round exact counts above 2^53.
  // Range-check the threshold before converting its integral ceiling.
  if (threshold >= std::ldexp(1.0, std::numeric_limits<std::size_t>::digits))
    return false;
  return count >= static_cast<std::size_t>(std::ceil(threshold));
}
}  // namespace

Ocel filter_ocel_events_object_type_counts_exact(
    const Ocel& log,
    const std::map<std::string, OcelObjectTypeCountThreshold>& minimum_counts) {
  const auto counts = ocel_event_object_type_counts(log);
  IdSet selected_events;
  for (const auto& [event_id, by_type] : counts) {
    bool keep = true;
    for (const auto& [object_type, minimum] : minimum_counts) {
      const auto type_it = by_type.find(object_type);
      if (type_it == by_type.end() ||
          !meets_object_type_count(type_it->second, minimum)) {
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
