// Pandas attribute, between, case, ltl, paths, timestamp and variants filters
// against the public seeded bank (docs/pandas-filter-bank.json.gz,
// scripts/capture-pandas-filter-bank.py). Each case carries one source
// DataFrame as a native event table and calls of the public callables of
// pm4py.algo.filtering.pandas.{attributes, between, cases, ltl, paths,
// timestamp, variants}. Returned tables compare as their complete native
// carrier (columns, dtypes, index class and labels, every cell), errors as
// class and message. No call may change its source table.
#include "minera/dataframe_format_json.hpp"
#include "minera/pandas_attributes_filter.hpp"
#include "minera/pandas_between_filter.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_ltl_filter.hpp"
#include "minera/pandas_paths_filter.hpp"
#include "minera/pandas_timestamp_filter.hpp"
#include "minera/pandas_variants_filter.hpp"

#include "../gzip_codec.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
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


// ------------------------------------------------------------ typed values

const std::string& tag_of(const Json& node) {
  if (node.kind != Json::kObject || node.fields.size() != 1) throw std::runtime_error("not a typed node: " + serialize(node));
  return node.fields.begin()->first;
}
const Json& body(const Json& node) { return node.fields.begin()->second; }

std::string shortest(double value) {
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
  return std::string(buffer, result.ptr);
}

