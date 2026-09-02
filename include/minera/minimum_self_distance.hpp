#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <initializer_list>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

enum class MinimumSelfDistanceVariant {
  kLog,
  kPandas,
  kPolars,
  LOG = kLog,
  PANDAS = kPandas,
  POLARS = kPolars,
};

struct MinimumSelfDistanceOptions {
  MinimumSelfDistanceVariant variant = MinimumSelfDistanceVariant::kLog;
  // Event activity and case-id selectors. Built-in aliases use the native
  // first-class fields; other names resolve typed event/trace attributes.
  std::string activity_key = "concept:name";
  std::string case_id_key = "case:concept:name";
};

struct MinimumSelfDistance {
  std::string activity;
  std::uint64_t distance = 0;
  std::vector<std::string> witnesses;

  friend bool operator==(const MinimumSelfDistance&,
                         const MinimumSelfDistance&) = default;
};

using MinimumSelfDistances = std::vector<MinimumSelfDistance>;

using MinimumSelfDistanceWitnessMap =
    std::map<std::string, std::set<std::string>>;

// Discovers the minimum number of events strictly between consecutive
// occurrences of each repeated activity, across all traces. Activities that
// occur at most once in every trace are omitted. Witnesses are the sorted
// union of activities occurring in every minimum-distance segment.
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    const EventLog& log,
    const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    std::initializer_list<Trace> traces,
    const MinimumSelfDistanceOptions& options = {});

// Flat adapters use the selected case field as the grouping key and retain
// the source row order within each group. The EventLog overload keeps its
// enclosing traces as the boundaries, as required by its trace-shaped input.
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    const EventStream& stream,
    const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    const EventTable& table,
    const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    const LazyEventTable& table,
    const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    const EventDataView& data,
    const MinimumSelfDistanceOptions& options = {});

// Derives the activity witnesses for a supplied minimum-distance map. Entries
// with distance zero, negative values, or no observed distance have no
// witnesses. When the map is omitted, distances are discovered first.
[[nodiscard]] MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log, const std::map<std::string, std::uint64_t>& msd,
    const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log, const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log,
    const std::map<std::string, std::uint64_t>* msd,
    const MinimumSelfDistanceOptions& options = {});
[[nodiscard]] MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log, const MinimumSelfDistances& distances,
    const MinimumSelfDistanceOptions& options = {});

[[nodiscard]] MinimumSelfDistanceWitnessMap
derive_minimum_self_distance_witnesses(
    const EventLog& log, const MinimumSelfDistances& distances,
    const MinimumSelfDistanceOptions& options = {});

class MinimumSelfDistanceWitnessDerivation {
 public:
  [[nodiscard]] static MinimumSelfDistanceWitnessMap derive(
      const EventLog& log, const std::map<std::string, std::uint64_t>& msd,
      const MinimumSelfDistanceOptions& options = {}) {
    return derive_msd_witnesses(log, msd, options);
  }
  [[nodiscard]] static MinimumSelfDistanceWitnessMap derive(
      const EventLog& log, const MinimumSelfDistanceOptions& options = {}) {
    return derive_msd_witnesses(log, options);
  }
  [[nodiscard]] static MinimumSelfDistanceWitnessMap derive(
      const EventLog& log, const MinimumSelfDistances& distances,
      const MinimumSelfDistanceOptions& options = {}) {
    return derive_minimum_self_distance_witnesses(log, distances, options);
  }
};

using MinimumSelfDistanceWitnesses = MinimumSelfDistanceWitnessDerivation;

// Distinct adapter-shaped callables retain the public variation names while
// sharing the same deterministic native result representation.
class MinimumSelfDistanceLogDiscovery {
 public:
  [[nodiscard]] static MinimumSelfDistances apply(
      const EventLog& log, MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances apply(
      const EventStream& stream, MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances apply(
      const EventDataView& data, MinimumSelfDistanceOptions options = {});
};

class MinimumSelfDistancePandasDiscovery {
 public:
  [[nodiscard]] static MinimumSelfDistances apply(
      const EventTable& table, MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances apply(
      const EventDataView& data, MinimumSelfDistanceOptions options = {});
};

class MinimumSelfDistancePolarsDiscovery {
 public:
  [[nodiscard]] static MinimumSelfDistances apply(
      const LazyEventTable& table, MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances apply(
      const EventDataView& data, MinimumSelfDistanceOptions options = {});
};

enum class MinimumSelfDistanceSelector {
  kLog,
  kPandas,
  kPolars,
  LOG = kLog,
  PANDAS = kPandas,
  POLARS = kPolars,
};

struct MinimumSelfDistanceSelectorDescriptor {
  MinimumSelfDistanceSelector selector = MinimumSelfDistanceSelector::kLog;
  std::string name;
};

class MinimumSelfDistanceSelectorRegistry {
 public:
  [[nodiscard]] static const std::vector<
      MinimumSelfDistanceSelectorDescriptor>& all();
  [[nodiscard]] static bool supports(
      MinimumSelfDistanceSelector selector) noexcept;
  [[nodiscard]] static std::optional<MinimumSelfDistanceSelector> parse(
      std::string_view name);

  [[nodiscard]] static MinimumSelfDistances dispatch(
      const EventLog& log, MinimumSelfDistanceSelector selector,
      MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances dispatch(
      const EventStream& stream, MinimumSelfDistanceSelector selector,
      MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances dispatch(
      const EventTable& table, MinimumSelfDistanceSelector selector,
      MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances dispatch(
      const LazyEventTable& table, MinimumSelfDistanceSelector selector,
      MinimumSelfDistanceOptions options = {});
  [[nodiscard]] static MinimumSelfDistances dispatch(
      const EventDataView& data, MinimumSelfDistanceSelector selector,
      MinimumSelfDistanceOptions options = {});
};

[[nodiscard]] std::string_view minimum_self_distance_variant_name(
    MinimumSelfDistanceSelector selector);
[[nodiscard]] MinimumSelfDistanceSelector parse_minimum_self_distance_variant(
    std::string_view name);

}  // namespace minera
