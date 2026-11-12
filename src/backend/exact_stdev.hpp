#pragma once

#include "exact_mean.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace minera::detail {

namespace exact_stdev_detail {

// A finite binary64 value is an integer multiple of 2^-1074.  A sum of at
// most SIZE_MAX such values fits in the 36-word accumulator used by
// exact_mean.  Squaring that sum, and multiplying the sum of the individual
// squares by the count, both fit in 72 words (4608 bits).
constexpr std::size_t kWordCount = 72;
constexpr std::size_t kWordBits = 64;
using Wide = std::array<std::uint64_t, kWordCount>;

struct Unsigned128 {
  std::uint64_t low = 0;
  std::uint64_t high = 0;
};

inline int highest_bit(const Wide& value) {
  for (std::size_t index = kWordCount; index-- > 0;) {
    const auto word = value[index];
    if (word == 0) continue;
    return static_cast<int>(index * kWordBits + std::bit_width(word) - 1);
  }
  return -1;
}

inline int highest_bit(const Unsigned128& value) {
  if (value.high != 0)
    return static_cast<int>(kWordBits + std::bit_width(value.high) - 1);
  if (value.low != 0) return static_cast<int>(std::bit_width(value.low) - 1);
  return -1;
}

inline int compare(const Wide& lhs, const Wide& rhs) {
  for (std::size_t index = kWordCount; index-- > 0;) {
    if (lhs[index] < rhs[index]) return -1;
    if (lhs[index] > rhs[index]) return 1;
  }
  return 0;
}

inline void add_word(Wide& value, std::size_t index,
                     std::uint64_t addend) {
  if (addend == 0) return;
  while (addend != 0 && index < kWordCount) {
    const auto old = value[index];
    value[index] += addend;
    addend = value[index] < old ? 1 : 0;
    ++index;
  }
}

inline void subtract(const Wide& larger, const Wide& smaller, Wide& result) {
  std::uint64_t borrow = 0;
  for (std::size_t index = 0; index < kWordCount; ++index) {
    const auto lhs = larger[index];
    const auto rhs = smaller[index];
    const auto with_borrow = rhs + borrow;
    const auto rhs_overflow = with_borrow < rhs;
    const auto difference = lhs - with_borrow;
    const auto difference_borrow = lhs < with_borrow;
    result[index] = difference;
    borrow = (rhs_overflow || difference_borrow) ? 1 : 0;
  }
}

// Multiply two equally sized arrays without relying on __int128, which is
// unavailable in MSVC.  Base-2^32 limbs keep each intermediate below 2^64.
template <std::size_t WordCount>
inline std::array<std::uint64_t, WordCount * 2> multiply_words(
    const std::array<std::uint64_t, WordCount>& lhs,
    const std::array<std::uint64_t, WordCount>& rhs) {
  constexpr std::size_t limb_count = WordCount * 2;
  std::array<std::uint32_t, limb_count * 2> result{};
  std::array<std::uint32_t, limb_count> lhs_limbs{};
  std::array<std::uint32_t, limb_count> rhs_limbs{};
  for (std::size_t index = 0; index < WordCount; ++index) {
    lhs_limbs[index * 2] = static_cast<std::uint32_t>(lhs[index]);
    lhs_limbs[index * 2 + 1] =
        static_cast<std::uint32_t>(lhs[index] >> 32);
    rhs_limbs[index * 2] = static_cast<std::uint32_t>(rhs[index]);
    rhs_limbs[index * 2 + 1] =
        static_cast<std::uint32_t>(rhs[index] >> 32);
  }

  for (std::size_t lhs_index = 0; lhs_index < limb_count; ++lhs_index) {
    std::uint64_t carry = 0;
    for (std::size_t rhs_index = 0; rhs_index < limb_count; ++rhs_index) {
      const auto index = lhs_index + rhs_index;
      const auto current =
          static_cast<std::uint64_t>(result[index]) +
          static_cast<std::uint64_t>(lhs_limbs[lhs_index]) *
              rhs_limbs[rhs_index] +
          carry;
      result[index] = static_cast<std::uint32_t>(current);
      carry = current >> 32;
    }
    std::size_t index = lhs_index + limb_count;
    while (carry != 0 && index < result.size()) {
      const auto current = static_cast<std::uint64_t>(result[index]) + carry;
      result[index] = static_cast<std::uint32_t>(current);
      carry = current >> 32;
      ++index;
    }
  }

  std::array<std::uint64_t, WordCount * 2> packed{};
  for (std::size_t index = 0; index < packed.size(); ++index) {
    packed[index] = static_cast<std::uint64_t>(result[index * 2]) |
                    (static_cast<std::uint64_t>(result[index * 2 + 1]) <<
                     32);
  }
  return packed;
}

inline Unsigned128 multiply_u64(std::uint64_t lhs, std::uint64_t rhs) {
  std::array<std::uint64_t, 1> lhs_words{lhs};
  std::array<std::uint64_t, 1> rhs_words{rhs};
  const auto product = multiply_words<1>(lhs_words, rhs_words);
  return {product[0], product[1]};
}

inline void add_shifted(Wide& target, std::uint64_t low,
                        std::uint64_t high, std::size_t shift) {
  const auto word = shift / kWordBits;
  const auto offset = static_cast<unsigned>(shift % kWordBits);
  if (offset == 0) {
    add_word(target, word, low);
    add_word(target, word + 1, high);
    return;
  }
  add_word(target, word, low << offset);
  add_word(target, word + 1,
           (low >> (kWordBits - offset)) | (high << offset));
  add_word(target, word + 2, high >> (kWordBits - offset));
}

template <std::size_t WordCount>
inline void add_shifted(Wide& target,
                        const std::array<std::uint64_t, WordCount>& value,
                        std::size_t shift) {
  const auto word = shift / kWordBits;
  const auto offset = static_cast<unsigned>(shift % kWordBits);
  for (std::size_t index = 0; index < WordCount; ++index) {
    if (offset == 0) {
      add_word(target, word + index, value[index]);
    } else {
      add_word(target, word + index, value[index] << offset);
      add_word(target, word + index + 1,
               value[index] >> (kWordBits - offset));
    }
  }
}

inline Wide shifted(const Wide& value, std::size_t shift) {
  Wide result{};
  const auto word = shift / kWordBits;
  const auto offset = static_cast<unsigned>(shift % kWordBits);
  for (std::size_t index = 0; index < kWordCount; ++index) {
    if (word + index >= kWordCount) break;
    if (offset == 0) {
      result[word + index] |= value[index];
    } else {
      result[word + index] |= value[index] << offset;
      if (word + index + 1 < kWordCount)
        result[word + index + 1] |= value[index] >> (kWordBits - offset);
    }
  }
  return result;
}

template <std::size_t WordCount>
inline Wide shifted(const std::array<std::uint64_t, WordCount>& value,
                    std::size_t shift) {
  Wide result{};
  const auto word = shift / kWordBits;
  const auto offset = static_cast<unsigned>(shift % kWordBits);
  for (std::size_t index = 0; index < WordCount; ++index) {
    if (word + index >= kWordCount) break;
    if (offset == 0) {
      result[word + index] |= value[index];
    } else {
      result[word + index] |= value[index] << offset;
      if (word + index + 1 < kWordCount)
        result[word + index + 1] |= value[index] >> (kWordBits - offset);
    }
  }
  return result;
}

inline void multiply_by_u64(Wide& value, std::uint64_t multiplier) {
  std::uint64_t carry = 0;
  for (auto& word : value) {
    const auto product = multiply_u64(word, multiplier);
    const auto low = product.low + carry;
    const auto overflow = low < product.low;
    word = low;
    carry = product.high + (overflow ? 1 : 0);
  }
}

inline Wide square(const exact_mean_detail::Magnitude& value) {
  return multiply_words<exact_mean_detail::kWordCount>(value.words,
                                                        value.words);
}

inline Wide denominator_times_square(const Unsigned128& denominator,
                                     std::uint64_t significand,
                                     std::size_t shift) {
  const auto significand_square = multiply_u64(significand, significand);
  const std::array<std::uint64_t, 2> denominator_words = {
      denominator.low, denominator.high};
  const std::array<std::uint64_t, 2> square_words = {significand_square.low,
                                                      significand_square.high};
  const auto product = multiply_words<2>(denominator_words, square_words);
  return shifted(product, shift);
}

inline bool square_is_at_most(const Wide& numerator,
                              const Unsigned128& denominator,
                              std::size_t shift,
                              std::uint64_t significand) {
  return compare(denominator_times_square(denominator, significand, shift),
                 numerator) <= 0;
}

inline std::uint64_t floor_scaled_sqrt(const Wide& numerator,
                                       const Unsigned128& denominator,
                                       std::size_t shift,
                                       std::uint64_t lower,
                                       std::uint64_t upper) {
  while (lower < upper) {
    const auto middle = lower + (upper - lower + 1) / 2;
    if (square_is_at_most(numerator, denominator, shift, middle)) {
      lower = middle;
    } else {
      upper = middle - 1;
    }
  }
  return lower;
}

inline double rounded_sqrt_ratio(const Wide& numerator,
                                 const Unsigned128& denominator) {
  const auto numerator_bit = highest_bit(numerator);
  const auto denominator_bit = highest_bit(denominator);
  int ratio_bit = numerator_bit - denominator_bit;

  // Establish floor(log2(numerator / denominator)) exactly.  At most one
  // correction is needed after aligning the leading bits.
  if (ratio_bit >= 0) {
    if (compare(numerator, shifted(
                              std::array<std::uint64_t, 2>{denominator.low,
                                                           denominator.high},
                              static_cast<std::size_t>(ratio_bit))) < 0)
      --ratio_bit;
  } else {
    const auto aligned_numerator =
        shifted(numerator, static_cast<std::size_t>(-ratio_bit));
    const auto denominator_words =
        std::array<std::uint64_t, 2>{denominator.low, denominator.high};
    if (compare(aligned_numerator, shifted(denominator_words, 0)) < 0)
      --ratio_bit;
  }

  const auto floor_half = ratio_bit >= 0
                              ? ratio_bit / 2
                              : -(((-ratio_bit) + 1) / 2);
  const int exponent = floor_half - 1074;
  if (exponent > 1023) return std::numeric_limits<double>::infinity();

  std::uint64_t significand = 0;
  if (exponent >= -1022) {
    const auto shift = static_cast<std::size_t>(2044 + 2 * exponent);
    significand = floor_scaled_sqrt(numerator, denominator, shift,
                                    std::uint64_t{1} << 52,
                                    (std::uint64_t{1} << 53) - 1);
  } else {
    significand = floor_scaled_sqrt(numerator, denominator, 0, 0,
                                    std::uint64_t{1} << 52);
  }

  // Compare the exact value with (significand + 1/2)^2.  The denominator
  // product already carries the normal-binade scale when needed; multiplying
  // the left hand side by four clears the remaining half-unit fraction in
  // both normal and subnormal modes.
  const auto normal = exponent >= -1022;
  const auto lhs = shifted(numerator, 2);
  const auto midpoint_significand = significand * 2 + 1;
  const auto rhs = denominator_times_square(
      denominator, midpoint_significand,
      normal ? static_cast<std::size_t>(2044 + 2 * exponent) : 0);
  const auto midpoint_comparison = compare(lhs, rhs);
  if (midpoint_comparison > 0 ||
      (midpoint_comparison == 0 && (significand & 1u) != 0))
    ++significand;

  if (exponent < -1022) {
    if (significand >= (std::uint64_t{1} << 52))
      return std::bit_cast<double>(std::uint64_t{1} << 52);
    return std::bit_cast<double>(significand);
  }

  int rounded_exponent = exponent;
  if (significand == (std::uint64_t{1} << 53)) {
    significand >>= 1;
    ++rounded_exponent;
  }
  if (rounded_exponent > 1023)
    return std::numeric_limits<double>::infinity();
  const auto bits =
      (static_cast<std::uint64_t>(rounded_exponent + 1023) << 52) |
      (significand - (std::uint64_t{1} << 52));
  return std::bit_cast<double>(bits);
}

}  // namespace exact_stdev_detail

