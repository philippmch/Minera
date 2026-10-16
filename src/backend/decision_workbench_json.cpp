#include "minera/decision_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"
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
  throw std::invalid_argument("decision workbench JSON: " + message);
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

DecisionMiningOptions mining_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"max_depth", "min_split", "min_leaf", "max_categories",
      "max_thresholds", "min_gain", "include_trace_attributes", "attributes", "activity_attribute"});
  DecisionMiningOptions result;
  if (const auto* item = field(object, "max_depth")) result.max_depth = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "min_split")) result.min_split = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "min_leaf")) result.min_leaf = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_categories")) result.max_categories = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_thresholds")) result.max_thresholds = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "min_gain")) result.min_gain = ieee_double(*item);
  if (const auto* item = field(object, "include_trace_attributes")) result.include_trace_attributes = item->boolean();
  if (const auto* item = field(object, "attributes")) result.attributes = vector_value<std::string>(*item, [](const Json& text) {return text.string();});
  if (const auto* item = field(object, "activity_attribute")) result.activity_attribute = item->string();
  return result;
}

void collect_fields(const Json::Object& object, const std::string& prefix,
                    std::set<std::string>& supplied) {
  for (const auto& [key, value] : object) {
    const auto path = prefix + key;
    supplied.insert(path);
    if (const auto* nested = std::get_if<Json::Object>(&value.value))
      collect_fields(*nested, path + '.', supplied);
  }
}

void set_supplied(const Json::Object& object, std::set<std::string>* supplied) {
  if (!supplied) return;
  std::set<std::string> actual;
  collect_fields(object, "", actual);
  *supplied = std::move(actual);
}

void split_json(std::ostream& out, const DecisionSplit& value) {
  out << "{\"key\":"; quoted(out, value.key);
  out << ",\"numeric\":"; boolean(out, value.numeric);
  out << ",\"value\":"; quoted(out, value.value);
  out << ",\"threshold\":"; number(out, value.threshold);
  out << '}';
}

void node_json(std::ostream& out, const DecisionNode& value) {
  out << "{\"branch_counts\":"; array_json(out, value.branch_counts, integer<std::uint64_t>);
  out << ",\"split\":"; split_json(out, value.split);
  out << ",\"then_child\":" << value.then_child << ",\"else_child\":" << value.else_child << '}';
}

void point_json(std::ostream& out, const DecisionPoint& value) {
  out << "{\"place\":" << value.place << ",\"branches\":";
  array_json(out, value.branches, quoted);
  out << ",\"observations\":" << value.observations << ",\"nodes\":";
  array_json(out, value.nodes, node_json);
  out << ",\"accuracy\":"; number(out, value.accuracy);
  out << ",\"majority_share\":"; number(out, value.majority_share);
  out << '}';
}

void optional_feature_json(std::ostream& out, const std::optional<double>& value) {
  if (value) number(out, *value); else out << "null";
}

void row_json(std::ostream& out, const DecisionTableRow& value) {
  out << "{\"features\":"; array_json(out, value.features, optional_feature_json);
  out << ",\"target\":" << value.target << '}';
}

void table_json(std::ostream& out, const EncodedDecisionTable& value) {
  out << "{\"place\":" << value.place << ",\"classes\":";
  array_json(out, value.classes, quoted);
  out << ",\"feature_names\":"; array_json(out, value.feature_names, quoted);
  out << ",\"rows\":"; array_json(out, value.rows, row_json);
  out << '}';
}

void guard_json(std::ostream& out, const DataPetriNetTransition& value) {
  out << "{\"transition_index\":" << value.transition_index << ",\"guard\":";
  quoted(out, value.guard);
  out << ",\"read_variables\":"; array_json(out, value.read_variables, quoted);
  out << ",\"write_variables\":"; array_json(out, value.write_variables, quoted);
  out << '}';
}

}  // namespace

DecisionMiningOptions parse_decision_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  auto result = mining_value(document);
  set_supplied(document.object(), supplied);
  return result;
}

DecisionTableOptions parse_decision_table_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"mining", "include_trace_attributes", "lookback", "attributes"});
  DecisionTableOptions result;
  if (const auto* item = field(object, "mining")) result.mining = mining_value(*item);
  if (const auto* item = field(object, "include_trace_attributes")) result.include_trace_attributes = item->boolean();
  if (const auto* item = field(object, "lookback")) result.lookback = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "attributes")) result.attributes = vector_value<std::string>(*item, [](const Json& text) {return text.string();});
  set_supplied(object, supplied);
  return result;
}

