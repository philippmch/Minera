// Common and DFG filtering helpers against the public seeded bank
// (docs/filtering-common-bank.json.gz, scripts/capture-filtering-common-bank.py).
// Each call names a public callable of pm4py.algo.filtering.common.
// {filtering_constants, attributes.attributes_common,
// timestamp.timestamp_common} or pm4py.algo.filtering.dfg.dfg_filtering and
// its typed arguments. Values compare as typed JSON (sets and unordered dicts
// without order), errors as class and message, and a changed argument as its
// value after the call. A call whose outcome varied between hash seeds must
// match one of them; a KeyError among several missing names may name any of
// them. KDE densities compare with a relative tolerance (see kde_close).
#include "minera/dfg_filtering.hpp"
#include "minera/filtering_common.hpp"

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


Json typed(const std::string& tag, std::string text) { return jtagged(tag, jstring(std::move(text))); }
Json typed_int(std::int64_t value) { return typed("i", std::to_string(value)); }
Json typed_float(double value) { return typed("f", bits_hex(value)); }
Json typed_string(const std::string& value) { return typed("s", value); }
Json typed_bool(bool value) {
  Json b;
  b.kind = Json::kBool;
  b.boolean = value;
  return jtagged("b", b);
}
Json typed_list(std::vector<Json> items) { return jtagged("l", jarray(std::move(items))); }
Json typed_tuple(std::vector<Json> items) { return jtagged("t", jarray(std::move(items))); }
Json typed_pairs(const std::string& tag, std::vector<std::pair<Json, Json>> pairs) {
  std::vector<Json> items;
  for (auto& [k, v] : pairs) items.push_back(jarray({std::move(k), std::move(v)}));
  return jtagged(tag, jarray(std::move(items)));
}

const std::string& tag_of(const Json& node) {
  if (node.kind != Json::kObject || node.fields.size() != 1) throw std::runtime_error("not a typed node: " + serialize(node));
  return node.fields.begin()->first;
}
const Json& body(const Json& node) { return node.fields.begin()->second; }

std::int64_t as_int(const Json& node) {
  if (tag_of(node) != "i") throw std::runtime_error("not an int: " + serialize(node));
  return std::stoll(body(node).text);
}
double as_number(const Json& node) {
  if (tag_of(node) == "i") return static_cast<double>(std::stoll(body(node).text));
  if (tag_of(node) == "f") return hex_bits(body(node).text);
  throw std::runtime_error("not a number: " + serialize(node));
}
FilteringNumber as_count(const Json& node) {
  if (tag_of(node) == "i") return FilteringNumber::of(static_cast<std::int64_t>(std::stoll(body(node).text)));
  return FilteringNumber::of(as_number(node));
}
Json count_typed(const FilteringNumber& value) {
  return value.integral ? typed_int(value.integer) : typed_float(value.real);
}
const std::string& as_string(const Json& node) {
  if (tag_of(node) != "s") throw std::runtime_error("not a string: " + serialize(node));
  return body(node).text;
}
// Members of a list, tuple, set or the keys of a dict.
std::vector<const Json*> members(const Json& node) {
  std::vector<const Json*> out;
  const auto& tag = tag_of(node);
  for (const auto& item : body(node).items) out.push_back(tag == "m" ? &item.items.at(0) : &item);
  return out;
}

DfgFilteringEdges edges_of(const Json& node) {
  DfgFilteringEdges out;
  for (const auto& pair : body(node).items) {
    const auto& key = body(pair.items.at(0)).items;
    out.push_back({{as_string(key.at(0)), as_string(key.at(1))}, as_int(pair.items.at(1))});
  }
  return out;
}
DfgFilteringCounts counts_of(const Json& node) {
  DfgFilteringCounts out;
  for (const auto& pair : body(node).items) out.push_back({as_string(pair.items.at(0)), as_int(pair.items.at(1))});
  return out;
}
DfgFilteringData data_of(const std::vector<const Json*>& args) {
  return {edges_of(*args.at(0)), counts_of(*args.at(1)), counts_of(*args.at(2)), counts_of(*args.at(3))};
}
DfgFilteringSuccessors successors_of(const Json& node) {
  DfgFilteringSuccessors out;
  for (const auto& pair : body(node).items) {
    auto& next = out[as_string(pair.items.at(0))];
    for (const auto* m : members(pair.items.at(1))) next.push_back(as_string(*m));
  }
  return out;
}