std::string offset_text(long minutes) {
  char buffer[8];
  const long magnitude = minutes < 0 ? -minutes : minutes;
  std::snprintf(buffer, sizeof buffer, "%c%02ld:%02ld", minutes < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
  return buffer;
}

// A typed argument as the native operand: Python containers become list,
// tuple or mapping attributes, datetimes date attributes (naive or with
// their UTC offset).
EventAttribute to_attribute(const Json& node, std::string key = {}) {
  const auto& tag = tag_of(node);
  const auto& value = body(node);
  EventAttribute out{std::move(key), ""};
  if (tag == "s") {
    out.value = value.text;
  } else if (tag == "i") {
    out.value = value.text;
    out.kind = AttributeKind::kInt;
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
  } else if (tag == "w") {
    auto text = value.items.at(0).text;
    text[10] = 'T';
    if (value.items.at(1).kind != Json::kNull) text += offset_text(std::stol(value.items.at(1).text));
    out.value = text;
    // A Python datetime carries no native payload.
    out.kind = AttributeKind::kDate;
  } else if (tag == "l" || tag == "t") {
    out.container = tag == "l" ? AttributeContainerKind::kList : AttributeContainerKind::kTuple;
    for (const auto& item : value.items) out.children.push_back(to_attribute(item, std::to_string(out.children.size())));
  } else if (tag == "m") {
    out.container = AttributeContainerKind::kContainer;
    for (const auto& pair : value.items) out.children.push_back(to_attribute(pair.items.at(1), body(pair.items.at(0)).text));
  } else {
    throw std::runtime_error("unsupported typed argument tag " + tag);
  }
  return out;
}

// Enum member name -> value of the public Parameters enums.
const std::map<std::string, std::string> kEnumValues = {
    {"ATTRIBUTE_KEY", "pm4py:param:attribute_key"},
    {"ACTIVITY_KEY", "pm4py:param:activity_key"},
    {"CASE_ID_KEY", "pm4py:param:case_id_key"},
    {"TIMESTAMP_KEY", "pm4py:param:timestamp_key"},
    {"RESOURCE_KEY", "pm4py:param:resource_key"},
    {"DECREASING_FACTOR", "decreasingFactor"},
    {"POSITIVE", "positive"},
    {"STREAM_FILTER_KEY1", "stream_filter_key1"},
    {"STREAM_FILTER_VALUE1", "stream_filter_value1"},
    {"STREAM_FILTER_KEY2", "stream_filter_key2"},
    {"STREAM_FILTER_VALUE2", "stream_filter_value2"},
    {"KEEP_ONCE_PER_CASE", "keep_once_per_case"},
    {"KEEP_NAN_VALUES", "keep_nan_values"},
    {"SUBCASE_CONCAT_STR", "subcase_concat_str"},
    {"BUSINESS_HOURS", "business_hours"},
    {"BUSINESS_HOUR_SLOTS", "business_hour_slots"},
    {"WORKCALENDAR", "workcalendar"},
    {"ENABLE_TIMESTAMP", "enable_timestamp"},
    {"TIMESTAMP_DIFF_BOUNDARIES", "timestamp_diff_boundaries"},
    {"TARGET_ATTRIBUTE_KEY", "target_attribute_key"},
    {"MIN_PERFORMANCE", "min_performance"},
    {"MAX_PERFORMANCE", "max_performance"},
};

struct Entry {
  EventAttribute value;  // keyed by the public parameter string
  bool enum_key = false;
};

// A parameters dictionary: "enum:NAME" keys are Parameters members, keyed
// by their value; other keys stay plain strings.
std::vector<Entry> entries_of(const Json& node) {
  std::vector<Entry> out;
  for (const auto& pair : body(node).items) {
    const auto& key = body(pair.items.at(0)).text;
    const bool member = key.rfind("enum:", 0) == 0;
    out.push_back({to_attribute(pair.items.at(1), member ? kEnumValues.at(key.substr(5)) : key), member});
  }
  return out;
}

template <typename Parameter>
std::vector<Parameter> parameters_of(const Json& node) {
  std::vector<Parameter> out;
  for (auto& entry : entries_of(node)) {
    Parameter p;
    p.value = std::move(entry.value);
    p.enum_key = entry.enum_key;
    out.push_back(std::move(p));
  }
  return out;
}

// The single-mapping carrier of the functions that take one: a member is
// read before the plain key equal to its value, so it replaces that entry.
EventAttribute mapping_of(const Json& node) {
  if (tag_of(node) != "m") return to_attribute(node);
  EventAttribute out{"", ""};
  out.container = AttributeContainerKind::kContainer;
  const auto entries = entries_of(node);
  for (const auto& entry : entries) {
    bool shadowed = false;
    for (const auto& other : entries) shadowed = shadowed || (!entry.enum_key && other.enum_key && other.value.key == entry.value.key);
    bool seen = false;
    for (auto& child : out.children)
      if (child.key == entry.value.key) {
        seen = true;
        if (entry.enum_key || !shadowed) child = entry.value;
      }
    if (!seen && (entry.enum_key || !shadowed)) out.children.push_back(entry.value);
  }
  return out;
}

// The bank's table JSON has no typed payload on null cells and spells float
// NaN as "NaN"; native masked frames carry either lexical form.
Json table_node(const EventTable& table) {
  EventTable normalized = table;
  for (auto& row : normalized.rows)
    for (auto& cell : row) {
      if (!cell) continue;
      if (cell->is_null) cell->typed_value = std::monostate{};
      else if (const auto* number = std::get_if<double>(&cell->typed_value); number && std::isnan(*number)) cell->value = "NaN";
    }
  return jtagged("table", jstring(serialize_event_table_input_json(normalized)));
}

// Expected tables travel as the reference's event-table JSON; they compare
// through the native carrier's canonical serialization.
Json normalized(const Json& node) {
  if (node.kind == Json::kObject && node.fields.size() == 1 && node.fields.begin()->first == "table")
    return table_node(parse_event_table_json(serialize(node.fields.begin()->second)));
  Json out = node;
  for (auto& item : out.items) item = normalized(item);
  for (auto& [key, item] : out.fields) item = normalized(item);
  return out;
}

const Json* keyword(const Json& kwargs, const std::string& name) {
  for (const auto& pair : kwargs.items)
    if (pair.items.at(0).text == name) return &pair.items.at(1);
  return nullptr;
}
template <typename Options>
void set(Options& options, EventAttribute Options::*field, const Json& kwargs, const char* name) {
  if (const auto* value = keyword(kwargs, name)) options.*field = to_attribute(*value);
}

using Args = std::vector<const Json*>;
EventAttribute arg(const Args& args, std::size_t i) { return to_attribute(*args.at(i)); }
bool is_mapping(const Json& node) { return tag_of(node) == "m"; }

// ------------------------------------------------------------ modules

Json attributes_module(const std::string& function, const EventTable& df, const Args& args, const Json& kwargs) {
  using P = PandasAttributesFilterParameter;
  const auto with_parameters = [&](std::size_t at, auto&& call) {
    const auto& node = *args.at(at);
    return is_mapping(node) ? call(parameters_of<P>(node)) : call(to_attribute(node));
  };
  if (function == "apply")
    return with_parameters(1, [&](const auto& p) { return table_node(filter_pandas_attributes(df, arg(args, 0), p)); });
  if (function == "apply_events")
    return with_parameters(1, [&](const auto& p) { return table_node(filter_pandas_attributes_events(df, arg(args, 0), p)); });
  if (function == "apply_numeric")
    return with_parameters(2, [&](const auto& p) {
      return table_node(filter_pandas_attributes_numeric(df, arg(args, 0), arg(args, 1), p));
    });
  if (function == "apply_numeric_events")
    return with_parameters(2, [&](const auto& p) {
      return table_node(filter_pandas_attributes_numeric_events(df, arg(args, 0), arg(args, 1), p));
    });
  if (function == "filter_df_relative_occurrence_event_attribute")
    return with_parameters(1, [&](const auto& p) {
      return table_node(filter_pandas_attributes_relative_occurrence(df, arg(args, 0), p));
    });
  PandasAttributesFilterOptions options;
  if (function == "filter_df_on_attribute_values") {
    set(options, &PandasAttributesFilterOptions::case_id_key, kwargs, "case_id_glue");
    set(options, &PandasAttributesFilterOptions::attribute_key, kwargs, "attribute_key");
    set(options, &PandasAttributesFilterOptions::positive, kwargs, "positive");
    set(options, &PandasAttributesFilterOptions::keep_nan_values, kwargs, "keep_nan_values");
    return table_node(filter_pandas_attributes_on_values(df, arg(args, 0), options));
  }
  if (function == "filter_df_keeping_activ_exc_thresh") {
    set(options, &PandasAttributesFilterOptions::activity_key, kwargs, "activity_key");
    if (const auto* counts = keyword(kwargs, "act_count0")) {
      if (is_mapping(*counts)) {
        PandasAttributesCountMap map;
        for (const auto& pair : body(*counts).items) map.entries.emplace_back(to_attribute(pair.items.at(0)), to_attribute(pair.items.at(1)));
        options.act_count0 = map;
      } else {
        options.act_count0 = to_attribute(*counts);
      }
    }
    if (const auto* variant = keyword(kwargs, "most_common_variant")) options.most_common_variant = to_attribute(*variant);
    return table_node(filter_pandas_attributes_threshold(df, arg(args, 0), options));
  }
  if (function == "filter_df_keeping_spno_activities") {
    set(options, &PandasAttributesFilterOptions::activity_key, kwargs, "activity_key");
    const auto* maximum = keyword(kwargs, "max_no_activities");
    return table_node(maximum ? filter_pandas_attributes_top_activities(df, to_attribute(*maximum), options)
                              : filter_pandas_attributes_top_activities(df, EventAttribute{"", "25", AttributeKind::kInt}, options));
  }
  throw std::runtime_error("unbound function " + function);
}

Json between_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(filter_pandas_between(df, arg(args, 0), arg(args, 1), mapping_of(*args.at(2))));
  throw std::runtime_error("unbound function " + function);
}

