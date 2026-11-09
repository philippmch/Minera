#pragma once

// A minimal XML pull parser shared by the XES log importer and the PNML model
// importer. Internal to the library (lives next to the sources, not under
// include/): it covers exactly the XML subset those formats use — start/end
// elements with attributes, entity decoding, and (opt-in) character data — and
// skips prologs, comments, CDATA, and DOCTYPE.
//
// By default the parser is lenient: it does not check well-formedness and
// passes unknown entities through.  A parser built with XmlStrictness::kStrict
// instead rejects what a conforming XML 1.0 parser rejects, with an
// XmlSyntaxError naming the line and column: malformed UTF-8 and characters
// outside the XML Char production, bare '&', undefined or external entity
// references, invalid character references, '<' in an attribute value,
// duplicate attributes, mismatched end tags, content before or after the root
// element, a missing root and a document that ends inside an element.  It
// expands internal entities declared in the DOCTYPE (with an amplification
// cap), normalizes literal TAB/CR/LF in attribute values to spaces, and limits
// nesting to 256 levels and attribute values to 10,000,000 bytes.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minera::detail {

// Raised by a strict parser for a document that is not well-formed XML.
class XmlSyntaxError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class XmlStrictness { kLenient, kStrict };

// Extra rules a strict parser may apply. The defaults keep the XES reader's
// behaviour; the model readers (BPMN, PNML, PTML) name themselves in the
// UTF-8 messages, refuse undeclared namespace prefixes as lxml does, and
// bound internal entity expansion by reference count and total size.
// check_namespaces also turns on the finer lxml rules the XES reader leaves
// alone: XML names and qualified names, the XML declaration's syntax,
// processing-instruction targets, ']]>' in content, a '/' that does not close
// its tag, and namespace URIs.
struct XmlStrictOptions {
  std::string_view reader = "XES";
  bool check_namespaces = false;
  std::size_t max_entity_expansions = static_cast<std::size_t>(-1);
  std::size_t max_expanded_bytes = static_cast<std::size_t>(-1);
};

// The strict options of the model readers.
inline XmlStrictOptions model_xml_options(std::string_view reader) {
  XmlStrictOptions options;
  options.reader = reader;
  options.check_namespaces = true;
  options.max_entity_expansions = 10000;
  options.max_expanded_bytes = 1u << 20;
  return options;
}

// The XML 1.0 Char production.
inline bool is_xml_char(std::uint32_t code_point) noexcept {
  return code_point == 0x9 || code_point == 0xA || code_point == 0xD ||
         (code_point >= 0x20 && code_point <= 0xD7FF) ||
         (code_point >= 0xE000 && code_point <= 0xFFFD) ||
         (code_point >= 0x10000 && code_point <= 0x10FFFF);
}

// Why `text` cannot appear in an XML 1.0 document, or an empty string when it
// can: it must be UTF-8 (no overlong forms or surrogates) and contain only
// Char code points. Writers use it so they never emit a document that the
// strict reader would reject.
inline std::string xml_text_problem(std::string_view text) {
  const auto code_name = [](std::uint32_t value) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string digits;
    do {
      digits.insert(digits.begin(), kDigits[value & 0xF]);
      value >>= 4;
    } while (value != 0);
    while (digits.size() < 4) digits.insert(digits.begin(), '0');
    return "character U+" + digits + " is not allowed in XML";
  };
  std::size_t index = 0;
  while (index < text.size()) {
    const auto lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80) {
      if (!is_xml_char(lead)) return code_name(lead);
      ++index;
      continue;
    }
    std::size_t length = 0;
    std::uint32_t code_point = 0;
    std::uint32_t minimum = 0;
    if (lead >= 0xC2 && lead <= 0xDF) { length = 2; code_point = lead & 0x1F; minimum = 0x80; }
    else if (lead >= 0xE0 && lead <= 0xEF) { length = 3; code_point = lead & 0x0F; minimum = 0x800; }
    else if (lead >= 0xF0 && lead <= 0xF4) { length = 4; code_point = lead & 0x07; minimum = 0x10000; }
    else return "invalid UTF-8 byte sequence";
    if (index + length > text.size()) return "invalid UTF-8 byte sequence";
    for (std::size_t offset = 1; offset < length; ++offset) {
      const auto next = static_cast<unsigned char>(text[index + offset]);
      if ((next & 0xC0) != 0x80) return "invalid UTF-8 byte sequence";
      code_point = (code_point << 6) | (next & 0x3F);
    }
    if (code_point < minimum || (code_point >= 0xD800 && code_point <= 0xDFFF) ||
        code_point > 0x10FFFF)
      return "invalid UTF-8 byte sequence";
    if (!is_xml_char(code_point)) return code_name(code_point);
    index += length;
  }
  return {};
}

