#include "minera/genetic_matrix_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
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
#include <cmath>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Genetic matrix workbench JSON: " + message);
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


void supplied_members(const Json::Object& object, std::set<std::string>* supplied) {
  if (!supplied) return;
  std::set<std::string> result;
  for (const auto& [key, unused] : object) { (void)unused; result.insert(key); }
  *supplied = std::move(result);
}

std::int32_t signed_index(const Json& value) {
  const auto* token = std::get_if<Number>(&value.value);
  if (!token || token->text.empty()) invalid("expected a signed int32 JSON integer token");
  const auto& text = token->text;
  const auto start = text.front() == '-' ? std::size_t{1} : std::size_t{0};
  if (start == text.size() || !std::all_of(text.begin() + start, text.end(),
      [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected a signed int32 JSON integer token");
  std::int32_t result = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    invalid("variant index exceeds int32 storage");
  return result;
}

GeneticMatrixMap matrix_map(const Json& json) {
  return vector_value<std::vector<GeneticMatrixPartition>>(json, [](const Json& groups) {
    return vector_value<GeneticMatrixPartition>(groups, [](const Json& members) {
      return vector_value<std::size_t>(members, [](const Json& member) {
        return native_unsigned<std::size_t>(member);
      });
    });
  });
}

void matrix_json(std::ostream& out, const GeneticCausalMatrix& matrix) {
  out << "{\"activities\":";
  array_json(out, matrix.activities, quoted);
  const auto maps = [&](const GeneticMatrixMap& map) {
    array_json(out, map, [](std::ostream& outer, const std::vector<GeneticMatrixPartition>& groups) {
      array_json(outer, groups, [](std::ostream& inner, const GeneticMatrixPartition& members) {
        array_json(inner, members, [](std::ostream& target, std::size_t member) { target << member; });
      });
    });
  };
  out << ",\"input_map\":"; maps(matrix.input_map);
  out << ",\"output_map\":"; maps(matrix.output_map); out << '}';
}

std::string variant_json(GeneticMatrixConversionVariant variant) {
  const auto index = static_cast<std::int32_t>(variant);
  return "{\"name\":" + std::string(index == 0 ? "\"TO_PETRI_NET\"" : "null") +
      ",\"index\":" + std::to_string(index) + '}';
}
}  // namespace

GeneticCausalMatrix parse_genetic_matrix_workbench_model_json(
    std::string_view input, std::set<std::string>* supplied) {
  const auto json = Parser(input).parse();
  const auto& object = json.object();
  allowed_fields(object, {"activities", "input_map", "output_map"});
  GeneticCausalMatrix result;
  if (const auto* value = field(object, "activities"))
    result.activities = vector_value<std::string>(*value, [](const Json& v) { return v.string(); });
  if (const auto* value = field(object, "input_map")) result.input_map = matrix_map(*value);
  if (const auto* value = field(object, "output_map")) result.output_map = matrix_map(*value);
  supplied_members(object, supplied);
  return result;
}

GeneticMatrixWorkbenchOptions parse_genetic_matrix_workbench_options_json(
    std::string_view input, std::set<std::string>* supplied) {
  const auto json = Parser(input).parse();
  const auto& object = json.object();
  allowed_fields(object, {"variant", "variant_index"});
  const auto* name = field(object, "variant");
  const auto* index = field(object, "variant_index");
  if (name && index) invalid("variant and variant_index conflict by presence");
  GeneticMatrixWorkbenchOptions result;
  if (name) result.variant = parse_genetic_matrix_conversion_variant(name->string());
  if (index) result.variant = static_cast<GeneticMatrixConversionVariant>(signed_index(*index));
  supplied_members(object, supplied);
  return result;
}

std::string serialize_genetic_matrix_workbench_model_json(const GeneticCausalMatrix& matrix) {
  auto out = output(); matrix_json(out, matrix); return out.str();
}

std::string serialize_genetic_matrix_workbench_options_json(const GeneticMatrixWorkbenchOptions& options) {
  return "{\"variant\":" + variant_json(options.variant) + '}';
}

std::string serialize_genetic_matrix_workbench_net_json(const PetriNet& net) {
  // Existing serializer retains all transition arcs/weights and sparse marking
  // namespaces. Append the three getter carriers its renderer does not expose.
  auto result = workflow_input_petri_json(net);
  result.pop_back();
  result += ",\"source_storage\":" + std::to_string(net.source()) +
      ",\"sink_storage\":" + std::to_string(net.sink()) + ",\"transition_names\":";
  if (!net.transition_names()) result += "null";
  else {
    auto out = output();
    array_json(out, *net.transition_names(), [](std::ostream& target, const auto& name) {
      if (name) quoted(target, *name); else target << "null";
    });
    result += out.str();
  }
  return result + '}';
}

std::string serialize_genetic_matrix_workbench_markings_json(const WorkflowMarkings& markings) {
  auto out = output();
  const auto write = [](std::ostream& target, std::int64_t value) { target << value; };
  out << "{\"initial\":"; array_json(out, markings.initial, write);
  out << ",\"final\":"; array_json(out, markings.final, write); out << '}';
  return out.str();
}

std::string serialize_genetic_matrix_workbench_result_json(const GeneticMatrixConversionResult& result) {
  return "{\"variant\":" + variant_json(result.variant) + ",\"matrix\":" +
      serialize_genetic_matrix_workbench_model_json(result.matrix) + ",\"net\":" +
      serialize_genetic_matrix_workbench_net_json(result.net) + ",\"markings\":" +
      serialize_genetic_matrix_workbench_markings_json(result.markings) + '}';
}

}  // namespace minera