Json edges_typed(const DfgFilteringEdges& edges) {
  std::vector<std::pair<Json, Json>> pairs;
  for (const auto& [e, c] : edges) pairs.push_back({typed_tuple({typed_string(e.first), typed_string(e.second)}), typed_int(c)});
  return typed_pairs("m", std::move(pairs));
}
Json counts_typed(const DfgFilteringCounts& counts) {
  std::vector<std::pair<Json, Json>> pairs;
  for (const auto& [a, c] : counts) pairs.push_back({typed_string(a), typed_int(c)});
  return typed_pairs("m", std::move(pairs));
}
Json data_typed(const DfgFilteringData& data) {
  return typed_tuple({edges_typed(data.dfg), counts_typed(data.start_activities), counts_typed(data.end_activities),
                      counts_typed(data.activities)});
}
Json set_typed(const std::set<std::string>& names) {
  std::vector<Json> items;
  for (const auto& n : names) items.push_back(typed_string(n));
  return jtagged("S", jarray(std::move(items)));
}
Json adjacency_typed(const std::map<std::string, std::set<std::string>>& adjacency) {
  std::vector<std::pair<Json, Json>> pairs;
  for (const auto& [node, next] : adjacency) pairs.push_back({typed_string(node), set_typed(next)});
  return typed_pairs("U", std::move(pairs));
}

FilteringDateTime wall_of(const Json& node) {
  if (tag_of(node) != "w") throw std::runtime_error("not a datetime: " + serialize(node));
  const auto& stamp = body(node).items.at(0).text;  // YYYY-MM-DD HH:MM:SS.ffffff
  FilteringDateTime out;
  out.year = std::stoi(stamp.substr(0, 4));
  out.month = std::stoi(stamp.substr(5, 2));
  out.day = std::stoi(stamp.substr(8, 2));
  out.hour = std::stoi(stamp.substr(11, 2));
  out.minute = std::stoi(stamp.substr(14, 2));
  out.second = std::stoi(stamp.substr(17, 2));
  out.microsecond = std::stoi(stamp.substr(20, 6));
  return out;
}
Json utc_typed(const FilteringDateTime& value) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02d %02d:%02d:%02d", value.year, value.month, value.day, value.hour,
                value.minute, value.second);
  std::string text = buffer;
  if (value.microsecond) {
    std::snprintf(buffer, sizeof buffer, ".%06d", value.microsecond);
    text += buffer;
  }
  return typed("d", text + "+00:00");
}

// A KDE parameters argument: absent, None or a dict keyed by enum member
// ("enum:GRAPH_POINTS") or by its string value.
std::int64_t kde_parameter(const std::vector<const Json*>& args, std::size_t index, const std::string& member,
                           const std::string& value, std::int64_t fallback) {
  if (args.size() <= index || tag_of(*args[index]) == "n") return fallback;
  for (const auto& pair : body(*args[index]).items) {
    const auto& key = as_string(pair.items.at(0));
    if (key == "enum:" + member || key == value) return as_int(pair.items.at(1));
  }
  return fallback;
}

Json value_outcome(Json value) {
  Json out;
  out.kind = Json::kObject;
  out.fields.emplace("value", std::move(value));
  return out;
}

// ------------------------------------------------------------ modules

Json filtering_constants_module(const std::string& name) {
  if (name == "CASE_CONCEPT_NAME") return typed_string(std::string(kFilteringCaseConceptName));
  if (name == "DECREASING_FACTOR") return typed_float(kFilteringDecreasingFactor);
  if (name == "DEFAULT_NOISE_THRESH_DF") return typed_float(kFilteringDefaultNoiseThreshDf);
  throw std::runtime_error("unbound constant " + name);
}

