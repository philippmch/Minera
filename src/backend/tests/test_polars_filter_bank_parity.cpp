// Polars attribute, between, case, ltl, paths, timestamp and variants filters
// against the public seeded bank (docs/polars-filter-bank.json.gz,
// scripts/capture-polars-filter-bank.py). Each case carries one source Polars
// frame as a native event table and calls of the public callables of
// pm4py.algo.filtering.polars.{attributes, between, cases, ltl, paths,
// timestamp, variants} on a LazyFrame. Returned frames compare as their
// complete native carrier (columns, dtypes, every cell), other values as
// typed JSON, errors as class and message (Polars plan text included). A call
// marked "unordered" (its reference rows or dict entries come in a different
// order on every run) compares with table rows and dict entries sorted.
#include "minera/dataframe_format_json.hpp"
#include "minera/polars_attributes_filter.hpp"
#include "minera/polars_between_filter.hpp"
#include "minera/polars_case_filter.hpp"
#include "minera/polars_ltl_filter.hpp"
#include "minera/polars_paths_filter.hpp"
#include "minera/polars_sequence_filters.hpp"
#include "minera/polars_timestamp_filter.hpp"
#include "minera/polars_variants_filter.hpp"
#include "minera/timestamp.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <cstdlib>
#include <optional>
#include <regex>
#include <set>
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
        if (c == '\n') {
          out += "\\n";
          continue;
        }
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
    // Round CE: a Python int above the Int64 range arrives as UInt64.
    std::int64_t signed_value = 0;
    std::uint64_t unsigned_value = 0;
    const auto* first = value.text.data();
    const auto* last = first + value.text.size();
    if (const auto r = std::from_chars(first, last, signed_value); r.ec == std::errc{} && r.ptr == last)
      out.typed_value = signed_value;
    else if (const auto u = std::from_chars(first, last, unsigned_value); u.ec == std::errc{} && u.ptr == last)
      out.typed_value = unsigned_value;
    else
      throw std::runtime_error("integer argument outside the UInt64 range: " + value.text);
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
    p.key = entry.value.key;
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

// Round CD: a frame without categorical columns carries either no
// categorical dtypes or an empty entry per column (between keeps the
// entries of a categorical case id it casts to String); both are one frame.
Json table_node(const EventTable& table) {
  if (std::none_of(table.categorical_dtypes.begin(), table.categorical_dtypes.end(),
                   [](const auto& c) { return c.has_value(); }) && !table.categorical_dtypes.empty()) {
    auto plain = table;
    plain.categorical_dtypes.clear();
    return jtagged("table", jstring(serialize_event_table_input_json(plain)));
  }
  return jtagged("table", jstring(serialize_event_table_input_json(table)));
}
Json table_node(const LazyEventTable& table) { return table_node(table.materialize()); }

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

// Round CD: the statistics filters keep the values of a Polars group_by
// without maintain_order; a failing is_in plan prints them in that
// (unspecified) order. Their items compare as a set, and a list Polars
// shortens (first two, last) only by its shape.
std::string unordered_is_in_text(std::string text) {
  for (std::size_t at = text.find("is_in([["); at != std::string::npos; at = text.find("is_in([[", at + 1)) {
    const auto begin = at + 8;
    const auto end = text.find("]])", begin);
    if (end == std::string::npos) break;
    const auto inner = text.substr(begin, end - begin);
    std::string replaced;
    if (inner.find("\xe2\x80\xa6") != std::string::npos) replaced = "\xe2\x80\xa6";
    else {
      std::vector<std::string> items;
      for (std::size_t from = 0;;) {
        const auto comma = inner.find(", ", from);
        items.push_back(inner.substr(from, comma == std::string::npos ? std::string::npos : comma - from));
        if (comma == std::string::npos) break;
        from = comma + 2;
      }
      std::sort(items.begin(), items.end());
      for (std::size_t i = 0; i < items.size(); ++i) replaced += (i ? ", " : "") + items[i];
    }
    text.replace(begin, end - begin, replaced);
  }
  return text;
}
Json unordered_is_in(const Json& node) {
  Json out = node;
  if (out.kind == Json::kString) out.text = unordered_is_in_text(out.text);
  for (auto& item : out.items) item = unordered_is_in(item);
  for (auto& [key, item] : out.fields) item = unordered_is_in(item);
  return out;
}

// A normalized outcome with table rows (unless dicts_only) and dict entries
// in a fixed order.
Json canonical(const Json& node, bool dicts_only = false) {
  if (!dicts_only && node.kind == Json::kObject && node.fields.size() == 1 && node.fields.begin()->first == "table") {
    auto table = parse_event_table_json(node.fields.begin()->second.text);
    std::vector<std::pair<std::string, EventTable::Row>> keyed;
    for (auto& row : table.rows) {
      EventTable one = table;
      one.rows = {row};
      keyed.emplace_back(serialize_event_table_input_json(one), std::move(row));
    }
    std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    table.rows.clear();
    for (auto& [key, row] : keyed) table.rows.push_back(std::move(row));
    return table_node(table);
  }
  Json out = node;
  for (auto& item : out.items) item = canonical(item, dicts_only);
  for (auto& [key, item] : out.fields) item = canonical(item, dicts_only);
  if (out.kind == Json::kObject && out.fields.size() == 1 && out.fields.begin()->first == "m") {
    auto& entries = out.fields.begin()->second.items;
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Json& a, const Json& b) { return serialize(a) < serialize(b); });
  }
  return out;
}

