#include "minera/variant_statistics.hpp"

#include "frame_cells.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {

const std::string* activity_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return &event.activity;
  if (key == "timestamp" || key == "time:timestamp") return &event.timestamp;
  if (key == "resource" || key == "org:resource") return &event.resource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return &event.lifecycle;
  return event.attribute(key);
}

}  // namespace

const char* variant_statistics_variant_name() noexcept { return "CLASSIC"; }

VariantStatisticsResult discover_variant_statistics(
    const EventLog& log, const VariantStatisticsOptions& options) {
  std::map<std::vector<std::string>, std::uint64_t> counts;
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto* value = activity_value(event, options.activity_key);
      if (!value)
        throw std::invalid_argument(
            "variant statistics: event is missing activity attribute '" +
            options.activity_key + "'");
      activities.push_back(*value);
    }
    ++counts[std::move(activities)];
  }

  std::vector<VariantStatisticsEntry> entries;
  entries.reserve(counts.size());
  for (auto& [activities, count] : counts)
    entries.push_back({std::move(activities), count, 0.0});
  std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
    if (left.count != right.count) return left.count > right.count;
    return left.activities > right.activities;
  });
  std::uint64_t total = 0;
  for (const auto& entry : entries) total += entry.count;
  if (options.max_variants != 0 && entries.size() > options.max_variants)
    entries.resize(options.max_variants);
  if (total != 0) {
    for (auto& entry : entries)
      entry.probability = static_cast<double>(entry.count) /
                          static_cast<double>(total);
  }
  return {options.activity_key, std::move(entries)};
}

VariantStatisticsResult discover_variant_statistics_as_uploaded(
    const EventLog& log, const VariantStatisticsOptions& options) {
  if (!log.dataframe_columns || log.dataframe_columns->csv_text)
    return discover_variant_statistics(log, options);
  return discover_variant_statistics(frame_slice_cases(log), options);
}

}  // namespace minera
