// Checks TokenInteger's inline 64-bit fast path against the limb-only
// arithmetic it replaced (kept below, verbatim apart from its name, as the
// reference): for values around every representation boundary — zero, the
// 1e9 limb base, the int64 and uint64 limits, and values far beyond — every
// operation must give the same value, decimal text, hash, ordering, double,
// narrowing conversion (or the same error), and exact ratio.
#include "minera/token_integer.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

class LegacyTokenInteger {
 public:
  LegacyTokenInteger() = default;
  template <std::integral Integer>
    requires (!std::same_as<Integer, bool> && sizeof(Integer) <= 8)
  LegacyTokenInteger(Integer value) {
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
  explicit LegacyTokenInteger(std::string_view text) {
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
  explicit operator bool() const noexcept { return !limbs_.empty(); }
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
  friend bool operator==(const LegacyTokenInteger&, const LegacyTokenInteger&) = default;
  friend std::strong_ordering operator<=>(const LegacyTokenInteger& left, const LegacyTokenInteger& right) {
    if (left.negative_ != right.negative_) return left.negative_ ? std::strong_ordering::less : std::strong_ordering::greater;
    auto result = magnitude_compare(left, right);
    if (left.negative_) result = -result;
    return result < 0 ? std::strong_ordering::less : result > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
  }
  LegacyTokenInteger operator-() const {
    auto result = *this;
    if (!result.limbs_.empty()) result.negative_ = !result.negative_;
    return result;
  }
  LegacyTokenInteger& operator+=(const LegacyTokenInteger& other) {
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
  LegacyTokenInteger& operator-=(const LegacyTokenInteger& other) { return *this += -other; }
  LegacyTokenInteger& operator*=(const LegacyTokenInteger& other) {
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
  LegacyTokenInteger& operator++() { return *this += 1; }
  LegacyTokenInteger& operator--() { return *this -= 1; }
  friend LegacyTokenInteger operator+(LegacyTokenInteger left, const LegacyTokenInteger& right) { return left += right; }
  friend LegacyTokenInteger operator-(LegacyTokenInteger left, const LegacyTokenInteger& right) { return left -= right; }
  friend LegacyTokenInteger operator*(LegacyTokenInteger left, const LegacyTokenInteger& right) { return left *= right; }
  friend std::ostream& operator<<(std::ostream& stream, const LegacyTokenInteger& value) { return stream << value.decimal(); }
 private:
  static constexpr std::uint32_t kBase = 1'000'000'000;
  std::vector<std::uint32_t> limbs_;
  bool negative_ = false;
  static int magnitude_compare(const LegacyTokenInteger& left, const LegacyTokenInteger& right) {
    if (left.limbs_.size() != right.limbs_.size()) return left.limbs_.size() < right.limbs_.size() ? -1 : 1;
    for (auto index = left.limbs_.size(); index > 0; --index)
      if (left.limbs_[index - 1] != right.limbs_[index - 1]) return left.limbs_[index - 1] < right.limbs_[index - 1] ? -1 : 1;
    return 0;
  }
  void normalize() { while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back(); if (limbs_.empty()) negative_ = false; }
  void subtract_magnitude(const LegacyTokenInteger& other) {
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

[[nodiscard]] inline double legacy_ratio(LegacyTokenInteger numerator,
                                               LegacyTokenInteger denominator) {
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
  std::vector<LegacyTokenInteger> bits;
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

using minera::TokenInteger;

std::size_t failures = 0;
void check(bool ok, const std::string& what) {
  if (!ok) {
    if (failures < 20) std::fprintf(stderr, "mismatch: %s\n", what.c_str());
    ++failures;
  }
}

template <class Integer, class Value>
std::string narrow(const Value& value) {
  try {
    return std::to_string(static_cast<Integer>(value));
  } catch (const std::exception& error) {
    return std::string("error: ") + error.what();
  }
}

template <class Value>
std::string observe(const Value& value) {
  std::string out = value.decimal();
  out += '|' + std::to_string(value.hash());
  out += '|' + std::string(static_cast<bool>(value) ? "t" : "f");
  try {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.17g", static_cast<double>(value));
    out += '|' + std::string(buffer);
  } catch (const std::exception& error) {
    out += std::string("|error: ") + error.what();
  }
  out += '|' + narrow<std::int8_t>(value) + '|' + narrow<std::uint8_t>(value) + '|' +
         narrow<std::int16_t>(value) + '|' + narrow<std::uint16_t>(value) + '|' +
         narrow<std::int32_t>(value) + '|' + narrow<std::uint32_t>(value) + '|' +
         narrow<std::int64_t>(value) + '|' + narrow<std::uint64_t>(value) + '|' +
         narrow<long long>(value) + '|' + narrow<unsigned long long>(value);
  return out;
}

template <class Value>
std::string ratio_text(const Value& numerator, const Value& denominator, bool legacy) {
  try {
    const double r = legacy ? legacy_ratio(LegacyTokenInteger(numerator.decimal()),
                                           LegacyTokenInteger(denominator.decimal()))
                            : minera::token_integer_ratio(TokenInteger(numerator.decimal()),
                                                          TokenInteger(denominator.decimal()));
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%a", r);
    return buffer;
  } catch (const std::exception& error) {
    return std::string("error: ") + error.what();
  }
}

std::vector<std::string> seeds() {
  std::vector<std::string> values;
  const auto add = [&](const std::string& text) {
    values.push_back(text);
    if (text != "0") values.push_back("-" + text);
  };
  for (const char* text : {"0", "1", "2", "7", "999999999", "1000000000", "1000000001",
                           "4294967295", "4294967296", "999999999999999999",
                           "1000000000000000000", "9223372036854775806", "9223372036854775807",
                           "9223372036854775808", "9223372036854775809", "18446744073709551615",
                           "18446744073709551616", "36893488147419103232",
                           "123456789012345678901234567890"})
    add(text);
  // Small and limb-sized values.
  for (std::int64_t v : {3ll, 10ll, 127ll, 128ll, 255ll, 256ll, 32767ll, 32768ll, 65535ll,
                         65536ll, 2147483647ll, 2147483648ll, 3037000499ll, 3037000500ll,
                         4611686018427387904ll})
    add(std::to_string(v));
  return values;
}

}  // namespace

int main() {
  const auto values = seeds();
  std::vector<TokenInteger> fast;
  std::vector<LegacyTokenInteger> slow;
  for (const auto& text : values) {
    fast.emplace_back(text);
    slow.emplace_back(text);
  }
  // Integer constructors at the boundaries.
  const auto from_integer = [&](auto value) {
    check(observe(TokenInteger(value)) == observe(LegacyTokenInteger(value)),
          "integer constructor " + std::to_string(value));
  };
  from_integer(std::numeric_limits<std::int64_t>::min());
  from_integer(std::numeric_limits<std::int64_t>::max());
  from_integer(std::numeric_limits<std::uint64_t>::max());
  from_integer(std::uint64_t{9223372036854775808ull});
  from_integer(std::numeric_limits<std::int32_t>::min());
  from_integer(std::numeric_limits<std::uint32_t>::max());
  from_integer(static_cast<signed char>(-128));
  from_integer(static_cast<unsigned char>(255));
  from_integer(static_cast<short>(-32768));
  from_integer(0);
  // Text parsing, including signs and errors.
  for (const char* text : {"+5", "-0", "+0", "007", "-000000000000000000000001", "", "-", "+",
                           "12a", " 1"}) {
    std::string a, b;
    try { a = observe(TokenInteger(std::string_view(text))); } catch (const std::exception& e) { a = e.what(); }
    try { b = observe(LegacyTokenInteger(std::string_view(text))); } catch (const std::exception& e) { b = e.what(); }
    check(a == b, std::string("parse ") + text);
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    check(observe(fast[i]) == observe(slow[i]), "value " + values[i]);
    check(observe(-fast[i]) == observe(-slow[i]), "negate " + values[i]);
    {
      auto f = fast[i]; auto s = slow[i];
      ++f; ++s;
      check(observe(f) == observe(s), "increment " + values[i]);
      --f; --f; --s; --s;
      check(observe(f) == observe(s), "decrement " + values[i]);
    }
    for (std::size_t j = 0; j < values.size(); ++j) {
      const std::string pair = values[i] + " , " + values[j];
      check(observe(fast[i] + fast[j]) == observe(slow[i] + slow[j]), "add " + pair);
      check(observe(fast[i] - fast[j]) == observe(slow[i] - slow[j]), "subtract " + pair);
      check(observe(fast[i] * fast[j]) == observe(slow[i] * slow[j]), "multiply " + pair);
      {
        auto f = fast[i]; f += fast[j]; f -= fast[j]; f *= fast[j];
        auto s = slow[i]; s += slow[j]; s -= slow[j]; s *= slow[j];
        check(observe(f) == observe(s), "compound " + pair);
      }
      check((fast[i] == fast[j]) == (slow[i] == slow[j]), "equal " + pair);
      check((fast[i] <=> fast[j]) == (slow[i] <=> slow[j]), "order " + pair);
      check(ratio_text(fast[i], fast[j], false) == ratio_text(slow[i], slow[j], true),
            "ratio " + pair);
    }
  }
  // Self-assignment, copies and moves keep the value.
  for (std::size_t i = 0; i < values.size(); ++i) {
    auto copy = fast[i];
    auto& alias = copy;
    copy = alias;
    auto moved = std::move(copy);
    TokenInteger target = 5;
    target = moved;
    check(target == fast[i] && observe(target) == observe(slow[i]), "copy " + values[i]);
    TokenInteger wide("123456789012345678901234567890");
    wide = fast[i];
    check(observe(wide) == observe(slow[i]), "assign over wide " + values[i]);
  }
  // A long product chain crosses into limbs and back.
  {
    TokenInteger f = 3;
    LegacyTokenInteger s = 3;
    for (int k = 0; k < 60; ++k) {
      f *= TokenInteger(7); s *= LegacyTokenInteger(7);
      f -= TokenInteger(k); s -= LegacyTokenInteger(k);
      check(observe(f) == observe(s), "chain " + std::to_string(k));
    }
    for (int k = 0; k < 60; ++k) {
      f = f - f + TokenInteger(k) * TokenInteger(-k);
      s = s - s + LegacyTokenInteger(k) * LegacyTokenInteger(-k);
      check(observe(f) == observe(s), "chain back " + std::to_string(k));
    }
  }
  // Marking hashes and comparisons.
  {
    minera::TokenMarking a{TokenInteger(1), TokenInteger("99999999999999999999"), TokenInteger(-3)};
    minera::TokenMarking b = a;
    assert(a == b);
    b[1] -= 1;
    assert(a != b && b < a);
    assert((minera::TokenMarking(std::vector<int>{1, 2}) == std::vector<int>{1, 2}));
  }
  std::fprintf(stderr, "%zu values, %zu mismatches\n", values.size(), failures);
  assert(failures == 0);
  return failures == 0 ? 0 : 1;
}
