#pragma once

// Round DW: list case keys as pandas reads them in the pandas filters
// (probes dw/l1, dw/l2, observed black-box). Three readings recur:
//
// - df[key] (a frame of the listed columns): the key becomes an array first
//   (np.asarray), so a regular 2-D key's rows turn into tuples, a deeper
//   regular key is refused ("Index data must be 1-dimensional") and a ragged
//   key stays a list of its items; a list of bools is a row mask; the labels
//   must hash, and the ones naming no column fail as one KeyError.
// - groupby(key): a key of labels the length of the frame that names some
//   label no column has is one array of those labels; otherwise each item in
//   turn is a dict (mapping the index labels), None or a set (called on each
//   index label), a list (an array, as long as the frame, its rows turned to
//   tuples as for df[key]) or a label naming a column.
// - set_index(key): list items are arrays (kept as they are), other items
//   labels naming columns.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "pandas_numpy_view.hpp"
#include "pandas_parameter_carrier.hpp"
#include "python_scalar.hpp"

namespace minera::detail {

inline bool pandas_list_sequence(const EventAttribute& value) {
  return !value.is_null && (value.container == AttributeContainerKind::kList ||
                            value.container == AttributeContainerKind::kTuple);
}

// np.asarray(list) as asarray_tuplesafe takes it: the dimensions numpy finds
// (every element of a level a list or tuple of one length) or 0 when the
// levels are uneven (numpy refuses, so the items stay a 1-D object array).
inline int pandas_list_ndim(const EventAttribute& list) {
  std::vector<const EventAttribute*> level;
  for (const auto& child : list.children) level.push_back(&child);
  int ndim = 1;
  while (!level.empty()) {
    std::size_t sequences = 0;
    for (const auto* item : level) sequences += pandas_list_sequence(*item) ? 1 : 0;
    if (sequences == 0) return ndim;
    if (sequences != level.size()) return 0;
    const auto width = level.front()->children.size();
    std::vector<const EventAttribute*> next;
    for (const auto* item : level) {
      if (item->children.size() != width) return 0;
      for (const auto& child : item->children) next.push_back(&child);
    }
    ++ndim;
    level = std::move(next);
  }
  return ndim;
}

inline EventAttribute pandas_list_tuple(const EventAttribute& sequence) {
  EventAttribute result{"", ""};
  result.container = AttributeContainerKind::kTuple;
  result.children = sequence.children;
  for (auto& child : result.children) child.key.clear();
  return result;
}

inline bool pandas_list_bool(const EventAttribute& value) {
  return !value.is_null && value.container == AttributeContainerKind::kScalar &&
         (value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value));
}

inline bool pandas_list_numeric_leaves(const EventAttribute& value);

// The items of np.asarray(list) for an Index: a 2-D array's rows are tuples;
// a deeper array cannot be one (a numeric array says so, a str or object
// array fails on its buffer).
template <typename Fail>
std::vector<EventAttribute> pandas_list_index_items(const EventAttribute& list, Fail&& fail) {
  const int ndim = pandas_list_ndim(list);
  if (ndim >= 3) {
    if (pandas_list_numeric_leaves(list)) fail("ValueError", "Index data must be 1-dimensional");
    fail("ValueError", "Buffer has wrong number of dimensions (expected 1, got " + std::to_string(ndim) + ")");
  }
  std::vector<EventAttribute> items;
  for (const auto& child : list.children) items.push_back(ndim == 2 ? pandas_list_tuple(child) : child);
  return items;
}

inline bool pandas_list_text(const EventAttribute& value) {
  return !value.is_null && value.container == AttributeContainerKind::kScalar &&
         (value.kind == AttributeKind::kString || value.kind == AttributeKind::kId) &&
         std::holds_alternative<std::monostate>(value.typed_value);
}

inline bool pandas_list_float(const EventAttribute& value) {
  return !value.is_null && value.container == AttributeContainerKind::kScalar &&
         (value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value)) &&
         !std::holds_alternative<NativeDate>(value.typed_value) && value.kind != AttributeKind::kDate;
}

inline bool pandas_list_int(const EventAttribute& value) {
  return !value.is_null && value.container == AttributeContainerKind::kScalar && !pandas_list_bool(value) &&
         (value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) ||
          std::holds_alternative<std::uint64_t>(value.typed_value));
}

inline double pandas_list_number(const EventAttribute& value) {
  if (const auto* d = std::get_if<double>(&value.typed_value)) return *d;
  if (const auto* i = std::get_if<std::int64_t>(&value.typed_value)) return static_cast<double>(*i);
  if (const auto* u = std::get_if<std::uint64_t>(&value.typed_value)) return static_cast<double>(*u);
  if (const auto* b = std::get_if<bool>(&value.typed_value)) return *b ? 1.0 : 0.0;
  if (pandas_list_bool(value)) return value.value == "true" || value.value == "True" ? 1.0 : 0.0;
  try { return std::stod(value.value); } catch (...) { return std::nan(""); }
}

