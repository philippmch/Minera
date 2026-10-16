#include "minera/monte_carlo_workbench_json.hpp"
#include "minera/active_event_source.hpp"
#include "minera/graph_alignment_workbench_json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("monte carlo workbench JSON: " + message);
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

void allowed_fields(const Json::Object& object,
                    std::initializer_list<std::string_view> allowed) {
  for (const auto& [name, value] : object) {
    (void)value;
    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
      invalid("unknown field " + name);
  }
}

const Json* field(const Json::Object& object, std::string_view name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

std::uint64_t unsigned_integer(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      !std::all_of(number->text.begin(), number->text.end(),
                   [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected an unsigned JSON integer token");
  std::uint64_t result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("unsigned integer exceeds uint64 range");
  return result;
}

template <class Integer>
Integer native_unsigned(const Json& value) {
  const auto result = unsigned_integer(value);
  if (result > std::numeric_limits<Integer>::max())
    invalid("integer exceeds native unsigned storage range");
  return static_cast<Integer>(result);
}

int native_int(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      number->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact native int token");
  int result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("integer exceeds native int storage range");
  return result;
}

double ieee_double(const Json& value) {
  if (const auto* tag = std::get_if<std::string>(&value.value)) {
    if (*tag == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (*tag == "INF") return std::numeric_limits<double>::infinity();
    if (*tag == "-INF") return -std::numeric_limits<double>::infinity();
    invalid("expected an IEEE double tag NaN, INF, or -INF");
  }
  const auto* number = std::get_if<Number>(&value.value);
  if (!number) invalid("expected a numeric double or IEEE tag");
  double result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result,
      std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == number->text.data() + number->text.size())
    return result;
  // from_chars reports range errors for decimal tokens that round below the
  // smallest subnormal as well as for overflow. Preserve signed underflow
  // zero without accepting numeric overflow as an implicit infinity tag.
  if (parsed.ec == std::errc::result_out_of_range) {
    std::istringstream input(number->text);
    input.imbue(std::locale::classic());
    input >> result;
    if (result == 0.0 && input.eof())
      return !number->text.empty() && number->text.front() == '-' ? -0.0 : 0.0;
  }
  invalid("double token exceeds finite native storage; use an explicit IEEE tag");
}

const char* duration_name(MonteCarloDurationDistribution value) {
  switch (value) {
    case MonteCarloDurationDistribution::kDeterministic: return "deterministic";
    case MonteCarloDurationDistribution::kExponential: return "exponential";
    case MonteCarloDurationDistribution::kConstant0: return "constant0";
    case MonteCarloDurationDistribution::kImmediate: return "immediate";
    case MonteCarloDurationDistribution::kNormal: return "normal";
    case MonteCarloDurationDistribution::kUniform: return "uniform";
    case MonteCarloDurationDistribution::kLogNormal: return "lognormal";
    case MonteCarloDurationDistribution::kGamma: return "gamma";
  }
  return "UNKNOWN";
}

const char* map_distribution_name(StochasticMapDistribution value) {
  switch (value) {
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

const char* timing_name(StochasticMapTiming value) {
  switch (value) {
    case StochasticMapTiming::kTimed: return "timed";
    case StochasticMapTiming::kImmediate: return "immediate";
  }
  return "UNKNOWN";
}

const char* replay_name(TokenReplayVariant value) {
  switch (value) {
    case TokenReplayVariant::kForward: return "forward";
    case TokenReplayVariant::kBackwards: return "backwards";
  }
  return "UNKNOWN";
}

template <class Enum, class Name>
Enum enum_value(const Json& value, Name name, int known_count) {
  if (const auto* text = std::get_if<std::string>(&value.value)) {
    for (int index = 0; index < known_count; ++index)
      if (*text == name(static_cast<Enum>(index))) return static_cast<Enum>(index);
    invalid("unknown enum name " + *text);
  }
  const auto& object = value.object();
  allowed_fields(object, {"name", "index"});
  if (object.size() != 2) invalid("enum object requires name and index");
  const auto result = static_cast<Enum>(native_int(object.at("index")));
  if (object.at("name").string() != name(result))
    invalid("enum name and index disagree");
  return result;
}

template <class T, class Convert>
std::vector<T> vector_value(const Json& value, Convert convert) {
  std::vector<T> result;
  result.reserve(value.array().size());
  for (const auto& item : value.array()) result.push_back(convert(item));
  return result;
}

MonteCarloDuration duration_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"distribution", "value_seconds", "secondary_seconds", "tertiary_seconds"});
  MonteCarloDuration result;
  if (const auto* item = field(object, "distribution"))
    result.distribution = enum_value<MonteCarloDurationDistribution>(*item, duration_name, 8);
  if (const auto* item = field(object, "value_seconds")) result.value_seconds = ieee_double(*item);
  if (const auto* item = field(object, "secondary_seconds")) result.secondary_seconds = ieee_double(*item);
  if (const auto* item = field(object, "tertiary_seconds")) result.tertiary_seconds = ieee_double(*item);
  return result;
}

StochasticMapTransition map_record(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"transition", "timing", "distribution", "parameter",
      "secondary_parameter", "tertiary_parameter", "activation_weight", "priority"});
  StochasticMapTransition result;
  if (const auto* item = field(object, "transition")) result.transition = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "timing")) result.timing = enum_value<StochasticMapTiming>(*item, timing_name, 2);
  if (const auto* item = field(object, "distribution")) result.distribution = enum_value<StochasticMapDistribution>(*item, map_distribution_name, 8);
  if (const auto* item = field(object, "parameter")) result.parameter = ieee_double(*item);
  if (const auto* item = field(object, "secondary_parameter")) result.secondary_parameter = ieee_double(*item);
  if (const auto* item = field(object, "tertiary_parameter")) result.tertiary_parameter = ieee_double(*item);
  if (const auto* item = field(object, "activation_weight")) result.activation_weight = ieee_double(*item);
  if (const auto* item = field(object, "priority")) result.priority = native_int(*item);
  return result;
}

