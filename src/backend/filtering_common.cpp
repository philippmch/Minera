#include "minera/filtering_common.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <numbers>
#include <optional>

#include "root_cause_unicode.hpp"

namespace minera {
namespace {

[[noreturn]] void fail(std::string type, std::string message) {
  throw FilteringCommonError(std::move(type), std::move(message));
}

std::u32string decode_utf8(std::string_view text) {
  std::u32string out;
  for (std::size_t at = 0; at < text.size();) {
    const auto first = static_cast<unsigned char>(text[at++]);
    char32_t code = first;
    unsigned more = 0;
    if (first < 0x80) {
    } else if (first >= 0xc2 && first <= 0xdf) {
      code = first & 31;
      more = 1;
    } else if (first >= 0xe0 && first <= 0xef) {
      code = first & 15;
      more = 2;
    } else if (first >= 0xf0 && first <= 0xf4) {
      code = first & 7;
      more = 3;
    } else {
      throw std::invalid_argument("timestamp text is not valid UTF-8");
    }
    for (unsigned i = 0; i < more; ++i) {
      if (at == text.size() || (static_cast<unsigned char>(text[at]) & 0xc0) != 0x80)
        throw std::invalid_argument("timestamp text is not valid UTF-8");
      code = (code << 6) | (static_cast<unsigned char>(text[at++]) & 63);
    }
    out.push_back(code);
  }
  return out;
}

std::string encode_utf8(std::u32string_view text) {
  std::string out;
  for (const char32_t c : text) {
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (c < 0x800) {
      out += static_cast<char>(0xc0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 63));
    } else if (c < 0x10000) {
      out += static_cast<char>(0xe0 | (c >> 12));
      out += static_cast<char>(0x80 | ((c >> 6) & 63));
      out += static_cast<char>(0x80 | (c & 63));
    } else {
      out += static_cast<char>(0xf0 | (c >> 18));
      out += static_cast<char>(0x80 | ((c >> 12) & 63));
      out += static_cast<char>(0x80 | ((c >> 6) & 63));
      out += static_cast<char>(0x80 | (c & 63));
    }
  }
  return out;
}

// repr() of a Python str.
std::string quoted(std::u32string_view value) {
  const bool single = value.find(U'\'') != std::u32string_view::npos && value.find(U'"') == std::u32string_view::npos;
  const char32_t quote = single ? U'"' : U'\'';
  constexpr char hex[] = "0123456789abcdef";
  std::string result(1, static_cast<char>(quote));
  for (const char32_t code : value) {
    if (code == quote || code == U'\\') {
      result += '\\';
      result += static_cast<char>(code);
    } else if (code == U'\n') {
      result += "\\n";
    } else if (code == U'\r') {
      result += "\\r";
    } else if (code == U'\t') {
      result += "\\t";
    } else if (!root_cause_unicode_printable(code)) {
      const int digits = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) result += hex[(code >> shift) & 15];
    } else {
      result += encode_utf8(std::u32string_view(&code, 1));
    }
  }
  return result + static_cast<char>(quote);
}

bool python_space(char32_t c) {
  return c == U' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || root_cause_unicode_space(c);
}

// The strptime expression of '%Y-%m-%d %H:%M:%S', matched like re.match:
// alternatives in order, greedy whitespace, backtracking.
struct Atom {
  enum Kind { kLiteral, kRange, kDigit } kind;
  char32_t low = 0;
  char32_t high = 0;
};
using Alternative = std::vector<Atom>;
struct Element {
  enum Kind { kGroup, kLiteral, kSpaces } kind;
  std::vector<Alternative> alternatives;
  char32_t literal = 0;
};

Atom lit(char32_t c) { return {Atom::kLiteral, c, c}; }
Atom range(char32_t a, char32_t b) { return {Atom::kRange, a, b}; }
Atom digit() { return {Atom::kDigit}; }