// Appends a Unicode code point to a string as UTF-8.
inline void append_utf8(std::string& out, unsigned long code_point) {
  if (code_point <= 0x7F) {
    out.push_back(static_cast<char>(code_point));
  } else if (code_point <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else if (code_point <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
}

// Decodes the five predefined XML entities plus numeric character references
// into a UTF-8 string. Attribute values and text (activity names, case ids)
// may legitimately contain '&', '<', and friends in escaped form.
inline std::string decode_entities(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '&') {
      out.push_back(text[i]);
      continue;
    }
    const auto semicolon = text.find(';', i + 1);
    if (semicolon == std::string_view::npos) {
      out.push_back('&');
      continue;
    }
    const std::string_view entity = text.substr(i + 1, semicolon - i - 1);
    if (entity == "amp") {
      out.push_back('&');
    } else if (entity == "lt") {
      out.push_back('<');
    } else if (entity == "gt") {
      out.push_back('>');
    } else if (entity == "quot") {
      out.push_back('"');
    } else if (entity == "apos") {
      out.push_back('\'');
    } else if (!entity.empty() && entity.front() == '#') {
      try {
        const bool hex = entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
        const auto code = std::stoul(
            std::string(entity.substr(hex ? 2 : 1)), nullptr, hex ? 16 : 10);
        append_utf8(out, code);
      } catch (const std::exception&) {
        out.push_back('&');
        out.append(entity);
        out.push_back(';');
      }
    } else {
      // Unknown entity: pass it through verbatim rather than losing data.
      out.push_back('&');
      out.append(entity);
      out.push_back(';');
    }
    i = semicolon;
  }
  return out;
}

inline bool is_name_char(char ch) {
  return !std::isspace(static_cast<unsigned char>(ch)) && ch != '/' &&
         ch != '>' && ch != '=';
}

// Yields start and end elements with their attributes. Character data is
// skipped by default (XES keeps its values in attributes); a parser built with
// emit_character_data = true also yields non-whitespace text runs, which PNML
// needs for its <text> nodes.
class XmlPullParser {
 public:
  explicit XmlPullParser(std::string_view xml, bool emit_character_data = false,
                         bool coalesce_character_data = false,
                         XmlStrictness strictness = XmlStrictness::kLenient,
                         XmlStrictOptions strict_options = {})
      : xml_(xml), emit_character_data_(emit_character_data),
        coalesce_character_data_(coalesce_character_data),
        strict_(strictness == XmlStrictness::kStrict),
        strict_options_(strict_options) {}

  static constexpr std::size_t kMaxStrictDepth = 256;
  static constexpr std::size_t kMaxStrictValueBytes = 10000000;

  enum class Kind { StartElement, EndElement, CharacterData,
                    ProcessingInstruction, EndOfInput };

  struct Token {
    Kind kind = Kind::EndOfInput;
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text;  // decoded character data (CharacterData tokens only)
    bool self_closing = false;
  };

  Token next() {
    if (!strict_) return next_impl();
    auto token = next_impl();
    validate_characters_to(pos_);
    if (token.kind == Kind::EndOfInput) {
      if (!root_seen_) fail("the document has no root element");
      if (!open_.empty())
        fail("the document ends before <" + open_.back() + "> is closed");
    }
    return token;
  }

 private:
  Token next_impl() {
    while (pos_ < xml_.size()) {
      const auto open = xml_.find('<', pos_);
      if (strict_ && !emit_character_data_ && open != pos_) {
        const auto end = open == std::string_view::npos ? xml_.size() : open;
        check_strict_text(xml_.substr(pos_, end - pos_));
      }
      if (emit_character_data_ && coalesce_character_data_ &&
          (open != pos_ || xml_.substr(pos_, 9) == "<![CDATA["))
        return read_coalesced_text();
      if (emit_character_data_ && open != pos_) {
        const auto end = open == std::string_view::npos ? xml_.size() : open;
        const std::string_view run = xml_.substr(pos_, end - pos_);
        pos_ = end;
        if (strict_) check_strict_text(run);
        if (!whitespace_only(run)) {
          Token token;
          token.kind = Kind::CharacterData;
          token.text = strict_ ? decode_strict(run, false) : decode_entities(run);
          return token;
        }
        continue;
      }
      if (open == std::string_view::npos) {
        pos_ = xml_.size();
        return {};
      }
      pos_ = open + 1;
      if (pos_ >= xml_.size()) {
        if (strict_) fail("the document ends inside a tag");
        throw std::runtime_error("XML input ends inside a tag");
      }

      const char marker = xml_[pos_];
      if (marker == '?') {
        if (strict_) check_strict_pi_target(open);
        skip_until("?>");
        if (coalesce_character_data_) {
          Token token;
          token.kind = Kind::ProcessingInstruction;
          return token;
        }
        continue;
      }
      if (marker == '!') {
        if (starts_with("!--")) {
          skip_until("-->");
        } else if (starts_with("![CDATA[")) {
          if (strict_ && open_.empty())
            fail(root_seen_ ? "extra content at the end of the document"
                            : "a CDATA section precedes the root element");
          skip_until("]]>");
        } else if (strict_) {
          if (root_seen_ || !starts_with("!DOCTYPE"))
            fail("unexpected markup declaration");
          read_doctype();
        } else {
          skip_declaration();
        }
        continue;
      }
      if (marker == '/') {
        ++pos_;
        return read_end_tag();
      }
      return read_start_tag();
    }
    return {};
  }

