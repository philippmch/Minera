#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/event_data.hpp"

namespace minera {

enum class ConceptDriftVariant {
  kBose,
  BOSE = kBose,
};

// Configuration for the eventual-follow relation feature and window test.
// Seeds through UINT32_MAX reproduce NumPy's legacy RandomState stream. A
// wider seed uses a deterministic native extension seeded from its low and
// high 32-bit words.
struct ConceptDriftOptions {
  ConceptDriftVariant variant = ConceptDriftVariant::kBose;
  std::size_t sub_log_size = 50;
  std::size_t window_size = 8;
  std::size_t num_permutations = 100;
  double threshold = 0.5;
  std::size_t max_no_change_points = 5;
  std::uint64_t seed = 0;
  bool include_timestamps = false;
  // Event/trace selectors use the standard XES names by default and resolve
  // non-standard keys from the generic attribute maps.
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::string case_id_attribute = "case:concept:name";
  // False leaves sublogs, returned_sublogs and cumulative_sublogs empty; the
  // matching *_ranges fields (always filled) then describe every partition by
  // trace indices into the input log, without copying any trace.
  bool materialize_sublogs = true;
};

// Traces [begin, end) of the input log, and the number of events they hold.
struct ConceptDriftTraceRange {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::size_t event_count = 0;
  [[nodiscard]] std::size_t trace_count() const noexcept { return end - begin; }
};

struct ConceptDriftChangePoint {
  std::size_t sublog_index = 0;
  double p_value = 1.0;
};

struct ConceptDriftResult {
  ConceptDriftVariant variant = ConceptDriftVariant::kBose;
  // Activities and one relation-type-count vector per raw sub-log. Activities
  // are sorted lexicographically and vectors are laid out [always, sometimes,
  // never] for each target activity in that order.
  std::vector<std::string> activities;
  std::vector<std::vector<double>> feature_vectors;
  std::vector<EventLog> sublogs;

  // Chronological partitions ending at selected drift boundaries, followed by
  // the final remainder. Each input trace occurs in exactly one returned log.
  std::vector<EventLog> returned_sublogs;

  // Cumulative logs ending at each raw sub-log. Trace and event metadata are
  // copied verbatim from the input log.
  std::vector<EventLog> cumulative_sublogs;
  // Index form of the three partitions above, one range per sub-log.
  std::vector<ConceptDriftTraceRange> sublog_ranges;
  std::vector<ConceptDriftTraceRange> returned_sublog_ranges;
  std::vector<ConceptDriftTraceRange> cumulative_sublog_ranges;
  std::vector<double> sublog_start_timestamps;
  std::vector<ConceptDriftChangePoint> change_points;
  std::vector<double> change_timestamps;
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::string case_id_attribute = "case:concept:name";
};

// Signed direct-trace BOSE options. This mirrors the Python-facing detector's
// parameters, including its signed slicing behavior. The native implementation
// applies explicit resource bounds before materializing windows or shuffles.
struct ConceptDriftTraceOptions {
  std::int64_t sub_log_size = 50;
  std::int64_t window_size = 8;
  std::int64_t num_permutations = 100;
  double threshold = 0.5;
  std::int64_t max_no_change_points = 5;
  std::uint64_t seed = 0;
};

struct ConceptDriftTraceChangePoint {
  std::int64_t sublog_index = 0;
  double p_value = 1.0;
};

struct ConceptDriftTraceResult {
  std::vector<std::vector<std::vector<std::string>>> sublogs;
  std::vector<ConceptDriftTraceChangePoint> change_points;
};

// BOSE detection over typed raw activity values. Returned sublogs copy the
// original EventAttribute values, including container structure and native
// scalar identity, while drift computation uses ordered surrogate labels.
struct ConceptDriftTypedTraceResult {
  std::vector<std::vector<std::vector<EventAttribute>>> sublogs;
  std::vector<ConceptDriftTraceChangePoint> change_points;
};

struct ConceptDriftTableOptions : ConceptDriftTraceOptions {
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::string case_id_attribute = "case:concept:name";
};

struct ConceptDriftTableResult {
  std::vector<EventTable> returned_tables;
  std::vector<double> change_timestamps;
  std::vector<double> p_values;
};

// Apply the BOSE DataFrame adapter, including its positional case projection
// and regenerated output frames. Selected case/timestamp columns control the
// separate change-time lookup; projection also requires the standard columns.
[[nodiscard]] ConceptDriftTableResult apply_concept_drift_table(
    const EventTable& table, ConceptDriftTableOptions options = {});

[[nodiscard]] ConceptDriftTableResult apply_concept_drift_table(
    const EventTable& table, ConceptDriftTableOptions options, std::mt19937& rng);

// Dispatch signed BOSE apply for EventLog or EventTable inputs. Unknown
// variants reject before adapter validation or RNG consumption.
[[nodiscard]] ConceptDriftTableResult apply_concept_drift(
    const EventLog& log, ConceptDriftTableOptions options = {},
    ConceptDriftVariant variant = ConceptDriftVariant::kBose);

[[nodiscard]] ConceptDriftTableResult apply_concept_drift(
    const EventLog& log, ConceptDriftTableOptions options, std::mt19937& rng,
    ConceptDriftVariant variant = ConceptDriftVariant::kBose);

[[nodiscard]] ConceptDriftTableResult apply_concept_drift(
    const EventTable& table, ConceptDriftTableOptions options = {},
    ConceptDriftVariant variant = ConceptDriftVariant::kBose);

[[nodiscard]] ConceptDriftTableResult apply_concept_drift(
    const EventTable& table, ConceptDriftTableOptions options,
    std::mt19937& rng,
    ConceptDriftVariant variant = ConceptDriftVariant::kBose);

[[nodiscard]] std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log);