const std::vector<Element>& strptime_pattern() {
  static const std::vector<Element> pattern = [] {
    std::vector<Element> p;
    p.push_back({Element::kGroup, {{digit(), digit(), digit(), digit()}}});
    p.push_back({Element::kLiteral, {}, U'-'});
    p.push_back({Element::kGroup, {{lit('1'), range('0', '2')}, {lit('0'), range('1', '9')}, {range('1', '9')}}});
    p.push_back({Element::kLiteral, {}, U'-'});
    p.push_back({Element::kGroup,
                 {{lit('3'), range('0', '1')}, {range('1', '2'), digit()}, {lit('0'), range('1', '9')}, {range('1', '9')},
                  {lit(' '), range('1', '9')}}});
    p.push_back({Element::kSpaces, {}, 0});
    p.push_back({Element::kGroup, {{lit('2'), range('0', '3')}, {range('0', '1'), digit()}, {digit()}}});
    p.push_back({Element::kLiteral, {}, U':'});
    p.push_back({Element::kGroup, {{range('0', '5'), digit()}, {digit()}}});
    p.push_back({Element::kLiteral, {}, U':'});
    p.push_back({Element::kGroup, {{lit('6'), range('0', '1')}, {range('0', '5'), digit()}, {digit()}}});
    return p;
  }();
  return pattern;
}

bool atom_matches(const Atom& atom, char32_t c) {
  switch (atom.kind) {
    case Atom::kLiteral: return c == atom.low;
    case Atom::kRange: return c >= atom.low && c <= atom.high;
    case Atom::kDigit: return root_cause_unicode_decimal(c) >= 0;
  }
  return false;
}

bool match_from(const std::u32string& s, std::size_t element, std::size_t at, std::vector<std::u32string>& groups,
                std::size_t& end) {
  const auto& pattern = strptime_pattern();
  if (element == pattern.size()) {
    end = at;
    return true;
  }
  const auto& e = pattern[element];
  if (e.kind == Element::kLiteral) {
    return at < s.size() && s[at] == e.literal && match_from(s, element + 1, at + 1, groups, end);
  }
  if (e.kind == Element::kSpaces) {
    std::size_t stop = at;
    while (stop < s.size() && python_space(s[stop])) ++stop;
    for (std::size_t to = stop; to > at; --to)
      if (match_from(s, element + 1, to, groups, end)) return true;
    return false;
  }
  for (const auto& alternative : e.alternatives) {
    if (at + alternative.size() > s.size()) continue;
    bool ok = true;
    for (std::size_t i = 0; i < alternative.size() && ok; ++i) ok = atom_matches(alternative[i], s[at + i]);
    if (!ok) continue;
    groups.push_back(s.substr(at, alternative.size()));
    if (match_from(s, element + 1, at + alternative.size(), groups, end)) return true;
    groups.pop_back();
  }
  return false;
}

int python_int(const std::u32string& digits) {
  int value = 0;
  for (const char32_t c : digits) {
    if (c == U' ') continue;
    value = value * 10 + root_cause_unicode_decimal(c);
  }
  return value;
}

bool leap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }
int days_in_month(int year, int month) {
  static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && leap(year) ? 29 : days[month - 1];
}

// Days from 1970-01-01 of a proleptic Gregorian date.
std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

std::int64_t wall_seconds(std::int64_t y, int m, int d, int hh, int mm, int ss) {
  return days_from_civil(y, static_cast<unsigned>(m), static_cast<unsigned>(d)) * 86400 + hh * 3600 + mm * 60 + ss;
}

// local(u) of CPython's datetime: the wall clock of POSIX second u under the
// process time zone, as seconds.
std::int64_t local_wall(std::int64_t u) {
  const auto t = static_cast<std::time_t>(u);
  std::tm fields{};
  if (!localtime_r(&t, &fields)) fail("OverflowError", "timestamp out of range for platform time_t");
  return wall_seconds(fields.tm_year + 1900LL, fields.tm_mon + 1, fields.tm_mday, fields.tm_hour, fields.tm_min,
                      fields.tm_sec);
}

