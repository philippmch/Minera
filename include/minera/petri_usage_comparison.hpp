#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Usage of one numbered net element in one or both logs.  IDs are stable net
// indices (rather than labels), so duplicate transition labels remain distinct.
struct PetriUsageRecord {
  std::size_t id = 0;
  std::uint64_t log1_occurrences = 0;
  std::uint64_t log2_occurrences = 0;
  std::uint64_t total_occurrences = 0;
  double direction = 0.0;  // (log2 - log1) / total, or zero for total == 0
};

// Arc usage is associated with the transition that owns the arc.  `id` is a
// deterministic ordinal in the flattened input/output arc lists; repeated
// place IDs therefore preserve weighted arcs instead of being silently lost.
struct PetriArcUsageRecord {
  std::size_t id = 0;
  std::size_t transition = 0;
  std::size_t place = 0;
  bool input = true;
  std::uint64_t log1_occurrences = 0;
  std::uint64_t log2_occurrences = 0;
  std::uint64_t total_occurrences = 0;
  double direction = 0.0;
};

struct PetriUsageComparison {
  std::vector<PetriUsageRecord> places;
  std::vector<PetriUsageRecord> transitions;
  std::vector<PetriArcUsageRecord> arcs;
};

// Compares forward token-replay element activations for two logs.  Replay is
// deliberately robust: unknown activities and missing tokens are counted as
// deviations, while the corresponding forced transition still counts as
// activated.  Place usage is consumed-token count plus final remaining-token
// count, matching the replay bookkeeping convention.
[[nodiscard]] PetriUsageComparison compare_petri_usage(
    const PetriNet& net, const EventLog& log1, const EventLog& log2);

// Descriptive compatibility spelling for callers that use the family name.
[[nodiscard]] PetriUsageComparison compare_element_usage_two_logs(
    const PetriNet& net, const EventLog& log1, const EventLog& log2);

}  // namespace minera
