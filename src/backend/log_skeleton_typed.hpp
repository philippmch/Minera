#pragma once

// pm4py's log skeleton conformance on a log keyed by values other than text
// (round HH, observed black-box): each trace's deviation list is sorted with
// Python's list.sort, and two activity-frequency deviations whose activities
// Python cannot order (text beside a number or a date, None beside anything)
// raise that comparison's TypeError for the first such pair the sort meets.
// `trace_types` names each label's Python type as the replayed events hold
// it, `model_types` as the model's keys hold it ("" or "str" for a str),
// `model_order` the model's keys as its discovery met them (first
// occurrence in the discovery log): the order of the absent activities in
// the deviation list before the sort.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "minera/log_skeleton.hpp"

namespace minera::detail {

[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance_typed(
    const EventLog& log, const LogSkeletonModel& model, const LogSkeletonOptions& options,
    const std::map<std::string, std::string>& trace_types, const std::map<std::string, std::string>& model_types,
    const std::vector<std::string>& model_order);

// The case groups of pm4py's log skeleton on a frame keyed by a typed
// column (round HO), in its groupby order: each row's label (str() of the
// first object equal to its own) and str() of its own object, for
// name_log_skeleton_pairs. nullopt unless the column is typed.
struct LogSkeletonFrameObjects {
  std::vector<std::vector<std::string>> labels, printed;
};
[[nodiscard]] std::optional<LogSkeletonFrameObjects> log_skeleton_xes_frame_objects(const EventDataView& data,
                                                                                   const LogSkeletonOptions& options);

}  // namespace minera::detail
