#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/petri_net.hpp"
#include "pnml_decimal.hpp"

namespace minera::detail {

// Signed magnitude retains the full uint64 magnitude without primitive expansion.
inline PetriArcWeight parse_pnml_arc_weight(const std::string& text) {
  const auto invalid = [] { throw std::runtime_error("PNML arc contains an invalid integer weight"); };
  std::vector<std::uint32_t> characters;
  for (std::size_t position = 0; position < text.size();) {
    const auto first = static_cast<unsigned char>(text[position++]);
    std::uint32_t code = first;
    unsigned count = 0;
    if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; count = 1; }
    else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; count = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { code = first & 0x07; count = 3; }
    else if (first >= 0x80) invalid();
    for (unsigned i = 0; i < count; ++i) {
      if (position == text.size()) invalid();
      const auto next = static_cast<unsigned char>(text[position++]);
      if ((next & 0xc0) != 0x80) invalid();
      code = (code << 6) | (next & 0x3f);
    }
    if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
        (count == 3 && code < 0x10000) || (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff) invalid();
    characters.push_back(code);
  }
  std::size_t first = 0, last = characters.size();
  while (first < last && pnml_integer_space(characters[first])) ++first;
  while (last > first && pnml_integer_space(characters[last - 1])) --last;
  PetriArcWeight result{0, false};
  if (first < last && (characters[first] == '+' || characters[first] == '-')) {
    result.negative = characters[first] == '-'; ++first;
  }
  bool digit_seen = false, previous_digit = false;
  for (std::size_t i = first; i < last; ++i) {
    if (characters[i] == '_') {
      if (!previous_digit || i + 1 == last || pnml_decimal_digit(characters[i + 1]) < 0) invalid();
      previous_digit = false; continue;
    }
    const auto digit = pnml_decimal_digit(characters[i]);
    if (digit < 0 || result.magnitude > (std::numeric_limits<std::uint64_t>::max() - static_cast<unsigned>(digit)) / 10) invalid();
    result.magnitude = result.magnitude * 10 + static_cast<unsigned>(digit);
    digit_seen = previous_digit = true;
  }
  if (!digit_seen) invalid();
  result.negative = result.negative && result.magnitude != 0;
  return result;
}

}  // namespace minera::detail
