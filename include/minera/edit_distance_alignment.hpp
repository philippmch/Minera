#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// The algorithm is exposed through a variant selector. Native code
// has one implementation, but retaining the selector keeps call sites
// source-compatible with a multi-variant registry.
enum class EditDistanceVariant {
  kEditDistance,
  EDIT_DISTANCE = kEditDistance,
};

// The three move kinds emitted by the edit-distance alignment. A substitution
// is represented by a log move followed by a model move (cost two deviations).
enum class EditDistanceMoveType {
  kSynchronous,
  kLog,
  kModel,
  SYNCHRONOUS = kSynchronous,
  LOG = kLog,
  MODEL = kModel,
};

struct EditDistanceMove {
  EditDistanceMoveType type = EditDistanceMoveType::kSynchronous;
  std::string log_activity;
  std::string model_activity;
  // Convenience name for clients that only need the activity of a move.
  std::string activity;
  // DFG-shaped spellings (`>>` is represented by an empty string).
  std::string log;
  std::string model;
};

struct EditDistanceOptions {
  EditDistanceVariant variant = EditDistanceVariant::kEditDistance;
  bool perform_anti_alignment = false;
  // Standard move cost. It is deliberately configurable for callers
  // that need a different scale, but must remain finite and positive.
  std::uint64_t deviation_cost = 10000;
};

struct EditDistanceAlignment {
  std::vector<EditDistanceMove> alignment;
  std::vector<EditDistanceMove> moves;  // synonym retained for API clarity
  std::vector<std::string> source_variant;
  std::vector<std::string> target_variant;
  std::size_t target_index = 0;
  std::uint64_t cost = 0;
  double fitness = 1.0;
  std::uint64_t best_worst_cost = 0;
  std::uint64_t bwc = 0;
  std::uint64_t denominator = 0;
};

// Align every trace in source against the closest (or, for anti-alignment,
// farthest) variant in target. Duplicate variants are grouped and results are
// restored to source-trace order; equal-distance ties choose the
// lexicographically smallest activity sequence and then its first occurrence.
[[nodiscard]] std::vector<EditDistanceAlignment> align_edit_distance(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options = {});

// Convenience aliases.
[[nodiscard]] inline std::vector<EditDistanceAlignment> apply_edit_distance(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options = {}) {
  return align_edit_distance(source, target, options);
}
[[nodiscard]] inline std::vector<EditDistanceAlignment> apply(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options = {}) {
  return align_edit_distance(source, target, options);
}

// Keep target activities, retaining timestamps from synchronous source events
// and marking projected events with @@is_conforming. Source-only moves are
// omitted. Trace metadata records @@original_trace; the resulting log records
// @@aligned_variant. Empty source traces are retained.
[[nodiscard]] EventLog project_log_on_variant(
    const EventLog& source, const std::vector<std::string>& variant,
    EditDistanceOptions options = {});

// Projects each source trace using its own selected alignment.  The
// alignments must be in source-trace order (as returned by
// align_edit_distance), allowing a heterogeneous source log to select a
// different target variant for each trace.
[[nodiscard]] EventLog project_log_on_alignments(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments);

}  // namespace minera
