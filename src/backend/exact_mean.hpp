#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace minera::detail {

namespace exact_mean_detail {

// A finite binary64 value is an integer multiple of 2^-1074.  The largest
// finite value needs 2098 such bits.  A vector can contain at most SIZE_MAX
// values, so 2304 bits leave room for the largest possible finite sum on the
// platforms supported by the project while keeping the arithmetic fixed-size.
constexpr std::size_t kWordCount = 36;
constexpr std::size_t kWordBits = 64;

struct Magnitude {
  std::array<std::uint64_t, kWordCount> words{};
};

inline int highest_bit(const Magnitude& value) {
  for (std::size_t index = kWordCount; index-- > 0;) {
    const auto word = value.words[index];
    if (word == 0) continue;
    return static_cast<int>(index * kWordBits +
                            std::bit_width(word) - 1);
  }
  return -1;
}

inline bool get_bit(const Magnitude& value, std::size_t bit) {
  return (value.words[bit / kWordBits] >> (bit % kWordBits) & 1u) != 0;
}

inline bool any_lower_bits(const Magnitude& value, std::size_t exclusive_end) {
  const auto full_words = exclusive_end / kWordBits;
  for (std::size_t index = 0; index < full_words; ++index) {
    if (value.words[index] != 0) return true;
  }
  const auto remainder = exclusive_end % kWordBits;
  if (remainder == 0) return false;
  const auto mask = (std::uint64_t{1} << remainder) - 1;
  return (value.words[full_words] & mask) != 0;
}

inline std::uint64_t extract_bits(const Magnitude& value, std::size_t first,
                                  unsigned count) {
  std::uint64_t result = 0;
  for (unsigned offset = 0; offset < count; ++offset) {
    if (get_bit(value, first + offset)) result |= std::uint64_t{1} << offset;
  }
  return result;
}

inline void add_word(Magnitude& value, std::size_t index,
                     std::uint64_t addend) {
  if (addend == 0) return;
  while (addend != 0 && index < kWordCount) {
    const auto old = value.words[index];
    value.words[index] += addend;
    addend = value.words[index] < old ? 1 : 0;
    ++index;
  }
}

inline void add_shifted(Magnitude& value, std::uint64_t mantissa,
                        std::size_t shift) {
  const auto word = shift / kWordBits;
  const auto offset = static_cast<unsigned>(shift % kWordBits);
  add_word(value, word, mantissa << offset);
  if (offset != 0) add_word(value, word + 1, mantissa >> (kWordBits - offset));
}

inline void subtract(const Magnitude& larger, const Magnitude& smaller,
                     Magnitude& result) {
  std::uint64_t borrow = 0;
  for (std::size_t index = 0; index < kWordCount; ++index) {
    const auto lhs = larger.words[index];
    const auto rhs = smaller.words[index];
    const auto with_borrow = rhs + borrow;
    const auto rhs_overflow = with_borrow < rhs;
    const auto difference = lhs - with_borrow;
    const auto difference_borrow = lhs < with_borrow;
    result.words[index] = difference;
    borrow = (rhs_overflow || difference_borrow) ? 1 : 0;
  }
}

inline int compare(const Magnitude& lhs, const Magnitude& rhs) {
  for (std::size_t index = kWordCount; index-- > 0;) {
    if (lhs.words[index] < rhs.words[index]) return -1;
    if (lhs.words[index] > rhs.words[index]) return 1;
  }
  return 0;
}

struct Division {
  Magnitude quotient;
  std::uint64_t remainder = 0;
};

inline Division divide(const Magnitude& dividend, std::uint64_t denominator) {
  Division result;
  const auto top = highest_bit(dividend);
  if (top < 0) return result;

  for (int bit = top; bit >= 0; --bit) {
    const auto incoming = get_bit(dividend, static_cast<std::size_t>(bit));
    // Test 2 * remainder + incoming >= denominator without overflowing a
    // uint64_t.  For an incoming zero the threshold is ceil(n/2); for one it
    // is floor(n/2).
    const auto threshold = denominator / 2 +
                           ((incoming == 0 && (denominator & 1u)) ? 1 : 0);
    const auto quotient_bit = result.remainder >= threshold;
    if (quotient_bit) {
      const auto complement = denominator - result.remainder;
      result.remainder = result.remainder - complement + incoming;
      result.quotient.words[static_cast<std::size_t>(bit) / kWordBits] |=
          std::uint64_t{1} << (static_cast<unsigned>(bit) % kWordBits);
    } else {
      result.remainder = result.remainder * 2 + incoming;
    }
  }
  return result;
}

inline bool round_fraction(std::uint64_t remainder, std::uint64_t denominator,
                           bool kept_odd) {
  const auto half = denominator / 2;
  if (remainder > half) return true;
  if (remainder < half) return false;
  if ((denominator & 1u) != 0) return false;
  return kept_odd;
}

inline double with_sign(std::uint64_t magnitude_bits, bool negative) {
  if (negative) magnitude_bits |= std::uint64_t{1} << 63;
  return std::bit_cast<double>(magnitude_bits);
}

inline double rounded_finite_mean(const Magnitude& magnitude,
                                 std::uint64_t count, bool negative) {
  const auto division = divide(magnitude, count);
  const auto& quotient = division.quotient;
  const auto remainder = division.remainder;
  constexpr std::uint64_t kSubnormalLimit = std::uint64_t{1} << 52;

  if (highest_bit(quotient) < 52) {
    // In subnormal units the exact value is quotient + remainder/count.
    auto units = quotient.words[0];
    if (round_fraction(remainder, count, (units & 1u) != 0)) ++units;
    if (units >= kSubnormalLimit) {
      return with_sign(std::uint64_t{1} << 52, negative);
    }
    return with_sign(units, negative);
  }

  auto exponent_bit = highest_bit(quotient);
  const auto shift = static_cast<std::size_t>(exponent_bit - 52);
  auto significand = extract_bits(quotient, shift, 53);
  bool round_up = false;
  if (shift == 0) {
    round_up = round_fraction(remainder, count,
                              (significand & 1u) != 0);
  } else {
    const auto half_bit = get_bit(quotient, shift - 1);
    if (half_bit) {
      round_up = any_lower_bits(quotient, shift - 1) || remainder != 0 ||
                 (significand & 1u) != 0;
    }
  }
  if (round_up) ++significand;
  if (significand == (std::uint64_t{1} << 53)) {
    significand >>= 1;
    ++exponent_bit;
  }

  const auto exponent = exponent_bit - 51;
  if (exponent >= 2047) {
    return negative ? -std::numeric_limits<double>::infinity()
                    : std::numeric_limits<double>::infinity();
  }
  const auto bits = (static_cast<std::uint64_t>(exponent) << 52) |
                    (significand - kSubnormalLimit);
  return with_sign(bits, negative);
}

}  // namespace exact_mean_detail

