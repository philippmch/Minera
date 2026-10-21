#include "minera/pandas_between_filter.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <numeric>
#include <optional>
#include <string_view>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

namespace minera {
namespace {
[[noreturn]] void error(std::string type, std::string message) {
  throw PandasBetweenError(std::move(type), std::move(message));
}
std::string string_key_repr(std::string_view value) {
  const char quote = value.find('\'') != std::string_view::npos &&
                             value.find('"') == std::string_view::npos
                         ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < value.size();) {
    const auto start = position;
    const auto first = static_cast<unsigned char>(value[position++]);
    char32_t code = first;
    unsigned continuation = 0;
    if (first < 0x80) {}
    else if (first >= 0xc2 && first <= 0xdf) { code = first & 31; continuation = 1; }
    else if (first >= 0xe0 && first <= 0xef) { code = first & 15; continuation = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; continuation = 3; }
    else error("ValueError", "pandas between selector has invalid UTF-8");
    for (unsigned i = 0; i < continuation; ++i) {
      if (position == value.size()) error("ValueError", "pandas between selector has short UTF-8");
      const auto next = static_cast<unsigned char>(value[position++]);
      if ((next & 0xc0) != 0x80) error("ValueError", "pandas between selector has invalid UTF-8 continuation");
      code = (code << 6) | (next & 63);
    }
    if ((continuation == 1 && code < 0x80) ||
        (continuation == 2 && code < 0x800) ||
        (continuation == 3 && code < 0x10000) || code > 0x10ffff)
      error("ValueError", "pandas between selector has invalid code point");
    if (code == static_cast<unsigned char>(quote) || code == '\\') {
      result += '\\'; result += static_cast<char>(code);
    } else if (code == '\n') result += "\\n";
    else if (code == '\r') result += "\\r";
    else if (code == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(code)) {
      const int digits = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(code >> shift) & 15];
    } else result.append(value.substr(start, position - start));
  }
  return result + quote;
}

bool string_value(const EventAttribute& value) {
  return !value.is_null && value.container == AttributeContainerKind::kScalar &&
         (value.kind == AttributeKind::kString || value.kind == AttributeKind::kId) &&
         std::holds_alternative<std::monostate>(value.typed_value);
}

std::string type_name(const EventAttribute& value) {
  if (value.kind == AttributeKind::kDate && value.is_null) return "NaTType";
  if (value.is_null) return "NoneType";
  switch (value.container) {
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kContainer: return "dict";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
    case AttributeContainerKind::kScalar: break;
  }
  if (is_native_na_attribute(value)) return "NAType";
  if (value.kind == AttributeKind::kBytes) return "bytes";
  if (value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value)) return "bool";
  if (value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) || std::holds_alternative<std::uint64_t>(value.typed_value)) return "int";
  if (value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value)) return "float";
  if (const auto* date = std::get_if<NativeDate>(&value.typed_value))
    return date->resolution == NativeDateResolution::kNanosecond ? "Timestamp" : "datetime";
  if (value.kind == AttributeKind::kDate) return "datetime";
  return "str";
}

bool date_value(const EventAttribute& value) {
  return value.kind == AttributeKind::kDate ||
         std::holds_alternative<NativeDate>(value.typed_value);
}

ParsedExactTimestamp date_instant(const EventAttribute& value) {
  const auto* native = std::get_if<NativeDate>(&value.typed_value);
  const auto parsed = parse_timestamp_exact(native ? native->value : value.value);
  if (!parsed) error("ValueError", "pandas between contains invalid datetime");
  return *parsed;
}

std::string date_string(const EventAttribute& value, bool numpy) {
  if (value.is_null) return "NaT";
  const auto* native = std::get_if<NativeDate>(&value.typed_value);
  const auto& text = native ? native->value : value.value;
  const auto instant = date_instant(value);
  if (text.size() < 19) error("ValueError", "pandas between contains short datetime");
  std::string result = text.substr(0, 19);
  result[10] = numpy ? 'T' : ' ';
  if (numpy || instant.nanoseconds) {
    const unsigned digits = numpy || instant.nanoseconds % 1000 ? 9 : 6;
    const auto fraction = std::to_string(digits == 9 ? instant.nanoseconds : instant.nanoseconds / 1000);
    result += '.' + std::string(digits - fraction.size(), '0') + fraction;
  }
  std::size_t suffix = 19;
  if (suffix < text.size() && text[suffix] == '.') {
    ++suffix;
    while (suffix < text.size() && text[suffix] >= '0' && text[suffix] <= '9') ++suffix;
  }
  if (!numpy) result += text.substr(suffix) == "Z" ? "+00:00" : text.substr(suffix);
  return result;
}

