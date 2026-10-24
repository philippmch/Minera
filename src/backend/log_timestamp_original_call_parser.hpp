#pragma once

#include "minera/embedding_workbench_json.hpp"
#include "minera/log_timestamp_filter.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <set>

namespace minera::log_timestamp_original_call_detail {
namespace shared = polars_ltl_original_call_detail;
using Node = shared::Node;

[[noreturn]] inline void invalid(std::string_view message) {
  throw std::invalid_argument("original LOG timestamp call_json: " + std::string(message));
}
inline const Node& at(const Node& n, std::string_view key) {
  const auto* value = shared::field(n, key);
  if (!value) invalid("missing packed field");
  return *value;
}
inline void exact(const Node& n, std::initializer_list<std::string_view> keys) {
  if (n.kind != Node::kObject || n.fields.size() != keys.size()) invalid("invalid packed fields");
  for (const auto key : keys) (void)at(n, key);
}
inline std::string string(const Node& n) {
  if (n.kind != Node::kString) invalid("string required");
  return n.text;
}
inline std::string tag(const Node& n) { return string(at(n, "type")); }
inline std::string token(std::string_view raw, const Node& n) {
  return std::string(raw.substr(n.begin, n.end - n.begin));
}
inline std::string integer_token(std::string_view raw, const Node& n) {
  const auto text = token(raw, n);
  const auto start = !text.empty() && text[0] == '-' ? 1U : 0U;
  if (n.kind != Node::kNumber || text.size() == start ||
      text.find_first_not_of("0123456789", start) != std::string::npos) invalid("integer token required");
  return text;
}
inline std::size_t index(std::string_view raw, const Node& n) {
  const auto text = integer_token(raw, n);
  if (text[0] == '-') invalid("trace index must be nonnegative");
  std::size_t result = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) invalid("trace index outside native range");
  return result;
}
inline void utf8(std::string_view text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i++]);
    if (lead < 0x80) continue;
    unsigned count = 0, code = 0, minimum = 0;
    if (lead >= 0xc2 && lead <= 0xdf) { count = 1; code = lead & 0x1f; minimum = 0x80; }
    else if (lead >= 0xe0 && lead <= 0xef) { count = 2; code = lead & 0x0f; minimum = 0x800; }
    else if (lead >= 0xf0 && lead <= 0xf4) { count = 3; code = lead & 7; minimum = 0x10000; }
    else invalid("descriptor requires strict UTF-8");
    if (text.size() - i < count) invalid("truncated UTF-8");
    while (count--) {
      const auto child = static_cast<unsigned char>(text[i++]);
      if ((child & 0xc0) != 0x80) invalid("descriptor requires strict UTF-8");
      code = (code << 6) | (child & 0x3f);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) invalid("invalid UTF-8 scalar");
  }
}
inline double finite_number(std::string_view raw, const Node& n) {
  if (n.kind != Node::kNumber) invalid("finite JSON number required");
  const auto text = token(raw, n);
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !std::isfinite(value)) invalid("finite JSON number required");
  return value;
}
inline bool canonical_uuid(std::string_view s) {
  if (s.size() != 36) return false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
    else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  }
  return true;
}
inline int hex_digit(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
inline std::string bytes(const Node& n) {
  const auto text = string(n);
  if (text.size() % 2) invalid("bytes require even lowercase hexadecimal text");
  std::string out;
  for (std::size_t i = 0; i < text.size(); i += 2) {
    const auto a = hex_digit(text[i]), b = hex_digit(text[i + 1]);
    if (a < 0 || b < 0) invalid("bytes require even lowercase hexadecimal text");
    out.push_back(static_cast<char>(16 * a + b));
  }
  return out;
}
inline double floating(const Node& n) {
  const auto text = string(n);
  if (text == "nan") return std::numeric_limits<double>::quiet_NaN();
  if (text == "inf") return std::numeric_limits<double>::infinity();
  if (text == "-inf") return -std::numeric_limits<double>::infinity();
  std::size_t pos = text.starts_with('-') ? 1 : 0;
  if (text.substr(pos, 2) != "0x") invalid("float requires Python hexadecimal form");
  pos += 2;
  bool digit = false, point = false;
  for (; pos < text.size() && text[pos] != 'p'; ++pos) {
    if (text[pos] == '.' && !point) point = true;
    else if (hex_digit(text[pos]) >= 0) digit = true;
    else invalid("invalid hexadecimal float significand");
  }
  if (!digit || pos == text.size()) invalid("invalid hexadecimal float exponent");
  ++pos;
  if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
  if (pos == text.size() || text.find_first_not_of("0123456789", pos) != std::string::npos) invalid("invalid hexadecimal float exponent");
  char* end = nullptr;
  const auto value = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !std::isfinite(value)) invalid("hexadecimal float outside native range");
  return value;
}

