#pragma once

#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "root_cause_trace_key.hpp"

namespace minera::detail {

// pandas' masked numeric extension dtypes (pd.NA for a missing cell).
inline bool masked_numeric_dtype(std::string_view dtype) {
  for (const std::string_view name : {"Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16",
                                      "UInt32", "UInt64", "Float32", "Float64"})
    if (dtype == name) return true;
  return false;
}

inline bool numpy_number_dtype(std::string_view dtype) {
  if (dtype == "bool") return true;
  for (const std::string_view prefix : {"int", "uint", "float"})
    if (dtype.starts_with(prefix) && dtype.size() > prefix.size() &&
        dtype.find_first_not_of("0123456789", prefix.size()) == std::string_view::npos)
      return true;
  return false;
}

inline bool missing_table_cell(const std::optional<EventAttribute>& cell) {
  return !cell || cell->is_null || is_native_na_attribute(*cell);
}

inline EventAttribute numpy_nan(const std::string& key) {
  EventAttribute value{key, "nan", AttributeKind::kFloat};
  value.typed_value = std::numeric_limits<double>::quiet_NaN();
  return value;
}

inline EventAttribute numpy_nat(const std::string& key) {
  return EventAttribute{key, "NaT", AttributeKind::kDate, true};
}

// How a missing cell of a categorical column reads when the Series is
// iterated or handed to NumPy: NaT for datetime categories, NaN otherwise.
inline EventAttribute categorical_missing(const EventTable& source, std::size_t col) {
  const auto* categorical = col < source.categorical_dtypes.size() && source.categorical_dtypes[col]
                                ? &*source.categorical_dtypes[col] : nullptr;
  if (categorical && categorical->categories_dtype.starts_with("datetime64["))
    return numpy_nat(source.columns[col]);
  return numpy_nan(source.columns[col]);
}

// The columns `cols` as Series.to_numpy() hands them to NumPy in pandas 3
// (observed in the reference environment, round BZ); every other column is
// left as it is. A masked numeric column (Int64, UInt8, Float64, ...) with a
// missing cell becomes a float64 array (float32 for Float32) holding NaN, and
// without one its NumPy dtype; a boolean column with a missing cell stays an
// object array holding pd.NA, and without one becomes bool; string columns
// are object arrays; a categorical column becomes its categories' array, with
// numeric categories turning to float64 with NaN when a cell is missing and
// other categories holding NaN (NaT for datetimes) as objects. The view's
// dtypes name the NumPy array ("object" for Python objects).
inline EventTable numpy_view(const EventTable& source, std::initializer_list<std::size_t> cols) {
  EventTable view = source;
  if (source.dtypes.empty()) return view;
  for (const auto col : cols) {
    if (col >= source.dtypes.size()) continue;
    const auto& dtype = source.dtypes[col];
    bool missing = false;
    for (const auto& row : source.rows) missing |= missing_table_cell(row[col]);
    const auto to_float = [&](bool float32) {
      for (auto& row : view.rows) {
        auto& cell = row[col];
        if (missing_table_cell(cell)) { cell = numpy_nan(source.columns[col]); continue; }
        double number = native_attribute_number(*cell);
        if (float32) number = static_cast<float>(number);
        EventAttribute value{cell->key, "", AttributeKind::kFloat};
        value.typed_value = number;
        cell = std::move(value);
      }
      view.dtypes[col] = float32 ? "float32" : "float64";
    };
    if (masked_numeric_dtype(dtype)) {
      if (missing) to_float(dtype == "Float32");
      else {
        std::string lowered = dtype;
        lowered[0] = static_cast<char>(lowered[0] == 'U' ? 'u' : lowered[0] == 'I' ? 'i' : 'f');
        view.dtypes[col] = lowered;
      }
    } else if (dtype == "boolean") {
      view.dtypes[col] = missing ? "object" : "bool";
    } else if (dtype == "category") {
      const auto* categorical = col < source.categorical_dtypes.size() && source.categorical_dtypes[col]
                                    ? &*source.categorical_dtypes[col] : nullptr;
      const std::string categories = categorical ? categorical->categories_dtype : "object";
      if (numpy_number_dtype(categories) && categories != "bool") {
        if (missing) to_float(categories == "float32");
        else view.dtypes[col] = categories;
      } else {
        const bool naive_dates = categories.starts_with("datetime64[") &&
                                 categories.find(',') == std::string::npos;
        view.dtypes[col] = naive_dates || (categories == "bool" && !missing) ? categories : "object";
        for (auto& row : view.rows)
          if (missing_table_cell(row[col])) row[col] = categorical_missing(source, col);
      }
      if (!view.categorical_dtypes.empty()) view.categorical_dtypes[col] = std::nullopt;
    } else if (dtype == "str" || dtype == "string") {
      view.dtypes[col] = "object";
    }
  }
  return view;
}

}  // namespace minera::detail