Json timestamp_common_module(const std::string& function, const std::vector<const Json*>& args) {
  if (function != "get_dt_from_string") throw std::runtime_error("unbound function " + function);
  const auto& node = *args.at(0);
  const auto& tag = tag_of(node);
  FilteringDtArgument argument;
  if (tag == "s") {
    argument.text = as_string(node);
  } else if (tag == "w") {
    argument.kind = FilteringDtArgument::Kind::kDateTime;
    argument.datetime = wall_of(node);
  } else {
    argument.kind = FilteringDtArgument::Kind::kOther;
    static const std::map<std::string, std::string> names = {
        {"i", "int"}, {"f", "float"}, {"b", "bool"}, {"n", "NoneType"}, {"l", "list"}, {"t", "tuple"}, {"m", "dict"}};
    argument.text = names.at(tag);
  }
  return utc_typed(get_dt_from_string(argument));
}

Json attributes_common_module(const std::string& function, const std::vector<const Json*>& args) {
  if (function == "get_sorted_attributes_list") {
    FilteringAttributeCounts counts;
    for (const auto& pair : body(*args.at(0)).items) counts.push_back({as_string(pair.items.at(0)), as_count(pair.items.at(1))});
    std::vector<Json> items;
    for (const auto& [name, count] : get_sorted_attributes_list(counts))
      items.push_back(typed_list({typed_string(name), count_typed(count)}));
    return typed_list(std::move(items));
  }
  if (function == "get_attributes_threshold") {
    FilteringAttributeCounts alist;
    for (const auto& item : body(*args.at(0)).items)
      alist.push_back({as_string(body(item).items.at(0)), as_count(body(item).items.at(1))});
    const double factor = as_number(*args.at(1));
    const std::int64_t minimum = args.size() > 2 ? as_int(*args[2]) : 1;
    const std::int64_t maximum = args.size() > 3 ? as_int(*args[3]) : 25;
    return count_typed(get_attributes_threshold(alist, factor, minimum, maximum));
  }
  if (function == "get_kde_numeric_attribute" || function == "get_kde_numeric_attribute_json") {
    std::vector<double> values;
    for (const auto* m : members(*args.at(0))) values.push_back(as_number(*m));
    const auto points = kde_parameter(args, 1, "GRAPH_POINTS", "graph_points", 200);
    if (function == "get_kde_numeric_attribute_json") return typed_string(get_kde_numeric_attribute_json(values, points));
    const auto kde = get_kde_numeric_attribute(values, points);
    std::vector<Json> x, y;
    for (const double v : kde.x) x.push_back(typed_float(v));
    for (const double v : kde.y) y.push_back(typed_float(v));
    return typed_tuple({typed_list(std::move(x)), typed_list(std::move(y))});
  }
  if (function == "get_kde_date_attribute" || function == "get_kde_date_attribute_json") {
    std::vector<FilteringDateTime> values;
    for (const auto* m : members(*args.at(0))) values.push_back(wall_of(*m));
    const auto points = kde_parameter(args, 1, "GRAPH_POINTS", "graph_points", 200);
    const auto sample = kde_parameter(args, 1, "POINT_TO_SAMPLE", "points_to_sample", 400);
    if (function == "get_kde_date_attribute_json")
      return typed_string(get_kde_date_attribute_json(values, points, sample));
    const auto kde = get_kde_date_attribute(values, points, sample);
    std::vector<Json> x, y;
    for (const auto v : kde.x_ns) x.push_back(jstring(std::to_string(v)));
    for (const double v : kde.y) y.push_back(typed_float(v));
    return typed_list({jtagged("ns", jarray(std::move(x))), typed_list(std::move(y))});
  }
  throw std::runtime_error("unbound function " + function);
}

std::optional<std::vector<DfgFilteringEdge>> common_paths(const std::vector<const Json*>& args) {
  if (args.size() <= 3 || tag_of(*args[3]) == "n") return std::nullopt;
  for (const auto& pair : body(*args[3]).items) {
    if (as_string(pair.items.at(0)) != "most_common_paths") continue;
    if (tag_of(pair.items.at(1)) == "n") return std::nullopt;
    std::vector<DfgFilteringEdge> paths;
    for (const auto* m : members(pair.items.at(1))) {
      const auto& key = body(*m).items;
      paths.push_back({as_string(key.at(0)), as_string(key.at(1))});
    }
    return paths;
  }
  return std::nullopt;
}

