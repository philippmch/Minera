#include "minera/link_analysis.hpp"

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "frame_cells.hpp"
#include "python_scalar.hpp"
#include "numpy_object_sort.hpp"

#include <optional>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace minera {
namespace {

struct Row {
  const Trace* trace = nullptr;
  const Event* event = nullptr;
  std::size_t trace_index = 0;
  std::size_t event_index = 0;
  std::size_t input_index = 0;
  std::size_t index = 0;
  std::string out_value;
  std::string in_value;
  std::string sorting_value;
  // Round CR: the merge keys (the values themselves outside a frame) and an
  // upload's typed sorting cell.
  std::string out_key;
  std::string in_key;
  std::optional<python::Scalar> sorting_cell;
};

std::string value_for(const Trace& trace, const Event& event,
                      std::size_t event_index, std::string_view column) {
  if (column == "case_id" || column == "case:concept:name" ||
      column == "case")
    return trace.case_id;
  if (column == "activity" || column == "concept:name") return event.activity;
  if (column == "timestamp" || column == "time:timestamp")
    return event.timestamp;
  if (column == "resource" || column == "org:resource") return event.resource;
  if (column == "lifecycle" || column == "lifecycle:transition")
    return event.lifecycle;
  if (column == "input_order") return std::to_string(event.input_order);
  if (column == "event_index") return std::to_string(event_index);

  if (const auto* value = event.attribute(column)) return *value;
  std::string_view trace_column = column;
  if (trace_column.starts_with("case:")) trace_column.remove_prefix(5);
  if (const auto* value = trace.attribute(trace_column)) return *value;
  if (const auto* value = trace.attribute(column)) return *value;
  return {};
}

bool is_builtin_column(std::string_view column) {
  return column == "case_id" || column == "case:concept:name" ||
         column == "case" || column == "activity" ||
         column == "concept:name" || column == "timestamp" ||
         column == "time:timestamp" || column == "resource" ||
         column == "org:resource" || column == "lifecycle" ||
         column == "lifecycle:transition" || column == "input_order" ||
         column == "event_index";
}

void validate_column(const EventLog& log, std::string_view column,
                     std::string_view option_name) {
  if (column.empty())
    throw std::invalid_argument(std::string(option_name) +
                                " must not be empty");
  if (is_builtin_column(column)) return;
  for (const auto& trace : log.traces) {
    if (trace.attribute(column) != nullptr) return;
    std::string_view stripped = column;
    if (stripped.starts_with("case:")) stripped.remove_prefix(5);
    if (trace.attribute(stripped) != nullptr) return;
    for (const auto& event : trace.events) {
      if (event.attribute(column) != nullptr) return;
    }
  }
  throw std::invalid_argument(std::string(option_name) +
                              " does not exist in the event log");
}

bool is_reserved_index_field(std::string_view column) {
  return column == "trace_index" || column == "event_index" ||
         column == "case_id" || column == "activity" ||
         column == "timestamp" || column == "value";
}

LinkAnalysisEvent event_ref(const Row& row, std::string value) {
  return {row.index, row.trace_index, row.event_index, row.trace->case_id,
          row.event->activity, row.event->timestamp, std::move(value)};
}

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          out << "\\u00" << hex[(static_cast<unsigned char>(c) >> 4) & 0xF]
              << hex[static_cast<unsigned char>(c) & 0xF];
        } else {
          out << c;
        }
    }
  }
  out << '"';
}

void write_event(std::ostream& out, const LinkAnalysisEvent& event,
                 std::string_view index_column) {
  out << "{\"index\":" << event.index;
  if (index_column != "index") {
    out << ',';
    write_json_string(out, index_column);
    out << ':' << event.index;
  }
  out << ",\"trace_index\":"
      << event.trace_index << ",\"event_index\":" << event.event_index
      << ",\"case_id\":";
  write_json_string(out, event.case_id);
  out << ",\"activity\":";
  write_json_string(out, event.activity);
  out << ",\"timestamp\":";
  write_json_string(out, event.timestamp);
  out << ",\"value\":";
  write_json_string(out, event.value);
  out << '}';
}


// ---------------------------------------------------------------- frames
// Round CR: on an upload pm4py's link analysis reads the frame (observed
// black-box): it sorts the rows by the sorting column's typed cells
// (missing cells last), numbers them, and merges the frame with itself on
// out_column = in_column, which pandas refuses for some dtype pairs and
// otherwise matches as Python values (1, 1.0 and True alike; missing cells
// match each other).

struct FrameColumn {
  std::string name;
  std::optional<DataFrameColumnType> type;
  std::string dtype;  // pandas' name of the dtype
};