  Token read_coalesced_text() {
    Token token;
    token.kind = Kind::CharacterData;
    while (pos_ < xml_.size()) {
      const auto open = xml_.find('<', pos_);
      if (open != pos_) {
        const auto end = open == std::string_view::npos ? xml_.size() : open;
        const auto run = xml_.substr(pos_, end - pos_);
        if (strict_) {
          check_strict_text(run);
          token.text += decode_strict(run, false);
        } else {
          token.text += decode_entities(run);
        }
        pos_ = end;
      }
      if (xml_.substr(pos_, 9) == "<![CDATA[") {
        if (strict_ && open_.empty())
          fail(root_seen_ ? "extra content at the end of the document"
                          : "a CDATA section precedes the root element");
        const auto end = xml_.find("]]>", pos_ + 9);
        if (end == std::string_view::npos) {
          if (strict_) fail("the document has an unterminated markup section");
          throw std::runtime_error("XML input has an unterminated CDATA section");
        }
        token.text.append(xml_.substr(pos_ + 9, end - pos_ - 9));
        pos_ = end + 3;
      } else if (xml_.substr(pos_, 4) == "<!--") {
        skip_until("-->");
      } else break;
    }
    return token;
  }
  static bool whitespace_only(std::string_view text) {
    for (const char ch : text) {
      if (!std::isspace(static_cast<unsigned char>(ch))) return false;
    }
    return true;
  }

  bool starts_with(std::string_view literal) const {
    return xml_.substr(pos_, literal.size()) == literal;
  }

  void skip_until(std::string_view terminator) {
    const auto end = xml_.find(terminator, pos_);
    if (end == std::string_view::npos) {
      if (strict_) fail("the document has an unterminated markup section");
      throw std::runtime_error("XML input has an unterminated markup section");
    }
    pos_ = end + terminator.size();
  }

  // Skips a <!DOCTYPE …> declaration, tolerating an internal [ … ] subset that
  // may itself contain '>' characters.
  void skip_declaration() {
    int bracket_depth = 0;
    while (pos_ < xml_.size()) {
      const char ch = xml_[pos_++];
      if (ch == '[') {
        ++bracket_depth;
      } else if (ch == ']') {
        if (bracket_depth > 0) --bracket_depth;
      } else if (ch == '>' && bracket_depth == 0) {
        return;
      }
    }
    throw std::runtime_error("XML input has an unterminated declaration");
  }

  void skip_space() {
    while (pos_ < xml_.size() &&
           std::isspace(static_cast<unsigned char>(xml_[pos_]))) {
      ++pos_;
    }
  }

  std::string read_name() {
    const auto start = pos_;
    while (pos_ < xml_.size() && is_name_char(xml_[pos_])) {
      ++pos_;
    }
    if (pos_ == start) {
      if (strict_) fail("a tag is missing its name");
      throw std::runtime_error("XML tag is missing a name");
    }
    const auto name = xml_.substr(start, pos_ - start);
    if (strict_ && strict_options_.check_namespaces) {
      if (!valid_xml_name(name)) fail_at(start, "'" + std::string(name) + "' is not a valid XML name");
      const auto colon = name.find(':');
      if (colon != std::string_view::npos &&
          (colon == 0 || colon + 1 == name.size() || name.find(':', colon + 1) != std::string_view::npos))
        fail_at(start, "failed to parse the qualified name '" + std::string(name) + "'");
    }
    return std::string(name);
  }

