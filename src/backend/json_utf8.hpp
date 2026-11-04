#pragma once

// Keeps JSON response text valid UTF-8 whatever bytes the data holds.
//
// Policy (one for every JSON body): WTF-8 surrogate code units (ED A0..BF xx,
// how Minera stores the lone surrogates a JSON input can carry) keep their
// identity as \uXXXX escapes, as Python's json.dumps writes a lone surrogate;
// every other ill-formed byte sequence becomes one U+FFFD per maximal subpart
// (Unicode 3.9 "U+FFFD substitution of maximal subparts"), which is what
// Python's bytes.decode("utf-8", "replace") and the browser's TextDecoder do.
// XML and text bodies take the same U+FFFD substitution (repair_text_utf8).

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace minera::detail {

// The UTF-8 unit starting at `at`: {length, valid}. For an ill-formed
// sequence, length is its maximal subpart (at least one byte). A WTF-8
// surrogate is ill-formed here; callers that keep surrogates test it first.
inline std::pair<std::size_t, bool> utf8_unit(std::string_view text, std::size_t at) {
  const auto lead = static_cast<unsigned char>(text[at]);
  if (lead < 0x80) return {1, true};
  std::size_t trailing = 0;
  unsigned char low = 0x80, high = 0xbf;
  if (lead >= 0xc2 && lead <= 0xdf) {
    trailing = 1;
  } else if (lead >= 0xe0 && lead <= 0xef) {
    trailing = 2;
    if (lead == 0xe0) low = 0xa0;
    else if (lead == 0xed) high = 0x9f;
  } else if (lead >= 0xf0 && lead <= 0xf4) {
    trailing = 3;
    if (lead == 0xf0) low = 0x90;
    else if (lead == 0xf4) high = 0x8f;
  } else {
    return {1, false};
  }
  for (std::size_t k = 1; k <= trailing; ++k) {
    if (at + k >= text.size()) return {k, false};
    const auto byte = static_cast<unsigned char>(text[at + k]);
    const auto min = k == 1 ? low : static_cast<unsigned char>(0x80);
    const auto max = k == 1 ? high : static_cast<unsigned char>(0xbf);
    if (byte < min || byte > max) return {k, false};
  }
  return {trailing + 1, true};
}

// The code unit of a WTF-8 surrogate at `at`, or 0 when there is none.
inline unsigned wtf8_surrogate_at(std::string_view text, std::size_t at) {
  if (at + 2 >= text.size() || static_cast<unsigned char>(text[at]) != 0xed) return 0;
  const auto b = static_cast<unsigned char>(text[at + 1]);
  const auto c = static_cast<unsigned char>(text[at + 2]);
  if (b < 0xa0 || b > 0xbf || c < 0x80 || c > 0xbf) return 0;
  return 0xd000u | ((b & 63u) << 6) | (c & 63u);
}

// True when the text is well-formed UTF-8 (no surrogates, nothing to repair).
inline bool is_clean_utf8(std::string_view text) {
  for (std::size_t i = 0; i < text.size();) {
    if (static_cast<unsigned char>(text[i]) < 0x80) { ++i; continue; }
    const auto [length, valid] = utf8_unit(text, i);
    if (!valid) return false;
    i += length;
  }
  return true;
}

// Appends text to out under the policy above; ASCII and well-formed UTF-8 are
// copied unchanged, so the call is the identity on clean text. Only for text
// that ends up inside a JSON string (the escapes are JSON escapes).
inline void append_json_utf8(std::string& out, std::string_view text) {
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < text.size();) {
    if (static_cast<unsigned char>(text[i]) < 0x80) { out += text[i++]; continue; }
    if (const auto unit = wtf8_surrogate_at(text, i)) {
      out += "\\u";
      for (int shift = 12; shift >= 0; shift -= 4) out += hex[(unit >> shift) & 15];
      i += 3;
      continue;
    }
    const auto [length, valid] = utf8_unit(text, i);
    if (valid) out.append(text.substr(i, length));
    else out += "\\ufffd";
    i += length;
  }
}

// A JSON document whose strings may hold raw bytes, made valid UTF-8.
// Raw bytes >= 0x80 only occur inside JSON strings, so the escapes are safe.
inline std::string repair_json_utf8(std::string_view json) {
  std::string out;
  out.reserve(json.size());
  append_json_utf8(out, json);
  return out;
}

// The same substitution for a body that is not JSON (XML, CSV, plain text),
// where a JSON escape would be literal text: every ill-formed sequence, a
// WTF-8 surrogate included, becomes one raw U+FFFD (EF BF BD) per maximal
// subpart. With `xml`, the characters XML 1.0 cannot carry at all (C0
// controls other than tab, LF and CR, and U+FFFE/U+FFFF) become U+FFFD too,
// so the document stays well-formed. Identity on clean text.
inline bool xml_forbidden_at(std::string_view text, std::size_t at) {
  const auto byte = static_cast<unsigned char>(text[at]);
  if (byte < 0x20) return byte != '\t' && byte != '\n' && byte != '\r';
  return byte == 0xef && at + 2 < text.size() && static_cast<unsigned char>(text[at + 1]) == 0xbf &&
         (static_cast<unsigned char>(text[at + 2]) == 0xbe || static_cast<unsigned char>(text[at + 2]) == 0xbf);
}

inline bool is_clean_text_utf8(std::string_view text, bool xml) {
  for (std::size_t i = 0; i < text.size();) {
    if (xml && xml_forbidden_at(text, i)) return false;
    if (static_cast<unsigned char>(text[i]) < 0x80) { ++i; continue; }
    const auto [length, valid] = utf8_unit(text, i);
    if (!valid) return false;
    i += length;
  }
  return true;
}

inline std::string repair_text_utf8(std::string_view text, bool xml) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size();) {
    if (xml && xml_forbidden_at(text, i)) {
      out += "\xEF\xBF\xBD";
      i += static_cast<unsigned char>(text[i]) < 0x80 ? 1 : 3;
      continue;
    }
    if (static_cast<unsigned char>(text[i]) < 0x80) { out += text[i++]; continue; }
    const auto [length, valid] = utf8_unit(text, i);
    if (valid) out.append(text.substr(i, length));
    else out += "\xEF\xBF\xBD";
    i += length;
  }
  return out;
}

}  // namespace minera::detail