// datetime.timestamp() of a naive datetime (fold 0): CPython's local_to_seconds.
double naive_timestamp(const FilteringDateTime& value) {
  constexpr std::int64_t max_fold_seconds = 24 * 3600;
  const std::int64_t t = wall_seconds(value.year, value.month, value.day, value.hour, value.minute, value.second);
  const std::int64_t a = local_wall(t) - t;
  const std::int64_t u1 = t - a;
  const std::int64_t t1 = local_wall(u1);
  std::int64_t b = 0;
  std::int64_t seconds = 0;
  bool done = false;
  if (t1 == t) {
    const std::int64_t u2 = u1 - max_fold_seconds;
    b = local_wall(u2) - u2;
    if (a == b) {
      seconds = u1;
      done = true;
    }
  } else {
    b = t1 - u1;
  }
  if (!done) {
    const std::int64_t u2 = t - b;
    const std::int64_t t2 = local_wall(u2);
    if (t2 == t) {
      seconds = u2;
    } else if (t1 == t) {
      seconds = u1;
    } else {
      seconds = std::max(u1, u2);
    }
  }
  return static_cast<double>(seconds) + value.microsecond / 1e6;
}

// ---------------------------------------------------------------- numerics

[[noreturn]] void samples_error(std::int64_t num) {
  fail("ValueError", "Number of samples, " + std::to_string(num) + ", must be non-negative.");
}

// numpy.linspace(start, stop, num) for float64 scalars.
std::vector<double> linspace(double start, double stop, std::int64_t num) {
  if (num < 0) samples_error(num);
  std::vector<double> y(static_cast<std::size_t>(num));
  const std::int64_t div = num - 1;
  const double delta = stop - start;
  if (div > 0) {
    const double step = delta / static_cast<double>(div);
    if (step == 0) {
      for (std::int64_t i = 0; i < num; ++i) y[i] = static_cast<double>(i) / static_cast<double>(div) * delta + start;
    } else {
      for (std::int64_t i = 0; i < num; ++i) y[i] = static_cast<double>(i) * step + start;
    }
  } else {
    for (std::int64_t i = 0; i < num; ++i) y[i] = static_cast<double>(i) * delta + start;
  }
  if (num > 1) y.back() = stop;
  return y;
}

// numpy.geomspace(start, stop, num) for float64 scalars of one sign.
std::vector<double> geomspace(double start, double stop, std::int64_t num) {
  if (num < 0) samples_error(num);
  const double sign = start < 0 ? -1.0 : 1.0;
  const double first = start / sign;
  const double last = stop / sign;
  auto exponents = linspace(std::log10(first), std::log10(last), num);
  for (auto& value : exponents) value = std::pow(10.0, value);
  if (num > 0) {
    exponents.front() = first;
    if (num > 1) exponents.back() = last;
  }
  for (auto& value : exponents) value *= sign;
  return exponents;
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  std::int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

// numpy's pairwise float64 summation (blocks of 8 accumulators up to 128
// elements, recursive halving above).
double pairwise_sum(const double* a, std::size_t n) {
  if (n < 8) {
    double res = 0.0;
    for (std::size_t i = 0; i < n; ++i) res += a[i];
    return res;
  }
  if (n <= 128) {
    double r[8];
    for (int k = 0; k < 8; ++k) r[k] = a[k];
    std::size_t i = 8;
    for (; i < n - n % 8; i += 8)
      for (int k = 0; k < 8; ++k) r[k] += a[i + k];
    double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; ++i) res += a[i];
    return res;
  }
  std::size_t half = n / 2;
  half -= half % 8;
  return pairwise_sum(a, half) + pairwise_sum(a + half, n - half);
}
double pairwise_sum(const std::vector<double>& a) { return pairwise_sum(a.data(), a.size()); }

