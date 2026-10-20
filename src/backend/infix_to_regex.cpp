#include "minera/infix_to_regex.hpp"

#include <string_view>

namespace minera {
namespace {

std::string escape_literal(std::string_view literal) {
  // Python 3.11 regex-literal escaping, witnessed for all ASCII codepoints.
  constexpr std::string_view special = "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
  std::string result;
  for (const char c : literal) {
    if (special.find(c) != std::string_view::npos) result += '\\';
    result += c;
  }
  return result;
}

}  // namespace

std::string translate_infix_to_regex(const std::vector<std::string>& infix) {
  std::string result = "^";
  for (std::size_t i = 0; i < infix.size(); ++i) {
    const bool last = i + 1 == infix.size();
    if (infix[i] == "...") {
      if (last) {
        // Preserve the observed conversion even when the previous character
        // is '^' (a lone wildcard) or '*' (adjacent trailing wildcards).
        result.pop_back();
        result += "(,[^,]*)*";
      } else {
        result += "([^,]*,)*";
      }
    } else {
      result += escape_literal(infix[i]);
      if (!last) result += ',';
    }
  }
  result += '$';
  return result;
}

}  // namespace minera