std::string ordinary_string(const EventAttribute& value, bool nested = false) {
  if (value.is_null) return value.kind == AttributeKind::kDate ? "NaT" : "None";
  if (value.scalar_wrapper || value.scalar_children_list)
    error("ValueError", "pandas between labels require ordinary values");
  if (string_value(value)) return nested ? string_key_repr(value.value) : value.value;
  if (date_value(value) && !nested) return date_string(value, false);
  if (value.container == AttributeContainerKind::kScalar)
    return root_cause_attribute_category(value);
  std::vector<const EventAttribute*> children;
  const bool dictionary = value.container == AttributeContainerKind::kContainer;
  const bool set = value.container == AttributeContainerKind::kSet ||
                   value.container == AttributeContainerKind::kFrozenSet;
  if (set) children = root_cause_set_members(value);
  else if (dictionary) {
    std::map<std::string, std::size_t> positions;
    for (const auto& child : value.children) {
      const auto [found, inserted] = positions.emplace(child.key, children.size());
      if (inserted) children.push_back(&child);
      else children[found->second] = &child;
    }
  } else for (const auto& child : value.children) children.push_back(&child);
  const bool tuple = value.container == AttributeContainerKind::kTuple;
  const bool frozen = value.container == AttributeContainerKind::kFrozenSet;
  if (set && children.empty()) return frozen ? "frozenset()" : "set()";
  std::string result = dictionary || set ? "{" : tuple ? "(" : "[";
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (i) result += ", ";
    if (dictionary) result += string_key_repr(children[i]->key) + ": ";
    result += ordinary_string(*children[i], true);
  }
  if (tuple && children.size() == 1) result += ',';
  result += dictionary || set ? "}" : tuple ? ")" : "]";
  return frozen ? "frozenset(" + result + ")" : result;
}

std::string representation(const EventAttribute& value) {
  return ordinary_string(value, true);
}

std::optional<std::size_t> column(const EventTable& source, const EventAttribute& key,
                                bool grouping) {
  if (string_value(key)) {
    const auto found = std::find(source.columns.begin(), source.columns.end(), key.value);
    if (found != source.columns.end()) return static_cast<std::size_t>(found - source.columns.begin());
  } else if (key.container == AttributeContainerKind::kContainer) {
    error("TypeError", "Passing a dict as an indexer is not supported. Use a list instead.");
  } else if (key.container == AttributeContainerKind::kList && key.children.empty()) {
    if (grouping) error("ValueError", "Columns must be same length as key");
    if (source.rows.empty()) return std::nullopt;
    error("ValueError", "The truth value of an empty array is ambiguous. Use `array.size > 0` to check that an array is not empty.");
  }
  error("KeyError", representation(key));
}

EventAttribute cell(const EventTable& source, std::size_t row, std::size_t col) {
  if (source.rows[row][col]) return *source.rows[row][col];
  EventAttribute missing{"", ""}; missing.is_null = true; return missing;
}

bool value_equal(const EventAttribute& a, const EventAttribute& b) {
  if (a.is_null || b.is_null) {
    if (a.kind == AttributeKind::kDate || b.kind == AttributeKind::kDate) return false;
    return a.is_null && b.is_null;
  }
  if (is_native_na_attribute(a) || is_native_na_attribute(b))
    error("TypeError", "boolean value of NA is ambiguous");
  const auto set = [](const EventAttribute& value) {
    return value.container == AttributeContainerKind::kSet ||
           value.container == AttributeContainerKind::kFrozenSet;
  };
  if (set(a) || set(b)) {
    if (!set(a) || !set(b)) return false;
    const auto left = root_cause_set_members(a), right = root_cause_set_members(b);
    if (left.size() != right.size()) return false;
    for (const auto* x : left)
      if (std::none_of(right.begin(), right.end(), [&](const auto* y) {
            return value_equal(*x, *y);
          })) return false;
    return true;
  }
  if (a.container != b.container) return false;
  if (a.container == AttributeContainerKind::kContainer) {
    std::map<std::string, const EventAttribute*> left, right;
    for (const auto& child : a.children) left[child.key] = &child;
    for (const auto& child : b.children) right[child.key] = &child;
    if (left.size() != right.size()) return false;
    for (const auto& [key, x] : left) {
      const auto found = right.find(key);
      if (found == right.end() || !value_equal(*x, *found->second)) return false;
    }
    return true;
  }
  if (a.container != AttributeContainerKind::kScalar) {
    if (a.children.size() != b.children.size()) return false;
    for (std::size_t i = 0; i < a.children.size(); ++i)
      if (!value_equal(a.children[i], b.children[i])) return false;
    return true;
  }
  for (const auto* v : {&a, &b}) {
    if (const auto* n = std::get_if<double>(&v->typed_value); n && std::isnan(*n)) return false;
    if (v->kind == AttributeKind::kFloat && std::holds_alternative<std::monostate>(v->typed_value) && std::isnan(native_attribute_number(*v))) return false;
  }
  if (date_value(a) || date_value(b)) {
    if (!date_value(a) || !date_value(b)) return false;
    const auto x = date_instant(a), y = date_instant(b);
    return x.timezone_aware == y.timezone_aware && x.seconds == y.seconds &&
           x.nanoseconds == y.nanoseconds;
  }
  return root_cause_scalar_keys_equal(a, b);
}