// scipy.stats.gaussian_kde(data)(points), one dimension, Scott's factor:
// numpy.cov with uniform aweights (its matrix product's summation order is
// BLAS-dependent, so the covariance can differ in the last bits), then the
// triangular rescaling by the reciprocal of the scaled Cholesky factor and
// the kernel sum in data order.
std::vector<double> gaussian_kde(const std::vector<double>& data, const std::vector<double>& points) {
  const std::size_t n = data.size();
  if (n <= 1) fail("ValueError", "`dataset` input should have multiple elements.");
  const std::vector<double> weights(n, 1.0 / static_cast<double>(n));
  std::vector<double> scratch(n);
  for (std::size_t i = 0; i < n; ++i) scratch[i] = weights[i] * weights[i];
  const double n_eff = 1.0 / pairwise_sum(scratch);
  const double w_sum = pairwise_sum(weights);
  for (std::size_t i = 0; i < n; ++i) scratch[i] = data[i] * weights[i];
  const double average = pairwise_sum(scratch) / w_sum;
  for (std::size_t i = 0; i < n; ++i) scratch[i] = weights[i] * weights[i];
  const double fact = w_sum - pairwise_sum(scratch) / w_sum;
  double spread = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double centred = data[i] - average;
    spread += centred * (centred * weights[i]);
  }
  const double covariance = spread / fact;
  if (!std::isfinite(covariance)) fail("ValueError", "array must not contain infs or NaNs");
  if (!(covariance > 0)) fail("LinAlgError", "1-th leading minor of the array is not positive definite");
  const double factor = std::pow(n_eff, -1.0 / 5.0);
  const double cho = std::sqrt(covariance) * factor;
  const double inverse = 1.0 / cho;
  const double norm = std::pow(2 * std::numbers::pi, -0.5) / cho;
  std::vector<double> scaled(n);
  for (std::size_t i = 0; i < n; ++i) scaled[i] = data[i] * inverse;
  std::vector<double> estimate(points.size(), 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < points.size(); ++j) {
      const double residual = scaled[i] - points[j] * inverse;
      estimate[j] += weights[i] * (std::exp(-(residual * residual) / 2) * norm);
    }
  }
  return estimate;
}

void require_finite(const std::vector<double>& values) {
  for (const double v : values)
    if (!std::isfinite(v)) fail("ValueError", "array must not contain infs or NaNs");
}

std::size_t distinct_count(const std::vector<double>& values) {
  std::vector<double> seen;
  std::size_t nan = 0;
  for (const double v : values) {
    if (std::isnan(v)) {
      ++nan;
      continue;
    }
    if (std::find(seen.begin(), seen.end(), v) == seen.end()) seen.push_back(v);
  }
  return seen.size() + nan;
}

std::vector<double> single_spike(std::int64_t graph_points) {
  std::vector<double> y(static_cast<std::size_t>(graph_points), 0.0);
  const auto index = floor_div(graph_points, 2);
  if (y.empty()) fail("IndexError", "index 0 is out of bounds for axis 0 with size 0");
  y[static_cast<std::size_t>(index)] = 1.0;
  return y;
}

std::string json_pairs(const std::vector<std::string>& x, const std::vector<double>& y) {
  std::string out = "[";
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (i) out += ", ";
    out += "[" + x[i] + ", " + filtering_float_repr(y[i]) + "]";
  }
  return out + "]";
}

// Python's round(value, 6).
double round6(double value) {
  if (!std::isfinite(value)) return value;
  char buffer[512];
  std::snprintf(buffer, sizeof buffer, "%.6f", value);
  return std::strtod(buffer, nullptr);
}

// The correctly rounded quotient ns / 10**9 (Python int true division).
double ns_to_seconds(std::int64_t ns) {
  const bool negative = ns < 0;
  const auto magnitude = negative ? static_cast<std::uint64_t>(-(ns + 1)) + 1 : static_cast<std::uint64_t>(ns);
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%s%llu.%09llu", negative ? "-" : "",
                static_cast<unsigned long long>(magnitude / 1000000000ULL),
                static_cast<unsigned long long>(magnitude % 1000000000ULL));
  return std::strtod(buffer, nullptr);
}

std::int64_t seconds_to_ns(double seconds) {
  const double scaled = seconds * 1e9;
  if (!std::isfinite(scaled) || scaled >= 9.2233720368547758e18 || scaled < -9.2233720368547758e18)
    return std::numeric_limits<std::int64_t>::min();
  return static_cast<std::int64_t>(scaled);
}

// Exact comparison of a Python int with a float: -1, 0 or 1; nullopt for NaN.
std::optional<int> compare_int_float(std::int64_t value, double bound) {
  if (std::isnan(bound)) return std::nullopt;
  if (bound >= 9.2233720368547758e18) return -1;
  if (bound < -9.2233720368547758e18) return 1;
  const double floor_bound = std::floor(bound);
  const auto integral = static_cast<std::int64_t>(floor_bound);
  if (value != integral) return value < integral ? -1 : 1;
  return floor_bound < bound ? -1 : 0;
}

bool number_greater(const FilteringNumber& value, double bound) {
  return value.integral ? compare_int_float(value.integer, bound) == 1 : value.real > bound;
}

