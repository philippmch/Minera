#pragma once

// Python scalar semantics for pandas-backed reference paths: the equality,
// hashing identity, ordering, str() spelling and TypeError messages of the
// bool/int/float/str/None/Timestamp values a dataframe column holds, plus the
// CPython list.sort and pandas groupby behaviours that depend on them. The
// rules were pinned black-box against the reference: a Python model of them
// was fuzzed against public calls until it agreed on every input.

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/filtering_common.hpp"
#include "minera/timestamp.hpp"
#include "numpy_object_sort.hpp"

namespace minera::python {

// Python's TypeError, as a std::invalid_argument so existing handlers keep
// reporting it.
struct TypeError : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};

struct Scalar {
  enum class Kind { kNone, kNA, kBool, kInt, kFloat, kText, kDate };
  Kind kind = Kind::kNone;
  bool boolean = false;
  bool negative = false;  // kInt sign
  std::string digits;     // kInt magnitude without leading zeros ("0" for zero)
  double real = 0.0;      // kFloat
  std::string text;       // kText value; the original spelling of a kDate
  ParsedExactTimestamp date;  // kDate
};

inline Scalar none() { return {}; }
inline Scalar text(std::string value) {
  Scalar s;
  s.kind = Scalar::Kind::kText;
  s.text = std::move(value);
  return s;
}
inline Scalar boolean(bool value) {
  Scalar s;
  s.kind = Scalar::Kind::kBool;
  s.boolean = value;
  return s;
}
inline Scalar real(double value) {
  Scalar s;
  s.kind = Scalar::Kind::kFloat;
  s.real = value;
  return s;
}
inline Scalar integer(std::int64_t value) {
  Scalar s;
  s.kind = Scalar::Kind::kInt;
  s.negative = value < 0;
  const std::uint64_t magnitude =
      s.negative ? std::uint64_t(0) - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
  s.digits = std::to_string(magnitude);
  return s;
}
inline Scalar unsigned_integer(std::uint64_t value) {
  Scalar s;
  s.kind = Scalar::Kind::kInt;
  s.digits = std::to_string(value);
  return s;
}
// A decimal integer literal of any width (optional sign, digits only).
inline std::optional<Scalar> integer_literal(std::string_view literal) {
  std::string_view rest = literal;
  bool negative = false;
  if (!rest.empty() && (rest.front() == '+' || rest.front() == '-')) {
    negative = rest.front() == '-';
    rest.remove_prefix(1);
  }
  if (rest.empty()) return std::nullopt;
  for (const char c : rest)
    if (c < '0' || c > '9') return std::nullopt;
  while (rest.size() > 1 && rest.front() == '0') rest.remove_prefix(1);
  Scalar s;
  s.kind = Scalar::Kind::kInt;
  s.digits = std::string(rest);
  s.negative = negative && s.digits != "0";
  return s;
}
inline std::optional<Scalar> date_literal(std::string_view literal) {
  const auto parsed = parse_timestamp_exact(literal);
  if (!parsed) return std::nullopt;
  Scalar s;
  s.kind = Scalar::Kind::kDate;
  s.date = *parsed;
  s.text = std::string(literal);
  return s;
}

// The Python object a dataframe cell holds.
inline Scalar from_attribute(const EventAttribute& cell) {
  if (cell.is_null) return none();
  if (std::holds_alternative<NativeNA>(cell.typed_value)) {
    Scalar s;
    s.kind = Scalar::Kind::kNA;
    return s;
  }
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) return boolean(*b);
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return integer(*i);
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return unsigned_integer(*u);
  if (const auto* d = std::get_if<double>(&cell.typed_value)) return real(*d);
  if (const auto* date = std::get_if<NativeDate>(&cell.typed_value)) {
    if (auto parsed = date_literal(date->value)) return *parsed;
    return text(date->value);
  }
  switch (cell.kind) {
    case AttributeKind::kBoolean:
      if (cell.value == "true" || cell.value == "True") return boolean(true);
      if (cell.value == "false" || cell.value == "False") return boolean(false);
      break;
    case AttributeKind::kInt:
      if (auto parsed = integer_literal(cell.value)) return *parsed;
      break;
    case AttributeKind::kFloat: {
      if (!cell.value.empty()) {
        char* end = nullptr;
        const double d = std::strtod(cell.value.c_str(), &end);
        if (end && *end == '\0') return real(d);
      }
      break;
    }
    case AttributeKind::kDate:
      if (auto parsed = date_literal(cell.value)) return *parsed;
      break;
    default: break;
  }
  return text(cell.value);
}

