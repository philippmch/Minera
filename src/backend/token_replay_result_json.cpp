#include "minera/json_export.hpp"
#include "root_cause_trace_key.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {
using Values = std::vector<std::string>;
using Items = std::vector<std::pair<std::string, std::string>>;

std::string quote(std::string_view value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << char(c);
    else if (c < 32) out << "\\u00" << std::hex << std::setw(2)
                        << std::setfill('0') << unsigned(c) << std::dec;
    else out << char(c);
  }
  out << '"';
  return out.str();
}

std::string string_value(std::string_view value) {
  return "{\"type\":\"str\",\"value\":" + quote(value) + '}';
}
std::string null_value() { return "{\"type\":\"NoneType\",\"value\":null}"; }
std::string integer(const TokenInteger& value) {
  return "{\"type\":\"int\",\"value\":" + value.decimal() + '}';
}
std::string boolean(bool value) {
  return std::string("{\"type\":\"bool\",\"value\":") + (value ? "true}" : "false}");
}
std::string real(double value) {
  std::string hex;
  if (std::isnan(value)) hex = "nan";
  else if (std::isinf(value)) hex = std::signbit(value) ? "-inf" : "inf";
  else if (value == 0) hex = std::signbit(value) ? "-0x0.0p+0" : "0x0.0p+0";
  else {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto exponent = (bits >> 52) & 0x7ff;
    const int power = exponent ? static_cast<int>(exponent) - 1023 : -1022;
    std::ostringstream out;
    if (std::signbit(value)) out << '-';
    out << (exponent ? "0x1." : "0x0.") << std::hex << std::setw(13)
        << std::setfill('0') << (bits & 0xfffffffffffffull) << std::dec
        << 'p' << (power < 0 ? "" : "+") << power;
    hex = out.str();
  }
  return "{\"type\":\"float\",\"hex\":" + quote(hex) + '}';
}
std::string sequence(const Values& values, std::string_view type = "list") {
  std::string result = "{\"type\":" + quote(type) + ",\"values\":[";
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) result += ',';
    result += values[i];
  }
  return result + "]}";
}
std::string dictionary(const Items& items, std::string_view type = "dict") {
  std::string result = "{\"type\":" + quote(type) + ",\"items\":[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i) result += ',';
    result += '[' + items[i].first + ',' + items[i].second + ']';
  }
  return result + "]}";
}

