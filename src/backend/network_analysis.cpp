#include "minera/network_analysis.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <variant>

#include "frame_cells.hpp"
#include "frame_times.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "numpy_object_sort.hpp"
#include "python_scalar.hpp"
#include "reference_activity_selector.hpp"

namespace minera {
namespace {

constexpr double kWeekSeconds = 7.0 * 86400.0;

std::vector<BusinessHourSlot> normalized_slots(const NetworkAnalysisOptions& o) {
  auto slots = o.business_hour_slots;
  if (slots.empty()) {
    for (int day = 0; day < 5; ++day) {
      const double start = day * 86400.0 + 7.0 * 3600.0;
      slots.push_back({start, start + 10.0 * 3600.0});
    }
  }
  for (const auto& s : slots) {
    if (!std::isfinite(s.start_seconds) || !std::isfinite(s.end_seconds) ||
        s.start_seconds < 0.0 || s.end_seconds > kWeekSeconds ||
        s.end_seconds <= s.start_seconds)
      throw std::invalid_argument("network analysis: business-hour slots must satisfy 0 <= start < end <= 604800");
  }
  std::sort(slots.begin(), slots.end(), [](const auto& a, const auto& b) {
    return std::tie(a.start_seconds, a.end_seconds) < std::tie(b.start_seconds, b.end_seconds);
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& s : slots) {
    if (!merged.empty() && s.start_seconds <= merged.back().end_seconds)
      merged.back().end_seconds = std::max(merged.back().end_seconds, s.end_seconds);
    else merged.push_back(s);
  }
  return merged;
}

// First-class fields count only when present: an explicitly present empty
// value is a value, a missing or null one is not (pandas drops NaN keys).
const std::string* present(FirstClassAttributeState state, const std::string& text) {
  if (state == FirstClassAttributeState::kPresent ||
      (state == FirstClassAttributeState::kMissing && !text.empty()))
    return &text;
  return nullptr;
}

// The literal cell a selector names: the event attribute, then (for "case:"
// keys) the trace attribute. nullptr when there is no such literal cell.
const EventAttribute* literal_cell(const Event& e, const Trace* trace, std::string_view selector) {
  for (const auto& attribute : e.attributes)
    if (attribute.key == selector) return &attribute;
  if (trace && selector.rfind("case:", 0) == 0) {
    const auto key = selector.substr(5);
    for (const auto& attribute : trace->attributes)
      if (attribute.key == key) return &attribute;
    for (const auto& attribute : trace->attributes)
      if (attribute.key == selector) return &attribute;
  }
  return nullptr;
}

// Whether a selector names the case column. A flat adapter's case column
// keeps its header name and, as format_dataframe copies the chosen column to
// case:concept:name, is also that canonical column (as its activity, time and
// resource columns are concept:name, time:timestamp and org:resource). An
// XES trace attribute (a trace with a selected case cell) is a dataframe
// column only under its "case:" prefix, so its bare key is not a column (the
// bare "concept:name" is the activity).
bool case_selector(const Event& e, const Trace* trace, std::string_view selector) {
  if (e.case_id_source_key.empty())
    return selector == "case:concept:name" || selector == "case:id" || selector == "case_id";
  if (selector == "case:" + e.case_id_source_key) return true;
  const bool flat = !(trace && trace->case_id_attribute);
  return flat && (selector == e.case_id_source_key || selector == "case:concept:name");
}

// The first-class projection a selector names on this event, with its
// presence: a projection answers only for the field it was read from (J1).
const std::string* projection(const Event& e, const Trace* trace, std::string_view selector) {
  using detail::projection_names;
  if (projection_names(e.activity_source_key, e.activity_attribute, "concept:name", selector))
    return present(e.activity_state, e.activity);
  if (projection_names(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", selector))
    return present(e.timestamp_state, e.timestamp);
  if (projection_names(e.resource_source_key, e.resource_attribute, "org:resource", selector))
    return present(e.resource_state, e.resource);
  if (projection_names(e.lifecycle_source_key, e.lifecycle_attribute, "lifecycle:transition", selector))
    return present(e.lifecycle_state, e.lifecycle);
  if (case_selector(e, trace, selector)) return present(e.case_id_state, e.case_id);
  return nullptr;
}

// Whether the event's dataframe row has the named column at all (a present
// null still is a column).
bool has_column(const Event& e, const Trace* trace, std::string_view selector) {
  if (literal_cell(e, trace, selector)) return true;
  using detail::projection_names;
  const auto owns = [&](const std::string& source, const std::optional<EventAttribute>& cell,
                        std::string_view canonical, FirstClassAttributeState state, const std::string& text) {
    return projection_names(source, cell, canonical, selector) &&
           (state != FirstClassAttributeState::kMissing || !text.empty());
  };
  if (owns(e.activity_source_key, e.activity_attribute, "concept:name", e.activity_state, e.activity)) return true;
  if (owns(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", e.timestamp_state, e.timestamp)) return true;
  if (owns(e.resource_source_key, e.resource_attribute, "org:resource", e.resource_state, e.resource)) return true;
  if (owns(e.lifecycle_source_key, e.lifecycle_attribute, "lifecycle:transition", e.lifecycle_state, e.lifecycle)) return true;
  if (case_selector(e, trace, selector) && (e.case_id_state != FirstClassAttributeState::kMissing || !e.case_id.empty() ||
                                     (trace && trace->has_case_id())))
    return true;
  return false;
}

// The typed cell an adapter kept for the projection a selector names (the
// XES value an int, float or boolean concept:name was read from), if any.
const EventAttribute* projection_cell(const Event& e, const Trace* trace, std::string_view selector) {
  using detail::projection_names;
  const auto kept = [](const std::optional<EventAttribute>& cell) { return cell ? &*cell : nullptr; };
  if (projection_names(e.activity_source_key, e.activity_attribute, "concept:name", selector)) return kept(e.activity_attribute);
  if (projection_names(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", selector)) return kept(e.timestamp_attribute);
  if (projection_names(e.resource_source_key, e.resource_attribute, "org:resource", selector)) return kept(e.resource_attribute);
  if (projection_names(e.lifecycle_source_key, e.lifecycle_attribute, "lifecycle:transition", selector)) return kept(e.lifecycle_attribute);
  if (case_selector(e, trace, selector) && trace) return kept(trace->case_id_attribute);
  return nullptr;
}

// The Python object the selector names on this event: a literal cell as it
// is typed, else the projection's typed cell, else its text (a timestamp
// projection is a Timestamp). An empty selector is a column name like any
// other. nullopt when the value is missing (None, NaN, pd.NA, absent).
std::optional<python::Scalar> native_typed_value(const Event& e, const Trace* trace, std::string_view selector) {
  if (const auto* cell = literal_cell(e, trace, selector)) {
    auto scalar = python::from_attribute(*cell);
    if (python::is_missing(scalar)) return std::nullopt;
    return scalar;
  }
  const auto* text = projection(e, trace, selector);
  if (!text) return std::nullopt;
  if (const auto* cell = projection_cell(e, trace, selector)) {
    auto scalar = python::from_attribute(*cell);
    if (python::is_missing(scalar)) return std::nullopt;
    if (scalar.kind != python::Scalar::Kind::kText) return scalar;
  }
  if (detail::projection_names(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", selector))
    if (auto date = python::date_literal(*text)) return date;
  return python::text(*text);
}

// Reads a selector's typed cell: on the DataFrame path (CSV and XES uploads)
// the frame's column of that name (frame_cells.hpp), else the native
// selector rules above.
struct Reader {
  const EventLog* frame = nullptr;
  std::optional<python::Scalar> typed(const Event& e, const Trace* trace, std::string_view selector) const {
    if (frame) return frame_scalar(*frame, *trace, e, std::string(selector), FrameKeyScope::kDataFrame);
    return native_typed_value(e, trace, selector);
  }
};

// The selected value as timestamp text: a Timestamp keeps the spelling it
// was read from.
std::optional<std::string> value(const Reader& reader, const Event& e, const Trace* trace, std::string_view selector) {
  const auto typed = reader.typed(e, trace, selector);
  if (!typed) return std::nullopt;
  if (typed->kind == python::Scalar::Kind::kDate) return typed->text;
  return python::str(*typed);
}

// Sort keys compare like the pandas column they come from: numbers (bool,
// int, uint, float) by exact value, datetimes by exact instant, strings
// lexically (a date-looking string is still a string), and missing values
// (None, NaN, pd.NA, an absent column value) last. Numbers and strings in one
// column cannot be ordered (Python raises TypeError).
struct SortKey {
  enum class Kind { kMissing, kNumber, kDate, kText } kind = Kind::kMissing;
  python::Scalar value;
};

SortKey sort_key(const Reader& reader, const Event& e, const Trace* trace, std::string_view selector) {
  SortKey key;
  auto typed = reader.typed(e, trace, selector);
  if (!typed) return key;
  if (python::is_number(*typed)) key.kind = SortKey::Kind::kNumber;
  else if (typed->kind == python::Scalar::Kind::kDate) key.kind = SortKey::Kind::kDate;
  else key.kind = SortKey::Kind::kText;
  key.value = std::move(*typed);
  return key;
}

std::optional<double> instant(const Reader& reader, const Event& e, const Trace* trace,
                              std::string_view selector) {
  const auto v = value(reader, e, trace, selector);
  if (!v) return std::nullopt;
  return parse_timestamp(*v);
}

std::optional<double> local_wall(std::string_view text) {
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
  const auto tz = text.substr(0, end).find_first_of("Zz+-", 10);
  if (tz != std::string_view::npos) end = tz;
  return parse_timestamp(text.substr(0, end));
}

double active(double left, double right, const std::vector<BusinessHourSlot>& slots) {
  double total = 0.0;
  for (const auto& s : slots) total += std::max(0.0, std::min(right, s.end_seconds) - std::max(left, s.start_seconds));
  return total;
}

double business_elapsed(double source, double target, const std::vector<BusinessHourSlot>& slots) {
  if (target <= source) return 0.0;
  constexpr double shift = 3.0 * 86400.0;
  const double a = source + shift, b = target + shift;
  const auto wa = static_cast<long long>(std::floor(a / kWeekSeconds));
  const auto wb = static_cast<long long>(std::floor(b / kWeekSeconds));
  const double oa = a - wa * kWeekSeconds, ob = b - wb * kWeekSeconds;
  double full = 0.0;
  for (const auto& s : slots) full += s.end_seconds - s.start_seconds;
  if (wa == wb) return active(oa, ob, slots);
  double total = active(oa, kWeekSeconds, slots) + active(0.0, ob, slots);
  if (wb - wa > 1) total += static_cast<double>(wb - wa - 1) * full;
  return total;
}

struct Ref {
  const Event* event;
  const Trace* trace;
  std::size_t index;
  SortKey sort;
};

// The relation value pandas merges on: equal keys of any numeric type join
// (1, 1.0 and True), a number and its text do not, and null-like keys (None,
// NaN, pd.NA or an absent value) join each other, unlike in grouping.
std::string relation_key(const Reader& reader, const Event& e, const Trace* trace, std::string_view selector) {
  if (const auto v = reader.typed(e, trace, selector)) return python::identity(*v);
  return std::string(1, '\0');
}

// df[[columns]] on the selected columns: every one must exist in some row
// (on the DataFrame path: be a column of the frame, `frame_keys`).
void require_columns(const EventLog& log, const NetworkAnalysisOptions& o,
                     const std::set<std::string>* frame_columns = nullptr) {
  if (log.event_count() == 0) return;
  const std::vector<std::string> selected{o.out_relation_attribute, o.in_relation_attribute,
                                          o.source_node_attribute, o.target_node_attribute,
                                          o.edge_attribute, o.sorting_attribute, o.timestamp_attribute};
  std::vector<std::string> distinct, absent;
  for (const auto& name : selected) {
    if (std::find(distinct.begin(), distinct.end(), name) != distinct.end()) continue;
    distinct.push_back(name);
    bool found = frame_columns && frame_columns->contains(name);
    for (const auto& tr : log.traces) {
      if (found || frame_columns) break;
      for (const auto& e : tr.events)
        if (has_column(e, &tr, name)) { found = true; break; }
    }
    if (!found) absent.push_back(name);
  }
  if (absent.empty()) return;
  const auto list = [](const std::vector<std::string>& names) {
    std::string out = "[";
    for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", '" : "'") + names[i] + "'";
    return out + "]";
  };
  // The reference lists the absent names in set order; native lists them in
  // selector order.
  if (absent.size() == distinct.size())
    throw std::invalid_argument("network analysis: KeyError: \"None of [Index(" + list(absent) +
                                ", dtype='str')] are in the [columns]\"");
  throw std::invalid_argument("network analysis: KeyError: \"" + list(absent) + " not in index\"");
}

// A frame column's pandas dtype as merge sees it: its name, its kind, and
// for object columns lib.infer_dtype(skipna=False) of its cells.
struct FrameDtype {
  enum class Kind { kNumber, kBool, kText, kObject, kDate } kind = Kind::kText;
  std::string name;
  std::string inferred;
};

FrameDtype frame_dtype(const EventLog& frame, const std::string& key) {
  const auto& columns = *frame.dataframe_columns;
  const bool csv = columns.csv_text;
  const auto of_type = [&](DataFrameColumnType type) {
    switch (type) {
      case DataFrameColumnType::kInt64: return FrameDtype{FrameDtype::Kind::kNumber, "int64", "integer"};
      case DataFrameColumnType::kFloat64: return FrameDtype{FrameDtype::Kind::kNumber, "float64", "floating"};
      case DataFrameColumnType::kBool: return FrameDtype{FrameDtype::Kind::kBool, "bool", "boolean"};
      case DataFrameColumnType::kString: return FrameDtype{FrameDtype::Kind::kText, "str", "string"};
      case DataFrameColumnType::kDate: return FrameDtype{FrameDtype::Kind::kDate, "datetime64[us, UTC]", "datetime64"};
      case DataFrameColumnType::kObject: return FrameDtype{FrameDtype::Kind::kObject, "object", ""};
    }
    return FrameDtype{};
  };
  FrameDtype dtype;
  bool found = false;
  if (key == "concept:name" || key == "case:concept:name") {
    dtype = {FrameDtype::Kind::kText, csv ? "string" : "str", "string"};
    found = true;
  } else if (key == "@@index" || key == "@@case_index") {
    dtype = of_type(DataFrameColumnType::kInt64);
    found = true;
  } else if (key == "time:timestamp" && !csv) {
    dtype = of_type(DataFrameColumnType::kDate);
    found = true;
  }
  for (const auto& column : columns.columns)
    if (!found && column.key == key) {
      dtype = of_type(column.type);
      found = true;
    }
  if (!found && key.starts_with("case:"))
    for (const auto& column : columns.trace_columns)
      if (!found && "case:" + column.key == key) {
        dtype = of_type(column.type);
        found = true;
      }
  if (!found && csv) dtype = of_type(columns.activity_type);  // the activity under its own header
  else if (!found) dtype = {FrameDtype::Kind::kText, "str", "string"};
  // read_csv keeps a bool column with a missing cell as objects.
  if (dtype.kind == FrameDtype::Kind::kBool)
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events)
        if (!frame_scalar(frame, trace, event, key, FrameKeyScope::kDataFrame)) dtype = of_type(DataFrameColumnType::kObject);
  if (dtype.kind != FrameDtype::Kind::kObject) return dtype;
  bool ints = false, floats = false, other = false, bools = false, texts = false, missing = false;
  std::size_t rows = 0;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      ++rows;
      const auto value = frame_scalar(frame, trace, event, key, FrameKeyScope::kDataFrame);
      if (!value) missing = true;
      else if (value->kind == python::Scalar::Kind::kInt) ints = true;
      else if (value->kind == python::Scalar::Kind::kFloat) floats = true;
      else if (value->kind == python::Scalar::Kind::kBool) bools = true;
      else if (value->kind == python::Scalar::Kind::kText) texts = true;
      else other = true;
    }
  if (rows == 0) dtype.inferred = "empty";
  else if (ints) dtype.inferred = (bools || texts || other) ? "mixed-integer"
                                  : floats ? "mixed-integer-float"
                                  : missing ? "integer-na" : "integer";
  else if (bools && !floats && !texts && !other && !missing) dtype.inferred = "boolean";
  else if (texts && !floats && !bools && !other && !missing) dtype.inferred = "string";
  else if (!bools && !texts && !other) dtype.inferred = "floating";
  else dtype.inferred = "mixed";
  return dtype;
}

// pandas' merge check of the relation columns (left the out column, right
// the in column; observed black-box): datetimes merge only with datetimes,
// numbers with numbers and bools, bools with anything else but datetimes,
// text and objects with each other, and a number with text or an object
// only when neither infers as string-like.
void require_mergeable(const EventLog& frame, const std::string& out, const std::string& in) {
  const auto left = frame_dtype(frame, out);
  const auto right = frame_dtype(frame, in);
  using Kind = FrameDtype::Kind;
  const auto raise = [&] {
    throw std::invalid_argument("network analysis: ValueError: You are trying to merge on " + left.name +
                                " and " + right.name + " columns for key '" + out +
                                "'. If you wish to proceed you should use pd.concat");
  };
  if (left.name == right.name) return;
  const bool left_number = left.kind == Kind::kNumber || left.kind == Kind::kBool;
  const bool right_number = right.kind == Kind::kNumber || right.kind == Kind::kBool;
  const bool left_object = left.kind == Kind::kText || left.kind == Kind::kObject;
  const bool right_object = right.kind == Kind::kText || right.kind == Kind::kObject;
  if (left_number && right_number) return;
  if ((left_object && right.kind == Kind::kBool) || (left.kind == Kind::kBool && right_object)) return;
  if ((left_object && right_number) || (left_number && right_object)) {
    const auto in_list = [](const std::string& inferred, std::initializer_list<const char*> list) {
      for (const auto* item : list)
        if (inferred == item) return true;
      return false;
    };
    const auto boolish = [&](const std::string& t) { return in_list(t, {"integer", "mixed-integer", "boolean", "empty"}); };
    const auto stringish = [&](const std::string& t) { return in_list(t, {"string", "unicode", "mixed", "bytes", "empty"}); };
    if (boolish(left.inferred) && boolish(right.inferred)) return;
    if (stringish(left.inferred) != stringish(right.inferred)) raise();
    return;
  }
  if ((left.kind == Kind::kDate) != (right.kind == Kind::kDate)) raise();
}

// The durations pandas takes from the joined frame's timestamp columns
// (target minus source) whatever the performance flag, observed black-box:
// a bool column cannot be subtracted; a text or object column subtracts cell
// by cell where both cells are present, so the first such pair Python cannot
// subtract is the error; and a result of no dates has no .dt accessor.
void require_timestamp_column(const EventLog& frame, const Reader& reader, const std::string& key,
                              const std::vector<Ref>& refs, const std::vector<const Ref*>& successors) {
  const auto dtype = frame_dtype(frame, key);
  if (dtype.kind == FrameDtype::Kind::kDate) return;
  if (dtype.kind == FrameDtype::Kind::kBool)
    throw std::invalid_argument(
        "network analysis: TypeError: numpy boolean subtract, the `-` operator, is not supported, use the "
        "bitwise_xor, the `^` operator, or the logical_xor function instead.");
  if (dtype.kind == FrameDtype::Kind::kText || dtype.kind == FrameDtype::Kind::kObject)
    for (std::size_t i = 0; i < refs.size(); ++i) {
      if (!successors[i]) continue;
      const auto source = reader.typed(*refs[i].event, refs[i].trace, key);
      const auto target = reader.typed(*successors[i]->event, successors[i]->trace, key);
      if (!source || !target || python::is_missing(*source) || python::is_missing(*target)) continue;
      using Kind = python::Scalar::Kind;
      if (python::is_number(*target) && python::is_number(*source)) continue;
      if (target->kind == Kind::kDate && source->kind == Kind::kDate) continue;
      if (target->kind == Kind::kDate && source->kind == Kind::kInt)
        throw std::invalid_argument(
            "network analysis: TypeError: Addition/subtraction of integers and integer-arrays with Timestamp is "
            "no longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`");
      throw std::invalid_argument("network analysis: TypeError: unsupported operand type(s) for -: '" +
                                  python::type_name(*target) + "' and '" + python::type_name(*source) + "'");
    }
  throw std::invalid_argument("network analysis: AttributeError: Can only use .dt accessor with datetimelike values");
}

}  // namespace

NetworkAnalysisResult discover_network_analysis(const EventLog& input, const NetworkAnalysisOptions& o) {
  if (o.variant != NetworkAnalysisVariant::kDataframe)
    throw std::invalid_argument("network_analysis_variant must be dataframe");
  if (o.edge_reference != NetworkAnalysisEdgeReference::kOut &&
      o.edge_reference != NetworkAnalysisEdgeReference::kIn)
    throw std::invalid_argument("network analysis edge_reference must be _out or _in");
  if (o.max_rows == 0 || o.max_rows > 1000000) throw std::invalid_argument("network analysis max_rows safety bound exceeded");
  if (o.max_values == 0 || o.max_values > 10000000) throw std::invalid_argument("network analysis max_values safety bound exceeded");
  // A log read on the DataFrame path (CSV and XES uploads) is read as pm4py
  // reads its frame: selectors are column names (no org:resource alias),
  // cells are typed (dates in UTC), and pandas' merge and duration rules
  // apply to the columns' dtypes.
  std::optional<EventLog> frame_storage;
  std::set<std::string> frame_columns;
  Reader reader;
  if (input.dataframe_columns) {
    frame_storage = dataframe_frame_event_log(input);
    reader.frame = &*frame_storage;
    frame_columns = frame_keys(*frame_storage, FrameKeyScope::kDataFrame);
  }
  const EventLog& log = reader.frame ? *reader.frame : input;
  require_columns(log, o, reader.frame ? &frame_columns : nullptr);
  if (reader.frame && log.event_count() != 0) {
    // format_dataframe's @@index is a second column of that name beside
    // the one the merge adds.
    for (const auto* relation : {&o.out_relation_attribute, &o.in_relation_attribute})
      if (*relation == "@@index" && log.dataframe_columns->csv_text)
        throw std::invalid_argument("network analysis: ValueError: The column label '@@index' is not unique.");
    require_mergeable(log, o.out_relation_attribute, o.in_relation_attribute);
  }
  const auto slots = o.business_hours ? normalized_slots(o) : std::vector<BusinessHourSlot>{};
  NetworkAnalysisResult result;
  result.variant = o.variant;
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();
  std::vector<Ref> refs;
  refs.reserve(log.event_count());
  std::size_t position = 0;
  for (const auto& tr : log.traces) for (const auto& e : tr.events)
    refs.push_back({&e, &tr, position++, sort_key(reader, e, &tr, o.sorting_attribute)});
  // sort_values on the object column (pandas' nargsort): numpy's default
  // quicksort argsorts the non-missing values, so equal keys come out in its
  // partition order (from 17 values on, not input order), then the missing
  // ones follow in input order. The first pair it cannot compare is the
  // error.
  std::vector<std::size_t> present, missing;
  for (std::size_t i = 0; i < refs.size(); ++i)
    (refs[i].sort.kind == SortKey::Kind::kMissing ? missing : present).push_back(i);
  std::vector<std::size_t> order;
  try {
    order = detail::numpy_object_quick_argsort(present.size(), [&](std::size_t a, std::size_t b) {
      return python::compare(refs[present[a]].sort.value, refs[present[b]].sort.value);
    });
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("network analysis: TypeError: ") + error.what());
  }
  std::vector<Ref> sorted;
  sorted.reserve(refs.size());
  for (const auto i : order) sorted.push_back(std::move(refs[present[i]]));
  for (const auto i : missing) sorted.push_back(std::move(refs[i]));
  refs = std::move(sorted);
  for (std::size_t i = 0; i < refs.size(); ++i) refs[i].index = i;
  std::vector<const Ref*> successors(refs.size(), nullptr);
  std::map<std::string, const Ref*> next_by_relation;
  for (std::size_t i = refs.size(); i-- > 0;) {
    const auto out = relation_key(reader, *refs[i].event, refs[i].trace, o.out_relation_attribute);
    const auto target = next_by_relation.find(out);
    if (target != next_by_relation.end()) successors[i] = target->second;
    next_by_relation[relation_key(reader, *refs[i].event, refs[i].trace, o.in_relation_attribute)] = &refs[i];
  }
  // Each link is a row of the joined frame; the edges are its groupby over
  // (source, target, edge), with pandas' typed key identity and labels.
  struct Link {
    std::optional<double> duration;
  };
  std::vector<std::vector<python::Scalar>> keys;
  std::vector<Link> links;
  // The analysis numbers the sorted rows in its own @@index column, which on
  // a CSV upload replaces format_dataframe's (observed black-box).
  const bool own_index = reader.frame && log.dataframe_columns->csv_text;
  const auto typed = [&](const Ref& ref, const std::string& selector) -> std::optional<python::Scalar> {
    if (own_index && selector == "@@index") return python::integer(static_cast<std::int64_t>(ref.index));
    return reader.typed(*ref.event, ref.trace, selector);
  };
  for (std::size_t source_index = 0; source_index < refs.size(); ++source_index) {
    const auto& source = refs[source_index];
    const auto source_node = typed(source, o.source_node_attribute);
    if (!source_node) continue;
    const Ref* target = successors[source_index];
    if (!target) continue;
    const auto target_node = typed(*target, o.target_node_attribute);
    const auto& edge_event = o.edge_reference == NetworkAnalysisEdgeReference::kOut ? source : *target;
    const auto edge_value = typed(edge_event, o.edge_attribute);
    if (!target_node || !edge_value) continue;
    keys.push_back({*source_node, *target_node, *edge_value});
    Link link;
    ++result.diagnostics.link_count;
    if (o.include_performance) {
      const auto st = instant(reader, *source.event, source.trace, o.timestamp_attribute);
      const auto tt = instant(reader, *target->event, target->trace, o.timestamp_attribute);
      if (st && tt) {
        double duration = o.business_hours
            ? business_elapsed(local_wall(value(reader, *source.event, source.trace, o.timestamp_attribute).value_or("")).value_or(*st), local_wall(value(reader, *target->event, target->trace, o.timestamp_attribute).value_or("")).value_or(*tt), slots)
            : (*tt - *st);
        // The frame's dates subtract exactly, as Timedelta.total_seconds.
        if (reader.frame && !o.business_hours) {
          const auto from = reader.typed(*source.event, source.trace, o.timestamp_attribute);
          const auto to = reader.typed(*target->event, target->trace, o.timestamp_attribute);
          if (from && to && from->kind == python::Scalar::Kind::kDate && to->kind == python::Scalar::Kind::kDate)
            duration = frame_times::total_seconds(from->date, to->date);
        }
        if (std::isfinite(duration) && result.diagnostics.performance_value_count < o.max_values) {
          link.duration = duration;
          ++result.diagnostics.performance_value_count;
        } else if (result.diagnostics.performance_value_count >= o.max_values) result.diagnostics.output_truncated = true;
      }
    }
    links.push_back(link);
  }
  if (reader.frame && log.event_count() != 0)
    require_timestamp_column(log, reader, o.timestamp_attribute, refs, successors);
  std::vector<python::Group> groups;
  try {
    groups = python::groupby(keys);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("network analysis: TypeError: ") + error.what());
  }
  for (const auto& group : groups) {
    if (result.edges.size() >= o.max_rows) { result.diagnostics.output_truncated = true; break; }
    NetworkAnalysisEdge edge{python::str(group.labels[0]), python::str(group.labels[1]), python::str(group.labels[2]), 0, {}};
    for (const auto row : group.rows) {
      ++edge.count;
      if (links[row].duration) edge.performance.push_back(*links[row].duration);
    }
    result.edges.push_back(std::move(edge));
  }
  result.diagnostics.row_count = result.edges.size();
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated ? "output safety bound applied" : (result.edges.empty() ? "no links" : "ok");
  return result;
}

}  // namespace minera
