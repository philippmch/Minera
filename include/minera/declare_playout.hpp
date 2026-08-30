#pragma once

#include "minera/declare.hpp"
#include "minera/event_log.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace minera {

struct DeclarePlayoutOptions {
  std::size_t n_traces = 1000;
  std::size_t min_length = 3;
  std::size_t max_length = 15;
  std::optional<std::uint64_t> seed;
};

// Classic prefix-safe random playout for a Declare model. Obligations are not
// finalized, so generated traces may end with pending response/existence
// requirements. `seed` is a deterministic Minera extension.
[[nodiscard]] EventLog declare_playout(
    const DeclareModel& model, const DeclarePlayoutOptions& options = {});

// Naming-compatible convenience alias.
[[nodiscard]] inline EventLog playout(
    const DeclareModel& model, const DeclarePlayoutOptions& options = {}) {
  return declare_playout(model, options);
}

}  // namespace minera