Json dfg_filtering_module(const std::string& function, const std::vector<const Json*>& args, Json& outcome) {
  if (function == "DEFAULT_NOISE_THRESH_DF") return typed_float(kDfgFilteringDefaultNoiseThreshDf);
  if (function == "filter_dfg_on_activities_percentage")
    return data_typed(filter_dfg_on_activities_percentage(data_of(args), as_number(*args.at(4))));
  if (function == "filter_dfg_on_paths_percentage")
    return data_typed(filter_dfg_on_paths_percentage(data_of(args), as_number(*args.at(4)),
                                                     args.size() > 5 && body(*args[5]).boolean));
  if (function == "filter_dfg_keep_connected")
    return data_typed(
        filter_dfg_keep_connected(data_of(args), as_number(*args.at(4)), args.size() > 5 && body(*args[5]).boolean));
  if (function == "filter_dfg_to_activity") return data_typed(filter_dfg_to_activity(data_of(args), as_string(*args.at(4))));
  if (function == "filter_dfg_from_activity")
    return data_typed(filter_dfg_from_activity(data_of(args), as_string(*args.at(4))));
  if (function == "filter_dfg_contain_activity")
    return data_typed(filter_dfg_contain_activity(data_of(args), as_string(*args.at(4))));
  if (function == "clean_dfg_based_on_noise_thresh") {
    std::vector<std::string> activities;
    for (const auto* m : members(*args.at(1))) activities.push_back(as_string(*m));
    return edges_typed(clean_dfg_based_on_noise_thresh(edges_of(*args.at(0)), activities, as_number(*args.at(2)),
                                                       common_paths(args)));
  }
  if (function == "build_adjacency_structures") {
    const auto built = build_adjacency_structures(edges_of(*args.at(0)), counts_of(*args.at(1)), counts_of(*args.at(2)));
    return typed_tuple({adjacency_typed(built.adjacency), adjacency_typed(built.reverse_adjacency),
                        typed_string(built.start_node), typed_string(built.end_node)});
  }
  if (function == "bfs_reachable") return set_typed(bfs_reachable(as_string(*args.at(0)), successors_of(*args.at(1))));
  if (function == "remove_unreachable_nodes") {
    auto data = data_of(args);
    const auto before = data;
    std::string failure;
    Json value;
    try {
      value = data_typed(remove_unreachable_nodes(data, successors_of(*args.at(4)), successors_of(*args.at(5)),
                                                  as_string(*args.at(6)), as_string(*args.at(7))));
    } catch (...) {
      if (data == before) throw;
      throw std::runtime_error("source changed before a failure");
    }
    std::vector<Json> after;
    const std::pair<const DfgFilteringCounts*, const DfgFilteringCounts*> changed[] = {
        {&before.start_activities, &data.start_activities},
        {&before.end_activities, &data.end_activities},
        {&before.activities, &data.activities}};
    for (int i = 0; i < 3; ++i)
      if (*changed[i].first != *changed[i].second) after.push_back(jarray({[&] {
        Json index;
        index.kind = Json::kNumber;
        index.text = std::to_string(i + 1);
        return index;
      }(), counts_typed(*changed[i].second)}));
    if (!after.empty()) {
      Json yes;
      yes.kind = Json::kBool;
      yes.boolean = true;
      outcome.fields.emplace("mutated", yes);
      outcome.fields.emplace("after", jarray(std::move(after)));
    }
    return value;
  }
  if (function == "generate_nx_graph_from_dfg") {
    const auto graph = generate_nx_graph_from_dfg(data_of(args));
    const auto name = [&](const std::string& n) {
      return n == graph.start_node ? std::string("<S>") : n == graph.end_node ? std::string("<E>") : n;
    };
    std::vector<Json> nodes, edges;
    for (const auto& n : graph.nodes) nodes.push_back(typed_string(name(n)));
    for (const auto& [a, b] : graph.edges) edges.push_back(typed_tuple({typed_string(name(a)), typed_string(name(b))}));
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
    Json ok;
    ok.kind = Json::kBool;
    ok.boolean = std::regex_match(graph.start_node, uuid) && std::regex_match(graph.end_node, uuid) &&
                 graph.start_node != graph.end_node;
    return jtagged("g", jarray({jarray(std::move(nodes)), jarray(std::move(edges)), ok}));
  }
  throw std::runtime_error("unbound function " + function);
}