inline bool is_number(const Scalar& s) {
  return s.kind == Scalar::Kind::kBool || s.kind == Scalar::Kind::kInt || s.kind == Scalar::Kind::kFloat;
}
inline bool is_missing(const Scalar& s) {
  return s.kind == Scalar::Kind::kNone || s.kind == Scalar::Kind::kNA ||
         (s.kind == Scalar::Kind::kFloat && std::isnan(s.real));
}

inline std::string type_name(const Scalar& s) {
  switch (s.kind) {
    case Scalar::Kind::kNone: return "NoneType";
    case Scalar::Kind::kNA: return "NAType";
    case Scalar::Kind::kBool: return "bool";
    case Scalar::Kind::kInt: return "int";
    case Scalar::Kind::kFloat: return "float";
    case Scalar::Kind::kText: return "str";
    case Scalar::Kind::kDate: return "Timestamp";
  }
  return "object";
}

// repr() of a Python float.
inline std::string float_repr(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
  return filtering_float_repr(value);
}

// The exact decimal digits of an integral double (no sign).
inline std::string integral_digits(double value) {
  if (value == 0) return "0";
  char buffer[512];
  const auto r = std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(value), std::chars_format::fixed, 0);
  if (r.ec != std::errc{}) throw std::invalid_argument("python scalar: float cannot be formatted");
  return std::string(buffer, r.ptr);
}

// str() of a pandas Timestamp: its wall time at its own offset, six
// fractional digits when it has microseconds and nine when it has
// nanoseconds, and the offset when it is aware.
inline std::string timestamp_str(const ParsedExactTimestamp& date) {
  const std::int64_t local = date.seconds + (date.timezone_aware ? std::int64_t(date.offset_minutes) * 60 : 0);
  std::int64_t days = local / 86400, rest = local % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  // Howard Hinnant's civil_from_days.
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const std::int64_t doe = days - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t year = yoe + era * 400 + (month <= 2 ? 1 : 0);
  char buffer[96];
  int n = std::snprintf(buffer, sizeof(buffer), "%04lld-%02lld-%02lld %02lld:%02lld:%02lld", static_cast<long long>(year),
                        static_cast<long long>(month), static_cast<long long>(day),
                        static_cast<long long>(rest / 3600), static_cast<long long>(rest / 60 % 60),
                        static_cast<long long>(rest % 60));
  std::string out(buffer, static_cast<std::size_t>(n));
  if (date.nanoseconds % 1000 != 0) {
    n = std::snprintf(buffer, sizeof(buffer), ".%09u", static_cast<unsigned>(date.nanoseconds));
    out.append(buffer, static_cast<std::size_t>(n));
  } else if (date.nanoseconds != 0) {
    n = std::snprintf(buffer, sizeof(buffer), ".%06u", static_cast<unsigned>(date.nanoseconds / 1000));
    out.append(buffer, static_cast<std::size_t>(n));
  }
  if (date.timezone_aware) {
    const int offset = date.offset_minutes < 0 ? -date.offset_minutes : date.offset_minutes;
    n = std::snprintf(buffer, sizeof(buffer), "%c%02d:%02d", date.offset_minutes < 0 ? '-' : '+', offset / 60, offset % 60);
    out.append(buffer, static_cast<std::size_t>(n));
  }
  return out;
}

// str() of the value.
inline std::string str(const Scalar& s) {
  switch (s.kind) {
    case Scalar::Kind::kNone: return "None";
    case Scalar::Kind::kNA: return "<NA>";
    case Scalar::Kind::kBool: return s.boolean ? "True" : "False";
    case Scalar::Kind::kInt: return (s.negative ? "-" : "") + s.digits;
    case Scalar::Kind::kFloat: return float_repr(s.real);
    case Scalar::Kind::kText: return s.text;
    case Scalar::Kind::kDate: return timestamp_str(s.date);
  }
  return {};
}

