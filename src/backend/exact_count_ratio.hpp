#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace minera::detail {

// A small unsigned integer type used for count arithmetic.  Classic DFG
// measures add at most two uint64_t counts (and sometimes one), so a two-word
// value is sufficient on every compiler, including MSVC without __int128.
struct CountUint128 {
  std::uint64_t hi = 0;
  std::uint64_t lo = 0;

  [[nodiscard]] static constexpr CountUint128 from_uint64(
      std::uint64_t value) {
    return {0, value};
  }

  [[nodiscard]] constexpr bool is_zero() const { return hi == 0 && lo == 0; }
};

[[nodiscard]] constexpr bool operator==(CountUint128 lhs, CountUint128 rhs) {
  return lhs.hi == rhs.hi && lhs.lo == rhs.lo;
}

[[nodiscard]] constexpr bool operator!=(CountUint128 lhs, CountUint128 rhs) {
  return !(lhs == rhs);
}

[[nodiscard]] constexpr bool operator<(CountUint128 lhs, CountUint128 rhs) {
  return lhs.hi < rhs.hi || (lhs.hi == rhs.hi && lhs.lo < rhs.lo);
}

[[nodiscard]] constexpr CountUint128 add(CountUint128 lhs,
                                         CountUint128 rhs) {
  const std::uint64_t lo = lhs.lo + rhs.lo;
  const std::uint64_t carry = lo < lhs.lo ? 1 : 0;
  return {lhs.hi + rhs.hi + carry, lo};
}

[[nodiscard]] constexpr CountUint128 add(std::uint64_t lhs,
                                         std::uint64_t rhs) {
  const std::uint64_t lo = lhs + rhs;
  return {lo < lhs ? std::uint64_t{1} : std::uint64_t{0}, lo};
}

[[nodiscard]] constexpr CountUint128 add_one(CountUint128 value) {
  return add(value, CountUint128::from_uint64(1));
}

[[nodiscard]] constexpr CountUint128 subtract(CountUint128 lhs,
                                              CountUint128 rhs) {
  const std::uint64_t borrow = lhs.lo < rhs.lo ? 1 : 0;
  return {lhs.hi - rhs.hi - borrow, lhs.lo - rhs.lo};
}

[[nodiscard]] constexpr CountUint128 subtract(std::uint64_t lhs,
                                              std::uint64_t rhs) {
  return {0, lhs - rhs};
}

[[nodiscard]] constexpr CountUint128 shift_left_one(CountUint128 value) {
  return {(value.hi << 1) | (value.lo >> 63), value.lo << 1};
}

[[nodiscard]] constexpr std::uint64_t bit_length(CountUint128 value) {
  if (value.hi != 0) {
    std::uint64_t bits = 0;
    auto word = value.hi;
    while (word != 0) {
      ++bits;
      word >>= 1;
    }
    return 64 + bits;
  }
  std::uint64_t bits = 0;
  auto word = value.lo;
  while (word != 0) {
    ++bits;
    word >>= 1;
  }
  return bits;
}

// Divide a non-negative integer ratio and round it to binary64 using exact
// integer remainders.  `negative` is kept separate so a zero numerator can
// retain a requested signed zero.  The count ratios used by Classic are small
// enough to stay in the normal binary64 range, including the AND ratio whose
// numerator may exceed its denominator.
[[nodiscard]] inline double exact_ratio(CountUint128 numerator,
                                        CountUint128 denominator,
                                        bool negative = false) {
  if (denominator.is_zero()) return std::numeric_limits<double>::quiet_NaN();
  if (numerator.is_zero()) return std::copysign(0.0, negative ? -1.0 : 1.0);
  constexpr std::uint64_t kMaxExactBinary64Integer = std::uint64_t{1} << 53;
  if (numerator.hi == 0 && denominator.hi == 0 &&
      numerator.lo <= kMaxExactBinary64Integer &&
      denominator.lo <= kMaxExactBinary64Integer) {
    const double value = static_cast<double>(numerator.lo) /
                          static_cast<double>(denominator.lo);
    return negative ? -value : value;
  }

  // Normalize n/d into [1, 2).  For ratios below one, double n until it
  // reaches d; for ratios at least two, double d until it exceeds n/2.  The
  // count values used here are at most 65 bits, so these shifts remain well
  // within CountUint128.
  CountUint128 scaled = numerator;
  int exponent = 0;
  while (scaled < denominator) {
    scaled = shift_left_one(scaled);
    --exponent;
  }
  CountUint128 scaled_denominator = denominator;
  while (!(scaled < shift_left_one(scaled_denominator))) {
    scaled_denominator = shift_left_one(scaled_denominator);
    ++exponent;
  }
  CountUint128 remainder = subtract(scaled, scaled_denominator);

  // The implicit leading one plus 52 fractional bits form the binary64
  // significand.  One more generated bit is the round-to-nearest guard bit;
  // a non-zero remainder after it is the sticky bit.
  std::uint64_t significand = std::uint64_t{1} << 52;
  for (int bit = 51; bit >= 0; --bit) {
    remainder = shift_left_one(remainder);
    if (!(remainder < scaled_denominator)) {
      remainder = subtract(remainder, scaled_denominator);
      significand |= std::uint64_t{1} << bit;
    }
  }
  remainder = shift_left_one(remainder);
  const bool guard = !(remainder < scaled_denominator);
  if (guard) remainder = subtract(remainder, scaled_denominator);
  if (guard && (!remainder.is_zero() || (significand & 1) != 0)) {
    ++significand;
    if (significand == (std::uint64_t{1} << 53)) {
      significand = std::uint64_t{1} << 52;
      ++exponent;
    }
  }

  const double value = std::ldexp(static_cast<double>(significand),
                                  exponent - 52);
  return negative ? -value : value;
}

[[nodiscard]] inline double exact_ratio(std::uint64_t numerator,
                                        std::uint64_t denominator) {
  return exact_ratio(CountUint128::from_uint64(numerator),
                     CountUint128::from_uint64(denominator));
}

// Classic bidirectional dependency: (forward - reverse) /
// (forward + reverse + 1), with all integer operations exact before the final
// correctly-rounded binary64 conversion.
[[nodiscard]] inline double exact_signed_count_ratio(
    std::uint64_t forward, std::uint64_t reverse) {
  const bool negative = forward < reverse;
  const CountUint128 numerator =
      negative ? subtract(reverse, forward) : subtract(forward, reverse);
  const CountUint128 denominator =
      add_one(add(forward, reverse));
  return exact_ratio(numerator, denominator, negative);
}

}  // namespace minera::detail