std::string frame_column_name(std::string_view column) {
  if (column == "case_id") return "case:concept:name";
  if (column == "activity") return "concept:name";
  if (column == "timestamp") return "time:timestamp";
  if (column == "resource") return "org:resource";
  if (column == "lifecycle") return "lifecycle:transition";
  return std::string(column);
}

FrameColumn frame_column(const EventLog& frame, const std::set<std::string>& keys, std::string_view requested) {
  FrameColumn column;
  column.name = frame_column_name(requested);
  if (!keys.contains(column.name)) throw std::invalid_argument("KeyError: '" + column.name + "'");
  column.type = frame_column_type(frame, column.name);
  const bool csv = frame.dataframe_columns && frame.dataframe_columns->csv_text;
  switch (column.type.value_or(DataFrameColumnType::kString)) {
    case DataFrameColumnType::kInt64: column.dtype = "int64"; break;
    case DataFrameColumnType::kFloat64: column.dtype = "float64"; break;
    case DataFrameColumnType::kBool: column.dtype = "bool"; break;
    case DataFrameColumnType::kObject: column.dtype = "object"; break;
    case DataFrameColumnType::kDate: column.dtype = "datetime64[us, UTC]"; break;
    case DataFrameColumnType::kString:
      column.dtype = csv && (column.name == "concept:name" || column.name == "case:concept:name") ? "string" : "str";
      break;
  }
  return column;
}

// pandas' merge key check (_maybe_coerce_merge_keys) for the dtypes an
// upload's frame holds.
void check_merge_keys(const FrameColumn& left, const FrameColumn& right, const std::vector<std::optional<python::Scalar>>& left_cells,
                      const std::vector<std::optional<python::Scalar>>& right_cells) {
  using T = DataFrameColumnType;
  const auto lt = left.type.value_or(T::kString), rt = right.type.value_or(T::kString);
  if (left.dtype == right.dtype) return;
  const auto fail = [&] {
    throw std::invalid_argument("ValueError: You are trying to merge on " + left.dtype + " and " + right.dtype +
                                " columns for key '" + left.name + "'. If you wish to proceed you should use pd.concat");
  };
  const auto numeric = [](T t) { return t == T::kInt64 || t == T::kFloat64 || t == T::kBool; };
  const auto textual = [](T t) { return t == T::kString || t == T::kObject; };
  if (numeric(lt) && numeric(rt)) return;  // same kind, int and float, or cast to object
  if ((textual(lt) && rt == T::kBool) || (lt == T::kBool && textual(rt))) return;
  if ((textual(lt) && numeric(rt)) || (numeric(lt) && textual(rt))) {
    // lib.infer_dtype(skipna=False): a str column "string" (an object
    // column of strs too, "mixed" beside other values); numbers
    // "integer"/"floating" ("mixed-integer-float" never arises here).
    const auto inferred = [](T t, const std::vector<std::optional<python::Scalar>>& cells) -> std::string {
      if (t == T::kInt64) return "integer";
      if (t == T::kFloat64) return "floating";
      if (t == T::kBool) return "boolean";
      if (cells.empty()) return "empty";
      bool text = true, ints = true, bools = true;
      for (const auto& cell : cells) {
        const auto kind = cell ? cell->kind : python::Scalar::Kind::kFloat;
        const bool missing = !cell || python::is_missing(*cell);
        if (missing || kind != python::Scalar::Kind::kText) text = false;
        if (missing || kind != python::Scalar::Kind::kInt) ints = false;
        if (missing || kind != python::Scalar::Kind::kBool) bools = false;
      }
      if (text) return "string";
      if (ints) return "integer";
      if (bools) return "boolean";
      return "mixed";
    };
    static const std::set<std::string> bool_types = {"integer", "mixed-integer", "boolean", "empty"};
    static const std::set<std::string> string_types = {"string", "unicode", "mixed", "bytes", "empty"};
    const auto a = inferred(lt, left_cells), b = inferred(rt, right_cells);
    if (bool_types.contains(a) && bool_types.contains(b)) return;
    if (string_types.contains(a) != string_types.contains(b)) fail();
    return;
  }
  if ((lt == T::kDate) != (rt == T::kDate)) fail();
}

std::string merge_key(const std::optional<python::Scalar>& cell) {
  if (!cell || python::is_missing(*cell)) return "NA";
  return python::identity(*cell);
}

std::string cell_text(const std::optional<python::Scalar>& cell, const FrameColumn& column) {
  if (!cell || python::is_missing(*cell)) return column.type == DataFrameColumnType::kDate ? "NaT" : "nan";
  return python::str(*cell);
}

}  // namespace