// A key equal exactly for values that are == and hash alike: 1, True and 1.0
// share one key, '1' has another, every NaN shares one (the same object).
inline std::string identity(const Scalar& s) {
  switch (s.kind) {
    case Scalar::Kind::kNone: return "N";
    case Scalar::Kind::kNA: return "A";
    case Scalar::Kind::kBool: return s.boolean ? "I1" : "I0";
    case Scalar::Kind::kInt: return "I" + std::string(s.negative ? "-" : "") + s.digits;
    case Scalar::Kind::kFloat: {
      if (std::isnan(s.real)) return "Fnan";
      if (std::isfinite(s.real) && std::trunc(s.real) == s.real)
        return "I" + std::string(s.real < 0 ? "-" : "") + integral_digits(s.real);
      char buffer[64];
      const auto r = std::to_chars(buffer, buffer + sizeof(buffer), s.real);
      return "F" + std::string(buffer, r.ptr);
    }
    case Scalar::Kind::kText: return "S" + s.text;
    case Scalar::Kind::kDate:
      return std::string(s.date.timezone_aware ? "DA" : "DN") + std::to_string(s.date.seconds) + ":" +
             std::to_string(s.date.nanoseconds);
  }
  return {};
}

inline bool equal(const Scalar& a, const Scalar& b) { return identity(a) == identity(b); }

[[noreturn]] inline void unorderable(const Scalar& a, const Scalar& b) {
  throw TypeError("'<' not supported between instances of '" + type_name(a) + "' and '" + type_name(b) + "'");
}

// Three-way comparison of two decimal magnitudes without leading zeros.
inline int compare_digits(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  const int c = a.compare(b);
  return c < 0 ? -1 : c > 0 ? 1 : 0;
}

// Three-way comparison of two signed integers given as sign and magnitude.
inline int compare_integers(bool negative_a, std::string_view a, bool negative_b, std::string_view b) {
  if (a == "0" && b == "0") return 0;
  if (negative_a != negative_b) return negative_a ? -1 : 1;
  const int magnitude = compare_digits(a, b);
  return negative_a ? -magnitude : magnitude;
}

// Python's a < b / a > b for numbers, exact across int and float. NaN orders
// with nothing (0).
inline int compare_numbers(const Scalar& a, const Scalar& b) {
  const auto as_int = [](const Scalar& s, bool& negative, std::string& digits) {
    if (s.kind == Scalar::Kind::kBool) {
      negative = false;
      digits = s.boolean ? "1" : "0";
      return true;
    }
    if (s.kind == Scalar::Kind::kInt) {
      negative = s.negative;
      digits = s.digits;
      return true;
    }
    return false;
  };
  bool na = false, nb = false;
  std::string da, db;
  const bool ia = as_int(a, na, da), ib = as_int(b, nb, db);
  if (ia && ib) return compare_integers(na, da, nb, db);
  if (!ia && !ib) {
    if (std::isnan(a.real) || std::isnan(b.real)) return 0;
    return a.real < b.real ? -1 : a.real > b.real ? 1 : 0;
  }
  // One integer, one float: compare with the float's integer part, then its
  // fraction breaks a tie.
  const bool swapped = !ia;
  const bool negative = swapped ? nb : na;
  const std::string& digits = swapped ? db : da;
  const double d = swapped ? a.real : b.real;
  if (std::isnan(d)) return 0;
  int result = 0;
  if (std::isinf(d)) result = d > 0 ? -1 : 1;
  else {
    const double whole = std::trunc(d);
    result = compare_integers(negative, digits, whole < 0, integral_digits(whole));
    if (result == 0 && whole != d) result = d > whole ? -1 : 1;
  }
  return swapped ? -result : result;
}

// Python's three-way ordering (a < b, a > b); raises TypeError where Python
// does.
inline int compare(const Scalar& a, const Scalar& b) {
  if (a.kind == Scalar::Kind::kNA || b.kind == Scalar::Kind::kNA) {
    // pd.NA answers every comparison with NA, whose truth raises, except
    // with None, which neither orders.
    if (a.kind == Scalar::Kind::kNone || b.kind == Scalar::Kind::kNone) unorderable(a, b);
    throw TypeError("boolean value of NA is ambiguous");
  }
  if (is_number(a) && is_number(b)) return compare_numbers(a, b);
  if (a.kind == Scalar::Kind::kText && b.kind == Scalar::Kind::kText) {
    const int c = a.text.compare(b.text);
    return c < 0 ? -1 : c > 0 ? 1 : 0;
  }
  if (a.kind == Scalar::Kind::kDate && b.kind == Scalar::Kind::kDate) {
    if (a.date.timezone_aware != b.date.timezone_aware)
      throw TypeError("Cannot compare tz-naive and tz-aware timestamps");
    const auto x = std::pair(a.date.seconds, a.date.nanoseconds);
    const auto y = std::pair(b.date.seconds, b.date.nanoseconds);
    return x < y ? -1 : y < x ? 1 : 0;
  }
  unorderable(a, b);
}

