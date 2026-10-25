// EventLog filtering modules against the public seeded bank
// (docs/log-filter-domain-bank.json.gz, scripts/capture-log-filter-bank.py).
// Each case is an XES document imported by both sides; each call names a
// public function of pm4py.algo.filtering.log.{attributes,between,cases,ltl,
// paths,timestamp,variants}, its typed arguments and enum parameters. Returned
// logs compare as trace names and `rd:eid` event ids, scalar results as typed
// JSON, errors as class and message, and a changed source as its summary. A
// call whose public outcome varied between hash seeds must match one of them.
#include "minera/event_log.hpp"
#include "minera/log_attributes_filter.hpp"
#include "minera/log_between_filter.hpp"
#include "minera/log_case_filter.hpp"
#include "minera/log_ltl_filter.hpp"
#include "minera/log_paths_filter.hpp"
#include "minera/log_timestamp_filter.hpp"
#include "minera/log_variants_filter.hpp"
#include "minera/xes_event_log_importer.hpp"

#include "../gzip_codec.hpp"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  bool boolean = false;
  std::string text;  // string, or the number's literal text
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
  const Json& at(const std::string& key) const {
    const auto* found = get(key);
    if (!found) throw std::runtime_error("missing field " + key);
    return *found;
  }
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view text) : at_(text.data()), end_(text.data() + text.size()) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space();
        ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kString;
      out.text = string();
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      out.kind = Json::kNumber;
      const auto* start = at_;
      while (at_ < end_ && (std::strchr("+-.eE", *at_) || (*at_ >= '0' && *at_ <= '9'))) ++at_;
      out.text.assign(start, at_);
    }
    return out;
  }

 private:
  void space() {
    while (at_ < end_ && (*at_ == ' ' || *at_ == '\n' || *at_ == '\r' || *at_ == '\t')) ++at_;
  }
  unsigned hex4() {
    unsigned value = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = *at_++;
      value = value * 16 + static_cast<unsigned>(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return value;
  }
  static void utf8(std::string& out, unsigned code) {
    if (code < 0x80) {
      out += static_cast<char>(code);
    } else if (code < 0x800) {
      out += static_cast<char>(0xc0 | (code >> 6));
      out += static_cast<char>(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
      out += static_cast<char>(0xe0 | (code >> 12));
      out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (code & 0x3f));
    } else {
      out += static_cast<char>(0xf0 | (code >> 18));
      out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
      out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (code & 0x3f));
    }
  }
  std::string string() {
    std::string out;
    for (++at_; *at_ != '"'; ++at_) {
      if (*at_ != '\\') {
        out += *at_;
        continue;
      }
      ++at_;
      switch (*at_) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'u': {
          ++at_;
          unsigned code = hex4();
          if (code >= 0xd800 && code < 0xdc00) {
            at_ += 2;  // "\u"
            code = 0x10000 + ((code - 0xd800) << 10) + (hex4() - 0xdc00);
          }
          utf8(out, code);
          --at_;
          break;
        }
        default: out += *at_;
      }
    }
    ++at_;
    return out;
  }
  const char* at_;
  const char* end_;
};

std::string serialize(const Json& value) {
  switch (value.kind) {
    case Json::kNull: return "null";
    case Json::kBool: return value.boolean ? "true" : "false";
    case Json::kNumber: return value.text;
    case Json::kString: {
      std::string out = "\"";
      for (const char c : value.text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
      }
      return out + "\"";
    }
    case Json::kArray: {
      std::string out = "[";
      for (std::size_t i = 0; i < value.items.size(); ++i) out += (i ? "," : "") + serialize(value.items[i]);
      return out + "]";
    }
    case Json::kObject: {
      std::string out = "{";
      bool first = true;
      for (const auto& [key, item] : value.fields) {
        out += (first ? "\"" : ",\"") + key + "\":" + serialize(item);
        first = false;
      }
      return out + "}";
    }
  }
  return {};
}