LinkAnalysisResult discover_link_analysis(const EventLog& log,
                                          const LinkAnalysisOptions& options) {
  std::optional<EventLog> frame;
  if (log.dataframe_columns) {
    frame = dataframe_frame_event_log(log);
    if (frame->traces.size() != log.traces.size()) frame.reset();
  }
  if (!frame) {
    validate_column(log, options.out_column, "link_analysis_out_column");
    validate_column(log, options.in_column, "link_analysis_in_column");
    validate_column(log, options.sorting_column,
                    "link_analysis_sorting_column");
  }
  if (options.index_column.empty())
    throw std::invalid_argument("link_analysis_index_column must not be empty");
  if (is_reserved_index_field(options.index_column))
    throw std::invalid_argument(
        "link_analysis_index_column collides with a link endpoint field");
  if (options.max_links == 0)
    throw std::invalid_argument("link_analysis_max_links must be positive");

  std::vector<Row> rows;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      const auto& event = trace.events[event_index];
      Row row;
      row.trace = &trace;
      row.event = &event;
      row.trace_index = trace_index;
      row.event_index = event_index;
      row.input_index = rows.size();
      row.out_value = value_for(trace, event, event_index, options.out_column);
      row.in_value = value_for(trace, event, event_index, options.in_column);
      row.sorting_value =
          value_for(trace, event, event_index, options.sorting_column);
      row.out_key = row.out_value;
      row.in_key = row.in_value;
      rows.push_back(std::move(row));
    }
  }
  if (frame) {
    // The frame's rows in its row order (an XES upload's file order).
    std::vector<std::size_t> offsets(frame->traces.size() + 1, 0);
    for (std::size_t t = 0; t < frame->traces.size(); ++t) offsets[t + 1] = offsets[t] + frame->traces[t].events.size();
    std::vector<Row> ordered;
    ordered.reserve(rows.size());
    for (const auto& frame_row : frame_rows(*frame)) {
      const auto t = static_cast<std::size_t>(&frame_row.trace - frame->traces.data());
      const auto e = static_cast<std::size_t>(&frame_row.event - frame_row.trace.events.data());
      ordered.push_back(rows[offsets[t] + e]);
      ordered.back().input_index = ordered.size() - 1;
    }
    rows = std::move(ordered);
    const auto keys = frame_keys(*frame, FrameKeyScope::kDataFrame);
    const auto out = frame_column(*frame, keys, options.out_column);
    const auto in = frame_column(*frame, keys, options.in_column);
    const auto sorting = frame_column(*frame, keys, options.sorting_column);
    std::vector<std::optional<python::Scalar>> out_cells, in_cells;
    for (auto& row : rows) {
      const auto& trace = frame->traces[row.trace_index];
      const auto& event = trace.events[row.event_index];
      out_cells.push_back(frame_scalar(*frame, trace, event, out.name, FrameKeyScope::kDataFrame));
      in_cells.push_back(frame_scalar(*frame, trace, event, in.name, FrameKeyScope::kDataFrame));
      row.sorting_cell = frame_scalar(*frame, trace, event, sorting.name, FrameKeyScope::kDataFrame);
      if (row.sorting_cell && python::is_missing(*row.sorting_cell)) row.sorting_cell.reset();
    }
    // pandas sorts first, then merges. sort_values (nargsort) argsorts the
    // present cells with numpy's default quicksort, so equal cells come out
    // in its partition order (from 17 cells on), then the missing ones
    // follow in row order.
    std::vector<std::size_t> present, missing;
    for (std::size_t i = 0; i < rows.size(); ++i) (rows[i].sorting_cell ? present : missing).push_back(i);
    std::vector<std::size_t> order;
    try {
      order = detail::numpy_object_quick_argsort(present.size(), [&](std::size_t a, std::size_t b) {
        return python::compare(*rows[present[a]].sorting_cell, *rows[present[b]].sorting_cell);
      });
    } catch (const python::TypeError& error) {
      throw std::invalid_argument(std::string("TypeError: ") + error.what());
    }
    std::vector<Row> sorted;
    sorted.reserve(rows.size());
    for (const auto i : order) sorted.push_back(std::move(rows[present[i]]));
    for (const auto i : missing) sorted.push_back(std::move(rows[i]));
    rows = std::move(sorted);
    check_merge_keys(out, in, out_cells, in_cells);
    for (auto& row : rows) {
      const auto& cell_out = out_cells[row.input_index];
      const auto& cell_in = in_cells[row.input_index];
      row.out_key = merge_key(cell_out);
      row.in_key = merge_key(cell_in);
      row.out_value = cell_text(cell_out, out);
      row.in_value = cell_text(cell_in, in);
    }
  }

  const auto timestamp_sort = options.sorting_column == "timestamp" ||
                              options.sorting_column == "time:timestamp";
  if (!frame) std::stable_sort(rows.begin(), rows.end(), [&](const Row& left,
                                                const Row& right) {
    if (timestamp_sort) {
      const auto left_time = parse_timestamp(left.sorting_value);
      const auto right_time = parse_timestamp(right.sorting_value);
      if (left_time && right_time && *left_time != *right_time)
        return *left_time < *right_time;
      if (left_time.has_value() != right_time.has_value())
        return left_time.has_value();
    }
    if (left.sorting_value != right.sorting_value)
      return left.sorting_value < right.sorting_value;
    return left.input_index < right.input_index;
  });
  for (std::size_t index = 0; index < rows.size(); ++index)
    rows[index].index = index;

  std::map<std::string, std::vector<std::size_t>> targets;
  for (const auto& row : rows) targets[row.in_key].push_back(row.index);

  std::map<std::size_t, std::set<std::size_t>> associations;
  std::size_t association_count = 0;
  for (const auto& source : rows) {
    const auto target_it = targets.find(source.out_key);
    if (target_it == targets.end()) continue;
    auto& linked = associations[source.index];
    for (const auto target_index : target_it->second) {
      if (options.look_forward && target_index <= source.index) continue;
      if (linked.insert(target_index).second) {
        ++association_count;
        if (association_count > options.max_links)
          throw std::invalid_argument("link analysis exceeded max_links");
      }
      if (options.keep_first_occurrence) break;
    }
  }

  if (options.propagate) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto& [source, linked] : associations) {
        std::vector<std::size_t> additions;
        for (const auto target : linked) {
          const auto next = associations.find(target);
          if (next == associations.end()) continue;
          for (const auto candidate : next->second) {
            if (!linked.contains(candidate)) additions.push_back(candidate);
          }
        }
        for (const auto target : additions) {
          if (linked.insert(target).second) {
            changed = true;
            ++association_count;
            if (association_count > options.max_links)
              throw std::invalid_argument("link analysis exceeded max_links");
          }
        }
      }
    }
  }

  LinkAnalysisResult result;
  result.options = options;
  result.event_count = rows.size();
  for (const auto& [source_index, linked] : associations) {
    const auto& source = rows[source_index];
    for (const auto target_index : linked) {
      const auto& target = rows[target_index];
      if (result.links.size() >= options.max_links)
        throw std::invalid_argument("link analysis exceeded max_links");
      result.links.push_back({event_ref(source, source.out_value),
                              event_ref(target, target.in_value)});
    }
  }
  return result;
}

