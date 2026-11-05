#include "minera/playout_workbench_json.hpp"
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
  throw std::invalid_argument("playout workbench JSON: " + message);
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

const char* dfg_name(DfgPlayoutVariant value) {
  switch (value) {
    case DfgPlayoutVariant::kClassic: return "classic";
    case DfgPlayoutVariant::kPerformance: return "performance";
  }
  return "UNKNOWN";
}

const char* tree_name(ProcessTreePlayoutVariant value) {
  switch (value) {
    case ProcessTreePlayoutVariant::kBasicPlayout: return "basic";
    case ProcessTreePlayoutVariant::kExtensive: return "extensive";
    case ProcessTreePlayoutVariant::kTopBottom: return "top_bottom";
  }
  return "UNKNOWN";
}

const char* visited_name(PlayoutVisitedElementKind value) {
  switch (value) {
    case PlayoutVisitedElementKind::MARKING: return "marking";
    case PlayoutVisitedElementKind::TRANSITION: return "transition";
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

std::ostringstream output() {
  std::ostringstream result;
  result.imbue(std::locale::classic());
  return result;
}

void set_supplied(const Json::Object& object, std::set<std::string>* supplied) {
  if (!supplied) return;
  std::set<std::string> actual;
  for (const auto& [key, value] : object) {
    (void)value;
    actual.insert(key);
  }
  *supplied = std::move(actual);
}

void visited_element_json(std::ostream& out, const PlayoutVisitedElement& value) {
  out << "{\"kind\":";
  enum_json(out, value.kind, visited_name);
  out << ",\"marking\":";
  array_json(out, value.marking, integer<std::uint32_t>);
  out << ",\"transition_index\":" << value.transition_index << '}';
}

void words_json(std::ostream& out, const std::vector<std::string>& words) {
  array_json(out, words, quoted);
}

}  // namespace

PlayoutOptions parse_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"traces", "max_steps", "seed", "start", "case_interval", "event_interval", "initial_marking", "final_marking", "add_only_if_fm_is_reached", "fm_leq_accepted", "return_visited_elements"});
  PlayoutOptions result;
  if (const auto* item = field(object, "traces")) result.traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_steps")) result.max_steps = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "seed")) result.seed = native_unsigned<std::uint64_t>(*item);
  if (const auto* item = field(object, "start")) result.start = ieee_double(*item);
  if (const auto* item = field(object, "case_interval")) result.case_interval = ieee_double(*item);
  if (const auto* item = field(object, "event_interval")) result.event_interval = ieee_double(*item);
  if (const auto* item = field(object, "initial_marking")) result.initial_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "final_marking")) result.final_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "add_only_if_fm_is_reached")) result.add_only_if_fm_is_reached = item->boolean();
  if (const auto* item = field(object, "fm_leq_accepted")) result.fm_leq_accepted = item->boolean();
  if (const auto* item = field(object, "return_visited_elements")) result.return_visited_elements = item->boolean();
  // Explicit timing replaces the reference envelope (cases "0".., epoch
  // 10,000,000, one second per event).
  result.add_envelope = field(object, "start") || field(object, "case_interval") || field(object, "event_interval");
  set_supplied(object, supplied);
  return result;
}

StochasticPlayoutOptions parse_stochastic_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"traces", "max_steps", "seed", "start", "case_interval", "event_interval", "transition_weights", "stop_at_final", "initial_marking", "final_marking", "fit_from_log", "fitting_max_states", "return_visited_elements"});
  StochasticPlayoutOptions result;
  if (const auto* item = field(object, "traces")) result.traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_steps")) result.max_steps = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "seed")) result.seed = native_unsigned<std::uint64_t>(*item);
  if (const auto* item = field(object, "start")) result.start = ieee_double(*item);
  if (const auto* item = field(object, "case_interval")) result.case_interval = ieee_double(*item);
  if (const auto* item = field(object, "event_interval")) result.event_interval = ieee_double(*item);
  if (const auto* item = field(object, "transition_weights")) result.transition_weights = vector_value<double>(*item, ieee_double);
  if (const auto* item = field(object, "stop_at_final")) result.stop_at_final = item->boolean();
  if (const auto* item = field(object, "initial_marking")) result.initial_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "final_marking")) result.final_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "fit_from_log")) result.fit_from_log = item->boolean();
  if (const auto* item = field(object, "fitting_max_states")) result.fitting_max_states = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "return_visited_elements")) result.return_visited_elements = item->boolean();
  set_supplied(object, supplied);
  return result;
}