Json jstring(std::string text) {
  Json out;
  out.kind = Json::kString;
  out.text = std::move(text);
  return out;
}
Json jarray(std::vector<Json> items = {}) {
  Json out;
  out.kind = Json::kArray;
  out.items = std::move(items);
  return out;
}
Json jtagged(const std::string& tag, Json value) {
  Json out;
  out.kind = Json::kObject;
  out.fields.emplace(tag, std::move(value));
  return out;
}

// ------------------------------------------------------------ typed values

std::string bits_hex(double value) {
  if (value != value) return "7ff8000000000000";
  std::uint64_t bits;
  std::memcpy(&bits, &value, sizeof bits);
  char buffer[17];
  std::snprintf(buffer, sizeof buffer, "%016llx", static_cast<unsigned long long>(bits));
  return buffer;
}
double hex_bits(const std::string& text) {
  const std::uint64_t bits = std::stoull(text, nullptr, 16);
  double value;
  std::memcpy(&value, &bits, sizeof value);
  return value;
}
std::string shortest(double value) {
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
  return std::string(buffer, result.ptr);
}

EventAttribute to_attribute(const Json& node, std::string key = {}) {
  const auto& [tag, value] = *node.fields.begin();
  EventAttribute out{std::move(key), ""};
  if (tag == "s") {
    out.value = value.text;
  } else if (tag == "i") {
    out.value = value.text;
    out.kind = AttributeKind::kInt;
    if (value.text.front() != '-' && value.text.size() >= 19 && std::stoull(value.text) > 9223372036854775807ULL)
      out.typed_value = static_cast<std::uint64_t>(std::stoull(value.text));
    else
      out.typed_value = static_cast<std::int64_t>(std::stoll(value.text));
  } else if (tag == "f") {
    const double number = hex_bits(value.text);
    out.value = shortest(number);
    out.kind = AttributeKind::kFloat;
    out.typed_value = number;
  } else if (tag == "b") {
    out.value = value.boolean ? "true" : "false";
    out.kind = AttributeKind::kBoolean;
    out.typed_value = value.boolean;
  } else if (tag == "n") {
    out.is_null = true;
  } else if (tag == "d") {
    auto text = value.text;
    text[10] = 'T';
    out.value = text;
    out.kind = AttributeKind::kDate;
    out.typed_value = NativeDate{text};
  } else if (tag == "l" || tag == "t") {
    out.container = tag == "l" ? AttributeContainerKind::kList : AttributeContainerKind::kTuple;
    for (const auto& item : value.items) out.children.push_back(to_attribute(item, std::to_string(out.children.size())));
  } else if (tag == "m") {
    out.container = AttributeContainerKind::kContainer;
    for (const auto& pair : value.items) {
      const auto name = to_attribute(pair.items.at(0));
      out.children.push_back(to_attribute(pair.items.at(1), name.value));
    }
  } else {
    throw std::runtime_error("unsupported typed argument tag " + tag);
  }
  return out;
}

Json to_typed(const EventAttribute& value) {
  if (value.container == AttributeContainerKind::kList || value.container == AttributeContainerKind::kTuple) {
    auto items = jarray();
    for (const auto& child : value.children) items.items.push_back(to_typed(child));
    return jtagged(value.container == AttributeContainerKind::kList ? "l" : "t", std::move(items));
  }
  if (value.container == AttributeContainerKind::kContainer) {
    auto items = jarray();
    for (const auto& child : value.children)
      items.items.push_back(jarray({jtagged("s", jstring(child.key)), to_typed(child)}));
    return jtagged("m", std::move(items));
  }
  if (value.is_null) return jtagged("n", Json{});
  switch (value.kind) {
    case AttributeKind::kInt:
      if (const auto* number = std::get_if<std::int64_t>(&value.typed_value))
        return jtagged("i", jstring(std::to_string(*number)));
      if (const auto* number = std::get_if<std::uint64_t>(&value.typed_value))
        return jtagged("i", jstring(std::to_string(*number)));
      return jtagged("i", jstring(value.value));
    case AttributeKind::kFloat:
      if (const auto* number = std::get_if<double>(&value.typed_value)) return jtagged("f", jstring(bits_hex(*number)));
      return jtagged("f", jstring(bits_hex(std::stod(value.value))));
    case AttributeKind::kBoolean: {
      Json flag;
      flag.kind = Json::kBool;
      if (const auto* b = std::get_if<bool>(&value.typed_value)) flag.boolean = *b;
      else flag.boolean = value.value == "true";
      return jtagged("b", flag);
    }
    case AttributeKind::kDate: return jtagged("d", jstring("native:" + value.value));
    default: return jtagged("s", jstring(value.value));
  }
}

