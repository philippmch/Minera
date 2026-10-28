#pragma once

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>

#include "pnml_decimal.hpp"

namespace minera::detail {

// Python's float(str) over one complete text: surrounding Unicode whitespace,
// Unicode decimal digits read as ASCII digits (both from the pinned Unicode
// 14 classifier), underscores only between two digits, an optional sign,
// inf/infinity/nan in any case, and otherwise a decimal mantissa with an
// optional complete exponent. Hexadecimal, trailing garbage, a lone '.' and
// an exponent without digits are rejected (nullopt). Overflow gives an
// infinity and underflow a (sub)normal or zero, as float() does.
inline std::optional<double> parse_python_float_text(const std::string& text) {
  std::string ascii;
  ascii.reserve(text.size());
  for (std::size_t position = 0; position < text.size();) {
    const auto first = static_cast<unsigned char>(text[position++]);
    std::uint32_t code = first;
    unsigned count = 0;
    if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; count = 1; }
    else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; count = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { code = first & 0x07; count = 3; }
    else if (first >= 0x80) return std::nullopt;
    for (unsigned i = 0; i < count; ++i) {
      if (position == text.size()) return std::nullopt;
      const auto next = static_cast<unsigned char>(text[position++]);
      if ((next & 0xc0) != 0x80) return std::nullopt;
      code = (code << 6) | (next & 0x3f);
    }
    if (code < 0x80) ascii += static_cast<char>(code);
    else if (pnml_integer_space(code)) ascii += ' ';
    else if (const auto digit = pnml_decimal_digit(code); digit >= 0) ascii += static_cast<char>('0' + digit);
    else return std::nullopt;
  }
  const auto space = [](char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r';
  };
  std::size_t first = 0, last = ascii.size();
  while (first < last && space(ascii[first])) ++first;
  while (last > first && space(ascii[last - 1])) --last;
  const auto digit = [](char ch) { return ch >= '0' && ch <= '9'; };
  std::string number;
  char previous = '\0';
  for (std::size_t i = first; i < last; ++i) {
    const char ch = ascii[i];
    if (ch == '_') {
      if (!digit(previous)) return std::nullopt;
    } else {
      if (previous == '_' && !digit(ch)) return std::nullopt;
      number += ch;
    }
    previous = ch;
  }
  if (previous == '_' || number.empty()) return std::nullopt;

  std::size_t at = 0;
  const bool negative = number[at] == '-';
  if (number[at] == '+' || number[at] == '-') ++at;
  std::string word;
  for (std::size_t i = at; i < number.size(); ++i)
    word += static_cast<char>(number[i] >= 'A' && number[i] <= 'Z' ? number[i] - 'A' + 'a' : number[i]);
  if (word == "inf" || word == "infinity")
    return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
  if (word == "nan")
    return negative ? -std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::quiet_NaN();
  std::size_t mantissa_digits = 0;
  while (at < number.size() && digit(number[at])) { ++at; ++mantissa_digits; }
  if (at < number.size() && number[at] == '.') {
    ++at;
    while (at < number.size() && digit(number[at])) { ++at; ++mantissa_digits; }
  }
  if (!mantissa_digits) return std::nullopt;
  if (at < number.size() && (number[at] == 'e' || number[at] == 'E')) {
    ++at;
    if (at < number.size() && (number[at] == '+' || number[at] == '-')) ++at;
    std::size_t exponent_digits = 0;
    while (at < number.size() && digit(number[at])) { ++at; ++exponent_digits; }
    if (!exponent_digits) return std::nullopt;
  }
  if (at != number.size()) return std::nullopt;
  errno = 0;
  return std::strtod(number.c_str(), nullptr);
}

}  // namespace minera::detail