// Return the sample standard deviation of finite binary64 inputs, rounded
// once to binary64.  The sums are exact integer arithmetic at the binary64
// input scale, so cancellation, overflow in x*x, and a rounded mean cannot
// affect the result.  A vector with fewer than two elements has deviation
// zero; non-finite inputs produce NaN.
inline double exact_stdev(const std::vector<double>& values) {
  if (values.size() <= 1) return 0.0;

  exact_mean_detail::Magnitude positive;
  exact_mean_detail::Magnitude negative;
  exact_stdev_detail::Wide sum_squares{};
  for (const double value : values) {
    if (!std::isfinite(value))
      return std::numeric_limits<double>::quiet_NaN();

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

    const std::array<std::uint64_t, 1> mantissa_words{mantissa};
    const auto square =
        exact_stdev_detail::multiply_words<1>(mantissa_words,
                                               mantissa_words);
    exact_stdev_detail::add_shifted(
        sum_squares, square, static_cast<std::size_t>(2 * shift));
  }

  exact_mean_detail::Magnitude sum_magnitude;
  const auto sum_comparison =
      exact_mean_detail::compare(positive, negative);
  if (sum_comparison > 0) {
    exact_mean_detail::subtract(positive, negative, sum_magnitude);
  } else if (sum_comparison < 0) {
    exact_mean_detail::subtract(negative, positive, sum_magnitude);
  } else {
    sum_magnitude = {};
  }

  auto numerator = sum_squares;
  exact_stdev_detail::multiply_by_u64(
      numerator, static_cast<std::uint64_t>(values.size()));
  const auto sum_square = exact_stdev_detail::square(sum_magnitude);
  exact_stdev_detail::subtract(numerator, sum_square, numerator);
  if (exact_stdev_detail::highest_bit(numerator) < 0) return 0.0;

  const auto count = static_cast<std::uint64_t>(values.size());
  const auto denominator =
      exact_stdev_detail::multiply_u64(count, count - 1);
  return exact_stdev_detail::rounded_sqrt_ratio(numerator, denominator);
}

