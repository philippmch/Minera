#include "minera/organizational_util.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace minera {
namespace {

// The event's value for a key: nullopt when absent, a nullopt value when null.
std::optional<OrganizationalValue> event_value(const Event& event, const std::string& key) {
  if (key == "concept:name") {
    if (!event.has_activity()) return std::nullopt;
    if (event.activity_state == FirstClassAttributeState::kNull) return OrganizationalValue{};
    return OrganizationalValue{event.activity};
  }
  if (key == "org:resource") {
    if (!event.has_resource()) return std::nullopt;
    if (event.resource_state == FirstClassAttributeState::kNull) return OrganizationalValue{};
    return OrganizationalValue{event.resource};
  }
  for (const auto& attribute : event.attributes) {
    if (attribute.key != key) continue;
    if (attribute.is_null) return OrganizationalValue{};
    return OrganizationalValue{attribute.value};
  }
  return std::nullopt;
}

void count_into(OrganizationalNested& nested, const OrganizationalValue& outer,
                const OrganizationalValue& inner, std::size_t amount = 1) {
  auto row = std::find_if(nested.begin(), nested.end(), [&](const auto& entry) { return entry.first == outer; });
  if (row == nested.end()) {
    nested.push_back({outer, {}});
    row = std::prev(nested.end());
  }
  auto cell = std::find_if(row->second.begin(), row->second.end(),
                           [&](const auto& entry) { return entry.first == inner; });
  if (cell == row->second.end()) row->second.push_back({inner, amount});
  else cell->second += amount;
}

std::optional<std::size_t> find_column(const EventTable& table, const std::string& name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::size_t column(const EventTable& table, const std::string& name) {
  const auto found = find_column(table, name);
  if (!found) throw OrganizationalUtilError("KeyError", "'" + name + "'");
  return *found;
}

// A cell's grouping value; nullopt for missing, null and NaN cells.
std::optional<std::string> cell_value(const EventTable::Row& row, std::size_t index) {
  const auto& cell = row[index];
  if (!cell || cell->is_null || cell->kind == AttributeKind::kNA) return std::nullopt;
  if (const auto* real = std::get_if<double>(&cell->typed_value); real && std::isnan(*real)) return std::nullopt;
  return cell->value;
}

// groupby([first, second]).count()[counted] in sorted group order.
std::map<std::pair<std::string, std::string>, std::size_t> grouped_counts(
    const EventTable& table, std::size_t first, std::size_t second, std::size_t counted) {
  std::map<std::pair<std::string, std::string>, std::size_t> counts;
  for (const auto& row : table.rows) {
    const auto a = cell_value(row, first);
    const auto b = cell_value(row, second);
    if (!a || !b) continue;
    auto& count = counts[{*a, *b}];
    if (cell_value(row, counted)) ++count;
  }
  return counts;
}

}  // namespace

OrganizationalNested organizational_groups_from_log(const EventLog& log, const OrganizationalUtilKeys& keys) {
  OrganizationalNested groups;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      const auto activity = event_value(event, keys.activity_key);
      const auto resource = event_value(event, keys.resource_key);
      const auto group = event_value(event, keys.group_key);
      if (activity && resource && group) count_into(groups, *group, *resource);
    }
  return groups;
}

OrganizationalNested organizational_groups_from_log(const EventTable& table, const OrganizationalUtilKeys& keys) {
  table.validate();
  const auto activity = column(table, keys.activity_key);
  const auto resource = column(table, keys.resource_key);
  const auto group = column(table, keys.group_key);
  OrganizationalNested groups;
  for (const auto& [pair, count] : grouped_counts(table, resource, group, activity))
    count_into(groups, pair.second, pair.first, count);
  return groups;
}

std::pair<OrganizationalNested, OrganizationalNested> organizational_res_act_from_log(
    const EventLog& log, const OrganizationalUtilKeys& keys) {
  std::pair<OrganizationalNested, OrganizationalNested> result;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      const auto activity = event_value(event, keys.activity_key);
      const auto resource = event_value(event, keys.resource_key);
      if (!activity || !resource) continue;
      count_into(result.first, *resource, *activity);
      count_into(result.second, *activity, *resource);
    }
  return result;
}

std::pair<OrganizationalNested, OrganizationalNested> organizational_res_act_from_log(
    const EventTable& table, const OrganizationalUtilKeys& keys) {
  table.validate();
  const auto activity = column(table, keys.activity_key);
  const auto resource = column(table, keys.resource_key);
  const auto group = column(table, keys.group_key);
  std::pair<OrganizationalNested, OrganizationalNested> result;
  for (const auto& [pair, count] : grouped_counts(table, activity, resource, group)) {
    count_into(result.first, pair.second, pair.first, count);
    count_into(result.second, pair.first, pair.second, count);
  }
  return result;
}

OrganizationalCounts organizational_resources_from_log(const EventLog& log, const OrganizationalUtilKeys& keys) {
  OrganizationalCounts counts;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      const auto resource = event_value(event, keys.resource_key);
      if (!resource) throw OrganizationalUtilError("KeyError", "'" + keys.resource_key + "'");
      const auto found = std::find_if(counts.begin(), counts.end(),
                                      [&](const auto& entry) { return entry.first == *resource; });
      if (found == counts.end()) counts.push_back({*resource, 1});
      else ++found->second;
    }
  return counts;
}

OrganizationalCounts organizational_resources_from_log(const EventTable& table, const OrganizationalUtilKeys& keys) {
  table.validate();
  const auto resource = column(table, keys.resource_key);
  OrganizationalCounts counts;
  for (const auto& row : table.rows) {
    const auto value = cell_value(row, resource);
    if (!value) continue;
    const auto found = std::find_if(counts.begin(), counts.end(),
                                    [&](const auto& entry) { return entry.first == value; });
    if (found == counts.end()) counts.push_back({value, 1});
    else ++found->second;
  }
  std::stable_sort(counts.begin(), counts.end(),
                   [](const auto& left, const auto& right) { return left.second > right.second; });
  return counts;
}

}  // namespace minera
