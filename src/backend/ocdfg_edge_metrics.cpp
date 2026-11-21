#include "minera/ocdfg_edge_metrics.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include "minera/timestamp.hpp"

#include "ocel_frame_types.hpp"

namespace minera {
namespace {

[[noreturn]] void missing_column(const std::string& key, bool selected_column) {
  throw OcdfgEdgeMetricsError(
      "KeyError", "'" + (selected_column ? "Column not found: " : std::string()) + key + "'");
}

void require_column(const OcdfgAssociationTable& table, const std::string& key,
                    bool selected_column) {
  if (std::find(table.columns.begin(), table.columns.end(), key) == table.columns.end())
    missing_column(key, selected_column);
}

std::string string_cell(const std::map<std::string, OcelValue>& row,
                        const std::string& key) {
  const auto found = row.find(key);
  if (found == row.end())
    throw std::invalid_argument("OCDFG string association row is missing cell: " + key);
  if (const auto* text = std::get_if<std::string>(&found->second)) return *text;
  throw std::invalid_argument("OCDFG association companion requires string cells: " + key);
}

// Round FC: find_associations_per_edge reads the event id and activity
// columns of the events frame, the event id and object id columns of the
// relations frame (its six columns: an attribute there raises KeyError) and
// the object id and type columns of the objects frame, in that order:
//   identifiers = events[eid]; activities = events.groupby(eid)[activity]
//   omap = relations.groupby(eid)[oid]; objtypes = objects.groupby(oid)[type]
// then walks the identifiers (repeated keys repeat their objects; a missing
// key is no group). Observed black-box (round FC em.py): the former adapter
// joined each relation row with its event's and object's attributes (event
// first), so a type key an event attribute shares took the event's value and
// a repeated event id key paired relation rows instead.
const std::string kEid = "ocel:eid", kOid = "ocel:oid", kActivity = "ocel:activity",
                  kType = "ocel:type", kTimestamp = "ocel:timestamp", kQualifier = "ocel:qualifier";

[[noreturn]] void key_error(const std::string& text) {
  throw OcdfgEdgeMetricsError("KeyError", ocel_frame::python_str_repr(text));
}

std::set<std::string> event_columns(const Ocel& log) {
  if (log.table_columns.events)
    return {log.table_columns.events->begin(), log.table_columns.events->end()};
  std::set<std::string> columns = {kEid, kTimestamp, kActivity};
  const auto frame = ocel_frame::event_frame(log);
  columns.insert(frame.names().begin(), frame.names().end());
  return columns;
}
std::set<std::string> object_columns(const Ocel& log) {
  if (log.table_columns.objects)
    return {log.table_columns.objects->begin(), log.table_columns.objects->end()};
  std::set<std::string> columns = {kOid, kType};
  const auto frame = ocel_frame::object_frame(log);
  columns.insert(frame.names().begin(), frame.names().end());
  return columns;
}
std::set<std::string> relation_columns(const Ocel& log) {
  if (log.table_columns.relations)
    return {log.table_columns.relations->begin(), log.table_columns.relations->end()};
  return {kEid, kActivity, kTimestamp, kOid, kType, kQualifier};
}

// A frame cell: a text, a missing cell or another value. The carrier stays
// bounded to string cells: a missing or other cell rejects where the
// reference uses it as a label (declared; a missing key is no group).
struct Cell {
  enum class Kind { kText, kMissing, kOther } kind = Kind::kMissing;
  std::string text;
  Cell() = default;
  Cell(std::string value) : kind(Kind::kText), text(std::move(value)) {}
  static Cell other() { Cell cell; cell.kind = Kind::kOther; return cell; }
  [[nodiscard]] bool missing() const { return kind == Kind::kMissing; }
};
// How a frame reads an attribute column's texts: a value read as a datetime
// is a Timestamp label in pm4py. Its text names it in a datetime64 column
// (every value a datetime), but an object column holds it next to texts
// that may print the same (o81's o3_81_0011: "2024-02-01T10:00:00Z" a str
// under one type, a datetime under another, two groups in pm4py), so there
// it rejects as another value (Round FC).
struct Reading {
  const OcelTypeCatalog* catalog = nullptr;
  bool date_column = false;
};
Cell attribute_cell(const std::vector<OcelAttribute>& attributes, const std::string& key,
                    const std::string& owner, const Reading& reading) {
  for (const auto& attribute : attributes) {
    if (attribute.key != key) continue;
    if (std::holds_alternative<std::nullptr_t>(attribute.value)) return {};
    if (const auto* text = std::get_if<std::string>(&attribute.value)) {
      if (reading.catalog != nullptr && !reading.date_column &&
          ocel_frame::reference_date_value(*reading.catalog, owner, attribute))
        return Cell::other();
      return Cell(*text);
    }
    return Cell::other();
  }
  return {};
}
Cell event_cell(const OcelEvent& event, const std::string& key, const Reading& reading = {}) {
  if (key == kEid) return Cell(event.id);
  if (key == kActivity) return Cell(event.activity);
  if (key == kTimestamp) return event.raw_time == OcelRawTime::kNone ? Cell(event.timestamp) : Cell::other();
  return attribute_cell(event.attributes, key, event.activity, reading);
}
Cell object_cell(const OcelObject& object, const std::string& key, const Reading& reading = {}) {
  if (key == kOid) return Cell(object.id);
  if (key == kType) return Cell(object.type);
  return attribute_cell(object.attributes, key, object.type, reading);
}
Cell relation_cell(const OcelRelation& relation, const OcelEvent& event, const OcelObject& object,
                   const std::string& key) {
  if (key == kEid) return Cell(relation.event_id);
  if (key == kOid) return Cell(relation.object_id);
  if (key == kActivity || key == kTimestamp) return event_cell(event, key);
  if (key == kType) return Cell(object.type);
  if (key == kQualifier)
    return relation.qualifier_kind == OcelQualifierKind::kStr ? Cell(relation.qualifier) : Cell::other();
  // A declared relations frame column beyond the six (a frame built by hand,
  // as in the retained public bank) has no native storage: its cell is the
  // event's attribute of that name, else the object's.
  if (const auto cell = event_cell(event, key); !cell.missing()) return cell;
  return object_cell(object, key);
}
const std::string& label(const Cell& cell, const std::string& key) {
  if (cell.kind == Cell::Kind::kMissing)
    throw std::invalid_argument("OCDFG string association row is missing cell: " + key);
  if (cell.kind == Cell::Kind::kOther)
    throw std::invalid_argument("OCDFG association companion requires string cells: " + key);
  return cell.text;
}
// Round FF: a datetime column's cells (ocel:timestamp, an attribute column
// read as datetime64) are Timestamps in pm4py, one group and one label per
// instant however the texts spell it ("...Z" and "...+00:00", or the same
// instant at two offsets; observed black-box, probe ff t3/tsk2.jsonocel).
// The label is the first spelling in event (object) order.
class InstantLabels {
 public:
  explicit InstantLabels(bool active) : active_(active) {}
  Cell operator()(Cell cell) {
    if (!active_ || cell.kind != Cell::Kind::kText) return cell;
    const auto parsed = parse_timestamp_exact(cell.text);
    if (!parsed) return cell;
    cell.text = seen_.try_emplace({parsed->seconds, parsed->nanoseconds}, cell.text).first->second;
    return cell;
  }