ExtensivePlayoutOptions parse_extensive_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"max_length", "max_traces", "max_states", "start", "case_interval", "event_interval", "initial_marking", "final_marking", "branching_factor_transitions", "seed", "return_elements", "max_marking_occ"});
  ExtensivePlayoutOptions result;
  if (const auto* item = field(object, "max_length")) result.max_length = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_traces")) result.max_traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_states")) result.max_states = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "start")) result.start = ieee_double(*item);
  if (const auto* item = field(object, "case_interval")) result.case_interval = ieee_double(*item);
  if (const auto* item = field(object, "event_interval")) result.event_interval = ieee_double(*item);
  if (const auto* item = field(object, "initial_marking")) result.initial_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "final_marking")) result.final_marking = vector_value<std::uint32_t>(*item, native_unsigned<std::uint32_t>);
  if (const auto* item = field(object, "branching_factor_transitions")) result.branching_factor_transitions = ieee_double(*item);
  if (const auto* item = field(object, "seed")) result.seed = native_unsigned<std::uint64_t>(*item);
  if (const auto* item = field(object, "return_elements")) result.return_elements = item->boolean();
  if (const auto* item = field(object, "max_marking_occ")) result.max_marking_occ = item->null() ? std::nullopt : std::optional<std::size_t>{native_unsigned<std::size_t>(*item)};
  set_supplied(object, supplied);
  return result;
}

DfgPlayoutOptions parse_dfg_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"variant", "max_variants", "min_weighted_probability", "max_occurrences_per_activity", "interrupt_when_dfg_complete", "add_trace_if_takes_new_edges", "max_execution_time_ms", "return_variants", "return_only_if_complete", "min_variant_occurrences", "traces", "case_arrival_rate", "seed", "max_events_per_trace", "start_time", "activity_key", "timestamp_key", "case_id_key", "artificial_start_activity", "artificial_end_activity", "max_steps"});
  DfgPlayoutOptions result;
  if (const auto* item = field(object, "variant")) result.variant = enum_value<DfgPlayoutVariant>(*item, dfg_name, 2);
  if (const auto* item = field(object, "max_variants")) result.max_variants = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "min_weighted_probability")) result.min_weighted_probability = ieee_double(*item);
  if (const auto* item = field(object, "max_occurrences_per_activity")) result.max_occurrences_per_activity = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "interrupt_when_dfg_complete")) result.interrupt_when_dfg_complete = item->boolean();
  if (const auto* item = field(object, "add_trace_if_takes_new_edges")) result.add_trace_if_takes_new_edges = item->boolean();
  if (const auto* item = field(object, "max_execution_time_ms")) result.max_execution_time_ms = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "return_variants")) result.return_variants = item->boolean();
  if (const auto* item = field(object, "return_only_if_complete")) result.return_only_if_complete = item->boolean();
  if (const auto* item = field(object, "min_variant_occurrences")) result.min_variant_occurrences = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "traces")) result.traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "case_arrival_rate")) result.case_arrival_rate = ieee_double(*item);
  if (const auto* item = field(object, "seed")) result.seed = native_unsigned<std::uint64_t>(*item);
  if (const auto* item = field(object, "max_events_per_trace")) result.max_events_per_trace = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "start_time")) result.start_time = item->string();
  if (const auto* item = field(object, "activity_key")) result.activity_key = item->string();
  if (const auto* item = field(object, "timestamp_key")) result.timestamp_key = item->string();
  if (const auto* item = field(object, "case_id_key")) result.case_id_key = item->string();
  if (const auto* item = field(object, "artificial_start_activity")) result.artificial_start_activity = item->string();
  if (const auto* item = field(object, "artificial_end_activity")) result.artificial_end_activity = item->string();
  if (const auto* item = field(object, "max_steps")) result.max_steps = native_unsigned<std::size_t>(*item);
  set_supplied(object, supplied);
  return result;
}