// The words of a wide unsigned sum that are in use: `words[i]` is word
// `base + i` of the full-width accumulator. Durations of one activity pair
// span a few words, so a pair keeps a few words instead of the full width.
class SparseWords {
 public:
  // Adds `count` words, shifted left by `bit_shift` bits.
  void add(const std::uint64_t* value, std::size_t count, std::size_t bit_shift) {
    const std::size_t word = bit_shift / 64, offset = bit_shift % 64;
    if (words_.empty()) {
      base_ = word;
    } else if (word < base_) {
      words_.insert(words_.begin(), base_ - word, 0);
      base_ = word;
    }
    if (words_.size() < word - base_ + count + 1) words_.resize(word - base_ + count + 1, 0);
    std::uint64_t carry = 0;
    std::size_t index = word - base_;
    for (std::size_t i = 0; i <= count; ++i, ++index) {
      const std::uint64_t low = i < count ? value[i] << offset : 0;
      const std::uint64_t high = offset == 0 || i == 0 ? 0 : value[i - 1] >> (64 - offset);
      const std::uint64_t part = low | high;
      const std::uint64_t sum = words_[index] + part;
      const std::uint64_t next = (sum < part ? 1 : 0);
      const std::uint64_t total = sum + carry;
      carry = next + (total < sum ? 1 : 0);
      words_[index] = total;
    }
    for (; carry != 0; ++index) {
      if (index == words_.size()) words_.push_back(0);
      words_[index] += carry;
      carry = words_[index] == 0 ? 1 : 0;
    }
  }

