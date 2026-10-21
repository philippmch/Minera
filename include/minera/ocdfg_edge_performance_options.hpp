#pragma once
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "minera/temporal_profile.hpp"

namespace minera {
// Omitted, explicit null, numeric slots and malformed string slot entries
// stay distinct. Numeric slots intentionally retain zero/reversed/outside-week
// endpoints: the captured public working-time arithmetic accepts those.
using OcdfgBusinessHourSlots =
    std::variant<std::monostate, std::nullptr_t, std::vector<BusinessHourSlot>,
                 std::vector<std::string>>;

struct OcdfgEdgePerformanceOptions {
  bool business_hours = false;
  OcdfgBusinessHourSlots business_hour_slots;
  // Absent selects the canonical column; a supplied empty key is literal.
  std::optional<std::string> event_id_attribute;
  std::optional<std::string> timestamp_attribute;
  // Retained, inert parameter. None, false, string and dictionary calendar
  // arguments all had no effect in the saved public bank. No holiday-calendar
  // protocol or callback execution is inferred from those observations.
  std::string workcalendar_json = "null";
};

}  // namespace minera