Json bool_typed(bool value) {
  Json flag;
  flag.kind = Json::kBool;
  flag.boolean = value;
  return jtagged("b", flag);
}

// ------------------------------------------------------------ logs

Json log_summary(const EventLog& log) {
  auto traces = jarray();
  for (const auto& trace : log.traces) {
    auto events = jarray();
    for (const auto& event : trace.events) {
      Json id;
      for (const auto& attribute : event.attributes)
        if (attribute.key == "rd:eid") id = jstring(attribute.value);
      events.items.push_back(id);
    }
    Json name = trace.case_id_state == FirstClassAttributeState::kMissing && trace.case_id.empty()
                    ? jtagged("n", Json{})
                    : jtagged("s", jstring(trace.case_id));
    traces.items.push_back(jarray({name, events}));
  }
  return traces;
}

// ------------------------------------------------------------ helpers

std::vector<Trace> traces_of(const EventLog& log, const Json& node) {
  std::vector<Trace> out;
  for (const auto& index : node.at("x").items) out.push_back(log.traces.at(std::stoul(index.text)));
  return out;
}

LogVariantsDictionary variants_dictionary(const EventLog& log, const Json& node) {
  LogVariantsDictionary out;
  for (const auto& pair : node.at("m").items) out.push_back({to_attribute(pair.items.at(0)), traces_of(log, pair.items.at(1)), {}});
  return out;
}

LogPathsVariants paths_variants(const EventLog& log, const Json& node) {
  LogPathsVariants out;
  for (const auto& pair : node.at("m").items) out.push_back({to_attribute(pair.items.at(0)), traces_of(log, pair.items.at(1))});
  return out;
}

std::vector<std::pair<EventAttribute, EventAttribute>> pairs_of(const Json& node) {
  std::vector<std::pair<EventAttribute, EventAttribute>> out;
  for (const auto& pair : node.at("m").items) out.emplace_back(to_attribute(pair.items.at(0)), to_attribute(pair.items.at(1)));
  return out;
}

Json pairs_typed(const std::vector<std::pair<EventAttribute, EventAttribute>>& pairs) {
  auto items = jarray();
  for (const auto& [key, value] : pairs) items.items.push_back(jarray({to_typed(key), to_typed(value)}));
  return jtagged("m", std::move(items));
}

// Enum member name -> value of the public Parameters enums.
const std::map<std::string, std::string> kEnumValues = {
    {"ATTRIBUTE_KEY", "pm4py:param:attribute_key"}, {"ACTIVITY_KEY", "pm4py:param:activity_key"},
    {"CASE_ID_KEY", "pm4py:param:case_id_key"},     {"PARAMETER_KEY_CASE_GLUE", "case_id_glue"},
    {"DECREASING_FACTOR", "decreasingFactor"},      {"POSITIVE", "positive"},
    {"STREAM_FILTER_KEY1", "stream_filter_key1"},   {"STREAM_FILTER_VALUE1", "stream_filter_value1"},
    {"STREAM_FILTER_KEY2", "stream_filter_key2"},   {"STREAM_FILTER_VALUE2", "stream_filter_value2"},
    {"KEEP_ONCE_PER_CASE", "keep_once_per_case"},   {"TIMESTAMP_KEY", "pm4py:param:timestamp_key"},
};

