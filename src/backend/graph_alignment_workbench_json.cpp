#include "minera/graph_alignment_workbench_json.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace minera {
namespace {
constexpr std::size_t max_input_bytes = 32u * 1024u * 1024u;
constexpr std::size_t max_input_values = 1000000;

struct Utf8Unit { std::uint32_t code; std::size_t bytes; };

// Both the input parser and output writer check Unicode encoding. Only the
// writer accepts WTF-8 surrogate code points, which it emits as JSON escapes.
Utf8Unit utf8_unit(std::string_view text, std::size_t at, bool allow_surrogate) {
  const auto first = static_cast<unsigned char>(text[at]);
  if (first < 0x80) return {first, 1};
  std::size_t bytes;
  std::uint32_t code;
  std::uint32_t minimum;
  if (first >= 0xc2 && first <= 0xdf) {
    bytes = 2; code = first & 0x1f; minimum = 0x80;
  } else if (first >= 0xe0 && first <= 0xef) {
    bytes = 3; code = first & 0x0f; minimum = 0x800;
  } else if (first >= 0xf0 && first <= 0xf4) {
    bytes = 4; code = first & 0x07; minimum = 0x10000;
  } else throw std::invalid_argument("graph alignment JSON: invalid UTF-8");
  if (text.size() - at < bytes)
    throw std::invalid_argument("graph alignment JSON: truncated UTF-8");
  for (std::size_t i = 1; i < bytes; ++i) {
    const auto next = static_cast<unsigned char>(text[at + i]);
    if ((next & 0xc0) != 0x80)
      throw std::invalid_argument("graph alignment JSON: invalid UTF-8 continuation");
    code = (code << 6) | (next & 0x3f);
  }
  if (code < minimum || code > 0x10ffff ||
      (!allow_surrogate && code >= 0xd800 && code <= 0xdfff))
    throw std::invalid_argument("graph alignment JSON: invalid UTF-8 code point");
  return {code, bytes};
}

void append_utf8(std::string& text, std::uint32_t code) {
  if (code < 0x80) text.push_back(static_cast<char>(code));
  else if (code < 0x800) {
    text.push_back(static_cast<char>(0xc0 | (code >> 6)));
    text.push_back(static_cast<char>(0x80 | (code & 0x3f)));
  } else if (code < 0x10000) {
    text.push_back(static_cast<char>(0xe0 | (code >> 12)));
    text.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
    text.push_back(static_cast<char>(0x80 | (code & 0x3f)));
  } else {
    text.push_back(static_cast<char>(0xf0 | (code >> 18)));
    text.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
    text.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
    text.push_back(static_cast<char>(0x80 | (code & 0x3f)));
  }
}

int hex_digit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

class TraceWordsParser {
 public:
  explicit TraceWordsParser(std::string_view text) : text_(text) {
    if (text.size() > max_input_bytes)
      invalid("request exceeds 32 MiB");
  }

  std::vector<std::string> parse() {
    skip_space();
    expect('[', "expected an array of strings");
    skip_space();
    std::vector<std::string> result;
    if (!take(']')) {
      for (;;) {
        if (++values_ > max_input_values)
          invalid("request exceeds one million values");
        result.push_back(string());
        skip_space();
        if (take(']')) break;
        expect(',', "expected ',' or ']'");
        skip_space();
      }
    }
    skip_space();
    if (at_ != text_.size()) invalid("unexpected trailing input");
    return result;
  }