// Records the module function's value, or the public reference failure it
// raised. The value may also fill in the outcome (mutated arguments).
template <typename Call>
Json run_call(Call&& call) {
  Json out;
  out.kind = Json::kObject;
  try {
    Json value = call(out);
    out.fields.emplace("value", std::move(value));
  } catch (const FilteringCommonError& error) {
    out.fields.clear();
    out.fields.emplace("error", jarray({jstring(error.type), jstring(error.message)}));
  }
  return out;
}

// ------------------------------------------------------------ comparison

// Sets ("S") and unordered dicts ("U") compare without their order.
Json canonical(const Json& node) {
  Json out = node;
  for (auto& item : out.items) item = canonical(item);
  for (auto& [key, item] : out.fields) item = canonical(item);
  if (out.kind == Json::kObject && out.fields.size() == 1) {
    const auto& tag = out.fields.begin()->first;
    if (tag == "S" || tag == "U") {
      auto& items = out.fields.begin()->second.items;
      std::sort(items.begin(), items.end(), [](const Json& a, const Json& b) { return serialize(a) < serialize(b); });
    }
  }
  return out;
}

struct Tolerance {
  double worst = 0.0;
  double worst_ratio = 0.0;  // relative error over the allowed tolerance
  bool numbers_close(double expected, double actual, double rtol, double atol) {
    if (std::isnan(expected) || std::isnan(actual)) return std::isnan(expected) && std::isnan(actual);
    if (expected == actual) return true;
    if (std::fabs(expected - actual) <= atol) return true;
    const double scale = std::max(std::fabs(expected), std::fabs(actual));
    const double error = std::fabs(expected - actual) / scale;
    worst = std::max(worst, error);
    worst_ratio = std::max(worst_ratio, error / rtol);
    return error <= rtol;
  }
};

// Python float tokens of a json.dumps text.
std::vector<double> json_numbers(const std::string& text) {
  std::vector<double> out;
  for (std::size_t at = 0; at < text.size();) {
    const char c = text[at];
    if (c == '[' || c == ']' || c == ',' || c == ' ') {
      ++at;
      continue;
    }
    std::size_t end = at;
    while (end < text.size() && text[end] != ',' && text[end] != ']') ++end;
    const auto token = text.substr(at, end - at);
    if (token == "NaN") out.push_back(std::nan(""));
    else if (token == "Infinity") out.push_back(HUGE_VAL);
    else if (token == "-Infinity") out.push_back(-HUGE_VAL);
    else out.push_back(std::stod(token));
    at = end;
  }
  return out;
}

// The x and y numbers of a KDE outcome (x as seconds for datetime grids).
bool kde_numbers(const std::string& function, const Json& value, std::vector<double>& x, std::vector<double>& y) {
  if (function.size() > 5 && function.compare(function.size() - 5, 5, "_json") == 0) {
    const auto numbers = json_numbers(body(value).text);
    for (std::size_t i = 0; i < numbers.size(); ++i) (i % 2 ? y : x).push_back(numbers[i]);
    return true;
  }
  const auto& parts = body(value).items;
  if (parts.size() != 2) return false;
  if (tag_of(parts[0]) == "ns") {
    for (const auto& v : body(parts[0]).items) x.push_back(static_cast<double>(std::stoll(v.text)) / 1e9);
  } else {
    for (const auto& v : body(parts[0]).items) x.push_back(as_number(v));
  }
  for (const auto& v : body(parts[1]).items) y.push_back(as_number(v));
  return true;
}

