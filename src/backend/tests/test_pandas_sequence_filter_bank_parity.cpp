// Pandas sequence filters against the public seeded bank
// (docs/pandas-sequence-filter-bank.json.gz,
// scripts/capture-pandas-sequence-filter-bank.py). Each case carries one source
// DataFrame as a native event table and calls of the public callables of
// pm4py.algo.filtering.pandas.{pd_filtering_constants, start_activities,
// end_activities, traces, starts_with, ends_with, rework, prefixes, suffixes,
// consecutive_act_case_grouping, timestamp_case_grouping, activity_split}.
// Returned tables compare as their complete native carrier (columns, dtypes,
// index class and labels, every cell), other values as typed JSON, errors as
// class and message. No call may change its source table.
#include "minera/dataframe_format_json.hpp"
#include "minera/pandas_sequence_filters.hpp"

#include "../gzip_codec.hpp"

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

PandasFilterValue value_of(const Json& node) {
  const auto& tag = tag_of(node);
  const auto& v = body(node);
  if (tag == "n") return PandasFilterValue::none();
  if (tag == "b") return PandasFilterValue::of(v.boolean);
  if (tag == "i") return PandasFilterValue::of(static_cast<std::int64_t>(std::stoll(v.text)));
  if (tag == "f") return PandasFilterValue::of(hex_bits(v.text));
  if (tag == "s") return PandasFilterValue::of(v.text);
  std::vector<PandasFilterValue> items;
  if (tag == "l" || tag == "t") {
    for (const auto& item : v.items) items.push_back(value_of(item));
    return tag == "l" ? PandasFilterValue::list(std::move(items)) : PandasFilterValue::tuple(std::move(items));
  }
  if (tag == "m") {
    std::vector<std::pair<PandasFilterValue, PandasFilterValue>> entries;
    for (const auto& pair : v.items) entries.emplace_back(value_of(pair.items.at(0)), value_of(pair.items.at(1)));
    return PandasFilterValue::dict(std::move(entries));
  }
  throw std::runtime_error("unbound value tag " + tag);
}

// A parameters dictionary: "enum:NAME" keys are Parameters members.
PandasFilterParameters parameters_of(const Json& node) {
  if (tag_of(node) == "n") return std::nullopt;
  std::vector<PandasFilterParameter> out;
  for (const auto& pair : body(node).items) {
    const auto& key = body(pair.items.at(0)).text;
    PandasFilterParameter p;
    p.enum_member = key.rfind("enum:", 0) == 0;
    p.key = p.enum_member ? key.substr(5) : key;
    p.value = value_of(pair.items.at(1));
    out.push_back(std::move(p));
  }
  return out;
}

Json typed(const std::string& tag, std::string text) { return jtagged(tag, jstring(std::move(text))); }

Json typed_value(const PandasFilterValue& v) {
  using K = PandasFilterValue::Kind;
  switch (v.kind) {
    case K::kNone: {
      Json null;
      return jtagged("n", null);
    }
    case K::kBool: {
      Json b;
      b.kind = Json::kBool;
      b.boolean = v.boolean;
      return jtagged("b", b);
    }
    case K::kInt: return typed("i", std::to_string(v.integer));
    case K::kFloat: return typed("f", bits_hex(v.real));
    case K::kString: return typed("s", v.text);
    case K::kList:
    case K::kTuple: {
      std::vector<Json> items;
      for (const auto& item : v.items) items.push_back(typed_value(item));
      return jtagged(v.kind == K::kList ? "l" : "t", jarray(std::move(items)));
    }
    case K::kDict: {
      std::vector<Json> items;
      for (const auto& [k, item] : v.entries) items.push_back(jarray({typed_value(k), typed_value(item)}));
      return jtagged("m", jarray(std::move(items)));
    }
  }
  return {};
}

Json table_node(const EventTable& table) { return jtagged("table", jstring(serialize_event_table_input_json(table))); }

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
std::string keyword_text(const Json& kwargs, const std::string& name, const std::string& fallback) {
  const auto* found = keyword(kwargs, name);
  return found ? body(*found).text : fallback;
}

// ------------------------------------------------------------ modules

Json constant(const std::string& name) {
  if (name == "MIN_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM")
    return typed("i", std::to_string(kPandasMinNoOfActivitiesToRetainForDiagram));
  if (name == "MAX_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM")
    return typed("i", std::to_string(kPandasMaxNoOfActivitiesToRetainForDiagram));
  if (name == "INT_CASE_ACT_SIZE") return typed("s", std::string(kPandasReworkIntCaseActSize));
  if (name == "CASE_CONCEPT_NAME") return typed("s", std::string(kPandasFilterCaseConceptName));
  if (name == "DEFAULT_NAME_KEY") return typed("s", std::string(kPandasFilterDefaultNameKey));
  if (name == "PARAMETER_CONSTANT_CASEID_KEY") return typed("s", std::string(kPandasFilterCaseIdKeyParameter));
  if (name == "PARAMETER_CONSTANT_ACTIVITY_KEY") return typed("s", std::string(kPandasFilterActivityKeyParameter));
  if (name == "GROUPED_DATAFRAME") return typed("s", std::string(kPandasFilterGroupedDataframe));
  if (name == "RETURN_EA_COUNT_DICT_AUTOFILTER")
    return typed("s", std::string(kPandasFilterReturnEaCountDictAutofilter));
  throw std::runtime_error("unbound constant " + name);
}

