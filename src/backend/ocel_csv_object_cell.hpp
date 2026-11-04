#pragma once

// The object ids of OCEL CSV object column cells, read as pm4py.read_ocel_csv
// reads them (observed black-box on pm4py 2.7.22): a cell that does not
// start with "[" holds no objects, and so does one whose text is no Python
// literal (ast.literal_eval fails: "[o1]", "['o1'", "[1+1]",
// "[float('nan')]"). The literal's elements become ids as pandas prints
// them in a column typed from every element (infer_id_dtype): str() of each
// ("[1, 'a', True]" lists "1", "a" and "True"; bytes decode as UTF-8), or,
// when the elements are numbers and None alone, as floats or complex numbers
// ("[1, None]" lists "1.0"). None and empty strings are skipped. Without an
// object table, a list, set or dict element (also a top-level tuple such as
// "['o1'],", whose elements are lists) makes the reference raise TypeError:
// unhashable type, and so does it here. Natively refused, as declared in
// docs/reference-2.7.22-parity.md: tuple elements (and, with an object
// table, the containers the reference prints), \N{...} escapes, lone
// surrogates, integers past 4300 digits, bytes with octal escapes past \377,
// and elements whose relation ids and object ids disagree in the reference.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ocel_frame_types.hpp"

namespace minera::ocel_csv_cell {

// One object id: its text (Python's str of the element) and a key equal for
// elements Python compares equal (1, 1.0, True and (1+0j) share one).
struct ObjectReference {
  std::string text;
  std::string key;
};

namespace detail {

struct Failed {};  // the cell is no Python literal: it lists no objects

[[noreturn]] inline void fail() { throw Failed{}; }

[[noreturn]] inline void unsupported(const std::string& what) {
  throw std::invalid_argument("OCEL CSV object reference cell holds " + what +
                              ", which this reader does not support");
}

// A decimal integer as a digit string with an optional leading '-'.
struct BigInt {
  bool negative = false;
  std::string digits = "0";
};

inline std::string decimal_from_base(std::string_view digits, unsigned base) {
  // Little-endian limbs of 10^9.
  std::vector<std::uint32_t> limbs{0};
  for (const char ch : digits) {
    unsigned digit = 0;
    if (ch >= '0' && ch <= '9') digit = static_cast<unsigned>(ch - '0');
    else if (ch >= 'a' && ch <= 'f') digit = static_cast<unsigned>(ch - 'a' + 10);
    else digit = static_cast<unsigned>(ch - 'A' + 10);
    std::uint64_t carry = digit;
    for (auto& limb : limbs) {
      const std::uint64_t value = static_cast<std::uint64_t>(limb) * base + carry;
      limb = static_cast<std::uint32_t>(value % 1000000000u);
      carry = value / 1000000000u;
    }
    while (carry != 0) {
      limbs.push_back(static_cast<std::uint32_t>(carry % 1000000000u));
      carry /= 1000000000u;
    }
  }
  std::string out = std::to_string(limbs.back());
  char buffer[16];
  for (std::size_t index = limbs.size() - 1; index-- > 0;) {
    std::snprintf(buffer, sizeof buffer, "%09u", static_cast<unsigned>(limbs[index]));
    out += buffer;
  }
  return out;
}

struct Value {
  enum class Kind { kNone, kBool, kInt, kFloat, kComplex, kStr, kBytes, kEllipsis,
                    kTuple, kList, kSet, kDict };
  Kind kind = Kind::kNone;
  bool flag = false;
  BigInt integer;
  double real = 0.0, imag = 0.0;
  std::string text;
  std::vector<Value> items;
};

// An AST node as ast.parse would build it: parentheses make no node.
struct Node {
  enum class Kind { kConstant, kUnary, kBinary, kDisplay };
  Kind kind = Kind::kConstant;
  char op = 0;  // '+' or '-' for kUnary and kBinary
  Value value;  // kConstant; kDisplay holds its kind and evaluated items
  std::vector<Node> children;
};

inline bool ident_start(unsigned char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch >= 0x80;
}
inline bool ident_char(unsigned char ch) { return ident_start(ch) || (ch >= '0' && ch <= '9'); }

inline void append_utf8(std::string& out, std::uint32_t code) {
  if (code >= 0xd800 && code <= 0xdfff) unsupported("a lone surrogate escape");
  if (code < 0x80) {
    out.push_back(static_cast<char>(code));
  } else if (code < 0x800) {
    out.push_back(static_cast<char>(0xc0 | (code >> 6)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
  } else if (code < 0x10000) {
    out.push_back(static_cast<char>(0xe0 | (code >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
  } else {
    out.push_back(static_cast<char>(0xf0 | (code >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
  }
}

inline bool valid_utf8(std::string_view text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto byte = static_cast<unsigned char>(text[i]);
    std::size_t length = byte < 0x80 ? 1 : (byte >> 5) == 6 ? 2 : (byte >> 4) == 14 ? 3
                                        : (byte >> 3) == 30 ? 4 : 0;
    if (length == 0 || i + length > text.size()) return false;
    std::uint32_t code = length == 1 ? byte : byte & (0x7f >> length);
    for (std::size_t k = 1; k < length; ++k) {
      const auto next = static_cast<unsigned char>(text[i + k]);
      if ((next & 0xc0) != 0x80) return false;
      code = (code << 6) | (next & 0x3f);
    }
    if ((length == 2 && code < 0x80) || (length == 3 && code < 0x800) ||
        (length == 4 && (code < 0x10000 || code > 0x10ffff)) ||
        (code >= 0xd800 && code <= 0xdfff))
      return false;
    i += length;
  }
  return true;
}

class Parser {
 public:
  explicit Parser(std::string_view text) : s_(text) {}

  Node parse_cell() {
    Node node = parse_expression_list();
    skip();
    if (pos_ != s_.size()) fail();
    return node;
  }

 private:
  std::string_view s_;
  std::size_t pos_ = 0;
  int depth_ = 0;

  bool newline_at(std::size_t at) const { return at < s_.size() && (s_[at] == '\n' || s_[at] == '\r'); }
  std::size_t newline_length(std::size_t at) const {
    return s_[at] == '\r' && at + 1 < s_.size() && s_[at + 1] == '\n' ? 2 : 1;
  }

  // Skips what separates tokens. Inside brackets newlines and comments join
  // lines; outside, the first newline ends the expression and only blank or
  // comment lines may follow.
  void skip() {
    for (;;) {
      if (pos_ >= s_.size()) return;
      const char ch = s_[pos_];
      if (ch == ' ' || ch == '\t' || ch == '\f') {
        ++pos_;
      } else if (ch == '#') {
        while (pos_ < s_.size() && !newline_at(pos_)) ++pos_;
      } else if (ch == '\\') {
        if (!newline_at(pos_ + 1)) fail();
        pos_ += 1 + newline_length(pos_ + 1);
        // A continued line must continue the expression.
        std::size_t probe = pos_;
        while (probe < s_.size() && (s_[probe] == ' ' || s_[probe] == '\t' || s_[probe] == '\f')) ++probe;
        if (probe >= s_.size()) fail();
      } else if (newline_at(pos_)) {
        if (depth_ > 0) {
          pos_ += newline_length(pos_);
          continue;
        }
        // The logical line ends: only blank and comment lines may follow.
        while (pos_ < s_.size()) {
          const char rest = s_[pos_];
          if (rest == ' ' || rest == '\t' || rest == '\f' || rest == '\n' || rest == '\r') {
            ++pos_;
          } else if (rest == '#') {
            while (pos_ < s_.size() && !newline_at(pos_)) ++pos_;
          } else {
            fail();
          }
        }
        return;
      } else {
        return;
      }
    }
  }

  char peek() {
    skip();
    return pos_ < s_.size() ? s_[pos_] : '\0';
  }

  void expect(char ch) {
    if (peek() != ch) fail();
    ++pos_;
  }

  // Expression or comma-separated tuple (top level, or inside parentheses).
  Node parse_expression_list() {
    Node first = parse_expression();
    if (peek() != ',') return first;
    Node tuple;
    tuple.kind = Node::Kind::kDisplay;
    tuple.value.kind = Value::Kind::kTuple;
    tuple.children.push_back(std::move(first));
    while (peek() == ',') {
      ++pos_;
      const char next = peek();
      if (next == '\0' || next == ')' || next == ']' || next == '}') break;
      tuple.children.push_back(parse_expression());
    }
    return tuple;
  }

  Node parse_expression() {
    Node left = parse_unary();
    for (;;) {
      const char ch = peek();
      if (ch != '+' && ch != '-') return left;
      ++pos_;
      Node node;
      node.kind = Node::Kind::kBinary;
      node.op = ch;
      node.children.push_back(std::move(left));
      node.children.push_back(parse_unary());
      left = std::move(node);
    }
  }

  Node parse_unary() {
    const char ch = peek();
    if (ch == '+' || ch == '-') {
      ++pos_;
      Node node;
      node.kind = Node::Kind::kUnary;
      node.op = ch;
      node.children.push_back(parse_unary());
      return node;
    }
    if (ch == '~') fail();
    Node atom = parse_atom();
    // A call, subscript or attribute on a literal is no literal.
    const char after = peek();
    if (after == '(' || after == '[' || after == '.') fail();
    return atom;
  }

  Node display(Value::Kind kind, char close) {
    Node node;
    node.kind = Node::Kind::kDisplay;
    node.value.kind = kind;
    ++depth_;
    ++pos_;
    if (peek() == close) {
      ++pos_;
      --depth_;
      return node;
    }
    for (;;) {
      node.children.push_back(parse_expression());
      if (kind == Value::Kind::kDict) {
        expect(':');
        node.children.push_back(parse_expression());
      } else if (node.children.size() == 1 && peek() == ':') {
        if (kind != Value::Kind::kSet) fail();
        // "{k: v, ...}" is a dict.
        node.value.kind = kind = Value::Kind::kDict;
        ++pos_;
        node.children.push_back(parse_expression());
      }
      const char next = peek();
      if (next == close) break;
      if (next != ',') fail();
      ++pos_;
      if (peek() == close) break;
    }
    ++pos_;
    --depth_;
    return node;
  }

  Node parse_atom() {
    const char ch = peek();
    if (ch == '\0') fail();
    if (ch == '[') return display(Value::Kind::kList, ']');
    if (ch == '{') return display(Value::Kind::kSet, '}');
    if (ch == '(') {
      ++depth_;
      ++pos_;
      if (peek() == ')') {
        ++pos_;
        --depth_;
        Node empty;
        empty.kind = Node::Kind::kDisplay;
        empty.value.kind = Value::Kind::kTuple;
        return empty;
      }
      Node inner = parse_expression();
      if (peek() == ',') {
        Node tuple;
        tuple.kind = Node::Kind::kDisplay;
        tuple.value.kind = Value::Kind::kTuple;
        tuple.children.push_back(std::move(inner));
        while (peek() == ',') {
          ++pos_;
          if (peek() == ')') break;
          tuple.children.push_back(parse_expression());
        }
        inner = std::move(tuple);
      }
      expect(')');
      --depth_;
      return inner;
    }
    if (ch == '.' && s_.substr(pos_, 3) == "...") {
      pos_ += 3;
      Node node;
      node.value.kind = Value::Kind::kEllipsis;
      return node;
    }
    if ((ch >= '0' && ch <= '9') ||
        (ch == '.' && pos_ + 1 < s_.size() && s_[pos_ + 1] >= '0' && s_[pos_ + 1] <= '9'))
      return parse_number();
    if (ch == '\'' || ch == '"') return parse_strings();
    if (ident_start(static_cast<unsigned char>(ch))) {
      const std::size_t begin = pos_;
      while (pos_ < s_.size() && ident_char(static_cast<unsigned char>(s_[pos_]))) ++pos_;
      const auto name = s_.substr(begin, pos_ - begin);
      if (pos_ < s_.size() && (s_[pos_] == '\'' || s_[pos_] == '"')) {
        pos_ = begin;
        return parse_strings();
      }
      Node node;
      if (name == "None") node.value.kind = Value::Kind::kNone;
      else if (name == "True" || name == "False") {
        node.value.kind = Value::Kind::kBool;
        node.value.flag = name == "True";
      } else if (name == "set") {
        expect('(');
        expect(')');
        node.kind = Node::Kind::kDisplay;
        node.value.kind = Value::Kind::kSet;
        return node;
      } else {
        fail();
      }
      return node;
    }
    fail();
  }

  // One string literal starting at pos_ (prefix included); appends its value.
  void string_literal(std::string& out, bool& is_bytes, bool& raw) {
    std::size_t begin = pos_;
    while (pos_ < s_.size() && ident_char(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    std::string prefix;
    for (std::size_t i = begin; i < pos_; ++i)
      prefix.push_back(static_cast<char>(s_[i] >= 'A' && s_[i] <= 'Z' ? s_[i] - 'A' + 'a' : s_[i]));
    if (!(prefix.empty() || prefix == "r" || prefix == "u" || prefix == "b" || prefix == "br" ||
          prefix == "rb"))
      fail();  // f-strings are no literal; other names before a quote do not parse
    is_bytes = prefix.find('b') != std::string::npos;
    raw = prefix.find('r') != std::string::npos;
    const char quote = s_[pos_];
    const bool triple = s_.substr(pos_, 3) == std::string(3, quote);
    pos_ += triple ? 3 : 1;
    for (;;) {
      if (pos_ >= s_.size()) fail();
      const char ch = s_[pos_];
      if (ch == quote && (!triple || s_.substr(pos_, 3) == std::string(3, quote))) {
        pos_ += triple ? 3 : 1;
        return;
      }
      if (!triple && newline_at(pos_)) fail();
      if (is_bytes && static_cast<unsigned char>(ch) >= 0x80) fail();
      if (ch != '\\') {
        if (ch == '\r') {
          // A newline inside a triple-quoted string reads as "\n".
          out.push_back('\n');
          pos_ += newline_length(pos_);
          continue;
        }
        out.push_back(ch);
        ++pos_;
        continue;
      }
      if (pos_ + 1 >= s_.size()) fail();
      const char escaped = s_[pos_ + 1];
      if (raw) {
        // The backslash stays; the next character never ends the string.
        out.push_back('\\');
        if (newline_at(pos_ + 1)) {
          out.push_back('\n');
          pos_ += 1 + newline_length(pos_ + 1);
        } else {
          out.push_back(escaped);
          pos_ += 2;
        }
        continue;
      }
      if (newline_at(pos_ + 1)) {
        pos_ += 1 + newline_length(pos_ + 1);
        continue;
      }
      pos_ += 2;
      switch (escaped) {
        case '\\': out.push_back('\\'); continue;
        case '\'': out.push_back('\''); continue;
        case '"': out.push_back('"'); continue;
        case 'a': out.push_back('\a'); continue;
        case 'b': out.push_back('\b'); continue;
        case 'f': out.push_back('\f'); continue;
        case 'n': out.push_back('\n'); continue;
        case 'r': out.push_back('\r'); continue;
        case 't': out.push_back('\t'); continue;
        case 'v': out.push_back('\v'); continue;
        default: break;
      }
      if (escaped >= '0' && escaped <= '7') {
        std::uint32_t code = static_cast<std::uint32_t>(escaped - '0');
        for (int digit = 0; digit < 2 && pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '7'; ++digit)
          code = code * 8 + static_cast<std::uint32_t>(s_[pos_++] - '0');
        if (is_bytes) {
          if (code > 0377) unsupported("a bytes octal escape past \\377");
          out.push_back(static_cast<char>(code));
        } else {
          append_utf8(out, code);
        }
        continue;
      }
      const auto hex = [&](int count) {
        std::uint32_t code = 0;
        for (int digit = 0; digit < count; ++digit) {
          if (pos_ >= s_.size()) fail();
          const char h = s_[pos_++];
          code <<= 4;
          if (h >= '0' && h <= '9') code += static_cast<std::uint32_t>(h - '0');
          else if (h >= 'a' && h <= 'f') code += static_cast<std::uint32_t>(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F') code += static_cast<std::uint32_t>(h - 'A' + 10);
          else fail();
        }
        return code;
      };
      if (escaped == 'x') {
        const auto code = hex(2);
        if (is_bytes) out.push_back(static_cast<char>(code));
        else append_utf8(out, code);
        continue;
      }
      if (!is_bytes && (escaped == 'u' || escaped == 'U')) {
        const auto code = hex(escaped == 'u' ? 4 : 8);
        if (code > 0x10ffff) fail();
        append_utf8(out, code);
        continue;
      }
      if (!is_bytes && escaped == 'N') unsupported("a \\N{...} escape");
      // An unknown escape keeps its backslash.
      out.push_back('\\');
      out.push_back(escaped);
    }
  }

  Node parse_strings() {
    Node node;
    std::string text;
    bool any = false, bytes = false;
    for (;;) {
      const char ch = peek();
      std::size_t probe = pos_;
      while (probe < s_.size() && ident_char(static_cast<unsigned char>(s_[probe]))) ++probe;
      const bool starts = ch == '\'' || ch == '"' ||
                          (probe > pos_ && probe < s_.size() && (s_[probe] == '\'' || s_[probe] == '"') &&
                           ident_start(static_cast<unsigned char>(ch)));
      if (!starts) break;
      bool is_bytes = false, raw = false;
      std::string part;
      string_literal(part, is_bytes, raw);
      if (any && is_bytes != bytes) fail();  // bytes next to str does not parse
      bytes = is_bytes;
      any = true;
      text += part;
    }
    node.value.kind = bytes ? Value::Kind::kBytes : Value::Kind::kStr;
    node.value.text = std::move(text);
    return node;
  }

  // digitpart: digits with single underscores between them.
  std::string digit_part(bool (*digit)(char)) {
    std::string out;
    if (pos_ >= s_.size() || !digit(s_[pos_])) fail();
    for (;;) {
      out.push_back(s_[pos_++]);
      if (pos_ < s_.size() && s_[pos_] == '_') {
        ++pos_;
        if (pos_ >= s_.size() || !digit(s_[pos_])) fail();
        continue;
      }
      if (pos_ >= s_.size() || !digit(s_[pos_])) return out;
    }
  }

  Node parse_number() {
    Node node;
    const auto decimal = [](char c) { return c >= '0' && c <= '9'; };
    if (s_[pos_] == '0' && pos_ + 1 < s_.size() &&
        std::string_view("xXoObB").find(s_[pos_ + 1]) != std::string_view::npos) {
      const char base_char = static_cast<char>(s_[pos_ + 1] | 0x20);
      pos_ += 2;
      if (pos_ < s_.size() && s_[pos_] == '_') ++pos_;
      std::string digits;
      if (base_char == 'x')
        digits = digit_part([](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); });
      else if (base_char == 'o')
        digits = digit_part([](char c) { return c >= '0' && c <= '7'; });
      else
        digits = digit_part([](char c) { return c == '0' || c == '1'; });
      if (pos_ < s_.size() && (ident_char(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.')) fail();
      node.value.kind = Value::Kind::kInt;
      node.value.integer.digits = decimal_from_base(digits, base_char == 'x' ? 16 : base_char == 'o' ? 8 : 2);
    } else {
      std::string integer, fraction, exponent;
      bool point = false, has_exponent = false;
      if (s_[pos_] != '.') integer = digit_part(decimal);
      if (pos_ < s_.size() && s_[pos_] == '.') {
        point = true;
        ++pos_;
        if (pos_ < s_.size() && decimal(s_[pos_])) fraction = digit_part(decimal);
        else if (integer.empty()) fail();
      }
      if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
        has_exponent = true;
        ++pos_;
        if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) exponent.push_back(s_[pos_++]);
        exponent += digit_part(decimal);
      }
      bool imaginary = false;
      if (pos_ < s_.size() && (s_[pos_] == 'j' || s_[pos_] == 'J')) {
        imaginary = true;
        ++pos_;
      }
      if (pos_ < s_.size() && (ident_char(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.')) fail();
      if (!point && !has_exponent && !imaginary) {
        // "007" does not parse; "00" and "0_0" are zero.
        const auto first = integer.find_first_not_of('0');
        if (first != std::string::npos && first != 0) fail();
        if (integer.size() > 4300) unsupported("an integer of more than 4300 digits");
        node.value.kind = Value::Kind::kInt;
        node.value.integer.digits = first == std::string::npos ? "0" : integer;
        return node;
      }
      std::string literal = integer.empty() ? "0" : integer;
      if (point) literal += "." + (fraction.empty() ? std::string("0") : fraction);
      if (has_exponent) literal += "e" + exponent;
      const double value = std::strtod(literal.c_str(), nullptr);
      if (imaginary) {
        node.value.kind = Value::Kind::kComplex;
        node.value.imag = value;
      } else {
        node.value.kind = Value::Kind::kFloat;
        node.value.real = value;
      }
    }
    if (node.value.integer.digits.size() > 4300) unsupported("an integer of more than 4300 digits");
    return node;
  }
};

inline double to_double(const Value& value) {
  if (value.kind == Value::Kind::kFloat) return value.real;
  const std::string text = (value.integer.negative ? "-" : "") + value.integer.digits;
  const double converted = std::strtod(text.c_str(), nullptr);
  if (std::isinf(converted)) unsupported("an integer too large for a float");
  return converted;
}

// ast.literal_eval of the node: the operators it allows on numbers.
inline Value evaluate(const Node& node) {
  using Kind = Value::Kind;
  const auto signed_number = [](const Node& operand) {
    if (operand.kind != Node::Kind::kConstant) fail();
    const auto kind = operand.value.kind;
    if (kind != Kind::kInt && kind != Kind::kFloat && kind != Kind::kComplex) fail();
    return operand.value;
  };
  switch (node.kind) {
    case Node::Kind::kConstant:
      return node.value;
    case Node::Kind::kUnary: {
      Value value = signed_number(node.children[0]);
      if (node.op == '-') {
        if (value.kind == Kind::kInt) value.integer.negative = !value.integer.negative && value.integer.digits != "0";
        value.real = -value.real;
        value.imag = -value.imag;
      }
      return value;
    }
    case Node::Kind::kBinary: {
      const auto& left_node = node.children[0];
      Value left;
      if (left_node.kind == Node::Kind::kUnary) left = evaluate(left_node);
      else left = signed_number(left_node);
      const Value right = signed_number(node.children[1]);
      if ((left.kind != Kind::kInt && left.kind != Kind::kFloat) || right.kind != Kind::kComplex) fail();
      if (node.children[1].kind != Node::Kind::kConstant) fail();
      Value out;
      out.kind = Kind::kComplex;
      const double real = to_double(left);
      // int/float + complex: the left side as complex(left, 0).
      out.real = node.op == '+' ? real + right.real : real - right.real;
      out.imag = node.op == '+' ? 0.0 + right.imag : 0.0 - right.imag;
      return out;
    }
    case Node::Kind::kDisplay: {
      Value out;
      out.kind = node.value.kind;
      for (const auto& child : node.children) out.items.push_back(evaluate(child));
      return out;
    }
  }
  fail();
}

inline std::string strip_point_zero(std::string text) {
  if (text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0) text.resize(text.size() - 2);
  return text;
}

// Python's repr of a complex number.
inline std::string complex_text(double real, double imag) {
  std::string imag_text = strip_point_zero(ocel_frame::float_repr(imag));
  if (real == 0.0 && !std::signbit(real)) return imag_text + "j";
  if (imag_text.front() != '-') imag_text = "+" + imag_text;
  return "(" + strip_point_zero(ocel_frame::float_repr(real)) + imag_text + "j)";
}

// A key equal for numbers Python compares equal: integral values as their
// exact integer, others by repr.
inline std::string real_key(double value) {
  if (std::isfinite(value) && value == std::floor(value)) {
    char buffer[400];
    std::snprintf(buffer, sizeof buffer, "%.0f", value);
    std::string text(buffer);
    if (text == "-0") text = "0";
    return "i" + text;
  }
  return "f" + ocel_frame::float_repr(value);
}

// Python's str of a scalar element, and a key equal for elements Python
// compares equal (1, 1.0, True and (1+0j) share one). nullopt for a
// container (a list, set, dict or tuple).
inline std::optional<ObjectReference> raw_reference(const Value& value) {
  using Kind = Value::Kind;
  switch (value.kind) {
    case Kind::kNone:
      return ObjectReference{"None", "n"};
    case Kind::kStr:
      return ObjectReference{value.text, "s" + value.text};
    case Kind::kBytes:
      if (!valid_utf8(value.text))
        throw std::invalid_argument(
            "OCEL CSV object reference bytes are not UTF-8; pm4py.read_ocel_csv raises "
            "UnicodeDecodeError");
      return ObjectReference{value.text, "b" + value.text};
    case Kind::kBool:
      return ObjectReference{value.flag ? "True" : "False", value.flag ? "i1" : "i0"};
    case Kind::kInt: {
      const std::string text = (value.integer.negative ? "-" : "") + value.integer.digits;
      return ObjectReference{text, "i" + text};
    }
    case Kind::kFloat:
      return ObjectReference{ocel_frame::float_repr(value.real), real_key(value.real)};
    case Kind::kComplex: {
      const std::string key = value.imag == 0.0 ? real_key(value.real)
                                                : "c" + real_key(value.real) + "|" + real_key(value.imag);
      return ObjectReference{complex_text(value.real, value.imag), key};
    }
    case Kind::kEllipsis:
      return ObjectReference{"Ellipsis", "e"};
    default:
      return std::nullopt;
  }
}

inline const char* container_name(Value::Kind kind) {
  switch (kind) {
    case Value::Kind::kList: return "list";
    case Value::Kind::kSet: return "set";
    case Value::Kind::kDict: return "dict";
    default: return "tuple";
  }
}

}  // namespace detail

using detail::Value;

// The elements of a cell's list literal, in order (None and '' included);
// nullopt for a cell that lists no objects because it is no list literal.
// Throws std::invalid_argument for a literal this reader does not support.
inline std::optional<std::vector<Value>> parse_object_cell(std::string_view cell) {
  if (cell.empty() || cell.front() != '[') return std::nullopt;
  if (cell.find('\0') != std::string_view::npos) return std::nullopt;
  try {
    detail::Parser parser(cell);
    auto value = detail::evaluate(parser.parse_cell());
    return std::move(value.items);
  } catch (const detail::Failed&) {
    return std::nullopt;
  }
}

// The dtype pandas infers for the relations' object id column from every
// element of every cell (all rows and object types, dropped rows, None and
// '' included), as observed: any text, bytes, Ellipsis or container makes it
// object (each id is str() of its element); otherwise a complex makes it
// complex, a float or None float, and integers alone stay integers (bools
// alone stay bools; bools next to numbers or None make it object; an
// integer past the uint64 range makes it object).
enum class IdDtype { kObject, kInteger, kFloat, kComplex };

inline IdDtype infer_id_dtype(const std::vector<const Value*>& values) {
  using Kind = Value::Kind;
  bool none = false, boolean = false, real = false, complex = false,
       big = false, unsigned_only = false, negative = false;
  for (const auto* value : values) {
    switch (value->kind) {
      case Kind::kNone: none = true; break;
      case Kind::kBool: boolean = true; break;
      case Kind::kInt: {
        const auto& digits = value->integer.digits;
        const auto fits = [&](std::string_view limit) {
          return digits.size() < limit.size() || (digits.size() == limit.size() && digits <= limit);
        };
        if (value->integer.negative) {
          negative = true;
          if (!fits("9223372036854775808")) big = true;
        } else if (!fits("9223372036854775807")) {
          if (fits("18446744073709551615")) unsigned_only = true;
          else big = true;
        }
        break;
      }
      case Kind::kFloat: real = true; break;
      case Kind::kComplex: complex = true; break;
      default: return IdDtype::kObject;
    }
  }
  if (boolean) return IdDtype::kObject;  // bools alone print the same either way
  if ((big || unsigned_only) && (none || real || complex))
    detail::unsupported("an integer past the int64 range next to None or other numbers");
  if (big || (unsigned_only && negative)) return IdDtype::kObject;
  if (complex) return IdDtype::kComplex;
  if (real || none) return IdDtype::kFloat;
  return IdDtype::kInteger;
}

// The id one element gets in a column of `dtype` (nullopt: None or '',
// which the reference skips), with the key and text of the element itself,
// which is what the reference's objects (without an object table) dedupe
// and print. Containers are refused: the reference raises TypeError without
// an object table and prints them with one (not supported here).
struct ElementId {
  ObjectReference relation;  // the relation's id and its value's key
  ObjectReference raw;       // str() of the element itself
};

inline std::optional<ElementId> element_id(const Value& value, IdDtype dtype,
                                           bool object_table) {
  using Kind = Value::Kind;
  if (value.kind == Kind::kNone) return std::nullopt;
  const auto raw = detail::raw_reference(value);
  if (!raw) {
    if (object_table) detail::unsupported(std::string("a ") + detail::container_name(value.kind));
    if (value.kind == Kind::kTuple) detail::unsupported("a tuple");
    const std::string name = detail::container_name(value.kind);
    throw std::invalid_argument("OCEL CSV object reference cell lists a " + name +
                                "; pm4py.read_ocel_csv raises TypeError: unhashable type: '" +
                                name + "'");
  }
  if ((value.kind == Kind::kStr || value.kind == Kind::kBytes) && value.text.empty())
    return std::nullopt;
  ObjectReference relation = *raw;
  if (dtype == IdDtype::kFloat && value.kind == Kind::kInt) {
    const double converted = detail::to_double(value);
    relation = {ocel_frame::float_repr(converted), detail::real_key(converted)};
  } else if (dtype == IdDtype::kComplex && value.kind != Kind::kComplex) {
    const double converted = detail::to_double(value);
    relation = {detail::complex_text(converted, 0.0), detail::real_key(converted)};
  }
  return ElementId{std::move(relation), *raw};
}

}  // namespace minera::ocel_csv_cell
