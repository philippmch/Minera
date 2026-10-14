#include "minera/petri_anti_multi_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"

#include <cstdlib>
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
  throw std::invalid_argument("petri anti/multi workbench JSON: " + message);
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

std::size_t unsigned_field(std::string_view text, std::string_view name) {
  std::size_t result = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    invalid(std::string(name) + ": expected an exact nonnegative size_t integer");
  return result;
}

double finite_field(const std::string& text, std::string_view name) {
  char* end = nullptr;
  const auto value = std::strtod(text.c_str(), &end);
  // A finite subnormal (or rounded signed zero) remains native double storage
  // even when strtod reports underflow. Overflow/nonfinite and partial syntax
  // are rejected; native validation owns exponent/epsilon semantic domains.
  if (end == text.c_str() || end != text.c_str() + text.size() || !std::isfinite(value))
    invalid(std::string(name) + ": expected a finite strtod numeric value");
  return value;
}

std::vector<std::size_t> json_marking(std::string_view text) {
  const auto document = Parser(text).parse();
  std::vector<std::size_t> result;
  for (const auto& item : document.array()) result.push_back(native_index(item));
  return result;
}

std::vector<std::size_t> comma_marking(std::string_view text, std::string_view name) {
  if (text.empty()) invalid(std::string(name) + ": expected a non-empty legacy marking");
  std::vector<std::size_t> result;
  std::size_t begin = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i != text.size() && text[i] != ',') continue;
    auto token = text.substr(begin, i - begin);
    const auto first = token.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) invalid(std::string(name) + ": expected a marking entry");
    const auto last = token.find_last_not_of(" \t\r\n");
    if (result.size() >= 999999) invalid("legacy marking exceeds one million values");
    result.push_back(unsigned_field(token.substr(first, last - first + 1), name));
    begin = i + 1;
  }
  return result;
}

class Fields {
 public:
  Fields(const FieldLookup& field, const FieldPresence& present) : field_(field), present_(present) {}
  bool supplied(std::string_view name) const { return present_ ? present_(name) : !field_(name).empty(); }
  std::string text(std::string_view name) const {
    auto value = field_(name);
    if (value.size() > 32u * 1024u * 1024u) invalid(std::string(name) + ": request exceeds 32 MiB");
    return value;
  }
  std::vector<std::size_t> marking(std::string_view legacy, std::string_view json_name) const {
    if (supplied(json_name)) {
      if (supplied(legacy)) invalid(std::string(json_name) + ": cannot coexist with the legacy marking carrier");
      return json_marking(text(json_name));
    }
    return supplied(legacy) ? comma_marking(text(legacy), legacy) : std::vector<std::size_t>{};
  }
 private:
  const FieldLookup& field_;
  const FieldPresence& present_;
};

template <class Options>
Options options_from_fields(const FieldLookup& field, const FieldPresence& present) {
  Fields input(field, present);
  Options result;
  if (input.supplied("variant")) {
    const auto value = input.text("variant");
    if (value != "version_discounted_a_star" && value != "VERSION_DISCOUNTED_A_STAR" && value != "discounted_a_star")
      invalid("variant: expected version_discounted_a_star");
  }
  if (input.supplied("exponent")) result.exponent = finite_field(input.text("exponent"), "exponent");
  if constexpr (std::is_same_v<Options, PetriAntiAlignmentOptions>)
    if (input.supplied("epsilon")) result.epsilon = finite_field(input.text("epsilon"), "epsilon");
  if (input.supplied("marking_limit")) result.marking_limit = unsigned_field(input.text("marking_limit"), "marking_limit");
  if (input.supplied("max_states")) result.max_states = unsigned_field(input.text("max_states"), "max_states");
  result.initial_marking = input.marking("initial_marking", "initial_marking_json");
  result.final_marking = input.marking("final_marking", "final_marking_json");
  if (input.supplied("activity_attribute")) result.activity_attribute = input.text("activity_attribute");
  return result;
}