Json cases_module(const std::string& function, const EventTable& df, const Args& args, const Json& kwargs) {
  if (function == "filter_case_performance")
    return table_node(filter_pandas_case_performance(df, arg(args, 0), arg(args, 1), mapping_of(*args.at(2))));
  if (function == "apply") return table_node(apply_pandas_case_filter(df, mapping_of(*args.at(0))));
  if (function == "apply_auto_filter") return table_node(auto_filter_pandas_cases(df, mapping_of(*args.at(0))));
  PandasCaseFilterOptions options;
  set(options, &PandasCaseFilterOptions::case_id_key, kwargs, "case_id_glue");
  const auto keyword_or = [&](const char* name, EventAttribute fallback) {
    const auto* found = keyword(kwargs, name);
    return found ? to_attribute(*found) : fallback;
  };
  if (function == "filter_on_ncases")
    return table_node(filter_pandas_ncases(df, keyword_or("max_no_cases", {"", "1000", AttributeKind::kInt}), options));
  if (function == "filter_on_case_size")
    return table_node(filter_pandas_case_size(df, keyword_or("min_case_size", {"", "2", AttributeKind::kInt}),
                                              keyword_or("max_case_size", {"", "", AttributeKind::kString, true}), options));
  if (function == "filter_on_case_performance") {
    set(options, &PandasCaseFilterOptions::timestamp_key, kwargs, "timestamp_key");
    set(options, &PandasCaseFilterOptions::business_hours, kwargs, "business_hours");
    return table_node(filter_pandas_case_performance(df, keyword_or("min_case_performance", {"", "0", AttributeKind::kInt}),
                                                     keyword_or("max_case_performance", {"", "10000000000", AttributeKind::kInt}),
                                                     options));
  }
  throw std::runtime_error("unbound function " + function);
}

