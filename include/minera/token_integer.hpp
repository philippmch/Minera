#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
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
//
// Every value that fits in a signed 64-bit integer is held inline, with no
// allocation; only values beyond that range spill into heap decimal limbs.
// The representation is canonical (a value has exactly one form), and every
// observable result — decimal text, hash, ordering, conversions, the overflow
// and range errors — is the one the limb arithmetic gives for the same value.
class TokenInteger {
 public:
  TokenInteger() noexcept = default;
  template <std::integral Integer>
    requires (!std::same_as<Integer, bool> && sizeof(Integer) <= 8)
  TokenInteger(Integer value) {
    if constexpr (std::is_signed_v<Integer> || sizeof(Integer) < 8) {
      small_ = static_cast<std::int64_t>(value);
    } else if (value <= static_cast<std::uint64_t>(kMax)) {
      small_ = static_cast<std::int64_t>(value);
    } else {
      wide_ = new Wide(Wide::from_magnitude(false, static_cast<std::uint64_t>(value)));
    }
  }
  explicit TokenInteger(std::string_view text) {
    if (text.empty()) throw std::invalid_argument("token integer is empty");
    bool negative = false;
    if (text.front() == '-' || text.front() == '+') {
      negative = text.front() == '-'; text.remove_prefix(1);
    }
    if (text.empty()) throw std::invalid_argument("token integer has no digits");
    Wide parsed;
    for (const auto digit : text) {
      if (digit < '0' || digit > '9')
        throw std::invalid_argument("token integer contains a non-decimal digit");
      std::uint64_t carry = static_cast<unsigned>(digit - '0');
      for (auto& limb : parsed.limbs) {
        const auto value = std::uint64_t(limb) * 10 + carry;
        limb = static_cast<std::uint32_t>(value % kBase); carry = value / kBase;
      }
      if (carry) parsed.limbs.push_back(static_cast<std::uint32_t>(carry));
    }
    parsed.negative = negative && !parsed.limbs.empty();
    assign(std::move(parsed));
  }
  TokenInteger(const TokenInteger& other)
      : small_(other.small_), wide_(other.wide_ ? new Wide(*other.wide_) : nullptr) {}
  TokenInteger(TokenInteger&& other) noexcept : small_(other.small_), wide_(other.wide_) {
    other.small_ = 0; other.wide_ = nullptr;
  }
  TokenInteger& operator=(const TokenInteger& other) {
    if (this == &other) return *this;
    if (!other.wide_) {
      delete wide_; wide_ = nullptr; small_ = other.small_;
    } else if (wide_) {
      *wide_ = *other.wide_;
    } else {
      wide_ = new Wide(*other.wide_); small_ = 0;
    }
    return *this;
  }
  TokenInteger& operator=(TokenInteger&& other) noexcept {
    if (this == &other) return *this;
    delete wide_;
    small_ = other.small_; wide_ = other.wide_;
    other.small_ = 0; other.wide_ = nullptr;
    return *this;
  }
  ~TokenInteger() { delete wide_; }

  // The value as a signed 64-bit integer, when it has that form (no copy).
  [[nodiscard]] bool is_small() const noexcept { return wide_ == nullptr; }
  [[nodiscard]] std::int64_t small_value() const noexcept { return small_; }

