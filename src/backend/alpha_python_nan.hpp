#pragma once

#include <string>

#include "minera/alpha_miner.hpp"

namespace minera::detail {

// Alpha+ on a pandas frame's EventLog, where `nan_label` stands for Python's
// NaN (round GA): one key in the directly-follows counts, but nan == nan is
// False, so it is never a length-one loop and never closes "a b a".
[[nodiscard]] AlphaPlusDiscoveryResult discover_alpha_plus_result_python_nan(
    const EventLog& log, const AlphaMinerOptions& options, const std::string& nan_label);

}  // namespace minera::detail
