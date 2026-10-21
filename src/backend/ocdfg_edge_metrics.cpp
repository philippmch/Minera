#include "minera/ocdfg_edge_metrics.hpp"

#include <algorithm>

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

OcdfgAssociationTable relation_table(const Ocel& log) {
  OcdfgAssociationTable table;
  if (log.table_columns.relations) table.columns = *log.table_columns.relations;
  else {
    std::set<std::string> columns = {"ocel:eid", "ocel:activity", "ocel:timestamp",
                                     "ocel:oid", "ocel:type", "ocel:qualifier"};
    for (const auto& event : log.events)
      for (const auto& attribute : event.attributes) columns.insert(attribute.key);
    for (const auto& object : log.objects)
      for (const auto& attribute : object.attributes) columns.insert(attribute.key);
    for (const auto& [type, schema] : log.event_type_attributes) {
      (void)type;
      for (const auto& [key, value] : schema) { (void)value; columns.insert(key); }
    }
    for (const auto& [type, schema] : log.object_type_attributes) {
      (void)type;
      for (const auto& [key, value] : schema) { (void)value; columns.insert(key); }
    }
    table.columns.assign(columns.begin(), columns.end());
  }
  std::map<std::string, const OcelEvent*> events;
  std::map<std::string, const OcelObject*> objects;
  for (const auto& event : log.events) events[event.id] = &event;
  for (const auto& object : log.objects) objects[object.id] = &object;
  for (const auto& relation : log.relations) {
    const auto event = events.find(relation.event_id);
    const auto object = objects.find(relation.object_id);
    if (event == events.end() || object == objects.end())
      throw std::invalid_argument("OCDFG association adapter requires source events and objects for each relation");
    std::map<std::string, OcelValue> row;
    for (const auto& attribute : event->second->attributes) row.emplace(attribute.key, attribute.value);
    for (const auto& attribute : object->second->attributes) row.emplace(attribute.key, attribute.value);
    row["ocel:eid"] = relation.event_id;
    row["ocel:activity"] = event->second->activity;
    row["ocel:timestamp"] = event->second->timestamp;
    row["ocel:oid"] = relation.object_id;
    row["ocel:type"] = object->second->type;
    row["ocel:qualifier"] = relation.qualifier;
    table.rows.push_back(std::move(row));
  }
  return table;
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
  return find_ocdfg_edge_associations(relation_table(log), options);
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