  [[nodiscard]] std::string decimal() const {
    if (!wide_) return std::to_string(static_cast<long long>(small_));
    return wide_->decimal();
  }
  explicit operator bool() const noexcept { return wide_ || small_ != 0; }
  explicit operator double() const {
    // Round-to-nearest-even either way: the conversion of a 64-bit integer
    // and the parse of its decimal text give the same binary64 value.
    if (!wide_) return static_cast<double>(small_);
    const auto text = wide_->decimal();
    double result = 0;
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), result);
    if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size())
      throw std::overflow_error("token integer does not fit floating-point range");
    return result;
  }
  template <std::integral Integer>
    requires (!std::same_as<Integer, bool> && sizeof(Integer) <= 8)
  explicit operator Integer() const {
    if (wide_) return wide_->template to_integer<Integer>();
    if constexpr (!std::is_signed_v<Integer>) {
      if (small_ < 0)
        throw std::overflow_error("negative token integer does not fit unsigned range");
      if (static_cast<std::uint64_t>(small_) > std::uint64_t(std::numeric_limits<Integer>::max()))
        throw std::overflow_error("token integer does not fit requested range");
    } else {
      if (small_ < std::int64_t(std::numeric_limits<Integer>::min()) ||
          small_ > std::int64_t(std::numeric_limits<Integer>::max()))
        throw std::overflow_error("token integer does not fit requested range");
    }
    return static_cast<Integer>(small_);
  }
  [[nodiscard]] std::size_t hash() const noexcept {
    if (wide_) return wide_->hash();
    const bool negative = small_ < 0;
    std::uint64_t magnitude = negative ? std::uint64_t(-(small_ + 1)) + 1 : std::uint64_t(small_);
    std::size_t value = negative ? 1 : 0;
    std::size_t count = 0;
    while (magnitude) {
      value = (value ^ static_cast<std::uint32_t>(magnitude % kBase)) * 1099511628211ull;
      magnitude /= kBase; ++count;
    }
    return value ^ (count * 0x9e3779b97f4a7c15ull);
  }
  friend bool operator==(const TokenInteger& left, const TokenInteger& right) noexcept {
    if (!left.wide_ && !right.wide_) return left.small_ == right.small_;
    if (!left.wide_ || !right.wide_) return false;  // canonical: a wide value never fits
    return left.wide_->negative == right.wide_->negative && left.wide_->limbs == right.wide_->limbs;
  }
  friend std::strong_ordering operator<=>(const TokenInteger& left, const TokenInteger& right) noexcept {
    if (!left.wide_ && !right.wide_) return left.small_ <=> right.small_;
    // A wide value lies beyond every inline one of either sign.
    if (!right.wide_) return left.wide_->negative ? std::strong_ordering::less : std::strong_ordering::greater;
    if (!left.wide_) return right.wide_->negative ? std::strong_ordering::greater : std::strong_ordering::less;
    const auto& l = *left.wide_;
    const auto& r = *right.wide_;
    if (l.negative != r.negative) return l.negative ? std::strong_ordering::less : std::strong_ordering::greater;
    auto result = Wide::magnitude_compare(l, r);
    if (l.negative) result = -result;
    return result < 0 ? std::strong_ordering::less : result > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
  }
  TokenInteger operator-() const {
    if (!wide_) {
      if (small_ != kMin) return TokenInteger(-small_);
    }
    auto value = to_wide();
    if (!value.limbs.empty()) value.negative = !value.negative;
    TokenInteger result;
    result.assign(std::move(value));
    return result;
  }
  TokenInteger& operator+=(const TokenInteger& other) {
    if (!wide_ && !other.wide_) {
      std::int64_t sum;
      if (!add_overflow(small_, other.small_, sum)) { small_ = sum; return *this; }
    }
    auto value = to_wide();
    value.add(other.wide_ ? *other.wide_ : Wide::from_small(other.small_));
    assign(std::move(value));
    return *this;
  }
  TokenInteger& operator-=(const TokenInteger& other) {
    if (!wide_ && !other.wide_) {
      std::int64_t difference;
      if (!sub_overflow(small_, other.small_, difference)) { small_ = difference; return *this; }
    }
    return *this += -other;
  }
  TokenInteger& operator*=(const TokenInteger& other) {
    if (!wide_ && !other.wide_) {
      std::int64_t product;
      if (!mul_overflow(small_, other.small_, product)) { small_ = product; return *this; }
    }
    auto value = to_wide();
    value.multiply(other.wide_ ? *other.wide_ : Wide::from_small(other.small_));
    assign(std::move(value));
    return *this;
  }
  TokenInteger& operator++() { return *this += 1; }
  TokenInteger& operator--() { return *this -= 1; }
  friend TokenInteger operator+(TokenInteger left, const TokenInteger& right) { return left += right; }
  friend TokenInteger operator-(TokenInteger left, const TokenInteger& right) { return left -= right; }
  friend TokenInteger operator*(TokenInteger left, const TokenInteger& right) { return left *= right; }
  friend std::ostream& operator<<(std::ostream& stream, const TokenInteger& value) { return stream << value.decimal(); }

 private:
  static constexpr std::uint32_t kBase = 1'000'000'000;
  static constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
  static constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

  static bool add_overflow(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if ((b > 0 && a > kMax - b) || (b < 0 && a < kMin - b)) return true;
    out = a + b;
    return false;
  }
  static bool sub_overflow(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if ((b < 0 && a > kMax + b) || (b > 0 && a < kMin + b)) return true;
    out = a - b;
    return false;
  }
  static bool mul_overflow(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if (a == 0 || b == 0) {
      out = 0;
      return false;
    }
    if (a == -1) {
      if (b == kMin) return true;
      out = -b;
      return false;
    }
    if (b == -1) {
      if (a == kMin) return true;
      out = -a;
      return false;
    }
    if (a > 0 ? (b > 0 ? a > kMax / b : b < kMin / a)
              : (b > 0 ? a < kMin / b : a < kMax / b)) return true;
    out = a * b;
    return false;
  }

  // Signed magnitude in base-1e9 limbs, least significant first, with no
  // leading zero limb and no negative zero: the arbitrary-precision form.
  struct Wide {
    std::vector<std::uint32_t> limbs;
    bool negative = false;

    static Wide from_magnitude(bool negative, std::uint64_t magnitude) {
      Wide value;
      while (magnitude) {
        value.limbs.push_back(static_cast<std::uint32_t>(magnitude % kBase));
        magnitude /= kBase;
      }
      value.negative = negative && !value.limbs.empty();
      return value;
    }
    static Wide from_small(std::int64_t inline_value) {
      return from_magnitude(inline_value < 0,
                            inline_value < 0 ? std::uint64_t(-(inline_value + 1)) + 1 : std::uint64_t(inline_value));
    }
    // The value as an int64 when it lies in that range.
    [[nodiscard]] bool fits_small(std::int64_t& out) const noexcept {
      if (limbs.size() > 3) return false;
      if (limbs.size() == 3 && limbs[2] > 9) return false;
      std::uint64_t magnitude = 0;
      for (auto index = limbs.size(); index > 0; --index)
        magnitude = magnitude * kBase + limbs[index - 1];  // < 1e19: no wrap
      const std::uint64_t limit = std::uint64_t(kMax) + (negative ? 1 : 0);
      if (magnitude > limit) return false;
      out = negative ? (magnitude == std::uint64_t(kMax) + 1 ? kMin : -static_cast<std::int64_t>(magnitude))
                     : static_cast<std::int64_t>(magnitude);
      return true;
    }
    [[nodiscard]] std::string decimal() const {
      if (limbs.empty()) return "0";
      std::string result = negative ? "-" : "";
      result += std::to_string(limbs.back());
      for (std::size_t index = limbs.size() - 1; index > 0; --index) {
        const auto digits = std::to_string(limbs[index - 1]);
        result.append(9 - digits.size(), '0'); result += digits;
      }
      return result;
    }
    template <class Integer>
    [[nodiscard]] Integer to_integer() const {
      const auto limit = std::is_signed_v<Integer>
          ? std::uint64_t(std::numeric_limits<Integer>::max()) + (negative ? 1 : 0)
          : std::uint64_t(std::numeric_limits<Integer>::max());
      if (negative && !std::is_signed_v<Integer>)
        throw std::overflow_error("negative token integer does not fit unsigned range");
      std::uint64_t value = 0;
      for (auto index = limbs.size(); index > 0; --index) {
        if (limbs[index - 1] > limit || value > (limit - limbs[index - 1]) / kBase)
          throw std::overflow_error("token integer does not fit requested range");
        value = value * kBase + limbs[index - 1];
      }
      if constexpr (std::is_signed_v<Integer>) {
        if (negative) {
          if (value == std::uint64_t(std::numeric_limits<Integer>::max()) + 1)
            return std::numeric_limits<Integer>::min();
          return -static_cast<Integer>(value);
        }
      }
      return static_cast<Integer>(value);
    }
    [[nodiscard]] std::size_t hash() const noexcept {
      std::size_t value = negative ? 1 : 0;
      for (const auto limb : limbs) value = (value ^ limb) * 1099511628211ull;
      return value ^ (limbs.size() * 0x9e3779b97f4a7c15ull);
    }
    static int magnitude_compare(const Wide& left, const Wide& right) {
      if (left.limbs.size() != right.limbs.size()) return left.limbs.size() < right.limbs.size() ? -1 : 1;
      for (auto index = left.limbs.size(); index > 0; --index)
        if (left.limbs[index - 1] != right.limbs[index - 1]) return left.limbs[index - 1] < right.limbs[index - 1] ? -1 : 1;
      return 0;
    }
    void normalize() { while (!limbs.empty() && limbs.back() == 0) limbs.pop_back(); if (limbs.empty()) negative = false; }
    void subtract_magnitude(const Wide& other) {
      std::int64_t borrow = 0;
      for (std::size_t index = 0; index < limbs.size(); ++index) {
        auto value = std::int64_t(limbs[index]) - (index < other.limbs.size() ? other.limbs[index] : 0) - borrow;
        borrow = value < 0;
        if (borrow) value += kBase;
        limbs[index] = static_cast<std::uint32_t>(value);
      }
      normalize();
    }
    void add(const Wide& other) {
      if (negative == other.negative) {
        const auto count = std::max(limbs.size(), other.limbs.size());
        limbs.resize(count, 0); std::uint64_t carry = 0;
        for (std::size_t index = 0; index < count; ++index) {
          const auto value = std::uint64_t(limbs[index]) + (index < other.limbs.size() ? other.limbs[index] : 0) + carry;
          limbs[index] = static_cast<std::uint32_t>(value % kBase); carry = value / kBase;
        }
        if (carry) limbs.push_back(static_cast<std::uint32_t>(carry));
      } else {
        const auto order = magnitude_compare(*this, other);
        if (order == 0) { limbs.clear(); negative = false; }
        else if (order > 0) subtract_magnitude(other);
        else { auto result = other; result.subtract_magnitude(*this); *this = std::move(result); }
      }
    }
    void multiply(const Wide& other) {
      if (limbs.empty() || other.limbs.empty()) { limbs.clear(); negative = false; return; }
      std::vector<std::uint32_t> result(limbs.size() + other.limbs.size(), 0);
      for (std::size_t left = 0; left < limbs.size(); ++left) {
        std::uint64_t carry = 0;
        for (std::size_t right = 0; right < other.limbs.size(); ++right) {
          const auto value = std::uint64_t(limbs[left]) * other.limbs[right] + result[left + right] + carry;
          result[left + right] = static_cast<std::uint32_t>(value % kBase); carry = value / kBase;
        }
        result[left + other.limbs.size()] = static_cast<std::uint32_t>(carry);
      }
      negative = negative != other.negative; limbs = std::move(result); normalize();
    }
  };

  [[nodiscard]] Wide to_wide() const { return wide_ ? *wide_ : Wide::from_small(small_); }
  // Stores `value` in canonical form: inline whenever it fits.
  void assign(Wide&& value) {
    // Not named `small`: the Windows SDK's rpcndr.h defines that as a macro.
    std::int64_t inline_value = 0;
    if (value.fits_small(inline_value)) {
      delete wide_; wide_ = nullptr; small_ = inline_value;
    } else if (wide_) {
      *wide_ = std::move(value); small_ = 0;
    } else {
      wide_ = new Wide(std::move(value)); small_ = 0;
    }
  }

  std::int64_t small_ = 0;  // the value, when wide_ is null
  Wide* wide_ = nullptr;     // owned; set exactly when the value exceeds int64
};

