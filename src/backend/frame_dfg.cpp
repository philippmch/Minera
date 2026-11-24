#include "minera/frame_dfg.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>
#include <tuple>

#include "minera/dataframe_columns.hpp"
#include "frame_key_column.hpp"
#include "performance_arithmetic.hpp"
#include "python_scalar.hpp"

namespace minera {
namespace {

bool equals_ignoring_case(std::string_view text, std::string_view word) {
  if (text.size() != word.size()) return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i] >= 'A' && text[i] <= 'Z' ? static_cast<char>(text[i] - 'A' + 'a') : text[i];
    if (c != word[i]) return false;
  }
  return true;
}

// The cell as the sort reads it: read_csv's object column of a Boolean
// column with a gap holds Python bools (the CSV views keep their text).
python::Scalar sort_cell(const DataFrameColumns& frame, const DataFrameColumn& column,
                         const std::optional<EventAttribute>& cell) {
  if (!cell) return python::none();
  if (frame.csv_text && column.type == DataFrameColumnType::kObject && !cell->is_null &&
      cell->kind == AttributeKind::kString) {
    if (equals_ignoring_case(cell->value, "true")) return python::boolean(true);
    if (equals_ignoring_case(cell->value, "false")) return python::boolean(false);
  }
  return detail::frame_key_cell(&*cell);
}

}  // namespace

std::optional<FrameDfgStartOrder> frame_dfg_start_order(const EventLog& log) {
  if (!log.dataframe_columns) return std::nullopt;
  const auto& frame = *log.dataframe_columns;
  const DataFrameColumn* column = nullptr;
  for (const auto& entry : frame.columns)
    if (entry.key == "start_timestamp") column = &entry;
  if (!column || column->role == FirstClassAttributeRole::kTimestamp) return std::nullopt;

  FrameDfgStartOrder result;
  result.order.resize(log.traces.size());
  result.start.resize(log.traces.size());
  const bool dates = column->type == DataFrameColumnType::kDate;
  if (!dates) {
    const bool numbers = column->type == DataFrameColumnType::kInt64 ||
                         column->type == DataFrameColumnType::kFloat64;
    result.subtraction_error = std::string("TypeError: cannot subtract DatetimeArray from ndarray[") +
                               (numbers ? "float64" : "object") + "]";
    result.business_hours_error = "AttributeError: Can only use .dt accessor with datetimelike values";
  }
  // pandas sorts the whole frame: the start column's ranks over every row.
  detail::FrameKeyColumn cells;
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto& events = log.traces[t].events;
    result.start[t].resize(events.size());
    for (std::size_t e = 0; e < events.size(); ++e) {
      const auto cell = dataframe_event_cell(frame, *column, events[e]);
      auto scalar = sort_cell(frame, *column, cell);
      if (dates && !detail::frame_key_missing(scalar) && scalar.kind == python::Scalar::Kind::kDate)
        result.start[t][e] = scalar.date;
      cells.cells.push_back(std::move(scalar));
    }
  }
  const auto ranks = detail::frame_key_pandas_ranks(cells);
  std::size_t row = 0;
  constexpr auto kLast = std::numeric_limits<std::int64_t>::max();
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto& events = log.traces[t].events;
    struct Key {
      std::size_t rank;
      std::int64_t seconds;
      std::uint32_t nanoseconds;
      std::size_t input_order;
      std::size_t position;
    };
    std::vector<Key> keys;
    keys.reserve(events.size());
    for (std::size_t e = 0; e < events.size(); ++e, ++row) {
      const auto instant = parse_timestamp_exact(events[e].timestamp);
      keys.push_back(Key{ranks[row], instant ? instant->seconds : kLast,
                         instant ? instant->nanoseconds : 0U, events[e].input_order, e});
    }
    std::stable_sort(keys.begin(), keys.end(), [](const Key& lhs, const Key& rhs) {
      return std::tie(lhs.rank, lhs.seconds, lhs.nanoseconds, lhs.input_order, lhs.position) <
             std::tie(rhs.rank, rhs.seconds, rhs.nanoseconds, rhs.input_order, rhs.position);
    });
    result.order[t].reserve(keys.size());
    for (const auto& key : keys) result.order[t].push_back(key.position);
  }
  return result;
}

DirectlyFollowsGraph frame_dfg_graph(const DirectlyFollowsGraph& graph, const EventLog& log,
                                     const FrameDfgStartOrder& order) {
  std::vector<std::vector<std::string_view>> cases;
  cases.reserve(log.traces.size());
  for (std::size_t t = 0; t < log.traces.size() && t < order.order.size(); ++t) {
    std::vector<std::string_view> activities;
    activities.reserve(order.order[t].size());
    for (const auto e : order.order[t]) activities.push_back(log.traces[t].events[e].activity);
    cases.push_back(std::move(activities));
  }
  return graph.with_edges_from(cases);
}

std::vector<FrameDfgPair> frame_dfg_pairs(const EventLog& log, const FrameDfgStartOrder& order) {
  std::vector<std::size_t> traces(std::min(log.traces.size(), order.order.size()));
  std::iota(traces.begin(), traces.end(), std::size_t{0});
  std::stable_sort(traces.begin(), traces.end(), [&](std::size_t lhs, std::size_t rhs) {
    return log.traces[lhs].case_id < log.traces[rhs].case_id;
  });
  std::vector<FrameDfgPair> pairs;
  for (const auto t : traces) {
    const auto& sequence = order.order[t];
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      FrameDfgPair pair;
      pair.trace = t;
      pair.previous = sequence[i - 1];
      pair.current = sequence[i];
      pair.previous_timestamp = parse_timestamp_exact(log.traces[t].events[pair.previous].timestamp);
      pair.current_start = order.start[t][pair.current];
      pair.seconds = std::numeric_limits<double>::quiet_NaN();
      if (pair.previous_timestamp && pair.current_start)
        if (const auto gap = detail::exact_gap(*pair.previous_timestamp, *pair.current_start))
          pair.seconds = detail::dataframe_gap_seconds(*gap);
      pairs.push_back(std::move(pair));
    }
  }
  return pairs;
}

}  // namespace minera
