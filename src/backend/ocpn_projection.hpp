#pragma once

#include "minera/ocel.hpp"

namespace minera {

// Returns a discovery view with the selected event activity and object type
// labels projected into the canonical Ocel fields. IDs, relations, timestamps,
// and all source attributes remain unchanged.
Ocel project_ocpn_labels(const Ocel& log,
                         const OcpnDiscoveryOptions& options);

}  // namespace minera