// KDE densities and numpy's transcendental geomspace points are compared with
// tolerances; every other part compares exactly: the grid sizes, the linspace
// and datetime grids (and their JSON seconds). numpy's SIMD log10/pow and the
// BLAS summation order of numpy.cov are not portable, and a covariance that
// differs in its last bits moves each density by about
// eps * max|x| / span relative (the kernel rescales absolute values before
// subtracting), so the density tolerance is 1e-9 + 1e-13 * max|x| / span,
// plus an absolute 1e-12 * max(y) for the far tails. A one-point grid has no
// span of its own, so the span of the input values stands in.
double wall_seconds(const FilteringDateTime& t) {
  const int y = t.year - (t.month <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (t.month + (t.month > 2 ? -3 : 9)) + 2) / 5 + t.day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const double days = era * 146097.0 + doe - 719468.0;
  return days * 86400.0 + t.hour * 3600.0 + t.minute * 60.0 + t.second + t.microsecond / 1e6;
}

double input_span(const std::string& function, const std::vector<const Json*>& args) {
  double low = HUGE_VAL, high = -HUGE_VAL;
  for (const auto* m : members(*args.at(0))) {
    const double v = function.find("numeric") != std::string::npos ? as_number(*m) : wall_seconds(wall_of(*m));
    low = std::min(low, v);
    high = std::max(high, v);
  }
  return high - low;
}

bool kde_close(const std::string& function, const Json& expected, const Json& actual, Tolerance& tolerance,
               const std::vector<const Json*>& args) {
  const auto* ev = expected.get("value");
  const auto* av = actual.get("value");
  if (!ev || !av) return false;
  const bool numeric = function.find("numeric") != std::string::npos;
  const bool json = function.size() > 5 && function.compare(function.size() - 5, 5, "_json") == 0;
  if (!json && !numeric && serialize(body(*ev).items.at(0)) != serialize(body(*av).items.at(0))) return false;
  std::vector<double> ex, ey, ax, ay;
  if (!kde_numbers(function, *ev, ex, ey) || !kde_numbers(function, *av, ax, ay)) return false;
  if (ex.size() != ax.size() || ey.size() != ay.size()) return false;
  double magnitude = 0.0, low = HUGE_VAL, high = -HUGE_VAL, ymax = 0.0;
  for (const double v : ex) {
    magnitude = std::max(magnitude, std::fabs(v));
    low = std::min(low, v);
    high = std::max(high, v);
  }
  for (const double v : ey) ymax = std::max(ymax, std::fabs(v));
  const double span = ex.size() == 1 ? input_span(function, args) : high - low;
  const double rtol = 1e-9 + (span > 0 && std::isfinite(span) ? 1e-13 * magnitude / span : 0.0);
  for (std::size_t i = 0; i < ex.size(); ++i) {
    if (numeric ? !tolerance.numbers_close(ex[i], ax[i], 1e-12, 0.0) : ex[i] != ax[i]) return false;
  }
  for (std::size_t i = 0; i < ey.size(); ++i)
    if (!tolerance.numbers_close(ey[i], ay[i], rtol, 1e-12 * ymax)) return false;
  return true;
}

// Names whose KeyError the reference may raise first when several are missing
// (its graph checks iterate sets): missing activities of a to/from/contain
// filter, or nodes without an adjacency entry for the reachability helpers.
std::set<std::string> key_candidates(const std::string& function, const std::vector<const Json*>& args) {
  std::set<std::string> out;
  if (function == "filter_dfg_to_activity" || function == "filter_dfg_from_activity" ||
      function == "filter_dfg_contain_activity") {
    const auto data = data_of(args);
    const auto x = as_string(*args.at(4));
    std::set<std::string> known;
    for (const auto& [a, c] : data.activities) known.insert(a);
    for (const auto& [e, c] : data.dfg) {
      if (function == "filter_dfg_to_activity" && e.first == x) continue;
      if (function == "filter_dfg_from_activity" && e.second == x) continue;
      for (const auto* n : {&e.first, &e.second})
        if (!known.count(*n)) out.insert(*n);
    }
    const auto* boundary = function == "filter_dfg_to_activity"     ? &data.start_activities
                           : function == "filter_dfg_from_activity" ? &data.end_activities
                                                                    : nullptr;
    if (boundary)
      for (const auto& [a, c] : *boundary)
        if (!known.count(a)) out.insert(a);
  } else if (function == "bfs_reachable" || function == "remove_unreachable_nodes") {
    std::vector<std::pair<std::string, DfgFilteringSuccessors>> walks;
    if (function == "bfs_reachable") {
      walks.push_back({as_string(*args.at(0)), successors_of(*args.at(1))});
    } else {
      walks.push_back({as_string(*args.at(6)), successors_of(*args.at(4))});
      walks.push_back({as_string(*args.at(7)), successors_of(*args.at(5))});
    }
    for (const auto& [start, adjacency] : walks) {
      if (!adjacency.count(start)) out.insert(start);
      for (const auto& [node, next] : adjacency)
        for (const auto& n : next)
          if (!adjacency.count(n)) out.insert(n);
    }
  }
  return out;
}