[[nodiscard]] std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log, std::string_view activity_attribute);

// Return unique typed raw activities in native ascending order. The first
// original representative of each equal label is retained, including its
// nested values and scalar identity. Incompatible label types reject because
// the result is sorted. For dates, canonical `YYYY-MM-DD` values remain
// distinct from datetime values that contain a time, including midnight.
// Distinct NaNs use native scalar-identity order, also inside tuples. This can
// differ from allocation-dependent ordering in the reference runtime.
// Frozen sets use unordered, deduplicated member identity. Their native order
// compares unique member counts, then canonical member identities; strict
// subsets precede supersets, while incomparable sets can differ from reference
// hash iteration order. Equal frozen-set tuple prefixes advance to later fields.
[[nodiscard]] std::vector<EventAttribute> extract_concept_drift_typed_activities(
    const std::vector<std::vector<EventAttribute>>& traces);

// Extract the Bose relation-type-count feature vector for a set of traces.
// For every target b and source a, traces containing a are classified by
// whether b eventually follows a in all, some, or no such traces.
// The supplied alphabet is preserved exactly, including its order and
// duplicate entries. A selected event label absent from the alphabet is
// invalid.
[[nodiscard]] std::vector<double> extract_concept_drift_features(
    const std::vector<Trace>& traces,
    const std::vector<std::string>& activities);

[[nodiscard]] std::vector<double> extract_concept_drift_features(
    const std::vector<Trace>& traces,
    const std::vector<std::string>& alphabet,
    std::string_view activity_attribute);

[[nodiscard]] std::vector<double> extract_concept_drift_features(
    const EventLog& log);

// Extract relation-count features for typed raw traces. The supplied alphabet
// order and duplicate entries are preserved; activities outside it reject.
// Unlike the sorted typed-activity helper, this explicit-alphabet operation
// accepts mixed label families. Canonical calendar dates and datetimes remain
// distinct even when both denote midnight on the same day.
[[nodiscard]] std::vector<double> extract_concept_drift_typed_features(
    const std::vector<std::vector<EventAttribute>>& traces,
    const std::vector<EventAttribute>& alphabet);