  // The XML Name production over ASCII; bytes of a multi-byte UTF-8 sequence
  // count as name characters (their encoding is checked separately).
  static bool valid_xml_name(std::string_view name) {
    const auto start_char = [](unsigned char c) {
      return c >= 0x80 || c == ':' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    };
    if (name.empty() || !start_char(static_cast<unsigned char>(name.front()))) return false;
    for (const char ch : name) {
      const auto c = static_cast<unsigned char>(ch);
      if (!start_char(c) && c != '-' && c != '.' && !(c >= '0' && c <= '9')) return false;
    }
    return true;
  }

  // A namespace name must parse as a URI reference: no spaces or characters
  // outside RFC 3986 (other than non-ASCII, which IRIs allow), and an
  // authority's port, when present, is digits.
  static bool valid_namespace_uri(std::string_view uri) {
    static constexpr std::string_view kAllowed =
        "-._~:/?#[]@!$&'()*+,;=%";
    for (const char ch : uri) {
      const auto c = static_cast<unsigned char>(ch);
      if (c >= 0x80 || std::isalnum(c)) continue;
      if (kAllowed.find(ch) == std::string_view::npos) return false;
    }
    const auto scheme_end = uri.find("://");
    if (scheme_end != std::string_view::npos) {
      const auto authority_start = scheme_end + 3;
      auto authority_end = uri.find_first_of("/?#", authority_start);
      if (authority_end == std::string_view::npos) authority_end = uri.size();
      auto authority = uri.substr(authority_start, authority_end - authority_start);
      if (const auto at = authority.rfind('@'); at != std::string_view::npos)
        authority.remove_prefix(at + 1);
      const auto bracket = authority.rfind(']');
      const auto colon = authority.rfind(':');
      if (colon != std::string_view::npos && (bracket == std::string_view::npos || colon > bracket)) {
        for (const char ch : authority.substr(colon + 1))
          if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
      }
    }
    return true;
  }

  Token read_end_tag() {
    Token token;
    token.kind = Kind::EndElement;
    token.name = read_name();
    if (strict_) {
      skip_space();
      if (pos_ >= xml_.size() || xml_[pos_] != '>')
        fail("expected '>' after </" + token.name);
      if (open_.empty()) fail("end tag </" + token.name + "> has no open element");
      if (open_.back() != token.name)
        fail("opening and ending tag mismatch: <" + open_.back() + "> and </" +
             token.name + ">");
      open_.pop_back();
      if (strict_options_.check_namespaces && !namespaces_.empty()) namespaces_.pop_back();
      if (open_.empty()) root_closed_ = true;
      ++pos_;
      return token;
    }
    const auto close = xml_.find('>', pos_);
    if (close == std::string_view::npos) {
      throw std::runtime_error("XML end tag is not closed");
    }
    pos_ = close + 1;
    return token;
  }

  Token read_start_tag() {
    Token token;
    token.kind = Kind::StartElement;
    if (strict_ && root_closed_) fail("extra content at the end of the document");
    token.name = read_name();
    // Duplicate attribute names: a linear scan for ordinary tags, a hash set
    // once a tag has many attributes.
    std::unordered_set<std::string> seen_names;

    while (true) {
      const auto before_space = pos_;
      skip_space();
      if (pos_ >= xml_.size()) {
        if (strict_) fail("the document ends inside <" + token.name + ">");
        throw std::runtime_error("XML start tag is not closed");
      }
      const char ch = xml_[pos_];
      if (ch == '>') {
        ++pos_;
        break;
      }
      if (ch == '/') {
        ++pos_;
        if (pos_ >= xml_.size() || xml_[pos_] != '>') {
          if (strict_ && strict_options_.check_namespaces)
            fail("'/' in <" + token.name + "> is not followed by '>'");
          throw std::runtime_error("XML self-closing tag is malformed");
        }
        ++pos_;
        token.self_closing = true;
        break;
      }

      if (strict_ && pos_ == before_space)
        fail("attributes of <" + token.name + "> are not separated by whitespace");
      std::string attr_name = read_name();
      skip_space();
      if (pos_ >= xml_.size() || xml_[pos_] != '=') {
        if (strict_) fail("attribute '" + attr_name + "' is missing a value");
        throw std::runtime_error("XML attribute '" + attr_name +
                                 "' is missing a value");
      }
      ++pos_;  // '='
      skip_space();
      if (pos_ >= xml_.size() || (xml_[pos_] != '"' && xml_[pos_] != '\'')) {
        if (strict_) fail("attribute '" + attr_name + "' value is not quoted");
        throw std::runtime_error("XML attribute '" + attr_name +
                                 "' value is not quoted");
      }
      const char quote = xml_[pos_++];
      const auto value_start = pos_;
      const auto value_end = xml_.find(quote, pos_);
      if (value_end == std::string_view::npos) {
        if (strict_) fail("attribute '" + attr_name + "' has an unterminated value");
        throw std::runtime_error("XML attribute '" + attr_name +
                                 "' has an unterminated value");
      }
      const auto raw = xml_.substr(value_start, value_end - value_start);
      if (strict_) {
        if (raw.size() > kMaxStrictValueBytes)
          fail("attribute '" + attr_name + "' value exceeds " +
               std::to_string(kMaxStrictValueBytes) + " bytes");
        constexpr std::size_t kLinearNames = 16;
        bool repeated = false;
        if (token.attributes.size() < kLinearNames) {
          for (const auto& [name, unused] : token.attributes)
            if (name == attr_name) repeated = true;
        } else {
          if (seen_names.empty())
            for (const auto& [name, unused] : token.attributes) seen_names.insert(name);
          repeated = !seen_names.insert(attr_name).second;
        }
        if (repeated)
          fail("attribute '" + attr_name + "' is defined twice on <" +
               token.name + ">");
      }
      auto value = strict_ ? decode_strict(raw, true) : decode_entities(raw);
      pos_ = value_end + 1;
      if (strict_ && pos_ < xml_.size() && xml_[pos_] != '>' && xml_[pos_] != '/' &&
          !std::isspace(static_cast<unsigned char>(xml_[pos_])))
        fail("attributes of <" + token.name + "> are not separated by whitespace");
      token.attributes.emplace_back(std::move(attr_name), std::move(value));
    }
    if (strict_) {
      if (open_.size() + 1 > kMaxStrictDepth)
        fail("the document nests elements deeper than " +
             std::to_string(kMaxStrictDepth) + " levels");
      root_seen_ = true;
      if (strict_options_.check_namespaces) check_namespaces(token);
      if (token.self_closing) {
        if (open_.empty()) root_closed_ = true;
      } else {
        open_.push_back(token.name);
      }
    }
    return token;
  }

