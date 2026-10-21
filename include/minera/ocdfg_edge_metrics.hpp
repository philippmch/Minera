#pragma once

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

using OcdfgEdgeKey = std::pair<std::string, std::string>;
using OcdfgEdgeAssociation = std::tuple<std::string, std::string, std::string>;
using OcdfgEdgeAssociations =
    std::map<std::string, std::map<OcdfgEdgeKey, std::vector<OcdfgEdgeAssociation>>>;
using OcdfgEventCoupleAggregation =
    std::map<std::string, std::map<OcdfgEdgeKey, std::set<std::pair<std::string, std::string>>>>;
using OcdfgTotalObjectAggregation =
    std::map<std::string, std::map<OcdfgEdgeKey, std::set<OcdfgEdgeAssociation>>>;
using OcdfgUniqueObjectAggregation =
    std::map<std::string, std::map<OcdfgEdgeKey, std::set<std::string>>>;

struct OcdfgAssociationOptions {
  // Omitted selectors use canonical columns; supplied empty names are literal.
  std::optional<std::string> event_id_attribute;
  std::optional<std::string> object_id_attribute;
  std::optional<std::string> object_type_attribute;
  std::optional<std::string> activity_attribute;
  // Retained but inactive in association extraction, as in the public bank.
  std::optional<std::string> timestamp_attribute;
};

// The direct companion retains the public relation table's row order and
// declared columns. This bounded native carrier supports string identity,
// activity and object-type cells; non-string or missing row cells reject.
struct OcdfgAssociationTable {
  std::vector<std::string> columns;
  std::vector<std::map<std::string, OcelValue>> rows;
};

class OcdfgEdgeMetricsError : public std::invalid_argument {
 public:
  OcdfgEdgeMetricsError(std::string type, std::string message)
      : std::invalid_argument(message), type(std::move(type)) {}
  const std::string type;
};

[[nodiscard]] OcdfgEdgeAssociations find_ocdfg_edge_associations(
    const OcdfgAssociationTable&, const OcdfgAssociationOptions& = {});
[[nodiscard]] OcdfgEdgeAssociations find_ocdfg_edge_associations(
    const Ocel&, const OcdfgAssociationOptions& = {});
[[nodiscard]] OcdfgEventCoupleAggregation aggregate_ocdfg_event_couples(
    const OcdfgEdgeAssociations&);
[[nodiscard]] OcdfgTotalObjectAggregation aggregate_ocdfg_total_objects(
    const OcdfgEdgeAssociations&);
[[nodiscard]] OcdfgUniqueObjectAggregation aggregate_ocdfg_unique_objects(
    const OcdfgEdgeAssociations&);

}  // namespace minera