StochasticMap map_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"transitions"});
  StochasticMap result;
  if (const auto* item = field(object, "transitions"))
    result.transitions = vector_value<StochasticMapTransition>(*item, map_record);
  return result;
}

std::map<std::size_t, std::size_t> resource_map(const Json& value) {
  std::map<std::size_t, std::size_t> result;
  for (const auto& [key, item] : value.object()) {
    // JSON object keys are strings, but only a unique canonical unsigned
    // decimal representation names a native map key. Reject aliases such
    // as 00 or +0 instead of silently merging them into the same place.
    if (key.empty() || (key.size() > 1 && key.front() == '0') ||
        !std::all_of(key.begin(), key.end(), [](char c) { return c >= '0' && c <= '9'; }))
      invalid("resource-map key must be a canonical decimal size_t");
    result.emplace(native_unsigned<std::size_t>(Json{Number{key}}),
                   native_unsigned<std::size_t>(item));
  }
  return result;
}

void quoted(std::ostream& out, const std::string& value) {
  const auto array = serialize_alignment_trace_words_json({value});
  out.write(array.data() + 1, static_cast<std::streamsize>(array.size() - 2));
}

void number(std::ostream& out, double value) {
  if (std::isnan(value)) out << "\"NaN\"";
  else if (std::isinf(value)) out << (value < 0 ? "\"-INF\"" : "\"INF\"");
  else if (value == 0.0 && std::signbit(value)) out << "-0.0";
  else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
}

template <class Enum, class Name>
void enum_json(std::ostream& out, Enum value, Name name) {
  out << "{\"name\":";
  quoted(out, name(value));
  out << ",\"index\":" << static_cast<std::underlying_type_t<Enum>>(value) << '}';
}

template <class T, class Write>
void array_json(std::ostream& out, const std::vector<T>& values, Write write) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    write(out, values[i]);
  }
  out << ']';
}

template <class Integer>
void integer(std::ostream& out, Integer value) { out << value; }

void boolean(std::ostream& out, bool value) { out << (value ? "true" : "false"); }

void duration_json(std::ostream& out, const MonteCarloDuration& value) {
  out << "{\"distribution\":";
  enum_json(out, value.distribution, duration_name);
  out << ",\"value_seconds\":"; number(out, value.value_seconds);
  out << ",\"secondary_seconds\":"; number(out, value.secondary_seconds);
  out << ",\"tertiary_seconds\":"; number(out, value.tertiary_seconds);
  out << '}';
}

void map_record_json(std::ostream& out, const StochasticMapTransition& value) {
  out << "{\"transition\":" << value.transition << ",\"timing\":";
  enum_json(out, value.timing, timing_name);
  out << ",\"distribution\":"; enum_json(out, value.distribution, map_distribution_name);
  out << ",\"parameter\":"; number(out, value.parameter);
  out << ",\"secondary_parameter\":"; number(out, value.secondary_parameter);
  out << ",\"tertiary_parameter\":"; number(out, value.tertiary_parameter);
  out << ",\"activation_weight\":"; number(out, value.activation_weight);
  out << ",\"priority\":" << value.priority << '}';
}

