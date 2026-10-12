#include "minera/precomputed_replay_json.hpp"
#include "minera/json_export.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {

[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("replay result JSON: " + message);
}

struct Number { std::string text; };
struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> value;
  bool null() const { return std::holds_alternative<std::nullptr_t>(value); }
  const Object& object() const {
    if (const auto* result = std::get_if<Object>(&value)) return *result;
    invalid("expected an object");
  }
  const Array& array() const {
    if (const auto* result = std::get_if<Array>(&value)) return *result;
    invalid("expected an array");
  }
  const std::string& string() const {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    invalid("expected a string");
  }
  bool boolean() const {
    if (const auto* result = std::get_if<bool>(&value)) return *result;
    invalid("expected a boolean");
  }
};

void append_utf8(std::string& output, std::uint32_t value) {
  if (value <= 0x7f) output += static_cast<char>(value);
  else if (value <= 0x7ff) {
    output += static_cast<char>(0xc0 | (value >> 6));
    output += static_cast<char>(0x80 | (value & 63));
  } else if (value <= 0xffff) {
    output += static_cast<char>(0xe0 | (value >> 12));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  } else {
    output += static_cast<char>(0xf0 | (value >> 18));
    output += static_cast<char>(0x80 | ((value >> 12) & 63));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  }
}

// This parser retains numeric tokens rather than rounding JSON integers to
// double. Its grammar, UTF-8, duplicate keys, size and recursion are checked.
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {
    if (text.size() > 32u * 1024u * 1024u) invalid("request exceeds 32 MiB");
  }
  Json parse() {
    auto result = value(0);
    whitespace();
    if (position_ != text_.size()) invalid("trailing data");
    return result;
  }
 private:
  bool digit() const { return position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9'; }
  void whitespace() {
    while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' || text_[position_] == '\n' || text_[position_] == '\r')) ++position_;
  }
  bool take(char c) {
    whitespace();
    if (position_ < text_.size() && text_[position_] == c) { ++position_; return true; }
    return false;
  }
  void require(char c) { if (!take(c)) invalid("unexpected character"); }
  std::uint32_t hex4() {
    std::uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
      if (position_ == text_.size()) invalid("short Unicode escape");
      const char c = text_[position_++];
      const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (d < 0) invalid("invalid Unicode escape");
      result = (result << 4) | static_cast<unsigned>(d);
    }
    return result;
  }
  std::string string() {
    require('"');
    std::string result;
    while (position_ < text_.size()) {
      const auto c = static_cast<unsigned char>(text_[position_++]);
      if (c == '"') return result;
      if (c < 0x20) invalid("unescaped control character");
      if (c >= 0x80) {
        const int count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
        if (count < 0) invalid("invalid UTF-8");
        std::uint32_t code = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
        for (int i = 0; i < count; ++i) {
          if (position_ == text_.size()) invalid("short UTF-8 sequence");
          const auto next = static_cast<unsigned char>(text_[position_++]);
          if ((next & 0xc0) != 0x80) invalid("invalid UTF-8 continuation");
          code = (code << 6) | (next & 63);
        }
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) invalid("invalid UTF-8 scalar");
        append_utf8(result, code);
        continue;
      }
      if (c != '\\') { result += static_cast<char>(c); continue; }
      if (position_ == text_.size()) invalid("unterminated escape");
      switch (text_[position_++]) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'u': {
          auto code = hex4();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (position_ + 2 > text_.size() || text_[position_] != '\\' || text_[position_ + 1] != 'u') invalid("unpaired Unicode surrogate");
            position_ += 2;
            const auto low = hex4();
            if (low < 0xdc00 || low > 0xdfff) invalid("unpaired Unicode surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
          } else if (code >= 0xdc00 && code <= 0xdfff) invalid("unpaired Unicode surrogate");
          append_utf8(result, code);
          break;
        }
        default: invalid("unknown escape");
      }
    }
    invalid("unterminated string");
  }
  Json value(unsigned depth) {
    if (depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000) invalid("request exceeds one million values");
    whitespace();
    if (position_ == text_.size()) invalid("unexpected end of input");
    const auto c = text_[position_];
    if (c == '"') return Json{string()};
    if (take('{')) {
      Json::Object result;
      if (!take('}')) {
        do {
          auto key = string();
          require(':');
          if (!result.emplace(std::move(key), value(depth + 1)).second) invalid("duplicate object key");
        } while (take(','));
        require('}');
      }
      return Json{std::move(result)};
    }
    if (take('[')) {
      Json::Array result;
      if (!take(']')) {
        do { result.push_back(value(depth + 1)); } while (take(','));
        require(']');
      }
      return Json{std::move(result)};
    }
    for (const auto token : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
      if (text_.substr(position_, token.size()) == token) {
        position_ += token.size();
        if (token == "null") return Json{nullptr};
        return Json{token == "true"};
      }
    }
    const auto start = position_;
    if (text_[position_] == '-') ++position_;
    if (!digit()) invalid("expected a value");
    if (text_[position_] == '0') ++position_;
    else while (digit()) ++position_;
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      if (!digit()) invalid("missing fractional digits");
      while (digit()) ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
      if (!digit()) invalid("missing exponent digits");
      while (digit()) ++position_;
    }
    return Json{Number{std::string(text_.substr(start, position_ - start))}};
  }
  std::string_view text_;
  std::size_t position_ = 0, nodes_ = 0;
};

