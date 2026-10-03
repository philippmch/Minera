#pragma once

#include <algorithm>
#include <charconv>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace minera {

// Exact signed token arithmetic. Decimal limbs keep interchange formatting
// independent of machine integer width, including negative weighted counts.
class TokenInteger {
 public:
  TokenInteger() = default;
  template <std::integral Integer>
    requires (!std::same_as<Integer, bool> && sizeof(Integer) <= 8)
  TokenInteger(Integer value) {
    std::uint64_t magnitude;
    if constexpr (std::is_signed_v<Integer>) {
      negative_ = value < 0;
      magnitude = negative_ ? std::uint64_t(-(value + 1)) + 1
                            : std::uint64_t(value);
    } else magnitude = value;
    while (magnitude) {
      limbs_.push_back(static_cast<std::uint32_t>(magnitude % kBase));
      magnitude /= kBase;
    }
  }
  explicit TokenInteger(std::string_view text) {
    if (text.empty()) throw std::invalid_argument("token integer is empty");
    bool negative = false;
    if (text.front() == '-' || text.front() == '+') {
      negative = text.front() == '-'; text.remove_prefix(1);
    }
    if (text.empty()) throw std::invalid_argument("token integer has no digits");
    for (const auto digit : text) {
      if (digit < '0' || digit > '9')
        throw std::invalid_argument("token integer contains a non-decimal digit");
      std::uint64_t carry = static_cast<unsigned>(digit - '0');
      for (auto& limb : limbs_) {
        const auto value = std::uint64_t(limb) * 10 + carry;
        limb = static_cast<std::uint32_t>(value % kBase); carry = value / kBase;
      }
      if (carry) limbs_.push_back(static_cast<std::uint32_t>(carry));
    }
    negative_ = negative && !limbs_.empty();
  }
  [[nodiscard]] std::string decimal() const {
    if (limbs_.empty()) return "0";
    std::string result = negative_ ? "-" : "";
    result += std::to_string(limbs_.back());
    for (std::size_t index = limbs_.size() - 1; index > 0; --index) {
      const auto digits = std::to_string(limbs_[index - 1]);
      result.append(9 - digits.size(), '0'); result += digits;
    }
    return result;
  }
  explicit operator double() const {
    const auto text = decimal();
    double result = 0;
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), result);
    if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size())
      throw std::overflow_error("token integer does not fit floating-point range");
    return result;
  }
  template <std::integral Integer>
    requires (!std::same_as<Integer, bool> && sizeof(Integer) <= 8)
  explicit operator Integer() const {
    const auto limit = std::is_signed_v<Integer>
        ? std::uint64_t(std::numeric_limits<Integer>::max()) + (negative_ ? 1 : 0)
        : std::uint64_t(std::numeric_limits<Integer>::max());
    if (negative_ && !std::is_signed_v<Integer>)
      throw std::overflow_error("negative token integer does not fit unsigned range");
    std::uint64_t value = 0;
    for (auto index = limbs_.size(); index > 0; --index) {
      if (limbs_[index - 1] > limit || value > (limit - limbs_[index - 1]) / kBase)
        throw std::overflow_error("token integer does not fit requested range");
      value = value * kBase + limbs_[index - 1];
    }
    if constexpr (std::is_signed_v<Integer>) {
      if (negative_) {
        if (value == std::uint64_t(std::numeric_limits<Integer>::max()) + 1)
          return std::numeric_limits<Integer>::min();
        return -static_cast<Integer>(value);
      }
    }
    return static_cast<Integer>(value);
  }
  [[nodiscard]] std::size_t hash() const noexcept {
    std::size_t value = negative_ ? 1 : 0;
    for (const auto limb : limbs_) value = (value ^ limb) * 1099511628211ull;
    return value ^ (limbs_.size() * 0x9e3779b97f4a7c15ull);
  }
  friend bool operator==(const TokenInteger&, const TokenInteger&) = default;
  friend std::strong_ordering operator<=>(const TokenInteger& left, const TokenInteger& right) {
    if (left.negative_ != right.negative_) return left.negative_ ? std::strong_ordering::less : std::strong_ordering::greater;
    auto result = magnitude_compare(left, right);
    if (left.negative_) result = -result;
    return result < 0 ? std::strong_ordering::less : result > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
  }
  TokenInteger operator-() const {
    auto result = *this;
    if (!result.limbs_.empty()) result.negative_ = !result.negative_;
    return result;
  }
  TokenInteger& operator+=(const TokenInteger& other) {
    if (negative_ == other.negative_) {
      const auto count = std::max(limbs_.size(), other.limbs_.size());
      limbs_.resize(count, 0); std::uint64_t carry = 0;
      for (std::size_t index = 0; index < count; ++index) {
        const auto value = std::uint64_t(limbs_[index]) + (index < other.limbs_.size() ? other.limbs_[index] : 0) + carry;
        limbs_[index] = static_cast<std::uint32_t>(value % kBase); carry = value / kBase;
      }
      if (carry) limbs_.push_back(static_cast<std::uint32_t>(carry));
    } else {
      const auto order = magnitude_compare(*this, other);
      if (order == 0) { limbs_.clear(); negative_ = false; }
      else if (order > 0) subtract_magnitude(other);
      else { auto result = other; result.subtract_magnitude(*this); *this = std::move(result); }
    }
    return *this;
  }
  TokenInteger& operator-=(const TokenInteger& other) { return *this += -other; }
  TokenInteger& operator*=(const TokenInteger& other) {
    if (limbs_.empty() || other.limbs_.empty()) { limbs_.clear(); negative_ = false; return *this; }
    std::vector<std::uint32_t> result(limbs_.size() + other.limbs_.size(), 0);
    for (std::size_t left = 0; left < limbs_.size(); ++left) {
      std::uint64_t carry = 0;
      for (std::size_t right = 0; right < other.limbs_.size(); ++right) {
        const auto value = std::uint64_t(limbs_[left]) * other.limbs_[right] + result[left + right] + carry;
        result[left + right] = static_cast<std::uint32_t>(value % kBase); carry = value / kBase;
      }
      result[left + other.limbs_.size()] = static_cast<std::uint32_t>(carry);
    }
    negative_ = negative_ != other.negative_; limbs_ = std::move(result); normalize(); return *this;
  }
  TokenInteger& operator++() { return *this += 1; }
  TokenInteger& operator--() { return *this -= 1; }
  friend TokenInteger operator+(TokenInteger left, const TokenInteger& right) { return left += right; }
  friend TokenInteger operator-(TokenInteger left, const TokenInteger& right) { return left -= right; }
  friend TokenInteger operator*(TokenInteger left, const TokenInteger& right) { return left *= right; }
  friend std::ostream& operator<<(std::ostream& stream, const TokenInteger& value) { return stream << value.decimal(); }
 private:
  static constexpr std::uint32_t kBase = 1'000'000'000;
  std::vector<std::uint32_t> limbs_;
  bool negative_ = false;
  static int magnitude_compare(const TokenInteger& left, const TokenInteger& right) {
    if (left.limbs_.size() != right.limbs_.size()) return left.limbs_.size() < right.limbs_.size() ? -1 : 1;
    for (auto index = left.limbs_.size(); index > 0; --index)
      if (left.limbs_[index - 1] != right.limbs_[index - 1]) return left.limbs_[index - 1] < right.limbs_[index - 1] ? -1 : 1;
    return 0;
  }
  void normalize() { while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back(); if (limbs_.empty()) negative_ = false; }
  void subtract_magnitude(const TokenInteger& other) {
    std::int64_t borrow = 0;
    for (std::size_t index = 0; index < limbs_.size(); ++index) {
      auto value = std::int64_t(limbs_[index]) - (index < other.limbs_.size() ? other.limbs_[index] : 0) - borrow;
      borrow = value < 0;
      if (borrow) value += kBase;
      limbs_[index] = static_cast<std::uint32_t>(value);
    }
    normalize();
  }
};

