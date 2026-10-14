#include "minera/model_conversion_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/workflow_input_result_json.hpp"

#include <charconv>
#include <cstdint>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace minera {
namespace {

constexpr std::size_t kMaxInputBytes = 32u * 1024u * 1024u;
constexpr std::size_t kMaxInputValues = 1000000;

[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("model conversion workbench: " + message);
}

bool whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view trimmed(std::string_view value) {
  while (!value.empty() && whitespace(value.front())) value.remove_prefix(1);
  while (!value.empty() && whitespace(value.back())) value.remove_suffix(1);
  return value;
}

std::int64_t signed_integer(std::string_view token) {
  if (token.empty()) invalid("expected an exact signed int64 integer");
  std::int64_t result = 0;
  const auto parsed = std::from_chars(token.data(), token.data() + token.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
    invalid("expected an exact signed int64 integer");
  return result;
}

void check_bytes(std::string_view input) {
  if (input.size() > kMaxInputBytes) invalid("marking input exceeds 32 MiB");
}

void append_marking(Marking& marking, std::int64_t value) {
  if (marking.size() >= kMaxInputValues - 1)
    invalid("marking input exceeds one million values");
  marking.push_back(value);
}

// A marking accepts only a JSON array and signed integer tokens. This schema
// rejects nested containers and strings directly, without a generic JSON tree
// or a floating-point intermediate allocation.
Marking json_marking(std::string_view input) {
  check_bytes(input);
  std::size_t position = 0;
  const auto space = [&] {
    while (position < input.size() && whitespace(input[position])) ++position;
  };
  const auto take = [&](char c) {
    space();
    if (position < input.size() && input[position] == c) { ++position; return true; }
    return false;
  };
  if (!take('[')) invalid("expected a JSON marking array");
  Marking result;
  if (!take(']')) {
    for (;;) {
      space();
      const auto start = position;
      if (position < input.size() && input[position] == '-') ++position;
      if (position == input.size() || input[position] < '0' || input[position] > '9')
        invalid("expected a signed JSON integer token");
      if (input[position] == '0') ++position;
      else while (position < input.size() && input[position] >= '0' && input[position] <= '9') ++position;
      append_marking(result, signed_integer(input.substr(start, position - start)));
      if (take(']')) break;
      if (!take(',')) invalid("expected a comma or end of marking array");
    }
  }
  space();
  if (position != input.size()) invalid("trailing marking data");
  return result;
}

Marking legacy_marking(std::string_view input) {
  check_bytes(input);
  Marking result;
  std::size_t start = 0;
  for (;;) {
    const auto end = input.find(',', start);
    const auto token = trimmed(input.substr(start, end == std::string_view::npos ? end : end - start));
    append_marking(result, signed_integer(token));
    if (end == std::string_view::npos) return result;
    start = end + 1;
  }
}

const char* boolean(bool value) { return value ? "true" : "false"; }

void quoted(std::ostream& out, const std::string& value) {
  const auto array = serialize_alignment_trace_words_json({value});
  out.write(array.data() + 1, static_cast<std::streamsize>(array.size() - 2));
}

const char* kind_name(PowlNodeKind kind) {
  switch (kind) {
    case PowlNodeKind::kTransition: return "TRANSITION";
    case PowlNodeKind::kSilent: return "SILENT";
    case PowlNodeKind::kXor: return "XOR";
    case PowlNodeKind::kLoop: return "LOOP";
    case PowlNodeKind::kStrictPartialOrder: return "STRICT_PARTIAL_ORDER";
  }
  return "UNKNOWN";
}

void powl_json(std::ostream& out, const PowlNode& node) {
  out << "{\"kind\":";
  quoted(out, kind_name(node.kind()));
  out << ",\"kind_index\":" << static_cast<std::underlying_type_t<PowlNodeKind>>(node.kind())
      << ",\"label\":";
  quoted(out, node.label());
  out << ",\"frequency\":{\"minimum\":" << node.frequency().minimum << ",\"maximum\":";
  if (node.frequency().maximum) out << *node.frequency().maximum;
  else out << "null";
  out << "},\"children\":[";
  for (std::size_t i = 0; i < node.children().size(); ++i) {
    if (i) out << ',';
    powl_json(out, node.children()[i]);
  }
  out << "],\"order\":[";
  for (std::size_t i = 0; i < node.order().size(); ++i) {
    if (i) out << ',';
    out << '[' << node.order()[i].first << ',' << node.order()[i].second << ']';
  }
  out << "]}";
}

template <class Integer>
void integers(std::ostream& out, const std::vector<Integer>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  out << ']';
}

std::ostringstream output() {
  std::ostringstream result;
  result.imbue(std::locale::classic());
  return result;
}

}  // namespace

std::string serialize_model_conversion_powl_json(const PowlNode& node) {
  auto out = output();
  powl_json(out, node);
  return out.str();
}

PetriNetReductionOptions parse_model_reduction_workbench_params(
    const FieldLookup& field, const FieldPresence& present) {
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto boolean_field = [](std::string_view name) {
    return name == "petri_reduction_parallel_place_fusion" ||
           name == "petri_reduction_abstraction" ||
           name == "petri_reduction_preserve_join_bridges";
  };
  const FieldLookup booleans = [&](std::string_view name) {
    return boolean_field(name) ? field(name) : std::string{};
  };
  const FieldPresence booleans_present = [&](std::string_view name) {
    return boolean_field(name) && supplied(name);
  };
  auto options = parse_petri_net_reduction_params(booleans, booleans_present);
  const auto marking = [&](std::string_view legacy, std::string_view json) {
    const bool has_legacy = supplied(legacy), has_json = supplied(json);
    if (has_legacy && has_json) invalid(std::string(legacy) + " conflicts with " + std::string(json));
    if (has_json) return json_marking(field(json));
    if (has_legacy) return legacy_marking(field(legacy));
    return Marking{};
  };
  options.initial_marking = marking("petri_reduction_initial_marking", "petri_reduction_initial_marking_json");
  options.final_marking = marking("petri_reduction_final_marking", "petri_reduction_final_marking_json");
  return options;
}

std::string serialize_model_reduction_options_json(const PetriNetReductionOptions& options) {
  auto out = output();
  out << "{\"initial_marking\":"; integers(out, options.initial_marking);
  out << ",\"final_marking\":"; integers(out, options.final_marking);
  out << ",\"enable_parallel_place_fusion\":" << boolean(options.enable_parallel_place_fusion)
      << ",\"enable_abstraction\":" << boolean(options.enable_abstraction)
      << ",\"preserve_join_bridges\":" << boolean(options.preserve_join_bridges) << '}';
  return out.str();
}

std::string serialize_model_reduction_result_json(const PetriNetReductionResult& result) {
  auto out = output();
  out << "{\"net\":" << workflow_input_petri_json(result.net) << ",\"markings\":{\"initial\":";
  integers(out, result.markings.initial);
  out << ",\"final\":"; integers(out, result.markings.final);
  out << "},\"old_to_new_place\":"; integers(out, result.old_to_new_place);
  out << ",\"old_to_new_transition\":"; integers(out, result.old_to_new_transition);
  out << ",\"changed\":" << boolean(result.changed) << '}';
  return out.str();
}

}  // namespace minera