inline bool pandas_list_nan(const EventAttribute& value) {
  return pandas_list_float(value) && std::isnan(pandas_list_number(value));
}

// Every leaf of a nested list a bool, int or float (NumPy then builds a
// numeric array; a str, None or any other object makes it a str or object
// array).
inline bool pandas_list_numeric_leaves(const EventAttribute& value) {
  if (pandas_list_sequence(value)) {
    for (const auto& child : value.children)
      if (!pandas_list_numeric_leaves(child)) return false;
    return true;
  }
  return pandas_list_bool(value) || pandas_list_int(value) || pandas_list_float(value);
}

// pandas' repr of an Index of `items` (each already formatted): one line
// when it fits the 80-column display, else wrapped after "Index([" with the
// items right-justified to one width (not for str items), the middle of more
// than 100 items cut to "..." with the length named (round DX, modeled and
// fuzzed against pandas: 12,000 random indexes, review-harness round-dx ir/).
inline std::size_t pandas_display_length(const std::string& text) {
  std::size_t n = 0;
  for (const unsigned char c : text) n += (c & 0xC0) != 0x80;
  return n;
}
inline std::string pandas_index_repr(const std::vector<std::string>& items, const std::string& dtype, bool justify) {
  constexpr std::size_t width = 80, max_seq = 100;
  const std::string space1 = "\n      ", space2 = "\n       ";
  const auto n = items.size();
  std::string summary;
  if (n == 0) summary = "[], ";
  else if (n == 1) summary = "[" + items[0] + "], ";
  else if (n == 2) summary = "[" + items[0] + ", " + items[1] + "], ";
  else {
    const bool truncated = n > max_seq;
    std::vector<std::string> head, tail;
    if (truncated) {
      const std::size_t h = std::min<std::size_t>(max_seq / 2, 10);
      head.assign(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(h));
      tail.assign(items.end() - static_cast<std::ptrdiff_t>(h), items.end());
    } else {
      tail = items;
    }
    const auto joined = [](const std::vector<std::string>& values) {
      std::size_t total = 0;
      for (std::size_t i = 0; i < values.size(); ++i) total += pandas_display_length(values[i]) + (i ? 2 : 0);
      return total;
    };
    if (justify && (truncated || !(joined(head) < width && joined(tail) < width))) {
      std::size_t longest = 0;
      for (const auto* part : {&head, &tail})
        for (const auto& x : *part) longest = std::max(longest, pandas_display_length(x));
      for (auto* part : {&head, &tail})
        for (auto& x : *part) x.insert(0, longest - std::min(longest, pandas_display_length(x)), ' ');
    }
    const auto rstrip = [](std::string text) {
      while (!text.empty() && (text.back() == ' ' || text.back() == '\n')) text.pop_back();
      return text;
    };
    std::string out, line = space2;
    const auto extend = [&](const std::string& value, std::size_t limit) {
      if (pandas_display_length(rstrip(line)) + pandas_display_length(rstrip(value)) >= limit) {
        out += rstrip(line);
        line = space2;
      }
      line += value;
    };
    for (const auto& value : head) extend(value + ", ", width);
    if (truncated) {
      out += rstrip(line) + space2 + "...";
      line = space2;
    }
    for (std::size_t i = 0; i + 1 < tail.size(); ++i) extend(tail[i] + ", ", width);
    extend(tail.back(), width - 2);
    out += line + "],";
    out += pandas_display_length(out) > width ? space1 : std::string(" ");
    summary = "[" + out.substr(space2.size());
  }
  return "Index(" + summary + "dtype='" + dtype + "'" + (n > max_seq ? ", length=" + std::to_string(n) : std::string()) + ")";
}