bool nan_value(const EventAttribute& value) {
  if (value.is_null || value.container != AttributeContainerKind::kScalar) return false;
  if (const auto* n = std::get_if<double>(&value.typed_value)) return std::isnan(*n);
  return value.kind == AttributeKind::kFloat && std::isnan(native_attribute_number(value));
}

struct CaseValues {
  std::vector<EventAttribute> values;
  bool numeric_array = false;
  bool numpy_dates = false;
};

CaseValues case_values(const EventTable& source, std::size_t key) {
  CaseValues result;
  result.values.reserve(source.rows.size());
  for (std::size_t row = 0; row < source.rows.size(); ++row)
    result.values.push_back(cell(source, row, key));
  const auto dtype = source.dtypes.empty() ? std::string() : source.dtypes[key];
  result.numeric_array = dtype == "float64";
  if (dtype == "Int64" && std::any_of(result.values.begin(), result.values.end(), [](const auto& v) {
        return v.is_null || is_native_na_attribute(v);
      })) result.numeric_array = true;
  if (result.numeric_array) {
    for (auto& value : result.values) {
      const double number = value.is_null || is_native_na_attribute(value)
          ? std::numeric_limits<double>::quiet_NaN() : native_attribute_number(value);
      value = EventAttribute{"", "", AttributeKind::kFloat};
      value.typed_value = number;
    }
  }
  result.numpy_dates = dtype == "datetime64[ns]";
  return result;
}

bool same_case(const EventAttribute& a, const EventAttribute& b, const CaseValues& values) {
  if (values.numeric_array && nan_value(a) && nan_value(b)) return true;
  if (values.numpy_dates && date_value(a) && date_value(b) && a.is_null && b.is_null) return true;
  return value_equal(a, b);
}

int case_compare(const EventAttribute& a, const EventAttribute& b,
                 bool numeric_array = false, bool numpy_dates = false) {
  const auto unsupported = [&]() -> int {
    error("TypeError", "'<' not supported between instances of '" + type_name(a) + "' and '" + type_name(b) + "'");
  };
  if (is_native_na_attribute(a) || is_native_na_attribute(b))
    error("TypeError", "boolean value of NA is ambiguous");
  if (numeric_array && (nan_value(a) || nan_value(b)))
    return nan_value(a) ? nan_value(b) ? 0 : 1 : -1;
  if (date_value(a) && date_value(b) && (a.is_null || b.is_null)) {
    if (!numpy_dates) return 0;
    return a.is_null ? b.is_null ? 0 : 1 : -1;
  }
  if (a.is_null || b.is_null) return unsupported();
  if ((a.container == AttributeContainerKind::kList || a.container == AttributeContainerKind::kTuple) && a.container == b.container) {
    for (std::size_t i = 0; i < std::min(a.children.size(), b.children.size()); ++i) {
      if (value_equal(a.children[i], b.children[i])) continue;
      return case_compare(a.children[i], b.children[i]);
    }
    return a.children.size() < b.children.size() ? -1 : a.children.size() > b.children.size() ? 1 : 0;
  }
  if (native_scalar_order_kind(a) == NativeScalarOrderKind::kUnsupported ||
      native_scalar_order_kind(a) != native_scalar_order_kind(b)) return unsupported();
  if (native_scalar_order_kind(a) == NativeScalarOrderKind::kNumeric &&
      (nan_value(a) || nan_value(b))) return 0;
  if (native_scalar_order_kind(a) == NativeScalarOrderKind::kDate) {
    const auto date = [](const EventAttribute& v) {
      const auto* native = std::get_if<NativeDate>(&v.typed_value);
      const auto parsed = parse_timestamp_exact(native ? native->value : v.value);
      if (!parsed) error("ValueError", "pandas between case contains invalid datetime");
      return *parsed;
    };
    const auto x = date(a), y = date(b);
    if (x.timezone_aware != y.timezone_aware) return unsupported();
    if (x.seconds != y.seconds) return x.seconds < y.seconds ? -1 : 1;
    return x.nanoseconds < y.nanoseconds ? -1 : x.nanoseconds > y.nanoseconds ? 1 : 0;
  }
  try { return native_scalar_key_compare(a, b); }
  catch (const std::invalid_argument&) { return unsupported(); }
}

