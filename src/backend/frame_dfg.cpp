#include "minera/frame_dfg.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string_view>
#include <tuple>

#include "minera/dataframe_columns.hpp"
#include "frame_cells.hpp"
#include "frame_key_column.hpp"
#include "performance_arithmetic.hpp"
#include "polars_missing_column.hpp"
#include "polars_sort.hpp"
#include "python_scalar.hpp"
#include "xes_frame_checks.hpp"

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

// Polars' error resolving the performance DFG's plan over a start column of
// no dates (`dtype` the copy's: i64, u64, f64, str or bool), the frame's columns
// in `columns` (round GV, observed black-box).
std::string polars_frame_dfg_error(std::string_view dtype, bool nanoseconds, const std::vector<std::string>& columns,
                                   const std::string& case_key, const std::string& activity_key,
                                   const std::string& timestamp_key, const std::string& start_key) {
  const std::string datetime = nanoseconds ? "datetime[ns, UTC]" : "datetime[\xce\xbcs, UTC]";
  const auto q = [](const std::string& name) { return "col(" + detail::polars_frame_quoted(name) + ")"; };
  std::vector<std::string> selected;
  for (const auto& name : columns)
    if (name == case_key || name == activity_key || name == timestamp_key || name == start_key) selected.push_back(name);
  const auto shift = [&](const std::string& name) {
    return q(name) + ".shift([dyn int: -1]).over([" + q(case_key) + "]).alias(" +
           detail::polars_frame_quoted(name + "_2") + ")";
  };
  // The plan below the failing node, each line indented by `pad` more.
  const auto body = [&](const std::string& pad) {
    return pad + "FILTER " + q(activity_key + "_2") + ".is_not_null()\n" + pad + "FROM\n" + pad + "  FILTER " +
           q(case_key + "_2") + ".is_not_null()\n" + pad + "  FROM\n" + pad + "     WITH_COLUMNS:\n" + pad +
           "     [" + shift(case_key) + ", " + shift(activity_key) + ", " + shift(start_key) + ", " +
           shift(timestamp_key) + "] \n" + pad + "      SORT BY [" + q(case_key) + ", " + q(start_key) + ", " +
           q(timestamp_key) + "]\n" + pad + "        SELECT " + detail::polars_plan_cols(selected) + "\n" + pad +
           "          " + detail::polars_df_node(columns) + "\n";
  };
  const std::string head = "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n";
  if (dtype == "bool")
    return "failed to determine supertype of bool and " + datetime + head + body("") +
           "\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n\t[2] 'with_columns'\n"
           "\t[3] 'group_by'\n";
  const bool unsigned_start = dtype == "u64";
  if (unsigned_start) dtype = "i64";
  const bool numeric = dtype == "i64" || dtype == "f64";
  const std::string horizontal = " WITH_COLUMNS:\n [" + q(start_key + "_2") +
                                 (unsigned_start ? ".strict_cast(Int64)" : "") + ".max_horizontal([" + q(timestamp_key) +
                                 (numeric ? std::string(".strict_cast(") + (dtype == "i64" ? "Int64" : "Float64") + ")"
                                          : std::string()) +
                                 "])] \n";
  if (numeric)
    return "- not allowed on " + std::string(dtype) + " and " + datetime + head + horizontal + body("  ") +
           "\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n\t[2] 'group_by'\n";
  return "got invalid or ambiguous dtypes: '[str, " + datetime + "]' in expression 'max_horizontal'\n\nConsider "
         "explicitly casting your input types to resolve potential ambiguity." + head + horizontal + body("  ") +
         "\nThis error occurred with the following context stack:\n\t[1] 'with_columns' failed\n\t[2] "
         "'with_columns'\n\t[3] 'group_by'\n";
}

}  // namespace

std::optional<FrameDfgStartOrder> frame_dfg_start_order(const EventLog& log) {
  return frame_dfg_start_order(log, false, nullptr);
}

