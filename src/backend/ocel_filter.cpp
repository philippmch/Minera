#include "minera/ocel_filter.hpp"
#include "ocel_frame_types.hpp"

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

Ocel propagate_event_filter_rows(Ocel log) {
  const auto selected_events = ids_of_events(log);
  if (selected_events.empty()) {
    clear_all_tables(log);
    return log;
  }
  // Round DV: the reference selects the relations by event id once an event
  // remains (a degenerate relations frame then raises; black-box).
  require_ocel_relation_column(log, "ocel:eid");

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

Ocel propagate_object_filter_rows(Ocel log) {
  const auto selected_objects = ids_of_objects(log);
  if (selected_objects.empty()) {
    clear_all_tables(log);
    return log;
  }
  // Round DV: by object id once an object remains (see above).
  require_ocel_relation_column(log, "ocel:oid");

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

Ocel propagate_relation_filter_rows(Ocel log) {
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

// Round EZ: the filtered log records the source rows it lacks in its frame
// hints (see ocel_frame::hint_filtered_rows).
Ocel propagate_event_filter(Ocel log, const Ocel& source) {
  auto result = propagate_event_filter_rows(std::move(log));
  ocel_frame::hint_filtered_rows(source, result);
  return result;
}

Ocel propagate_object_filter(Ocel log, const Ocel& source) {
  auto result = propagate_object_filter_rows(std::move(log));
  ocel_frame::hint_filtered_rows(source, result);
  return result;
}

Ocel propagate_relation_filter(Ocel log, const Ocel& source) {
  auto result = propagate_relation_filter_rows(std::move(log));
  ocel_frame::hint_filtered_rows(source, result);
  return result;
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
  if (level <= 1) return selected;
  const auto object_types = object_types_by_id(log);
  // Round EP: the reference's interaction graph has a node for every listed
  // object, related or not, and adds the edges event by event in event id
  // order, each event's objects in relation order. An object the objects
  // table does not list raises KeyError there once it shares an event with
  // another object, whatever was selected (observed black-box).
  std::map<std::string, std::vector<std::string>> by_event;
  for (const auto& relation : log.relations) {
    auto& objects = by_event[relation.event_id];
    if (std::find(objects.begin(), objects.end(), relation.object_id) == objects.end())
      objects.push_back(relation.object_id);
  }
  std::map<std::string, std::set<std::string>> adjacency;
  for (const auto& entry : object_types) adjacency[entry.first];
  for (const auto& [event_id, objects] : by_event) {
    (void)event_id;
    if (objects.size() < 2) continue;
    for (const auto& object_id : objects)
      if (!object_types.contains(object_id))
        throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(object_id));
    for (const auto& left : objects) {
      auto& neighbours = adjacency[left];
      neighbours.insert(objects.begin(), objects.end());
      neighbours.erase(left);
    }
  }
  // Round DT: every requested id is then looked up in that graph, so an id
  // the objects table does not list raises KeyError (several such ids: the
  // first in its set order, which varies with the hash seed; native names
  // the smallest). Round EQ: the smallest by code point, whatever the
  // unordered set's iteration order (which differs between standard
  // libraries).
  const std::string* missing = nullptr;
  for (const auto& object_id : selected)
    if (!adjacency.contains(object_id) && (missing == nullptr || object_id < *missing))
      missing = &object_id;
  if (missing != nullptr)
    throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(*missing));
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
  // Round DV: an events frame without columns (an empty playout).
  require_ocel_event_column(log, attribute_key);
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
  return propagate_event_filter(std::move(filtered), log);
}

namespace {
// Round FA: a column only rows a reader or an earlier filter removed hold
// stays a column of the reference frame (OcelFrameHint, round EM / EZ), so
// the exact attribute filters find it there (observed black-box: pm4py
// filters an object type and then an event attribute only the removed events
// held without a KeyError).
bool frame_hint_has_column(const OcelFrameHint& hint, std::string_view key) {
  if (std::find(hint.columns.begin(), hint.columns.end(), key) != hint.columns.end()) return true;
  return std::any_of(hint.dropped.begin(), hint.dropped.end(),
                     [&](const OcelFrameHintColumn& column) { return column.key == key; });
}

bool event_filter_column_exists(const Ocel& log, std::string_view key) {
  if (key == "ocel:eid" || key == "ocel:activity" || key == "ocel:timestamp")
    return true;
  if (frame_hint_has_column(log.event_frame_hint, key)) return true;
  if (log.table_columns.events)
    return std::find(log.table_columns.events->begin(),
                     log.table_columns.events->end(), key) !=
           log.table_columns.events->end();
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes)
      if (attribute.key == key) return true;
  return false;
}

// Round EZ: whether the row's value under `key` is a text the reference
// keeps as a str under a date declaration (OcelAttribute::text): it then
// compares as a str, never as a datetime.
bool kept_text_value(const std::vector<OcelAttribute>& attributes, std::string_view key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return attribute.text;
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
  // A frame without columns (round DT: a file that lists no object) has no
  // id or type either; declared frames with other names keep them.
  if (log.table_columns.objects && log.table_columns.objects->empty()) return false;
  if (key == "ocel:oid" || key == "ocel:type") return true;
  if (frame_hint_has_column(log.object_frame_hint, key)) return true;
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

// Round FD: the exact attribute filters pass the values to pandas' isin,
// which matches None, NaT and NaN by the column's dtype (observed
// black-box, pandas 3.0.5): a str column (texts alone) holds every missing
// cell as NaN and matches it with any of the three; an object column keeps
// an explicit None apart from a missing cell's NaN (None matches the one,
// NaN the other, NaT neither); a float64 column matches NaN alone, a
// datetime64 one NaT alone; an int64 or bool column has no missing cell.
// A column of missing cells alone is object when every cell is an explicit
// None (or an all-NULL SQLite column took part), float64 otherwise.
enum class NullColumn { kStr, kObject, kFloat, kDatetime, kNever };
enum class NullValue { kNone, kNaT, kNaN };
std::optional<NullValue> null_value(const OcelEventAttributeFilterValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return NullValue::kNone;
  if (std::holds_alternative<OcelAttributeNaT>(value)) return NullValue::kNaT;
  if (const auto* real = std::get_if<double>(&value); real && std::isnan(*real)) return NullValue::kNaN;
  return std::nullopt;
}
template <typename Records>
NullColumn null_column(const ocel_frame::Columns& frame, const Records& /*records*/, const std::string& key) {
  const auto* stats = frame.stats(key);
  // Round FG: the dropped rows' nulls count too (Columns::all_nulls).
  if (stats == nullptr || stats->present == 0)
    return frame.all_nulls(key) ? NullColumn::kObject : NullColumn::kFloat;
  switch (stats->dtype(frame.rows())) {
    case ocel_frame::Dtype::kDatetime: return NullColumn::kDatetime;
    case ocel_frame::Dtype::kFloat64: return NullColumn::kFloat;
    case ocel_frame::Dtype::kInt64:
    case ocel_frame::Dtype::kBool: return NullColumn::kNever;
    default:
      return stats->has_string && !stats->has_bool && !stats->has_int && !stats->has_float && !stats->has_date() &&
                     !stats->has_none
                 ? NullColumn::kStr
                 : NullColumn::kObject;
  }
}
// Whether a cell (`absent`: the row lacks the attribute) matches a null value.
bool null_cell_match(NullColumn column, NullValue value, const OcelValue& cell, bool absent) {
  const bool none = !absent && std::holds_alternative<std::nullptr_t>(cell);
  const auto* real = absent ? nullptr : std::get_if<double>(&cell);
  const auto* text = absent ? nullptr : std::get_if<std::string>(&cell);
  const bool nan = absent || (real && std::isnan(*real));
  switch (column) {
    case NullColumn::kStr: return absent || none;
    case NullColumn::kObject: return value == NullValue::kNone ? none : value == NullValue::kNaN && nan;
    case NullColumn::kFloat: return value == NullValue::kNaN && (nan || none);
    case NullColumn::kDatetime: return value == NullValue::kNaT && (absent || none || (text && (text->empty() || *text == "NaT")));
    default: return false;
  }
}
bool attribute_absent(const std::vector<OcelAttribute>& attributes, std::string_view key) {
  return std::none_of(attributes.begin(), attributes.end(), [&](const OcelAttribute& attribute) { return attribute.key == key; });
}

// Round FA: whether a candidate of a date-declared column compares as a
// datetime: a timestamp text (or "NaT") and a missing cell (NaT) do; a plain
// text or a number under the key stays a scalar of the reference frame's
// object column, which pm4py's isin compares as it is (observed black-box,
// bank c2/c61: `<string key="z" value=""/>` and `<float key="z">` next to
// `<date>`s of the same object type match "" and 1.5). An explicit column
// kind keeps its comparison.
bool date_like_candidate(const OcelValue& candidate) {
  if (std::holds_alternative<std::nullptr_t>(candidate)) return true;
  if (const auto* real = std::get_if<double>(&candidate)) return std::isnan(*real);
  if (const auto* text = std::get_if<std::string>(&candidate))
    return *text == "NaT" || parse_timestamp_exact(*text).has_value();
  return false;
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

// Round FB: the window on an attribute column compares the events frame's
// column with two aware datetimes (`series >= lower`), which raises unless
// every cell the comparison meets is an aware datetime (observed black-box,
// pandas 3.0.5): an int64, float64 or bool column raises at its first cell
// (a NaN of a float64 column too), a naive datetime64 one for its dtype;
// an object column compares cell by cell in frame order, skipping missing
// cells (None, NaN), and the first text, number, boolean or naive datetime
// raises. Native compared the timestamp texts and skipped everything else.
// A date-declared text counts as a datetime as the frame typing reads it
// (ocel_frame::reference_date_value); the rest of the filter then compares
// the aware ones as before.
void check_event_attribute_window_column(const Ocel& log, std::string_view key) {
  const std::string name(key);
  const auto frame = ocel_frame::event_frame(log, true);
  if (log.events.empty()) {
    // Round FC: a frame without rows compares nothing, save that a naive
    // datetime64 column raises for its dtype; a filter's result keeps its
    // source's dtypes (the frame hint's dropped rows; observed black-box:
    // o73's o2t73_0019 after a negated object type filter keeps no event;
    // pandas 3.0.5 passes an empty int64, float64, bool, object, str or
    // aware datetime64 column).
    const auto* stats = frame.stats(name);
    if (frame.dtype(name) == ocel_frame::Dtype::kDatetime && stats != nullptr && stats->has_naive_date)
      throw std::invalid_argument("TypeError: Invalid comparison between dtype=datetime64[us] and datetime");
    return;
  }
  const auto refuse = [](std::string_view type) {
    throw std::invalid_argument(std::string("TypeError: '>=' not supported between instances of '") +
                                std::string(type) + "' and 'datetime.datetime'");
  };
  const auto naive = [] {
    throw std::invalid_argument("TypeError: can't compare offset-naive and offset-aware datetimes");
  };
  switch (frame.dtype(name)) {
    case ocel_frame::Dtype::kEmpty: {
      // A column of nulls alone: pandas builds it float64 when a row lacks
      // the key (NaN next to None) and object when every row holds a null
      // (observed black-box: o72's o2t72_0040, one null "d" among four
      // events, raises 'float'); an all-NULL SQLite table column is object.
      const auto* stats = frame.stats(name);
      if (stats != nullptr && stats->has_none) return;
      // Round FC: read_ocel_csv reads its files as str, so its column of
      // nulls is a str one and compares nothing (OcelFrameHint::str_columns;
      // observed black-box: q26's files whose column only empty cells hold).
      if (log.event_frame_hint.str_columns) return;
      const bool every = std::all_of(log.events.begin(), log.events.end(), [&](const OcelEvent& event) {
        return std::any_of(event.attributes.begin(), event.attributes.end(),
                           [&](const OcelAttribute& attribute) { return attribute.key == key; });
      });
      if (every && log.event_frame_hint.dropped_rows == 0) return;
      refuse("float");
      return;
    }
    case ocel_frame::Dtype::kInt64: refuse("int"); return;
    case ocel_frame::Dtype::kFloat64: refuse("float"); return;
    case ocel_frame::Dtype::kBool: refuse("bool"); return;
    case ocel_frame::Dtype::kDatetime:
      if (const auto* stats = frame.stats(name); stats && stats->has_naive_date)
        throw std::invalid_argument("TypeError: Invalid comparison between dtype=datetime64[us] and datetime");
      return;
    case ocel_frame::Dtype::kObject: break;
  }
  for (const auto& event : log.events) {
    const auto found = std::find_if(event.attributes.begin(), event.attributes.end(),
                                    [&](const OcelAttribute& attribute) { return attribute.key == key; });
    if (found == event.attributes.end()) continue;
    const auto* attribute = &*found;
    const auto& value = attribute->value;
    if (std::holds_alternative<std::nullptr_t>(value)) continue;
    if (const auto* real = std::get_if<double>(&value)) {
      if (std::isnan(*real)) continue;
      refuse("float");
    }
    if (std::holds_alternative<bool>(value)) refuse("bool");
    if (std::holds_alternative<std::int64_t>(value)) refuse("int");
    if (!attribute->text && std::get<std::string>(value) == "NaT" &&
        ocel_frame::declared_date(log.event_type_attributes, event.activity, name)) continue;
    if (!ocel_frame::reference_date_value(log.event_type_attributes, event.activity, *attribute)) refuse("str");
    if (ocel_frame::naive_timestamp_text(std::get<std::string>(value))) naive();
  }
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
  // Round FD: None, NaT and NaN match by the column's dtype (NullColumn).
  std::optional<NullColumn> nulls;
  const bool null_values = std::any_of(values.begin(), values.end(), [](const auto& value) { return null_value(value).has_value(); });
  if (column_kind == OcelEventAttributeColumnKind::kAutomatic && null_values) {
    if (attribute_key == "ocel:eid" || attribute_key == "ocel:activity") nulls = NullColumn::kNever;
    else if (attribute_key == "ocel:timestamp") {
      if (!ocel_raw_event_times(log) && !log.none_event_times) nulls = NullColumn::kDatetime;
    } else if (log.event_frame_hint.str_columns) {
      // An OCEL CSV read's columns are str ones (OcelFrameHint::str_columns).
      nulls = NullColumn::kStr;
    } else {
      nulls = null_column(ocel_frame::event_frame(log, true), log.events, std::string(attribute_key));
    }
  }
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    bool matches = false;
    const auto candidate = event_filter_scalar(event, attribute_key);
    if (nulls) {
      const bool canonical = attribute_key == "ocel:eid" || attribute_key == "ocel:activity" || attribute_key == "ocel:timestamp";
      const bool absent = !canonical && attribute_absent(event.attributes, attribute_key);
      for (const auto& value : values)
        if (const auto kind = null_value(value)) matches = matches || null_cell_match(*nulls, *kind, candidate, absent);
      if (matches) return matches == positive;
    }
    const auto skip = [&](const OcelEventAttributeFilterValue& value) { return nulls && null_value(value); };
    if (!kept_text_value(event.attributes, attribute_key) &&
        event_filter_timestamp_column(log, event, attribute_key, column_kind) &&
        (column_kind != OcelEventAttributeColumnKind::kAutomatic || date_like_candidate(candidate))) {
      for (const auto& value : values)
        matches = matches || (!skip(value) && attribute_filter_datetime_equal(candidate, value));
    } else {
      for (const auto& value : values)
        matches = matches || (!skip(value) && event_filter_scalar_equal(candidate, value));
    }
    return matches == positive;
  });
  return propagate_event_filter(std::move(filtered), log);
}

Ocel filter_ocel_events_timestamp_exact(
    const Ocel& log, const OcelEventTimestampBound& from,
    const OcelEventTimestampBound& to, std::string_view timestamp_key, bool reference_frame) {
  const auto lower = event_filter_timestamp_bound(from);
  const auto upper = event_filter_timestamp_bound(to);
  if (!event_filter_column_exists(log, timestamp_key))
    throw std::out_of_range("'" + std::string(timestamp_key) + "'");
  // Round DU: both bounds are aware here (see event_filter_timestamp_bound),
  // so a naive events frame raises (see filter_ocel_events_timestamp).
  if (timestamp_key == "ocel:timestamp" && !log.naive_event_time_unit.empty())
    throw std::invalid_argument("TypeError: Invalid comparison between dtype=datetime64[" +
                                log.naive_event_time_unit + "] and datetime");
  // Round EL: a raw time column (OcelRawTime) compares its first value
  // (observed black-box).
  if (timestamp_key == "ocel:timestamp" && !log.events.empty() && log.events.front().raw_time != OcelRawTime::kNone)
    throw std::invalid_argument(std::string("TypeError: '>=' not supported between instances of '") +
                                ocel_raw_time_type(log.events.front().raw_time) + "' and 'datetime.datetime'");
  if (reference_frame && timestamp_key != "ocel:timestamp") check_event_attribute_window_column(log, timestamp_key);
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
  return propagate_event_filter(std::move(filtered), log);
}

Ocel filter_ocel_object_attribute(const Ocel& log,
                                  std::string_view attribute_key,
                                  const std::vector<OcelValue>& values,
                                  bool positive) {
  if (attribute_key.empty()) {
    throw std::invalid_argument("OCEL object attribute key must not be empty");
  }
  if (attribute_key == "ocel:oid" || attribute_key == "ocel:type")
    require_ocel_object_column(log, attribute_key);
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
  return propagate_object_filter(std::move(filtered), log);
}

Ocel filter_ocel_object_attribute_exact(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelObjectAttributeFilterValue>& values, bool positive,
    OcelObjectAttributeColumnKind column_kind) {
  if (!object_filter_column_exists(log, attribute_key))
    throw std::out_of_range("'" + std::string(attribute_key) + "'");
  // Round FD: None, NaT and NaN match by the column's dtype (NullColumn).
  std::optional<NullColumn> nulls;
  const bool null_values = std::any_of(values.begin(), values.end(), [](const auto& value) { return null_value(value).has_value(); });
  if (column_kind == OcelObjectAttributeColumnKind::kAutomatic && null_values) {
    if (attribute_key == "ocel:oid" || attribute_key == "ocel:type") nulls = NullColumn::kNever;
    // An OCEL CSV read's objects file is read as str too (observed
    // black-box, bank q28; the events frame's hint marks the read).
    else if (log.event_frame_hint.str_columns) nulls = NullColumn::kStr;
    else {
      nulls = null_column(ocel_frame::object_frame(log, true), log.objects, std::string(attribute_key));
    }
  }
  Ocel filtered = log;
  filter_records(filtered.objects, [&](const OcelObject& object) {
    const auto candidate = object_filter_scalar(object, attribute_key);
    if (nulls) {
      const bool canonical = attribute_key == "ocel:oid" || attribute_key == "ocel:type";
      const bool absent = !canonical && attribute_absent(object.attributes, attribute_key);
      bool null_match = false;
      for (const auto& value : values)
        if (const auto kind = null_value(value)) null_match = null_match || null_cell_match(*nulls, *kind, candidate, absent);
      if (null_match) return positive;
    }
    const auto skip = [&](const OcelObjectAttributeFilterValue& value) { return nulls && null_value(value); };
    const bool datetime = !kept_text_value(object.attributes, attribute_key) &&
                          object_filter_timestamp_column(log, object, attribute_key, column_kind) &&
                          (column_kind != OcelObjectAttributeColumnKind::kAutomatic || date_like_candidate(candidate));
    const bool matches = std::any_of(values.begin(), values.end(), [&](const auto& value) {
      return !skip(value) && (datetime ? attribute_filter_datetime_equal(candidate, value)
                                       : event_filter_scalar_equal(candidate, value));
    });
    return matches == positive;
  });
  return propagate_object_filter(std::move(filtered), log);
}

Ocel filter_ocel_object_types(const Ocel& log,
                              const std::vector<std::string>& types,
                              bool positive, std::size_t level) {
  require_ocel_object_column(log, "ocel:type");
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
  return propagate_object_filter(std::move(filtered), log);
}

Ocel filter_ocel_objects(const Ocel& log,
                         const std::vector<std::string>& object_ids,
                         bool positive, std::size_t level) {
  // Round DV: a deeper level walks the relations from their event ids first.
  if (level > 1) require_ocel_relation_column(log, "ocel:eid");
  require_ocel_object_column(log, "ocel:oid");
  auto selected = expand_object_ids(log, string_set(object_ids), level);
  Ocel filtered = log;
  filter_records(filtered.objects, [&](const OcelObject& object) {
    const bool matches = selected.count(object.id) != 0;
    return positive ? matches : !matches;
  });
  return propagate_object_filter(std::move(filtered), log);
}

Ocel filter_ocel_events(const Ocel& log,
                        const std::vector<std::string>& event_ids,
                        bool positive) {
  // Round DV: an events frame without columns (an empty playout).
  require_ocel_event_column(log, "ocel:eid");
  const auto selected = string_set(event_ids);
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    const bool matches = selected.count(event.id) != 0;
    return positive ? matches : !matches;
  });
  return propagate_event_filter(std::move(filtered), log);
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
  // Round DV: an empty playout's events frame has no timestamp at all.
  require_ocel_event_column(log, "ocel:timestamp");
  // Round DU: the reference compares the events frame with aware bounds, so
  // a naive frame (an OCEL CSV import without offsets, a playout) raises,
  // empty or not (black-box).
  if (!log.naive_event_time_unit.empty())
    throw std::invalid_argument("TypeError: Invalid comparison between dtype=datetime64[" +
                                log.naive_event_time_unit + "] and datetime");
  // Round EL: a raw time column (OcelRawTime) compares its first value
  // (observed black-box).
  if (!log.events.empty() && log.events.front().raw_time != OcelRawTime::kNone)
    throw std::invalid_argument(std::string("TypeError: '>=' not supported between instances of '") +
                                ocel_raw_time_type(log.events.front().raw_time) + "' and 'datetime.datetime'");
  // Reversed bounds select no event, so the result is an empty OCEL, as in
  // the reference.
  Ocel filtered = log;
  filter_records(filtered.events, [&](const OcelEvent& event) {
    const auto instant = parse_timestamp(event.timestamp);
    return instant && *instant >= lower && *instant <= upper;
  });
  return propagate_event_filter(std::move(filtered), log);
}

