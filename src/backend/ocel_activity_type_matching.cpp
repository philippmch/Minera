#include "minera/ocel_activity_type_matching.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "minera/ocel_filter.hpp"

namespace minera {
namespace {
bool canonical_relation_column(std::string_view key) {
  return key == "ocel:eid" || key == "ocel:oid" || key == "ocel:activity" ||
         key == "ocel:type" || key == "ocel:timestamp" || key == "ocel:qualifier";
}

void require_relation_column(const Ocel& log, std::string_view key) {
  // Canonical native roles stay available when a dataframe adapter has kept
  // its original literal role names in the descriptive schema.
  if (canonical_relation_column(key)) return;
  if (log.table_columns.relations &&
      std::find(log.table_columns.relations->begin(),
                log.table_columns.relations->end(), key) !=
          log.table_columns.relations->end()) return;
  throw std::out_of_range("'" + std::string(key) + "'");
}

const std::string* relation_value(const OcelRelation& relation,
                                  const OcelEvent& event,
                                  const OcelObject& object,
                                  std::string_view key) {
  if (key == "ocel:eid") return &relation.event_id;
  if (key == "ocel:oid") return &relation.object_id;
  if (key == "ocel:activity") return &event.activity;
  if (key == "ocel:type") return &object.type;
  if (key == "ocel:timestamp") return &event.timestamp;
  if (key == "ocel:qualifier") return &relation.qualifier;
  // Additional relation dataframe cells have no native storage member.
  throw std::invalid_argument("OCEL matching selector has no native relation value: " +
                              std::string(key));
}

// Round EX: a qualifier column that holds numbers or bools (an OCEL SQLite
// relation table with a numeric affinity, OCEL JSON; OcelQualifierKind) as
// the activity or type key. The reference adds the separator to the
// activity column, then the type column to that (observed black-box, probe
// ex p/atm2.py): an int64, float64 or bool column raises numpy's
// UFuncTypeError as the activity column, an object column Python's
// TypeError naming its first value that is no str, and as the type column
// any of them raises "can only concatenate str". The column's dtype is read
// from the rows (one kind throughout is a numpy column; declared: a reader
// column of Python objects whose kept rows share a kind is not told apart).
void refuse_typed_qualifier_key(const Ocel& log, std::string_view key, bool activity,
                                std::size_t separator_length) {
  if (key != "ocel:qualifier") return;
  // Round EY: the column's dtype decides numpy's error, rows or not (an
  // object column of ints raises Python's TypeError; observed black-box,
  // probes ey p/e1.py, p/e2.py).
  const auto dtype = ocel_qualifier_dtype(log.relations, log.relation_qualifier_dtype);
  if (activity && dtype != OcelQualifierKind::kStr)
    throw std::invalid_argument(
        std::string("UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('") +
        ocel_qualifier_dtype_name(dtype) + "'), dtype('<U" + std::to_string(separator_length) + "')) -> None");
  const auto first = std::find_if(log.relations.begin(), log.relations.end(), [](const OcelRelation& relation) {
    return relation.qualifier_kind != OcelQualifierKind::kStr;
  });
  if (first == log.relations.end()) return;
  const auto kind = first->qualifier_kind;
  const std::string type = kind == OcelQualifierKind::kInt ? "int" : kind == OcelQualifierKind::kFloat ? "float" : "bool";
  if (!activity) throw std::invalid_argument("TypeError: can only concatenate str (not \"" + type + "\") to str");
  throw std::invalid_argument("TypeError: unsupported operand type(s) for +: '" + type + "' and 'str'");
}
}  // namespace

Ocel filter_ocel_activity_type_matching_exact(
    const Ocel& log,
    const std::map<std::string, std::vector<std::string>>& correspondence,
    const OcelActivityTypeMatchingOptions& options) {
  // Round DV: a degenerate relations frame (an OCEL CSV import without any
  // object reference) fails on the activity key first (black-box).
  require_ocel_relation_column(log, options.activity_key);
  // Round EP: the reference reads the activity column first, adds the
  // separator text to it, and then reads the type column, so a missing
  // activity key raises first, and a relations frame without rows of a
  // reference read (float64 columns, Ocel::untyped_relations) raises numpy's
  // UFuncTypeError naming the separator's length in code points (at least 1)
  // before a missing type key (black-box).
  require_relation_column(log, options.activity_key);
  std::size_t length = 4;
  if (options.temporary_separator) {
    length = 0;
    for (const char byte : *options.temporary_separator)
      if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80) ++length;
    length = std::max<std::size_t>(length, 1);
  }
  if (log.relations.empty() && log.untyped_relations) {
    throw std::invalid_argument(
        "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('float64'), "
        "dtype('<U" + std::to_string(length) + "')) -> None");
  }
  refuse_typed_qualifier_key(log, options.activity_key, true, length);
  // Round FC: the relations frame's ocel:timestamp column is datetime64, so
  // the separator added to it raises, rows or not; as the type column the
  // text column added to it raises per cell (pandas names the scalar with
  // two rows or more, the array with one) and an empty frame passes
  // (observed black-box, probes fc t1/atmts.py, atmts2.py).
  if (options.activity_key == "ocel:timestamp")
    throw std::invalid_argument("TypeError: unsupported operand type(s) for +: 'DatetimeArray' and 'str'");
  require_relation_column(log, options.object_type_key);
  refuse_typed_qualifier_key(log, options.object_type_key, false, length);
  if (options.object_type_key == "ocel:timestamp" && !log.relations.empty())
    throw std::invalid_argument(std::string("TypeError: unsupported operand type(s) for +: '") +
                                (log.relations.size() == 1 ? "DatetimeArray" : "Timestamp") + "' and 'str'");
  std::unordered_map<std::string, const OcelEvent*> events;
  std::unordered_map<std::string, const OcelObject*> objects;
  for (const auto& event : log.events) events.emplace(event.id, &event);
  for (const auto& object : log.objects) objects.emplace(object.id, &object);