const Json* optional(const Json::Object& object, std::string_view key) {
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &found->second;
}
const Json& required(const Json::Object& object, std::string_view key) {
  if (const auto* value = optional(object, key)) return *value;
  invalid("missing field " + std::string(key));
}
void fields(const Json::Object& object, std::initializer_list<std::string_view> allowed) {
  for (const auto& [key, value] : object) {
    (void)value;
    if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) invalid("unknown field " + key);
  }
}
std::string text(const Json::Object& object, std::string_view key, std::string fallback = {}) {
  const auto* found = optional(object, key);
  return found ? found->string() : std::move(fallback);
}
bool flag(const Json::Object& object, std::string_view key) {
  const auto* found = optional(object, key);
  return found && found->boolean();
}

template <class T> T number(std::string_view value) {
  T result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) invalid("invalid or out-of-range numeric cell");
  return result;
}
std::string number_text(const Json& value) {
  if (const auto* token = std::get_if<Number>(&value.value)) return token->text;
  return value.string();
}


bool flag_or(const Json::Object& object, std::string_view key, bool fallback) {
  const auto* found = optional(object, key);
  return found ? found->boolean() : fallback;
}
double real(const Json::Object& object, std::string_view key, double fallback=0) {
  const auto* found = optional(object, key);
  if (!found) return fallback;
  const auto value = number_text(*found);
  if (value == "nan" || value == "NaN") return std::numeric_limits<double>::quiet_NaN();
  if (value == "inf" || value == "Infinity" || value == "+inf") return std::numeric_limits<double>::infinity();
  if (value == "-inf" || value == "-Infinity") return -std::numeric_limits<double>::infinity();
  return number<double>(value);
}
TokenInteger integer(const Json::Object& object, std::string_view key) {
  const auto* found = optional(object, key);
  return found ? TokenInteger(number_text(*found)) : TokenInteger{};
}
std::optional<TokenInteger> optional_integer(const Json::Object& object, std::string_view key) {
  const auto* found = optional(object, key);
  return found && !found->null() ? std::optional<TokenInteger>(TokenInteger(number_text(*found))) : std::nullopt;
}
std::string error_text(const Json::Object& object) {
  const auto* found = optional(object, "error");
  return found && !found->null() ? found->string() : std::string{};
}
const Json::Array& token_rows(const Json& input) {
  if (std::holds_alternative<Json::Array>(input.value)) return input.array();
  const auto& object = input.object();
  if (const auto* traces = optional(object, "trace_diagnostics")) return traces->array();
  if (const auto* traces = optional(object, "traces")) return traces->array();
  if (const auto* replay = optional(object, "token_replay")) return token_rows(*replay);
  if (const auto* fitness = optional(object, "replay_fitness")) return token_rows(*fitness);
  invalid("expected token rows or an object containing traces/trace_diagnostics");
}
std::vector<PrecomputedTokenReplay> token_records(const Json& input) {
  std::vector<PrecomputedTokenReplay> result;
  for (const auto& item : token_rows(input)) {
    const auto& object = item.object();
    PrecomputedTokenReplay row;
    row.computed = flag_or(object, "computed", true);
    row.exhausted = flag_or(object, "exhausted", false);
    row.trace_is_fit = flag_or(object, "trace_is_fit", false);
    row.trace_fitness = real(object, "trace_fitness");
    row.missing_tokens = optional_integer(object, "missing_tokens");
    row.consumed_tokens = optional_integer(object, "consumed_tokens");
    row.remaining_tokens = optional_integer(object, "remaining_tokens");
    row.produced_tokens = optional_integer(object, "produced_tokens");
    row.missing = integer(object, "missing"); row.consumed = integer(object, "consumed");
    row.remaining = integer(object, "remaining"); row.produced = integer(object, "produced");
    row.error = error_text(object);
    if (const auto* frequencies = optional(object, "transition_activations"))
      for (const auto& value : frequencies->array())
        row.transition_activations.push_back(number<std::uint64_t>(number_text(value)));
    result.push_back(std::move(row));
  }
  return result;
}
std::vector<PrecomputedAlignmentReplay> alignment_records(const Json& input) {
  std::vector<PrecomputedAlignmentReplay> result;
  for (const auto& item : input.array()) {
    PrecomputedAlignmentReplay row;
    if (item.null()) { row.computed = false; result.push_back(std::move(row)); continue; }
    const auto& object = item.object();
    row.computed = flag_or(object, "computed", true);
    row.exhausted = flag_or(object, "exhausted", false);
    row.trace_is_fit = flag_or(object, "trace_is_fit", false);
    row.fitness = real(object, "fitness"); row.cost = real(object, "cost");
    row.best_worst_cost = real(object, "best_worst_cost"); row.bwc = real(object, "bwc");
    row.error = error_text(object);
    result.push_back(std::move(row));
  }
  return result;
}
}  // namespace