bool key_error_in(const Json& outcome, const std::set<std::string>& candidates) {
  const auto* error = outcome.get("error");
  if (!error || error->items.at(0).text != "KeyError") return false;
  for (const auto& c : candidates)
    if ("'" + c + "'" == error->items.at(1).text || "\"" + c + "\"" == error->items.at(1).text) return true;
  return false;
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
  std::size_t failures = 0, shown = 0, kde_calls = 0, key_order = 0;
  Tolerance tolerance;
  for (const auto& item : bank.at("cases").items) {
    for (const auto& node : item.at("calls").items) {
      const auto qualified = node.at("function").text;
      const auto dot = qualified.find('.');
      const auto module = qualified.substr(0, dot);
      const auto function = qualified.substr(dot + 1);
      std::vector<const Json*> args;
      for (const auto& arg : node.at("args").items) args.push_back(&arg);
      const auto actual = canonical(run_call([&](Json& outcome) {
        if (module == "filtering_constants") return filtering_constants_module(function);
        if (module == "timestamp_common") return timestamp_common_module(function, args);
        if (module == "attributes_common") return attributes_common_module(function, args);
        if (module == "dfg_filtering") return dfg_filtering_module(function, args, outcome);
        throw std::runtime_error("unbound module " + module);
      }));
      std::vector<Json> expected;
      if (const auto* single = node.get("outcome")) expected.push_back(canonical(*single));
      else
        for (const auto& observed : node.at("observed").items) expected.push_back(canonical(observed));
      const bool kde = function.rfind("get_kde_", 0) == 0;
      bool matched = false;
      for (const auto& e : expected) {
        matched = matched || serialize(e) == serialize(actual) || (kde && kde_close(function, e, actual, tolerance, args));
      }
      if (!matched) {
        const auto candidates = key_candidates(function, args);
        bool all_keys = candidates.size() > 1 && key_error_in(actual, candidates);
        for (const auto& e : expected) all_keys = all_keys && key_error_in(e, candidates);
        if (all_keys) {
          matched = true;
          ++key_order;
        }
      }
      if (kde) ++kde_calls;
      auto& counts = tally[qualified];
      if (matched) {
        ++counts.first;
        continue;
      }
      ++counts.second;
      ++failures;
      if (verbose || shown < 40) {
        ++shown;
        std::cerr << qualified << " args " << serialize(node.at("args")).substr(0, 1500) << "\n  expected "
                  << serialize(expected.front()).substr(0, 1500) << "\n  actual   "
                  << serialize(actual).substr(0, 1500) << "\n";
      }
    }
  }
  for (const auto& [name, counts] : tally)
    std::cout << name << " " << counts.first << " passed, " << counts.second << " failed\n";
  std::cout << "KDE calls " << kde_calls << ", worst relative difference " << tolerance.worst
            << " (" << tolerance.worst_ratio << " of its tolerance)\n";
  std::cout << "KeyErrors among several missing names " << key_order << "\n";
  std::cout << (failures ? "FAILED " : "passed ") << failures << " mismatches\n";
  return failures ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << '\n';
  return 1;
}
