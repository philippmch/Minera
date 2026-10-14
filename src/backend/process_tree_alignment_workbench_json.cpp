#include "minera/process_tree_alignment_workbench_json.hpp"
#include "minera/inductive_input_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("process-tree alignment input JSON: " + message);
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
  explicit Parser(std::string_view text, bool escaped_lone_surrogates = true,
                  bool bounded_transport = true)
      : text_(text), escaped_lone_surrogates_(escaped_lone_surrogates),
        bounded_transport_(bounded_transport) {
    if (bounded_transport_ && text.size() > 32u * 1024u * 1024u)
      invalid("request exceeds 32 MiB");
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
            if (position_ + 2 <= text_.size() && text_[position_] == '\\' && text_[position_ + 1] == 'u') {
              const auto saved = position_;
              position_ += 2;
              const auto low = hex4();
              if (low >= 0xdc00 && low <= 0xdfff)
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
              else {
                if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
                position_ = saved;
              }
            } else if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
          } else if (code >= 0xdc00 && code <= 0xdfff && !escaped_lone_surrogates_)
            invalid("unpaired Unicode surrogate");
          append_utf8(result, code);
          break;
        }
        default: invalid("unknown escape");
      }
    }
    invalid("unterminated string");
  }
  Json value(unsigned depth) {
    if (bounded_transport_ && depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000 && bounded_transport_) invalid("request exceeds one million values");
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
  bool escaped_lone_surrogates_ = false;
  bool bounded_transport_ = true;
  std::size_t position_ = 0, nodes_ = 0;
};

void fields(const Json::Object& object, std::initializer_list<std::string_view> expected) {
  if (object.size() != expected.size()) invalid("missing or unknown object fields");
  for (const auto name : expected) if (!object.contains(name)) invalid("missing required field " + std::string(name));
}

