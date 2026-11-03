#include "minera/powl.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {

void validate_frequency(const PowlFrequency& f) {
  if ((f.minimum != 0 && f.minimum != 1) ||
      (f.maximum && (*f.maximum != 1 || *f.maximum < f.minimum))) {
    throw std::invalid_argument("POWL frequency must be min 0/1 and max one/unbounded");
  }
}

std::string quote(const std::string& value) {
  std::string out = "\"";
  for (const unsigned char c : value) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          const char* digits = "0123456789abcdef";
          out += "\\u00";
          out += digits[c >> 4]; out += digits[c & 15];
        } else out += static_cast<char>(c);
    }
  }
  out += '"';
  return out;
}

class Parser {
 public:
  explicit Parser(const std::string& text) : text_(text) {}

  PowlNode run() {
    skip();
    auto result = node();
    skip();
    if (pos_ != text_.size()) fail("trailing characters");
    return result;
  }

 private:
  const std::string& text_;
  std::size_t pos_ = 0;
  // node() recurses once per nesting level; bound it so a deeply nested
  // model is a parse error rather than a stack overflow of the caller.
  static constexpr std::size_t kMaxDepth = 1000;
  std::size_t depth_ = 0;

  [[noreturn]] void fail(const char* message) const {
    throw std::invalid_argument(std::string("invalid POWL at offset ") +
                                std::to_string(pos_) + ": " + message);
  }
  void skip() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
  }
  bool consume(char c) {
    skip();
    if (pos_ < text_.size() && text_[pos_] == c) { ++pos_; return true; }
    return false;
  }
  void expect(char c) { if (!consume(c)) fail("expected delimiter"); }
  bool operator_token(const char* token, char following) {
    skip();
    const std::size_t n = std::char_traits<char>::length(token);
    if (text_.compare(pos_, n, token) != 0) return false;
    std::size_t next = pos_ + n;
    while (next < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[next]))) {
      ++next;
    }
    if (next >= text_.size() || text_[next] != following) return false;
    pos_ += n;
    return true;
  }
  bool tau_token() {
    skip();
    if (text_.compare(pos_, 3, "tau") != 0) return false;
    std::size_t next = pos_ + 3;
    while (next < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[next]))) {
      ++next;
    }
    if (next < text_.size() && text_[next] != ',' && text_[next] != ';' &&
        text_[next] != ')' && text_[next] != '}') {
      return false;
    }
    pos_ += 3;
    return true;
  }
  std::string quoted() {
    skip();
    if (pos_ >= text_.size() || text_[pos_] != '"') fail("expected quoted label");
    ++pos_; std::string out;
    while (pos_ < text_.size()) {
      char c = text_[pos_++];
      if (c == '"') return out;
      if (c != '\\') { out += c; continue; }
      if (pos_ >= text_.size()) fail("unterminated escape");
      c = text_[pos_++];
      switch (c) {
        case 'n': out += '\n'; break; case 'r': out += '\r'; break;
        case 't': out += '\t'; break; case '\\': out += '\\'; break;
        case '"': out += '"'; break;
        case 'u': {
          if (pos_ + 4 > text_.size()) fail("short unicode escape");
          unsigned value = 0;
          for (int i = 0; i < 4; ++i) {
            const char h = text_[pos_++]; unsigned d;
            if (h >= '0' && h <= '9') d = h - '0';
            else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
            else fail("bad unicode escape");
            value = value * 16 + d;
          }
          if (value <= 0x7f) out += static_cast<char>(value);
          else fail("non-ASCII unicode escape is unsupported");
          break;
        }
        default: fail("unknown escape");
      }
    }
    fail("unterminated quoted label");
  }
  std::string single_quoted() {
    skip();
    if (pos_ >= text_.size() || text_[pos_] != '\'')
      fail("expected single-quoted label");
    ++pos_;
    std::string out;
    while (pos_ < text_.size()) {
      char c = text_[pos_++];
      if (c == '\'') return out;
      if (c != '\\') {
        out += c;
        continue;
      }
      if (pos_ >= text_.size()) fail("unterminated escape");
      c = text_[pos_++];
      if (c != '\\' && c != '\'') fail("unknown single-quote escape");
      out += c;
    }
    fail("unterminated single-quoted label");
  }
  std::size_t number() {
    skip();
    const auto begin = pos_;
    while (pos_ < text_.size() &&
           std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
    if (begin == pos_) fail("expected index");
    try {
      const auto parsed = std::stoull(text_.substr(begin, pos_ - begin));
      if (parsed > std::numeric_limits<std::size_t>::max())
        fail("index is too large");
      return static_cast<std::size_t>(parsed);
    } catch (const std::invalid_argument&) {
      fail("invalid index");
    } catch (const std::out_of_range&) {
      fail("index is too large");
    }
  }
  std::string raw_label() {
    skip();
    const auto begin = pos_;
    while (pos_ < text_.size() && text_[pos_] != ',' &&
           text_[pos_] != ';' && text_[pos_] != ')' && text_[pos_] != '}') {
      ++pos_;
    }
    auto end = pos_;
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text_[end - 1]))) {
      --end;
    }
    if (begin == end) fail("expected node");
    return text_.substr(begin, end - begin);
  }
  PowlNode node() {
    if (depth_ >= kMaxDepth) fail("nesting exceeds 1000 levels");
    ++depth_;
    struct Leave { std::size_t& depth; ~Leave() { --depth; } } leave{depth_};
    skip();
    if (tau_token()) return PowlNode::silent();
    if (operator_token("X", '(')) return composite('X');
    if (operator_token("*", '(')) return composite('*');
    if (operator_token("PO", '=')) return partial_order(true);
    if (operator_token("PO", '(')) return partial_order(false);
    if (operator_token("T", '(')) {
      expect('('); const auto s = quoted(); skip();
      PowlFrequency f;
      if (consume(',')) {
        const auto minimum = number();
        if (minimum > static_cast<std::size_t>(
                          std::numeric_limits<int>::max())) {
          fail("frequency is too large");
        }
        f.minimum = static_cast<int>(minimum);
        expect(',');
        skip();
        if (consume('*')) f.maximum = std::nullopt;
        else {
          const auto maximum = number();
          if (maximum > static_cast<std::size_t>(
                            std::numeric_limits<int>::max())) {
            fail("frequency is too large");
          }
          f.maximum = static_cast<int>(maximum);
        }
      }
      expect(')'); return PowlNode::transition(s, f);
    }
    skip();
    if (pos_ < text_.size() && text_[pos_] == '"') return PowlNode::transition(quoted());
    if (pos_ < text_.size() && text_[pos_] == '\'')
      return PowlNode::transition(single_quoted());
    return PowlNode::transition(raw_label());
  }
  PowlNode composite(char kind) {
    expect('('); std::vector<PowlNode> children;
    skip(); if (!consume(')')) {
      for (;;) {
        children.push_back(node()); skip();
        if (consume(')')) break;
        if (!consume(',')) expect(';');
      }
    }
    if (kind == 'X') return PowlNode::xor_node(std::move(children));
    if (children.size() != 2) fail("loop requires exactly two children");
    return PowlNode::loop(std::move(children[0]), std::move(children[1]));
  }
  static std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
  }
  std::string relation_item() {
    skip();
    const auto begin = pos_;
    int parentheses = 0;
    int braces = 0;
    char quote_character = '\0';
    bool escaped = false;
    while (pos_ < text_.size()) {
      const char c = text_[pos_];
      if (quote_character != '\0') {
        ++pos_;
        if (escaped) {
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == quote_character) {
          quote_character = '\0';
        }
        continue;
      }
      if (c == '"' || c == '\'') {
        quote_character = c;
        ++pos_;
      } else if (c == '(') {
        ++parentheses;
        ++pos_;
      } else if (c == ')') {
        if (parentheses == 0) break;
        --parentheses;
        ++pos_;
      } else if (c == '{') {
        ++braces;
        ++pos_;
      } else if (c == '}') {
        if (braces == 0 && parentheses == 0) break;
        --braces;
        ++pos_;
      } else if ((c == ',' || c == ';') && parentheses == 0 && braces == 0) {
        break;
      } else {
        ++pos_;
      }
    }
    const auto item = trim(text_.substr(begin, pos_ - begin));
    if (item.empty()) fail("expected relation");
    return item;
  }
  static std::size_t arrow_position(const std::string& item) {
    int parentheses = 0;
    int braces = 0;
    char quote_character = '\0';
    bool escaped = false;
    for (std::size_t i = 0; i + 2 < item.size(); ++i) {
      const char c = item[i];
      if (quote_character != '\0') {
        if (escaped) {
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == quote_character) {
          quote_character = '\0';
        }
        continue;
      }
      if (c == '"' || c == '\'') quote_character = c;
      else if (c == '(') ++parentheses;
      else if (c == ')') --parentheses;
      else if (c == '{') ++braces;
      else if (c == '}') --braces;
      else if (c == '-' && item[i + 1] == '-' && item[i + 2] == '>' &&
               parentheses == 0 && braces == 0) {
        return i;
      }
    }
    return std::string::npos;
  }
  static bool decimal(const std::string& value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(),
                       [](unsigned char c) { return std::isdigit(c); });
  }
  std::size_t resolve_reference(
      const std::string& raw,
      const std::vector<std::string>& parsed_representations,
      const std::vector<PowlNode>& children) {
    const auto reference = trim(raw);
    if (reference.size() > 1 && reference[0] == '@' &&
        decimal(reference.substr(1))) {
      try {
        const auto index = std::stoull(reference.substr(1));
        if (index >= children.size())
          fail("relation index is out of range");
        return static_cast<std::size_t>(index);
      } catch (const std::out_of_range&) {
        fail("relation index is too large");
      }
    }
    if (decimal(reference)) {
      try {
        const auto index = std::stoull(reference);
        if (index >= children.size())
          fail("relation index is out of range");
        return static_cast<std::size_t>(index);
      } catch (const std::out_of_range&) {
        fail("relation index is too large");
      }
    }
    std::vector<std::size_t> matches;
    for (std::size_t i = 0; i < children.size(); ++i) {
      bool match = reference == parsed_representations[i] ||
                   reference == children[i].serialize();
      if (children[i].kind() == PowlNodeKind::kTransition) {
        match = match || reference == children[i].label() ||
                reference == quote(children[i].label()) ||
                reference == "'" + children[i].label() + "'";
      } else if (children[i].kind() == PowlNodeKind::kSilent) {
        match = match || reference == "tau";
      }
      if (match) matches.push_back(i);
    }
    if (matches.empty()) fail("relation references an unknown node");
    if (matches.size() != 1) fail("relation reference is ambiguous");
    return matches.front();
  }
  PowlNode partial_order(bool has_equals) {
    if (has_equals) expect('=');
    expect('(');
    skip();
    if (!operator_token("nodes", '=')) fail("expected nodes");
    expect('=');
    expect('{');
    std::vector<PowlNode> children;
    std::vector<std::string> parsed_representations;
    skip();
    if (!consume('}')) {
      for (;;) {
        const auto begin = pos_;
        children.push_back(node());
        parsed_representations.push_back(
            trim(text_.substr(begin, pos_ - begin)));
        skip();
        if (consume('}')) break;
        if (!consume(',') && !consume(';')) fail("expected node separator");
      }
    }
    expect(',');
    if (!operator_token("order", '=')) fail("expected order");
    expect('=');
    expect('{');
    std::vector<std::pair<std::size_t, std::size_t>> order; skip();
    if (!consume('}')) {
      for (;;) {
        const auto item = relation_item();
        const auto arrow = arrow_position(item);
        if (arrow == std::string::npos)
          fail("expected '-->' in relation");
        const auto source = resolve_reference(
            item.substr(0, arrow), parsed_representations, children);
        const auto target = resolve_reference(
            item.substr(arrow + 3), parsed_representations, children);
        order.emplace_back(source, target);
        skip();
        if (consume('}')) break;
        if (!consume(',') && !consume(';')) fail("expected order separator");
      }
    }
    expect(')');
    return PowlNode::strict_partial_order(std::move(children), order);
  }
};

}  // namespace

