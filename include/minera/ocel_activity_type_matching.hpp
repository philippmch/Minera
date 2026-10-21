#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

struct OcelActivityTypeMatchingOptions {
  // Public dataframe adapters normalize configured role aliases to these
  // canonical native field names; other names remain literal selectors.
  std::string activity_key = "ocel:activity";
  std::string object_type_key = "ocel:type";
  std::optional<std::string> temporary_column;
  std::optional<std::string> temporary_separator;
};

[[nodiscard]] Ocel filter_ocel_activity_type_matching_exact(
    const Ocel& log,
    const std::map<std::string, std::vector<std::string>>& correspondence,
    const OcelActivityTypeMatchingOptions& options = {});

}  // namespace minera