 private:
  [[noreturn]] void invalid(const char* reason) const {
    throw std::invalid_argument(std::string("alignment trace JSON: ") + reason +
                                " at byte " + std::to_string(at_));
  }
  void skip_space() {
    while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' ||
                                text_[at_] == '\n' || text_[at_] == '\r')) ++at_;
  }
  bool take(char character) {
    if (at_ < text_.size() && text_[at_] == character) { ++at_; return true; }
    return false;
  }
  void expect(char character, const char* reason) {
    if (!take(character)) invalid(reason);
  }
  std::uint32_t hex4(std::size_t at) const {
    if (text_.size() - at < 4) invalid("truncated Unicode escape");
    std::uint32_t code = 0;
    for (std::size_t i = 0; i < 4; ++i) {
      const auto digit = hex_digit(text_[at + i]);
      if (digit < 0) invalid("invalid Unicode escape");
      code = (code << 4) | static_cast<std::uint32_t>(digit);
    }
    return code;
  }
  std::string string() {
    expect('"', "array members must be strings");
    std::string result;
    while (at_ < text_.size()) {
      const auto character = static_cast<unsigned char>(text_[at_]);
      if (character == '"') { ++at_; return result; }
      if (character < 0x20) invalid("unescaped control character");
      if (character >= 0x80) {
        const auto unit = utf8_unit(text_, at_, false);
        result.append(text_.substr(at_, unit.bytes));
        at_ += unit.bytes;
        continue;
      }
      ++at_;
      if (character != '\\') { result.push_back(static_cast<char>(character)); continue; }
      if (at_ == text_.size()) invalid("truncated escape");
      switch (text_[at_++]) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
          auto code = hex4(at_);
          at_ += 4;
          if (code >= 0xd800 && code <= 0xdbff && text_.size() - at_ >= 6 &&
              text_[at_] == '\\' && text_[at_ + 1] == 'u') {
            const auto low = hex4(at_ + 2);
            if (low >= 0xdc00 && low <= 0xdfff) {
              code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
              at_ += 6;
            }
          }
          append_utf8(result, code);
          break;
        }
        default: invalid("invalid escape");
      }
    }
    invalid("unterminated string");
  }
  std::string_view text_;
  std::size_t at_ = 0;
  std::size_t values_ = 1;
};

void unicode_escape(std::ostream& out, std::uint32_t code) {
  static constexpr char digits[] = "0123456789abcdef";
  out << "\\u";
  for (int shift = 12; shift >= 0; shift -= 4) out << digits[(code >> shift) & 0xf];
}

void quote(std::ostream& out, std::string_view text) {
  out << '"';
  for (std::size_t at = 0; at < text.size();) {
    const auto unit = utf8_unit(text, at, true);
    if (unit.code >= 0xd800 && unit.code <= 0xdfff) unicode_escape(out, unit.code);
    else if (unit.bytes > 1) out.write(text.data() + at, static_cast<std::streamsize>(unit.bytes));
    else switch (unit.code) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (unit.code < 0x20) unicode_escape(out, unit.code);
        else out << static_cast<char>(unit.code);
    }
    at += unit.bytes;
  }
  out << '"';
}

void floating(std::ostream& out, double value) {
  if (std::isnan(value)) { out << "\"NaN\""; return; }
  if (std::isinf(value)) { out << (value < 0 ? "\"-INF\"" : "\"INF\""); return; }
  std::ostringstream number;
  number.imbue(std::locale::classic());
  number << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  auto text = number.str();
  if (text.find_first_of(".eE") == std::string::npos) text += ".0";
  out << text;
}

const char* boolean(bool value) { return value ? "true" : "false"; }

void words_json(std::ostream& out, const std::vector<std::string>& words) {
  out << '[';
  for (std::size_t i = 0; i < words.size(); ++i) {
    if (i) out << ',';
    quote(out, words[i]);
  }
  out << ']';
}

void dfg_trace_json(std::ostream& out, const DfgAlignmentResult& result) {
  out << "{\"computed\":" << boolean(result.computed) << ",\"alignment\":[";
  for (std::size_t i = 0; i < result.alignment.size(); ++i) {
    if (i) out << ',';
    out << "{\"log\":"; quote(out, result.alignment[i].log);
    out << ",\"model\":"; quote(out, result.alignment[i].model); out << '}';
  }
  out << "],\"cost\":"; floating(out, result.cost);
  out << ",\"internal_cost\":"; floating(out, result.internal_cost);
  out << ",\"fitness\":"; floating(out, result.fitness);
  out << ",\"best_worst_cost\":"; floating(out, result.best_worst_cost);
  out << ",\"visited_states\":" << result.visited_states << ",\"closed\":" << result.closed
      << ",\"closed_states\":" << result.closed_states << '}';
}

void projection_json(std::ostream& out, const DfgAlignmentProjection& projection) {
  out << "{\"dfg\":[";
  bool first = true;
  for (const auto& [path, counts] : projection.dfg) {
    if (!first) out << ',';
    first = false;
    out << "{\"source\":"; quote(out, path.first);
    out << ",\"target\":"; quote(out, path.second);
    out << ",\"sync\":" << counts.sync << ",\"mm\":" << counts.mm << '}';
  }
  out << "],\"activities\":[";
  first = true;
  for (const auto& [name, counts] : projection.activities) {
    if (!first) out << ',';
    first = false;
    out << "{\"activity\":"; quote(out, name);
    out << ",\"sync\":" << counts.sync << ",\"mm\":" << counts.mm
        << ",\"lm\":" << counts.lm << '}';
  }
  out << "]}";
}

