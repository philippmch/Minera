#pragma once

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minera::detail {

// strtod over one complete token, read the way Python's float() reads a
// number: a result below the normal range (5e-324, -1e-308, or 1e-400 that
// rounds to zero) is a value, although C libraries flag it with ERANGE. Only
// an overflow to infinity is out of range. nullopt when the text is not one
// complete number or overflows.
inline std::optional<double> parse_full_double(std::string_view text) {
  if (text.empty()) return std::nullopt;
  const std::string owned(text);
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(owned.c_str(), &end);
  if (end != owned.c_str() + owned.size()) return std::nullopt;
  if (errno == ERANGE && std::isinf(value)) return std::nullopt;
  return value;
}

// std::stod, except that an underflowing value is returned instead of
// throwing out_of_range: invalid_argument without a conversion, out_of_range
// only on overflow.
inline double stod(const std::string& text, std::size_t* used = nullptr) {
  const char* begin = text.c_str();
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(begin, &end);
  if (end == begin) throw std::invalid_argument("stod");
  if (errno == ERANGE && std::isinf(value)) throw std::out_of_range("stod");
  if (used) *used = static_cast<std::size_t>(end - begin);
  return value;
}

}  // namespace minera::detail