// Return the exact arithmetic mean of binary64 inputs, rounded once to
// binary64.  Finite values are accumulated as integer multiples of 2^-1074,
// avoiding the overflow and loss of precision caused by a floating sum.
inline double exact_mean(const std::vector<double>& values) {
  if (values.empty()) return 0.0;

  exact_mean_detail::Magnitude positive;
  exact_mean_detail::Magnitude negative;
  bool positive_infinity = false;
  bool negative_infinity = false;
  bool nan = false;
  for (const double value : values) {
    if (std::isnan(value)) {
      nan = true;
      continue;
    }
    if (std::isinf(value)) {
      if (std::signbit(value)) {
        negative_infinity = true;
      } else {
        positive_infinity = true;
      }
      continue;
    }
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto exponent = (bits >> 52) & 0x7ffu;
    const auto fraction = bits & ((std::uint64_t{1} << 52) - 1);
    if (fraction == 0 && exponent == 0) continue;
    const auto mantissa = exponent == 0 ? fraction
                                       : (std::uint64_t{1} << 52) | fraction;
    const auto shift = exponent == 0 ? 0 : exponent - 1;
    auto& accumulator = (bits >> 63) == 0 ? positive : negative;
    exact_mean_detail::add_shifted(accumulator, mantissa,
                                   static_cast<std::size_t>(shift));
  }

  if (nan || (positive_infinity && negative_infinity))
    return std::numeric_limits<double>::quiet_NaN();
  if (positive_infinity) return std::numeric_limits<double>::infinity();
  if (negative_infinity) return -std::numeric_limits<double>::infinity();

  const auto comparison = exact_mean_detail::compare(positive, negative);
  exact_mean_detail::Magnitude magnitude;
  bool negative_result = false;
  if (comparison > 0) {
    exact_mean_detail::subtract(positive, negative, magnitude);
  } else if (comparison < 0) {
    exact_mean_detail::subtract(negative, positive, magnitude);
    negative_result = true;
  } else {
    return 0.0;
  }
  return exact_mean_detail::rounded_finite_mean(
      magnitude, static_cast<std::uint64_t>(values.size()), negative_result);
}

}  // namespace minera::detail