std::uint64_t unsigned_integer(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      !std::all_of(number->text.begin(), number->text.end(), [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected an unsigned JSON integer token");
  std::uint64_t result = 0;
  const auto parsed = std::from_chars(number->text.data(), number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("unsigned integer exceeds uint64 range");
  return result;
}

std::size_t native_index(const Json& value) {
  const auto result = unsigned_integer(value);
  if (result > std::numeric_limits<std::size_t>::max()) invalid("index exceeds native size_t range");
  return static_cast<std::size_t>(result);
}

std::vector<std::string> words(const Json& value) {
  std::vector<std::string> result;
  for (const auto& word : value.array()) result.push_back(word.string());
  return result;
}

template <class Enum>
Enum native_enum(const Json& value) {
  using Integer = std::underlying_type_t<Enum>;
  static_assert(std::is_same_v<Integer, int> && std::is_signed_v<Integer>);
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty()) invalid("expected a signed native enum integer token");
  const auto begin = number->text.begin() + (number->text.front() == '-' ? 1 : 0);
  if (begin == number->text.end() ||
      !std::all_of(begin, number->text.end(), [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected a signed native enum integer token");
  Integer result = 0;
  const auto parsed = std::from_chars(number->text.data(), number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("enum index exceeds native signed int range");
  return static_cast<Enum>(result);
}

const char* operator_name(ProcessTreeOperator op) {
  switch (op) {
    case ProcessTreeOperator::kActivity: return "ACTIVITY";
    case ProcessTreeOperator::kTau: return "TAU";
    case ProcessTreeOperator::kSequence: return "SEQUENCE";
    case ProcessTreeOperator::kExclusive: return "EXCLUSIVE";
    case ProcessTreeOperator::kInclusiveChoice: return "INCLUSIVE_CHOICE";
    case ProcessTreeOperator::kParallel: return "PARALLEL";
    case ProcessTreeOperator::kInterleaving: return "INTERLEAVING";
    case ProcessTreeOperator::kLoop: return "LOOP";
  }
  return "UNKNOWN";
}

ProcessTreeNode node(const Json& value) {
  const auto& object = value.object(); fields(object, {"op", "op_index", "activity", "children"});
  ProcessTreeNode result;
  result.op = native_enum<ProcessTreeOperator>(object.at("op_index"));
  if (object.at("op").string() != operator_name(result.op))
    invalid("operator name does not match its native enum index");
  result.activity = native_index(object.at("activity"));
  for (const auto& child : object.at("children").array()) result.children.push_back(node(child));
  return result;
}

void quote(std::ostream& out, std::string_view value) {
  // The frozen writer has no transport bounds and escapes internal WTF-8.
  const auto encoded = serialize_alignment_trace_words_json({std::string(value)});
  out.write(encoded.data() + 1, static_cast<std::streamsize>(encoded.size() - 2));
}

void floating(std::ostream& out, double value) {
  if (std::isnan(value)) { out << "\"NaN\""; return; }
  if (std::isinf(value)) { out << (value < 0 ? "\"-INF\"" : "\"INF\""); return; }
  std::ostringstream number; number.imbue(std::locale::classic());
  number << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  auto text = number.str();
  if (text.find_first_of(".eE") == std::string::npos) text += ".0";
  out << text;
}

const char* boolean(bool value) { return value ? "true" : "false"; }

const char* variant_name(ProcessTreeAlignmentVariant variant) {
  switch (variant) {
    case ProcessTreeAlignmentVariant::kSearchGraphPt: return "SEARCH_GRAPH_PT";
    case ProcessTreeAlignmentVariant::kDynamicProgramming: return "DYNAMIC_PROGRAMMING";
    case ProcessTreeAlignmentVariant::kMilp: return "MILP";
    case ProcessTreeAlignmentVariant::kApproximatedOriginal: return "APPROXIMATED_ORIGINAL";
  }
  return "UNKNOWN";
}

const char* move_name(ProcessTreeAlignmentMoveType type) {
  switch (type) {
    case ProcessTreeAlignmentMoveType::kSynchronous: return "sync";
    case ProcessTreeAlignmentMoveType::kLog: return "log";
    case ProcessTreeAlignmentMoveType::kModel: return "model";
  }
  return "UNKNOWN";
}

template <class Write>
std::string json(Write write) {
  std::ostringstream out; out.imbue(std::locale::classic()); write(out); return out.str();
}

void variant_fields(std::ostream& out, ProcessTreeAlignmentVariant variant) {
  out << "\"variant\":"; quote(out, variant_name(variant));
  out << ",\"variant_index\":" << static_cast<std::underlying_type_t<ProcessTreeAlignmentVariant>>(variant);
}

void trace_json(std::ostream& out, const ProcessTreeTraceAlignment& trace) {
  out << "{\"trace\":" << serialize_alignment_trace_words_json(trace.trace) << ",\"moves\":[";
  for (std::size_t i = 0; i < trace.moves.size(); ++i) {
    if (i) out << ',';
    const auto& move = trace.moves[i];
    out << "{\"type\":"; quote(out, move_name(move.type));
    out << ",\"type_index\":" << static_cast<std::underlying_type_t<ProcessTreeAlignmentMoveType>>(move.type)
        << ",\"log_activity\":"; quote(out, move.log_activity);
    out << ",\"model_activity\":"; quote(out, move.model_activity);
    out << ",\"activity\":"; quote(out, move.activity); out << '}';
  }
  out << "],\"cost\":" << trace.cost << ",\"fitness\":"; floating(out, trace.fitness);
  out << ",\"completed\":" << boolean(trace.completed) << ",\"timed_out\":" << boolean(trace.timed_out) << '}';
}
}  // namespace

ProcessTree parse_process_tree_alignment_input_json(std::string_view input) {
  const auto document = Parser(input).parse();
  const auto& object = document.object(); fields(object, {"activities", "root"});
  return ProcessTree(words(object.at("activities")), node(object.at("root")));
}

std::string serialize_process_tree_alignment_input_json(const ProcessTree& tree) {
  return serialize_inductive_process_tree_json(tree);
}

std::string serialize_process_tree_alignment_result_json(const ProcessTreeAlignmentResult& result) {
  return json([&](std::ostream& out) {
    out << "{\"computed\":" << boolean(result.computed) << ",\"completed\":" << boolean(result.completed)
        << ",\"timed_out\":" << boolean(result.timed_out) << ",\"exhausted\":" << boolean(result.exhausted) << ',';
    variant_fields(out, result.variant);
    out << ",\"total_cost\":" << result.total_cost << ",\"fitness\":"; floating(out, result.fitness);
    const auto& diagnostics = result.diagnostics;
    out << ",\"diagnostics\":{\"visited_states\":" << diagnostics.visited_states
        << ",\"queued_states\":" << diagnostics.queued_states
        << ",\"traversed_arcs\":" << diagnostics.traversed_arcs
        << ",\"lp_solved\":" << diagnostics.lp_solved
        << ",\"traces_aligned\":" << diagnostics.traces_aligned << ",\"message\":";
    quote(out, diagnostics.message); out << "},\"traces\":[";
    for (std::size_t i = 0; i < result.traces.size(); ++i) {
      if (i) out << ',';
      trace_json(out, result.traces[i]);
    }
    out << "]}";
  });
}

std::string serialize_process_tree_alignment_options_json(const ProcessTreeAlignmentOptions& options) {
  return json([&](std::ostream& out) {
    out << '{'; variant_fields(out, options.variant);
    out << ",\"max_states\":" << options.max_states << ",\"max_time_seconds\":"; floating(out, options.max_time_seconds);
    out << ",\"max_total_time_seconds\":"; floating(out, options.max_total_time_seconds);
    out << ",\"max_trace_length\":" << options.max_trace_length
        << ",\"max_process_tree_height\":" << options.max_process_tree_height
        << ",\"max_trace_time_seconds\":"; floating(out, options.max_trace_time_seconds);
    out << ",\"max_per_trace_time_seconds\":"; floating(out, options.max_per_trace_time_seconds);
    out << ",\"use_global_cache\":" << boolean(options.use_global_cache)
        << ",\"batch_size\":" << options.batch_size << ",\"clear_cache\":" << boolean(options.clear_cache)
        << ",\"activity_attribute\":"; quote(out, options.activity_attribute); out << '}';
  });
}

}  // namespace minera

