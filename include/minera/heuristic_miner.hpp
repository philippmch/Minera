#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/event_data.hpp"
#include "minera/heuristic_count.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

enum class HeuristicMinerVariant {
  kClassic,
  kPlusPlus,
  CLASSIC = kClassic,
  PLUSPLUS = kPlusPlus,
};

enum class HeuristicNetDecoration {
  kFrequency,
  kPerformance,
  FREQUENCY = kFrequency,
  PERFORMANCE = kPerformance,
};

enum class HeuristicPerformanceAggregation {
  kMean,
  kMedian,
  kMinimum,
  kMaximum,
  kStandardDeviation,
  kSum,
  kRawValues,
  kAll,
};

// The structured performance values returned by the raw_values and all
// aggregations.  Their values mirror the public JSON representation; the
// scalar `representation` fields below remain the compatibility projection.
struct HeuristicPerformanceSummary {
  double mean = 0.0;
  double median = 0.0;
  double minimum = 0.0;
  double maximum = 0.0;
  double stdev = 0.0;
  double sum = 0.0;
};

// An opaque JSON value used by already-aggregated performance DFG inputs.
// Keeping the native integer alternatives separate from double preserves the
// exact value through export, while the recursive containers retain nested
// PM4Py values without imposing a schema on callers.
struct HeuristicStructuredValue {
  using Array = std::vector<HeuristicStructuredValue>;
  using Object = std::map<std::string, HeuristicStructuredValue>;
  using Value = std::variant<std::nullptr_t, bool, std::int64_t,
                             std::uint64_t, double, std::string, Array,
                             Object>;

  Value value = nullptr;
};

using HeuristicPerformanceValue =
    std::variant<double, std::vector<double>, HeuristicPerformanceSummary,
                 HeuristicStructuredValue>;

// A directed dependency between two activities accepted by the Heuristic Miner,
// carrying both how often it was observed and how reliable the ordering is.
struct HeuristicDependency {
  std::size_t source = 0;
  std::size_t target = 0;
  double dependency = 0.0;      // dependency measure in (-1, 1]
  std::uint64_t frequency = 0;  // |source > target|: direct successions observed
  double representation = 0.0;
  bool length_two_loop = false;
  // Typed raw/all output or an already-aggregated DFG value. Empty means use
  // `representation`; opaque non-double DFG values use zero in that legacy
  // scalar field and preserve their complete payload here.
  std::optional<HeuristicPerformanceValue> representation_value;
};

// One bundle of an activity's split (or join): the neighbours it activates
// (or awaits) together. An activity takes exactly one of its bundles — XOR
// across bundles — and all members of the chosen bundle run in parallel.
using HeuristicBundle = std::vector<std::size_t>;
using HeuristicFrequencyDfg =
    std::map<std::pair<std::string, std::string>, std::uint64_t>;
using HeuristicTripleFrequencies =
    std::map<std::tuple<std::string, std::string, std::string>,
             std::uint64_t>;
using HeuristicPerformanceDfg =
    std::map<std::pair<std::string, std::string>, std::vector<double>>;
// Computed mean-valued output, distinct from sample input and supplied
// already-aggregated decorated values.
using HeuristicMeanPerformanceDfg =
    std::map<std::pair<std::string, std::string>, double>;
using HeuristicDecoratedDfg =
    std::map<std::pair<std::string, std::string>, HeuristicPerformanceValue>;

struct HeuristicActivityStatistics {
  std::size_t activity = 0;
  // Widened to preserve exact aggregate counts beyond uint64_t.  Convert with
  // to_uint64() only when a checked native-width value is required.
  HeuristicOccurrenceCount occurrences = 0;
  std::uint64_t start_occurrences = 0;
  std::uint64_t end_occurrences = 0;
  double mean_sojourn_seconds = 0.0;
  // Both variants return these node attributes explicitly. CLASSIC derives
  // boundary membership; PLUSPLUS exposes false even for boundary nodes.
  std::optional<bool> is_start_activity;
  std::optional<bool> is_end_activity;
};

struct HeuristicRelationStatistics {
  std::size_t source = 0;
  std::size_t target = 0;
  std::uint64_t eventually_follows_frequency = 0;
  std::uint64_t direct_adjacency_frequency = 0;
  double mean_wait_seconds = 0.0;
  std::uint64_t raw_frequency = 0;
  std::uint64_t cleaned_frequency = 0;
  double representation = 0.0;
  bool length_two_loop = false;
  std::vector<double> wait_samples;
  double median_wait_seconds = 0.0;
  double min_wait_seconds = 0.0;
  double max_wait_seconds = 0.0;
  double stdev_wait_seconds = 0.0;
  double sum_wait_seconds = 0.0;
  // Typed raw/all output or an already-aggregated DFG value, with the same
  // legacy scalar fallback as HeuristicDependency::representation_value.
  std::optional<HeuristicPerformanceValue> representation_value;
};