std::string link_analysis_json(const LinkAnalysisResult& result) {
  if (result.options.index_column.empty() || result.options.max_links == 0)
    throw std::invalid_argument("link analysis result has invalid options");
  if (result.links.size() > result.options.max_links)
    throw std::invalid_argument("link analysis result exceeds max_links");
  const auto valid_event = [&](const LinkAnalysisEvent& event) {
    return event.index < result.event_count && !event.case_id.empty() &&
           !event.activity.empty();
  };
  for (const auto& link : result.links) {
    if (!valid_event(link.source) || !valid_event(link.target))
      throw std::invalid_argument("link analysis result contains an invalid endpoint");
  }
  std::ostringstream out;
  out << "{\"family\":\"LINK_ANALYSIS\",\"variant\":\"CLASSIC\""
      << ",\"out_column\":";
  write_json_string(out, result.options.out_column);
  out << ",\"in_column\":";
  write_json_string(out, result.options.in_column);
  out << ",\"sorting_column\":";
  write_json_string(out, result.options.sorting_column);
  out << ",\"index_column\":";
  write_json_string(out, result.options.index_column);
  out << ",\"look_forward\":"
      << (result.options.look_forward ? "true" : "false")
      << ",\"keep_first_occurrence\":"
      << (result.options.keep_first_occurrence ? "true" : "false")
      << ",\"propagate\":" << (result.options.propagate ? "true" : "false")
      << ",\"event_count\":" << result.event_count << ",\"links\":[";
  for (std::size_t i = 0; i < result.links.size(); ++i) {
    if (i != 0) out << ',';
    out << "{\"source\":";
    write_event(out, result.links[i].source, result.options.index_column);
    out << ",\"target\":";
    write_event(out, result.links[i].target, result.options.index_column);
    out << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
