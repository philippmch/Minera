#pragma once

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace minera {

// Activity occurrence totals are widened to an unsigned 128-bit value so
// repeated DFG aggregation cannot truncate a valid count at uint64_t's limit.
// Conversion back to uint64_t is intentionally checked; callers must handle
// values that do not fit instead of receiving an implicit narrowing conversion.
struct HeuristicOccurrenceCount {
  std::uint64_t high = 0;
  std::uint64_t low = 0;

  // This constructor remains implicit so uint64_t count thresholds and
  // comparisons continue to work with widened occurrence fields.
  constexpr HeuristicOccurrenceCount(std::uint64_t value = 0) noexcept
      : high(0), low(value) {}

  constexpr HeuristicOccurrenceCount(std::uint64_t high_word,
                                     std::uint64_t low_word) noexcept
      : high(high_word), low(low_word) {}

  friend constexpr bool operator==(const HeuristicOccurrenceCount&,
                                   const HeuristicOccurrenceCount&) = default;
  friend constexpr std::strong_ordering operator<=>(
      const HeuristicOccurrenceCount&, const HeuristicOccurrenceCount&) =
          default;

  // Add a native count exactly, throwing if the 128-bit result would wrap.
  HeuristicOccurrenceCount& operator+=(std::uint64_t or_count) {
    const std::uint64_t new_low = low + or_count;
    const bool carry = new_low < low;
    if (carry && high == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("heuristic occurrence count overflow");
    low = new_low;
    if (carry) ++high;
    return *this;
  }

  // The wide overload is useful when composing already widened totals and
  // keeps the same checked arithmetic guarantee as += uint64_t.
  HeuristicOccurrenceCount& operator+=(
      const HeuristicOccurrenceCount& other) {
    const std::uint64_t new_low = low + other.low;
    const bool carry = new_low < low;
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    if (other.high > max - high ||
        (carry && other.high == max - high)) {
      throw std::overflow_error("heuristic occurrence count overflow");
    }
    high += other.high;
    if (carry) ++high;
    low = new_low;
    return *this;
  }

  [[nodiscard]] bool fits_uint64() const noexcept { return high == 0; }

  [[nodiscard]] std::uint64_t to_uint64() const {
    if (!fits_uint64())
      throw std::overflow_error(
          "heuristic occurrence count does not fit uint64_t");
    return low;
  }

  // Match int(float(total / 2)) for a non-negative Python integer.  The
  // binary64 significand, guard bit, sticky bits, and ties-to-even increment
  // are computed directly from the two 64-bit words.  In particular, this
  // keeps the rounded half of uint128_max at 2^127 without an overflowing
  // intermediate or a dependency on non-portable long double/__int128.
  [[nodiscard]] HeuristicOccurrenceCount rounded_half() const noexcept {
    const std::size_t length = bit_length();
    if (length <= 53)
      return HeuristicOccurrenceCount(low >> 1);

    // Keep 53 significant bits of total/2.  If total has L bits, the
    // resulting binary64 integer spacing is 2^(L-54).  The retained
    // significand is therefore total >> (L-53); the next bit is the guard.
    const std::size_t significand_shift = length - 53;
    std::uint64_t significand = shift_right(significand_shift);
    const bool guard = bit_at(significand_shift - 1);
    const bool sticky = any_low_bits(significand_shift - 1);
    if (guard && (sticky || (significand & 1U) != 0)) ++significand;

    return from_shifted(significand, significand_shift - 1);
  }

  [[nodiscard]] std::string to_string() const {
    if (high == 0) return std::to_string(low);

    // Four base-2^32 limbs make division by ten portable without relying on
    // an implementation-specific wider integer type.
    std::uint32_t limbs[4] = {
        static_cast<std::uint32_t>(high >> 32),
        static_cast<std::uint32_t>(high),
        static_cast<std::uint32_t>(low >> 32),
        static_cast<std::uint32_t>(low),
    };
    std::string result;
    result.reserve(39);  // ceil(log10(2^128))
    while (limbs[0] != 0 || limbs[1] != 0 || limbs[2] != 0 ||
           limbs[3] != 0) {
      std::uint64_t remainder = 0;
      for (std::uint32_t& limb : limbs) {
        const std::uint64_t divided = (remainder << 32) | limb;
        limb = static_cast<std::uint32_t>(divided / 10);
        remainder = divided % 10;
      }
      result.push_back(static_cast<char>('0' + remainder));
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

 private:
  [[nodiscard]] static constexpr unsigned word_bit_length(
      std::uint64_t value) noexcept {
    unsigned result = 0;
    while (value != 0) {
      ++result;
      value >>= 1;
    }
    return result;
  }

  [[nodiscard]] constexpr std::size_t bit_length() const noexcept {
    if (high != 0) return 64U + word_bit_length(high);
    return word_bit_length(low);
  }

  [[nodiscard]] constexpr std::uint64_t shift_right(
      std::size_t amount) const noexcept {
    if (amount == 0) return low;
    if (amount < 64)
      return (low >> amount) | (high << (64 - amount));
    if (amount < 128) return high >> (amount - 64);
    return 0;
  }

  [[nodiscard]] constexpr bool bit_at(std::size_t position) const noexcept {
    if (position < 64) return ((low >> position) & 1U) != 0;
    if (position < 128) return ((high >> (position - 64)) & 1U) != 0;
    return false;
  }

  [[nodiscard]] constexpr bool any_low_bits(std::size_t count) const noexcept {
    if (count == 0) return false;
    if (count < 64) {
      const std::uint64_t mask = (std::uint64_t{1} << count) - 1;
      return (low & mask) != 0;
    }
    if (count == 64) return low != 0;
    if (count < 128) {
      const std::size_t high_count = count - 64;
      const std::uint64_t mask =
          (std::uint64_t{1} << high_count) - 1;
      return low != 0 || (high & mask) != 0;
    }
    return high != 0 || low != 0;
  }

  [[nodiscard]] static constexpr HeuristicOccurrenceCount from_shifted(
      std::uint64_t value, std::size_t amount) noexcept {
    if (value == 0) return HeuristicOccurrenceCount{};
    if (amount == 0) return HeuristicOccurrenceCount{0, value};
    if (amount < 64)
      return HeuristicOccurrenceCount{value >> (64 - amount),
                                      value << amount};
    if (amount == 64) return HeuristicOccurrenceCount{value, 0};
    if (amount < 128)
      return HeuristicOccurrenceCount{value << (amount - 64), 0};
    return HeuristicOccurrenceCount{};
  }
};

[[nodiscard]] inline HeuristicOccurrenceCount operator+(
    HeuristicOccurrenceCount left, std::uint64_t right) {
  left += right;
  return left;
}

[[nodiscard]] inline HeuristicOccurrenceCount operator+(
    std::uint64_t left, HeuristicOccurrenceCount right) {
  right += left;
  return right;
}

[[nodiscard]] inline HeuristicOccurrenceCount operator+(
    HeuristicOccurrenceCount left, const HeuristicOccurrenceCount& right) {
  left += right;
  return left;
}

}  // namespace minera