  // The observed string-column temporary join loses terminal NUL characters
  // from its scalar separator. Python mapping strings preserve those bytes.
  // Interior and leading NULs remain significant on both sides.
  bool separator_matches = true;
  if (options.temporary_separator) {
    auto scalar_separator = *options.temporary_separator;
    while (!scalar_separator.empty() && scalar_separator.back() == '\0')
      scalar_separator.pop_back();
    separator_matches = scalar_separator == *options.temporary_separator;
  }
  Ocel filtered = log;
  auto& relations = filtered.relations;
  std::map<std::string, std::set<std::string>> retained_pairs;
  relations.erase(std::remove_if(relations.begin(), relations.end(), [&](const auto& relation) {
    const auto event = events.find(relation.event_id);
    const auto object = objects.find(relation.object_id);
    if (event == events.end() || object == objects.end()) return true;
    const auto* type = relation_value(relation, *event->second, *object->second, options.object_type_key);
    const auto allowed = correspondence.find(*type);
    if (allowed == correspondence.end() || !separator_matches) return true;
    const auto* activity = relation_value(relation, *event->second, *object->second, options.activity_key);
    if (std::find(allowed->second.begin(), allowed->second.end(), *activity) == allowed->second.end())
      return true;
    retained_pairs[object->second->type].insert(event->second->activity);
    return false;
  }), relations.end());

  if (options.temporary_column) {
    const auto& key = *options.temporary_column;
    if (!relations.empty() && (key == "ocel:eid" || key == "ocel:oid"))
      throw std::out_of_range("'" + key + "'");
    if (filtered.table_columns.relations) {
      auto& columns = *filtered.table_columns.relations;
      columns.erase(std::remove(columns.begin(), columns.end(), key), columns.end());
    }
    // Round EX: the cleared qualifier is a str ('').
    if (key == "ocel:qualifier") {
      for (auto& relation : relations) {
        relation.qualifier.clear();
        relation.qualifier_kind = OcelQualifierKind::kStr;
      }
      filtered.relation_qualifier_dtype = OcelQualifierKind::kStr;
    }
  }
  std::map<std::string, std::vector<std::string>> propagation_pairs;
  for (const auto& [type, activities] : retained_pairs)
    propagation_pairs.emplace(type, std::vector<std::string>(activities.begin(), activities.end()));
  // Reuse the established stable relation-based six-table propagation. Its
  // predicate admits every already selected native relation and no new rows.
  return filter_ocel_activity_type_matching(filtered, propagation_pairs);
}

}  // namespace minera
