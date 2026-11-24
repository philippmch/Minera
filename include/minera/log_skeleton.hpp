#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/event_data.hpp"

namespace minera {

// Classic log-skeleton relation pair. Relations are stored in
// deterministic lexicographic order by the discovery implementation.
struct LogSkeletonPair {
  std::string source;
  std::string target;
  friend bool operator==(const LogSkeletonPair&, const LogSkeletonPair&) = default;
  friend bool operator<(const LogSkeletonPair& left, const LogSkeletonPair& right) {
    return left.source < right.source ||
           (left.source == right.source && left.target < right.target);
  }
};

struct LogSkeletonOptions {
  double noise_threshold = 0.0;
  // Empty means all six classic families.  Names are exactly:
  // equivalence, always_after, always_before, never_together,
  // directly_follows, activ_freq.
  std::vector<std::string> considered_constraints;
  // Event attribute used as the activity label.  The canonical activity
  // field is selected by default; any other key must be present and non-null
  // on every event.
  std::string activity_key = "concept:name";
  // Distinguish an explicitly empty conformance family selection from the
  // legacy omitted form (an empty vector means all families).
  bool considered_constraints_present = false;
  // Case attribute used to group flat event records.  It is ignored for an
  // already grouped EventLog, but is applied by stream/table adapters.
  std::string case_id_key = "case:concept:name";
};

struct LogSkeletonModel {
  std::vector<LogSkeletonPair> equivalence;
  std::vector<LogSkeletonPair> always_after;
  std::vector<LogSkeletonPair> always_before;
  std::vector<LogSkeletonPair> never_together;
  std::vector<LogSkeletonPair> directly_follows;
  // Allowed per-trace occurrence counts for each activity, sorted ascending.
  std::map<std::string, std::vector<std::size_t>> activ_freq;
};

[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventLog& log, const LogSkeletonOptions& options = {});
// The log's traces met in the given order (positions into log.traces, each
// once), e.g. case_id_order(log) for pm4py's DataFrame path: the order only
// decides the activity-frequency ties.
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventLog& log, const LogSkeletonOptions& options,
    const std::vector<std::size_t>& trace_order);
// Retain the pre-adapter brace-call form (for example, an empty log as `{}`).
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    std::initializer_list<Trace> traces,
    const LogSkeletonOptions& options = {});
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventStream& stream, const LogSkeletonOptions& options = {});
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventTable& table, const LogSkeletonOptions& options = {});
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const LazyEventTable& table, const LogSkeletonOptions& options = {});
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventDataView& data, const LogSkeletonOptions& options = {});
// The cases of `data` met in the given order (positions into the cases the
// functions above meet: a log's traces, flat data's groups by
// options.case_id_key), e.g. log_skeleton_case_order(data, options).
[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventDataView& data, const LogSkeletonOptions& options,
    const std::vector<std::size_t>& trace_order);
// The positions of the cases the log-skeleton functions meet on `data` (a
// log's traces; flat data's groups by options.case_id_key, in first-appearance
// order) in the stable code-point order of their case ids: the order of
// pm4py's DataFrame log skeleton and Declare (case_id_order for a log). The
// Declare functions group flat data the same way.
// pm4py's log skeleton discovery on the frame of an XES upload, pandas (an
// event table) or Polars (a lazy table): the groupby over the case column,
// the activity counts over every row, gaps and missing columns as pm4py
// reads them (round FY). Other inputs as discover_log_skeleton. With
// `null_labels`, a row without an activity on Polars is the label None in
// the model (kLogSkeletonNullActivity; round FZ, observed black-box), as
// pm4py answers; without it such a row is refused.
[[nodiscard]] LogSkeletonModel discover_log_skeleton_xes_frame(
    const EventDataView& data, const LogSkeletonOptions& options, bool null_labels = false);
// Round GZ: an activity column of other values than text keys the model of
// discover_log_skeleton_xes_frame by the Python objects and labels it with
// str() of each (a boolean True, a date its Timestamp's str(); 0 and False
// one activity, the first met naming it; pandas' KeyError names a missing
// None, nan or NaT). This gives that model with each label's first row
// text instead, as the session's log names the activities.
[[nodiscard]] LogSkeletonModel log_skeleton_xes_frame_texts(const EventDataView& data,
                                                            const LogSkeletonOptions& options,
                                                            LogSkeletonModel model);