// The public `parameters` argument: None, or a mapping of enum values.
EventAttribute parameters_attribute(const std::vector<std::pair<std::string, EventAttribute>>& params) {
  EventAttribute out{"", "", AttributeKind::kString, true};
  if (params.empty()) return out;
  out.is_null = false;
  out.container = AttributeContainerKind::kContainer;
  for (const auto& [name, value] : params) {
    auto child = value;
    child.key = kEnumValues.at(name);
    out.children.push_back(std::move(child));
  }
  return out;
}

struct Call {
  const Json& node;
  std::vector<std::pair<std::string, EventAttribute>> params;
  std::vector<const Json*> args;
  EventAttribute arg(std::size_t i) const { return to_attribute(*args.at(i)); }
  const EventAttribute* param(const std::string& name) const {
    for (auto it = params.rbegin(); it != params.rend(); ++it)
      if (it->first == name) return &it->second;
    return nullptr;
  }
};

template <class Options>
void set(Options& options, EventAttribute Options::*field, const Call& call, const char* name) {
  if (const auto* value = call.param(name)) options.*field = *value;
}

// One function per public module; each runs one call and returns the outcome
// object without source_after.
Json cases(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  if (function == "filter_on_case_size") {
    returned(call.args.size() > 1 ? filter_log_on_case_size(log, call.arg(0), call.arg(1))
                                  : filter_log_on_case_size(log, call.arg(0)));
  } else if (function == "filter_on_ncases") {
    returned(filter_log_on_ncases(log, call.arg(0)));
  } else if (function == "filter_on_case_performance" || function == "filter_case_performance") {
    LogCasePerformanceOptions options;
    set(options, &LogCasePerformanceOptions::timestamp_key, call, "TIMESTAMP_KEY");
    returned(function == "filter_on_case_performance"
                 ? filter_log_on_case_performance(log, call.arg(0), call.arg(1), options)
                 : filter_log_case_performance(log, call.arg(0), call.arg(1), options));
  } else if (function == "apply") {
    returned(apply_log_case_filter(log, parameters_attribute(call.params)));
  } else if (function == "apply_auto_filter") {
    returned(apply_log_case_auto_filter(log, parameters_attribute(call.params)));
  } else if (function == "satisfy_perf") {
    const auto& trace = log.traces.at(std::stoul(call.args.at(0)->at("i").text));
    const auto result = log_case_satisfies_performance(trace, call.arg(1), call.arg(2), call.arg(3));
    if (const auto* flag = std::get_if<bool>(&result)) value(bool_typed(*flag));
    else out.fields.emplace("unrepresentable", jstring("NA"));
  } else {
    throw std::runtime_error("unbound function");
  }
  return out;
}

Json paths(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  if (function == "apply" || function == "apply_performance") {
    LogPathsPerformanceOptions options;
    set<LogPathsFilterOptions>(options, &LogPathsFilterOptions::attribute_key, call, "ATTRIBUTE_KEY");
    set<LogPathsFilterOptions>(options, &LogPathsFilterOptions::positive, call, "POSITIVE");
    if (function == "apply") {
      returned(filter_log_paths(log, call.arg(0), options));
    } else {
      set(options, &LogPathsPerformanceOptions::timestamp_key, call, "TIMESTAMP_KEY");
      set(options, &LogPathsPerformanceOptions::minimum, call, "MIN_PERFORMANCE");
      set(options, &LogPathsPerformanceOptions::maximum, call, "MAX_PERFORMANCE");
      returned(filter_log_paths_performance(log, call.arg(0), options));
    }
  } else if (function == "get_paths_from_log") {
    value(pairs_typed(call.args.empty() ? get_log_paths_from_log(log) : get_log_paths_from_log(log, call.arg(0))));
  } else if (function == "get_sorted_paths_list") {
    value(to_typed(get_sorted_log_paths_list(LogPathsDictionary(pairs_of(*call.args.at(0))))));
  } else if (function == "get_paths_threshold") {
    value(to_typed(get_log_paths_threshold(call.arg(0), call.arg(1))));
  } else if (function == "filter_log_by_paths") {
    returned(filter_log_by_paths(log, pairs_of(*call.args.at(0)), paths_variants(log, *call.args.at(1)), call.arg(2),
                                 call.arg(3), call.arg(4)));
  } else {
    throw std::runtime_error("unbound function");
  }
  return out;
}