struct HeuristicConcurrencyStatistics {
  std::size_t first = 0;
  std::size_t second = 0;
  std::uint64_t frequency = 0;
};

struct HeuristicRawActivityStatistics {
  std::string activity;
  std::uint64_t occurrences = 0;
  double mean_sojourn_seconds = 0.0;
};

struct HeuristicRawConcurrencyStatistics {
  std::string first;
  std::string second;
  std::uint64_t frequency = 0;
};

// One accepted AND relation around an activity.  `activity` is the center
// node; `first` and `second` are the name-sorted neighbours whose score
// cleared the configured AND threshold.
struct HeuristicAndMeasure {
  std::size_t activity = 0;
  std::size_t first = 0;
  std::size_t second = 0;
  double value = 0.0;
};

struct HeuristicDependencyMeasure {
  std::size_t source = 0;
  std::size_t target = 0;
  double value = 0.0;
};

struct HeuristicLoopLengthTwo {
  std::size_t source = 0;
  std::size_t target = 0;
  std::uint64_t frequency = 0;
};

// Boundary statistics in the public DFG object are either an ordered list of
// activity names (direct DFG input) or a name-to-count mapping (event-log
// input).  The JSON wire format wraps each selected value in one outer array.
using HeuristicBoundaryActivities =
    std::variant<std::vector<std::string>,
                 std::map<std::string, std::uint64_t>>;

// Raw metadata retained from the public heuristic-miner input.  This is kept
// separate from the internal activity-id table in HeuristicsNet: callers can
// inspect declared names, duplicate ordering, and unprojected boundary/count
// data without changing the ids used by graph consumers.
struct HeuristicNetMetadata {
  std::vector<std::string> activities;
  // Metadata retains the same widened occurrence totals as activity
  // statistics; conversion back to uint64_t is explicitly checked.
  std::map<std::string, HeuristicOccurrenceCount> activities_occurrences;
  HeuristicBoundaryActivities start_activities;
  HeuristicBoundaryActivities end_activities;
};

// A heuristics net: the dependency graph the Heuristic Miner discovers. Unlike
// the raw directly-follows graph it keeps only edges whose dependency measure
// and frequency clear the configured thresholds, which is what lets it ignore
// the infrequent, noisy paths that mislead the Alpha Miner.
struct HeuristicsNet {
  HeuristicMinerVariant variant = HeuristicMinerVariant::kClassic;
  HeuristicNetDecoration decoration = HeuristicNetDecoration::kFrequency;
  HeuristicPerformanceAggregation performance_aggregation =
      HeuristicPerformanceAggregation::kMean;
  // Names of the event fields used to construct this net. Empty selectors
  // mean the canonical activity and completion-time fields. When activity
  // labels come from a generic field, all ids in this object are local to
  // `activities` and `activity_ids_are_local` is true.
  // The pandas entry point retains literal selectors, including empty names.
  std::string activity_attribute;
  std::string timestamp_attribute;
  bool activity_ids_are_local = false;
  std::vector<std::string> activities;
  std::vector<HeuristicDependency> edges;  // sorted by (source, target)
  std::vector<std::size_t> start_activities;
  std::vector<std::size_t> end_activities;
  // Activities that discovery created as heuristic nodes. CLASSIC can omit isolated
  // filtered activities; PLUSPLUS creates all retained activities.
  std::vector<std::size_t> active_activities;

  // Split/join semantics per activity, derived from the AND measure: outgoing
  // successors grouped into parallel bundles, and incoming predecessors
  // likewise. Indexed by activity id; a self-loop edge is always its own
  // bundle on both sides.
  std::vector<std::vector<HeuristicBundle>> output_bundles;
  std::vector<std::vector<HeuristicBundle>> input_bundles;

