#include "minera/variant_statistics.hpp"
#include "reference_activity_selector.hpp"

#include "frame_cells.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "polars_sort.hpp"

#include <algorithm>
#include <numeric>
#include <optional>
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
  if (const auto* value = event.attribute(key)) return value;
  // A CSV's own header of a selected column (its source key; round BP).
  return reference_selected_field(event, key);
}

VariantStatisticsResult ranked_variants(std::map<std::vector<std::string>, std::uint64_t> counts,
                                        const VariantStatisticsOptions& options) {
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

bool numeric_dtype(std::string_view kind) {
  return kind.rfind("int", 0) == 0 || kind.rfind("uint", 0) == 0 || kind.rfind("float", 0) == 0 ||
         kind.rfind("Int", 0) == 0 || kind.rfind("UInt", 0) == 0 || kind.rfind("Float", 0) == 0;
}

bool text_dtype(std::string_view kind) {
  return kind == "str" || kind == "object" || kind == "string" || kind == "String";
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
  return ranked_variants(std::move(counts), options);
}

VariantStatisticsResult discover_variant_statistics(
    const LazyEventTable& frame, const VariantStatisticsOptions& options) {
  const auto table = frame.materialize();
  const auto& key = options.activity_key;
  const std::string activity_column =
      key.empty() || key == "activity" || key == "concept:name" ? "concept:name" : key;
  const auto column_of = [&](const std::string& name) {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end())
      throw std::invalid_argument("variant statistics: the frame has no column '" + name + "'");
    return static_cast<std::size_t>(found - table.columns.begin());
  };
  const auto case_at = column_of("case:concept:name");
  const auto time_at = column_of("time:timestamp");
  const auto activity_at = column_of(activity_column);
  const auto dtype = [&](std::size_t at) {
    return at < table.dtypes.size() ? table.dtypes[at] : std::string();
  };
  const bool case_numeric = numeric_dtype(dtype(case_at));
  const bool time_text = text_dtype(dtype(time_at));
  struct Key {
    std::optional<std::string> case_text;
    double case_number = 0;
    std::optional<std::string> time_text;
    std::optional<ParsedExactTimestamp> time;
  };
  const auto present = [](const std::optional<EventAttribute>& cell) { return cell && !cell->is_null; };
  std::vector<Key> keys(table.rows.size());
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    const auto& cells = table.rows[row];
    // pm4py checks the frame's case and activity columns for missing values
    // before it sorts (check_pandas_dataframe_columns).
    if (!present(cells[case_at]))
      throw std::invalid_argument("the case ID column should not contain any empty value.");
    if (!present(cells[activity_at]))
      throw std::invalid_argument("the activity column should not contain any empty value.");
    keys[row].case_text = cells[case_at]->value;
    if (case_numeric) {
      try { keys[row].case_number = std::stod(cells[case_at]->value); } catch (const std::exception&) {}
    }
    if (present(cells[time_at])) {
      keys[row].time_text = cells[time_at]->value;
      if (!time_text) keys[row].time = parse_timestamp_exact(cells[time_at]->value);
    }
  }
  const auto same_case = [&](std::size_t a, std::size_t b) {
    return case_numeric ? keys[a].case_number == keys[b].case_number : *keys[a].case_text == *keys[b].case_text;
  };
  std::vector<std::size_t> order(table.rows.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  polars_sort::sort_by_keys(order, [&](std::size_t a, std::size_t b) {
    const auto& lhs = keys[a];
    const auto& rhs = keys[b];
    if (!same_case(a, b))
      return case_numeric ? lhs.case_number < rhs.case_number : *lhs.case_text < *rhs.case_text;
    if (time_text || !lhs.time || !rhs.time) {
      const auto& x = time_text || lhs.time ? lhs.time_text : std::optional<std::string>();
      const auto& y = time_text || rhs.time ? rhs.time_text : std::optional<std::string>();
      if (!x || !y) return !x && y;
      return *x < *y;
    }
    if (lhs.time->seconds != rhs.time->seconds) return lhs.time->seconds < rhs.time->seconds;
    return lhs.time->nanoseconds < rhs.time->nanoseconds;
  }, dtype(case_at), dtype(time_at));
  std::map<std::vector<std::string>, std::uint64_t> counts;
  for (std::size_t at = 0; at < order.size();) {
    std::size_t end = at + 1;
    while (end < order.size() && same_case(order[at], order[end])) ++end;
    std::vector<std::string> activities;
    activities.reserve(end - at);
    for (auto i = at; i < end; ++i) activities.push_back(table.rows[order[i]][activity_at]->value);
    ++counts[std::move(activities)];
    at = end;
  }
  return ranked_variants(std::move(counts), options);
}

VariantStatisticsResult discover_variant_statistics_as_uploaded(
    const EventLog& log, const VariantStatisticsOptions& options) {
  // pm4py.get_variants checks the key column of the frame first, then reads
  // its cells, the upload's own case column too (round BT).
  if (const auto keyed = frame_keyed_activities(log, options.activity_key)) {
    if (const auto error = frame_activity_column_error(*keyed); !error.empty())
      throw std::invalid_argument(error);
    auto projected = options;
    projected.activity_key = "concept:name";
    auto result = discover_variant_statistics_as_uploaded(keyed->log, projected);
    result.activity_key = options.activity_key;
    return result;
  }
  if (!log.dataframe_columns || log.dataframe_columns->csv_text)
    return discover_variant_statistics(log, options);
  return discover_variant_statistics(frame_slice_cases(log), options);
}

}  // namespace minera