PowlNode::PowlNode(PowlNodeKind kind, std::string label, PowlFrequency frequency,
                   std::vector<PowlNode> children,
                   std::vector<std::pair<std::size_t, std::size_t>> order)
    : kind_(kind), label_(std::move(label)), frequency_(frequency),
      children_(std::move(children)), order_(std::move(order)) {}

PowlNode PowlNode::transition(std::string label, PowlFrequency frequency) {
  validate_frequency(frequency);
  return PowlNode(PowlNodeKind::kTransition, std::move(label), frequency, {}, {});
}

PowlNode PowlNode::silent() { return PowlNode(PowlNodeKind::kSilent, {}, {}, {}, {}); }

PowlNode PowlNode::xor_node(std::vector<PowlNode> children) {
  if (children.size() < 2) throw std::invalid_argument("POWL XOR requires at least two children");
  return PowlNode(PowlNodeKind::kXor, {}, {}, std::move(children), {});
}

PowlNode PowlNode::loop(PowlNode body, PowlNode redo) {
  std::vector<PowlNode> children; children.push_back(std::move(body)); children.push_back(std::move(redo));
  return PowlNode(PowlNodeKind::kLoop, {}, {}, std::move(children), {});
}

PowlNode PowlNode::strict_partial_order(
    std::vector<PowlNode> children,
    const std::vector<std::pair<std::size_t, std::size_t>>& order) {
  const std::size_t n = children.size();
  std::vector<std::vector<bool>> reach(n, std::vector<bool>(n, false));
  for (const auto [a, b] : order) {
    if (a >= n || b >= n) throw std::invalid_argument("POWL order index out of range");
    if (a == b) throw std::invalid_argument("POWL order is not irreflexive");
    reach[a][b] = true;
  }
  for (std::size_t k = 0; k < n; ++k)
    for (std::size_t i = 0; i < n; ++i)
      if (reach[i][k]) for (std::size_t j = 0; j < n; ++j) reach[i][j] = reach[i][j] || reach[k][j];
  for (std::size_t i = 0; i < n; ++i) if (reach[i][i]) throw std::invalid_argument("POWL order contains a cycle");
  std::vector<std::pair<std::size_t, std::size_t>> closure;
  for (std::size_t i = 0; i < n; ++i) for (std::size_t j = 0; j < n; ++j) if (reach[i][j]) closure.emplace_back(i, j);
  return PowlNode(PowlNodeKind::kStrictPartialOrder, {}, {}, std::move(children), std::move(closure));
}

