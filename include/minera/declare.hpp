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

// pm4py's DataFrame Declare discovery and conformance list on an XES
// upload's frame (its rows in file order) in any input shape: a log with
// DataFrame columns as in discover_declare_as_uploaded and
// check_declare_conformance_as_uploaded; a table or lazy table with each
// case (by its case_id_key column) read as the rows from its first row on,
// as many as it has, in the order of the cases' first rows; an event stream
// as is (round BK, observed black-box).
[[nodiscard]] DeclareModel discover_declare_frame_slices(
    const EventDataView& data, const DeclareDiscoveryOptions& options = {});
[[nodiscard]] std::vector<DeclareConformance> check_declare_conformance_frame_slices(
    const EventDataView& data, const DeclareModel& model,
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