double as_double(const FilteringNumber& value) {
  return value.integral ? static_cast<double>(value.integer) : value.real;
}

// Python a < b over int/float counts.
bool number_less(const FilteringNumber& a, const FilteringNumber& b) {
  if (a.integral && b.integral) return a.integer < b.integer;
  if (!a.integral && !b.integral) return a.real < b.real;
  if (a.integral) return compare_int_float(a.integer, b.real) == -1;
  return compare_int_float(b.integer, a.real) == 1;
}

}  // namespace

// ---------------------------------------------------------------- floats

std::string filtering_float_repr(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
  if (value == 0) return std::signbit(value) ? "-0.0" : "0.0";
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::scientific);
  std::string text(buffer, result.ptr);
  std::string sign;
  if (text[0] == '-') {
    sign = "-";
    text.erase(0, 1);
  }
  const auto e = text.find('e');
  std::string digits = text.substr(0, e);
  digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
  const int exponent = std::stoi(text.substr(e + 1));
  const int decpt = exponent + 1;
  std::string out;
  if (decpt > -4 && decpt <= 16) {
    if (decpt <= 0) {
      out = "0." + std::string(static_cast<std::size_t>(-decpt), '0') + digits;
    } else if (static_cast<std::size_t>(decpt) >= digits.size()) {
      out = digits + std::string(static_cast<std::size_t>(decpt) - digits.size(), '0') + ".0";
    } else {
      out = digits.substr(0, static_cast<std::size_t>(decpt)) + "." + digits.substr(static_cast<std::size_t>(decpt));
    }
  } else {
    out = digits.substr(0, 1);
    if (digits.size() > 1) out += "." + digits.substr(1);
    const int shown = decpt - 1;
    out += shown < 0 ? "e-" : "e+";
    const auto magnitude = std::to_string(shown < 0 ? -shown : shown);
    out += magnitude.size() < 2 ? "0" + magnitude : magnitude;
  }
  return sign + out;
}

// ---------------------------------------------------------------- timestamp_common

FilteringDateTime get_dt_from_string(std::string_view text) {
  const auto data = decode_utf8(text);
  std::vector<std::u32string> groups;
  std::size_t end = 0;
  if (!match_from(data, 0, 0, groups, end))
    fail("ValueError", "time data " + quoted(data) + " does not match format '%Y-%m-%d %H:%M:%S'");
  if (end != data.size()) fail("ValueError", "unconverted data remains: " + encode_utf8(data.substr(end)));
  FilteringDateTime out;
  out.year = python_int(groups[0]);
  out.month = python_int(groups[1]);
  out.day = python_int(groups[2]);
  out.hour = python_int(groups[3]);
  out.minute = python_int(groups[4]);
  out.second = python_int(groups[5]);
  if (out.year < 1 || out.year > 9999) fail("ValueError", "year " + std::to_string(out.year) + " is out of range");
  if (out.day > days_in_month(out.year, out.month)) fail("ValueError", "day is out of range for month");
  if (out.second > 59) fail("ValueError", "second must be in 0..59");
  return out;
}

FilteringDateTime get_dt_from_string(const FilteringDtArgument& argument) {
  switch (argument.kind) {
    case FilteringDtArgument::Kind::kString: return get_dt_from_string(argument.text);
    case FilteringDtArgument::Kind::kDateTime: return argument.datetime;
    case FilteringDtArgument::Kind::kOther: break;
  }
  if (argument.text == "bytes" || argument.text == "bytearray")
    fail("TypeError", argument.text + ".replace() takes no keyword arguments");
  fail("AttributeError", "'" + argument.text + "' object has no attribute 'replace'");
}

// ---------------------------------------------------------------- attributes_common

FilteringAttributeCounts get_sorted_attributes_list(const FilteringAttributeCounts& attributes) {
  auto sorted = attributes;
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const auto& a, const auto& b) { return number_less(b.second, a.second); });
  return sorted;
}

