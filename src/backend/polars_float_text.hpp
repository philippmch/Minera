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

// Rust's f64 Display, which Polars uses for dynamic float literals: the
// shortest digits that read back, never an exponent (round CF: 2**63 prints
// 9223372036854776000, 1.5e300 a 15 and zeros, not the exact expansion).
inline std::string polars_dyn_float(double x) {
  if (std::isnan(x)) return "NaN";
  if (std::isinf(x)) return x < 0 ? "-inf" : "inf";
  char buffer[64];
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), x, std::chars_format::scientific);
  if (converted.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars numeric text");
  std::string text(buffer, converted.ptr);
  const bool negative = !text.empty() && text[0] == '-';
  if (negative) text.erase(0, 1);
  const auto e = text.find('e');
  const int exponent = std::stoi(text.substr(e + 1));
  std::string digits;
  for (const char c : text.substr(0, e))
    if (c != '.') digits += c;
  while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
  const int point = exponent + 1;
  std::string out;
  if (point <= 0) out = "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
  else if (point >= static_cast<int>(digits.size())) out = digits + std::string(static_cast<std::size_t>(point) - digits.size(), '0');
  else out = digits.substr(0, static_cast<std::size_t>(point)) + "." + digits.substr(static_cast<std::size_t>(point));
  return (negative ? "-" : "") + out;
}

// Polars' cast of a float to String (round CH, black-box: fitted on 805,046
// Float64 and Float32 values against Polars 1.44.1 without a difference).
// The shortest digits that read back (as a float for a Float32 column) in
// fixed notation while the leading digit's decimal exponent is below 16
// (Float32: 13) and at least -5 (Float32: -6); integral values end in ".0";
// other values as d.ddde+X or d.ddde-X without padding (1e-5 casts to
// "0.00001", 1e-7 to "1e-7", 1e16 to "1e+16"; Python's repr differs).
inline std::string polars_cast_float(double x, bool float32 = false) {
  if (std::isnan(x)) return "NaN";
  if (std::isinf(x)) return x < 0 ? "-inf" : "inf";
  const std::string sign = std::signbit(x) ? "-" : "";
  if (x == 0) return sign + "0.0";
  char buffer[64];
  const auto converted = float32
      ? std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(static_cast<float>(x)), std::chars_format::scientific)
      : std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(x), std::chars_format::scientific);
  if (converted.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars numeric text");
  const std::string text(buffer, converted.ptr);
  const auto e = text.find('e');
  std::string digits;
  for (const char c : text.substr(0, e))
    if (c != '.') digits += c;
  const int length = static_cast<int>(digits.size());
  const int point = std::stoi(text.substr(e + 1)) + 1;  // digits before the decimal point
  const int high = float32 ? 13 : 16, low = float32 ? -6 : -5;
  std::string out;
  if (point >= length && point <= high) out = digits + std::string(static_cast<std::size_t>(point - length), '0') + ".0";
  else if (point > 0 && point <= high) out = digits.substr(0, static_cast<std::size_t>(point)) + "." + digits.substr(static_cast<std::size_t>(point));
  else if (point > low && point <= 0) out = "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
  else out = digits.substr(0, 1) + (length > 1 ? "." + digits.substr(1) : "") + "e" + (point > 0 ? "+" : "") + std::to_string(point - 1);
  return sign + out;
}

// Polars' fmt_float, used for Float64 literals in plans and for frame
// cells: integral values under 999999 with one decimal; a Rust display
// longer than nine characters in scientific notation with four decimals
// (outside [1e-6, 999999]) or rounded to six decimals without trailing
// zeros; other integral values in the shortest scientific notation; the
// rest as Rust prints them. Round CF: replaces sampled thresholds that
// printed 1e7 as 1.0000e7 (probed 36 values against Polars 1.44.1).
inline std::string polars_plan_float(double x) {
  if (std::isnan(x) || std::isinf(x)) return polars_dyn_float(x);
  char buffer[512];
  const auto chars = [&](std::chars_format format, int precision) {
    const auto converted = precision < 0 ? std::to_chars(buffer, buffer + sizeof(buffer), x, format)
                                         : std::to_chars(buffer, buffer + sizeof(buffer), x, format, precision);
    if (converted.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars numeric text");
    return std::string(buffer, converted.ptr);
  };
  const auto exponent_form = [&](int precision) {
    const auto text = chars(std::chars_format::scientific, precision);
    const auto e = text.find('e');
    return text.substr(0, e) + "e" + std::to_string(std::stoi(text.substr(e + 1)));
  };
  const double a = std::fabs(x);
  const bool integral = x == std::trunc(x);
  if (integral && a < 999'999.0) return chars(std::chars_format::fixed, 1);
  if (polars_dyn_float(x).size() > 9) {
    if (!(a >= 0.000001 && a <= 999'999.0)) return exponent_form(4);
    auto text = chars(std::chars_format::fixed, 6);
    if (text.back() == '0') {
      text.erase(text.find_last_not_of('0') + 1);
      if (text.back() == '.') text += '0';
    }
    return text;
  }
  if (integral) return exponent_form(-1);
  return polars_dyn_float(x);
}

}  // namespace minera::detail
