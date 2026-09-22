#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/timestamp.hpp"

namespace minera::detail {
namespace concept_drift_table_output_detail {

// Keep generated materialization under the same native event-copy ceiling used
// by the EventLog concept-drift implementation.
constexpr std::size_t kMaxGeneratedEvents = 20'000'000;
constexpr std::int64_t kGeneratedEpochSeconds = 10'000'000;

inline EventAttribute string_cell(std::string key, std::string value,
                                  FirstClassAttributeRole role) {
  EventAttribute result{std::move(key), std::move(value),
                        AttributeKind::kString};
  result.first_class_role = role;
  result.first_class_state = FirstClassAttributeState::kPresent;
  result.first_class_source_key = result.key;
  result.container = AttributeContainerKind::kScalar;
  result.typed_value = std::monostate{};
  return result;
}

inline EventAttribute timestamp_cell(std::string value) {
  EventAttribute result{"time:timestamp", value, AttributeKind::kDate};
  result.typed_value = NativeDate{value, NativeDateResolution::kMicrosecond,
                                  std::string("UTC")};
  result.first_class_role = FirstClassAttributeRole::kTimestamp;
  result.first_class_state = FirstClassAttributeState::kPresent;
  result.first_class_source_key = result.key;
  result.container = AttributeContainerKind::kScalar;
  return result;
}

inline EventAttribute index_cell(std::int64_t value) {
  EventAttribute result{"", std::to_string(value), AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

}  // namespace concept_drift_table_output_detail

// Convert nonempty BOSE returned partitions into fresh public-compatible
// frames. Each partition is a vector of nonempty activity traces. Empty
// partition/trace behavior remains intentionally unresolved at this internal
// boundary; callers must filter or handle those cases before invoking this
// helper.
inline std::vector<EventTable> make_bose_output_tables(
    const std::vector<std::vector<std::vector<std::string>>>& partitions) {
  using namespace concept_drift_table_output_detail;

  std::size_t total_events = 0;
  for (const auto& partition : partitions) {
    if (partition.empty()) {
      throw std::invalid_argument(
          "concept drift table output: empty partitions are not supported");
    }
    for (const auto& trace : partition) {
      if (trace.empty()) {
        throw std::invalid_argument(
            "concept drift table output: empty traces are not supported");
      }
      if (total_events > kMaxGeneratedEvents ||
          trace.size() > kMaxGeneratedEvents - total_events) {
        throw std::invalid_argument(
            "concept drift table output: generated event materialization exceeds safe bound");
      }
      total_events += trace.size();
    }
  }

  std::vector<EventTable> result;
  result.reserve(partitions.size());
  for (const auto& partition : partitions) {
    EventTable table;
    table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
    table.dtypes = {"str", "str", "datetime64[us, UTC]"};
    table.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    std::size_t partition_events = 0;
    for (const auto& trace : partition) partition_events += trace.size();
    table.rows.reserve(partition_events);
    table.index->labels.reserve(partition_events);

    std::size_t event_ordinal = 0;
    for (std::size_t trace_index = 0; trace_index < partition.size();
         ++trace_index) {
      const auto& trace = partition[trace_index];
      for (const auto& activity : trace) {
        if (event_ordinal >
            static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max)() -
                                     kGeneratedEpochSeconds)) {
          throw std::invalid_argument(
              "concept drift table output: generated timestamp exceeds safe bound");
        }
        const auto seconds = static_cast<double>(
            kGeneratedEpochSeconds + static_cast<std::int64_t>(event_ordinal));
        const auto timestamp = format_timestamp_microseconds(seconds);

        EventTable::Row row;
        row.reserve(3);
        row.emplace_back(string_cell("case:concept:name",
                                     std::to_string(trace_index),
                                     FirstClassAttributeRole::kCaseId));
        row.emplace_back(string_cell("concept:name", activity,
                                     FirstClassAttributeRole::kActivity));
        row.emplace_back(timestamp_cell(timestamp));
        table.rows.push_back(std::move(row));
        table.index->labels.push_back(
            {index_cell(static_cast<std::int64_t>(event_ordinal))});
        ++event_ordinal;
      }
    }
    result.push_back(std::move(table));
  }
  return result;
}

}  // namespace minera::detail
