#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {

// A public reference failure: `type` is the Python exception class name and
// `message` its str().
class FilteringCommonError : public std::invalid_argument {
 public:
  FilteringCommonError(std::string type, std::string message)
      : std::invalid_argument(message),
        type(std::move(type)),
        message(std::move(message)) {}
  const std::string type;
  const std::string message;
};

// algo.filtering.common.filtering_constants
inline constexpr std::string_view kFilteringCaseConceptName = "case:concept:name";
inline constexpr double kFilteringDecreasingFactor = 0.6;
inline constexpr double kFilteringDefaultNoiseThreshDf = 0.16;

// A datetime by its wall-clock fields.
struct FilteringDateTime {
  int year = 1;
  int month = 1;
  int day = 1;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int microsecond = 0;
  friend bool operator==(const FilteringDateTime&, const FilteringDateTime&) = default;
};

// algo.filtering.common.timestamp.timestamp_common.get_dt_from_string.
// A string is parsed with the strptime format '%Y-%m-%d %H:%M:%S' (Unicode
// decimal digits and whitespace as Python's re matches them); a datetime keeps
// its wall clock. The result is the UTC-aware datetime with those fields.
// Any other argument raises the AttributeError of its `replace` lookup, named
// by its Python type (`bytes` raises the TypeError of bytes.replace).
struct FilteringDtArgument {
  enum class Kind { kString, kDateTime, kOther };
  Kind kind = Kind::kString;
  std::string text;          // kString: UTF-8 text; kOther: Python type name
  FilteringDateTime datetime;  // kDateTime
};
[[nodiscard]] FilteringDateTime get_dt_from_string(std::string_view text);
[[nodiscard]] FilteringDateTime get_dt_from_string(const FilteringDtArgument& argument);

// algo.filtering.common.attributes.attributes_common
// A count as the reference keeps it: a Python int or float.
struct FilteringNumber {
  bool integral = true;
  std::int64_t integer = 0;
  double real = 0.0;
  [[nodiscard]] static FilteringNumber of(std::int64_t value) { return {true, value, 0.0}; }
  [[nodiscard]] static FilteringNumber of(double value) { return {false, 0, value}; }
  friend bool operator==(const FilteringNumber& a, const FilteringNumber& b) {
    return a.integral == b.integral && (a.integral ? a.integer == b.integer : a.real == b.real);
  }
};
using FilteringAttributeCounts = std::vector<std::pair<std::string, FilteringNumber>>;

// Stable descending order by count; ties keep the mapping order.
[[nodiscard]] FilteringAttributeCounts get_sorted_attributes_list(const FilteringAttributeCounts& attributes);

// Starts at the count at index max(0, min(min_activity_count - 1, len - 1))
// and walks the later entries in sequence order, replacing the threshold by
// every count strictly greater than threshold * decreasing_factor and stopping
// after the index max_activity_count. An empty list raises IndexError.
[[nodiscard]] FilteringNumber get_attributes_threshold(const FilteringAttributeCounts& alist,
                                                       double decreasing_factor,
                                                       std::int64_t min_activity_count = 1,
                                                       std::int64_t max_activity_count = 25);

// Numeric KDE: x is the sorted union of numpy linspace and geomspace grids of
// max(graph_points // 2, 2) points between the extremes (linspace only when
// the range touches or crosses zero, a negated geomspace for negative
// ranges); y is scipy's gaussian_kde (Scott bandwidth) of the sorted values at
// x. One distinct value v gives graph_points points over v -/+ max(|v| / 100,
// 1e-6) (-1..1 for zero) with a single 1 at index graph_points // 2.
struct FilteringNumericKde {
  std::vector<double> x;
  std::vector<double> y;
};
[[nodiscard]] FilteringNumericKde get_kde_numeric_attribute(const std::vector<double>& values,
                                                            std::int64_t graph_points = 200);
// json.dumps of [[x, y], ...].
[[nodiscard]] std::string get_kde_numeric_attribute_json(const std::vector<double>& values,
                                                         std::int64_t graph_points = 200);

// Date KDE over naive wall clocks (an aware datetime contributes its wall
// clock). Values become local-time POSIX seconds (datetime.timestamp() under
// the process time zone); the positions floor(i * n / points_to_sample) are
// kept when points_to_sample >= n, otherwise n // (2k) + floor(i * n / k) plus
// the first and last positions; the kept seconds are sorted. x holds
// graph_points evenly spaced seconds between the kept extremes as
// datetime64[ns] values (truncated seconds * 1e9), y the KDE at those seconds.
// One distinct value s spans s -/+ 3600 seconds with a single 1 at index
// graph_points // 2.
struct FilteringDateKde {
  std::vector<std::int64_t> x_ns;
  std::vector<double> y;
};
[[nodiscard]] FilteringDateKde get_kde_date_attribute(const std::vector<FilteringDateTime>& values,
                                                      std::int64_t graph_points = 200,
                                                      std::int64_t points_to_sample = 400);
// json.dumps of [[Timestamp(x).timestamp(), y], ...].
[[nodiscard]] std::string get_kde_date_attribute_json(const std::vector<FilteringDateTime>& values,
                                                      std::int64_t graph_points = 200,
                                                      std::int64_t points_to_sample = 400);

// Python repr of a float, as json.dumps writes it (NaN, Infinity, -Infinity).
[[nodiscard]] std::string filtering_float_repr(double value);

}  // namespace minera