DataPetriNetOptions parse_data_petri_workbench_options(
    std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"include_decision_diagnostics", "decision_options", "decision_table_lookback"});
  DataPetriNetOptions result;
  if (const auto* item = field(object, "include_decision_diagnostics")) result.include_decision_diagnostics = item->boolean();
  if (const auto* item = field(object, "decision_options")) result.decision_options = mining_value(*item);
  if (const auto* item = field(object, "decision_table_lookback")) result.decision_table_lookback = native_unsigned<std::size_t>(*item);
  set_supplied(object, supplied);
  return result;
}

std::string serialize_decision_workbench_options_json(const DecisionMiningOptions& value) {
  auto out = output();
  out << "{\"max_depth\":" << value.max_depth << ",\"min_split\":" << value.min_split;
  out << ",\"min_leaf\":" << value.min_leaf << ",\"max_categories\":" << value.max_categories;
  out << ",\"max_thresholds\":" << value.max_thresholds << ",\"min_gain\":"; number(out, value.min_gain);
  out << ",\"include_trace_attributes\":"; boolean(out, value.include_trace_attributes);
  out << ",\"attributes\":"; array_json(out, value.attributes, quoted);
  out << ",\"activity_attribute\":"; quoted(out, value.activity_attribute);
  out << '}';
  return out.str();
}

std::string serialize_decision_workbench_options_json(const DecisionTableOptions& value) {
  auto out = output();
  out << "{\"mining\":" << serialize_decision_workbench_options_json(value.mining);
  out << ",\"include_trace_attributes\":"; boolean(out, value.include_trace_attributes);
  out << ",\"lookback\":" << value.lookback << ",\"attributes\":";
  array_json(out, value.attributes, quoted);
  out << '}';
  return out.str();
}

std::string serialize_decision_workbench_options_json(const DataPetriNetOptions& value) {
  auto out = output();
  out << "{\"include_decision_diagnostics\":"; boolean(out, value.include_decision_diagnostics);
  out << ",\"decision_options\":" << serialize_decision_workbench_options_json(value.decision_options);
  out << ",\"decision_table_lookback\":" << value.decision_table_lookback << '}';
  return out.str();
}

std::string serialize_decision_workbench_result_json(const DecisionMiningResult& value) {
  auto out = output();
  out << "{\"miner\":"; quoted(out, value.miner);
  out << ",\"observed_traces\":" << value.observed_traces << ",\"replayed_traces\":" << value.replayed_traces;
  out << ",\"aligned_traces\":" << value.aligned_traces << ",\"alignment_failures\":" << value.alignment_failures;
  out << ",\"trace_count\":" << value.trace_count << ",\"decision_points\":";
  array_json(out, value.decision_points, point_json);
  out << ",\"activity_attribute\":"; quoted(out, value.activity_attribute);
  out << '}';
  return out.str();
}

std::string serialize_decision_workbench_result_json(const DecisionTablesResult& value) {
  auto out = output();
  out << "{\"observed_traces\":" << value.observed_traces << ",\"replayed_traces\":" << value.replayed_traces;
  out << ",\"aligned_traces\":" << value.aligned_traces << ",\"alignment_failures\":" << value.alignment_failures;
  out << ",\"trace_count\":" << value.trace_count << ",\"tables\":";
  array_json(out, value.tables, table_json);
  out << ",\"activity_attribute\":"; quoted(out, value.activity_attribute);
  out << '}';
  return out.str();
}

std::string serialize_decision_workbench_result_json(const DataPetriNet& value) {
  auto out = output();
  out << "{\"variant\":"; quoted(out, value.variant);
  out << ",\"net\":" << workflow_input_petri_json(value.net);
  out << ",\"decisions\":" << serialize_decision_workbench_result_json(value.decisions);
  out << ",\"decision_tables\":" << serialize_decision_workbench_result_json(value.decision_tables);
  out << ",\"guarded_transitions\":"; array_json(out, value.guarded_transitions, guard_json);
  out << '}';
  return out.str();
}

}  // namespace minera
