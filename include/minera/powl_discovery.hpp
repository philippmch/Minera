#pragma once

#include "minera/event_log.hpp"
#include "minera/powl.hpp"

#include <cstddef>

namespace minera {

// Clean-room implementations of the public PM4Py POWL discovery variants.
// kDynamicClustering is PM4Py's frequency based dynamic clustering variant;
// kDynamicClusteringPlain exposes the otherwise-unreachable unweighted
// implementation so callers can select every source variation explicitly.
enum class PowlDiscoveryVariant {
  kTree,
  kBruteForce,
  kMaximal,
  kDynamicClustering,
  kDynamicClusteringPlain,
  TREE = kTree,
  BRUTE_FORCE = kBruteForce,
  MAXIMAL = kMaximal,
  DYNAMIC_CLUSTERING = kDynamicClustering,
  DYNAMIC_CLUSTERING_PLAIN = kDynamicClusteringPlain,
};

enum class PowlFilteringType {
  kDecreasingFactor,
  kDynamic,
  DECREASING_FACTOR = kDecreasingFactor,
  DYNAMIC = kDynamic,
};

struct PowlDiscoveryOptions {
  PowlDiscoveryVariant variant = PowlDiscoveryVariant::kMaximal;
  PowlFilteringType filtering_type = PowlFilteringType::kDecreasingFactor;
  double filtering_threshold = 0.0;
  double order_frequency_ratio = 1.0;
  // Zero means unlimited. A positive value fails before brute-force
  // partition enumeration when the local alphabet is larger than this cap.
  std::size_t brute_force_activity_limit = 0;
};

[[nodiscard]] PowlNode discover_powl(
    const EventLog& event_log, const PowlDiscoveryOptions& options = {});

}  // namespace minera