FilteringNumber get_attributes_threshold(const FilteringAttributeCounts& alist, double decreasing_factor,
                                         std::int64_t min_activity_count, std::int64_t max_activity_count) {
  const auto length = static_cast<std::int64_t>(alist.size());
  const std::int64_t first = min_activity_count == std::numeric_limits<std::int64_t>::min()
                                 ? std::numeric_limits<std::int64_t>::min()
                                 : min_activity_count - 1;
  std::int64_t index = std::max<std::int64_t>(0, std::min(first, length - 1));
  if (index >= length) fail("IndexError", "list index out of range");
  FilteringNumber threshold = alist[static_cast<std::size_t>(index)].second;
  for (std::int64_t i = index + 1; i < length; ++i) {
    const auto& value = alist[static_cast<std::size_t>(i)].second;
    if (number_greater(value, as_double(threshold) * decreasing_factor)) threshold = value;
    if (i >= max_activity_count) break;
  }
  return threshold;
}

FilteringNumericKde get_kde_numeric_attribute(const std::vector<double>& values, std::int64_t graph_points) {
  FilteringNumericKde out;
  if (values.empty()) return out;
  if (distinct_count(values) == 1) {
    const double v = values.front();
    double low = -1;
    double high = 1;
    if (v != 0) {
      const double delta = std::max(std::fabs(v) * 0.01, 1e-6);
      low = v - delta;
      high = v + delta;
    }
    out.x = linspace(low, high, graph_points);
    out.y = single_spike(graph_points);
    return out;
  }
  auto data = values;
  std::sort(data.begin(), data.end());
  const double low = data.front();
  const double high = data.back();
  const std::int64_t count = std::max<std::int64_t>(floor_div(graph_points, 2), 2);
  auto grid = linspace(low, high, count);
  if ((low > 0 && high > 0) || (low < 0 && high < 0)) {
    const auto geometric = geomspace(low, high, count);
    grid.insert(grid.end(), geometric.begin(), geometric.end());
  }
  std::sort(grid.begin(), grid.end());
  grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
  require_finite(data);
  out.y = gaussian_kde(data, grid);
  out.x = std::move(grid);
  return out;
}

std::string get_kde_numeric_attribute_json(const std::vector<double>& values, std::int64_t graph_points) {
  const auto kde = get_kde_numeric_attribute(values, graph_points);
  std::vector<std::string> x;
  for (const double v : kde.x) x.push_back(filtering_float_repr(v));
  return json_pairs(x, kde.y);
}

FilteringDateKde get_kde_date_attribute(const std::vector<FilteringDateTime>& values, std::int64_t graph_points,
                                        std::int64_t points_to_sample) {
  if (values.empty()) fail("IndexError", "list index out of range");
  std::vector<double> seconds;
  seconds.reserve(values.size());
  for (const auto& value : values) seconds.push_back(naive_timestamp(value));
  FilteringDateKde out;
  bool single = true;
  for (const auto& value : values) single = single && value == values.front();
  if (single) {
    const double s = seconds.front();
    const auto grid = linspace(s - 3600, s + 3600, graph_points);
    out.y = single_spike(graph_points);
    for (const double v : grid) out.x_ns.push_back(seconds_to_ns(v));
    return out;
  }
  const auto n = static_cast<std::int64_t>(values.size());
  const std::int64_t k = points_to_sample;
  std::vector<std::int64_t> positions;
  if (k >= n) {
    for (std::int64_t i = 0; i < k; ++i) positions.push_back(floor_div(i * n, k));
  } else {
    for (std::int64_t i = 0; i < k; ++i) positions.push_back(floor_div(n, 2 * k) + floor_div(i * n, k));
    positions.push_back(0);
    positions.push_back(n - 1);
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
  }
  std::vector<double> data;
  for (const auto p : positions) data.push_back(seconds[static_cast<std::size_t>(p)]);
  std::sort(data.begin(), data.end());
  const auto grid = linspace(data.front(), data.back(), graph_points);
  out.y = gaussian_kde(data, grid);
  for (const double v : grid) out.x_ns.push_back(seconds_to_ns(v));
  return out;
}

std::string get_kde_date_attribute_json(const std::vector<FilteringDateTime>& values, std::int64_t graph_points,
                                        std::int64_t points_to_sample) {
  const auto kde = get_kde_date_attribute(values, graph_points, points_to_sample);
  std::vector<std::string> x;
  for (const auto ns : kde.x_ns) x.push_back(filtering_float_repr(round6(ns_to_seconds(ns))));
  return json_pairs(x, kde.y);
}

}  // namespace minera