void map_json(std::ostream& out, const StochasticMap& value) {
  out << "{\"transitions\":";
  array_json(out, value.transitions, map_record_json);
  out << '}';
}

std::ostringstream output() {
  std::ostringstream result;
  result.imbue(std::locale::classic());
  return result;
}

void interval_json(std::ostream& out, const MonteCarloInterval& interval) {
  out << "{\"start\":"; number(out, interval.start);
  out << ",\"end\":"; number(out, interval.end); out << '}';
}

void place_tree_json(std::ostream& out, const MonteCarloPlaceIntervalTree& tree) {
  out << "{\"place_index\":" << tree.place_index << ",\"intervals\":";
  array_json(out, tree.intervals, interval_json); out << '}';
}

void transition_tree_json(std::ostream& out, const MonteCarloTransitionIntervalTree& tree) {
  out << "{\"transition_index\":" << tree.transition_index << ",\"intervals\":";
  array_json(out, tree.intervals, interval_json); out << '}';
}

}  // namespace

MonteCarloOptions parse_monte_carlo_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"simulations", "case_arrival_ratio", "case_arrival_seconds",
      "activity_key", "timestamp_key", "initial_marking", "final_marking", "max_steps",
      "seed", "start", "default_duration_seconds", "transition_weights",
      "transition_durations", "resources_per_place", "default_resources_per_place",
      "enable_diagnostics", "stop_at_final", "force_distribution", "stochastic_map",
      "token_replay_variant", "diagnostic_interval", "small_scale_factor",
      "max_thread_execution_time", "place_names", "transition_names",
      "initial_marking_keys", "final_marking_keys"});
  MonteCarloOptions result;
  if (const auto* item = field(object, "simulations")) result.simulations = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "case_arrival_ratio"))
    result.case_arrival_ratio = item->null() ? std::nullopt : std::optional<double>{ieee_double(*item)};
  if (const auto* item = field(object, "case_arrival_seconds"))
    result.case_arrival_seconds = item->null() ? std::nullopt : std::optional<double>{ieee_double(*item)};
  if (const auto* item = field(object, "activity_key")) result.activity_key = item->string();
  if (const auto* item = field(object, "timestamp_key")) result.timestamp_key = item->string();
  if (const auto* item = field(object, "initial_marking")) result.initial_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "final_marking")) result.final_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "max_steps")) result.max_steps = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "seed")) result.seed = native_unsigned<std::uint64_t>(*item);
  if (const auto* item = field(object, "start")) result.start = ieee_double(*item);
  if (const auto* item = field(object, "default_duration_seconds")) result.default_duration_seconds = ieee_double(*item);
  if (const auto* item = field(object, "transition_weights")) result.transition_weights = vector_value<double>(*item, ieee_double);
  if (const auto* item = field(object, "transition_durations")) result.transition_durations = vector_value<MonteCarloDuration>(*item, duration_value);
  if (const auto* item = field(object, "resources_per_place")) result.resources_per_place = resource_map(*item);
  if (const auto* item = field(object, "default_resources_per_place")) result.default_resources_per_place = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "enable_diagnostics")) result.enable_diagnostics = item->boolean();
  if (const auto* item = field(object, "stop_at_final")) result.stop_at_final = item->boolean();
  if (const auto* item = field(object, "force_distribution"))
    result.force_distribution = item->null() ? std::nullopt :
        std::optional<MonteCarloDurationDistribution>{enum_value<MonteCarloDurationDistribution>(*item, duration_name, 8)};
  if (const auto* item = field(object, "stochastic_map"))
    result.stochastic_map = item->null() ? std::nullopt : std::optional<StochasticMap>{map_value(*item)};
  if (const auto* item = field(object, "token_replay_variant")) result.token_replay_variant = enum_value<TokenReplayVariant>(*item, replay_name, 2);
  if (const auto* item = field(object, "diagnostic_interval")) result.diagnostic_interval = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "small_scale_factor")) result.small_scale_factor = ieee_double(*item);
  if (const auto* item = field(object, "max_thread_execution_time")) result.max_thread_execution_time = ieee_double(*item);
  if (const auto* item = field(object, "place_names")) result.place_names = vector_value<std::string>(*item, [](const Json& value) { return value.string(); });
  if (const auto* item = field(object, "transition_names")) result.transition_names = vector_value<std::string>(*item, [](const Json& value) { return value.string(); });
  if (const auto* item = field(object, "initial_marking_keys")) result.initial_marking_keys = vector_value<bool>(*item, [](const Json& value) { return value.boolean(); });
  if (const auto* item = field(object, "final_marking_keys")) result.final_marking_keys = vector_value<bool>(*item, [](const Json& value) { return value.boolean(); });
  if (supplied) {
    std::set<std::string> actual;
    for (const auto& [key, value] : object) {
      (void)value;
      actual.insert(key);
    }
    *supplied = std::move(actual);
  }
  return result;
}

