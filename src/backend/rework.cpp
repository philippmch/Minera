#include "minera/rework.hpp"
#include "minera/dataframe_validation.hpp"
#include "minera/frame_keys.hpp"
#include "reference_activity_selector.hpp"

#include <map>
#include <optional>
#include <set>
#include <vector>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace minera {
namespace {

std::string activity_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (key == "resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  // A CSV's own header of a selected column (its source key; round BP).
  if (const auto* value = reference_selected_field(event, key)) return *value;
  throw std::invalid_argument("rework: event is missing activity attribute '" +
                              std::string(key) + "'");
}

std::string case_value(const Trace& trace, std::string_view key) {
  if (key.empty() || key == "case_id" || key == "concept:name")
    return trace.case_id;
  if (const auto* value = trace.attribute(key)) return *value;
  throw std::invalid_argument("rework: trace is missing case id attribute '" +
                              std::string(key) + "'");
}

}  // namespace

const char* rework_variant_name(ReworkVariant variant) {
  switch (variant) {
    case ReworkVariant::kActivities: return "ACTIVITIES";
    case ReworkVariant::kCases: return "CASES";
  }
  throw std::invalid_argument("rework: unknown variant");
}

ReworkResult discover_rework(const EventLog& log,
                             const ReworkOptions& options) {
  ReworkResult result;
  result.variant = options.variant;
  if (options.variant == ReworkVariant::kActivities) {
    for (const auto& trace : log.traces) {
      std::map<std::string, std::uint64_t> counts;
      for (const auto& event : trace.events)
        ++counts[activity_value(event, options.activity_key)];
      for (const auto& [activity, count] : counts)
        if (count > 1) ++result.activity_cases[activity];
    }
    return result;
  }
  if (options.variant == ReworkVariant::kCases) {
    for (const auto& trace : log.traces) {
      std::map<std::string, std::uint64_t> counts;
      for (const auto& event : trace.events)
        ++counts[activity_value(event, options.activity_key)];
      const auto case_id = case_value(trace, options.case_id_key);
      ReworkCaseStats stats;
      stats.number_activities = trace.events.size();
      std::uint64_t unique = 0;
      for (const auto& [unused, count] : counts) {
        (void)unused;
        if (count != 0) ++unique;
      }
      stats.rework = stats.number_activities - unique;
      result.cases[case_id] = stats;
    }
    return result;
  }
  throw std::invalid_argument("rework: unknown variant");
}

namespace {

// Round CW: the labels of a column of an upload's frame per event (python
// str of each cell), with the missing cells marked; nullopt when the frame
// does not pair with the log.
struct FrameLabels {
  std::vector<std::vector<std::string>> labels;
  std::vector<std::vector<bool>> missing;
  std::optional<DataFrameColumnType> column;
  bool any_missing = false;
};
std::optional<FrameLabels> frame_labels(const EventLog& log, const std::string& key) {
  FrameLabels out;
  if (key == "concept:name" || key == "case:concept:name") {
    out.column = DataFrameColumnType::kString;
    for (const auto& trace : log.traces) {
      auto& labels = out.labels.emplace_back();
      out.missing.emplace_back(trace.events.size(), false);
      for (const auto& event : trace.events) labels.push_back(key == "concept:name" ? event.activity : trace.case_id);
    }
    return out;
  }
  const auto keyed = frame_keyed_activities(log, key, FrameKeyScope::kDataFrame);
  if (!keyed) return std::nullopt;
  out.column = keyed->column;
  out.any_missing = keyed->any_missing;
  out.missing = keyed->missing;
  for (const auto& trace : keyed->log.traces) {
    auto& labels = out.labels.emplace_back();
    for (const auto& event : trace.events) labels.push_back(event.activity);
  }
  return out;
}
std::string standard_key(const std::string& key, std::string_view alias, const char* standard) {
  return key.empty() || key == alias ? std::string(standard) : key;
}
bool text_column(const FrameLabels& labels) {
  return labels.column == DataFrameColumnType::kString || labels.column == DataFrameColumnType::kObject;
}

}  // namespace

ReworkResult discover_rework_as_uploaded(const EventLog& log, const ReworkOptions& options) {
  if (!log.dataframe_columns) return discover_rework(log, options);
  const auto activity_key = standard_key(options.activity_key, "activity", "concept:name");
  const auto case_key = standard_key(options.case_id_key, "case_id", "case:concept:name");
  const auto columns = frame_keys(log, FrameKeyScope::kDataFrame);
  const auto available = [&] { return dataframe_available_columns({columns.begin(), columns.end()}); };
  ReworkResult result;
  result.variant = options.variant;
  if (options.variant == ReworkVariant::kActivities) {
    // pm4py.get_rework_cases_per_activity checks the case column, then the
    // activity column, then counts the cases (groups of the case column)
    // holding an activity more than once (round CW, observed black-box).
    if (!columns.contains(case_key))
      throw std::invalid_argument("the specified case ID column is not contained in the dataframe. Available columns: " + available());
    const auto cases = frame_labels(log, case_key);
    if (!cases) return discover_rework(log, options);
    if (!text_column(*cases)) throw std::invalid_argument("the case ID column should be of type string.");
    if (cases->any_missing) throw std::invalid_argument("the case ID column should not contain any empty value.");
    if (!columns.contains(activity_key))
      throw std::invalid_argument("the specified activity column is not contained in the dataframe. Available columns: " + available());
    const auto activities = frame_labels(log, activity_key);
    if (!activities) return discover_rework(log, options);
    if (!text_column(*activities)) throw std::invalid_argument("the activity column should be of type string.");
    if (activities->any_missing) throw std::invalid_argument("the activity column should not contain any empty value.");
    std::map<std::pair<std::string, std::string>, std::uint64_t> counts;
    for (std::size_t t = 0; t < log.traces.size(); ++t)
      for (std::size_t e = 0; e < log.traces[t].events.size(); ++e)
        ++counts[{cases->labels[t][e], activities->labels[t][e]}];
    for (const auto& [pair, count] : counts)
      if (count > 1) ++result.activity_cases[pair.second];
    return result;
  }
  if (options.variant == ReworkVariant::kCases) {
    // The cases variant groups the frame by the case column, reads the
    // activity column (a missing cell counts nowhere) and then names
    // case:concept:name whatever the case key (round CW, observed
    // black-box: pandas' KeyErrors).
    if (!columns.contains(case_key)) throw std::invalid_argument("KeyError: '" + case_key + "'");
    if (!columns.contains(activity_key)) throw std::invalid_argument("KeyError: 'Column not found: " + activity_key + "'");
    if (case_key != "case:concept:name") throw std::invalid_argument("KeyError: 'case:concept:name'");
    const auto activities = frame_labels(log, activity_key);
    if (!activities) return discover_rework(log, options);
    for (std::size_t t = 0; t < log.traces.size(); ++t) {
      std::set<std::string> distinct;
      ReworkCaseStats stats;
      for (std::size_t e = 0; e < log.traces[t].events.size(); ++e) {
        if (activities->missing[t][e]) continue;
        ++stats.number_activities;
        distinct.insert(activities->labels[t][e]);
      }
      stats.rework = stats.number_activities - distinct.size();
      result.cases[log.traces[t].case_id] = stats;
    }
    return result;
  }
  throw std::invalid_argument("rework: unknown variant");
}

}  // namespace minera