std::string attribute(const EventAttribute& value) {
  if (value.is_null || value.first_class_state == FirstClassAttributeState::kNull)
    return null_value();
  if (value.container == AttributeContainerKind::kTuple ||
      value.container == AttributeContainerKind::kSet ||
      value.container == AttributeContainerKind::kFrozenSet) {
    Values values;
    if (value.container == AttributeContainerKind::kTuple)
      for (const auto& child : value.children) values.push_back(attribute(child));
    else for (const auto* child : root_cause_set_members(value)) values.push_back(attribute(*child));
    return sequence(values, value.container == AttributeContainerKind::kTuple ? "tuple"
        : value.container == AttributeContainerKind::kSet ? "set" : "frozenset");
  }
  if (value.container != AttributeContainerKind::kScalar || value.scalar_wrapper ||
      value.scalar_children_list || !value.children.empty()) {
    auto scalar = value;
    scalar.children.clear();
    scalar.container = AttributeContainerKind::kScalar;
    scalar.scalar_wrapper = scalar.scalar_children_list = false;
    const bool ordered = value.container == AttributeContainerKind::kList || value.scalar_children_list;
    Values pairs;
    std::map<std::string, const EventAttribute*> children;
    for (const auto& child : value.children) {
      if (ordered) pairs.push_back(sequence({string_value(child.key), attribute(child)}, "tuple"));
      else children[child.key] = &child;
    }
    Items items;
    for (const auto& [key, child] : children) items.emplace_back(string_value(key), attribute(*child));
    return dictionary({{string_value("value"), value.container == AttributeContainerKind::kScalar
        ? attribute(scalar) : null_value()}, {string_value("children"), ordered ? sequence(pairs) : dictionary(items)}});
  }
  if (is_native_na_attribute(value)) return "{\"type\":\"NativeNA\"}";
  if (const auto* scalar = std::get_if<bool>(&value.typed_value)) return boolean(*scalar);
  if (const auto* scalar = std::get_if<std::int64_t>(&value.typed_value)) return integer(*scalar);
  if (const auto* scalar = std::get_if<std::uint64_t>(&value.typed_value)) return integer(*scalar);
  if (const auto* scalar = std::get_if<double>(&value.typed_value)) return real(*scalar);
  if (value.kind == AttributeKind::kInt) return integer(TokenInteger(root_cause_attribute_category(value)));
  if (value.kind == AttributeKind::kFloat) return real(native_attribute_number(value));
  if (value.kind == AttributeKind::kBoolean) return boolean(root_cause_attribute_category(value) == "True");
  if (value.kind == AttributeKind::kBytes) {
    const auto bytes = decode_bytes_attribute(value);
    std::ostringstream out;
    for (const unsigned char c : bytes) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
    return "{\"type\":\"bytes\",\"hex\":" + quote(out.str()) + '}';
  }
  if (value.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value))
    return "{\"type\":\"NativeDate\",\"value\":" + quote(root_cause_attribute_category(value)) + '}';
  return string_value(event_attribute_lexical_value(value));
}
std::string attributes(const std::vector<EventAttribute>& values) {
  Items items;
  for (const auto& value : values) items.emplace_back(string_value(value.key), attribute(value));
  return dictionary(items);
}