// The model's name for a missing activity (Python's None): no XES or CSV
// label holds a NUL character. The JSON shows it as null.
inline const std::string kLogSkeletonNullActivity{"\0None", 5};

[[nodiscard]] std::vector<std::size_t> log_skeleton_case_order(
    const EventDataView& data, const LogSkeletonOptions& options);

// A variant list is an ordered sequence of activity variants paired with the
// source count metadata. The classic entry point materializes one trace per
// entry; the count is intentionally not consulted. Duplicate entries are
// therefore retained until the normal variant aggregation, and empty entries
// remain representable.
using LogSkeletonVariant =
    std::pair<std::vector<std::string>, std::size_t>;
using LogSkeletonVariantList = std::vector<LogSkeletonVariant>;

[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const LogSkeletonVariantList& variants,
    const LogSkeletonOptions& options = {});

struct LogSkeletonDeviation {
  // A relation violation fills `relation`; an activ_freq violation fills
  // `activity`, `observed_frequency`, and (when known) allowed_frequencies.
  // This compact typed representation stands in for the heterogeneous
  // tuple/set diagnostics while retaining deterministic ordering.
  std::string constraint;
  LogSkeletonPair relation;
  std::string activity;
  std::size_t observed_frequency = 0;
  std::vector<std::size_t> allowed_frequencies;
  friend bool operator==(const LogSkeletonDeviation&, const LogSkeletonDeviation&) = default;
};

struct LogSkeletonConformance {
  std::vector<LogSkeletonDeviation> deviations;
  std::size_t no_dev_total = 0;
  std::size_t no_constr_total = 0;
  double dev_fitness = 1.0;
  bool is_fit = true;
};

using LogSkeletonVariantConformance =
    std::map<std::vector<std::string>, LogSkeletonConformance>;

[[nodiscard]] LogSkeletonConformance check_log_skeleton_conformance(
    const std::vector<std::string>& activities, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});

[[nodiscard]] LogSkeletonConformance check_log_skeleton_conformance(
    const Trace& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});

[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventLog& log, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});
[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventStream& stream, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});
[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventTable& table, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});
[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const LazyEventTable& table, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});
[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventDataView& data, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});
// pm4py's log skeleton conformance on the frame of an XES upload (round GA,
// observed black-box): one result per case of the groupby discovery reads
// (pandas: sorted case ids, rows without one left out; Polars: first-row
// order, the rows without one a case of their own, a name shared by traces
// one case), a Polars row without an activity the label None. When the
// activity-frequency deviations of a case name None beside a named
// activity, sorting them raises TypeError as CPython's list.sort meets the
// pair. Other inputs as check_log_skeleton_conformance.
[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance_xes_frame(
    const EventDataView& data, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});

// The variant-list conformance result is keyed by activity sequence, matching
// the dictionary-shaped contract: duplicate keys overwrite earlier entries
// and the count field is not consulted.
[[nodiscard]] LogSkeletonVariantConformance
check_log_skeleton_conformance(const LogSkeletonVariantList& variants,
                               const LogSkeletonModel& model,
                               const LogSkeletonOptions& options = {});

struct LogSkeletonDiagnosticsOptions {
  // Case-id attribute used for the diagnostics table. Canonical and custom
  // first-class selectors are both accepted.
  std::string case_id_key = "case:concept:name";
};

[[nodiscard]] EventTable log_skeleton_conformance_diagnostics(
    const EventLog& log,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options = {});
[[nodiscard]] EventTable log_skeleton_conformance_diagnostics(
    const EventStream& stream,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options = {});
[[nodiscard]] EventTable log_skeleton_conformance_diagnostics(
    const EventTable& table,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options = {});
[[nodiscard]] EventTable log_skeleton_conformance_diagnostics(
    const LazyEventTable& table,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options = {});
[[nodiscard]] EventTable log_skeleton_conformance_diagnostics(
    const EventDataView& data,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options = {});

}  // namespace minera
