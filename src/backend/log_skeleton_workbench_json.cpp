#include "minera/log_skeleton_workbench_json.hpp"
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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Log skeleton workbench JSON: " + message);
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

const Json& required(const Json::Object& object, std::string_view name) {
  if (const auto* value = field(object, name)) return *value;
  invalid("missing field " + std::string(name));
}

void set_supplied(const Json::Object& object, std::set<std::string>* supplied) {
  if (!supplied) return;
  std::set<std::string> actual;
  for (const auto& [key, value] : object) { (void)value; actual.insert(key); }
  *supplied = std::move(actual);
}

LogSkeletonPair pair_value(const Json& value) {
  const auto& object = value.object(); allowed_fields(object, {"source", "target"});
  return {required(object, "source").string(), required(object, "target").string()};
}
void pair_json(std::ostream& out, const LogSkeletonPair& value) {
  out << "{\"source\":"; quoted(out, value.source); out << ",\"target\":"; quoted(out, value.target); out << '}';
}
std::vector<std::string> strings(const Json& value) {
  return vector_value<std::string>(value, [](const Json& item) {return item.string();});
}
std::vector<std::size_t> counts(const Json& value) {
  return vector_value<std::size_t>(value, [](const Json& item) {return native_unsigned<std::size_t>(item);});
}
LogSkeletonDeviation deviation_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"constraint", "relation", "activity", "observed_frequency", "allowed_frequencies"});
  LogSkeletonDeviation result;
  result.constraint = required(object, "constraint").string();
  result.relation = pair_value(required(object, "relation"));
  result.activity = required(object, "activity").string();
  result.observed_frequency = native_unsigned<std::size_t>(required(object, "observed_frequency"));
  result.allowed_frequencies = counts(required(object, "allowed_frequencies"));
  return result;
}
void deviation_json(std::ostream& out, const LogSkeletonDeviation& value) {
  out << "{\"constraint\":"; quoted(out, value.constraint);
  out << ",\"relation\":"; pair_json(out, value.relation);
  out << ",\"activity\":"; quoted(out, value.activity);
  out << ",\"observed_frequency\":" << value.observed_frequency << ",\"allowed_frequencies\":";
  array_json(out, value.allowed_frequencies, integer<std::size_t>); out << '}';
}
LogSkeletonConformance conformance_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"deviations", "no_dev_total", "no_constr_total", "dev_fitness", "is_fit"});
  LogSkeletonConformance result;
  result.deviations = vector_value<LogSkeletonDeviation>(required(object, "deviations"), deviation_value);
  result.no_dev_total = native_unsigned<std::size_t>(required(object, "no_dev_total"));
  result.no_constr_total = native_unsigned<std::size_t>(required(object, "no_constr_total"));
  result.dev_fitness = ieee_double(required(object, "dev_fitness"));
  result.is_fit = required(object, "is_fit").boolean();
  return result;
}
void conformance_json(std::ostream& out, const LogSkeletonConformance& result) {
  out << "{\"deviations\":"; array_json(out, result.deviations, deviation_json);
  out << ",\"no_dev_total\":" << result.no_dev_total << ",\"no_constr_total\":" << result.no_constr_total;
  out << ",\"dev_fitness\":"; number(out, result.dev_fitness);
  out << ",\"is_fit\":"; boolean(out, result.is_fit); out << '}';
}
LogSkeletonVariant variant_value(const Json& value) {
  const auto& object = value.object(); allowed_fields(object, {"activities", "count"});
  return {strings(required(object, "activities")), native_unsigned<std::size_t>(required(object, "count"))};
}
void variant_json(std::ostream& out, const LogSkeletonVariant& value) {
  out << "{\"activities\":"; array_json(out, value.first, quoted); out << ",\"count\":" << value.second << '}';
}
}  // namespace

