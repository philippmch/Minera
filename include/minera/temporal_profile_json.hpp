#pragma once

#include <string>
#include <string_view>

#include "minera/temporal_profile.hpp"

namespace minera {

// Profile moment tuples [source, target, mean_seconds, stddev_seconds].
// Labels use ordinary JSON strings, independently of request separators.
[[nodiscard]] TemporalProfile parse_temporal_profile_json(std::string_view json);
[[nodiscard]] std::string temporal_profile_json(const TemporalProfile& profile);

}  // namespace minera
