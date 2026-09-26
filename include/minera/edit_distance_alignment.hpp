#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
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

// Result returned by the direct trace helper. Fresh results are stored in the
// shared std::any and can be retrieved with
// std::any_cast<EditDistanceTraceResult>.
struct EditDistanceTraceResult {
  std::vector<EditDistanceMove> alignment;
  std::uint64_t cost = 0;
};

// Typed direct-trace result. An empty optional is a gap; an engaged attribute
// whose is_null flag is set is the public None activity value.
struct EditDistanceTypedMove {
  EditDistanceMoveType type = EditDistanceMoveType::kSynchronous;
  std::optional<EventAttribute> log_activity;
  std::optional<EventAttribute> model_activity;
};

struct EditDistanceTypedTraceResult {
  std::vector<EditDistanceTypedMove> alignment;
  std::uint64_t cost = 0;
};

// A type-erased cache value. Cache hits return the exact shared handle held by
// the cache, including when it contains a caller-defined opaque value. A null
// handle is a valid cached value; callers distinguish it from a cache miss by
// checking whether the encoded-trace key is present.
using EditDistanceTraceHandle = std::shared_ptr<std::any>;

// Ordered input forms mirror the public helper's dict/list/set arguments.
// Mapping activity keys and cache encoded-trace keys must be unique. Mapping
// order is significant when several activities share an encoded value: the
// last mapping entry supplies the reverse-decoded activity.
using EditDistanceTraceMapping =
    std::vector<std::pair<std::string, std::string>>;
// Mapping entries are normalized with Python-dict semantics by the typed
// helper: equal keys retain the first key object and insertion position while
// the last value replaces its encoding.
using EditDistanceTypedTraceMapping =
    std::vector<std::pair<EventAttribute, std::string>>;
using EditDistanceTraceCache =
    std::vector<std::pair<std::string, EditDistanceTraceHandle>>;

// Align one trace against encoded target variants. Activity values are
// concatenated from mapping entries, with unmapped trace labels omitted.
// Encoded strings are split into validated UTF-8 codepoint tokens for distance
// and move generation. The supplied set is an independent exact-match
// shortcut; it is not derived from list_encodings. Cache lookup takes
// precedence over that shortcut and ignores later option/target changes for
// an existing encoded trace key. A null cache disables external caching.
// Ordinary equal-distance ties preserve first list occurrence regardless of
// target length. Anti-alignment chooses maximum distance, then a longer target
// on ties, then first list occurrence; it has no selectable target if all
// candidates are exact.
//
// The direct helper reads activity_attribute, perform_anti_alignment, and
// positive deviation_cost from options. The activity_attribute is the
// effective selector; a caller adapting PM4Py parameter dictionaries must
// preserve the distinct enum-key and string-key interpretation there. The
// helper does not require timestamps or case fields.
[[nodiscard]] EditDistanceTraceHandle apply_edit_distance_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {});

// Typed-key counterpart to the string-key direct helper. Activity lookup uses
// native scalar/tuple/frozen-set equality and hashability; mapping-key type is
// retained in emitted moves. It shares the same UTF-8 encodings, target
// selection, costs, cache key, and type-erased cache as the string helper.
[[nodiscard]] EditDistanceTraceHandle apply_typed_edit_distance_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTypedTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {});

[[nodiscard]] inline EditDistanceTraceHandle apply_typed_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTypedTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {}) {
  return apply_typed_edit_distance_trace(trace, list_encodings, set_encodings,
                                         mapping, cache, std::move(options));
}
[[nodiscard]] inline EditDistanceTraceHandle align_typed_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTypedTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {}) {
  return apply_typed_edit_distance_trace(trace, list_encodings, set_encodings,
                                         mapping, cache, std::move(options));
}

// Reference-named aliases for the direct helper.
[[nodiscard]] inline EditDistanceTraceHandle apply_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {}) {
  return apply_edit_distance_trace(trace, list_encodings, set_encodings,
                                  mapping, cache, std::move(options));
}
[[nodiscard]] inline EditDistanceTraceHandle align_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {}) {
  return apply_edit_distance_trace(trace, list_encodings, set_encodings,
                                  mapping, cache, std::move(options));
}
[[nodiscard]] inline EditDistanceTraceHandle align_edit_distance_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTraceMapping& mapping,
    EditDistanceTraceCache* cache = nullptr,
    EditDistanceOptions options = {}) {
  return apply_edit_distance_trace(trace, list_encodings, set_encodings,
                                  mapping, cache, std::move(options));
}

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