std::optional<FrameDfgStartOrder> frame_dfg_start_order(const EventLog& log, bool polars,
                                                        const std::vector<std::string>* columns,
                                                        bool timestamp_start) {
  if (!log.dataframe_columns) return std::nullopt;
  const auto& frame = *log.dataframe_columns;
  const DataFrameColumn* column = nullptr;
  for (const auto& entry : frame.columns)
    if (entry.key == "start_timestamp") column = &entry;
  // Each event starting at its timestamp: the frame's timestamp column (an
  // XES frame lists none; its events hold the timestamps).
  const bool own_timestamps = timestamp_start && polars && !column;
  DataFrameColumn timestamp_column;
  if (own_timestamps) {
    timestamp_column.key = "time:timestamp";
    timestamp_column.type = DataFrameColumnType::kDate;
    timestamp_column.role = FirstClassAttributeRole::kTimestamp;
    column = &timestamp_column;
  } else if (!column || column->role == FirstClassAttributeRole::kTimestamp) {
    return std::nullopt;
  }

  FrameDfgStartOrder result;
  result.polars = polars;
  result.order.resize(log.traces.size());
  result.start.resize(log.traces.size());
  result.rank.resize(log.traces.size());
  const bool dates = column->type == DataFrameColumnType::kDate;
  if (!dates && !polars) {
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
      python::Scalar scalar;
      if (own_timestamps) {
        if (const auto instant = parse_timestamp_exact(events[e].timestamp)) {
          scalar.kind = python::Scalar::Kind::kDate;
          scalar.date = *instant;
          scalar.text = events[e].timestamp;
        }
      } else {
        scalar = sort_cell(frame, *column, dataframe_event_cell(frame, *column, events[e]));
      }
      if (dates && !detail::frame_key_missing(scalar) && scalar.kind == python::Scalar::Kind::kDate)
        result.start[t][e] = scalar.date;
      cells.cells.push_back(std::move(scalar));
    }
  }
  if (polars) {
    // Polars sorts the whole copy (in the frame's row order) by case, start
    // and timestamp, nulls first; each case's rows keep that order.
    auto key_column = detail::frame_key_column(std::move(cells.cells));
    // A CSV frame's copy takes the pandas dtype: an object column (Python
    // ints past uint64, booleans beside a gap, mixed cells) and a str one
    // become String, spelled by str().
    if (frame.csv_text && !dates) {
      const auto type = frame_column_type(log, "start_timestamp");
      if (type == DataFrameColumnType::kString || type == DataFrameColumnType::kObject)
        key_column.dtype = detail::FrameKeyDtype::kStr;
    }
    const auto ranks = detail::frame_key_polars_ranks(key_column);
    const auto past_int64 = [](const python::Scalar& cell) {
      return cell.kind == python::Scalar::Kind::kInt && !cell.negative &&
             (cell.digits.size() > 19 || (cell.digits.size() == 19 && cell.digits > "9223372036854775807"));
    };
    bool unsigned_start = false;
    if (!dates) {
      // The copy's dtype: an all-empty column Float64, a Boolean one with a
      // gap (object) String.
      const bool empty = std::all_of(key_column.cells.begin(), key_column.cells.end(), detail::frame_key_missing);
      const char* dtype = empty ? "f64" : detail::frame_key_polars_dtype(key_column.dtype);
      // read_csv's uint64 column (an int past int64, none negative): its
      // copy is UInt64, cast to Int64 beside the timestamp.
      if (key_column.dtype == detail::FrameKeyDtype::kInt64 &&
          std::any_of(key_column.cells.begin(), key_column.cells.end(), past_int64)) {
        dtype = "u64";
        unsigned_start = true;
      }
      const DataFrameColumn* timestamp = nullptr;
      for (const auto& entry : frame.columns)
        if (entry.role == FirstClassAttributeRole::kTimestamp) timestamp = &entry;
      const auto& names = columns ? *columns : frame.order;
      result.subtraction_error = polars_frame_dfg_error(dtype, timestamp && timestamp->nanoseconds, names,
                                                        "case:concept:name", "concept:name", "time:timestamp",
                                                        "start_timestamp");
      result.business_hours_error = result.subtraction_error;
      // With business hours a numeric start resolves (max_horizontal casts
      // the timestamp to its dtype) and the business-time UDF gets that
      // number at the first pair: replace() fails (none without a pair).
      if (dtype == std::string_view("i64") || dtype == std::string_view("u64") || dtype == std::string_view("f64")) {
        result.business_hours_error.clear();
        for (const auto& trace : log.traces)
          if (trace.events.size() > 1) {
            result.business_hours_error =
                std::string("AttributeError: '") + (dtype == std::string_view("f64") ? "float" : "int") +
                "' object has no attribute 'replace'\n\nThis error occurred in the following expression:\n"
                "\tas_struct(\"time:timestamp\", \"start_timestamp_2\").python_udf()\n";
            break;
          }
      }
    }
    struct Row {
      std::size_t trace, event, rank, position;
      std::optional<ParsedExactTimestamp> instant;
      std::size_t flat;
    };
    std::vector<Row> rows;
    std::size_t flat = 0;
    for (std::size_t t = 0; t < log.traces.size(); ++t) {
      const auto& events = log.traces[t].events;
      result.rank[t].reserve(events.size());
      for (std::size_t e = 0; e < events.size(); ++e, ++flat) {
        result.rank[t].push_back(ranks[flat]);
        const auto input = events[e].input_order;
        const std::size_t position =
            frame.csv_text && input < frame.row_index.size() ? static_cast<std::size_t>(frame.row_index[input]) : flat;
        rows.push_back(Row{t, e, ranks[flat], position, parse_timestamp_exact(events[e].timestamp), flat});
      }
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row& lhs, const Row& rhs) { return lhs.position < rhs.position; });
    polars_sort::sort_unstable(rows, [&](const Row& lhs, const Row& rhs) {
      const auto& a = log.traces[lhs.trace].case_id;
      const auto& b = log.traces[rhs.trace].case_id;
      if (a != b) return a < b;
      if (lhs.rank != rhs.rank) return lhs.rank < rhs.rank;
      if (!lhs.instant || !rhs.instant) return !lhs.instant && rhs.instant;
      return std::tie(lhs.instant->seconds, lhs.instant->nanoseconds) <
             std::tie(rhs.instant->seconds, rhs.instant->nanoseconds);
    });
    for (const auto& entry : rows) result.order[entry.trace].push_back(entry.event);
    if (unsigned_start) {
      // With business hours the UInt64 start's strict cast to Int64 runs
      // over the pairs' later starts in the sorted order and fails on the
      // values past int64: Polars lists up to ten (two, an ellipsis and the
      // tenth or last beyond three).
      std::vector<std::string> failed;
      std::size_t failures = 0, values = 0;
      for (std::size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].trace != rows[i - 1].trace) continue;
        ++values;
        const auto& cell = key_column.cells[rows[i].flat];
        if (!past_int64(cell)) continue;
        ++failures;
        if (failed.size() < 10) failed.push_back(cell.digits);
      }
      if (failures) {
        std::string listed;
        if (failed.size() <= 3) {
          for (std::size_t i = 0; i < failed.size(); ++i) listed += (i ? ", " : "") + failed[i];
        } else {
          listed = failed[0] + ", " + failed[1] + ", \xe2\x80\xa6 " + failed.back();
        }
        result.business_hours_error =
            "conversion from `u64` to `i64` failed in column 'start_timestamp_2' for " + std::to_string(failures) +
            " out of " + std::to_string(values) + " values: [" + listed + "]\n\n" +
            (failures > 10 ? "Did not show all failed cases as there were too many.\n\n" : "") +
            "This error occurred in the following expression:\n\tcol(\"start_timestamp_2\").strict_cast(Int64)\n"
            "while evaluating this larger expression:\n\tcol(\"start_timestamp_2\").strict_cast(Int64).max_horizontal("
            "[col(\"time:timestamp\").strict_cast(Int64)])\n";
      }
    }
    return result;
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
    result.rank[t].reserve(events.size());
    for (std::size_t e = 0; e < events.size(); ++e, ++row) {
      result.rank[t].push_back(ranks[row]);
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
      if (order.polars) {
        // max_horizontal(start, timestamp) - timestamp, in whole seconds.
        pair.seconds = 0.0;
        if (pair.previous_timestamp && pair.current_start)
          if (const auto gap = detail::exact_gap(*pair.previous_timestamp, *pair.current_start);
              gap && gap->seconds > 0)
            pair.seconds = static_cast<double>(gap->seconds);
      } else if (pair.previous_timestamp && pair.current_start) {
        if (const auto gap = detail::exact_gap(*pair.previous_timestamp, *pair.current_start))
          pair.seconds = detail::dataframe_gap_seconds(*gap);
      }
      pairs.push_back(std::move(pair));
    }
  }
  return pairs;
}

}  // namespace minera