inline bool less(const Scalar& a, const Scalar& b) { return compare(a, b) < 0; }

// CPython's list.sort for fewer than 64 items (an initial run, descending runs
// reversed, then binary insertion); the same stable result as timsort for any
// strict weak ordering, and the same comparisons where the ordering is not
// one (set-valued tuple members, TypeError operands).
template <class T, class Less>
void sort(std::vector<T>& items, Less less) {
  const std::size_t n = items.size();
  if (n < 2) return;
  std::size_t run = 2;
  if (less(items[1], items[0])) {
    while (run < n && less(items[run], items[run - 1])) ++run;
    std::reverse(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(run));
  } else {
    while (run < n && !less(items[run], items[run - 1])) ++run;
  }
  for (std::size_t i = run; i < n; ++i) {
    T pivot = std::move(items[i]);
    std::size_t low = 0, high = i;
    while (low < high) {
      const auto middle = low + ((high - low) >> 1);
      if (less(pivot, items[middle])) high = middle;
      else low = middle + 1;
    }
    for (std::size_t j = i; j > low; --j) items[j] = std::move(items[j - 1]);
    items[low] = std::move(pivot);
  }
}

// sorted(items, key=..., reverse=True): CPython reverses, sorts stably and
// reverses again, so equal keys keep their input order.
template <class T, class Less>
void sort_reverse(std::vector<T>& items, Less less) {
  std::reverse(items.begin(), items.end());
  sort(items, less);
  std::reverse(items.begin(), items.end());
}

// One group of a pandas groupby over object key columns.
struct Group {
  std::vector<Scalar> labels;      // the key tuple, as the result index holds it
  std::vector<std::size_t> rows;   // member rows in input order
};

