#pragma once

#include <string>

#include "minera/event_data.hpp"

namespace minera {

// Retains the existing log-conversion table wire format. Does not validate
// column uniqueness, so rejected formatter inputs can also be reported.
[[nodiscard]] std::string serialize_event_table_json(const EventTable& table);

}  // namespace minera