// Return ordered eventual-follow pairs from one trace. The alphabet filters
// target labels only; source labels may be any selected event label. Missing
// generic selector values are skipped, and repeated labels produce one set
// entry per pair.
[[nodiscard]] std::set<std::pair<std::string, std::string>>
compute_concept_drift_follows_relation(
    const Trace& trace, const std::vector<std::string>& alphabet,
    std::string_view activity_attribute = "concept:name");

// Return eventual-follow pairs for typed activities. The alphabet filters
// targets only; source labels may be any trace value. Pairs are emitted in
// first-generated order, retaining the first typed representative of each
// semantic pair. Canonical calendar dates and datetimes remain distinct even
// when both denote midnight on the same day.
// Trace activities must be hashable. Alphabet entries may also be mutable
// containers and are compared in order when equality requires it.
[[nodiscard]] std::vector<std::pair<EventAttribute, EventAttribute>>
compute_concept_drift_typed_follows_relation(
    const std::vector<EventAttribute>& trace,
    const std::vector<EventAttribute>& alphabet);

// Split a log into consecutive trace groups. Negative sizes return no groups,
// zero is invalid, and a final partial group is retained only when requested.
[[nodiscard]] std::vector<EventLog> split_concept_drift_sublogs(
    const EventLog& log, std::int64_t sub_log_size = 50,
    bool keep_leftover = true);

// Compare two feature windows by a seeded permutation test. Matrix rows must
// be rectangular with matching widths; negative counts return signed zero,
// zero is invalid, and positive work is bounded for safe materialization.
[[nodiscard]] double concept_drift_permutation_test(
    const std::vector<std::vector<double>>& first,
    const std::vector<std::vector<double>>& second,
    std::int64_t num_permutations = 100, std::uint64_t seed = 0);

// Continue a caller-owned MT19937 stream. The state overload consumes exactly
// the same shuffle draws as the seeded overload and leaves the engine advanced.
[[nodiscard]] double concept_drift_permutation_test(
    const std::vector<std::vector<double>>& first,
    const std::vector<std::vector<double>>& second,
    std::int64_t num_permutations, std::mt19937& rng);

// Detect BOSE drift directly from activity traces, without EventLog timestamp
// validation. Raw consecutive trace groups and p-value-ranked change points
// are returned. Signed options preserve Python list/range edge behavior.
[[nodiscard]] ConceptDriftTraceResult detect_concept_drift_traces(
    const std::vector<std::vector<std::string>>& traces,
    ConceptDriftTraceOptions options = {});

// Continue a caller-owned NumPy-compatible MT19937 stream. The seeded
// overload constructs the same engine and delegates to this overload.
[[nodiscard]] ConceptDriftTraceResult detect_concept_drift_traces(
    const std::vector<std::vector<std::string>>& traces,
    ConceptDriftTraceOptions options, std::mt19937& rng);

// Detect BOSE drift directly from typed activity traces. Immutable tuples,
// frozen sets, scalar aliases, nulls, and NaN identity share ordering with the
// DataFrame adapter; mutable containers and incompatible mixed types reject
// before any random draws. Original typed values are retained in returned
// sublogs.
[[nodiscard]] ConceptDriftTypedTraceResult detect_concept_drift_typed_traces(
    const std::vector<std::vector<EventAttribute>>& traces,
    ConceptDriftTraceOptions options = {});

[[nodiscard]] ConceptDriftTypedTraceResult detect_concept_drift_typed_traces(
    const std::vector<std::vector<EventAttribute>>& traces,
    ConceptDriftTraceOptions options, std::mt19937& rng);

// Apply signed BOSE options to an EventLog and return regenerated event tables.
// The selected activity and timestamp fields are honored; case_id_attribute is
// retained for API symmetry but does not regroup an already grouped EventLog.
[[nodiscard]] ConceptDriftTableResult apply_concept_drift_log(
    const EventLog& log, ConceptDriftTableOptions options = {});

[[nodiscard]] ConceptDriftTableResult apply_concept_drift_log(
    const EventLog& log, ConceptDriftTableOptions options, std::mt19937& rng);

[[nodiscard]] ConceptDriftResult detect_concept_drift(
    const EventLog& log, ConceptDriftOptions options = {});

}  // namespace minera
