#pragma once

#include <bit>
#include <cmath>
#include <cstdint>

namespace minera {

// Python's int / int for an int64 numerator and a positive denominator
// below 2^53: the correctly rounded (ties-to-even) quotient. Exact integer
// long division, so no 128-bit or extended-precision type is needed.
inline double true_divide(std::int64_t numerator, std::int64_t denominator) {
  constexpr std::int64_t exact = std::int64_t{1} << 53;
  if (numerator > -exact && numerator < exact) return static_cast<double>(numerator) / static_cast<double>(denominator);
  const bool negative = numerator < 0;
  // |INT64_MIN| fits in the unsigned type.
  const std::uint64_t magnitude = negative ? std::uint64_t{0} - static_cast<std::uint64_t>(numerator)
                                           : static_cast<std::uint64_t>(numerator);
  const auto divisor = static_cast<std::uint64_t>(denominator);
  // X = floor(|n| / d * 2^fraction_bits) with exactly 55 significant bits:
  // 53 for the mantissa, one guard bit and one bit folded into `sticky`.
  std::uint64_t x = magnitude / divisor;
  std::uint64_t remainder = magnitude % divisor;
  int fraction_bits = 0;
  bool sticky = false;
  if (std::bit_width(x) > 55) {
    const int drop = static_cast<int>(std::bit_width(x)) - 55;
    sticky = (x & ((std::uint64_t{1} << drop) - 1)) != 0;
    x >>= drop;
    fraction_bits = -drop;
  }
  while (std::bit_width(x) < 55) {
    remainder <<= 1;  // remainder < d < 2^53, so this cannot overflow
    x = (x << 1) | (remainder >= divisor ? 1 : 0);
    if (remainder >= divisor) remainder -= divisor;
    ++fraction_bits;
  }
  sticky = sticky || remainder != 0 || (x & 1) != 0;
  const bool guard = ((x >> 1) & 1) != 0;
  std::uint64_t mantissa = x >> 2;
  if (guard && (sticky || (mantissa & 1) != 0)) ++mantissa;
  const double value = std::ldexp(static_cast<double>(mantissa), 2 - fraction_bits);
  return negative ? -value : value;
}

}  // namespace minera
