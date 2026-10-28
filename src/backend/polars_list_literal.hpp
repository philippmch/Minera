#pragma once

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <variant>

#include "minera/event_log.hpp"
#include "full_double.hpp"

namespace minera::detail {

// The Polars dtype of a Python scalar or list literal ("Null" for None).
inline std::string polars_literal_dtype(const EventAttribute& value);

// A Python list (or tuple) turned into a Polars Series: the first non-null
// item fixes the dtype; an Int64 series takes booleans and a Float64 series
// integers, any other item fails construction.
struct PolarsListLiteral {
  std::string inner = "Null";
  std::optional<std::string> error;  // the TypeError message
};

namespace polars_literal_detail {
inline bool is_bool(const EventAttribute& v) {
  return v.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(v.typed_value);
}
inline bool is_int(const EventAttribute& v) {
  return v.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(v.typed_value) ||
         std::holds_alternative<std::uint64_t>(v.typed_value);
}
inline bool is_float(const EventAttribute& v) {
  return v.kind == AttributeKind::kFloat || std::holds_alternative<double>(v.typed_value);
}
inline bool is_date(const EventAttribute& v) {
  return v.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(v.typed_value);
}
inline std::string date_text(const EventAttribute& v) {
  const auto* native = std::get_if<NativeDate>(&v.typed_value);
  return native ? native->value : v.value;
}
inline double float_of(const EventAttribute& v) {
  if (const auto* d = std::get_if<double>(&v.typed_value)) return *d;
  return ::minera::detail::stod(v.value);
}
// The AnyValue display of an item in the construction error.
inline std::string display(const EventAttribute& v) {
  if (v.container != AttributeContainerKind::kScalar) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.children.size(); ++i) out += (i ? ", " : "") + display(v.children[i]);
    return out + "]";
  }
  if (is_bool(v)) {
    if (const auto* b = std::get_if<bool>(&v.typed_value)) return *b ? "true" : "false";
    return v.value == "true" ? "true" : "false";
  }
  if (is_int(v)) return v.value;
  if (is_float(v)) {
    const double d = float_of(v);
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
    char buffer[64];
    const auto end = std::to_chars(buffer, buffer + sizeof buffer, d).ptr;
    std::string out(buffer, end);
    if (out.find_first_of(".e") == std::string::npos) out += ".0";
    return out;
  }
  if (is_date(v)) {
    auto text = date_text(v);
    if (text.size() > 10) text[10] = ' ';
    return text.substr(0, 19);
  }
  std::string out = "\"";
  for (const char c : v.value) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + '"';
}
}  // namespace polars_literal_detail

inline PolarsListLiteral polars_list_literal(const EventAttribute& list) {
  using namespace polars_literal_detail;
  PolarsListLiteral out;
  for (const auto& item : list.children) {
    if (item.container == AttributeContainerKind::kScalar && item.is_null) continue;
    const auto type = polars_literal_dtype(item);
    if (out.inner == "Null") {
      out.inner = type;
      continue;
    }
    if (type == out.inner || (out.inner == "Int64" && type == "Boolean") || (out.inner == "Float64" && type == "Int64"))
      continue;
    const std::string tail =
        "\n\nThis error occurred with the following context stack:\n\t[1] while constructing Series ''\n\n\n"
        "Hint: Try setting `strict=False` to allow passing data with mixed types.";
    out.error = out.inner.starts_with("List(")
                    ? "unexpected value while building Series of type " + out.inner + tail
                    : "unexpected value while building Series of type " + out.inner + "; found value of type " + type +
                          ": " + display(item) + tail;
    return out;
  }
  return out;
}

inline std::string polars_literal_dtype(const EventAttribute& value) {
  using namespace polars_literal_detail;
  if (value.container == AttributeContainerKind::kList || value.container == AttributeContainerKind::kTuple)
    return "List(" + polars_list_literal(value).inner + ")";
  if (value.is_null) return "Null";
  if (is_bool(value)) return "Boolean";
  if (is_int(value)) return "Int64";
  if (is_float(value)) return "Float64";
  if (is_date(value)) {
    const auto text = date_text(value);
    if (text.size() == 10) return "Date";
    return text.size() > 19 ? "Datetime('\xce\xbcs', 'UTC')" : "Datetime('\xce\xbcs')";
  }
  return "String";
}

}  // namespace minera::detail
