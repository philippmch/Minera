#include "minera/llm_sql_utils.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "python_unicode_alnum.hpp"
#include "root_cause_unicode.hpp"

namespace minera {
namespace {

constexpr std::string_view kKept = " !\"'()*+,-.<=>_";
constexpr std::string_view kMarker = "ordabcchr";
constexpr std::string_view kPrefix = "ordabc";

// Code points of UTF-8 text; a stray byte stands for itself.
std::vector<char32_t> decode(std::string_view text) {
  std::vector<char32_t> out;
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i]);
    std::size_t count = 0;
    char32_t code = lead;
    if ((lead & 0xe0) == 0xc0) {
      count = 1;
      code = lead & 0x1f;
    } else if ((lead & 0xf0) == 0xe0) {
      count = 2;
      code = lead & 0x0f;
    } else if ((lead & 0xf8) == 0xf0) {
      count = 3;
      code = lead & 0x07;
    } else if (lead >= 0x80) {
      count = 9;
    }
    bool valid = count <= 3 && i + count < text.size();
    for (std::size_t k = 1; valid && k <= count; ++k) {
      const auto next = static_cast<unsigned char>(text[i + k]);
      valid = (next & 0xc0) == 0x80;
      code = (code << 6) | (next & 0x3f);
    }
    if (!valid) {
      out.push_back(lead);
      ++i;
      continue;
    }
    out.push_back(code);
    i += count + 1;
  }
  return out;
}

void encode(char32_t code, std::string& out) {
  if (code < 0x80) {
    out += static_cast<char>(code);
  } else if (code < 0x800) {
    out += static_cast<char>(0xc0 | (code >> 6));
    out += static_cast<char>(0x80 | (code & 0x3f));
  } else if (code < 0x10000) {
    out += static_cast<char>(0xe0 | (code >> 12));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (code & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (code >> 18));
    out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (code & 0x3f));
  }
}

// repr() of a Python str, cut to 200 characters as "%.200R" does.
std::string repr200(const std::vector<char32_t>& text) {
  bool single = false, dbl = false;
  for (const auto code : text) {
    single = single || code == '\'';
    dbl = dbl || code == '"';
  }
  const char32_t quote = single && !dbl ? '"' : '\'';
  std::vector<char32_t> out{quote};
  constexpr char hex[] = "0123456789abcdef";
  for (const auto code : text) {
    if (code == '\\' || code == quote) {
      out.push_back('\\');
      out.push_back(code);
    } else if (code == '\n' || code == '\r' || code == '\t') {
      out.push_back('\\');
      out.push_back(code == '\n' ? 'n' : code == '\r' ? 'r' : 't');
    } else if (!root_cause_unicode_printable(code)) {
      const int digits = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
      out.push_back('\\');
      out.push_back(digits == 2 ? 'x' : digits == 4 ? 'u' : 'U');
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) out.push_back(static_cast<char32_t>(hex[(code >> shift) & 15]));
    } else {
      out.push_back(code);
    }
  }
  out.push_back(quote);
  if (out.size() > 200) out.resize(200);
  std::string text_out;
  for (const auto code : out) encode(code, text_out);
  return text_out;
}

bool ascii_space(char32_t code) { return code == ' ' || (code >= '\t' && code <= '\r'); }

// chr(int(digits)) as Python evaluates it.
char32_t python_chr_of_int(const std::vector<char32_t>& original) {
  // int() first maps non-ASCII spaces to ' ' and non-ASCII decimals to their
  // ASCII digit; any other non-ASCII character makes the literal invalid.
  std::vector<char32_t> text;
  for (const auto code : original) {
    if (code < 0x80) {
      text.push_back(code);
    } else if (root_cause_unicode_space(code)) {
      text.push_back(' ');
    } else if (const int digit = root_cause_unicode_decimal(code); digit >= 0) {
      text.push_back(static_cast<char32_t>('0' + digit));
    } else {
      text.push_back('?');
    }
  }
  const auto fail = [&]() -> LlmSqlUtilsError {
    return LlmSqlUtilsError("ValueError", "invalid literal for int() with base 10: " + repr200(original));
  };
  std::size_t at = 0;
  while (at < text.size() && ascii_space(text[at])) ++at;
  bool negative = false;
  if (at < text.size() && (text[at] == '+' || text[at] == '-')) negative = text[at++] == '-';
  if (at >= text.size() || text[at] == '_') throw fail();
  std::size_t digits = 0;
  bool previous_underscore = false;
  long long value = 0;
  bool big = false;
  while (at < text.size() && ((text[at] >= '0' && text[at] <= '9') || text[at] == '_')) {
    if (text[at] == '_') {
      if (previous_underscore) throw fail();
      previous_underscore = true;
    } else {
      previous_underscore = false;
      ++digits;
      if (!big) {
        value = value * 10 + static_cast<long long>(text[at] - '0');
        big = value > (1LL << 40);
      }
    }
    ++at;
  }
  if (digits == 0 || previous_underscore) throw fail();
  if (digits > 4300)
    throw LlmSqlUtilsError("ValueError", "Exceeds the limit (4300 digits) for integer string conversion: value has " +
                                             std::to_string(digits) +
                                             " digits; use sys.set_int_max_str_digits() to increase the limit");
  while (at < text.size() && ascii_space(text[at])) ++at;
  if (at != text.size()) throw fail();
  if (negative) value = -value;
  if (big || value > 2147483647LL || value < -2147483648LL)
    throw LlmSqlUtilsError("OverflowError", "Python int too large to convert to C int");
  if (value < 0 || value > 0x10ffff) throw LlmSqlUtilsError("ValueError", "chr() arg not in range(0x110000)");
  return static_cast<char32_t>(value);
}

}  // namespace

std::string mask_non_alphanumeric(std::string_view text) {
  std::string out;
  for (const auto code : decode(text)) {
    if (detail::python_isalnum(code) || (code < 0x80 && kKept.find(static_cast<char>(code)) != std::string_view::npos)) {
      encode(code, out);
    } else {
      out += "ordabcchrordabc" + std::to_string(static_cast<unsigned long>(code)) + "ordabcchr";
    }
  }
  return out;
}

std::string restore_non_alphanumeric(std::string_view text) {
  std::string out;
  std::size_t start = 0;
  for (;;) {
    const auto found = text.find(kMarker, start);
    const auto piece = text.substr(start, found == std::string_view::npos ? std::string_view::npos : found - start);
    if (piece.substr(0, kPrefix.size()) == kPrefix) {
      encode(python_chr_of_int(decode(piece.substr(kPrefix.size()))), out);
    } else {
      out.append(piece);
    }
    if (found == std::string_view::npos) break;
    start = found + kMarker.size();
  }
  return out;
}

}  // namespace minera