std::string serialize_monte_carlo_workbench_options_json(const MonteCarloOptions& options) {
  auto out = output();
  out << "{\"simulations\":" << options.simulations << ",\"case_arrival_ratio\":";
  if (options.case_arrival_ratio) number(out, *options.case_arrival_ratio); else out << "null";
  out << ",\"case_arrival_seconds\":";
  if (options.case_arrival_seconds) number(out, *options.case_arrival_seconds); else out << "null";
  out << ",\"activity_key\":"; quoted(out, options.activity_key);
  out << ",\"timestamp_key\":"; quoted(out, options.timestamp_key);
  out << ",\"initial_marking\":"; array_json(out, options.initial_marking, integer<std::uint32_t>);
  out << ",\"final_marking\":"; array_json(out, options.final_marking, integer<std::uint32_t>);
  out << ",\"max_steps\":" << options.max_steps << ",\"seed\":" << options.seed;
  out << ",\"start\":"; number(out, options.start);
  out << ",\"default_duration_seconds\":"; number(out, options.default_duration_seconds);
  out << ",\"transition_weights\":"; array_json(out, options.transition_weights, number);
  out << ",\"transition_durations\":"; array_json(out, options.transition_durations, duration_json);
  out << ",\"resources_per_place\":{";
  bool comma = false;
  for (const auto& [key, value] : options.resources_per_place) {
    if (comma) out << ',';
    comma = true; quoted(out, std::to_string(key)); out << ':' << value;
  }
  out << "},\"default_resources_per_place\":" << options.default_resources_per_place;
  out << ",\"enable_diagnostics\":"; boolean(out, options.enable_diagnostics);
  out << ",\"stop_at_final\":"; boolean(out, options.stop_at_final);
  out << ",\"force_distribution\":";
  if (options.force_distribution) enum_json(out, *options.force_distribution, duration_name); else out << "null";
  out << ",\"stochastic_map\":";
  if (options.stochastic_map) map_json(out, *options.stochastic_map); else out << "null";
  out << ",\"token_replay_variant\":"; enum_json(out, options.token_replay_variant, replay_name);
  out << ",\"diagnostic_interval\":" << options.diagnostic_interval;
  out << ",\"small_scale_factor\":"; number(out, options.small_scale_factor);
  out << ",\"max_thread_execution_time\":"; number(out, options.max_thread_execution_time);
  out << ",\"place_names\":"; array_json(out, options.place_names, quoted);
  out << ",\"transition_names\":"; array_json(out, options.transition_names, quoted);
  out << ",\"initial_marking_keys\":"; array_json(out, options.initial_marking_keys, boolean);
  out << ",\"final_marking_keys\":"; array_json(out, options.final_marking_keys, boolean);
  out << '}';
  return out.str();
}

std::string serialize_monte_carlo_workbench_result_json(const MonteCarloResult& result) {
  auto out = output();
  const auto& d = result.diagnostics;
  out << "{\"log\":" << serialize_active_source_log_json(result.log) << ",\"diagnostics\":{";
  out << "\"simulated_cases\":" << d.simulated_cases << ",\"completed_cases\":" << d.completed_cases
      << ",\"deadlocked_cases\":" << d.deadlocked_cases << ",\"step_limited_cases\":" << d.step_limited_cases;
  out << ",\"case_arrival_seconds\":"; number(out, d.case_arrival_seconds);
  out << ",\"total_cases_seconds\":"; number(out, d.total_cases_seconds);
  out << ",\"median_case_execution_seconds\":"; number(out, d.median_case_execution_seconds);
  out << ",\"case_execution_seconds\":"; array_json(out, d.case_execution_seconds, number);
  out << ",\"case_arrival_ratio\":"; number(out, d.case_arrival_ratio);
  out << ",\"total_cases_time\":"; number(out, d.total_cases_time);
  out << ",\"median_cases_ex_time\":"; number(out, d.median_cases_ex_time);
  out << ",\"cases_ex_time\":"; array_json(out, d.cases_ex_time, number);
  out << "},\"output_places_interval_trees\":";
  array_json(out, result.output_places_interval_trees, place_tree_json);
  out << ",\"output_transitions_interval_trees\":";
  array_json(out, result.output_transitions_interval_trees, transition_tree_json);
  out << '}';
  return out.str();
}

}  // namespace minera