// Descriptor grammar validation never materializes opaque log/trace/enum values.
inline void validate(std::string_view raw, const Node& n, unsigned depth = 0);
inline void trace_snapshot(std::string_view raw, const Node& n, unsigned depth) {
  exact(n, {"attributes", "properties", "events"});
  for (const auto key : {"attributes", "properties"}) {
    const auto& child = at(n, key); if (tag(child) != "dict") invalid("trace metadata requires dictionaries"); validate(raw, child, depth + 1);
  }
  const auto& events = at(n, "events");
  if (events.kind != Node::kArray) invalid("trace events require array");
  for (const auto& event : events.items) {
    if (tag(event) != "dict") invalid("event requires dictionary"); validate(raw, event, depth + 1);
  }
}
inline void validate(std::string_view raw, const Node& n, unsigned depth) {
  if (depth > 64) invalid("Packed nesting limit exceeded");
  const auto type = tag(n);
  if (type == "nat" || type == "native_na") { exact(n, {"type"}); return; }
  if (type == "NoneType" || type == "str" || type == "bool" || type == "int" || type == "uuid") {
    exact(n, {"type", "value"}); const auto& value = at(n, "value");
    if (type == "NoneType") { if (value.kind != Node::kNull) invalid("NoneType requires null"); }
    else if (type == "bool") { if (value.kind != Node::kBoolean) invalid("bool requires Boolean"); }
    else if (type == "int") (void)integer_token(raw, value);
    else { const auto text = string(value); if (type == "uuid" && !canonical_uuid(text)) invalid("UUID requires canonical lowercase text"); }
    return;
  }
  if (type == "float" || type == "bytes") {
    exact(n, {"type", "hex"}); if (type == "float") (void)floating(at(n, "hex")); else (void)bytes(at(n, "hex")); return;
  }
  if (type == "datetime") {
    exact(n, {"type", "iso", "fold", "offset_seconds"}); (void)string(at(n, "iso"));
    const auto fold = integer_token(raw, at(n, "fold")); if (fold != "0" && fold != "1") invalid("datetime fold requires 0 or 1");
    const auto& offset = at(n, "offset_seconds"); if (offset.kind != Node::kNull) (void)finite_number(raw, offset);
    return;
  }
  if (type == "dict") {
    exact(n, {"type", "items"}); const auto& items = at(n, "items"); if (items.kind != Node::kArray) invalid("dictionary requires pair array");
    for (const auto& pair : items.items) {
      if (pair.kind != Node::kArray || pair.items.size() != 2) invalid("dictionary requires pairs");
      validate(raw, pair.items[0], depth + 1); validate(raw, pair.items[1], depth + 1);
    }
    return;
  }
  if (type == "list" || type == "tuple" || type == "set" || type == "frozenset") {
    exact(n, {"type", "values"}); const auto& values = at(n, "values"); if (values.kind != Node::kArray) invalid("sequence requires value array");
    for (const auto& child : values.items) validate(raw, child, depth + 1); return;
  }
  if (type == "enum") {
    exact(n, {"type", "module", "name", "value"}); (void)string(at(n, "module")); (void)string(at(n, "name")); validate(raw, at(n, "value"), depth + 1); return;
  }
  if (type == "trace") {
    exact(n, {"type", "snapshot", "source_trace_index"}); const auto& selected = at(n, "source_trace_index");
    if (selected.kind != Node::kNull) (void)integer_token(raw, selected);
    trace_snapshot(raw, at(n, "snapshot"), depth + 1); return;
  }
  if (type == "log") {
    exact(n, {"type", "snapshot"}); const auto& snap = at(n, "snapshot");
    exact(snap, {"attributes", "extensions", "classifiers", "omni_present", "properties", "traces"});
    for (const auto key : {"attributes", "extensions", "classifiers", "omni_present", "properties"}) {
      const auto& child = at(snap, key); if (tag(child) != "dict") invalid("log metadata requires dictionaries"); validate(raw, child, depth + 1);
    }
    const auto& traces = at(snap, "traces"); if (traces.kind != Node::kArray) invalid("log traces require array");
    for (const auto& trace : traces.items) trace_snapshot(raw, trace, depth + 1); return;
  }
  invalid("unsupported Packed tag");
}
inline EventAttribute packed(std::string_view raw, const Node& n, std::string key = {}) {
  const auto type = tag(n);
  if (type == "native_na") return make_native_na_attribute(std::move(key));
  if (type == "nat") { EventAttribute out{std::move(key), "NaT", AttributeKind::kDate, true}; out.typed_value = NativeDate{"NaT", NativeDateResolution::kNanosecond}; return out; }
  if (type == "NoneType") { EventAttribute out{std::move(key), ""}; out.is_null = true; return out; }
  if (type == "str" || type == "uuid") return EventAttribute{std::move(key), string(at(n, "value")), type == "uuid" ? AttributeKind::kId : AttributeKind::kString};
  if (type == "bool") { const auto value = at(n, "value").boolean; EventAttribute out{std::move(key), value ? "true" : "false", AttributeKind::kBoolean}; out.typed_value = value; return out; }
  if (type == "int") {
    const auto text = integer_token(raw, at(n, "value")); EventAttribute out{std::move(key), text, AttributeKind::kInt};
    std::int64_t signed_value = 0; const auto a = std::from_chars(text.data(), text.data() + text.size(), signed_value);
    if (a.ec == std::errc{} && a.ptr == text.data() + text.size()) { out.value = std::to_string(signed_value); out.typed_value = signed_value; }
    else if (text[0] != '-') {
      std::uint64_t unsigned_value = 0; const auto b = std::from_chars(text.data(), text.data() + text.size(), unsigned_value);
      if (b.ec == std::errc{} && b.ptr == text.data() + text.size()) { out.value = std::to_string(unsigned_value); out.typed_value = unsigned_value; }
    }
    return out;
  }
  if (type == "float") { const auto value = floating(at(n, "hex")); EventAttribute out{std::move(key), std::to_string(value), AttributeKind::kFloat}; out.typed_value = value; return out; }
  if (type == "bytes") return make_bytes_attribute(std::move(key), bytes(at(n, "hex")));
  if (type == "datetime") { const auto iso = string(at(n, "iso")); EventAttribute out{std::move(key), iso, AttributeKind::kDate}; out.typed_value = NativeDate{iso}; return out; }
  EventAttribute out{std::move(key), ""};
  if (type == "dict") {
    out.container = AttributeContainerKind::kContainer;
    for (const auto& pair : at(n, "items").items) {
      if (tag(pair.items[0]) != "str") invalid("native dictionary children require string keys");
      out.children.push_back(packed(raw, pair.items[1], string(at(pair.items[0], "value"))));
    }
    return out;
  }
  if (type != "list" && type != "tuple" && type != "set" && type != "frozenset") invalid("enum/log/trace cannot be native attribute operands");
  out.container = type == "list" ? AttributeContainerKind::kList : type == "tuple" ? AttributeContainerKind::kTuple : type == "set" ? AttributeContainerKind::kSet : AttributeContainerKind::kFrozenSet;
  const auto& children = at(n, "values").items;
  for (std::size_t i = 0; i < children.size(); ++i) out.children.push_back(packed(raw, children[i], std::to_string(i)));
  return out;
}