// The order pandas' safe_sort gives one level's distinct values: sorted when
// they are mutually comparable; otherwise every non-string value (numbers and
// Timestamps), sorted, then the strings, sorted. Sorting the non-string
// values is numpy's argsort, so a Timestamp beside a number, or an aware
// beside a naive Timestamp, raises the TypeError of the first comparison
// numpy makes between them.
inline std::vector<std::size_t> safe_sort_order(const std::vector<Scalar>& uniques) {
  const auto category = [](const Scalar& s) {
    if (is_number(s)) return 0;
    if (s.kind == Scalar::Kind::kText) return 1;
    if (s.kind == Scalar::Kind::kDate) return s.date.timezone_aware ? 2 : 3;
    return 4;
  };
  std::vector<std::size_t> order(uniques.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  bool comparable = true;
  for (const auto& value : uniques) comparable = comparable && category(value) == category(uniques.front());
  const auto by_value = [&](std::size_t a, std::size_t b) { return compare(uniques[a], uniques[b]) < 0; };
  if (comparable) {
    std::stable_sort(order.begin(), order.end(), by_value);
    return order;
  }
  std::vector<std::size_t> others, strings;
  for (std::size_t i = 0; i < uniques.size(); ++i)
    (uniques[i].kind == Scalar::Kind::kText ? strings : others).push_back(i);
  const auto sorted_others = detail::numpy_object_quick_argsort(
      others.size(), [&](std::size_t a, std::size_t b) { return compare(uniques[others[a]], uniques[others[b]]); });
  std::stable_sort(strings.begin(), strings.end(), by_value);
  order.clear();
  for (const auto i : sorted_others) order.push_back(others[i]);
  order.insert(order.end(), strings.begin(), strings.end());
  return order;
}

// Whether a level of int and float labels becomes a float64 index: it needs
// a float, no bool, and integers numpy can hold beside it (all within int64,
// or all non-negative within uint64); otherwise it stays object.
inline bool float_level(const std::vector<Scalar>& labels) {
  bool floats = false, negative = false, above_int64 = false;
  for (const auto& label : labels) {
    if (label.kind == Scalar::Kind::kFloat) {
      floats = true;
      continue;
    }
    if (label.kind != Scalar::Kind::kInt) return false;
    if (label.digits == "0") continue;
    if (label.negative) {
      if (compare_digits(label.digits, "9223372036854775808") > 0) return false;
      negative = true;
    } else {
      if (compare_digits(label.digits, "18446744073709551615") > 0) return false;
      above_int64 = above_int64 || compare_digits(label.digits, "9223372036854775807") > 0;
    }
  }
  return floats && !(negative && above_int64);
}

// pandas groupby(keys) (sort=True, dropna=True) over object columns, as the
// reference iterates it. Each level is factorized over every row: its
// distinct values (None/NaN/NA excluded) are labelled by the first equal
// value, ordered by safe_sort_order (which may raise, also for values only
// rows dropped for another level's null carry) and typed by float_level
// (ints beside floats become floats, which can give two groups one label).
// Rows with a null key are dropped unless asked to keep them as their own
// last key; groups are ordered by their levels' value order.
inline std::vector<Group> groupby(const std::vector<std::vector<Scalar>>& keys, bool keep_nulls = false) {
  std::vector<Group> result;
  if (keys.empty()) return result;
  const std::size_t levels = keys.front().size();
  // Per level: distinct values in first-seen order, and each row's code.
  std::vector<std::vector<Scalar>> uniques(levels);
  std::vector<std::vector<std::size_t>> codes(levels, std::vector<std::size_t>(keys.size()));
  constexpr auto kNull = static_cast<std::size_t>(-1);
  for (std::size_t level = 0; level < levels; ++level) {
    std::map<std::string, std::size_t> seen;
    for (std::size_t row = 0; row < keys.size(); ++row) {
      const auto& value = keys[row][level];
      if (is_missing(value)) {
        codes[level][row] = kNull;
        continue;
      }
      const auto [it, added] = seen.emplace(identity(value), uniques[level].size());
      if (added) uniques[level].push_back(value);
      codes[level][row] = it->second;
    }
  }
  // Ranks in each level's sorted order; a kept null ranks last.
  std::vector<std::vector<std::size_t>> rank(levels);
  for (std::size_t level = 0; level < levels; ++level) {
    const auto order = safe_sort_order(uniques[level]);
    rank[level].resize(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) rank[level][order[i]] = i;
    if (float_level(uniques[level]))
      for (auto& label : uniques[level])
        if (label.kind == Scalar::Kind::kInt) label = real(std::strtod(str(label).c_str(), nullptr));
  }
  std::map<std::vector<std::size_t>, std::size_t> index;
  for (std::size_t row = 0; row < keys.size(); ++row) {
    std::vector<std::size_t> key(levels);
    bool drop = false;
    for (std::size_t level = 0; level < levels; ++level) {
      const auto code = codes[level][row];
      drop = drop || (code == kNull && !keep_nulls);
      key[level] = code == kNull ? uniques[level].size() : rank[level][code];
    }
    if (drop) continue;
    const auto [it, added] = index.emplace(key, result.size());
    if (added) {
      Group group;
      for (std::size_t level = 0; level < levels; ++level) {
        const auto code = codes[level][row];
        group.labels.push_back(code == kNull ? keys[row][level] : uniques[level][code]);
      }
      result.push_back(std::move(group));
    }
    result[it->second].rows.push_back(row);
  }
  // std::map orders the keys like the level codes: lexicographically. The
  // reference collects the groups by label, so two groups that float labels
  // made equal are one entry with the first key and the last group's rows.
  std::vector<Group> ordered;
  ordered.reserve(result.size());
  std::map<std::vector<std::string>, std::size_t> merged;
  for (const auto& [key, position] : index) {
    auto& group = result[position];
    std::vector<std::string> labels;
    for (const auto& label : group.labels) labels.push_back(identity(label));
    const auto [it, added] = merged.emplace(std::move(labels), ordered.size());
    if (added) ordered.push_back(std::move(group));
    else ordered[it->second].rows = std::move(group.rows);
  }
  return ordered;
}

}  // namespace minera::python
