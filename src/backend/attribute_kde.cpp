#include "minera/attribute_kde.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>

#include "minera/filtering_common.hpp"
#include "minera/timestamp.hpp"

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
