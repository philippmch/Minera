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

// str() of the value.
inline std::string str(const Scalar& s) {
  switch (s.kind) {
    case Scalar::Kind::kNone: return "None";
    case Scalar::Kind::kNA: return "<NA>";
    case Scalar::Kind::kBool: return s.boolean ? "True" : "False";
    case Scalar::Kind::kInt: return (s.negative ? "-" : "") + s.digits;
    case Scalar::Kind::kFloat: return float_repr(s.real);
    case Scalar::Kind::kText: return s.text;
    case Scalar::Kind::kDate: return s.text;
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
  if (a.kind == Scalar::Kind::kNA || b.kind == Scalar::Kind::kNA)
    throw TypeError("boolean value of NA is ambiguous");
  if (is_number(a) && is_number(b)) return compare_numbers(a, b);
  if (a.kind == Scalar::Kind::kText && b.kind == Scalar::Kind::kText) {
    const int c = a.text.compare(b.text);
    return c < 0 ? -1 : c > 0 ? 1 : 0;
  }
  if (a.kind == Scalar::Kind::kDate && b.kind == Scalar::Kind::kDate) {
    if (a.date.timezone_aware != b.date.timezone_aware)
      throw TypeError("can't compare offset-naive and offset-aware datetimes");
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

// pandas groupby(keys) (sort=True, dropna=True) over object columns, as the
// reference iterates it: rows with a None/NaN/NA key are dropped; a level's
// label is the first value equal to it in that column; groups are ordered by
// their key tuples with numbers before text (and null keys, kept only when
// asked, last); a level whose labels are all int or float with at least one
// float becomes float64, which rounds wide integers, and two groups whose
// rounded tuples collide are one dictionary entry with the first key and the
// last group's rows.
inline std::vector<Group> groupby(const std::vector<std::vector<Scalar>>& keys, bool keep_nulls = false) {
  std::vector<Group> result;
  if (keys.empty()) return result;
  const std::size_t levels = keys.front().size();
  std::vector<std::map<std::string, Scalar>> first_seen(levels);
  std::map<std::vector<std::string>, std::size_t> index;
  std::vector<Group> groups;
  for (std::size_t row = 0; row < keys.size(); ++row) {
    bool drop = false;
    std::vector<std::string> identities;
    std::vector<Scalar> labels;
    for (std::size_t level = 0; level < levels; ++level) {
      const auto& value = keys[row][level];
      drop = drop || (is_missing(value) && !keep_nulls);
      auto id = identity(value);
      const auto [it, added] = first_seen[level].emplace(id, value);
      labels.push_back(it->second);
      identities.push_back(std::move(id));
    }
    if (drop) continue;
    const auto [it, added] = index.emplace(identities, groups.size());
    if (added) groups.push_back({std::move(labels), {}});
    groups[it->second].rows.push_back(row);
  }
  // Ordered by key tuple: safe_sort puts every number before every string.
  const auto category = [](const Scalar& s) {
    if (is_number(s)) return 0;
    if (s.kind == Scalar::Kind::kText) return 1;
    if (s.kind == Scalar::Kind::kDate) return 2;
    return 3;
  };
  const auto less_scalar = [&](const Scalar& a, const Scalar& b) {
    const int ca = category(a), cb = category(b);
    if (ca != cb) return ca < cb;
    if (ca == 3) return false;
    try {
      return compare(a, b) < 0;
    } catch (const TypeError&) {
      return identity(a) < identity(b);
    }
  };
  std::stable_sort(groups.begin(), groups.end(), [&](const Group& a, const Group& b) {
    for (std::size_t level = 0; level < levels; ++level) {
      if (less_scalar(a.labels[level], b.labels[level])) return true;
      if (less_scalar(b.labels[level], a.labels[level])) return false;
    }
    return false;
  });
  // Index inference per level: ints alongside floats become float64.
  for (std::size_t level = 0; level < levels; ++level) {
    bool numbers = true, floats = false;
    for (const auto& group : groups) {
      const auto& label = group.labels[level];
      if (label.kind == Scalar::Kind::kFloat) floats = true;
      else if (label.kind != Scalar::Kind::kInt) numbers = false;
    }
    if (!numbers || !floats) continue;
    for (auto& group : groups) {
      auto& label = group.labels[level];
      if (label.kind == Scalar::Kind::kInt) label = real(std::strtod(str(label).c_str(), nullptr));
    }
  }
  std::map<std::vector<std::string>, std::size_t> merged;
  for (auto& group : groups) {
    std::vector<std::string> key;
    for (const auto& label : group.labels) key.push_back(identity(label));
    const auto [it, added] = merged.emplace(std::move(key), result.size());
    if (added) result.push_back(std::move(group));
    else result[it->second].rows = std::move(group.rows);
  }
  return result;
}

}  // namespace minera::python