Json ltl_module(const std::string& function, const EventTable& df, const Args& args) {
  using P = PandasLtlFilterParameter;
  const auto& last = *args.back();
  const auto run = [&](auto&& call) { return is_mapping(last) ? call(parameters_of<P>(last)) : call(to_attribute(last)); };
  if (function == "eventually_follows")
    return run([&](const auto& p) { return table_node(filter_pandas_ltl_eventually_follows(df, arg(args, 0), p)); });
  if (function == "A_next_B_next_C")
    return run([&](const auto& p) { return table_node(filter_pandas_ltl_next(df, arg(args, 0), arg(args, 1), arg(args, 2), p)); });
  if (function == "four_eyes_principle")
    return run([&](const auto& p) { return table_node(filter_pandas_ltl_four_eyes(df, arg(args, 0), arg(args, 1), p)); });
  if (function == "attr_value_different_persons")
    return run([&](const auto& p) { return table_node(filter_pandas_ltl_different_persons(df, arg(args, 0), p)); });
  throw std::runtime_error("unbound function " + function);
}

Json paths_module(const std::string& function, const EventTable& df, const Args& args) {
  using P = PandasPathsFilterParameter;
  const auto& last = *args.at(1);
  const auto run = [&](auto&& call) { return is_mapping(last) ? call(parameters_of<P>(last)) : call(to_attribute(last)); };
  if (function == "apply") return run([&](const auto& p) { return table_node(filter_pandas_paths(df, arg(args, 0), p)); });
  if (function == "apply_performance")
    return run([&](const auto& p) { return table_node(filter_pandas_path_performance(df, arg(args, 0), p)); });
  throw std::runtime_error("unbound function " + function);
}

Json timestamp_module(const std::string& function, const EventTable& df, const Args& args) {
  using P = PandasTimestampFilterParameter;
  if (function == "apply") return table_node(apply_pandas_timestamp_filter(df, mapping_of(*args.at(0))));
  if (function == "apply_auto_filter") return table_node(auto_filter_pandas_timestamp(df, mapping_of(*args.at(0))));
  const auto& last = *args.back();
  const auto run = [&](auto&& call) { return is_mapping(last) ? call(parameters_of<P>(last)) : call(to_attribute(last)); };
  if (function == "filter_traces_attribute_in_timeframe")
    return run([&](const auto& p) {
      return table_node(filter_pandas_timestamp_attribute(df, arg(args, 0), arg(args, 1), arg(args, 2), arg(args, 3), p));
    });
  const auto lo = arg(args, 0), hi = arg(args, 1);
  if (function == "filter_traces_contained")
    return run([&](const auto& p) { return table_node(filter_pandas_timestamp_contained(df, lo, hi, p)); });
  if (function == "filter_traces_intersecting")
    return run([&](const auto& p) { return table_node(filter_pandas_timestamp_intersecting(df, lo, hi, p)); });
  if (function == "apply_events")
    return run([&](const auto& p) { return table_node(filter_pandas_timestamp_events(df, lo, hi, p)); });
  if (function == "filter_traces_starting_in_timeframe")
    return run([&](const auto& p) { return table_node(filter_pandas_timestamp_starting(df, lo, hi, p)); });
  if (function == "filter_traces_completing_in_timeframe")
    return run([&](const auto& p) { return table_node(filter_pandas_timestamp_completing(df, lo, hi, p)); });
  throw std::runtime_error("unbound function " + function);
}