struct Signature {
  std::string native_function;
  std::vector<std::string> names;
  bool predicate = false, filter = false;
};
inline Signature signature(std::string_view operation) {
  if (operation == "apply") return {"apply_log_timestamp_filter", {}};
  if (operation == "apply_auto_filter") return {"apply_log_timestamp_auto_filter", {}};
  if (operation == "apply_events") return {"filter_log_timestamp_events", {"dt1", "dt2"}, false, true};
  if (operation == "filter_traces_contained") return {"filter_log_timestamp_contained", {"dt1", "dt2"}, false, true};
  if (operation == "filter_traces_intersecting") return {"filter_log_timestamp_intersecting", {"dt1", "dt2"}, false, true};
  if (operation == "filter_traces_starting_in_timeframe") return {"filter_log_timestamp_starting", {"dt1", "dt2"}, false, true};
  if (operation == "filter_traces_completing_in_timeframe") return {"filter_log_timestamp_completing", {"dt1", "dt2"}, false, true};
  if (operation == "filter_on_trace_attribute") return {"filter_log_timestamp_trace_attribute", {"dt1", "dt2"}, false, true};
  if (operation == "filter_traces_attribute_in_timeframe") return {"filter_log_timestamp_attribute", {"attribute", "value", "dt1", "dt2"}, false, true};
  if (operation == "is_contained") return {"log_timestamp_is_contained", {"dt1", "dt2", "timestamp_key"}, true};
  if (operation == "is_intersecting") return {"log_timestamp_is_intersecting", {"dt1", "dt2", "timestamp_key"}, true};
  if (operation == "trace_attr_is_contained") return {"log_timestamp_trace_attribute_is_contained", {"dt1", "dt2", "trace_attribute"}, true};
  if (operation == "has_attribute_in_timeframe") return {"log_timestamp_has_attribute_in_timeframe", {"attribute", "value", "dt1", "dt2", "timestamp_key"}, true};
  invalid("unsupported operation");
}
struct Assignment { std::size_t keyword, item; std::string member; };
struct Call {
  Signature spec;
  std::vector<EventAttribute> args;
  std::optional<LogTimestampFilterOptions> options;
  std::optional<std::size_t> source_trace_index;
  std::string parameter_state;
  std::vector<Assignment> projected;
  std::vector<std::size_t> unprojected_parameters, unprojected_keywords;
  bool bound = false;
};
inline Call parse(std::string_view raw, std::string_view operation) {
  try {
    utf8(raw);
    if (raw.starts_with(std::string_view("\xef\xbb\xbf", 3))) raw.remove_prefix(3);
    const auto root = shared::Reader(raw).read();
    exact(root, {"schema", "args", "keyword_entries", "source_trace_index"});
    if (string(at(root, "schema")) != "minera-log-timestamp-original-call-v1") invalid("unsupported schema");
    Call call; call.spec = signature(operation);
    const auto& args = at(root, "args"); const auto& keywords = at(root, "keyword_entries");
    if (args.kind != Node::kArray || keywords.kind != Node::kArray) invalid("args and keyword_entries require arrays");
    if (args.items.size() != call.spec.names.size()) invalid("wrong non-source positional arity");
    const auto& selected = at(root, "source_trace_index");
    if (call.spec.predicate) call.source_trace_index = index(raw, selected);
    else if (selected.kind != Node::kNull) invalid("source_trace_index must be null for log operations");
    for (const auto& arg : args.items) { validate(raw, arg); call.args.push_back(packed(raw, arg)); }
    if (call.spec.filter) call.options.emplace();
    call.parameter_state = call.spec.filter ? "omitted" : call.spec.predicate ? "not_applicable" : "omitted";
    std::set<std::string> names;
    for (std::size_t ki = 0; ki < keywords.items.size(); ++ki) {
      const auto& entry = keywords.items[ki]; exact(entry, {"key", "value"});
      const auto key = string(at(entry, "key")); if (!names.insert(key).second) invalid("duplicate top-level keyword");
      const auto& value = at(entry, "value"); validate(raw, value);
      if (!call.spec.filter || key != "parameters") {
        call.unprojected_keywords.push_back(ki);
        if (key == "parameters") call.parameter_state = "ignored";
        continue;
      }
      if (tag(value) != "dict") { call.parameter_state = "non_dictionary"; call.unprojected_keywords.push_back(ki); continue; }
      call.parameter_state = "dictionary";
      const auto& pairs = at(value, "items").items;
      for (std::size_t pi = 0; pi < pairs.size(); ++pi) {
        const auto& parameter_key = pairs[pi].items[0]; const auto& parameter_value = pairs[pi].items[1];
        const auto key_type = tag(parameter_key); const bool enum_key = key_type == "enum";
        const Node* selected_key = enum_key ? &at(parameter_key, "value") : &parameter_key;
        std::string native_key;
        if ((enum_key || key_type == "str") && tag(*selected_key) == "str") native_key = string(at(*selected_key, "value"));
        std::string member;
        if (native_key == "pm4py:param:timestamp_key") member = enum_key ? "enum_timestamp_key" : "timestamp_key";
        else if (native_key == "positive") member = "positive";
        if (member.empty()) { call.unprojected_parameters.push_back(pi); continue; }
        auto operand = packed(raw, parameter_value);
        if (member == "timestamp_key") call.options->timestamp_key = std::move(operand);
        else if (member == "enum_timestamp_key") call.options->enum_timestamp_key = std::move(operand);
        else call.options->positive = std::move(operand);
        call.projected.push_back({ki, pi, std::move(member)});
      }
    }
    return call;
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what(); constexpr std::string_view prefix = "original Polars call_json: ";
    if (message.starts_with(prefix)) invalid(std::string_view(message).substr(prefix.size()));
    throw;
  }
}
inline void bind(Call& call, const EventLog& source) {
  if (call.spec.predicate && *call.source_trace_index >= source.traces.size()) invalid("trace reference outside actual source log");
  call.bound = true;
}
inline std::string q(std::string_view text) { return serialize_embedding_workbench_string_json(text); }
inline std::string cell_json(const EventAttribute& attribute) {
  EventTable table; table.columns = {attribute.key}; table.dtypes = {"object"}; table.rows = {{attribute}};
  return serialize_embedding_workbench_table_json(table);
}
inline std::string indices_json(const std::vector<std::size_t>& values) {
  std::string out = "[";
  for (std::size_t i = 0; i < values.size(); ++i) { if (i) out += ','; out += std::to_string(values[i]); }
  return out + ']';
}
inline std::string projection_json(const Call& call, std::string_view operation) {
  if (!call.bound) throw std::logic_error("native call must be bound before projection inspection");
  const auto selected = call.source_trace_index ? std::to_string(*call.source_trace_index) : "null";
  std::string names = "["; std::vector<std::size_t> origins;
  for (std::size_t i = 0; i < call.spec.names.size(); ++i) { if (i) names += ','; names += q(call.spec.names[i]); origins.push_back(i); }
  names += ']';
  std::string projected = "[";
  for (std::size_t i = 0; i < call.projected.size(); ++i) {
    if (i) projected += ','; const auto& entry = call.projected[i];
    projected += "{\"keyword_entry_index\":" + std::to_string(entry.keyword) + ",\"parameter_item_index\":" + std::to_string(entry.item) + ",\"native_member\":" + q(entry.member) + '}';
  }
  projected += ']';
  return "{\"schema\":\"minera-log-timestamp-call-projection-v1\",\"operation\":" + q(operation) + ",\"native_function\":" + q(call.spec.native_function) +
    ",\"source_argument_kind\":" + q(call.spec.predicate ? "trace" : "log") + ",\"source_trace_index\":" + selected + ",\"native_argument_names\":" + names +
    ",\"native_argument_origins\":" + indices_json(origins) + ",\"native_options_passing\":" + q(call.spec.filter ? "explicit_caller_owned_object" : "not_applicable") +
    ",\"stub_parameters_passing\":" + q(!call.spec.filter && !call.spec.predicate ? "implicit_default_not_inspected" : "not_applicable") +
    ",\"parameters_descriptor_state\":" + q(call.parameter_state) + ",\"projected_parameter_entries\":" + projected + ",\"unprojected_parameter_entries\":" + indices_json(call.unprojected_parameters) +
    ",\"unprojected_keyword_entries\":" + indices_json(call.unprojected_keywords) + ",\"projection_scope\":\"retained_native_mapper_construction\"}";
}
inline std::string controls_json(const Call& call, const EventLog& source) {
  if (!call.bound) throw std::logic_error("native call must be bound before controls inspection");
  std::string source_argument;
  if (call.spec.predicate) {
    const Trace& actual = source.traces.at(*call.source_trace_index);
    EventLog wrapper; wrapper.traces.push_back(actual); // Serialization only; never a dispatch operand.
    source_argument = "{\"kind\":\"trace\",\"source_trace_index\":" + std::to_string(*call.source_trace_index) + ",\"log_json\":null,\"trace_json\":" + q(serialize_embedding_workbench_raw_log_json(wrapper)) + '}';
  } else source_argument = "{\"kind\":\"log\",\"source_trace_index\":null,\"log_json\":" + q(serialize_embedding_workbench_raw_log_json(source)) + ",\"trace_json\":null}";
  std::string args = "[";
  for (std::size_t i = 0; i < call.args.size(); ++i) {
    if (i) args += ',';
    args += "{\"name\":" + q(call.spec.names.at(i)) + ",\"position\":" + std::to_string(i) + ",\"value_json\":" + q(cell_json(call.args[i])) + '}';
  }
  args += ']';
  std::string options = "null";
  if (call.options) {
    const auto& actual = *call.options;
    options = "{\"timestamp_key_json\":" + q(cell_json(actual.timestamp_key)) + ",\"enum_timestamp_key_json\":" +
      (actual.enum_timestamp_key ? q(cell_json(*actual.enum_timestamp_key)) : "null") + ",\"positive_json\":" + q(cell_json(actual.positive)) + '}';
  }
  return "{\"schema\":\"minera-log-timestamp-native-controls-v1\",\"source_argument\":" + source_argument + ",\"arguments\":" + args + ",\"options\":" + options + '}';
}
} // namespace minera::log_timestamp_original_call_detail