class Codec {
 public:
  explicit Codec(const TokenReplayResult& value) : result(value) {
    if (result.transition_names.size() != result.transition_labels.size())
      throw std::invalid_argument("token replay transition labels have the wrong size");
    place_slots = slots(result.place_names);
    transition_slots = slots(result.transition_names);
  }
  std::string encode() const {
    Values rows;
    for (const auto& row : result.traces) rows.push_back(trace_row(row));
    if (!result.pltr_fitness) return sequence(rows);
    const auto& fitness = *result.pltr_fitness;
    Items places, transitions, unknown;
    for (std::size_t p = 0; p < fitness.places.size(); ++p) {
      const auto& counts = fitness.places[p];
      places.emplace_back(place(p), dictionary({
          {string_value("underfed_traces"), trace_set(counts.underfed_traces)},
          {string_value("overfed_traces"), trace_set(counts.overfed_traces)},
          {string_value("m"), integer(counts.missing)},
          {string_value("r"), integer(counts.remaining)},
          {string_value("c"), integer(counts.consumed)},
          {string_value("p"), integer(counts.produced)}}));
    }
    for (const auto& entry : fitness.transitions)
      transitions.emplace_back(transition(entry.transition_index), dictionary({
          {string_value("underfed_traces"), contexts(entry.underfed_traces, false)},
          {string_value("fit_traces"), contexts(entry.fit_traces, false)}}));
    for (const auto& entry : fitness.unknown_activities)
      unknown.emplace_back(attribute(entry.activity), contexts(entry.traces, true));
    return sequence({sequence(rows), dictionary(places), dictionary(transitions), dictionary(unknown)}, "tuple");
  }
 private:
  const TokenReplayResult& result;
  std::vector<std::size_t> place_slots, transition_slots;
  static std::vector<std::size_t> slots(const std::vector<std::string>& names) {
    std::vector<std::size_t> order(names.size()), positions(names.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](auto left, auto right) { return names[left] < names[right]; });
    for (std::size_t i = 0; i < order.size(); ++i) positions[order[i]] = i;
    return positions;
  }
  std::string place(std::size_t index) const {
    return "{\"type\":\"Place\",\"index\":" + std::to_string(place_slots.at(index))
        + ",\"name\":" + string_value(result.place_names.at(index)) + '}';
  }
  std::string label(std::size_t index) const {
    const auto& value = result.transition_labels.at(index);
    return value ? string_value(*value) : null_value();
  }
  std::string transition(std::size_t index) const {
    return "{\"type\":\"Transition\",\"index\":" + std::to_string(transition_slots.at(index))
        + ",\"name\":" + string_value(result.transition_names.at(index))
        + ",\"label\":" + label(index) + '}';
  }
  std::string trace(std::size_t reference) const {
    const auto& entry = result.pltr_fitness->trace_references.at(reference);
    std::string encoded = "{\"type\":\"Trace\",\"index\":" + (entry.source_trace_index
        ? std::to_string(*entry.source_trace_index) : std::string("null"))
        + ",\"attributes\":" + attributes(native_trace_attributes(entry.value)) + ",\"events\":[";
    for (std::size_t i = 0; i < entry.value.events.size(); ++i) {
      if (i) encoded += ',';
      encoded += attributes(native_event_attributes(entry.value.events[i]));
    }
    return encoded + "]}";
  }
  std::string trace_set(const std::vector<std::size_t>& references) const {
    Values values;
    for (const auto reference : references) values.push_back(trace(reference));
    return sequence(values, "set");
  }
  std::string contexts(const std::vector<TokenReplayTraceContexts>& entries, bool single) const {
    Items items;
    for (const auto& entry : entries) {
      Values values;
      for (const auto& context : entry.contexts) values.push_back(attributes(native_event_attributes(context)));
      if (single && values.size() != 1) throw std::invalid_argument("unknown activity requires one context per trace");
      items.emplace_back(trace(entry.trace_reference), single ? values.front() : sequence(values));
    }
    return dictionary(items);
  }
  std::string trace_row(const TokenReplayTraceDiagnostics& row) const {
    const auto transitions = [&](const auto& indices, std::string_view type) {
      Values values;
      for (const auto t : indices) values.push_back(result.return_names
          ? string_value(result.transition_names.at(t)) : transition(t));
      return sequence(values, type);
    };
    Items marking;
    for (std::size_t p = 0; p < row.reached_marking.size(); ++p) {
      const bool present = row.reached_marking_keys.empty() ? row.reached_marking[p] != 0
          : row.reached_marking_keys.at(p);
      if (present) marking.emplace_back(result.return_names ? string_value(result.place_names.at(p))
          : std::to_string(place_slots.at(p)), integer(row.reached_marking[p]));
    }
    Items items{{string_value("trace_is_fit"), boolean(row.trace_is_fit)},
        {string_value("trace_fitness"), real(row.trace_fitness)},
        {string_value("activated_transitions"), transitions(row.activated_transitions, "list")},
        {string_value("transitions_with_problems"), transitions(row.transitions_with_problems, "list")},
        {string_value("enabled_transitions_in_marking"), transitions(row.enabled_transitions, result.return_names ? "list" : "set")},
        {string_value("reached_marking"), dictionary(marking, result.return_names ? "dict" : "Marking")},
        {string_value("missing_tokens"), integer(row.missing)},
        {string_value("consumed_tokens"), integer(row.consumed)},
        {string_value("remaining_tokens"), integer(row.remaining)},
        {string_value("produced_tokens"), integer(row.produced)}};
    if (result.return_names) {
      Values activated, enabled;
      for (const auto t : row.activated_transitions) activated.push_back(label(t));
      for (const auto t : row.enabled_transitions) enabled.push_back(label(t));
      items.emplace_back(string_value("activated_transitions_labels"), sequence(activated));
      items.emplace_back(string_value("enabled_transitions_in_marking_labels"), sequence(enabled));
    }
    return dictionary(items);
  }
};
}  // namespace

std::string token_replay_result_json(const TokenReplayResult& result) {
  return Codec(result).encode();
}
}  // namespace minera