void decomposed_trace_json(std::ostream& out, const DecomposedAlignmentTrace& trace) {
  out << "{\"activities\":"; words_json(out, trace.activities);
  out << ",\"count\":" << trace.count << ",\"complete\":" << boolean(trace.complete)
      << ",\"timed_out\":" << boolean(trace.timed_out)
      << ",\"exhausted\":" << boolean(trace.exhausted) << ",\"cost\":";
  floating(out, trace.cost);
  out << ",\"fitness\":"; floating(out, trace.fitness);
  out << ",\"best_worst_cost\":"; floating(out, trace.best_worst_cost);
  out << ",\"alignment\":[";
  for (std::size_t i = 0; i < trace.alignment.size(); ++i) {
    if (i) out << ',';
    const auto& move = trace.alignment[i];
    out << "{\"log\":"; if (move.log) quote(out, *move.log); else out << "null";
    out << ",\"model\":"; if (move.model) quote(out, *move.model); else out << "null";
    out << ",\"transition\":" << move.transition << '}';
  }
  out << "]}";
}

void diagnostics_json(std::ostream& out, const DecomposedAlignmentDiagnostics& value) {
  out << "{\"decompositions\":" << value.decompositions << ",\"components\":" << value.components
      << ",\"merges\":" << value.merges << ",\"cache_hits\":" << value.cache_hits
      << ",\"cache_misses\":" << value.cache_misses << ",\"border_disagreements\":" << value.border_disagreements
      << ",\"agreement_iterations\":" << value.agreement_iterations << ",\"visited_states\":" << value.visited_states
      << ",\"queued_states\":" << value.queued_states << ",\"traversed_arcs\":" << value.traversed_arcs
      << ",\"whole_net_fallback\":" << boolean(value.whole_net_fallback) << ",\"note\":";
  quote(out, value.note); out << '}';
}

template <class Map>
void cost_map_json(std::ostream& out, const Map& costs) {
  // Unordered native maps have no public iteration-order contract. Use the
  // same stable string-key order for both native map types without mutation.
  const std::map<std::string, double> ordered(costs.begin(), costs.end());
  out << '{';
  bool first = true;
  for (const auto& [name, cost] : ordered) {
    if (!first) out << ',';
    first = false;
    quote(out, name); out << ':'; floating(out, cost);
  }
  out << '}';
}

void marking_json(std::ostream& out, const std::vector<std::size_t>& marking) {
  out << '[';
  for (std::size_t i = 0; i < marking.size(); ++i) {
    if (i) out << ',';
    out << marking[i];
  }
  out << ']';
}

template <class Write>
std::string json(Write write) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  write(out);
  return out.str();
}
}  // namespace

std::vector<std::string> parse_alignment_trace_words_json(std::string_view input) {
  return TraceWordsParser(input).parse();
}

std::string serialize_alignment_trace_words_json(const std::vector<std::string>& words) {
  return json([&](std::ostream& out) { words_json(out, words); });
}

std::string serialize_dfg_alignment_trace_json(const DfgAlignmentResult& result) {
  return json([&](std::ostream& out) { dfg_trace_json(out, result); });
}

std::string serialize_dfg_alignment_projection_json(const DfgAlignmentProjection& projection) {
  return json([&](std::ostream& out) { projection_json(out, projection); });
}

std::string serialize_dfg_alignment_log_json(const DfgAlignmentLogResult& result,
                                             const DfgAlignmentProjection* projection) {
  return json([&](std::ostream& out) {
    out << "{\"alignments\":[";
    for (std::size_t i = 0; i < result.alignments.size(); ++i) {
      if (i) out << ',';
      dfg_trace_json(out, result.alignments[i]);
    }
    out << "],\"unique_variants\":" << result.unique_variants << ",\"projection\":";
    if (projection) projection_json(out, *projection); else out << "null";
    out << '}';
  });
}

std::string serialize_decomposed_alignment_trace_json(const DecomposedAlignmentTrace& trace) {
  return json([&](std::ostream& out) { decomposed_trace_json(out, trace); });
}

