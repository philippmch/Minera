#pragma once

#include <string_view>

#include "minera/ocel.hpp"

namespace minera {

// Returns a discovery view with the selected event activity and object type
// labels projected into the canonical Ocel fields. IDs, relations, timestamps,
// and all source attributes remain unchanged.
Ocel project_ocpn_labels(const Ocel& log,
                         const OcpnDiscoveryOptions& options);

// Shared projection used by object-centric discovery families. It copies the
// log and deterministically renders selected scalar attributes into the
// canonical activity/type labels while preserving IDs and source attributes.
Ocel project_ocel_labels(const Ocel& log, std::string_view activity_attribute,
                         std::string_view object_type_attribute,
                         std::string_view family);

}  // namespace minera
