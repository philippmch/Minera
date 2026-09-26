#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_data.hpp"
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
  // Event fields used for source/target variants and projected timestamps.
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  // Case field used when an EventDataView table or stream is converted to an
  // EventLog. It does not affect an EventLog whose traces are already grouped.
  std::string case_id_attribute = "case:concept:name";
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
// restored to source-trace order. Ordinary equal-distance ties choose the
// shortest target and then the last distinct target in first-occurrence order;
// anti-alignment ties choose the longest target and then the last distinct
// target in first-occurrence order.
[[nodiscard]] std::vector<EditDistanceAlignment> align_edit_distance(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options = {});

// Adapts flat event data using the configured case-id selector before
// alignment. Table rows are grouped in first-seen case order by the shared
// event-data conversion layer.
[[nodiscard]] std::vector<EditDistanceAlignment> align_edit_distance(
    const EventDataView& source, const EventDataView& target,
    EditDistanceOptions options = {});

// Convenience aliases.
[[nodiscard]] inline std::vector<EditDistanceAlignment> apply_edit_distance(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options = {}) {
  return align_edit_distance(source, target, options);
}
[[nodiscard]] inline std::vector<EditDistanceAlignment> apply_edit_distance(
    const EventDataView& source, const EventDataView& target,
    EditDistanceOptions options = {}) {
  return align_edit_distance(source, target, options);
}
[[nodiscard]] inline std::vector<EditDistanceAlignment> apply(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options = {}) {
  return align_edit_distance(source, target, options);
}
[[nodiscard]] inline std::vector<EditDistanceAlignment> apply(
    const EventDataView& source, const EventDataView& target,
    EditDistanceOptions options = {}) {
  return align_edit_distance(source, target, options);
}

// Keep target activities, retaining selected timestamps from synchronous
// source events and marking projected events with @@is_conforming. Source-only
// moves are omitted. The output is a fresh projection containing only the
// @@aligned_variant log marker, @@original_trace trace marker, and selected
// activity/timestamp plus conformance fields on events. Empty input traces are
// omitted, matching the public project_log_on_variant contract.
[[nodiscard]] EventLog project_log_on_variant(
    const EventLog& source, const std::vector<std::string>& variant,
    EditDistanceOptions options = {});

// Event-data adapter for public variant projection. Dataframe/event-stream
// case grouping uses EditDistanceOptions::case_id_attribute.
[[nodiscard]] EventLog project_log_on_variant(
    const EventDataView& source, const std::vector<std::string>& variant,
    EditDistanceOptions options = {});

// Projects each source trace using its own selected alignment.  The
// alignments must be in source-trace order (as returned by
// align_edit_distance), allowing a heterogeneous source log to select a
// different target variant for each trace. This native extension preserves
// source log/trace/event context; project_log_on_variant implements the
// public fresh-output projection contract.
[[nodiscard]] EventLog project_log_on_alignments(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments);

// Selector-aware overload.  The two-argument overload above remains the
// source-compatible conventional-key wrapper.
[[nodiscard]] EventLog project_log_on_alignments(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments,
    EditDistanceOptions options);

}  // namespace minera