LogSkeletonModel parse_log_skeleton_workbench_model(std::string_view input) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"equivalence", "always_after", "always_before", "never_together", "directly_follows", "activ_freq"});
  LogSkeletonModel result;
  if (const auto* item = field(object, "equivalence")) result.equivalence = vector_value<LogSkeletonPair>(*item, pair_value);
  if (const auto* item = field(object, "always_after")) result.always_after = vector_value<LogSkeletonPair>(*item, pair_value);
  if (const auto* item = field(object, "always_before")) result.always_before = vector_value<LogSkeletonPair>(*item, pair_value);
  if (const auto* item = field(object, "never_together")) result.never_together = vector_value<LogSkeletonPair>(*item, pair_value);
  if (const auto* item = field(object, "directly_follows")) result.directly_follows = vector_value<LogSkeletonPair>(*item, pair_value);
  if (const auto* item = field(object, "activ_freq"))
    for (const auto& [activity, values] : item->object()) result.activ_freq.emplace(activity, counts(values));
  return result;
}
std::string serialize_log_skeleton_workbench_model_json(const LogSkeletonModel& model) {
  auto out = output(); out << "{\"equivalence\":"; array_json(out, model.equivalence, pair_json);
  out << ",\"always_after\":"; array_json(out, model.always_after, pair_json);
  out << ",\"always_before\":"; array_json(out, model.always_before, pair_json);
  out << ",\"never_together\":"; array_json(out, model.never_together, pair_json);
  out << ",\"directly_follows\":"; array_json(out, model.directly_follows, pair_json);
  out << ",\"activ_freq\":{"; bool comma = false;
  for (const auto& [activity, values] : model.activ_freq) {
    if (comma) out << ','; comma = true;
    quoted(out, activity); out << ':'; array_json(out, values, integer<std::size_t>);
  }
  out << "}}"; return out.str();
}
LogSkeletonOptions parse_log_skeleton_conformance_workbench_options(std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"noise_threshold", "considered_constraints", "activity_key", "considered_constraints_present", "case_id_key"});
  LogSkeletonOptions result;
  if (const auto* item = field(object, "noise_threshold")) result.noise_threshold = ieee_double(*item);
  if (const auto* item = field(object, "considered_constraints")) result.considered_constraints = strings(*item);
  if (const auto* item = field(object, "activity_key")) result.activity_key = item->string();
  if (const auto* item = field(object, "considered_constraints_present")) result.considered_constraints_present = item->boolean();
  if (const auto* item = field(object, "case_id_key")) result.case_id_key = item->string();
  set_supplied(object, supplied); return result;
}
LogSkeletonDiagnosticsOptions parse_log_skeleton_diagnostics_workbench_options(std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"case_id_key"}); LogSkeletonDiagnosticsOptions result;
  if (const auto* item = field(object, "case_id_key")) result.case_id_key = item->string();
  set_supplied(object, supplied); return result;
}
std::string serialize_log_skeleton_workbench_options_json(const LogSkeletonOptions& value) {
  auto out = output(); out << "{\"noise_threshold\":"; number(out, value.noise_threshold);
  out << ",\"considered_constraints\":"; array_json(out, value.considered_constraints, quoted);
  out << ",\"activity_key\":"; quoted(out, value.activity_key);
  out << ",\"considered_constraints_present\":"; boolean(out, value.considered_constraints_present);
  out << ",\"case_id_key\":"; quoted(out, value.case_id_key); out << '}'; return out.str();
}
std::string serialize_log_skeleton_workbench_options_json(const LogSkeletonDiagnosticsOptions& value) {
  auto out = output(); out << "{\"case_id_key\":"; quoted(out, value.case_id_key); out << '}'; return out.str();
}
LogSkeletonVariantList parse_log_skeleton_workbench_variants_json(std::string_view input) {
  return vector_value<LogSkeletonVariant>(Parser(input).parse(), variant_value);
}
std::string serialize_log_skeleton_workbench_variants_json(const LogSkeletonVariantList& values) {
  auto out = output(); array_json(out, values, variant_json); return out.str();
}
std::vector<LogSkeletonConformance> parse_log_skeleton_workbench_conformances_json(std::string_view input) {
  return vector_value<LogSkeletonConformance>(Parser(input).parse(), conformance_value);
}
std::string serialize_log_skeleton_workbench_conformance_json(const LogSkeletonConformance& value) {
  auto out = output(); conformance_json(out, value); return out.str();
}
std::string serialize_log_skeleton_workbench_conformance_json(const std::vector<LogSkeletonConformance>& values) {
  auto out = output(); array_json(out, values, conformance_json); return out.str();
}
std::string serialize_log_skeleton_workbench_conformance_json(const LogSkeletonVariantConformance& values) {
  auto out = output(); out << '['; bool comma = false;
  for (const auto& [activities, value] : values) {
    if (comma) out << ','; comma = true;
    out << "{\"activities\":"; array_json(out, activities, quoted); out << ",\"value\":"; conformance_json(out, value); out << '}';
  }
  out << ']'; return out.str();
}
}  // namespace minera
