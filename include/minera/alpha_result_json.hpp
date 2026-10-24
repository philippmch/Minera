#pragma once

#include <string>
#include <vector>
#include "minera/alpha_miner.hpp"

namespace minera {
// Preserve live and detached markings without converting them into a workflow
// source/sink pair. Detached places remain distinct from same-named live places.
[[nodiscard]] std::string alpha_plus_result_json(const AlphaPlusDiscoveryResult& result);
// Every reachable boundary-cleanup alternative, each a raw result or an error.
[[nodiscard]] std::string alpha_plus_cleanup_alternatives_json(
    const std::vector<AlphaPlusCleanupAlternative>& alternatives);
}
