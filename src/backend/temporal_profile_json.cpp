#include "minera/temporal_profile_json.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {
int json_hex_digit(char value);
void append_utf8(std::string& output, std::uint32_t codepoint);

class ProfileParser {
 public:
  explicit ProfileParser(std::string_view input) : input_(input) {}
  TemporalProfile parse() {
    TemporalProfile profile;
    expect('[');
    if (!consume(']')) {
      do {
        expect('[');
        auto source = parse_string();
        expect(',');
        auto target = parse_string();
        expect(',');
        const auto mean = number();
        expect(',');
        const auto deviation = number();
        expect(']');
        profile.entries.push_back({std::move(source), std::move(target), 1,
                                   mean, deviation});
      } while (consume(','));
      expect(']');
    }
    skip();
    if (pos_ != input_.size()) fail("trailing JSON data");
    return profile;
  }
 private:
  [[noreturn]] void fail(const char* message) const {
    throw std::invalid_argument(std::string("invalid temporal profile JSON: ") + message);
  }
  void skip() {
    while (pos_ < input_.size() &&
           (input_[pos_] == ' ' || input_[pos_] == '\n' ||
            input_[pos_] == '\r' || input_[pos_] == '\t')) ++pos_;
  }
  void expect(char ch) {
    skip();
    if (pos_ >= input_.size() || input_[pos_] != ch) fail("unexpected character");
    ++pos_;
  }
  std::string parse_string() {
    expect('"');
    std::string result;
    while (pos_ < input_.size()) {
      const unsigned char ch = static_cast<unsigned char>(input_[pos_++]);
      if (ch == '"') return result;
      if (ch < 0x20) fail("control character in string");
      if (ch != '\\') { result.push_back(static_cast<char>(ch)); continue; }
      if (pos_ >= input_.size()) fail("unterminated escape");
      const char escaped = input_[pos_++];
      switch (escaped) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
          if (pos_ + 4 > input_.size()) fail("short unicode escape");
          std::uint32_t codepoint = 0;
          for (int i = 0; i < 4; ++i) {
            const int digit = json_hex_digit(input_[pos_++]);
            if (digit < 0) fail("invalid unicode escape");
            codepoint = (codepoint << 4) | static_cast<std::uint32_t>(digit);
          }
          if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (pos_ + 6 > input_.size() || input_[pos_] != '\\' ||
                input_[pos_ + 1] != 'u')
              fail("unpaired unicode surrogate");
            pos_ += 2;
            std::uint32_t low = 0;
            for (int i = 0; i < 4; ++i) {
              const int digit = json_hex_digit(input_[pos_++]);
              if (digit < 0) fail("invalid unicode escape");
              low = (low << 4) | static_cast<std::uint32_t>(digit);
            }
            if (low < 0xdc00 || low > 0xdfff)
              fail("invalid unicode surrogate pair");
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
          } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
            fail("unpaired unicode surrogate");
          }
          append_utf8(result, codepoint);
          break;
        }
        default: fail("unsupported string escape");
      }
    }
    fail("unterminated string");
  }
  bool consume(char character) {
    skip();
    if (pos_ == input_.size() || input_[pos_] != character) return false;
    ++pos_;
    return true;
  }
  double number() {
    skip();
    const auto start = pos_;
    if (pos_ < input_.size() && input_[pos_] == '-') ++pos_;
    if (pos_ == input_.size()) fail("missing number");
    const auto digit = [&](std::size_t index) {
      return index < input_.size() && input_[index] >= '0' && input_[index] <= '9';
    };
    if (input_[pos_] == '0') ++pos_;
    else {
      if (input_[pos_] < '1' || input_[pos_] > '9') fail("expected number");
      while (digit(pos_)) ++pos_;
    }
    if (pos_ < input_.size() && input_[pos_] == '.') {
      ++pos_;
      if (!digit(pos_)) fail("missing fractional digits");
      while (digit(pos_)) ++pos_;
    }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < input_.size() && (input_[pos_] == '-' || input_[pos_] == '+')) ++pos_;
      if (!digit(pos_)) fail("missing exponent digits");
      while (digit(pos_)) ++pos_;
    }
    double value = 0.0;
    const auto parsed = std::from_chars(input_.data()+start, input_.data()+pos_, value);
    if (parsed.ec != std::errc{} || parsed.ptr != input_.data()+pos_ ||
        !std::isfinite(value) || value < 0.0)
      fail("profile moments must be finite and non-negative");
    return value;
  }
  std::string_view input_;
  std::size_t pos_ = 0;
};

void json_string(std::ostringstream& out, std::string_view value) {
  out << '"';
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          constexpr char digits[] = "0123456789abcdef";
          out << "\\u00" << digits[ch >> 4] << digits[ch & 0xf];
        } else out << static_cast<char>(ch);
    }
  }
  out << '"';
}

int json_hex_digit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
  if (codepoint <= 0x7f) {
    output.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ff) {
    output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint <= 0xffff) {
    output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
}

}  // namespace

TemporalProfile parse_temporal_profile_json(std::string_view json) {
  return ProfileParser(json).parse();
}

std::string temporal_profile_json(const TemporalProfile& profile) {
  std::ostringstream out;
  out.precision(std::numeric_limits<double>::max_digits10);
  out << '[';
  for (std::size_t index = 0; index < profile.entries.size(); ++index) {
    const auto& entry = profile.entries[index];
    if (!std::isfinite(entry.mean_seconds) || entry.mean_seconds < 0.0 ||
        !std::isfinite(entry.standard_deviation_seconds) ||
        entry.standard_deviation_seconds < 0.0)
      throw std::invalid_argument("temporal profile moments must be finite and non-negative");
    if (index) out << ',';
    out << '[';
    json_string(out, entry.source);
    out << ',';
    json_string(out, entry.target);
    out << ',' << entry.mean_seconds << ',' << entry.standard_deviation_seconds << ']';
  }
  out << ']';
  return out.str();
}
}  // namespace minera