// df[key] for a list key holding at least one item: the positions of the
// listed columns (duplicates kept), or pandas' error. `rows` is the frame's
// length (a list of bools is a row mask), `repr` Python's repr of one label,
// `quote` Python's repr of a str (a KeyError prints its message so).
template <typename Repr, typename Quote, typename Fail>
std::vector<std::size_t> pandas_getitem_list(const std::vector<std::string>& columns, const EventAttribute& key,
                                             std::size_t rows, Repr&& repr, Quote&& quote, Fail&& fail) {
  if (!key.children.empty() && std::all_of(key.children.begin(), key.children.end(), pandas_list_bool)) {
    if (key.children.size() != rows)
      fail("ValueError", "Item wrong length " + std::to_string(key.children.size()) + " instead of " + std::to_string(rows) + ".");
    fail("ValueError", "row masks as list case keys are outside the captured native domain");
  }
  const auto labels = pandas_list_index_items(key, fail);
  for (const auto& label : labels) pandas_check_hashable_key(label, fail);
  // The Index's dtype: str when every label is a str or missing (one str at
  // least), int64 for ints (and bools beside them), float64 for numbers with
  // a float, object otherwise.
  bool strings = true, any_string = false, integers = true, numbers = true, any_float = false;
  for (const auto& label : labels) {
    const bool text = pandas_list_text(label), missing = label.is_null || pandas_list_nan(label);
    const bool scalar = label.container == AttributeContainerKind::kScalar && !label.is_null &&
                        !std::holds_alternative<NativeDate>(label.typed_value) && label.kind != AttributeKind::kDate;
    any_string |= text;
    strings &= text || (missing && label.container == AttributeContainerKind::kScalar);
    const bool number = scalar && (pandas_list_int(label) || pandas_list_bool(label) || pandas_list_float(label));
    numbers &= number;
    integers &= scalar && (pandas_list_int(label) || pandas_list_bool(label));
    any_float |= scalar && pandas_list_float(label);
  }
  std::string dtype = "object";
  if (strings && any_string) dtype = "str";
  else if (numbers && !labels.empty() && !any_float && integers) dtype = "int64";
  else if (numbers && !labels.empty()) dtype = "float64";
  const auto shown = [&](const EventAttribute& label) {
    if (dtype == "str" && !pandas_list_text(label)) return std::string("nan");
    if (dtype == "int64") return std::to_string(static_cast<long long>(pandas_list_number(label)));
    if (dtype == "float64") return python::float_repr(pandas_list_number(label));
    return std::string(repr(label));
  };
  std::vector<std::size_t> found;
  std::vector<std::string> names, absent;
  for (const auto& label : labels) {
    names.push_back(shown(label));
    std::optional<std::size_t> position;
    if (pandas_list_text(label))
      for (std::size_t c = 0; c < columns.size(); ++c)
        if (columns[c] == label.value) { position = c; break; }
    if (position) { found.push_back(*position); continue; }
    if (std::find(absent.begin(), absent.end(), names.back()) == absent.end()) absent.push_back(names.back());
  }
  const auto list = [](const std::vector<std::string>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) out += (i ? ", " : "") + values[i];
    return out + "]";
  };
  if (found.empty()) fail("KeyError", quote("None of [" + pandas_index_repr(names, dtype, dtype != "str") + "] are in the [columns]"));
  if (!absent.empty()) fail("KeyError", quote(list(absent) + " not in index"));
  return found;
}

// One grouper of groupby(list key).
struct PandasListGrouper {
  enum class Kind { kColumn, kArray, kMapping, kCallable };
  Kind kind = Kind::kColumn;
  std::size_t column = 0;
  std::vector<EventAttribute> values;  // kArray: one per row
  const EventAttribute* item = nullptr;
  // kArray from a list of three or more dimensions: grouping it fails on
  // the buffer (pandas factorizes one grouper after another).
  int depth = 0;
};

// Factorizing one grouper: a deep array fails on its buffer, an unhashable
// value on its hash.
template <typename Value, typename Fail>
void pandas_list_factorize(const PandasListGrouper& grouper, std::size_t rows, Value&& value, Fail&& fail) {
  if (grouper.depth)
    fail("ValueError", "Buffer has wrong number of dimensions (expected 1, got " + std::to_string(grouper.depth) + ")");
  for (std::size_t i = 0; i < rows; ++i) pandas_check_hashable_key(value(grouper, i), fail);
}

