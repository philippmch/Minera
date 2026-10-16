#include "minera/dfg_playout_input_json.hpp"
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
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("DFG playout input JSON: " + message);
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


const Json& required(const Json::Object& object, std::string_view name) {
  if (const auto* value = field(object, name)) return *value;
  invalid("missing field " + std::string(name));
}

DfgPerformanceSummary summary_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"count", "mean", "median", "minimum", "maximum",
                          "standard_deviation", "sum"});
  DfgPerformanceSummary result;
  if (const auto* item = field(object, "count")) result.count = unsigned_integer(*item);
  if (const auto* item = field(object, "mean")) result.mean = ieee_double(*item);
  if (const auto* item = field(object, "median")) result.median = ieee_double(*item);
  if (const auto* item = field(object, "minimum")) result.minimum = ieee_double(*item);
  if (const auto* item = field(object, "maximum")) result.maximum = ieee_double(*item);
  if (const auto* item = field(object, "standard_deviation")) result.standard_deviation = ieee_double(*item);
  if (const auto* item = field(object, "sum")) result.sum = ieee_double(*item);
  return result;
}

std::vector<double> doubles(const Json& value) {
  std::vector<double> result;
  result.reserve(value.array().size());
  for (const auto& item : value.array()) result.push_back(ieee_double(item));
  return result;
}

DfgPerformanceValue selected_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"index", "value"});
  const auto index = unsigned_integer(required(object, "index"));
  const auto& selected = required(object, "value");
  switch (index) {
    case 0: return ieee_double(selected);
    case 1: return doubles(selected);
    case 2: return summary_value(selected);
    default: invalid("performance value index must be 0, 1, or 2");
  }
}

DfgPerformanceStatistics statistics_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"summary", "raw_values", "value"});
  DfgPerformanceStatistics result;
  if (const auto* item = field(object, "summary")) result.summary = summary_value(*item);
  if (const auto* item = field(object, "raw_values")) result.raw_values = doubles(*item);
  if (const auto* item = field(object, "value")) result.value = selected_value(*item);
  return result;
}

DfgFrequency frequency_value(const Json& value) {
  DfgFrequency result;
  for (const auto& record : value.array()) {
    const auto& object = record.object();
    allowed_fields(object, {"source", "target", "count"});
    DfgActivityPair edge{required(object, "source").string(), required(object, "target").string()};
    const auto* count = field(object, "count");
    if (!result.emplace(std::move(edge), count ? unsigned_integer(*count) : 0).second)
      invalid("duplicate frequency edge");
  }
  return result;
}

DfgActivityCounts activities_value(const Json& value) {
  DfgActivityCounts result;
  for (const auto& record : value.array()) {
    const auto& object = record.object();
    allowed_fields(object, {"activity", "count"});
    const auto* count = field(object, "count");
    if (!result.emplace(required(object, "activity").string(), count ? unsigned_integer(*count) : 0).second)
      invalid("duplicate activity count label");
  }
  return result;
}

DfgPerformance performance_value(const Json& value) {
  DfgPerformance result;
  for (const auto& record : value.array()) {
    const auto& object = record.object();
    allowed_fields(object, {"source", "target", "statistics"});
    DfgActivityPair edge{required(object, "source").string(), required(object, "target").string()};
    const auto* statistics = field(object, "statistics");
    if (!result.emplace(std::move(edge), statistics ? statistics_value(*statistics) : DfgPerformanceStatistics{}).second)
      invalid("duplicate performance edge");
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

void summary_json(std::ostream& out, const DfgPerformanceSummary& value) {
  out << "{\"count\":" << value.count << ",\"mean\":"; number(out, value.mean);
  out << ",\"median\":"; number(out, value.median);
  out << ",\"minimum\":"; number(out, value.minimum);
  out << ",\"maximum\":"; number(out, value.maximum);
  out << ",\"standard_deviation\":"; number(out, value.standard_deviation);
  out << ",\"sum\":"; number(out, value.sum); out << '}';
}

void doubles_json(std::ostream& out, const std::vector<double>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    number(out, values[i]);
  }
  out << ']';
}

void statistics_json(std::ostream& out, const DfgPerformanceStatistics& value) {
  out << "{\"summary\":"; summary_json(out, value.summary);
  out << ",\"raw_values\":"; doubles_json(out, value.raw_values);
  out << ",\"value\":{\"index\":" << value.value.index() << ",\"value\":";
  if (const auto* scalar = std::get_if<double>(&value.value)) number(out, *scalar);
  else if (const auto* values = std::get_if<std::vector<double>>(&value.value)) doubles_json(out, *values);
  else summary_json(out, std::get<DfgPerformanceSummary>(value.value));
  out << "}}";
}

template <class Map, class Write>
void map_json(std::ostream& out, const Map& values, Write write) {
  out << '[';
  bool comma = false;
  for (const auto& record : values) {
    if (comma) out << ',';
    comma = true;
    write(record.first, record.second);
  }
  out << ']';
}

void activity_json(std::ostream& out, const DfgActivityCounts& values) {
  map_json(out, values, [&](const std::string& label, std::uint64_t count) {
    out << "{\"activity\":"; quoted(out, label); out << ",\"count\":" << count << '}';
  });
}

void edge_json(std::ostream& out, const DfgActivityPair& edge) {
  out << "{\"source\":"; quoted(out, edge.first);
  out << ",\"target\":"; quoted(out, edge.second);
}

}  // namespace

DfgPlayoutInput parse_dfg_playout_input_json(std::string_view input) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"frequency", "start_activities", "end_activities", "performance"});
  DfgPlayoutInput result;
  if (const auto* item = field(object, "frequency")) result.frequency = frequency_value(*item);
  if (const auto* item = field(object, "start_activities")) result.start_activities = activities_value(*item);
  if (const auto* item = field(object, "end_activities")) result.end_activities = activities_value(*item);
  if (const auto* item = field(object, "performance"))
    result.performance = item->null() ? std::nullopt : std::optional<DfgPerformance>{performance_value(*item)};
  return result;
}

std::string serialize_dfg_playout_input_json(const DfgPlayoutInput& input) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "{\"frequency\":";
  map_json(out, input.frequency, [&](const DfgActivityPair& edge, std::uint64_t count) {
    edge_json(out, edge); out << ",\"count\":" << count << '}';
  });
  out << ",\"start_activities\":"; activity_json(out, input.start_activities);
  out << ",\"end_activities\":"; activity_json(out, input.end_activities);
  out << ",\"performance\":";
  if (input.performance) {
    map_json(out, *input.performance, [&](const DfgActivityPair& edge, const DfgPerformanceStatistics& statistics) {
      edge_json(out, edge); out << ",\"statistics\":"; statistics_json(out, statistics); out << '}';
    });
  } else out << "null";
  out << '}';
  return out.str();
}

}  // namespace minera