void quote(std::ostream& out, std::string_view value) {
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

template <class Values, class Write>
void array(std::ostream& out, const Values& values, Write write) {
  out << '[';
  bool first = true;
  for (const auto& value : values) {
    if (!first) out << ',';
    first = false; write(value);
  }
  out << ']';
}

template <class Integers>
void integers(std::ostream& out, const Integers& values) {
  array(out, values, [&](const auto& value) { out << value; });
}

template <class Write>
std::string json(Write write) {
  std::ostringstream out; out.imbue(std::locale::classic()); write(out); return out.str();
}

const char* variant_name(PetriAlignmentVariant variant) {
  return variant == PetriAlignmentVariant::kDiscountedAStar ? "VERSION_DISCOUNTED_A_STAR" : "UNKNOWN";
}

template <class Options>
std::string options_json(const Options& options) {
  return json([&](std::ostream& out) {
    out << "{\"variant\":"; quote(out, variant_name(options.variant));
    out << ",\"variant_index\":" << static_cast<std::underlying_type_t<PetriAlignmentVariant>>(options.variant)
        << ",\"exponent\":"; floating(out, options.exponent);
    if constexpr (std::is_same_v<Options, PetriAntiAlignmentOptions>) {
      out << ",\"epsilon\":"; floating(out, options.epsilon);
    }
    out << ",\"marking_limit\":" << options.marking_limit << ",\"max_states\":" << options.max_states
        << ",\"initial_marking\":"; integers(out, options.initial_marking);
    out << ",\"final_marking\":"; integers(out, options.final_marking);
    out << ",\"activity_attribute\":"; quote(out, options.activity_attribute); out << '}';
  });
}

template <class Result>
std::string result_json(const Result& result) {
  return json([&](std::ostream& out) {
    out << "{\"activities\":" << serialize_alignment_trace_words_json(result.activities) << ",\"transitions\":";
    integers(out, result.transitions);
    out << ",\"cost\":"; floating(out, result.cost);
    out << ",\"objective\":"; floating(out, result.objective);
    if constexpr (std::is_same_v<Result, PetriAntiAlignmentResult>) {
      out << ",\"precision\":"; floating(out, result.precision);
    } else {
      out << ",\"max_distance_to_log\":"; floating(out, result.max_distance_to_log);
    }
    out << ",\"diagnostics\":{\"visited_states\":" << result.diagnostics.visited_states
        << ",\"queued_states\":" << result.diagnostics.queued_states
        << ",\"traversed_arcs\":" << result.diagnostics.traversed_arcs
        << ",\"exhausted\":" << boolean(result.diagnostics.exhausted) << "}}";
  });
}

const char* arc_name(PetriArcType type) {
  switch (type) {
    case PetriArcType::kNormal: return "NORMAL";
    case PetriArcType::kReset: return "RESET";
    case PetriArcType::kInhibitor: return "INHIBITOR";
  }
  return "UNKNOWN";
}

const char* timing_name(StochasticMapTiming type) {
  switch (type) {
    case StochasticMapTiming::kTimed: return "timed";
    case StochasticMapTiming::kImmediate: return "immediate";
  }
  return "UNKNOWN";
}

const char* distribution_name(StochasticMapDistribution type) {
  switch (type) {
    case StochasticMapDistribution::kDeterministic: return "deterministic";
    case StochasticMapDistribution::kExponential: return "exponential";
    case StochasticMapDistribution::kConstant0: return "constant0";
    case StochasticMapDistribution::kImmediate: return "immediate";
    case StochasticMapDistribution::kNormal: return "normal";
    case StochasticMapDistribution::kUniform: return "uniform";
    case StochasticMapDistribution::kLogNormal: return "lognormal";
    case StochasticMapDistribution::kGamma: return "gamma";
  }
  return "UNKNOWN";
}
}  // namespace

PetriAntiAlignmentOptions parse_petri_anti_workbench_params(const FieldLookup& field, const FieldPresence& present) {
  return options_from_fields<PetriAntiAlignmentOptions>(field, present);
}

PetriMultiAlignmentOptions parse_petri_multi_workbench_params(const FieldLookup& field, const FieldPresence& present) {
  return options_from_fields<PetriMultiAlignmentOptions>(field, present);
}

std::string serialize_petri_anti_workbench_options_json(const PetriAntiAlignmentOptions& options) { return options_json(options); }
std::string serialize_petri_multi_workbench_options_json(const PetriMultiAlignmentOptions& options) { return options_json(options); }
std::string serialize_petri_anti_workbench_result_json(const PetriAntiAlignmentResult& result) { return result_json(result); }
std::string serialize_petri_multi_workbench_result_json(const PetriMultiAlignmentResult& result) { return result_json(result); }

std::string serialize_petri_workbench_document_json(const PnmlDocument& document) {
  return json([&](std::ostream& out) {
    out << "{\"net\":" << workflow_input_petri_json(document.net)
        << ",\"raw_source\":" << document.net.source() << ",\"raw_sink\":" << document.net.sink()
        << ",\"initial_marking\":";
    array(out, document.initial_marking, [&](const auto& value) { out << value.decimal(); });
    out << ",\"final_marking\":";
    array(out, document.final_marking, [&](const auto& value) { out << value.decimal(); });
    out << ",\"place_ids\":" << serialize_alignment_trace_words_json(document.place_ids)
        << ",\"transition_ids\":" << serialize_alignment_trace_words_json(document.transition_ids)
        << ",\"initial_marking_keys\":";
    array(out, document.initial_marking_keys, [&](auto value) { out << boolean(value); });
    out << ",\"final_marking_keys\":";
    array(out, document.final_marking_keys, [&](auto value) { out << boolean(value); });
    out << ",\"arc_occurrences\":";
    if (!document.arc_occurrences) out << "null";
    else array(out, *document.arc_occurrences, [&](const auto& arc) {
      out << "{\"place\":" << arc.place << ",\"transition\":" << arc.transition
          << ",\"input\":" << boolean(arc.input) << ",\"type\":"; quote(out, arc_name(arc.type));
      out << ",\"type_index\":" << static_cast<std::underlying_type_t<PetriArcType>>(arc.type)
          << ",\"weight\":" << arc.weight.decimal() << ",\"negative_weight\":" << boolean(arc.negative_weight) << ",\"id\":";
      quote(out, arc.id); out << '}';
    });
    out << ",\"stochastic_map\":";
    if (!document.stochastic_map) out << "null";
    else {
      out << "{\"transitions\":";
      array(out, document.stochastic_map->transitions, [&](const auto& record) {
        out << "{\"transition\":" << record.transition << ",\"timing\":"; quote(out, timing_name(record.timing));
        out << ",\"timing_index\":" << static_cast<std::underlying_type_t<StochasticMapTiming>>(record.timing) << ",\"distribution\":";
        quote(out, distribution_name(record.distribution));
        out << ",\"distribution_index\":" << static_cast<std::underlying_type_t<StochasticMapDistribution>>(record.distribution)
            << ",\"parameter\":"; floating(out, record.parameter);
        out << ",\"secondary_parameter\":"; floating(out, record.secondary_parameter);
        out << ",\"tertiary_parameter\":"; floating(out, record.tertiary_parameter);
        out << ",\"activation_weight\":"; floating(out, record.activation_weight);
        out << ",\"priority\":" << record.priority << '}';
      });
      out << '}';
    }
    out << '}';
  });
}

}  // namespace minera