Json variants(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  LogVariantsFilterOptions options;
  set(options, &LogVariantsFilterOptions::activity_key, call, "ACTIVITY_KEY");
  set(options, &LogVariantsFilterOptions::positive, call, "POSITIVE");
  if (function == "apply") returned(filter_log_variants(log, call.arg(0), options));
  else if (function == "filter_variants_top_k") returned(filter_log_variants_top_k(log, call.arg(0), options));
  else if (function == "filter_variants_by_coverage_percentage")
    returned(filter_log_variants_min_coverage(log, call.arg(0), options));
  else if (function == "filter_variants_by_maximum_coverage_percentage")
    returned(filter_log_variants_max_coverage(log, call.arg(0), options));
  else if (function == "filter_log_variants_percentage")
    returned(call.args.empty() ? filter_log_variants_cumulative(log, to_attribute(jtagged("f", jstring(bits_hex(0.8)))), options)
                               : filter_log_variants_cumulative(log, call.arg(0), options));
  else if (function == "filter_variants_variants_percentage")
    returned(filter_log_variants_dictionary_percentage(log, variants_dictionary(log, *call.args.at(0)), call.arg(1)));
  else if (function == "find_auto_threshold") {
    Json typed = jtagged("f", jstring(bits_hex(
                     find_log_variants_auto_threshold(log, variants_dictionary(log, *call.args.at(0)), call.arg(1)))));
    value(std::move(typed));
  } else
    throw std::runtime_error("unbound function");
  return out;
}

Json timestamp(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  LogTimestampFilterOptions options;
  if (const auto* key = call.param("TIMESTAMP_KEY")) options.enum_timestamp_key = *key;
  const auto dt = [&](std::size_t i) { return call.arg(i); };
  if (function == "apply") returned(apply_log_timestamp_filter(log, parameters_attribute(call.params)));
  else if (function == "apply_auto_filter") returned(apply_log_timestamp_auto_filter(log, parameters_attribute(call.params)));
  else if (function == "filter_traces_contained") returned(filter_log_timestamp_contained(log, dt(0), dt(1), options));
  else if (function == "filter_traces_intersecting") returned(filter_log_timestamp_intersecting(log, dt(0), dt(1), options));
  else if (function == "apply_events") returned(filter_log_timestamp_events(log, dt(0), dt(1), options));
  else if (function == "filter_traces_starting_in_timeframe") returned(filter_log_timestamp_starting(log, dt(0), dt(1), options));
  else if (function == "filter_traces_completing_in_timeframe") returned(filter_log_timestamp_completing(log, dt(0), dt(1), options));
  else if (function == "filter_on_trace_attribute") returned(filter_log_timestamp_trace_attribute(log, dt(0), dt(1), options));
  else if (function == "filter_traces_attribute_in_timeframe")
    returned(filter_log_timestamp_attribute(log, call.arg(0), call.arg(1), dt(2), dt(3), options));
  else
    throw std::runtime_error("unbound function");
  return out;
}

Json between(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  if (function != "apply") throw std::runtime_error("unbound function");
  LogBetweenFilterOptions options;
  set(options, &LogBetweenFilterOptions::subcase_separator, call, "SUBCASE_CONCAT_STR");
  set(options, &LogBetweenFilterOptions::activity_key, call, "ACTIVITY_KEY");
  set(options, &LogBetweenFilterOptions::case_id_key, call, "CASE_ID_KEY");
  // Nonstring case-id keys stay out of the string-key projection, which
  // still carries every trace's concept:name.
  returned(filter_log_between_result(log, call.arg(0), call.arg(1), options).string_key_log);
  return out;
}

