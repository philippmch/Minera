#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class AttributeKdeVariant {
  kNumeric,
  kDate,

  NUMERIC = kNumeric,
  DATE = kDate,
};

struct AttributeKdeOptions {
  // Event attribute to sample. Built-in timestamp aliases are accepted for
  // both variants; the date variant parses the selected values as timestamps.
  std::string attribute_key;
  AttributeKdeVariant variant = AttributeKdeVariant::kNumeric;
  // Number of requested plot points. Numeric KDEs may add deterministic tail
  // points so both exact extrema and geometric tails are represented.
  std::size_t graph_points = 200;
  // Date KDEs deterministically thin large samples before fitting.
  std::size_t points_to_sample = 400;
};

struct AttributeKdeResult {
  AttributeKdeVariant variant = AttributeKdeVariant::kNumeric;
  std::string attribute_key;
  std::vector<double> x;
  std::vector<double> y;
};

[[nodiscard]] AttributeKdeResult discover_attribute_kde(
    const EventLog& log, const AttributeKdeOptions& options);

[[nodiscard]] inline AttributeKdeResult attribute_kde(
    const EventLog& log, const AttributeKdeOptions& options) {
  return discover_attribute_kde(log, options);
}

[[nodiscard]] const char* attribute_kde_variant_name(
    AttributeKdeVariant variant);

}  // namespace minera