  template <std::size_t N>
  [[nodiscard]] std::array<std::uint64_t, N> expand() const {
    std::array<std::uint64_t, N> full{};
    for (std::size_t i = 0; i < words_.size() && base_ + i < N; ++i) full[base_ + i] = words_[i];
    return full;
  }

 private:
  std::size_t base_ = 0;
  std::vector<std::uint64_t> words_;
};

// exact_mean and exact_stdev of finite values added one at a time: Python's
// statistics.mean and statistics.stdev (exact, rounded once) without keeping
// the values. A non-finite value makes both NaN.
class ExactMoments {
 public:
  void add(double value) {
    ++count_;
    if (!std::isfinite(value)) {
      nonfinite_ = true;
      return;
    }
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto exponent = (bits >> 52) & 0x7ffu;
    const auto fraction = bits & ((std::uint64_t{1} << 52) - 1);
    if (fraction == 0 && exponent == 0) return;
    const auto mantissa = exponent == 0 ? fraction : (std::uint64_t{1} << 52) | fraction;
    const auto shift = static_cast<std::size_t>(exponent == 0 ? 0 : exponent - 1);
    ((bits >> 63) == 0 ? positive_ : negative_).add(&mantissa, 1, shift);
    const std::array<std::uint64_t, 1> mantissa_words{mantissa};
    const auto square = exact_stdev_detail::multiply_words<1>(mantissa_words, mantissa_words);
    squares_.add(square.data(), square.size(), 2 * shift);
  }

  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }

  [[nodiscard]] double mean() const {
    if (count_ == 0) return 0.0;
    if (nonfinite_) return std::numeric_limits<double>::quiet_NaN();
    bool negative = false;
    const auto magnitude = sum(negative);
    if (exact_mean_detail::highest_bit(magnitude) < 0) return 0.0;
    return exact_mean_detail::rounded_finite_mean(magnitude, count_, negative);
  }

  [[nodiscard]] double stdev() const {
    if (count_ <= 1) return 0.0;
    if (nonfinite_) return std::numeric_limits<double>::quiet_NaN();
    bool negative = false;
    const auto magnitude = sum(negative);
    auto numerator = squares_.expand<exact_stdev_detail::kWordCount>();
    exact_stdev_detail::multiply_by_u64(numerator, count_);
    exact_stdev_detail::subtract(numerator, exact_stdev_detail::square(magnitude), numerator);
    if (exact_stdev_detail::highest_bit(numerator) < 0) return 0.0;
    return exact_stdev_detail::rounded_sqrt_ratio(numerator, exact_stdev_detail::multiply_u64(count_, count_ - 1));
  }

 private:
  [[nodiscard]] exact_mean_detail::Magnitude sum(bool& negative) const {
    exact_mean_detail::Magnitude positive, minus, magnitude;
    positive.words = positive_.expand<exact_mean_detail::kWordCount>();
    minus.words = negative_.expand<exact_mean_detail::kWordCount>();
    const auto comparison = exact_mean_detail::compare(positive, minus);
    negative = comparison < 0;
    if (comparison > 0) exact_mean_detail::subtract(positive, minus, magnitude);
    else if (comparison < 0) exact_mean_detail::subtract(minus, positive, magnitude);
    return magnitude;
  }

  SparseWords positive_;
  SparseWords negative_;
  SparseWords squares_;
  std::uint64_t count_ = 0;
  bool nonfinite_ = false;
};

}  // namespace minera::detail