Json variants_module(const std::string& function, const EventTable& df, const Args& args) {
  using P = PandasVariantsFilterParameter;
  const auto& last = *args.at(1);
  const auto run = [&](auto&& call) { return is_mapping(last) ? call(parameters_of<P>(last)) : call(to_attribute(last)); };
  if (function == "apply") return run([&](const auto& p) { return table_node(filter_pandas_variants(df, arg(args, 0), p)); });
  if (function == "filter_variants_top_k")
    return run([&](const auto& p) { return table_node(filter_pandas_variants_top_k(df, arg(args, 0), p)); });
  if (function == "filter_variants_by_coverage_percentage")
    return run([&](const auto& p) { return table_node(filter_pandas_variants_min_coverage(df, arg(args, 0), p)); });
  if (function == "filter_variants_by_maximum_coverage_percentage")
    return run([&](const auto& p) { return table_node(filter_pandas_variants_max_coverage(df, arg(args, 0), p)); });
  throw std::runtime_error("unbound function " + function);
}

Json error_node(const std::string& type, const std::string& message) {
  Json out;
  out.kind = Json::kObject;
  out.fields.emplace("error", jarray({jstring(type), jstring(message)}));
  return out;
}

// The value of a call or the public failure it raised.
template <typename Call>
Json run_call(Call&& call) {
  try {
    Json out;
    out.kind = Json::kObject;
    out.fields.emplace("value", call());
    return out;
  } catch (const PandasAttributesFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PandasBetweenError& error) {
    return error_node(error.type, error.message);
  } catch (const PandasCaseFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PandasLtlFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PandasPathsFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PandasTimestampFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PandasVariantsFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const std::exception& error) {
    return error_node("native", error.what());
  }
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
  const auto text = read_bank(argv[1]);
  const auto bank = JsonReader(text).value();
  std::map<std::string, std::pair<std::size_t, std::size_t>> tally;  // function -> passed, failed
  std::size_t failures = 0, shown = 0;
  std::size_t case_index = 0;
  for (const auto& item : bank.at("cases").items) {
    ++case_index;
    const auto df = [&] {
      try {
        return parse_event_table_json(serialize(item.at("source")));
      } catch (const std::exception& error) {
        throw std::runtime_error("source of case " + std::to_string(case_index - 1) + ": " + error.what());
      }
    }();
    for (const auto& node : item.at("calls").items) {
      const auto qualified = node.at("function").text;
      const auto dot = qualified.find('.');
      const auto module = qualified.substr(0, dot);
      const auto function = qualified.substr(dot + 1);
      Args args;
      for (const auto& arg : node.at("args").items) args.push_back(&arg);
      const auto& kwargs = node.at("kwargs");
      const auto actual = run_call([&]() -> Json {
        if (module == "attributes") return attributes_module(function, df, args, kwargs);
        if (module == "between") return between_module(function, df, args);
        if (module == "cases") return cases_module(function, df, args, kwargs);
        if (module == "ltl") return ltl_module(function, df, args);
        if (module == "paths") return paths_module(function, df, args);
        if (module == "timestamp") return timestamp_module(function, df, args);
        if (module == "variants") return variants_module(function, df, args);
        throw std::runtime_error("unbound module " + module);
      });
      std::vector<Json> expected;
      try {
        if (const auto* single = node.get("outcome")) expected.push_back(normalized(*single));
        else
          for (const auto& observed : node.at("observed").items) expected.push_back(normalized(observed));
      } catch (const std::exception& error) {
        throw std::runtime_error(qualified + " case " + std::to_string(case_index - 1) + ": " + error.what());
      }
      bool matched = false;
      for (const auto& e : expected) matched = matched || serialize(e) == serialize(actual);
      auto& counts = tally[qualified];
      if (matched) {
        ++counts.first;
        continue;
      }
      ++counts.second;
      ++failures;
      if (verbose || shown < 30) {
        ++shown;
        const auto e = serialize(expected.front()), a = serialize(actual);
        std::size_t at = 0;
        while (at < e.size() && at < a.size() && e[at] == a[at]) ++at;
        const auto from = at > 200 ? at - 200 : 0;
        std::cerr << qualified << " args " << serialize(node.at("args")).substr(0, 400) << " kwargs "
                  << serialize(kwargs).substr(0, 300) << "\n  case " << case_index - 1 << " rows " << df.size()
                  << "\n  expected ..." << e.substr(from, 600) << "\n  actual   ..." << a.substr(from, 600) << "\n";
      }
    }
  }
  for (const auto& [name, counts] : tally)
    std::cout << name << " " << counts.first << " passed, " << counts.second << " failed\n";
  std::cout << (failures ? "FAILED " : "passed ") << failures << " mismatches\n";
  return failures ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
