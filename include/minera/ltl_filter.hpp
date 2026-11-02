#pragma once

#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// The bounded trace predicates supported by filter_ltl_event_log.
enum class LtlFilterVariant {
  kEventuallyFollows,
  kANextBNextC,
  kFourEyes,
  kAttributeValueDifferentPersons,

  // Verbose spellings retained for callers that prefer enum-style names.
  eventually_follows = kEventuallyFollows,
  a_next_b_next_c = kANextBNextC,
  four_eyes_principle = kFourEyes,
  attr_value_different_persons = kAttributeValueDifferentPersons,
  kFourEyesPrinciple = kFourEyes,
  kAttrValueDifferentPersons = kAttributeValueDifferentPersons,
};
using LtlVariant = LtlFilterVariant;

// Arguments for one predicate. `attribute` names the event value to inspect:
// An empty attribute_key selects Event::activity; a non-empty key selects a
// generic event attribute. An empty resource_key selects Event::resource; a
// non-empty key selects a generic attribute for the performer. `attribute_values`
// is an ordered pattern (at least one value for eventually follows, exactly
// three for A-next-B-next-C, exactly two for the four-eyes predicate, and
// exactly one for the different-persons predicate). When enabled,
// timestamp_diff_boundaries supplies one inclusive [min,max] pair for every
// adjacent pair in an eventually-follows pattern.
struct LtlFilterOptions {
  LtlFilterVariant variant = LtlFilterVariant::kEventuallyFollows;
  std::string attribute_key;
  std::vector<std::string> attribute_values;
  bool positive = true;
  bool enable_timestamp = false;
  std::vector<std::pair<double, double>> timestamp_diff_boundaries;
  std::string resource_key;
};

// Which of pm4py's two ltl_checker implementations a log is judged by. pm4py
// 2.7.22 loads CSV and XES files as DataFrames, so an imported log follows the
// DataFrame checker; only a log that arrives as an EventLog object (a native
// log JSON) follows the EventLog checker. They differ in three rules:
//  - eventually follows with timestamp bounds: the DataFrame checks each bound
//    between the matched events (searching every ordered occurrence), the
//    EventLog between trace positions i and i + 1;
//  - A-next-B-next-C: the DataFrame requires the last B and last C to be
//    adjacent and preceded by an A, the EventLog accepts any adjacent window;
//  - four eyes: the DataFrame compares the A resources with the resource of
//    the last B only, the EventLog with every B resource.
// The DataFrame checker also counts an empty-string resource as a value.
enum class LtlInputShape {
  kImportedTable,
  kEventLogObject,
};

// Returns a copy of the input log containing only traces satisfying (positive)
// or not satisfying (negative) the selected predicate. Event and trace data,
// including generic attributes, are preserved verbatim. The two-argument form
// judges the log as an imported table.
[[nodiscard]] EventLog filter_ltl_event_log(const EventLog& event_log,
                                            const LtlFilterOptions& options);
[[nodiscard]] EventLog filter_ltl_event_log(const EventLog& event_log,
                                            const LtlFilterOptions& options,
                                            LtlInputShape shape);

// Short aliases for the native predicate filter.
[[nodiscard]] EventLog filter_ltl(const EventLog& event_log,
                                  const LtlFilterOptions& options);
[[nodiscard]] EventLog filter_event_log_ltl(
    const EventLog& event_log, const LtlFilterOptions& options);

}  // namespace minera