void merge_cases(std::vector<std::size_t>& rows, std::vector<std::size_t>& buffer,
                 const CaseValues& source, std::size_t first,
                 std::size_t last) {
  if (last - first < 2) return;
  if (last - first <= 16) {
    // Adjacent comparisons preserve the observed object-array diagnostics;
    // larger blocks retain the same stable merge ordering.
    for (std::size_t i = first + 1; i < last; ++i) {
      const auto current = rows[i];
      auto position = i;
      while (position > first &&
             case_compare(source.values[rows[position - 1]], source.values[current],
                          source.numeric_array, source.numpy_dates) > 0) {
        rows[position] = rows[position - 1];
        --position;
      }
      rows[position] = current;
    }
    return;
  }
  const auto middle = first + (last - first) / 2;
  merge_cases(rows, buffer, source, first, middle);
  merge_cases(rows, buffer, source, middle, last);
  std::size_t a = first, b = middle, out = first;
  while (a < middle && b < last) {
    if (case_compare(source.values[rows[a]], source.values[rows[b]], source.numeric_array, source.numpy_dates) <= 0)
      buffer[out++] = rows[a++];
    else buffer[out++] = rows[b++];
  }
  while (a < middle) buffer[out++] = rows[a++];
  while (b < last) buffer[out++] = rows[b++];
  std::copy(buffer.begin() + first, buffer.begin() + last, rows.begin() + first);
}

bool contains(const EventAttribute& query, const EventAttribute& value) {
  if (string_value(query)) return value_equal(query, value);
  if (query.container == AttributeContainerKind::kList || query.container == AttributeContainerKind::kTuple ||
      query.container == AttributeContainerKind::kSet || query.container == AttributeContainerKind::kFrozenSet) {
    for (const auto& choice : query.children)
      if (value_equal(choice, value)) return true;
    return false;
  }
  if (query.container == AttributeContainerKind::kContainer) {
    for (const auto& choice : query.children)
      if (value_equal(EventAttribute{"", choice.key}, value)) return true;
    return false;
  }
  if (query.kind == AttributeKind::kBytes && !query.is_null) {
    const auto bytes = decode_bytes_attribute(query);
    if (value.kind == AttributeKind::kBytes && !value.is_null)
      return bytes.find(decode_bytes_attribute(value)) != std::string::npos;
    if (!value.is_null && native_scalar_order_kind(value) == NativeScalarOrderKind::kNumeric && type_name(value) != "float") {
      const auto n = native_attribute_number(value);
      if (n < 0 || n > 255) error("ValueError", "byte must be in range(0, 256)");
      return bytes.find(static_cast<char>(n)) != std::string::npos;
    }
    error("TypeError", "a bytes-like object is required, not '" + type_name(value) + "'");
  }
  error("TypeError", "argument of type '" + type_name(query) + "' is not iterable");
}

bool arithmetic_index(const std::vector<EventTableIndexLabel>& labels) {
  std::vector<TokenInteger> numbers;
  for (const auto& label : labels) {
    const auto* value = std::get_if<EventAttribute>(&label.value);
    if (!value || value->is_null) return false;
    if (const auto* n = std::get_if<std::int64_t>(&value->typed_value)) numbers.emplace_back(*n);
    else if (const auto* n = std::get_if<std::uint64_t>(&value->typed_value)) numbers.emplace_back(*n);
    else return false;
  }
  if (numbers.size() < 2) return true;
  const auto step = numbers[1] - numbers[0];
  if (step == 0) return false;
  for (std::size_t i = 2; i < numbers.size(); ++i)
    if (numbers[i] - numbers[i - 1] != step) return false;
  return true;
}

