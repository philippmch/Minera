#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace minera::detail {

inline std::string polars_shortest_fixed(double x) {
  char buffer[512];
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), x, std::chars_format::fixed);
  if (converted.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars numeric text");
  return std::string(buffer, converted.ptr);
}

// Rust's f64 Display, which Polars uses for dynamic float literals.
inline std::string polars_dyn_float(double x) {
  if (std::isnan(x)) return "NaN";
  if (std::isinf(x)) return x < 0 ? "-inf" : "inf";
  return polars_shortest_fixed(x);
}

// Polars' plan display of a Float64 scalar: integral values below 1e7 get
// ".0", other values in [1e-5, 1e7) print in full, the rest in scientific
// notation with four decimals (sampled thresholds).
inline std::string polars_plan_float(double x) {
  if (std::isnan(x) || std::isinf(x)) return polars_dyn_float(x);
  const double a = std::fabs(x);
  if (x == std::floor(x) && a < 1e7) return polars_shortest_fixed(x) + ".0";
  if (a >= 1e-5 && a < 1e7) return polars_shortest_fixed(x);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.4e", x);
  const std::string out(buffer);
  const auto e = out.find('e');
  std::string exponent = out.substr(e + 2);
  while (exponent.size() > 1 && exponent[0] == '0') exponent.erase(0, 1);
  return out.substr(0, e) + "e" + (out[e + 1] == '-' ? "-" : "") + exponent;
}

}  // namespace minera::detail