Ocel filter_ocel_activity_type_matching(
    const Ocel& log,
    const std::map<std::string, std::vector<std::string>>& allowed) {
  // Round DV: a degenerate relations frame (see ocel_relations_frame_degenerate).
  require_ocel_relation_column(log, "ocel:activity");
  // Round EP: a reference read without relations has float64 relation
  // columns (Ocel::untyped_relations), and the filter adds its separator
  // text to the activity column (black-box).
  if (log.relations.empty() && log.untyped_relations)
    throw std::invalid_argument(
        "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('float64'), "
        "dtype('<U4')) -> None");
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
  return propagate_relation_filter(std::move(filtered), log);
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
  require_ocel_relation_column(log, "ocel:eid");
  // Round FD: it groups the relations by their type (a temporary column
  // may drop it).
  require_ocel_relation_column_kept(log, "ocel:type", true);
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
  return propagate_event_filter(std::move(filtered), log);
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
  require_ocel_relation_column(log, "ocel:eid");
  // Round FD: it groups the relations by their type (a temporary column
  // may drop it).
  require_ocel_relation_column_kept(log, "ocel:type", true);
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
  return propagate_event_filter(std::move(filtered), log);
}

namespace {

Ocel filter_ocel_lifecycle_events_per_object_type(const Ocel& log,
                                                   std::string_view object_type,
                                                   bool first) {
  require_ocel_relation_column(log, "ocel:type");
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
  return propagate_event_filter(std::move(filtered), log);
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
  require_ocel_relation_column(log, "ocel:type");
  // Round FD: then its activity (a temporary column may drop it).
  require_ocel_relation_column_kept(log, "ocel:activity");
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
  return propagate_relation_filter(std::move(filtered), log);
}

Ocel filter_ocel_cc_object(const Ocel& log, std::string_view object_id) {
  require_ocel_relation_column(log, "ocel:eid");
  require_ocel_object_column(log, "ocel:oid");
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
  require_ocel_relation_column(log, "ocel:eid");
  require_ocel_object_column(log, "ocel:oid");
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
  require_ocel_object_column(log, "ocel:type");
  require_ocel_relation_column(log, "ocel:eid");
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
  // Round DV: an empty playout fails on its events frame's activity first.
  require_ocel_event_column(log, "ocel:activity");
  require_ocel_relation_column(log, "ocel:eid");
  require_ocel_object_column(log, "ocel:oid");
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
