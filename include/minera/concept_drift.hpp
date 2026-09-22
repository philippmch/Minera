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
  std::vector<double> sublog_start_timestamps;
  std::vector<ConceptDriftChangePoint> change_points;
  std::vector<double> change_timestamps;
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::string case_id_attribute = "case:concept:name";
};

[[nodiscard]] std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log);

[[nodiscard]] std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log, std::string_view activity_attribute);

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

// Return ordered eventual-follow pairs from one trace. The alphabet filters
// target labels only; source labels may be any selected event label. Missing
// generic selector values are skipped, and repeated labels produce one set
// entry per pair.
[[nodiscard]] std::set<std::pair<std::string, std::string>>
compute_concept_drift_follows_relation(
    const Trace& trace, const std::vector<std::string>& alphabet,
    std::string_view activity_attribute = "concept:name");

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

[[nodiscard]] ConceptDriftResult detect_concept_drift(
    const EventLog& log, ConceptDriftOptions options = {});

}  // namespace minera