std::vector<PrecomputedTokenReplay> parse_precomputed_token_replay_json(std::string_view json) {
  return token_records(Parser(json).parse());
}
std::vector<PrecomputedAlignmentReplay> parse_precomputed_alignment_replay_json(std::string_view json) {
  return alignment_records(Parser(json).parse());
}
std::string precomputed_replay_fitness_json(std::string_view json, std::string_view input_kind,
                                          std::optional<ReplayFitnessVariant> variant) {
  const auto parsed = Parser(json).parse();
  ModelEvaluation evaluation; evaluation.miner = "precomputed";
  std::size_t submitted = 0;
  if (input_kind == "token") {
    const auto rows = token_records(parsed); submitted = rows.size();
    evaluation.replay_fitness = evaluate_replay_fitness(rows, variant.value_or(ReplayFitnessVariant::TOKEN_BASED));
  } else if (input_kind == "alignment") {
    const auto rows = alignment_records(parsed); submitted = rows.size();
    evaluation.replay_fitness = evaluate_replay_fitness(rows, variant.value_or(ReplayFitnessVariant::ALIGNMENT_BASED));
  } else invalid("input_kind must be token or alignment");
  const auto& result = *evaluation.replay_fitness;
  std::ostringstream out;
  out << "{\"family\":\"REPLAY_RESULT_FITNESS\",\"input_kind\":\"" << input_kind
      << "\",\"submitted_rows\":" << submitted << ",\"input\":" << json
      << ",\"result\":" << model_evaluation_json(evaluation);
  if (input_kind == "token") {
    out << ",\"token_totals\":{\"missing\":" << result.token_result.missing
        << ",\"consumed\":" << result.token_result.consumed
        << ",\"remaining\":" << result.token_result.remaining
        << ",\"produced\":" << result.token_result.produced << "},\"transition_frequencies\":[";
    for (std::size_t i=0;i<result.token_result.transition_frequencies.size();++i) {
      if (i) out << ','; out << result.token_result.transition_frequencies[i];
    }
    out << ']';
  }
  out << '}';
  return out.str();
}
}  // namespace minera