// Retains normal vector access and exact values while allowing equality with
// existing integer-vector callers. No implicit narrowing of token values.
class TokenMarking : public std::vector<TokenInteger> {
 public:
  using std::vector<TokenInteger>::vector;
  TokenMarking() = default;
  template <class Integer>
  TokenMarking(const std::vector<Integer>& values) : std::vector<TokenInteger>(values.begin(), values.end()) {}
  friend bool operator==(const TokenMarking& left, const TokenMarking& right) {
    return static_cast<const std::vector<TokenInteger>&>(left) == static_cast<const std::vector<TokenInteger>&>(right);
  }
  friend auto operator<=>(const TokenMarking& left, const TokenMarking& right) {
    return static_cast<const std::vector<TokenInteger>&>(left) <=> static_cast<const std::vector<TokenInteger>&>(right);
  }
  template <class Integer>
  friend bool operator==(const TokenMarking& left, const std::vector<Integer>& right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](const auto& first, const auto& second) { return first == TokenInteger(second); });
  }
};

struct TokenMarkingHash {
  std::size_t operator()(const TokenMarking& marking) const noexcept {
    std::size_t value = 1469598103934665603ull;
    for (const auto& count : marking) value = (value ^ count.hash()) * 1099511628211ull;
    return value;
  }
};

}  // namespace minera