// groupby(key) for a list key with at least one item: its groupers, or the
// error pandas raises while it builds them (in the items' order). `column`
// finds a label among the columns. A key whose items are labels only, as
// many as the frame has rows, one of them naming no column (None counts as
// the index's name), is one array grouper of the labels.
template <typename Column, typename Repr, typename Fail>
std::vector<PandasListGrouper> pandas_list_groupers(const EventAttribute& key, std::size_t rows, Column&& column,
                                                    Repr&& repr, Fail&& fail) {
  std::vector<PandasListGrouper> result;
  const auto set_like = [](const EventAttribute& item) {
    return !item.is_null && (item.container == AttributeContainerKind::kSet ||
                             item.container == AttributeContainerKind::kFrozenSet);
  };
  const auto mapping = [](const EventAttribute& item) {
    return !item.is_null && item.container == AttributeContainerKind::kContainer;
  };
  bool plain = key.children.size() == rows;
  for (const auto& item : key.children)
    if (pandas_list_sequence(item) || mapping(item)) { plain = false; break; }
  // Round DX (probe dw/s1): a set is neither an array nor callable for this
  // reading; looking it up among the columns hashes it. The lookup stops at
  // the first label naming no column.
  bool all_known = true;
  if (plain)
    for (const auto& item : key.children) {
      if (set_like(item)) fail("TypeError", "unhashable type: '" + carrier_type_name(item) + "'");
      if (!item.is_null && !column(item)) { all_known = false; break; }
    }
  if (plain && !all_known) {
    PandasListGrouper grouper;
    grouper.kind = PandasListGrouper::Kind::kArray;
    grouper.values = key.children;
    for (auto& value : grouper.values) value.key.clear();
    result.push_back(std::move(grouper));
    return result;
  }
  for (const auto& item : key.children) {
    PandasListGrouper grouper;
    grouper.item = &item;
    if (mapping(item)) {
      grouper.kind = PandasListGrouper::Kind::kMapping;
    } else if (item.is_null || set_like(item)) {
      // Called on each index label: None and sets are not callable.
      if (rows) fail("TypeError", "'" + carrier_type_name(item) + "' object is not callable");
      grouper.kind = PandasListGrouper::Kind::kCallable;
    } else if (item.container == AttributeContainerKind::kList) {
      if (item.children.size() != rows) fail("ValueError", "Grouper and axis must be same length");
      const int ndim = pandas_list_ndim(item);
      grouper.kind = PandasListGrouper::Kind::kArray;
      if (ndim >= 3) grouper.depth = ndim;
      for (const auto& value : item.children) {
        auto entry = ndim == 2 ? pandas_list_tuple(value) : value;
        entry.key.clear();
        grouper.values.push_back(std::move(entry));
      }
    } else {
      pandas_check_hashable_key(item, fail);
      const auto found = column(item);
      if (!found) fail("KeyError", std::string(repr(item)));
      grouper.kind = PandasListGrouper::Kind::kColumn;
      grouper.column = *found;
    }
    result.push_back(std::move(grouper));
  }
  return result;
}

// df[key].to_numpy() for the listed columns, flattened row by row (N rows
// times k columns) as the one column of a frame: the array's dtype is the
// columns' common NumPy one (one dtype as it is, numbers int64 or float64,
// naive datetimes their finest unit, anything else object), and an object
// array built from a datetime64 column holds Timestamps (NaT for a missing
// cell). `view` is numpy_view_of(source, listed).
inline EventTable pandas_list_flat_cases(const EventTable& view, const std::vector<std::size_t>& listed) {
  std::string dtype;
  if (!view.dtypes.empty()) {
    bool same = true, numbers = true, floats = false, naive_dates = true;
    char finest = 's';
    for (const auto c : listed) {
      const auto& d = view.dtypes[c];
      same &= d == view.dtypes[listed.front()];
      const bool integer = d.starts_with("int") || d.starts_with("uint");
      const bool real = d.starts_with("float");
      numbers &= integer || real;
      floats |= real;
      const bool naive = d.starts_with("datetime64[") && d.find(',') == std::string::npos;
      naive_dates &= naive;
      if (naive) {
        const auto unit = d.substr(11, d.find(']') - 11);
        const char rank = unit == "ns" ? 'n' : unit == "us" ? 'u' : unit == "ms" ? 'm' : 's';
        const auto order = std::string("smun");
        if (order.find(rank) > order.find(finest)) finest = rank;
      }
    }
    if (same) dtype = view.dtypes[listed.front()];
    else if (numbers) dtype = floats ? "float64" : "int64";
    else if (naive_dates)
      dtype = std::string("datetime64[") + (finest == 'n' ? "ns" : finest == 'u' ? "us" : finest == 'm' ? "ms" : "s") + "]";
    else dtype = "object";
  }
  EventTable flat;
  flat.columns = {"case"};
  if (!view.dtypes.empty()) flat.dtypes = {dtype};
  const auto timestamps = [&](std::size_t c) {
    return !view.dtypes.empty() && !dtype.starts_with("datetime64[") && view.dtypes[c].starts_with("datetime64[");
  };
  for (const auto& row : view.rows)
    for (const auto c : listed) {
      auto value = row[c];
      if (timestamps(c)) {
        if (!value || value->is_null) {
          EventAttribute nat{"", ""};
          nat.is_null = true;
          nat.kind = AttributeKind::kDate;
          value = nat;
        } else if (std::holds_alternative<NativeDate>(value->typed_value) || value->kind == AttributeKind::kDate) {
          const auto* native = std::get_if<NativeDate>(&value->typed_value);
          NativeDate stamp = native ? *native : NativeDate{value->value};
          stamp.resolution = NativeDateResolution::kNanosecond;
          value->typed_value = stamp;
        }
      }
      flat.rows.push_back({std::move(value)});
    }
  return flat;
}

inline bool pandas_list_has_list_item(const EventAttribute& key) {
  if (key.is_null || key.container != AttributeContainerKind::kList) return false;
  for (const auto& item : key.children)
    if (!item.is_null && item.container == AttributeContainerKind::kList) return true;
  return false;
}

}  // namespace minera::detail