PowlNode PowlNode::sequence(std::vector<PowlNode> children) {
  std::vector<std::pair<std::size_t, std::size_t>> order;
  for (std::size_t i = 0; i < children.size(); ++i) for (std::size_t j = i + 1; j < children.size(); ++j) order.emplace_back(i, j);
  return strict_partial_order(std::move(children), order);
}

PowlNode PowlNode::concurrency(std::vector<PowlNode> children) {
  return strict_partial_order(std::move(children), {});
}

bool PowlNode::has_edge(std::size_t source, std::size_t target) const {
  if (kind_ != PowlNodeKind::kStrictPartialOrder)
    throw std::invalid_argument("POWL edge queries require a strict partial order");
  if (source >= children_.size() || target >= children_.size())
    throw std::invalid_argument("POWL edge query index is out of range");
  return std::binary_search(order_.begin(), order_.end(), std::pair{source, target});
}

std::vector<std::size_t> PowlNode::start_nodes() const {
  if (kind_ != PowlNodeKind::kStrictPartialOrder)
    throw std::invalid_argument("POWL start-node queries require a strict partial order");
  std::vector<std::size_t> result;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    bool incoming = false; for (const auto [a, b] : order_) if (b == i) { incoming = true; break; }
    if (!incoming) result.push_back(i);
  }
  return result;
}

