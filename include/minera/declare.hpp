#pragma once

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

#include <cstddef>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace minera {

inline constexpr const char* kDeclareExistence = "existence";
inline constexpr const char* kDeclareExactlyOne = "exactly_one";
inline constexpr const char* kDeclareInit = "init";
inline constexpr const char* kDeclareRespondedExistence = "responded_existence";
inline constexpr const char* kDeclareResponse = "response";
inline constexpr const char* kDeclarePrecedence = "precedence";
inline constexpr const char* kDeclareSuccession = "succession";
inline constexpr const char* kDeclareAltResponse = "altresponse";
inline constexpr const char* kDeclareAltPrecedence = "altprecedence";
inline constexpr const char* kDeclareAltSuccession = "altsuccession";
inline constexpr const char* kDeclareChainResponse = "chainresponse";
inline constexpr const char* kDeclareChainPrecedence = "chainprecedence";
inline constexpr const char* kDeclareChainSuccession = "chainsuccession";
inline constexpr const char* kDeclareAbsence = "absence";
inline constexpr const char* kDeclareCoexistence = "coexistence";
inline constexpr const char* kDeclareNonCoexistence = "noncoexistence";
inline constexpr const char* kDeclareNonSuccession = "nonsuccession";
inline constexpr const char* kDeclareNonChainSuccession = "nonchainsuccession";

struct DeclareConstraint {
  std::string first;
  std::string second;
  bool binary = false;
  int support = 0;
  int confidence = 0;
  friend bool operator==(const DeclareConstraint&, const DeclareConstraint&) =
      default;
};

struct DeclareModel {
  // All 18 families are present (possibly empty), in lexical-independent
  // Stable order. Rules within a family are ordered by activity keys.
  std::map<std::string, std::vector<DeclareConstraint>> rules;

  DeclareModel();
  void add(std::string family, DeclareConstraint constraint);
  [[nodiscard]] std::size_t size() const noexcept;
};

struct DeclareDiscoveryOptions {
  // nullopt means the default (infer activities / enable all templates);
  // an engaged empty vector is an explicit empty filter.
  std::optional<std::vector<std::string>> considered_activities;
  std::optional<std::vector<std::string>> allowed_templates;
  double min_support_ratio = 0.0;
  double min_confidence_ratio = 0.0;
  bool has_min_support_ratio = false;
  bool has_min_confidence_ratio = false;
  double auto_selection_multiplier = 0.8;
  // Event attribute used as the activity label.  The canonical activity
  // field is selected by default; any other key must be present and non-null
  // on every event.
  std::string activity_key = "concept:name";
  // Case attribute used to group flat event records.  It is ignored for an
  // already grouped EventLog, but is applied by stream/table adapters.
  std::string case_id_key = "case:concept:name";
};

[[nodiscard]] DeclareModel discover_declare(
    const EventLog& log, const DeclareDiscoveryOptions& options = {});
// The discovery of a CSV or XES upload (a log with DataFrame columns), as
// pm4py's DataFrame discovery reads it: each case is the frame's rows from
// its first row on, as many as the case has (frame_slice_cases; an XES
// upload whose trace names repeat apart gives a case other cases' rows;
// rounds BH and BI, observed black-box). Any other log is discovered as is.
[[nodiscard]] DeclareModel discover_declare_as_uploaded(
    const EventLog& log, const DeclareDiscoveryOptions& options = {});
// Retain the pre-adapter brace-call form (for example, an empty log as `{}`).
[[nodiscard]] DeclareModel discover_declare(
    std::initializer_list<Trace> traces,
    const DeclareDiscoveryOptions& options = {});
[[nodiscard]] DeclareModel discover_declare(
    const EventStream& stream, const DeclareDiscoveryOptions& options = {});
[[nodiscard]] DeclareModel discover_declare(
    const EventTable& table, const DeclareDiscoveryOptions& options = {});
[[nodiscard]] DeclareModel discover_declare(
    const LazyEventTable& table, const DeclareDiscoveryOptions& options = {});
[[nodiscard]] DeclareModel discover_declare(
    const EventDataView& data, const DeclareDiscoveryOptions& options = {});

struct DeclareDeviation {
  std::string template_name;
  std::string first;
  std::string second;
  bool binary = false;
};

struct DeclareConformance {
  std::vector<DeclareDeviation> deviations;
  int no_constr_total = 0;
  int no_dev_total = 0;
  double dev_fitness = 1.0;
  bool is_fit = true;
};

struct DeclareConformanceOptions {
  // Event attribute used as the activity label.  The canonical activity
  // field is selected by default; any other key must be present and non-null
  // on every event.
  std::string activity_key = "concept:name";
  // Case attribute used to group flat event records.  It is ignored for an
  // already grouped EventLog, but is applied by stream/table adapters.
  std::string case_id_key = "case:concept:name";
};

struct DeclareDiagnosticsOptions {
  // Case-id attribute used for the diagnostics table. Canonical and custom
  // first-class selectors are both accepted.
  std::string case_id_key = "case:concept:name";
};

[[nodiscard]] DeclareConformance check_declare_conformance(
    const std::vector<std::string>& trace, const DeclareModel& model);
[[nodiscard]] DeclareConformance check_declare_conformance(
    const Trace& trace, const DeclareModel& model);
