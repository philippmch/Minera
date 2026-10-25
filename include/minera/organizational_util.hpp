#pragma once

// algo.organizational_mining.util of the PM4Py 2.7.22 reference: resource,
// group and activity counts of an event log or an event table.
//
// On an EventLog, a key is present on an event when the event carries it
// (concept:name and org:resource are the first-class activity and resource
// fields, other keys are attributes); a present null value counts as its
// own key (std::nullopt, the reference's None). Counts follow the events in
// log order. On an EventTable, the reference groups with pandas: null cells
// are left out of the grouping keys, groups come in sorted key order, and
// a group's value is the number of non-null cells of the remaining column.

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct OrganizationalUtilKeys {
  std::string activity_key = "concept:name";
  std::string resource_key = "org:resource";
  std::string group_key = "org:group";
};

using OrganizationalValue = std::optional<std::string>;  // nullopt is None
using OrganizationalCounts = std::vector<std::pair<OrganizationalValue, std::size_t>>;
using OrganizationalNested = std::vector<std::pair<OrganizationalValue, OrganizationalCounts>>;

class OrganizationalUtilError : public std::invalid_argument {
 public:
  OrganizationalUtilError(std::string type, const std::string& message)
      : std::invalid_argument(message), type(std::move(type)) {}
  std::string type;
};

// get_groups_from_log: group -> resource -> count. A log counts the events
// that carry the activity, resource and group keys; a table counts the
// non-null activities of each (resource, group) row group, keyed by group in
// sorted (resource, group) order.
[[nodiscard]] OrganizationalNested organizational_groups_from_log(
    const EventLog& log, const OrganizationalUtilKeys& keys = {});
[[nodiscard]] OrganizationalNested organizational_groups_from_log(
    const EventTable& table, const OrganizationalUtilKeys& keys = {});

// get_res_act_from_log: (resource -> activity -> count,
// activity -> resource -> count). A log counts the events that carry both
// keys; a table counts the non-null groups of each (activity, resource) row
// group in sorted (activity, resource) order.
[[nodiscard]] std::pair<OrganizationalNested, OrganizationalNested> organizational_res_act_from_log(
    const EventLog& log, const OrganizationalUtilKeys& keys = {});
[[nodiscard]] std::pair<OrganizationalNested, OrganizationalNested> organizational_res_act_from_log(
    const EventTable& table, const OrganizationalUtilKeys& keys = {});

// get_resources_from_log: resource -> number of events. A log raises
// KeyError when an event lacks the resource key and counts in first
// appearance; a table counts the non-null resources by descending count,
// ties in first appearance.
[[nodiscard]] OrganizationalCounts organizational_resources_from_log(
    const EventLog& log, const OrganizationalUtilKeys& keys = {});
[[nodiscard]] OrganizationalCounts organizational_resources_from_log(
    const EventTable& table, const OrganizationalUtilKeys& keys = {});

}  // namespace minera