Json pd_filtering_constants_module(const std::string& name) {
  if (name == "MIN_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM")
    return typed("i", std::to_string(kPandasMinNoOfActivitiesToRetainForDiagram));
  if (name == "MAX_NO_OF_ACTIVITIES_TO_RETAIN_FOR_DIAGRAM")
    return typed("i", std::to_string(kPandasMaxNoOfActivitiesToRetainForDiagram));
  throw std::runtime_error("unbound constant " + name);
}

using Args = std::vector<const Json*>;

Json start_activities_module(const std::string& function, const EventTable& df, const Args& args,
                             const Json& kwargs) {
  if (function == "apply") return table_node(pandas_start_activities_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  const auto glue = keyword_text(kwargs, "case_id_glue", "case:concept:name");
  const auto activity = keyword_text(kwargs, "activity_key", "concept:name");
  if (function == "filter_df_on_start_activities") {
    const auto* positive = keyword(kwargs, "positive");
    return table_node(pandas_filter_df_on_start_activities(df, value_of(*args.at(0)), glue, activity,
                                                           positive ? value_of(*positive) : PandasFilterValue::of(true)));
  }
  if (function == "filter_df_on_start_activities_nocc") {
    const auto* counts = keyword(kwargs, "sa_count0");
    return table_node(pandas_filter_df_on_start_activities_nocc(
        df, value_of(*args.at(0)), counts ? std::optional(value_of(*counts)) : std::nullopt, glue, activity));
  }
  return constant(function);
}

Json end_activities_module(const std::string& function, const EventTable& df, const Args& args, const Json& kwargs) {
  if (function == "apply") return table_node(pandas_end_activities_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  const auto glue = keyword_text(kwargs, "case_id_glue", "case:concept:name");
  const auto activity = keyword_text(kwargs, "activity_key", "concept:name");
  if (function == "filter_df_on_end_activities") {
    const auto* positive = keyword(kwargs, "positive");
    return table_node(pandas_filter_df_on_end_activities(df, value_of(*args.at(0)), glue, activity,
                                                         positive ? value_of(*positive) : PandasFilterValue::of(true)));
  }
  if (function == "filter_df_on_end_activities_nocc") {
    const auto* counts = keyword(kwargs, "ea_count0");
    const auto* dict = keyword(kwargs, "return_dict");
    const auto* variant = keyword(kwargs, "most_common_variant");
    const auto out = pandas_filter_df_on_end_activities_nocc(
        df, value_of(*args.at(0)), counts ? std::optional(value_of(*counts)) : std::nullopt, glue, activity,
        dict && body(*dict).boolean, variant ? std::optional(value_of(*variant)) : std::nullopt);
    if (out.counts) return jtagged("t", jarray({table_node(out.table), typed_value(*out.counts)}));
    return table_node(out.table);
  }
  return constant(function);
}

Json traces_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_trace_filter_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  return constant(function);
}
Json starts_with_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply")
    return table_node(pandas_starts_with_filter_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  return constant(function);
}
Json ends_with_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply")
    return table_node(pandas_ends_with_filter_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  return constant(function);
}
Json rework_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_rework_filter_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  return constant(function);
}
Json prefixes_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_prefix_filter_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  throw std::runtime_error("unbound function " + function);
}
Json suffixes_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_suffix_filter_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  throw std::runtime_error("unbound function " + function);
}
Json consecutive_act_case_grouping_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_consecutive_act_case_grouping_apply(df, parameters_of(*args.at(0))));
  throw std::runtime_error("unbound function " + function);
}
Json timestamp_case_grouping_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_timestamp_case_grouping_apply(df, parameters_of(*args.at(0))));
  throw std::runtime_error("unbound function " + function);
}
Json activity_split_module(const std::string& function, const EventTable& df, const Args& args) {
  if (function == "apply") return table_node(pandas_activity_split_apply(df, value_of(*args.at(0)), parameters_of(*args.at(1))));
  throw std::runtime_error("unbound function " + function);
}

// The value of a call or the public failure it raised.
template <typename Call>
Json run_call(Call&& call) {
  Json out;
  out.kind = Json::kObject;
  try {
    out.fields.emplace("value", call());
  } catch (const PandasSequenceFilterError& error) {
    out.fields.clear();
    out.fields.emplace("error", jarray({jstring(error.type), jstring(error.message)}));
  }
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
      const auto dot = qualified.find('.');
      const auto module = qualified.substr(0, dot);
      const auto function = qualified.substr(dot + 1);
      Args args;
      for (const auto& arg : node.at("args").items) args.push_back(&arg);
      const auto& kwargs = node.at("kwargs");
      const auto actual = run_call([&]() -> Json {
        if (module == "pd_filtering_constants") return pd_filtering_constants_module(function);
        if (module == "start_activities") return start_activities_module(function, df, args, kwargs);
        if (module == "end_activities") return end_activities_module(function, df, args, kwargs);
        if (module == "traces") return traces_module(function, df, args);
        if (module == "starts_with") return starts_with_module(function, df, args);
        if (module == "ends_with") return ends_with_module(function, df, args);
        if (module == "rework") return rework_module(function, df, args);
        if (module == "prefixes") return prefixes_module(function, df, args);
        if (module == "suffixes") return suffixes_module(function, df, args);
        if (module == "consecutive_act_case_grouping") return consecutive_act_case_grouping_module(function, df, args);
        if (module == "timestamp_case_grouping") return timestamp_case_grouping_module(function, df, args);
        if (module == "activity_split") return activity_split_module(function, df, args);
        throw std::runtime_error("unbound module " + module);
      });
      std::vector<Json> expected;
      if (const auto* single = node.get("outcome")) expected.push_back(normalized(*single));
      else
        for (const auto& observed : node.at("observed").items) expected.push_back(normalized(observed));
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