  // Variant-specific activity and relation statistics are kept separately
  // from accepted edges so callers can inspect the returned node and edge
  // attributes without changing the graph or losing filtered relation data.
  std::vector<HeuristicActivityStatistics> activity_statistics;
  // Both variants expose an engaged name-to-sojourn mapping, including when
  // empty. PLUSPLUS retains raw activity means even when activities are filtered.
  std::optional<std::map<std::string, double>> sojourn_times;
  std::vector<HeuristicRelationStatistics> relation_statistics;
  std::vector<HeuristicConcurrencyStatistics> concurrency_statistics;
  // Discovery retains these two abstractions before activity/path filtering.
  std::vector<HeuristicRawActivityStatistics> raw_activity_statistics;
  std::vector<HeuristicRawConcurrencyStatistics>
      raw_concurrency_statistics;
  HeuristicFrequencyDfg raw_dfg;
  HeuristicFrequencyDfg cleaned_dfg;
  HeuristicFrequencyDfg window_two_dfg;
  HeuristicTripleFrequencies frequency_triples;
  std::vector<HeuristicAndMeasure> and_measures_in;
  std::vector<HeuristicAndMeasure> and_measures_out;
  std::vector<HeuristicDependencyMeasure> dependency_measures;
  std::vector<HeuristicLoopLengthTwo> loop_length_two;
  std::optional<HeuristicNetMetadata> metadata;
  // Retains an explicitly supplied already-aggregated performance DFG.
  std::optional<HeuristicDecoratedDfg> performance_values;
  // Presence of the two optional public CLASSIC object fields. Defaults keep
  // manually assembled legacy nets projecting their concrete maps as present.
  bool classic_public_dfg_window_2_present = true;
  bool classic_public_freq_triples_present = true;
  // Independent PLUSPLUS public-field presence. Legacy manually assembled
  // nets keep their concrete auxiliary maps present; PLUSPLUS discovery marks
  // its unavailable window-two/triple outputs absent without altering the maps.
  bool plusplus_public_dfg_window_2_present = true;
  bool plusplus_public_freq_triples_present = true;
  // Computed outputs never overload performance_values (an input carrier).
  // Performance decoration engages the DFG before filtering, including an
  // empty result, and includes only retained EFG pairs with direct waits.
  std::optional<HeuristicMeanPerformanceDfg>
      plusplus_computed_performance_dfg;
  // All retained EFG pairs are present. Frequency decoration stores 0.0;
  // performance decoration stores mean wait (0.0 for an EFG-only pair).
  HeuristicMeanPerformanceDfg plusplus_computed_performance_matrix;
};

struct HeuristicMinerOptions {
  HeuristicMinerVariant variant = HeuristicMinerVariant::kClassic;
  // Event field used as the activity label. The standard XES aliases
  // `concept:name` and `activity` use Event::activity; any other name is
  // resolved through Event::attribute.
  std::string activity_attribute = "concept:name";
  // Event field used as the completion timestamp. The standard XES aliases
  // `time:timestamp` and `timestamp` use Event::timestamp; any other name is
  // resolved through Event::attribute.
  std::string timestamp_attribute = "time:timestamp";
  // Minimum dependency measure for an edge to be accepted. Higher values keep
  // only strongly directional relations.
  // Both variants default to 0.5.
  std::optional<double> dependency_threshold;
  // Minimum number of direct successions for an edge to be considered.
  std::uint64_t min_frequency = 1;
  // When true, every activity is connected to its strongest dependency even if
  // that edge is below the threshold, so no task is left dangling.
  bool connect_all = false;
  // Minimum AND measure for two edges leaving (or entering) the same activity
  // to be bundled as a parallel split (or join) rather than an exclusive
  // choice. Two successors b, c of a are parallel when they follow each other
  // often enough relative to the split itself:
  //   (|b>c| + |c>b|) / (|a>b| + |a>c| + 1) >= and_threshold
  // (mirrored for joins). 0.65 is the customary default.
  double and_threshold = 0.65;
  // Minimum activity occurrence filter. Classic applies it while accepting
  // edges; PLUSPLUS applies it while retaining activities and relations.
  std::uint64_t min_activity_count = 1;
  HeuristicNetDecoration decoration = HeuristicNetDecoration::kFrequency;
  HeuristicPerformanceAggregation performance_aggregation =
      HeuristicPerformanceAggregation::kMean;
  double dfg_pre_cleaning_noise_threshold = 0.05;
  double loop_length_two_threshold = 0.5;
  bool keep_once_per_case = false;
  std::set<std::pair<std::string, std::string>> most_common_paths;
  // An omitted selector has variant-specific defaults: CLASSIC uses event
  // completion timestamps, while PLUSPLUS probes the conventional
  // `start_timestamp` attribute before falling back to lifecycle conversion.
  // An explicitly selected nonempty attribute must exist for CLASSIC
  // performance; PLUSPLUS retains its interval/lifecycle detection.
  // An explicit empty string keeps the
  // native completion/lifecycle fallback.
  std::optional<std::string> start_timestamp_attribute = std::nullopt;
  std::string lifecycle_instance_attribute = "concept:instance";
  bool use_business_hours = false;
  // Empty uses the compatible Monday-Friday 07:00-17:00 schedule unless
  // business_hour_slots_explicit is true.
  std::vector<BusinessHourSlot> business_hour_slots;
  // Distinguishes an omitted schedule from an explicitly empty schedule.
  bool business_hour_slots_explicit = false;
  // Case field used to group flat EventStream/EventTable inputs. Existing
  // EventLog trace boundaries remain authoritative.
  std::string case_id_attribute = "case:concept:name";
  // PLUSPLUS-only raw selector. Omitted uses decoration; an explicit
  // "frequency" selects frequency, while any other string selects performance.
  // Appended to preserve existing positional aggregate initialization.
  std::optional<std::string> plusplus_decoration_selector = std::nullopt;
  // PLUSPLUS accepts signed, fractional and nonfinite numeric count cutoffs.
  // Supplied values override the integer filters for PLUSPLUS only; omission
  // retains exact uint64_t comparisons and the existing CLASSIC behavior.
  std::optional<double> plusplus_min_activity_count = std::nullopt;
  std::optional<double> plusplus_min_frequency = std::nullopt;
};

