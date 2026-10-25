#pragma once

// algo.querying.llm.utils.sql_utils of the PM4Py 2.7.22 reference: the
// masking that keeps SQL identifiers alphanumeric.
//
// mask_non_alphanumeric keeps every character for which Python's
// str.isalnum() holds and the characters  !"'()*+,-.<=>_ ; any other
// character c becomes "ordabcchrordabc<ord(c)>ordabcchr".
// restore_non_alphanumeric splits the text at every "ordabcchr" and replaces
// each piece that starts with "ordabc" by chr(int(rest)), keeping the other
// pieces. int() and chr() behave as in Python 3.11: surrounding whitespace,
// a sign, single underscores between digits and non-ASCII decimal digits are
// accepted; anything else raises ValueError, as do more than 4,300 digits
// and code points outside [0, 0x10ffff] (OverflowError beyond a C int).
// Text is UTF-8; a restored surrogate is written as its three-byte form.

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace minera {

class LlmSqlUtilsError : public std::invalid_argument {
 public:
  LlmSqlUtilsError(std::string type, const std::string& message)
      : std::invalid_argument(message), type(std::move(type)) {}
  std::string type;  // the Python exception class
};

[[nodiscard]] std::string mask_non_alphanumeric(std::string_view text);
[[nodiscard]] std::string restore_non_alphanumeric(std::string_view text);

}  // namespace minera
