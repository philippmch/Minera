#include "minera/attribute_kde.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>

#include "minera/dataframe_columns.hpp"
#include "minera/filtering_common.hpp"
#include "minera/timestamp.hpp"
#include "frame_cells.hpp"
#include "ocel_frame_types.hpp"

namespace minera {
namespace {

const std::string* event_value(const Event& event, std::string_view key) {
  if (key == "activity" || key == "concept:name") return &event.activity;
  if (key == "timestamp" || key == "time:timestamp") return &event.timestamp;
  if (key == "resource" || key == "org:resource") return &event.resource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return &event.lifecycle;
  return event.attribute(key);
}

double parse_number(std::string_view value, std::string_view key) {
  std::string copy(value);
  char* end = nullptr;
  const double parsed = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || end != copy.c_str() + copy.size() ||
      !std::isfinite(parsed)) {
    throw std::invalid_argument("attribute KDE: invalid numeric value for '" +
                                std::string(key) + "'");
  }
  return parsed;
}

// Timestamp.timestamp(): the instant's whole microseconds divided by 1e6,
// rounded once (summing seconds and a fraction can land one ulp away, which
// moves a narrow density in its ninth digit).
double parse_date(std::string_view value, std::string_view key) {
  const auto parsed = parse_timestamp_microseconds(value);
  if (!parsed) {
    throw std::invalid_argument("attribute KDE: invalid date value for '" +
                                std::string(key) + "'");
  }
  return static_cast<double>(*parsed) / 1e6;
}

// pm4py.statistics.attributes.pandas.get.get_kde_numeric_attribute and
// get_kde_date_attribute, the DataFrame path CSV and XES inputs take: the
// column's values go to the common estimators sorted (the numeric one sorts
// them anyway; the date one picks its points_to_sample positions from the
// sorted list). Densities are scipy's gaussian_kde with Scott's factor.
AttributeKdeResult make_result(const AttributeKdeOptions& options,
                               std::vector<double> values) {
  AttributeKdeResult result;
  result.variant = options.variant;
  result.attribute_key = options.attribute_key;
  // An empty numeric column gives empty axes; the date estimator indexes the
  // first value and fails (IndexError).
  if (values.empty() && options.variant == AttributeKdeVariant::kNumeric)
    return result;
  if (values.empty())
    throw std::invalid_argument("attribute KDE: no date values for '" +
                                options.attribute_key + "'");
  std::sort(values.begin(), values.end());
  const auto graph_points = static_cast<std::int64_t>(options.graph_points);
  if (options.variant == AttributeKdeVariant::kDate) {
    const auto kde = get_kde_date_attribute_seconds(
        values, graph_points,
        static_cast<std::int64_t>(options.points_to_sample));
    result.x.reserve(kde.x_ns.size());
    for (const auto ns : kde.x_ns)
      result.x.push_back(static_cast<double>(ns) / 1e9);
    result.y = kde.y;
  } else {
    auto kde = get_kde_numeric_attribute(values, graph_points);
    result.x = std::move(kde.x);
    result.y = std::move(kde.y);
  }
  return result;
}

// A Python number as float() gives it.
double python_float(const python::Scalar& value) {
  switch (value.kind) {
    case python::Scalar::Kind::kBool: return value.boolean ? 1.0 : 0.0;
    case python::Scalar::Kind::kFloat: return value.real;
    default: break;
  }
  const double magnitude = std::strtod(value.digits.c_str(), nullptr);
  return value.negative ? -magnitude : magnitude;
}

// A frame Timestamp's .timestamp() (UTC; replace(tzinfo=None) keeps the
// wall clock, which a frame's datetime column holds as UTC).
double frame_timestamp_seconds(const python::Scalar& value) {
  return static_cast<double>(value.date.seconds * 1000000 + value.date.nanoseconds / 1000) / 1e6;
}

[[noreturn]] void frame_kde_error(const std::string& message, std::optional<DataFrameColumnType> type) {
  throw std::invalid_argument(column_type_error(message, type));
}

// pm4py's DataFrame statistics on an uploaded log (CSV or XES frame): the
// column's cells without the missing ones (dropna, a key that is no column
// a KeyError of the list [key]), sorted by Python's sorted() (its TypeError
// for cells that do not order). The numeric estimator takes one distinct
// value through float() (a text cell as numpy's str_, a date raising),
// several distinct numbers (bools as 0 and 1) as floats, and fails on
// several texts or dates; the date estimator calls replace(tzinfo=None) on
// each cell, so the first sorted cell that is no date raises (round BE,
// observed black-box and modelled against pm4py on 6,000 columns).
AttributeKdeResult frame_attribute_kde(const EventLog& input, const AttributeKdeOptions& options) {
  const auto frame = dataframe_frame_event_log(input);
  const auto& key = options.attribute_key;
  const auto type = frame_column_type(frame, key);
  if (!type) throw std::invalid_argument("KeyError: [" + ocel_frame::python_str_repr(key) + "]");
  std::vector<python::Scalar> cells;
  // In the frame's row order (an XES upload's file order, round BF).
  for (const auto& row : frame_rows(frame))
    if (auto cell = frame_row_scalar(frame, row.trace, row.event, key)) cells.push_back(std::move(*cell));
  const auto fail = [&](const std::string& message) { frame_kde_error(message, type); };
  try {
    python::sort(cells, [](const python::Scalar& a, const python::Scalar& b) { return python::less(a, b); });
  } catch (const python::TypeError& error) {
    fail(std::string("TypeError: ") + error.what());
  }
  AttributeKdeResult result;
  result.variant = options.variant;
  result.attribute_key = key;
  const auto graph_points = static_cast<std::int64_t>(options.graph_points);
  const auto is_date = [](const python::Scalar& cell) { return cell.kind == python::Scalar::Kind::kDate; };
  if (options.variant == AttributeKdeVariant::kNumeric) {
    if (cells.empty()) return result;
    bool single = true;
    for (const auto& cell : cells) single = single && python::compare(cell, cells.front()) == 0;
    std::vector<double> values;
    if (single) {
      const auto& cell = cells.front();
      if (is_date(cell))
        fail("TypeError: float() argument must be a string or a real number, not 'Timestamp'");
      if (cell.kind == python::Scalar::Kind::kText) {
        const auto parsed = detail::parse_python_float_text(cell.text);
        if (!parsed)
          frame_kde_error("ValueError: could not convert string to float: np.str_(" +
                              ocel_frame::python_str_repr(cell.text) + ")",
                          type);
        values.push_back(*parsed);
      } else {
        values.push_back(python_float(cell));
      }
    } else {
      if (cells.front().kind == python::Scalar::Kind::kText)
        fail("TypeError: The `dtype` and `signature` arguments to ufuncs only select the general DType and not "
             "details such as the byte order or time unit. You can avoid this error by using the scalar types "
             "`np.float64` or the dtype string notation.");
      if (is_date(cells.front())) fail("TypeError: unsupported operand type(s) for *: 'Timestamp' and 'float'");
      for (const auto& cell : cells) values.push_back(python_float(cell));
    }
    try {
      auto kde = get_kde_numeric_attribute(values, graph_points);
      result.x = std::move(kde.x);
      result.y = std::move(kde.y);
    } catch (const FilteringCommonError& error) {
      fail(error.type + ": " + error.message);
    }
    return result;
  }
  if (cells.empty()) fail("IndexError: list index out of range");
  const auto& first = cells.front();
  if (first.kind == python::Scalar::Kind::kText) fail("TypeError: str.replace() takes no keyword arguments");
  if (!is_date(first)) fail("AttributeError: '" + python::type_name(first) + "' object has no attribute 'replace'");
  std::vector<double> seconds;
  seconds.reserve(cells.size());
  for (const auto& cell : cells) seconds.push_back(frame_timestamp_seconds(cell));
  try {
    const auto kde = get_kde_date_attribute_seconds(seconds, graph_points,
                                                    static_cast<std::int64_t>(options.points_to_sample));
    result.x.reserve(kde.x_ns.size());
    for (const auto ns : kde.x_ns) result.x.push_back(static_cast<double>(ns) / 1e9);
    result.y = kde.y;
  } catch (const FilteringCommonError& error) {
    fail(error.type + ": " + error.message);
  }
  return result;
}

}  // namespace