std::string separator(const EventAttribute& value) {
  if (string_value(value)) return value.value;
  error("TypeError", "can only concatenate str (not \"" + type_name(value) + "\") to str");
}
}  // namespace

EventTable filter_pandas_between(const EventTable& source, const EventAttribute& act1,
                                const EventAttribute& act2, const PandasBetweenOptions& options) {
  source.validate();
  const auto key = *column(source, options.case_id_key, true);
  const auto cases = case_values(source, key);
  std::vector<std::size_t> sorted(source.rows.size()), buffer(source.rows.size());
  std::iota(sorted.begin(), sorted.end(), 0);
  merge_cases(sorted, buffer, cases, 0, sorted.size());
  const auto activity = column(source, options.activity_key, false);
  std::vector<std::optional<std::string>> marks(source.rows.size());
  for (std::size_t first = 0; first < sorted.size();) {
    std::size_t last = first + 1;
    while (last < sorted.size() && same_case(cases.values[sorted[first]], cases.values[sorted[last]], cases)) ++last;
    const auto begin = *std::min_element(sorted.begin() + first, sorted.begin() + last);
    const auto end = begin + last - first;
    std::optional<std::size_t> opening;
    std::size_t subcase = 0;
    for (std::size_t row = begin; row < end; ++row) {
      const auto value = cell(source, row, *activity);
      if (!opening) {
        if (contains(act1, value)) opening = row;
      } else if (contains(act2, value)) {
        const auto& case_value = cases.values[sorted[first]];
        const auto label = (cases.numpy_dates ? date_string(case_value, true) : ordinary_string(case_value)) + separator(options.subcase_concat_str) + std::to_string(subcase++);
        for (std::size_t kept = *opening; kept <= row; ++kept) marks[kept] = label;
        opening = value_equal(act1, act2) ? std::optional<std::size_t>(row) : std::nullopt;
      }
    }
    first = last;
  }
  EventTable result = source;
  result.rows.clear();
  if (result.index) result.index->labels.clear();
  else result.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  for (std::size_t row = 0; row < marks.size(); ++row) {
    if (!marks[row]) continue;
    auto kept = source.rows[row];
    EventAttribute replacement = kept[key] ? *kept[key] : EventAttribute{source.columns[key], ""};
    replacement.value = *marks[row]; replacement.kind = AttributeKind::kString;
    replacement.typed_value = std::monostate{}; replacement.is_null = false;
    replacement.children.clear(); replacement.container = AttributeContainerKind::kScalar;
    replacement.scalar_wrapper = false; replacement.scalar_children_list = false;
    if (replacement.first_class_role != FirstClassAttributeRole::kNone)
      replacement.first_class_state = FirstClassAttributeState::kPresent;
    kept[key] = std::move(replacement); result.rows.push_back(std::move(kept));
    if (source.index) result.index->labels.push_back(source.index->labels[row]);
    else {
      EventAttribute index{"", std::to_string(row), AttributeKind::kInt};
      index.typed_value = static_cast<std::uint64_t>(row);
      result.index->labels.push_back({std::move(index)});
    }
  }
  if (result.index->kind == "RangeIndex" && !arithmetic_index(result.index->labels)) result.index->kind = "Index";
  if (!result.dtypes.empty()) result.dtypes[key] = result.rows.empty() ? "float64" : "str";
  if (!result.categorical_dtypes.empty()) result.categorical_dtypes[key] = std::nullopt;
  return result;
}

EventTable filter_pandas_between(const EventTable& source, const EventAttribute& act1,
                                const EventAttribute& act2, const EventAttribute& parameters) {
  PandasBetweenOptions options;
  if (parameters.is_null || (parameters.container == AttributeContainerKind::kList && parameters.children.empty()))
    return filter_pandas_between(source, act1, act2, options);
  if (parameters.container == AttributeContainerKind::kContainer) {
    for (const auto& value : parameters.children) {
      if (value.key == "pm4py:param:case_id_key") options.case_id_key = value;
      else if (value.key == "pm4py:param:activity_key") options.activity_key = value;
      else if (value.key == "subcase_concat_str") options.subcase_concat_str = value;
    }
    return filter_pandas_between(source, act1, act2, options);
  }
  if (string_value(parameters)) {
    if (parameters.value.empty()) return filter_pandas_between(source, act1, act2, options);
    error("TypeError", "string indices must be integers, not 'str'");
  }
  error("TypeError", "'" + type_name(parameters) + "' object is not iterable");
}
}  // namespace minera
