#pragma once

// check_conformance where an event's null activity fires one of the net's
// silent transitions (the one the full replay's transition map picks: an
// enabled one, else the last by name), as pm4py's token replay does for a
// Polars frame's None label (round GD, observed black-box).

#include "minera/conformance.hpp"

namespace minera::detail {

[[nodiscard]] ConformanceResult check_conformance_null_silent(const EventLog& event_log,
                                                              const PetriNet& net,
                                                              TokenReplayOptions options = {});

}  // namespace minera::detail