const char* attribute_kde_variant_name(AttributeKdeVariant variant) {
  switch (variant) {
    case AttributeKdeVariant::kNumeric: return "NUMERIC";
    case AttributeKdeVariant::kDate: return "DATE";
  }
  throw std::invalid_argument("attribute KDE: unknown variant");
}

AttributeKdeResult discover_attribute_kde(const EventLog& log,
                                           const AttributeKdeOptions& options) {
  if (options.attribute_key.empty())
    throw std::invalid_argument("attribute KDE: attribute key is required");
  if (options.graph_points == 0)
    throw std::invalid_argument("attribute KDE: graph_points must be positive");
  if (options.points_to_sample == 0)
    throw std::invalid_argument(
        "attribute KDE: points_to_sample must be positive");
  if (log.dataframe_columns) return frame_attribute_kde(log, options);
  std::vector<double> values;
  bool present = false;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      const auto* text = event_value(event, options.attribute_key);
      if (text) present = true;
      if (!text || text->empty()) continue;
      values.push_back(options.variant == AttributeKdeVariant::kDate
                           ? parse_date(*text, options.attribute_key)
                           : parse_number(*text, options.attribute_key));
    }
  }
  // The reference selects a DataFrame column: a key no event carries is a
  // KeyError there. First-class fields are always columns.
  const auto& key = options.attribute_key;
  const bool first_class = key == "activity" || key == "concept:name" ||
                           key == "timestamp" || key == "time:timestamp" ||
                           key == "resource" || key == "org:resource" ||
                           key == "lifecycle" || key == "lifecycle:transition";
  if (!present && !first_class)
    throw std::invalid_argument("attribute KDE: no event has the attribute '" +
                                key + "'");
  return make_result(options, std::move(values));
}

}  // namespace minera
