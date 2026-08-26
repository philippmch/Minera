#include "minera/attribute_kde.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr double kSqrtTwoPi = 2.5066282746310002;

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

double parse_date(std::string_view value, std::string_view key) {
  const auto parsed = parse_timestamp(value);
  if (!parsed || !std::isfinite(*parsed)) {
    throw std::invalid_argument("attribute KDE: invalid date value for '" +
                                std::string(key) + "'");
  }
  return *parsed;
}

std::vector<double> sorted_unique(const std::vector<double>& input) {
  auto values = input;
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

std::vector<double> linspace(double first, double last, std::size_t count) {
  if (count == 0) return {};
  if (count == 1) return {first};
  std::vector<double> result;
  result.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const double ratio = static_cast<double>(i) /
                         static_cast<double>(count - 1);
    result.push_back(first + (last - first) * ratio);
  }
  return result;
}

std::vector<double> geomspace(double first, double last, std::size_t count) {
  if (count == 0) return {};
  if (count == 1) return {first};
  std::vector<double> result;
  result.reserve(count);
  const double log_first = std::log(first);
  const double log_last = std::log(last);
  for (std::size_t i = 0; i < count; ++i) {
    const double ratio = static_cast<double>(i) /
                         static_cast<double>(count - 1);
    result.push_back(std::exp(log_first + (log_last - log_first) * ratio));
  }
  return result;
}

std::vector<double> numeric_grid(double minimum, double maximum,
                                 std::size_t graph_points) {
  const auto half = std::max(graph_points / 2, std::size_t{2});
  auto result = linspace(minimum, maximum, half);
  if (minimum > 0.0 && maximum > 0.0) {
    const auto geometric = geomspace(std::max(minimum, 1e-6), maximum, half);
    result.insert(result.end(), geometric.begin(), geometric.end());
  } else if (minimum < 0.0 && maximum < 0.0) {
    auto geometric = geomspace(std::abs(minimum),
                               std::max(std::abs(maximum), 1e-6), half);
    for (auto& value : geometric) value = -value;
    result.insert(result.end(), geometric.begin(), geometric.end());
  } else {
    const auto linear = linspace(minimum, maximum, half);
    result.insert(result.end(), linear.begin(), linear.end());
  }
  result.push_back(minimum);
  result.push_back(maximum);
  return sorted_unique(result);
}

double bandwidth(const std::vector<double>& values) {
  const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                      static_cast<double>(values.size());
  double squared = 0.0;
  for (const double value : values) {
    const double delta = value - mean;
    squared += delta * delta;
  }
  const double variance = squared /
                          static_cast<double>(values.size() - 1);
  const double scott = std::pow(static_cast<double>(values.size()), -0.2);
  const double result = std::sqrt(variance) * scott;
  if (!std::isfinite(result) || result <= std::numeric_limits<double>::epsilon())
    return 1.0;
  return result;
}

std::vector<double> density(const std::vector<double>& values,
                            const std::vector<double>& x) {
  if (values.size() < 2) return std::vector<double>(x.size(), 0.0);
  const double h = bandwidth(values);
  const double scale = 1.0 /
                       (static_cast<double>(values.size()) * h * kSqrtTwoPi);
  std::vector<double> result;
  result.reserve(x.size());
  for (const double point : x) {
    double sum = 0.0;
    for (const double value : values) {
      const double z = (point - value) / h;
      sum += std::exp(-0.5 * z * z);
    }
    result.push_back(scale * sum);
  }
  return result;
}

std::vector<double> sample_dates(std::vector<double> values,
                                 std::size_t limit) {
  std::sort(values.begin(), values.end());
  if (values.size() <= limit) return values;
  std::vector<double> result;
  result.reserve(limit);
  if (limit == 1) return {values.front()};
  for (std::size_t i = 0; i < limit; ++i) {
    const auto index = static_cast<std::size_t>(
        std::floor(static_cast<double>(i) *
                   static_cast<double>(values.size() - 1) /
                   static_cast<double>(limit - 1)));
    result.push_back(values[index]);
  }
  return result;
}

AttributeKdeResult make_result(const AttributeKdeOptions& options,
                               std::vector<double> values) {
  AttributeKdeResult result;
  result.variant = options.variant;
  result.attribute_key = options.attribute_key;
  if (values.empty()) return result;
  if (options.variant == AttributeKdeVariant::kDate)
    values = sample_dates(std::move(values), options.points_to_sample);
  const auto unique = sorted_unique(values);
  if (unique.size() == 1) {
    const double center = unique.front();
    const double epsilon = options.variant == AttributeKdeVariant::kDate
                               ? 3600.0
                               : (center == 0.0 ? 1.0
                                                : std::max(std::abs(center) * 0.01,
                                                           1e-6));
    result.x = linspace(center - epsilon, center + epsilon,
                        options.graph_points);
    result.y.assign(result.x.size(), 0.0);
    if (!result.y.empty()) result.y[result.y.size() / 2] = 1.0;
    return result;
  }
  if (options.variant == AttributeKdeVariant::kDate) {
    result.x = linspace(unique.front(), unique.back(), options.graph_points);
  } else {
    result.x = numeric_grid(unique.front(), unique.back(), options.graph_points);
  }
  result.y = density(values, result.x);
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
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      const auto* text = event_value(event, options.attribute_key);
      if (!text || text->empty()) continue;
      values.push_back(options.variant == AttributeKdeVariant::kDate
                           ? parse_date(*text, options.attribute_key)
                           : parse_number(*text, options.attribute_key));
    }
  }
  return make_result(options, std::move(values));
}

}  // namespace minera