  // Namespace prefixes must be declared on the element or an ancestor, as a
  // namespace-aware parser (lxml) requires; xml and xmlns are predefined.
  void check_namespaces(const Token& token) {
    std::vector<std::string> declared;
    for (const auto& [name, value] : token.attributes) {
      if ((name == "xmlns" || name.rfind("xmlns:", 0) == 0) && !value.empty() &&
          !valid_namespace_uri(value))
        fail("the namespace name '" + value + "' of " + name + " is not a valid URI");
      if (name.rfind("xmlns:", 0) == 0) {
        if (value.empty())
          fail("the namespace prefix " + name.substr(6) + " is bound to an empty URI");
        declared.push_back(name.substr(6));
      }
    }
    const auto known = [&](std::string_view prefix) {
      if (prefix == "xml" || prefix == "xmlns") return true;
      if (std::find(declared.begin(), declared.end(), prefix) != declared.end()) return true;
      for (const auto& frame : namespaces_)
        if (std::find(frame.begin(), frame.end(), prefix) != frame.end()) return true;
      return false;
    };
    const auto prefix_of = [](std::string_view name) {
      const auto colon = name.find(':');
      return colon == std::string_view::npos ? std::string_view{} : name.substr(0, colon);
    };
    const auto element_prefix = prefix_of(token.name);
    if (!element_prefix.empty() && !known(element_prefix))
      fail("namespace prefix " + std::string(element_prefix) + " on " +
           token.name.substr(element_prefix.size() + 1) + " is not defined");
    for (const auto& [name, value] : token.attributes) {
      const auto prefix = prefix_of(name);
      if (!prefix.empty() && !known(prefix))
        fail("namespace prefix " + std::string(prefix) + " for " +
             name.substr(prefix.size() + 1) + " on " + token.name + " is not defined");
    }
    if (!token.self_closing) namespaces_.push_back(std::move(declared));
  }

  // Line and column (1-based, in bytes) of a document offset, for messages.
  std::string location(std::size_t offset) const {
    offset = std::min(offset, xml_.size());
    std::size_t line = 1;
    std::size_t line_start = 0;
    for (std::size_t index = 0; index < offset; ++index) {
      if (xml_[index] == '\n') {
        ++line;
        line_start = index + 1;
      }
    }
    return "line " + std::to_string(line) + ", column " +
           std::to_string(offset - line_start + 1);
  }

  [[noreturn]] void fail_at(std::size_t offset, const std::string& reason) const {
    throw XmlSyntaxError("XML is not well-formed: " + reason + " (" +
                         location(offset) + ")");
  }
  [[noreturn]] void fail(const std::string& reason) const { fail_at(pos_, reason); }