std::vector<std::size_t> PowlNode::end_nodes() const {
  if (kind_ != PowlNodeKind::kStrictPartialOrder)
    throw std::invalid_argument("POWL end-node queries require a strict partial order");
  std::vector<std::size_t> result;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    bool outgoing = false; for (const auto [a, b] : order_) if (a == i) { outgoing = true; break; }
    if (!outgoing) result.push_back(i);
  }
  return result;
}

std::vector<std::pair<std::size_t, std::size_t>> PowlNode::transitive_reduction() const {
  if (kind_ != PowlNodeKind::kStrictPartialOrder)
    throw std::invalid_argument(
        "POWL transitive reduction requires a strict partial order");
  std::vector<std::pair<std::size_t, std::size_t>> result;
  for (const auto [a, b] : order_) {
    bool redundant = false;
    for (std::size_t k = 0; k < children_.size() && !redundant; ++k)
      if (k != a && k != b && has_edge(a, k) && has_edge(k, b)) redundant = true;
    if (!redundant) result.emplace_back(a, b);
  }
  return result;
}

std::string PowlNode::serialize() const {
  switch (kind_) {
    case PowlNodeKind::kTransition: {
      std::string out = "T(" + quote(label_);
      if (frequency_ != PowlFrequency{}) {
        out += ',' + std::to_string(frequency_.minimum) + ',';
        out += frequency_.maximum ? std::to_string(*frequency_.maximum) : "*";
      }
      return out + ')';
    }
    case PowlNodeKind::kSilent: return "tau";
    case PowlNodeKind::kXor: {
      std::string out = "X("; for (std::size_t i = 0; i < children_.size(); ++i) { if (i) out += ','; out += children_[i].serialize(); } return out + ')';
    }
    case PowlNodeKind::kLoop: return "*(" + children_[0].serialize() + ',' + children_[1].serialize() + ')';
    case PowlNodeKind::kStrictPartialOrder: {
      std::string out = "PO=(nodes={";
      for (std::size_t i = 0; i < children_.size(); ++i) { if (i) out += ','; out += children_[i].serialize(); }
      out += "},order={";
      const auto reference = [&](std::size_t index) {
        std::string candidate;
        const auto& child = children_[index];
        if (child.kind() == PowlNodeKind::kTransition &&
            !child.label().empty() &&
            child.label().find_first_not_of(" \t\r\n") == 0 &&
            child.label().find_last_not_of(" \t\r\n") ==
                child.label().size() - 1 &&
            child.label().find("-->") == std::string::npos &&
            child.label().find_first_of(",;{}()") == std::string::npos) {
          candidate = child.label();
        } else if (child.kind() == PowlNodeKind::kSilent) {
          candidate = "tau";
        } else {
          candidate = child.serialize();
        }
        std::size_t occurrences = 0;
        for (const auto& other : children_) {
          std::string other_candidate;
          if (other.kind() == PowlNodeKind::kTransition &&
              !other.label().empty() &&
              other.label().find_first_not_of(" \t\r\n") == 0 &&
              other.label().find_last_not_of(" \t\r\n") ==
                  other.label().size() - 1 &&
              other.label().find("-->") == std::string::npos &&
              other.label().find_first_of(",;{}()") == std::string::npos) {
            other_candidate = other.label();
          } else if (other.kind() == PowlNodeKind::kSilent) {
            other_candidate = "tau";
          } else {
            other_candidate = other.serialize();
          }
          if (other_candidate == candidate) ++occurrences;
        }
        return occurrences == 1 ? candidate : "@" + std::to_string(index);
      };
      for (std::size_t i = 0; i < order_.size(); ++i) {
        if (i) out += ',';
        out += reference(order_[i].first) + "-->" +
               reference(order_[i].second);
      }
      return out + "})";
    }
  }
  return {};
}

PowlNode PowlNode::parse(const std::string& text) { return Parser(text).run(); }

bool operator==(const PowlNode& lhs, const PowlNode& rhs) noexcept {
  return lhs.kind_ == rhs.kind_ && lhs.label_ == rhs.label_ && lhs.frequency_ == rhs.frequency_ &&
         lhs.children_ == rhs.children_ && lhs.order_ == rhs.order_;
}

}  // namespace minera