// Divide exact integers with binary64 round-to-nearest, ties-to-even. Converting
// the operands first loses low bits and can overflow even for a finite ratio.
[[nodiscard]] inline double token_integer_ratio(TokenInteger numerator,
                                               TokenInteger denominator) {
  if (denominator == 0) throw std::domain_error("division by zero");
  const bool negative = (numerator < 0) != (denominator < 0);
  if (numerator < 0) numerator = -numerator;
  if (denominator < 0) denominator = -denominator;
  if (numerator == 0) return negative ? -0.0 : 0.0;

  int exponent = 0;
  if (numerator >= denominator) {
    auto boundary = denominator;
    while (boundary * 2 <= numerator) {
      boundary *= 2;
      if (++exponent > 1023)
        throw std::overflow_error("integer division result too large for a float");
    }
  } else {
    auto boundary = numerator;
    while (boundary < denominator) {
      boundary *= 2;
      if (--exponent < -1075) return negative ? -0.0 : 0.0;
    }
  }
  const int shift = exponent >= -1022 ? 52 - exponent : 1074;
  if (shift >= 0) {
    for (int i = 0; i < shift; ++i) numerator *= 2;
  } else {
    for (int i = 0; i < -shift; ++i) denominator *= 2;
  }
  std::vector<TokenInteger> bits;
  bits.reserve(53);
  bits.push_back(denominator);
  for (int i = 1; i <= 52; ++i) bits.push_back(bits.back() * 2);
  std::uint64_t significand = 0;
  for (int i = 52; i >= 0; --i) {
    if (numerator >= bits[static_cast<std::size_t>(i)]) {
      numerator -= bits[static_cast<std::size_t>(i)];
      significand |= std::uint64_t{1} << i;
    }
  }
  const auto twice_remainder = numerator * 2;
  if (twice_remainder > denominator ||
      (twice_remainder == denominator && (significand & 1))) ++significand;
  const double result = std::ldexp(static_cast<double>(significand), -shift);
  if (!std::isfinite(result))
    throw std::overflow_error("integer division result too large for a float");
  return negative ? -result : result;
}

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