  // Checks UTF-8 and the XML Char production for every byte before target.
  // Done lazily, as the parser advances, so a reader that stops early (a
  // trace limit) does not reject bytes it never reached.
  void validate_characters_to(std::size_t target) {
    target = std::min(target, xml_.size());
    while (validated_ < target) {
      const auto lead = static_cast<unsigned char>(xml_[validated_]);
      if (lead < 0x80) {
        if (!is_xml_char(lead))
          fail_at(validated_, "character U+" + hex(lead) + " is not allowed in XML");
        ++validated_;
        continue;
      }
      std::size_t length = 0;
      std::uint32_t code_point = 0;
      std::uint32_t minimum = 0;
      if (lead >= 0xC2 && lead <= 0xDF) { length = 2; code_point = lead & 0x1F; minimum = 0x80; }
      else if (lead >= 0xE0 && lead <= 0xEF) { length = 3; code_point = lead & 0x0F; minimum = 0x800; }
      else if (lead >= 0xF0 && lead <= 0xF4) { length = 4; code_point = lead & 0x07; minimum = 0x10000; }
      else fail_at(validated_, utf8_message("invalid"));
      if (validated_ + length > xml_.size())
        fail_at(validated_, utf8_message("truncated"));
      for (std::size_t index = 1; index < length; ++index) {
        const auto next = static_cast<unsigned char>(xml_[validated_ + index]);
        if ((next & 0xC0) != 0x80)
          fail_at(validated_, utf8_message("invalid"));
        code_point = (code_point << 6) | (next & 0x3F);
      }
      if (code_point < minimum || (code_point >= 0xD800 && code_point <= 0xDFFF) ||
          code_point > 0x10FFFF)
        fail_at(validated_, utf8_message("invalid"));
      if (!is_xml_char(code_point))
        fail_at(validated_, "character U+" + hex(code_point) + " is not allowed in XML");
      validated_ += length;
    }
  }

  std::string utf8_message(std::string_view kind) const {
    return std::string(kind) + " UTF-8 byte sequence (the " +
           std::string(strict_options_.reader) + " reader requires UTF-8)";
  }

