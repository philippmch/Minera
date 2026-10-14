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
  throw std::invalid_argument("inductive input JSON: " + message);
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

std::vector<std::uint64_t> counts(const Json& value) {
  std::vector<std::uint64_t> result;
  for (const auto& count : value.array()) result.push_back(unsigned_integer(count));
  return result;
}

InductiveMinerVariantLog variants(const Json& value) {
  InductiveMinerVariantLog result;
  for (const auto& entry : value.array()) {
    const auto& object = entry.object(); fields(object, {"activities", "count"});
    result.push_back({words(object.at("activities")), unsigned_integer(object.at("count"))});
  }
  return result;
}

void quote(std::ostream& out, std::string_view value) {
  // Reuse the frozen unbounded UTF/WTF-8-safe writer, without parsing outputs
  // through its bounded input API or changing any prior serializer contract.
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

template <class Write>
std::string json(Write write) {
  std::ostringstream out; out.imbue(std::locale::classic()); write(out); return out.str();
}

template <class Variants>
void variants_json(std::ostream& out, const Variants& input) {
  out << '[';
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (i) out << ',';
    out << "{\"activities\":" << serialize_alignment_trace_words_json(input[i].activities)
        << ",\"count\":" << input[i].count << '}';
  }
  out << ']';
}

void counts_json(std::ostream& out, const std::vector<std::uint64_t>& input) {
  out << '[';
  for (std::size_t i = 0; i < input.size(); ++i) { if (i) out << ','; out << input[i]; }
  out << ']';
}

void constructor_fields_json(std::ostream& out, const InductiveDfgInput& input) {
  out << "\"activities\":" << serialize_alignment_trace_words_json(input.activities) << ",\"edges\":[";
  for (std::size_t i = 0; i < input.edges.size(); ++i) {
    if (i) out << ',';
    const auto& edge = input.edges[i];
    out << "{\"source\":" << edge.source << ",\"target\":" << edge.target << ",\"count\":" << edge.count << '}';
  }
  out << "],\"start_counts\":"; counts_json(out, input.start_counts);
  out << ",\"end_counts\":"; counts_json(out, input.end_counts);
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

void node_json(std::ostream& out, const ProcessTreeNode& root) {
  struct Frame { const ProcessTreeNode* node; std::size_t child = 0; bool opened = false; };
  std::vector<Frame> frames{{&root}};
  while (!frames.empty()) {
    auto& frame = frames.back();
    if (!frame.opened) {
      out << "{\"op\":"; quote(out, operator_name(frame.node->op));
      out << ",\"op_index\":" << static_cast<std::underlying_type_t<ProcessTreeOperator>>(frame.node->op)
          << ",\"activity\":" << frame.node->activity << ",\"children\":[";
      frame.opened = true;
    }
    if (frame.child < frame.node->children.size()) {
      if (frame.child) out << ',';
      const auto* child = &frame.node->children[frame.child++];
      frames.push_back({child});
    } else { out << "]}"; frames.pop_back(); }
  }
}
}  // namespace

InductiveMinerVariantLog parse_inductive_variant_log_json(std::string_view input) {
  return variants(Parser(input).parse());
}

std::string serialize_inductive_variant_log_json(const InductiveMinerVariantLog& input) {
  return json([&](std::ostream& out) { variants_json(out, input); });
}

InductiveDfgInput parse_inductive_dfg_input_json(std::string_view input) {
  const auto document = Parser(input).parse();
  const auto& object = document.object(); fields(object, {"activities", "edges", "start_counts", "end_counts"});
  InductiveDfgInput result;
  result.activities = words(object.at("activities"));
  for (const auto& value : object.at("edges").array()) {
    const auto& edge = value.object(); fields(edge, {"source", "target", "count"});
    result.edges.push_back({native_index(edge.at("source")), native_index(edge.at("target")), unsigned_integer(edge.at("count"))});
  }
  result.start_counts = counts(object.at("start_counts")); result.end_counts = counts(object.at("end_counts"));
  (void)DirectlyFollowsGraph::from_frequencies(result.activities, result.edges, result.start_counts, result.end_counts);
  return result;
}

std::string serialize_inductive_dfg_input_json(const InductiveDfgInput& input) {
  (void)DirectlyFollowsGraph::from_frequencies(input.activities, input.edges, input.start_counts, input.end_counts);
  return json([&](std::ostream& out) { out << '{'; constructor_fields_json(out, input); out << '}'; });
}

std::string serialize_inductive_native_dfg_json(const DirectlyFollowsGraph& graph) {
  InductiveDfgInput input; input.activities = graph.activities(); input.edges = graph.edges();
  std::vector<std::uint64_t> activity_counts, rework_counts;
  for (const auto& activity : graph.activities()) {
    input.start_counts.push_back(graph.start_activity_count(activity)); input.end_counts.push_back(graph.end_activity_count(activity));
    activity_counts.push_back(graph.activity_count(activity)); rework_counts.push_back(graph.rework_case_count(activity));
  }
  return json([&](std::ostream& out) {
    out << '{'; constructor_fields_json(out, input);
    out << ",\"activity_counts\":"; counts_json(out, activity_counts);
    out << ",\"rework_case_counts\":"; counts_json(out, rework_counts);
    out << ",\"variants\":"; variants_json(out, graph.variants()); out << '}';
  });
}

std::string serialize_inductive_process_tree_json(const ProcessTree& tree) {
  return json([&](std::ostream& out) {
    out << "{\"activities\":" << serialize_alignment_trace_words_json(tree.activities()) << ",\"root\":";
    node_json(out, tree.root()); out << '}';
  });
}

std::string serialize_inductive_options_json(const InductiveMinerOptions& options) {
  const char* name = "UNKNOWN";
  switch (options.variant) {
    case InductiveMinerVariant::kLog: name = "IM"; break;
    case InductiveMinerVariant::kImf: name = "IMF"; break;
    case InductiveMinerVariant::kDfg: name = "IMD"; break;
  }
  return json([&](std::ostream& out) {
    out << "{\"noise_threshold\":"; floating(out, options.noise_threshold);
    out << ",\"variant\":"; quote(out, name);
    out << ",\"variant_index\":" << static_cast<std::underlying_type_t<InductiveMinerVariant>>(options.variant);
    out << ",\"activity_key\":"; quote(out, options.activity_key);
    out << ",\"timestamp_key\":"; quote(out, options.timestamp_key);
    out << ",\"case_id_key\":"; quote(out, options.case_id_key);
    out << ",\"sort_events_by_timestamp\":" << boolean(options.sort_events_by_timestamp)
        << ",\"disable_strict_sequence_cut\":" << boolean(options.disable_strict_sequence_cut)
        << ",\"multiprocessing\":" << boolean(options.multiprocessing)
        << ",\"disable_fallthroughs\":" << boolean(options.disable_fallthroughs) << '}';
  });
}

}  // namespace minera