ProcessTreePlayoutOptions parse_process_tree_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"variant", "traces", "max_steps", "seed", "max_length", "min_length", "max_traces", "max_loop_occurrences", "start", "case_interval", "event_interval", "return_set_strings"});
  ProcessTreePlayoutOptions result;
  if (const auto* item = field(object, "variant")) result.variant = enum_value<ProcessTreePlayoutVariant>(*item, tree_name, 3);
  if (const auto* item = field(object, "traces")) result.traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_steps")) result.max_steps = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "seed")) result.seed = native_unsigned<std::uint64_t>(*item);
  if (const auto* item = field(object, "max_length")) result.max_length = item->null() ? std::nullopt : std::optional<std::size_t>{native_unsigned<std::size_t>(*item)};
  if (const auto* item = field(object, "min_length")) result.min_length = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_traces")) result.max_traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_loop_occurrences")) result.max_loop_occurrences = item->null() ? std::nullopt : std::optional<std::size_t>{native_unsigned<std::size_t>(*item)};
  if (const auto* item = field(object, "start")) result.start = ieee_double(*item);
  if (const auto* item = field(object, "case_interval")) result.case_interval = ieee_double(*item);
  if (const auto* item = field(object, "event_interval")) result.event_interval = ieee_double(*item);
  if (const auto* item = field(object, "return_set_strings")) result.return_set_strings = item->boolean();
  result.add_envelope = field(object, "start") || field(object, "case_interval") || field(object, "event_interval");
  set_supplied(object, supplied);
  return result;
}

std::string serialize_playout_workbench_options_json(const PlayoutOptions& options) {
  auto out = output();
  out << "{\"traces\":"; out << options.traces;
  out << ",\"max_steps\":"; out << options.max_steps;
  out << ",\"seed\":"; out << options.seed;
  out << ",\"start\":"; number(out, options.start);
  out << ",\"case_interval\":"; number(out, options.case_interval);
  out << ",\"event_interval\":"; number(out, options.event_interval);
  out << ",\"initial_marking\":"; array_json(out, options.initial_marking, integer<std::uint32_t>);
  out << ",\"final_marking\":"; array_json(out, options.final_marking, integer<std::uint32_t>);
  out << ",\"add_only_if_fm_is_reached\":"; boolean(out, options.add_only_if_fm_is_reached);
  out << ",\"fm_leq_accepted\":"; boolean(out, options.fm_leq_accepted);
  out << ",\"return_visited_elements\":"; boolean(out, options.return_visited_elements);
  out << '}';
  return out.str();
}

std::string serialize_playout_workbench_options_json(const StochasticPlayoutOptions& options) {
  auto out = output();
  out << "{\"traces\":"; out << options.traces;
  out << ",\"max_steps\":"; out << options.max_steps;
  out << ",\"seed\":"; out << options.seed;
  out << ",\"start\":"; number(out, options.start);
  out << ",\"case_interval\":"; number(out, options.case_interval);
  out << ",\"event_interval\":"; number(out, options.event_interval);
  out << ",\"transition_weights\":"; array_json(out, options.transition_weights, number);
  out << ",\"stop_at_final\":"; boolean(out, options.stop_at_final);
  out << ",\"initial_marking\":"; array_json(out, options.initial_marking, integer<std::uint32_t>);
  out << ",\"final_marking\":"; array_json(out, options.final_marking, integer<std::uint32_t>);
  out << ",\"fit_from_log\":"; boolean(out, options.fit_from_log);
  out << ",\"fitting_max_states\":"; out << options.fitting_max_states;
  out << ",\"return_visited_elements\":"; boolean(out, options.return_visited_elements);
  out << '}';
  return out.str();
}

std::string serialize_playout_workbench_options_json(const ExtensivePlayoutOptions& options) {
  auto out = output();
  out << "{\"max_length\":"; out << options.max_length;
  out << ",\"max_traces\":"; out << options.max_traces;
  out << ",\"max_states\":"; out << options.max_states;
  out << ",\"start\":"; number(out, options.start);
  out << ",\"case_interval\":"; number(out, options.case_interval);
  out << ",\"event_interval\":"; number(out, options.event_interval);
  out << ",\"initial_marking\":"; array_json(out, options.initial_marking, integer<std::uint32_t>);
  out << ",\"final_marking\":"; array_json(out, options.final_marking, integer<std::uint32_t>);
  out << ",\"branching_factor_transitions\":"; number(out, options.branching_factor_transitions);
  out << ",\"seed\":"; out << options.seed;
  out << ",\"return_elements\":"; boolean(out, options.return_elements);
  out << ",\"max_marking_occ\":"; if (options.max_marking_occ) out << *options.max_marking_occ; else out << "null";
  out << '}';
  return out.str();
}