[[nodiscard]] DeclareConformance check_declare_conformance(
    const Trace& trace, const DeclareModel& model,
    const DeclareConformanceOptions& options);
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const EventLog& log, const DeclareModel& model);
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const EventLog& log, const DeclareModel& model,
    const DeclareConformanceOptions& options);
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const EventStream& stream, const DeclareModel& model,
    const DeclareConformanceOptions& options = {});
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const EventTable& table, const DeclareModel& model,
    const DeclareConformanceOptions& options = {});
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const LazyEventTable& table, const DeclareModel& model,
    const DeclareConformanceOptions& options = {});
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance(
    const EventDataView& data, const DeclareModel& model,
    const DeclareConformanceOptions& options = {});

// The conformance of a CSV or XES upload (a log with DataFrame columns) as
// pm4py's DataFrame conformance_declare lists it: each case read as its
// slice of the frame's rows (frame_slice_cases), one result per trace in the
// log's order. Any other log is checked as is. pm4py's diagnostics table
// reads the merged cases instead (check_declare_conformance; rounds BI and
// BJ, observed black-box).
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance_as_uploaded(
    const EventLog& log, const DeclareModel& model,
    const DeclareConformanceOptions& options = {});

// pm4py's Declare frame checks on the Polars frame of an XES upload, where a
// column without any value is absent (round FW): run before
// check_declare_conformance on such a lazy table, whose generic path keeps
// every column as a CSV frame does.
void check_declare_xes_polars_frame(const LazyEventTable& table,
                                    const DeclareConformanceOptions& options = {});
// Conformance on an XES upload's Polars frame, its frame check on the
// columns as read_xes types them.
std::vector<DeclareConformance> check_declare_conformance_xes_polars(
    const LazyEventTable& table, const DeclareModel& model, const DeclareConformanceOptions& options);

// pm4py's DataFrame Declare discovery and conformance list on an XES
// upload's frame (its rows in file order) in any input shape: a log with
// DataFrame columns as in discover_declare_as_uploaded and
// check_declare_conformance_as_uploaded; a table or lazy table with each
// case (by its case_id_key column) read as the rows from its first row on,
// as many as it has, in the order of the cases' first rows; an event stream
// as is (round BK, observed black-box).
//
// `shown` (round HA): on a pandas frame keyed by a typed column (or a text
// column with gaps) it receives the model pm4py's pandas Declare answers,
// labelled by str() of the column's objects as pandas' rules frame hands
// them out (its errors are raised); the returned model is then mined on the
// XES texts the session replays (an empty model where that fails). On a
// Polars frame keyed by an object column it receives the model on str() of
// each object, as the Polars copy holds them.
//
// `shown_conformance` (round HB) then receives pm4py's conformance against
// that typed model (discover_declare_typed_frame).
struct DeclareShownConformance {
  std::vector<std::string> case_ids;         // each result's case, in the projection's order
  std::vector<DeclareConformance> results;
  std::string error;                         // the error the conformance raises (no results)
};
[[nodiscard]] DeclareModel discover_declare_frame_slices(
    const EventDataView& data, const DeclareDiscoveryOptions& options = {},
    std::optional<DeclareModel>* shown = nullptr,
    std::optional<DeclareShownConformance>* shown_conformance = nullptr);

// pm4py's pandas Declare on a frame keyed by a typed column, or a text
// column with gaps (round HA, typed_declare_model), and its conformance
// against that model (round HB, observed black-box): each case the frame's
// rows from its first row on, the column's objects as the activities. The
// conformance checks the families in pm4py's order on the objects: `in`
// finds an identical or equal object (the column's one NaN or NaT finds
// itself, a float64 level's NaN nothing), `!=` holds for NaN and NaT, and a
// key whose second activity collapsed is subscripted or unpacked (TypeError
// "'float' object is not subscriptable", "cannot unpack non-iterable bool
// object", a str's own characters or IndexError/ValueError), an error the
// conformance then reports. Frame checks and discovery errors are raised;
// nullopt (before any check: the caller's own path runs them) when the
// column is text without gaps or outside the model. A CSV
// upload's frame (its typed cells, read_csv's NaN cells) and its event log
// (convert_to_event_log of that frame) both read this way.
struct TypedDeclareResult {
  DeclareModel model;
  DeclareShownConformance conformance;
};
[[nodiscard]] std::optional<TypedDeclareResult> discover_declare_typed_frame(
    const EventTable& frame, const DeclareDiscoveryOptions& options = {});
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance_frame_slices(
    const EventDataView& data, const DeclareModel& model,
    const DeclareConformanceOptions& options = {});
// The pandas frame of an XES upload as pm4py's Declare conformance reads it
// (round FW, observed black-box): the algorithm's frame checks (the column
// counts, KeyError for the case, activity and "time:timestamp" columns, the
// TypeError of np.unique on a missing case id among named ones) and each row
// without an activity read as an event no rule names. A column with no value
// is no column of the frame (read_xes has no column for an attribute no
// event carries).
[[nodiscard]] EventTable declare_pandas_conformance_table(const EventTable& table,
                                                          const DeclareConformanceOptions& options = {});

[[nodiscard]] EventTable declare_conformance_diagnostics(
    const EventLog& log, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options = {});
[[nodiscard]] EventTable declare_conformance_diagnostics(
    const EventStream& stream, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options = {});
[[nodiscard]] EventTable declare_conformance_diagnostics(
    const EventTable& table, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options = {});
[[nodiscard]] EventTable declare_conformance_diagnostics(
    const LazyEventTable& table, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options = {});
[[nodiscard]] EventTable declare_conformance_diagnostics(
    const EventDataView& data, const std::vector<DeclareConformance>& results,
    const DeclareDiagnosticsOptions& options = {});

}  // namespace minera