 private:
  bool active_;
  std::map<std::pair<std::int64_t, std::uint32_t>, std::string> seen_;
};

// A groupby key: a missing cell is no group; another value rejects.
const std::string* group_key(const Cell& cell, const std::string& key) {
  if (cell.missing()) return nullptr;
  return &label(cell, key);
}

template <class Output, class Project>
Output aggregate(const OcdfgEdgeAssociations& input, Project project) {
  Output result;
  for (const auto& [type, edges] : input) {
    auto& projected = result[type];
    for (const auto& [edge, associations] : edges) {
      auto& values = projected[edge];
      for (const auto& association : associations) values.insert(project(association));
    }
  }
  return result;
}

}  // namespace

OcdfgEdgeAssociations find_ocdfg_edge_associations(
    const OcdfgAssociationTable& table, const OcdfgAssociationOptions& options) {
  const auto eid = options.event_id_attribute.value_or("ocel:eid");
  const auto oid = options.object_id_attribute.value_or("ocel:oid");
  const auto type = options.object_type_attribute.value_or("ocel:type");
  const auto activity = options.activity_attribute.value_or("ocel:activity");
  require_column(table, eid, false);
  require_column(table, oid, true);
  require_column(table, type, true);
  require_column(table, activity, true);
  struct Previous { std::string event; std::string activity; std::string type; };
  std::map<std::string, Previous> previous;
  OcdfgEdgeAssociations result;
  for (const auto& row : table.rows) {
    const auto event_id = string_cell(row, eid), object_id = string_cell(row, oid);
    const auto object_type = string_cell(row, type), label = string_cell(row, activity);
    const auto found = previous.find(object_id);
    if (found != previous.end()) {
      if (found->second.type != object_type)
        throw std::invalid_argument("OCDFG string association companion requires a stable type for each object");
      result[object_type][{found->second.activity, label}].emplace_back(
          found->second.event, event_id, object_id);
    }
    previous[object_id] = {event_id, label, object_type};
  }
  return result;
}

OcdfgEdgeAssociations find_ocdfg_edge_associations(
    const Ocel& log, const OcdfgAssociationOptions& options) {
  const auto eid = options.event_id_attribute.value_or(kEid);
  const auto oid = options.object_id_attribute.value_or(kOid);
  const auto type = options.object_type_attribute.value_or(kType);
  const auto activity = options.activity_attribute.value_or(kActivity);
  const auto events = event_columns(log);
  if (!events.contains(eid)) key_error(eid);
  if (!events.contains(activity)) key_error("Column not found: " + activity);
  const auto relations = relation_columns(log);
  if (!relations.contains(eid)) key_error(eid);
  if (!relations.contains(oid)) key_error("Column not found: " + oid);
  const auto objects = object_columns(log);
  if (!objects.contains(oid)) key_error(oid);
  if (!objects.contains(type)) key_error("Column not found: " + type);

  const auto event_frame = ocel_frame::event_frame(log, true);
  const auto object_frame = ocel_frame::object_frame(log, true);
  const auto events_reading = [&](const std::string& key) {
    return Reading{&log.event_type_attributes, event_frame.dtype(key) == ocel_frame::Dtype::kDatetime};
  };
  const auto objects_reading = [&](const std::string& key) {
    return Reading{&log.object_type_attributes, object_frame.dtype(key) == ocel_frame::Dtype::kDatetime};
  };
  const auto eid_reading = events_reading(eid), activity_reading = events_reading(activity);
  const auto oid_reading = objects_reading(oid), type_reading = objects_reading(type);
  InstantLabels eid_instants(eid == kTimestamp || eid_reading.date_column),
      activity_instants(activity == kTimestamp || activity_reading.date_column),
      type_instants(type_reading.date_column);

  // Round FH: a missing event time (an OCEL CSV row's NaT) as the event id
  // is no group: pm4py's groupby drops the NaT key, so the event takes no
  // part in any edge (observed black-box, bank q34's files with an empty
  // time cell, fh q/emg2.py).
  const auto id_cell = [&](Cell cell) {
    if (eid == kTimestamp && cell.kind == Cell::Kind::kText && cell.text.empty()) return Cell();
    return eid_instants(std::move(cell));
  };
  std::vector<Cell> identifiers;
  std::unordered_map<std::string, Cell> activities;
  identifiers.reserve(log.events.size());
  for (const auto& event : log.events) {
    identifiers.push_back(id_cell(event_cell(event, eid, eid_reading)));
    auto label_cell = activity_instants(event_cell(event, activity, activity_reading));
    // Round FF: a missing event time (an OCEL CSV row's NaT) as the activity
    // is the label NaT, as the OC-DFG's edges have it (observed black-box,
    // bank q30, ff q/emr.py).
    if (activity == kTimestamp && label_cell.kind == Cell::Kind::kText && label_cell.text.empty())
      label_cell = Cell(std::string("NaT"));
    // Round FG: so is a datetime64 attribute column's missing cell (observed
    // black-box, probe fg t1/p12.py on t1/g3.json).
    if (activity_reading.date_column && label_cell.missing()) label_cell = Cell(std::string("NaT"));
    if (const auto* key = group_key(identifiers.back(), eid))
      activities.try_emplace(*key, std::move(label_cell));
  }
  std::unordered_map<std::string, const OcelEvent*> event_by_id;
  std::unordered_map<std::string, const OcelObject*> object_by_id;
  for (const auto& event : log.events) event_by_id.emplace(event.id, &event);
  for (const auto& object : log.objects) object_by_id.emplace(object.id, &object);
  std::unordered_map<std::string, std::vector<std::string>> omap;
  for (const auto& relation : log.relations) {
    const auto event = event_by_id.find(relation.event_id);
    const auto object = object_by_id.find(relation.object_id);
    if (event == event_by_id.end() || object == object_by_id.end())
      throw std::invalid_argument("OCDFG association adapter requires source events and objects for each relation");
    const auto key = id_cell(relation_cell(relation, *event->second, *object->second, eid));
    const auto* group = group_key(key, eid);
    if (group == nullptr) continue;
    omap[*group].push_back(label(relation_cell(relation, *event->second, *object->second, oid), oid));
  }
  std::unordered_map<std::string, Cell> objtypes;
  for (const auto& object : log.objects) {
    const auto key = object_cell(object, oid, oid_reading);
    auto type_cell = type_instants(object_cell(object, type, type_reading));
    if (type_reading.date_column && type_cell.missing()) type_cell = Cell(std::string("NaT"));
    if (const auto* group = group_key(key, oid)) objtypes.try_emplace(*group, std::move(type_cell));
  }

  std::unordered_map<std::string, std::string> history;
  OcdfgEdgeAssociations result;
  for (const auto& cell : identifiers) {
    if (cell.kind != Cell::Kind::kText) continue;
    const auto* evid = &cell.text;
    const auto related = omap.find(*evid);
    if (related == omap.end()) continue;
    for (const auto& obj : related->second) {
      const auto previous = history.find(obj);
      if (previous != history.end()) {
        const auto found_type = objtypes.find(obj);
        if (found_type == objtypes.end()) key_error(obj);
        const auto& object_type = label(found_type->second, type);
        const auto& source = label(activities.at(previous->second), activity);
        const auto& target = label(activities.at(*evid), activity);
        result[object_type][{source, target}].emplace_back(previous->second, *evid, obj);
      }
      history[obj] = *evid;
    }
  }
  return result;
}

OcdfgEventCoupleAggregation aggregate_ocdfg_event_couples(const OcdfgEdgeAssociations& input) {
  return aggregate<OcdfgEventCoupleAggregation>(input, [](const auto& row) {
    return std::pair{std::get<0>(row), std::get<1>(row)};
  });
}

OcdfgTotalObjectAggregation aggregate_ocdfg_total_objects(const OcdfgEdgeAssociations& input) {
  return aggregate<OcdfgTotalObjectAggregation>(input, [](const auto& row) { return row; });
}

OcdfgUniqueObjectAggregation aggregate_ocdfg_unique_objects(const OcdfgEdgeAssociations& input) {
  return aggregate<OcdfgUniqueObjectAggregation>(input, [](const auto& row) { return std::get<2>(row); });
}

}  // namespace minera