// The between result joins its segments back without an order: its cases
// come in another order on some runs (round CD: 14 of 180 calls varied over
// six runs, case kinds str, int, Categorical, Float32, UInt32, Datetime),
// each case's rows staying in event order. So, rarely, do the cases kept by
// attr_value_different_persons (1 of 200 calls; seed 4243 case 201 gave
// cases [1, 2] in 7 of 16 runs, [2, 1] in 9). Rows sort stably by case id.
Json case_blocks(const Json& node, const std::string& key) {
  if (node.kind == Json::kObject && node.fields.size() == 1 && node.fields.begin()->first == "table") {
    auto table = parse_event_table_json(node.fields.begin()->second.text);
    const auto at = std::find(table.columns.begin(), table.columns.end(), key);
    if (at == table.columns.end()) return node;
    const auto c = static_cast<std::size_t>(at - table.columns.begin());
    const auto id = [c](const auto& row) { return row[c] ? row[c]->value : std::string(); };
    std::stable_sort(table.rows.begin(), table.rows.end(), [&](const auto& a, const auto& b) { return id(a) < id(b); });
    return table_node(table);
  }
  Json out = node;
  for (auto& item : out.items) item = case_blocks(item, key);
  for (auto& [name, item] : out.fields) item = case_blocks(item, key);
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

Json typed(const std::string& tag, std::string text) { return jtagged(tag, jstring(std::move(text))); }

// A result value the capture cannot encode (its TypeError "unrepresentable ...").
struct Unrepresentable : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// A native scalar as the reference's typed JSON.
Json typed_attribute(const EventAttribute& v) {
  if (v.is_null) {
    Json null;
    return jtagged("n", null);
  }
  if (const auto* b = std::get_if<bool>(&v.typed_value)) {
    Json out;
    out.kind = Json::kBool;
    out.boolean = *b;
    return jtagged("b", out);
  }
  if (const auto* i = std::get_if<std::int64_t>(&v.typed_value)) return typed("i", std::to_string(*i));
  if (const auto* u = std::get_if<std::uint64_t>(&v.typed_value)) return typed("i", std::to_string(*u));
  if (const auto* d = std::get_if<double>(&v.typed_value)) return typed("f", bits_hex(*d));
  if (v.kind == AttributeKind::kInt) return typed("i", v.value);
  if (v.kind == AttributeKind::kFloat) return typed("f", bits_hex(std::stod(v.value)));
  if (v.kind == AttributeKind::kBoolean) {
    Json out;
    out.kind = Json::kBool;
    out.boolean = v.value == "true";
    return jtagged("b", out);
  }
  // Round CD: the capture encodes a datetime.date as unrepresentable and an
  // aware datetime as its UTC isoformat ({"d": ...}).
  const auto* native_date = std::get_if<NativeDate>(&v.typed_value);
  const auto& date_text = native_date ? native_date->value : v.value;
  if (v.kind == AttributeKind::kDate && date_text.size() == 10) throw Unrepresentable("unrepresentable date");
  if (v.kind == AttributeKind::kDate && date_text.size() > 19 &&
      (date_text.back() == 'Z' || date_text[date_text.size() - 3] == ':')) {
    if (const auto parsed = parse_timestamp_exact(date_text); parsed && parsed->timezone_aware) {
      const auto days = parsed->seconds >= 0 ? parsed->seconds / 86'400 : -((-parsed->seconds + 86'399) / 86'400);
      const auto clock = parsed->seconds - days * 86'400;
      // Proleptic Gregorian civil date of a day number.
      const std::int64_t z = days + 719'468;
      const std::int64_t era = (z >= 0 ? z : z - 146'096) / 146'097;
      const auto doe = static_cast<unsigned>(z - era * 146'097);
      const unsigned yoe = (doe - doe / 1460 + doe / 36'524 - doe / 146'096) / 365;
      const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
      const unsigned mp = (5 * doy + 2) / 153;
      const unsigned day = doy - (153 * mp + 2) / 5 + 1;
      const unsigned month = mp < 10 ? mp + 3 : mp - 9;
      const std::int64_t year = static_cast<std::int64_t>(yoe) + era * 400 + (month <= 2);
      char buffer[64];
      std::snprintf(buffer, sizeof buffer, "%04lld-%02u-%02u %02lld:%02lld:%02lld", static_cast<long long>(year), month,
                    day, static_cast<long long>(clock / 3600), static_cast<long long>(clock / 60 % 60),
                    static_cast<long long>(clock % 60));
      std::string text = buffer;
      if (parsed->nanoseconds / 1000) {
        std::snprintf(buffer, sizeof buffer, ".%06u", parsed->nanoseconds / 1000);
        text += buffer;
      }
      return typed("d", text + "+00:00");
    }
  }
  if (v.kind == AttributeKind::kDate && v.value.size() >= 19) {
    // [wall clock with microseconds, UTC offset minutes or null]
    std::string wall = v.value.substr(0, 19), rest = v.value.substr(19);
    wall[10] = ' ';
    std::string fraction = ".000000";
    if (!rest.empty() && rest[0] == '.') {
      const auto end = rest.find_first_not_of("0123456789", 1);
      fraction = (rest.substr(0, end) + "000000").substr(0, 7);
      rest = end == std::string::npos ? "" : rest.substr(end);
    }
    Json offset;
    if (!rest.empty()) {
      const int minutes = std::stoi(rest.substr(1, 2)) * 60 + std::stoi(rest.substr(4, 2));
      offset = jstring(std::to_string(rest[0] == '-' ? -minutes : minutes));
      offset.kind = Json::kNumber;
    }
    return jtagged("w", jarray({jstring(wall + fraction), offset}));
  }
  return typed("s", v.value);
}

Json counts_node(const PolarsAttributeCounts& counts) {
  std::vector<Json> items;
  for (const auto& entry : counts) items.push_back(jarray({typed_attribute(entry.value), typed_attribute(entry.count)}));
  return jtagged("m", jarray(std::move(items)));
}

// ------------------------------------------------------------ modules

Json constant(const std::string& name) {
  if (name == "CASE_CONCEPT_NAME") return typed("s", std::string(kPolarsFilterCaseConceptName));
  if (name == "DEFAULT_NAME_KEY") return typed("s", std::string(kPolarsFilterDefaultNameKey));
  if (name == "DEFAULT_TIMESTAMP_KEY") return typed("s", std::string(kPolarsFilterDefaultTimestampKey));
  if (name == "DEFAULT_RESOURCE_KEY") return typed("s", std::string(kPolarsFilterDefaultResourceKey));
  if (name == "DEFAULT_VARIANT_SEP") return typed("s", std::string(kPolarsFilterDefaultVariantSep));
  if (name == "PARAMETER_CONSTANT_CASEID_KEY") return typed("s", std::string(kPolarsFilterCaseIdKeyParameter));
  if (name == "PARAMETER_CONSTANT_ACTIVITY_KEY") return typed("s", std::string(kPolarsFilterActivityKeyParameter));
  if (name == "PARAMETER_CONSTANT_ATTRIBUTE_KEY") return typed("s", std::string(kPolarsFilterAttributeKeyParameter));
  if (name == "PARAMETER_CONSTANT_TIMESTAMP_KEY") return typed("s", std::string(kPolarsFilterTimestampKeyParameter));
  if (name == "PARAMETER_CONSTANT_RESOURCE_KEY") return typed("s", std::string(kPolarsFilterResourceKeyParameter));
  throw std::runtime_error("unbound constant " + name);
}
bool is_constant(const std::string& name) { return !name.empty() && std::all_of(name.begin(), name.end(), [](char c) { return c == '_' || (c >= 'A' && c <= 'Z'); }); }

Json attributes_module(const std::string& function, const LazyEventTable& df, const Args& args, const Json& kwargs) {
  if (is_constant(function)) return constant(function);
  using Op = PolarsAttributesFilterOperation;
  PolarsAttributesFilterCall call;
  const auto parameters_at = [&](std::size_t at) {
    const auto& node = *args.at(at);
    if (is_mapping(node)) call.parameters = parameters_of<PolarsAttributesFilterParameter>(node);
    else call.parameter_value = to_attribute(node);
  };
  const auto run = [&]() -> Json {
    const auto result = invoke_polars_attributes_filter(df, call);
    if (const auto* counts = std::get_if<PolarsAttributeCounts>(&result)) return counts_node(*counts);
    return table_node(std::get<LazyEventTable>(result));
  };
  if (function == "apply" || function == "apply_events") {
    call.operation = function == "apply" ? Op::kTraces : Op::kEvents;
    call.values = arg(args, 0);
    parameters_at(1);
    return run();
  }
  if (function == "apply_numeric" || function == "apply_numeric_events") {
    call.operation = function == "apply_numeric" ? Op::kNumericTraces : Op::kNumericEvents;
    call.lower = arg(args, 0);
    call.upper = arg(args, 1);
    parameters_at(2);
    return run();
  }
  if (function == "filter_df_relative_occurrence_event_attribute") {
    call.operation = Op::kRelative;
    call.stake = arg(args, 0);
    parameters_at(1);
    return run();
  }
  if (function == "get_attribute_values") {
    call.operation = Op::kCounts;
    call.attribute = arg(args, 0);
    parameters_at(1);
    return run();
  }
  auto& options = call.options;
  if (function == "filter_df_on_attribute_values") {
    call.operation = Op::kOnValues;
    call.values = arg(args, 0);
    set(options, &PolarsAttributesFilterOptions::case_id_key, kwargs, "case_id_glue");
    set(options, &PolarsAttributesFilterOptions::attribute_key, kwargs, "attribute_key");
    set(options, &PolarsAttributesFilterOptions::positive, kwargs, "positive");
    return run();
  }
  if (function == "filter_df_keeping_activ_exc_thresh") {
    call.operation = Op::kThreshold;
    call.threshold = arg(args, 0);
    set(options, &PolarsAttributesFilterOptions::activity_key, kwargs, "activity_key");
    if (const auto* counts = keyword(kwargs, "act_count0")) {
      if (is_mapping(*counts)) {
        PolarsAttributeCounts map;
        for (const auto& pair : body(*counts).items) map.push_back({to_attribute(pair.items.at(0)), to_attribute(pair.items.at(1))});
        call.count_cache = map;
      } else {
        call.count_cache = to_attribute(*counts);
      }
    }
    if (const auto* variant = keyword(kwargs, "most_common_variant")) call.most_common_variant = to_attribute(*variant);
    return run();
  }
  if (function == "filter_df_keeping_spno_activities") {
    call.operation = Op::kTopActivities;
    set(options, &PolarsAttributesFilterOptions::activity_key, kwargs, "activity_key");
    const auto* maximum = keyword(kwargs, "max_no_activities");
    call.maximum = maximum ? to_attribute(*maximum) : EventAttribute{"", "25", AttributeKind::kInt};
    return run();
  }
  throw std::runtime_error("unbound function " + function);
}

Json between_module(const std::string& function, const LazyEventTable& df, const Args& args) {
  if (function == "apply") {
    const auto& last = *args.at(2);
    const auto options = is_mapping(last) ? resolve_polars_between_parameters(parameters_of<PolarsBetweenParameter>(last))
                                          : resolve_polars_between_parameter_value(to_attribute(last));
    return table_node(filter_polars_between(df, arg(args, 0), arg(args, 1), options));
  }
  throw std::runtime_error("unbound function " + function);
}

Json cases_module(const std::string& function, const LazyEventTable& df, const Args& args, const Json& kwargs) {
  using P = PolarsCaseFilterParameter;
  if (function == "filter_case_performance") {
    const auto& last = *args.at(2);
    return table_node(is_mapping(last) ? filter_polars_case_performance_parameters(df, arg(args, 0), arg(args, 1), parameters_of<P>(last))
                                       : filter_polars_case_performance_parameter_value(df, arg(args, 0), arg(args, 1), to_attribute(last)));
  }
  if (function == "apply") return table_node(apply_polars_case_filter(df, mapping_of(*args.at(0))));
  if (function == "apply_auto_filter") return table_node(auto_filter_polars_cases(df, mapping_of(*args.at(0))));
  PolarsCaseFilterOptions options;
  set(options, &PolarsCaseFilterOptions::case_id_key, kwargs, "case_id_glue");
  const auto keyword_or = [&](const char* name, EventAttribute fallback) {
    const auto* found = keyword(kwargs, name);
    return found ? to_attribute(*found) : fallback;
  };
  if (function == "filter_on_ncases")
    return table_node(filter_polars_ncases(df, keyword_or("max_no_cases", {"", "1000", AttributeKind::kInt}), options));
  if (function == "filter_on_case_size") {
    EventAttribute none;
    none.is_null = true;
    return table_node(filter_polars_case_size(df, keyword_or("min_case_size", {"", "2", AttributeKind::kInt}),
                                              keyword_or("max_case_size", none), options));
  }
  if (function == "filter_on_case_performance") {
    set(options, &PolarsCaseFilterOptions::timestamp_key, kwargs, "timestamp_key");
    set(options, &PolarsCaseFilterOptions::business_hours, kwargs, "business_hours");
    return table_node(filter_polars_case_performance(df, keyword_or("min_case_performance", {"", "0", AttributeKind::kInt}),
                                                     keyword_or("max_case_performance", {"", "10000000000", AttributeKind::kInt}),
                                                     options));
  }
  throw std::runtime_error("unbound function " + function);
}

Json ltl_module(const std::string& function, const LazyEventTable& df, const Args& args) {
  if (is_constant(function)) return constant(function);
  using Op = PolarsLtlFilterOperation;
  PolarsLtlFilterCall call;
  if (function == "eventually_follows") call.operation = Op::kEventuallyFollows;
  else if (function == "A_next_B_next_C") call.operation = Op::kNext;
  else if (function == "four_eyes_principle") call.operation = Op::kFourEyes;
  else if (function == "attr_value_different_persons") call.operation = Op::kDifferentPersons;
  else throw std::runtime_error("unbound function " + function);
  call.values = arg(args, 0);
  const auto& last = *args.at(1);
  if (is_mapping(last)) call.parameters = parameters_of<PolarsLtlFilterParameter>(last);
  else call.parameter_value = to_attribute(last);
  return table_node(invoke_polars_ltl_filter(df, call));
}

Json paths_module(const std::string& function, const LazyEventTable& df, const Args& args) {
  if (is_constant(function)) return constant(function);
  using P = PolarsPathsFilterParameter;
  const auto& last = *args.at(1);
  if (function == "apply")
    return table_node(is_mapping(last) ? filter_polars_paths_parameters(df, arg(args, 0), parameters_of<P>(last))
                                       : filter_polars_paths_parameter_value(df, arg(args, 0), to_attribute(last)));
  if (function == "apply_performance")
    return table_node(is_mapping(last) ? filter_polars_path_performance_parameters(df, arg(args, 0), parameters_of<P>(last))
                                       : filter_polars_path_performance_parameter_value(df, arg(args, 0), to_attribute(last)));
  throw std::runtime_error("unbound function " + function);
}

Json timestamp_module(const std::string& function, const LazyEventTable& df, const Args& args) {
  if (is_constant(function)) return constant(function);
  using P = PolarsTimestampFilterParameter;
  if (function == "apply") return table_node(apply_polars_timestamp_filter(df, mapping_of(*args.at(0))));
  if (function == "apply_auto_filter") return table_node(auto_filter_polars_timestamp(df, mapping_of(*args.at(0))));
  const auto& last = *args.back();
  const bool mapping = is_mapping(last);
  if (function == "filter_traces_attribute_in_timeframe") {
    const auto a = arg(args, 0), v = arg(args, 1), lo = arg(args, 2), hi = arg(args, 3);
    return table_node(mapping ? filter_polars_timestamp_attribute_parameters(df, a, v, lo, hi, parameters_of<P>(last))
                              : filter_polars_timestamp_attribute_parameter_value(df, a, v, lo, hi, to_attribute(last)));
  }
  const auto lo = arg(args, 0), hi = arg(args, 1);
  if (function == "filter_traces_contained")
    return table_node(mapping ? filter_polars_timestamp_contained_parameters(df, lo, hi, parameters_of<P>(last))
                              : filter_polars_timestamp_contained_parameter_value(df, lo, hi, to_attribute(last)));
  if (function == "filter_traces_intersecting")
    return table_node(mapping ? filter_polars_timestamp_intersecting_parameters(df, lo, hi, parameters_of<P>(last))
                              : filter_polars_timestamp_intersecting_parameter_value(df, lo, hi, to_attribute(last)));
  if (function == "apply_events")
    return table_node(mapping ? filter_polars_timestamp_events_parameters(df, lo, hi, parameters_of<P>(last))
                              : filter_polars_timestamp_events_parameter_value(df, lo, hi, to_attribute(last)));
  throw std::runtime_error("unbound function " + function);
}

Json variants_module(const std::string& function, const LazyEventTable& df, const Args& args) {
  if (is_constant(function)) return constant(function);
  using P = PolarsVariantsFilterParameter;
  const auto& last = *args.back();
  const bool mapping = is_mapping(last);
  if (function == "apply_auto_filter")
    return table_node(mapping ? auto_filter_polars_variants_parameters(df, parameters_of<P>(last))
                              : auto_filter_polars_variants_parameter_value(df, to_attribute(last)));
  const auto first = arg(args, 0);
  if (function == "apply")
    return table_node(mapping ? filter_polars_variants_parameters(df, first, parameters_of<P>(last))
                              : filter_polars_variants_parameter_value(df, first, to_attribute(last)));
  if (function == "apply_from_variant_list")
    return table_node(mapping ? filter_polars_variants_from_list_parameters(df, first, parameters_of<P>(last))
                              : filter_polars_variants_from_list_parameter_value(df, first, to_attribute(last)));
  if (function == "filter_variants_top_k")
    return table_node(mapping ? filter_polars_variants_top_k_parameters(df, first, parameters_of<P>(last))
                              : filter_polars_variants_top_k_parameter_value(df, first, to_attribute(last)));
  throw std::runtime_error("unbound function " + function);
}

const EventTable* table_of(const Json& outcome, EventTable& storage) {
  const auto* value = outcome.get("value");
  if (!value || value->kind != Json::kObject || !value->get("table")) return nullptr;
  storage = parse_event_table_json(value->at("table").text);
  return &storage;
}

// Two top-activity selections are equivalent when they keep as many
// activities with the same counts, each the source rows of its activities.
// Both outputs keep the source rows of as many values of `name`, with the
// same row counts.
// Round CF: filter_on_ncases keeps any k of the distinct case ids, a null
// id among them (which keeps no rows) included: seed 2026111460 case 346
// kept {2}, {3} or {2, 3} of {null, 2, 3} over 24 reference runs. The
// actual table must be the frame's rows of k such ids, or of k - 1 when the
// frame has a null id.
bool valid_ncases(const EventTable& df, const std::string& name, const Json& actual, long long k) {
  EventTable a_storage;
  const auto* a = table_of(actual, a_storage);
  if (!a || k < 0) return false;
  const auto at = std::find(df.columns.begin(), df.columns.end(), name);
  if (at == df.columns.end()) return false;
  const auto column = static_cast<std::size_t>(at - df.columns.begin());
  const auto label = [&](const EventTable::Row& row) {
    return row[column] && !row[column]->is_null ? "v" + row[column]->value : std::string("n");
  };
  std::set<std::string> distinct, kept;
  for (const auto& row : df.rows) distinct.insert(label(row));
  for (const auto& row : a->rows) kept.insert(label(row));
  if (kept.count("n")) return false;
  const auto want = std::min<std::size_t>(static_cast<std::size_t>(k), distinct.size());
  if (kept.size() != want && !(distinct.count("n") && want > 0 && kept.size() == want - 1)) return false;
  EventTable rebuilt = df;
  rebuilt.rows.clear();
  for (const auto& row : df.rows)
    if (kept.count(label(row))) rebuilt.rows.push_back(row);
  return serialize_event_table_input_json(rebuilt) == serialize_event_table_input_json(*a);
}
bool same_selection(const EventTable& df, const std::string& name, const Json& expected, const Json& actual) {
  EventTable e_storage, a_storage;
  const auto* e = table_of(expected, e_storage);
  const auto* a = table_of(actual, a_storage);
  if (!e || !a) return false;
  const auto at = std::find(df.columns.begin(), df.columns.end(), name);
  if (at == df.columns.end()) return false;
  const auto column = static_cast<std::size_t>(at - df.columns.begin());
  const auto label = [&](const EventTable::Row& row) {
    return row[column] && !row[column]->is_null ? "v" + row[column]->value : std::string("n");
  };
  std::map<std::string, std::size_t> counts;
  for (const auto& row : df.rows) ++counts[label(row)];
  const auto kept = [&](const EventTable& table) {
    std::map<std::string, bool> out;
    for (const auto& row : table.rows) out[label(row)] = true;
    return out;
  };
  const auto ek = kept(*e), ak = kept(*a);
  std::vector<std::size_t> ec, ac;
  for (const auto& [k, _] : ek) ec.push_back(counts[k]);
  for (const auto& [k, _] : ak) ac.push_back(counts[k]);
  std::sort(ec.begin(), ec.end());
  std::sort(ac.begin(), ac.end());
  if (ec != ac) return false;
  EventTable rebuilt = df;
  rebuilt.rows.clear();
  for (const auto& row : df.rows)
    if (ek.count(label(row))) rebuilt.rows.push_back(row);
  return serialize_event_table_input_json(rebuilt) == serialize_event_table_input_json(*e);
}

// Round CE: filter_df_keeping_spno_activities keeps the `k` largest groups
// of unordered group_by counts, the null group among them (is_in never keeps
// its rows). With ties at the cut any k groups that no unkept group outranks
// may be kept (case 804 of seed 4244 kept r1, r2, r3 or two of them and the
// null group across 48 runs); the output holds the source rows of the kept
// values in order.
bool valid_top_selection(const EventTable& df, const std::string& name, const Json& actual, std::size_t k) {
  EventTable storage;
  const auto* a = table_of(actual, storage);
  if (!a) return false;
  const auto at = std::find(df.columns.begin(), df.columns.end(), name);
  if (at == df.columns.end()) return false;
  const auto column = static_cast<std::size_t>(at - df.columns.begin());
  const auto label = [&](const EventTable::Row& row) {
    return row[column] && !row[column]->is_null ? "v" + row[column]->value : std::string("n");
  };
  std::map<std::string, std::size_t> counts;
  for (const auto& row : df.rows) ++counts[label(row)];
  std::set<std::string> kept;
  for (const auto& row : a->rows) kept.insert(label(row));
  if (kept.count("n")) return false;
  EventTable rebuilt = df;
  rebuilt.rows.clear();
  for (const auto& row : df.rows)
    if (kept.count(label(row))) rebuilt.rows.push_back(row);
  if (serialize_event_table_input_json(rebuilt) != serialize_event_table_input_json(*a)) return false;
  std::vector<std::size_t> sizes;
  for (const auto& [key, n] : counts) sizes.push_back(n);
  std::sort(sizes.rbegin(), sizes.rend());
  const auto want = std::min(k, sizes.size());
  if (want == 0) return kept.empty();
  const auto cut = sizes[want - 1];
  for (const bool with_null : {false, true}) {
    auto chosen = kept;
    if (with_null) {
      if (!counts.count("n")) continue;
      chosen.insert("n");
    }
    if (chosen.size() != want) continue;
    bool ok = true;
    for (const auto& [key, n] : counts) ok = ok && (chosen.count(key) ? n >= cut : n <= cut);
    if (ok) return true;
  }
  return false;
}

// Round CE: the same failure of filter_df_keeping_spno_activities lists the
// kept values in its is_in plan in tie order: every value all captured runs
// listed must be listed, and each listed value some run listed (case 1005 of
// seed 4244: five runs, five lists).
std::optional<std::pair<std::string, std::vector<std::string>>> listed_values(const Json& outcome) {
  const auto* error = outcome.get("error");
  if (!error || error->items.size() != 2) return {};
  auto text = error->items[0].text + "\n" + error->items[1].text;
  const auto at = text.find("is_in([[");
  if (at == std::string::npos) return {};
  const auto begin = at + 8;
  const auto end = text.find("]])", begin);
  if (end == std::string::npos) return {};
  const auto inner = text.substr(begin, end - begin);
  std::vector<std::string> items;
  for (std::size_t from = 0;;) {
    const auto comma = inner.find(", ", from);
    items.push_back(inner.substr(from, comma == std::string::npos ? std::string::npos : comma - from));
    if (comma == std::string::npos) break;
    from = comma + 2;
  }
  text.replace(begin, end - begin, "*");
  return std::pair<std::string, std::vector<std::string>>{text, items};
}
bool tied_listing(const std::vector<Json>& expected, const Json& actual) {
  const auto mine = listed_values(actual);
  if (!mine) return false;
  std::optional<std::set<std::string>> every;
  std::set<std::string> some;
  for (const auto& e : expected) {
    const auto theirs = listed_values(e);
    if (!theirs || theirs->first != mine->first || theirs->second.size() != mine->second.size()) return false;
    const std::set<std::string> items(theirs->second.begin(), theirs->second.end());
    some.insert(items.begin(), items.end());
    if (!every) every = items;
    else {
      std::set<std::string> both;
      std::set_intersection(every->begin(), every->end(), items.begin(), items.end(), std::inserter(both, both.end()));
      every = both;
    }
  }
  if (!every) return false;
  const std::set<std::string> listed(mine->second.begin(), mine->second.end());
  if (listed.size() != mine->second.size()) return false;
  for (const auto& item : *every) if (!listed.count(item)) return false;
  for (const auto& item : listed) if (!some.count(item)) return false;
  return true;
}

// MSVC's time-zone database takes abbreviations from ICU ("GMT+1" where
// Polars prints "CET"; declared in Review AY2), as in the sequence filter
// bank test: on MSVC the word after a datetime's wall text in an error
// message is masked on both sides (CD's CC bank failed only on this on
// Windows). Everywhere else the abbreviations must match exactly.
Json zone_masked(Json node) {
#if defined(_MSC_VER)
  if (const auto found = node.fields.find("error"); found != node.fields.end() && found->second.items.size() == 2) {
    static const std::regex wall(R"re((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?) [^\s,\]\)"\\]+)re");
    auto& message = found->second.items[1].text;
    message = std::regex_replace(message, wall, "$1 <zone>");
  }
#endif
  return node;
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
  } catch (const Unrepresentable& error) {
    Json out;
    out.kind = Json::kObject;
    out.fields.emplace("unrepresentable", jstring(error.what()));
    return out;
  } catch (const PolarsAttributesFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PolarsBetweenError& error) {
    return error_node(error.type, error.message);
  } catch (const PolarsCaseFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PolarsLtlFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PolarsPathsFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PolarsTimestampFilterError& error) {
    return error_node(error.type, error.message);
  } catch (const PolarsVariantsFilterError& error) {
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
  // MINERA_BANK_DUMP=path writes every mismatch as one JSON line.
  std::unique_ptr<std::ofstream> dump;
  if (const char* path = std::getenv("MINERA_BANK_DUMP")) dump = std::make_unique<std::ofstream>(path);
  // MINERA_BANK_ONLY=text replays only the functions whose name contains it.
  const char* only = std::getenv("MINERA_BANK_ONLY");
  const auto text = read_bank(argv[1]);
  const auto bank = JsonReader(text).value();
  std::map<std::string, std::pair<std::size_t, std::size_t>> tally;  // function -> passed, failed
  std::size_t failures = 0, shown = 0;
  std::size_t case_index = 0;
  for (const auto& item : bank.at("cases").items) {
    ++case_index;
    const auto df = parse_event_table_json(serialize(item.at("source")));
    for (const auto& node : item.at("calls").items) {
      const auto qualified = node.at("function").text;
      if (only && qualified.find(only) == std::string::npos) continue;
      const auto dot = qualified.find('.');
      const auto module = qualified.substr(0, dot);
      const auto function = qualified.substr(dot + 1);
      Args args;
      for (const auto& arg : node.at("args").items) args.push_back(&arg);
      const auto& kwargs = node.at("kwargs");
      const auto call_with = [&](const Args& call_args) {
        return run_call([&]() -> Json {
          const auto& args = call_args;
          const auto lazy = LazyEventTable::from_table(df);
          if (module == "attributes") return attributes_module(function, lazy, args, kwargs);
          if (module == "between") return between_module(function, lazy, args);
          if (module == "cases") return cases_module(function, lazy, args, kwargs);
          if (module == "ltl") return ltl_module(function, lazy, args);
          if (module == "paths") return paths_module(function, lazy, args);
          if (module == "timestamp") return timestamp_module(function, lazy, args);
          if (module == "variants") return variants_module(function, lazy, args);
          throw std::runtime_error("unbound module " + module);
        });
      };
      std::vector<Json> set_orders;
      std::vector<Json> candidates;
      if (set_orders.empty()) candidates.push_back(call_with(args));
      for (const auto& order : set_orders) {
        Args permuted = args;
        permuted[0] = &order;
        candidates.push_back(call_with(permuted));
      }
      auto actual = candidates.front();
      const bool unordered = node.get("unordered") && node.at("unordered").boolean;
      // Attribute value counts come from a Polars group_by without
      // maintain_order: their dict order is unspecified even when every
      // captured run agreed.
      const bool dicts_unordered = function == "get_attribute_values";
      const bool group_values = function == "filter_df_relative_occurrence_event_attribute" ||
                                function == "filter_df_keeping_activ_exc_thresh";
      std::vector<Json> expected;
      if (const auto* single = node.get("outcome")) expected.push_back(normalized(*single));
      else
        for (const auto& observed : node.at("observed").items) expected.push_back(normalized(observed));
      // Round CF: Polars 1.44.1 panics on a few inputs (a Null activity
      // column against Int64 between anchors: "implementation error, cannot
      // get ref Int32 from Int64", a pyo3 PanicException). Declared, not
      // compared.
      if (std::all_of(expected.begin(), expected.end(), [](const Json& e) {
            const auto* error = e.get("error");
            return error && !error->items.empty() && error->items.front().text == "PanicException";
          })) {
        ++tally[qualified].first;
        continue;
      }
      bool matched = false;
      for (auto& e : expected) e = zone_masked(std::move(e));
      for (auto& candidate : candidates) candidate = zone_masked(std::move(candidate));
      actual = candidates.front();
      for (auto& e : expected) {
        if (unordered) e = canonical(e);
        else if (dicts_unordered) e = canonical(e, true);
        if (group_values) e = unordered_is_in(e);
        for (auto& candidate : candidates) {
          if (unordered) candidate = canonical(candidate);
          else if (dicts_unordered) candidate = canonical(candidate, true);
          if (group_values) candidate = unordered_is_in(candidate);
          if (serialize(e) == serialize(candidate)) {
            matched = true;
            actual = candidate;
          }
        }
      }
      // The most frequent activities come from a sort of unordered group_by
      // counts: activities tied at the cut may be kept in either order.
      const auto key_or = [&](const char* keyword_name, const char* fallback) {
        const auto* key = keyword(kwargs, keyword_name);
        return key && tag_of(*key) == "s" ? body(*key).text : std::string(fallback);
      };
      if (!matched && function == "filter_df_keeping_spno_activities") {
        const auto name = key_or("activity_key", "concept:name");
        for (const auto& e : expected) matched = matched || same_selection(df, name, e, actual);
        const auto* maximum = keyword(kwargs, "max_no_activities");
        std::optional<long long> k;
        if (!maximum) k = 25;
        else if (tag_of(*maximum) == "i") k = std::stoll(body(*maximum).text);
        if (!matched && k && *k >= 0 && std::any_of(expected.begin(), expected.end(), [](const Json& e) { return e.get("value") != nullptr; }))
          matched = valid_top_selection(df, name, actual, static_cast<std::size_t>(*k));
        if (!matched) matched = tied_listing(expected, actual);
      }
      // Round CF: so, rarely, do the cases kept by filter_on_case_performance
      // and filter_traces_intersecting (round CE seeds 4247 case 333 and
      // 2026111437 case 518, round CF seed 4251 case 94: both orders over
      // 16 runs in the reference).
      if (!matched && qualified == "cases.filter_on_case_performance") {
        const auto key = key_or("case_id_glue", "case:concept:name");
        const auto mine = case_blocks(actual, key);
        for (const auto& e : expected) matched = matched || serialize(case_blocks(e, key)) == serialize(mine);
      }
      // Round CF: filter_case_performance too (seed 4262 case 27).
      if (!matched && (qualified == "between.apply" || qualified == "ltl.attr_value_different_persons" ||
                       qualified == "timestamp.filter_traces_intersecting" || qualified == "cases.filter_case_performance")) {
        std::string key = "case:concept:name";
        const auto& last = *args.at(qualified == "ltl.attr_value_different_persons" ? 1 : 2);
        // The case id key as the filters resolve it: enum keys last.
        if (is_mapping(last))
          for (const bool enum_key : {false, true})
            for (const auto& p : parameters_of<PolarsBetweenParameter>(last))
              if (p.enum_key == enum_key && p.key == "pm4py:param:case_id_key" && !p.value.is_null) key = p.value.value;
        const auto mine = case_blocks(actual, key);
        for (const auto& e : expected) matched = matched || serialize(case_blocks(e, key)) == serialize(mine);
      }
      // filter_on_ncases keeps the first cases of a group_by without
      // maintain_order: on a Categorical column their order follows the
      // process-wide category cache, and on others it can vary per run too
      // (round CD: an Int64 case column kept cases {4, 2, 1} in 7 of 12 runs
      // and {4, 3, 1} in 5).
      if (!matched && function == "filter_on_ncases") {
        const auto name = key_or("case_id_glue", "case:concept:name");
        for (const auto& e : expected) matched = matched || same_selection(df, name, e, actual);
        const auto* maximum = keyword(kwargs, "max_no_cases");
        const bool tables = std::all_of(expected.begin(), expected.end(), [](const Json& e) { return e.get("value") != nullptr; });
        if (!matched && tables && (!maximum || tag_of(*maximum) == "i"))
          matched = valid_ncases(df, name, actual, maximum ? std::stoll(body(*maximum).text) : 1000);
      }
      // Round CF: dict values of the timeframe filter are struct literals;
      // only String columns with str/int/bool fields are modelled, the rest
      // is refused (declared, docs/reference-2.7.22-parity.md CF1).
      if (!matched && qualified == "timestamp.filter_traces_attribute_in_timeframe" && args.size() > 1 &&
          is_mapping(*args.at(1))) {
        const auto* error = actual.get("error");
        matched = error && error->items.size() > 1 && error->items.front().text == "native" &&
                  error->items.at(1).text.rfind("Native Polars timeframe filters take dict", 0) == 0;
      }
      auto& counts = tally[qualified];
      if (matched) {
        ++counts.first;
        continue;
      }
      ++counts.second;
      ++failures;
      if (dump) *dump << serialize(jarray({jstring(qualified), jstring(std::to_string(case_index - 1)), node.at("args"), kwargs,
                                           expected.front(), actual}))
                      << "\n";
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