  static std::string hex(std::uint32_t value) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string text;
    do {
      text.insert(text.begin(), kDigits[value & 0xF]);
      value >>= 4;
    } while (value != 0);
    while (text.size() < 4) text.insert(text.begin(), '0');
    return text;
  }

  // A processing instruction named "xml" is the XML declaration, allowed only
  // as the first bytes of the document (after a byte-order mark the caller
  // strips); other case variants of "xml" are reserved names.
  void check_strict_pi_target(std::size_t open) const {
    std::size_t end = open + 2;
    while (end < xml_.size() && xml_[end] != '?' &&
           !std::isspace(static_cast<unsigned char>(xml_[end])))
      ++end;
    const auto target = xml_.substr(open + 2, end - open - 2);
    if (strict_options_.check_namespaces) {
      std::size_t name_end = open + 2;
      while (name_end < xml_.size() && is_name_char(xml_[name_end]) && xml_[name_end] != '?') ++name_end;
      const auto name = xml_.substr(open + 2, name_end - open - 2);
      if (!valid_xml_name(name)) fail_at(open + 2, "a processing instruction has no valid target");
      if (name_end < xml_.size() && xml_.substr(name_end, 2) != "?>" &&
          !std::isspace(static_cast<unsigned char>(xml_[name_end])))
        fail_at(name_end, "processing instruction " + std::string(name) + ": space expected");
      if (name == "xml" && open == 0) check_xml_declaration(name_end);
    }
    if (target.size() != 3) return;
    const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    if (lower(target[0]) != 'x' || lower(target[1]) != 'm' || lower(target[2]) != 'l') return;
    if (target != "xml") fail_at(open, "Invalid PI name");
    if (open != 0) fail_at(open, "XML declaration allowed only at the start of the document");
  }

  // The XML declaration: version, then an optional encoding and standalone
  // pseudo-attribute, each double- or single-quoted.
  void check_xml_declaration(std::size_t at) const {
    const auto space = [&] {
      const auto before = at;
      while (at < xml_.size() && std::isspace(static_cast<unsigned char>(xml_[at]))) ++at;
      return at != before;
    };
    const auto pseudo = [&](std::string_view key) -> std::optional<std::string_view> {
      if (xml_.substr(at, key.size()) != key) return std::nullopt;
      at += key.size();
      space();
      if (at >= xml_.size() || xml_[at] != '=') fail_at(at, "the XML declaration expects '=' after " + std::string(key));
      ++at;
      space();
      if (at >= xml_.size() || (xml_[at] != '"' && xml_[at] != '\'')) fail_at(at, "the XML declaration's " + std::string(key) + " is not quoted");
      const char quote = xml_[at++];
      const auto close = xml_.find(quote, at);
      if (close == std::string_view::npos || xml_.substr(at, close - at).find('?') != std::string_view::npos)
        fail_at(at, "the XML declaration's " + std::string(key) + " is not closed");
      const auto value = xml_.substr(at, close - at);
      at = close + 1;
      return value;
    };
    if (!space()) fail_at(at, "the XML declaration expects a version");
    const auto version = pseudo("version");
    if (!version) fail_at(at, "the XML declaration expects a version");
    if (version->size() < 3 || version->substr(0, 2) != "1." ||
        !std::all_of(version->begin() + 2, version->end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
      fail_at(at, "unsupported XML version '" + std::string(*version) + "'");
    bool spaced = space();
    if (spaced) {
      if (const auto encoding = pseudo("encoding")) {
        const bool valid = !encoding->empty() && std::isalpha(static_cast<unsigned char>(encoding->front())) &&
            std::all_of(encoding->begin(), encoding->end(), [](char c) {
              return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-';
            });
        if (!valid) fail_at(at, "invalid encoding name '" + std::string(*encoding) + "'");
        spaced = space();
      }
    }
    if (spaced) {
      if (const auto standalone = pseudo("standalone")) {
        if (*standalone != "yes" && *standalone != "no")
          fail_at(at, "standalone accepts only 'yes' or 'no'");
        space();
      }
    }
    if (xml_.substr(at, 2) != "?>") fail_at(at, "the XML declaration expects '?>'");
  }

  // Character data between tags: only whitespace outside the root element,
  // and well-formed references inside it.
  void check_strict_text(std::string_view run) {
    if (open_.empty()) {
      if (!whitespace_only(run))
        fail(root_seen_ ? "extra content at the end of the document"
                        : "text precedes the root element");
      return;
    }
    if (strict_options_.check_namespaces && run.find("]]>") != std::string_view::npos)
      fail_at(static_cast<std::size_t>(run.data() + run.find("]]>") - xml_.data()),
              "the sequence ']]>' is not allowed in content");
    if (run.find('&') != std::string_view::npos) (void)decode_strict(run, false);
  }

  // Reads a DOCTYPE and records its internal general entity declarations.
  // Parameter entities and other declarations are skipped; external entities
  // are recorded so a reference to one fails instead of loading a file.
  void read_doctype() {
    const auto start = pos_;
    skip_declaration();
    const auto doctype = xml_.substr(start, pos_ - start);
    std::size_t at = 0;
    while ((at = doctype.find("<!ENTITY", at)) != std::string_view::npos) {
      at += 8;
      const auto space = [&] {
        while (at < doctype.size() &&
               std::isspace(static_cast<unsigned char>(doctype[at])))
          ++at;
      };
      space();
      if (at < doctype.size() && doctype[at] == '%') continue;  // parameter entity
      const auto name_start = at;
      while (at < doctype.size() && is_name_char(doctype[at]) && doctype[at] != '"' &&
             doctype[at] != '\'')
        ++at;
      const std::string name(doctype.substr(name_start, at - name_start));
      space();
      if (name.empty() || at >= doctype.size()) fail_at(start + at, "malformed ENTITY declaration");
      DeclaredEntity entity;
      if (doctype[at] == '"' || doctype[at] == '\'') {
        const char quote = doctype[at++];
        const auto close = doctype.find(quote, at);
        if (close == std::string_view::npos)
          fail_at(start + at, "malformed ENTITY declaration");
        entity.value = std::string(doctype.substr(at, close - at));
        at = close + 1;
      } else {
        entity.external = true;
      }
      entities_.emplace(name, std::move(entity));  // the first declaration wins
    }
  }

  // Strict reference decoding.  In an attribute value literal TAB, CR, LF and
  // CR LF become one space each (XML attribute-value normalization), while
  // character references keep the character they name.
  std::string decode_strict(std::string_view text, bool attribute_value,
                            std::size_t depth = 0) {
    std::string out;
    out.reserve(text.size());
    // The model readers report a bad reference at the reference itself, after
    // any bad character before it, so errors come in document order as with
    // lxml; the XES reader keeps its token-position messages.
    const bool in_document = strict_options_.check_namespaces && text.data() >= xml_.data() &&
                             text.data() + text.size() <= xml_.data() + xml_.size();
    std::size_t mark = 0;
    const auto fail_reference = [&](const std::string& reason) {
      if (!in_document) fail(reason);
      const auto at = static_cast<std::size_t>(text.data() - xml_.data()) + mark;
      validate_characters_to(at);
      fail_at(at, reason);
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
      const char ch = text[i];
      mark = i;
      if (attribute_value) {
        if (ch == '<') fail_reference("unescaped '<' in an attribute value");
        if (ch == '\r') {
          out.push_back(' ');
          if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
          continue;
        }
        if (ch == '\n' || ch == '\t') {
          out.push_back(' ');
          continue;
        }
      }
      if (ch != '&') {
        out.push_back(ch);
        continue;
      }
      const auto semicolon = text.find(';', i + 1);
      const auto name = semicolon == std::string_view::npos
                            ? std::string_view{}
                            : text.substr(i + 1, semicolon - i - 1);
      const bool name_ok =
          !name.empty() && std::none_of(name.begin(), name.end(), [](char c) {
            return std::isspace(static_cast<unsigned char>(c)) || c == '&' ||
                   c == '<' || c == '"' || c == '\'';
          });
      if (!name_ok) fail_reference("'&' does not start a valid entity or character reference");
      i = semicolon;
      if (name == "amp") out.push_back('&');
      else if (name == "lt") out.push_back('<');
      else if (name == "gt") out.push_back('>');
      else if (name == "quot") out.push_back('"');
      else if (name == "apos") out.push_back('\'');
      else if (name.front() == '#') {
        const bool hexadecimal = name.size() > 1 && name[1] == 'x';
        const auto digits = name.substr(hexadecimal ? 2 : 1);
        std::uint32_t code = 0;
        bool valid = !digits.empty() && digits.size() <= 8;
        for (const char digit : digits) {
          if (!valid) break;
          int value = -1;
          if (digit >= '0' && digit <= '9') value = digit - '0';
          else if (hexadecimal && digit >= 'a' && digit <= 'f') value = digit - 'a' + 10;
          else if (hexadecimal && digit >= 'A' && digit <= 'F') value = digit - 'A' + 10;
          if (value < 0) valid = false;
          else code = code * (hexadecimal ? 16u : 10u) + static_cast<std::uint32_t>(value);
        }
        if (!valid || !is_xml_char(code))
          fail_reference("character reference &" + std::string(name) +
               "; does not name an XML character");
        append_utf8(out, code);
      } else {
        const auto found = entities_.find(std::string(name));
        if (found == entities_.end())
          fail_reference("entity '" + std::string(name) + "' is not defined");
        if (found->second.external)
          fail_reference("entity '" + std::string(name) +
                         "' is external; external entities are not loaded");
        if (depth >= 32) fail("entity '" + std::string(name) + "' is recursive");
        if (++entity_expansions_ > strict_options_.max_entity_expansions)
          fail("entity expansion exceeds the limit of " +
               std::to_string(strict_options_.max_entity_expansions) + " references");
        auto expanded = decode_strict(found->second.value, attribute_value, depth + 1);
        expanded_bytes_ += expanded.size();
        if (expanded_bytes_ > (1u << 20) && expanded_bytes_ / 5 > xml_.size())
          fail("entity expansion exceeds the amplification limit");
        if (expanded_bytes_ > strict_options_.max_expanded_bytes)
          fail("entity expansion exceeds the limit of " +
               std::to_string(strict_options_.max_expanded_bytes) + " bytes");
        out += expanded;
      }
    }
    return out;
  }

  struct DeclaredEntity {
    std::string value;
    bool external = false;
  };

  std::string_view xml_;
  std::size_t pos_ = 0;
  bool emit_character_data_ = false;
  bool coalesce_character_data_ = false;
  // Strict-mode state.
  bool strict_ = false;
  bool root_seen_ = false;
  bool root_closed_ = false;
  std::size_t validated_ = 0;
  std::size_t expanded_bytes_ = 0;
  std::size_t entity_expansions_ = 0;
  XmlStrictOptions strict_options_;
  std::vector<std::vector<std::string>> namespaces_;
  std::vector<std::string> open_;
  std::unordered_map<std::string, DeclaredEntity> entities_;
};

// Checks a whole model document for well-formedness before a model reader
// interprets it, so that (as with lxml, which parses the document first) a
// syntax error is reported ahead of any semantic error further up the file.
// Throws XmlSyntaxError.
inline void validate_model_xml(std::string_view xml, std::string_view reader) {
  XmlPullParser parser(xml, true, false, XmlStrictness::kStrict, model_xml_options(reader));
  while (parser.next().kind != XmlPullParser::Kind::EndOfInput) {
  }
}

// Returns the value of the named attribute, or nullptr when absent.
inline const std::string* attribute(
    const std::vector<std::pair<std::string, std::string>>& attributes,
    std::string_view name) {
  for (const auto& [key, value] : attributes) {
    if (key == name) {
      return &value;
    }
  }
  return nullptr;
}

}  // namespace minera::detail