std::string serialize_playout_workbench_options_json(const DfgPlayoutOptions& options) {
  auto out = output();
  out << "{\"variant\":"; enum_json(out, options.variant, dfg_name);
  out << ",\"max_variants\":"; out << options.max_variants;
  out << ",\"min_weighted_probability\":"; number(out, options.min_weighted_probability);
  out << ",\"max_occurrences_per_activity\":"; out << options.max_occurrences_per_activity;
  out << ",\"interrupt_when_dfg_complete\":"; boolean(out, options.interrupt_when_dfg_complete);
  out << ",\"add_trace_if_takes_new_edges\":"; boolean(out, options.add_trace_if_takes_new_edges);
  out << ",\"max_execution_time_ms\":"; out << options.max_execution_time_ms;
  out << ",\"return_variants\":"; boolean(out, options.return_variants);
  out << ",\"return_only_if_complete\":"; boolean(out, options.return_only_if_complete);
  out << ",\"min_variant_occurrences\":"; out << options.min_variant_occurrences;
  out << ",\"traces\":"; out << options.traces;
  out << ",\"case_arrival_rate\":"; number(out, options.case_arrival_rate);
  out << ",\"seed\":"; out << options.seed;
  out << ",\"max_events_per_trace\":"; out << options.max_events_per_trace;
  out << ",\"start_time\":"; quoted(out, options.start_time);
  out << ",\"activity_key\":"; quoted(out, options.activity_key);
  out << ",\"timestamp_key\":"; quoted(out, options.timestamp_key);
  out << ",\"case_id_key\":"; quoted(out, options.case_id_key);
  out << ",\"artificial_start_activity\":"; quoted(out, options.artificial_start_activity);
  out << ",\"artificial_end_activity\":"; quoted(out, options.artificial_end_activity);
  out << ",\"max_steps\":"; out << options.max_steps;
  out << '}';
  return out.str();
}

std::string serialize_playout_workbench_options_json(const ProcessTreePlayoutOptions& options) {
  auto out = output();
  out << "{\"variant\":"; enum_json(out, options.variant, tree_name);
  out << ",\"traces\":"; out << options.traces;
  out << ",\"max_steps\":"; out << options.max_steps;
  out << ",\"seed\":"; out << options.seed;
  out << ",\"max_length\":"; if (options.max_length) out << *options.max_length; else out << "null";
  out << ",\"min_length\":"; out << options.min_length;
  out << ",\"max_traces\":"; out << options.max_traces;
  out << ",\"max_loop_occurrences\":"; if (options.max_loop_occurrences) out << *options.max_loop_occurrences; else out << "null";
  out << ",\"start\":"; number(out, options.start);
  out << ",\"case_interval\":"; number(out, options.case_interval);
  out << ",\"event_interval\":"; number(out, options.event_interval);
  out << ",\"return_set_strings\":"; boolean(out, options.return_set_strings);
  out << '}';
  return out.str();
}

std::string serialize_playout_workbench_result_json(const PlayoutVisitedResult& result) {
  auto out = output();
  out << "{\"traces\":";
  array_json(out, result.traces, [](std::ostream& stream, const PlayoutVisitedTrace& trace) {
    array_json(stream, trace, visited_element_json);
  });
  out << ",\"truncated\":"; boolean(out, result.truncated);
  out << ",\"trace_limit_reached\":"; boolean(out, result.trace_limit_reached);
  out << ",\"state_limit_reached\":"; boolean(out, result.state_limit_reached);
  out << '}';
  return out.str();
}

std::string serialize_playout_workbench_result_json(const DfgPlayoutResult& result) {
  auto out = output();
  out << "{\"variant\":"; enum_json(out, result.variant, dfg_name);
  out << ",\"log\":" << serialize_active_source_log_json(result.log);
  out << ",\"counts\":[";
  bool comma = false;
  for (const auto& [activities, count] : result.counts) {
    if (comma) out << ',';
    comma = true;
    out << "{\"activities\":"; words_json(out, activities);
    out << ",\"count\":" << count << '}';
  }
  out << "],\"complete\":"; boolean(out, result.complete);
  out << ",\"interrupted\":"; boolean(out, result.interrupted);
  out << ",\"completed_traces\":" << result.completed_traces;
  out << ",\"truncated_traces\":" << result.truncated_traces << '}';
  return out.str();
}

std::string serialize_playout_workbench_result_json(const ProcessTreePlayoutResult& result) {
  auto out = output();
  out << "{\"log\":" << serialize_active_source_log_json(result.log);
  out << ",\"trace_strings\":"; array_json(out, result.trace_strings, words_json);
  out << ",\"truncated\":"; boolean(out, result.truncated);
  out << '}';
  return out.str();
}

}  // namespace minera