std::string serialize_decomposed_alignment_result_json(const DecomposedAlignmentResult& result) {
  return json([&](std::ostream& out) {
    out << "{\"complete\":" << boolean(result.complete) << ",\"timed_out\":" << boolean(result.timed_out)
        << ",\"exhausted\":" << boolean(result.exhausted) << ",\"fitness\":";
    floating(out, result.fitness);
    out << ",\"best_worst_cost\":"; floating(out, result.best_worst_cost);
    out << ",\"trace_count\":" << result.trace_count << ",\"total_cost\":"; floating(out, result.total_cost);
    out << ",\"diagnostics\":"; diagnostics_json(out, result.diagnostics);
    out << ",\"traces\":[";
    for (std::size_t i = 0; i < result.traces.size(); ++i) {
      if (i) out << ',';
      decomposed_trace_json(out, result.traces[i]);
    }
    out << "]}";
  });
}

std::string serialize_dfg_alignment_options_json(const DfgAlignmentOptions& options) {
  return json([&](std::ostream& out) {
    out << "{\"variant\":";
    quote(out, options.variant == DfgAlignmentVariant::kClassic ? "CLASSIC" : "UNKNOWN");
    out << ",\"variant_index\":" << static_cast<std::underlying_type_t<DfgAlignmentVariant>>(options.variant);
    out << ",\"sync_cost\":"; cost_map_json(out, options.sync_cost);
    out << ",\"model_move_cost\":"; cost_map_json(out, options.model_move_cost);
    out << ",\"log_move_cost\":"; cost_map_json(out, options.log_move_cost);
    out << ",\"internal_log_move_cost\":"; cost_map_json(out, options.internal_log_move_cost);
    out << ",\"max_visited_states\":" << options.max_visited_states << ",\"max_time_ms\":" << options.max_time_ms;
    out << ",\"activity_attribute\":"; quote(out, options.activity_attribute);
    out << ",\"timestamp_attribute\":"; quote(out, options.timestamp_attribute);
    out << ",\"sync_cost_supplied\":" << boolean(options.sync_cost_supplied)
        << ",\"model_move_cost_supplied\":" << boolean(options.model_move_cost_supplied)
        << ",\"log_move_cost_supplied\":" << boolean(options.log_move_cost_supplied)
        << ",\"internal_log_move_cost_supplied\":" << boolean(options.internal_log_move_cost_supplied) << '}';
  });
}

std::string serialize_decomposed_alignment_options_json(const DecomposedAlignmentOptions& options) {
  return json([&](std::ostream& out) {
    out << "{\"variant\":";
    quote(out, options.variant == DecomposedAlignmentVariant::kRecomposMaximal ? "RECOMPOS_MAXIMAL" : "UNKNOWN");
    out << ",\"variant_index\":" << static_cast<std::underlying_type_t<DecomposedAlignmentVariant>>(options.variant);
    out << ",\"threshold_border_agreement\":"; floating(out, options.threshold_border_agreement);
    out << ",\"max_time_trace\":"; floating(out, options.max_time_trace);
    out << ",\"max_time_total\":"; floating(out, options.max_time_total);
    out << ",\"max_states\":" << options.max_states << ",\"max_total_states\":" << options.max_total_states
        << ",\"use_cache\":" << boolean(options.use_cache) << ",\"clear_cache\":" << boolean(options.clear_cache);
    out << ",\"log_move_cost\":"; floating(out, options.log_move_cost);
    out << ",\"model_move_cost\":"; floating(out, options.model_move_cost);
    out << ",\"tau_model_cost\":"; floating(out, options.tau_model_cost);
    out << ",\"sync_cost\":"; floating(out, options.sync_cost);
    out << ",\"activity_log_costs\":"; cost_map_json(out, options.activity_log_costs);
    out << ",\"activity_model_costs\":"; cost_map_json(out, options.activity_model_costs);
    out << ",\"activity_sync_costs\":"; cost_map_json(out, options.activity_sync_costs);
    out << ",\"initial_marking\":"; marking_json(out, options.initial_marking);
    out << ",\"final_marking\":"; marking_json(out, options.final_marking);
    out << ",\"activity_attribute\":"; quote(out, options.activity_attribute); out << '}';
  });
}

}  // namespace minera