Json attributes(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  LogAttributesFilterParameters parameters;
  for (const auto& [name, attribute] : call.params) parameters.push_back({kEnumValues.at(name), attribute, true});
  const auto options = resolve_log_attributes_filter_parameters(parameters);
  if (function == "apply") returned(filter_log_attributes_traces(log, call.arg(0), options));
  else if (function == "apply_events") returned(filter_log_attributes_events(log, call.arg(0), options));
  else if (function == "apply_trace_attribute") returned(filter_log_attributes_trace_values(log, call.arg(0), options));
  else if (function == "apply_numeric") returned(filter_log_attributes_numeric(log, call.arg(0), call.arg(1), options));
  else if (function == "apply_numeric_events")
    returned(filter_log_attributes_numeric_events(log, call.arg(0), call.arg(1), options));
  else if (function == "filter_log_on_max_no_activities")
    returned(filter_log_attributes_max_activities(
        log, call.args.empty() ? to_attribute(jtagged("i", jstring("25"))) : call.arg(0), parameters));
  else if (function == "filter_log_relative_occurrence_event_attribute")
    returned(filter_log_attributes_relative_occurrence(log, call.arg(0), options));
  else if (function == "filter_log_by_attributes_threshold")
    returned(filter_log_attributes_threshold(log, LogAttributesCounts(pairs_of(*call.args.at(0))),
                                             variants_dictionary(log, *call.args.at(1)), call.arg(2), call.arg(3),
                                             call.arg(4)));
  else
    throw std::runtime_error("unbound function");
  return out;
}

Json ltl(const std::string& function, const Call& call, EventLog& log) {
  Json out;
  out.kind = Json::kObject;
  const auto returned = [&](const EventLog& result) { out.fields.emplace("log", log_summary(result)); };
  const auto value = [&](Json typed) { out.fields.emplace("value", std::move(typed)); };
  (void)value;
  LogLtlFilterOptions options;
  set(options, &LogLtlFilterOptions::attribute_key, call, "ATTRIBUTE_KEY");
  set(options, &LogLtlFilterOptions::resource_key, call, "RESOURCE_KEY");
  set(options, &LogLtlFilterOptions::timestamp_key, call, "TIMESTAMP_KEY");
  set(options, &LogLtlFilterOptions::positive, call, "POSITIVE");
  set(options, &LogLtlFilterOptions::enable_timestamp, call, "ENABLE_TIMESTAMP");
  if (const auto* bounds = call.param("TIMESTAMP_DIFF_BOUNDARIES")) options.timestamp_diff_boundaries = *bounds;
  if (function == "eventually_follows") returned(filter_log_ltl_eventually_follows(log, call.arg(0), options));
  else if (function == "A_next_B_next_C") returned(filter_log_ltl_next(log, call.arg(0), call.arg(1), call.arg(2), options));
  else if (function == "four_eyes_principle") returned(filter_log_ltl_four_eyes(log, call.arg(0), call.arg(1), options));
  else if (function == "attr_value_different_persons") returned(filter_log_ltl_different_persons(log, call.arg(0), options));
  else if (function == "timestamp_list_is_ge") value(to_typed(log_ltl_timestamp_list_is_ge(call.arg(0), call.arg(1))));
  else if (function == "timestamp_list_is_le") value(to_typed(log_ltl_timestamp_list_is_le(call.arg(0), call.arg(1))));
  else
    throw std::runtime_error("unbound function");
  return out;
}

template <class Error>
bool typed_error(const std::exception& error, Json& out) {
  const auto* typed = dynamic_cast<const Error*>(&error);
  if (!typed) return false;
  out.fields.emplace("error", jstring(typed->type));
  out.fields.emplace("message", jstring(typed->message));
  return true;
}