struct HeuristicClassicDfgInput {
  HeuristicFrequencyDfg dfg;
  std::optional<std::vector<std::string>> activities;
  std::optional<std::map<std::string, std::uint64_t>> activity_counts;
  std::optional<std::map<std::string, std::uint64_t>> start_counts;
  std::optional<std::map<std::string, std::uint64_t>> end_counts;
  HeuristicFrequencyDfg window_two_dfg;
  HeuristicTripleFrequencies frequency_triples;
  // Legacy native sample input, aggregated according to the miner options.
  std::optional<HeuristicPerformanceDfg> performance_dfg;
  std::optional<HeuristicBoundaryActivities> start_activities;
  std::optional<HeuristicBoundaryActivities> end_activities;
  // Already-aggregated values, preserved verbatim. Takes precedence over the
  // legacy sample map and selects performance decoration when supplied.
  std::optional<HeuristicDecoratedDfg> performance_values;
  // Appended to preserve existing positional aggregate initialization.
  // Nonempty maps are considered supplied even when these flags are false.
  bool dfg_window_2_supplied = false;
  bool freq_triples_supplied = false;
  // Optional order of the direct DFG's paths. When present, this must be an
  // exact permutation of `dfg`'s keys and controls first-encounter order for
  // inferred start/end activity lists. An omitted order keeps map-key order.
  std::optional<std::vector<std::pair<std::string, std::string>>> dfg_order;
};

// Discovers a heuristics net from an event log. The dependency of a over b is
// (|a>b| - |b>a|) / (|a>b| + |b>a| + 1); self-loops use |a>a| / (|a>a| + 1).
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const EventLog& event_log,
    const HeuristicMinerOptions& options = {});
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const EventDataView& data,
    const HeuristicMinerOptions& options = {});
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const EventStream& data,
    const HeuristicMinerOptions& options = {});
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const EventTable& data,
    const HeuristicMinerOptions& options = {});
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const LazyEventTable& data,
    const HeuristicMinerOptions& options = {});
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const HeuristicClassicDfgInput& input,
    const HeuristicMinerOptions& options = {});

// A native exception retaining the public Python error category for adapters.
class HeuristicPandasError : public std::invalid_argument {
 public:
  HeuristicPandasError(std::string reference_type, std::string message)
      : std::invalid_argument(std::move(message)),
        reference_type_(std::move(reference_type)) {}

  [[nodiscard]] const std::string& reference_type() const noexcept {
    return reference_type_;
  }

 private:
  std::string reference_type_;
};

// PLUSPLUS's public pandas entry point. It uses the pandas concurrency
// projection and materializes a missing start column plus @@diff durations.
// Declared timestamp dtypes are authoritative; object columns raise the public
// AttributeError category. The generic const-table entry point stays separate.
// All four column selectors are literal. Lookup failures preserve the public
// KeyError category, message and input-mutation order.
// Selecting @@diff as a timestamp preserves its duration overwrite and the
// resulting public error or missing-value behavior.
[[nodiscard]] HeuristicsNet discover_plus_plus_pandas(
    EventTable& data, const HeuristicMinerOptions& options = {});

}  // namespace minera