using Runner = std::function<Json(const std::string& module, const std::string& function, const Call&, EventLog&)>;

Json outcome(const Runner& run, const std::string& module, const std::string& function, const Call& call,
             const EventLog& source) {
  EventLog log = source;
  const auto before = serialize(log_summary(log));
  Json out;
  out.kind = Json::kObject;
  try {
    out = run(module, function, call, log);
  } catch (const std::exception& error) {
    out = Json{};
    out.kind = Json::kObject;
    if (!typed_error<LogCaseFilterError>(error, out) && !typed_error<LogPathsFilterError>(error, out) &&
        !typed_error<LogVariantsFilterError>(error, out) && !typed_error<LogTimestampFilterError>(error, out) &&
        !typed_error<LogBetweenFilterError>(error, out) && !typed_error<LogAttributesFilterError>(error, out) &&
        !typed_error<LogLtlFilterError>(error, out)) {
      out.fields.emplace("error", jstring("native"));
      out.fields.emplace("message", jstring(error.what()));
    }
  }
  auto after = log_summary(log);
  if (serialize(after) != before) out.fields.emplace("source_after", std::move(after));
  return out;
}

std::string read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return detail::gzip_decompress(bytes, std::size_t{1} << 31);
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.json.gz [--verbose]\n";
    return 2;
  }
  const bool verbose = argc > 2;
  const Runner run = [](const std::string& module, const std::string& function, const Call& call, EventLog& log) {
    if (module == "attributes") return attributes(function, call, log);
    if (module == "between") return between(function, call, log);
    if (module == "cases") return cases(function, call, log);
    if (module == "ltl") return ltl(function, call, log);
    if (module == "paths") return paths(function, call, log);
    if (module == "timestamp") return timestamp(function, call, log);
    if (module == "variants") return variants(function, call, log);
    throw std::runtime_error("unbound module " + module);
  };
  const auto text = read_bank(argv[1]);
  const auto bank = JsonReader(text).value();
  std::map<std::string, std::pair<std::size_t, std::size_t>> tally;  // function -> passed, failed
  std::size_t failures = 0, shown = 0;
  std::size_t case_index = 0;
  for (const auto& item : bank.at("cases").items) {
    std::istringstream xes(item.at("xes").text);
    const auto log = import_xes_event_log(xes);
    for (const auto& node : item.at("calls").items) {
      const auto module = node.at("module").text;
      const auto function = node.at("function").text;
      Call call{node, {}, {}};
      for (const auto& arg : node.at("args").items) call.args.push_back(&arg);
      for (const auto& pair : node.at("params").items)
        call.params.emplace_back(pair.items.at(0).text, to_attribute(pair.items.at(1)));
      const auto actual = serialize(outcome(run, module, function, call, log));
      bool matched = false;
      if (const auto* single = node.get("outcome")) {
        matched = serialize(*single) == actual;
      } else {
        for (const auto& observed : node.at("observed").items) matched = matched || serialize(observed) == actual;
      }
      auto& counts = tally[module + "." + function];
      if (matched) {
        ++counts.first;
        continue;
      }
      ++counts.second;
      ++failures;
      if (verbose || shown < 40) {
        ++shown;
        std::cerr << "case " << case_index << " " << module << "." << function << " args "
                  << serialize(node.at("args")) << " params " << serialize(node.at("params")) << "\n  expected "
                  << serialize(node.get("outcome") ? *node.get("outcome") : node.at("observed")) << "\n  actual   "
                  << actual << "\n";
      }
    }
    ++case_index;
  }
  for (const auto& [name, counts] : tally)
    std::cout << name << " " << counts.first << " passed, " << counts.second << " failed\n";
  std::cout << (failures ? "FAILED " : "passed ") << failures << " mismatches\n";
  return failures ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << '\n';
  return 1;
}
