#include "minera/ocel.hpp"
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/conformance.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/timestamp.hpp"
#include "ocpn_projection.hpp"
#include "reference_tree_net.hpp"

#include <algorithm>
#include <string_view>
#include <bit>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace minera {
namespace {

// Default elapsed edge timing. Exact parsing keeps microseconds that a
// double epoch value loses at present-day magnitudes, and the conversion is
// shared with the typed timing path. Shapes the exact parser rejects keep the
// historic double projection.
std::optional<double> legacy_edge_elapsed(const std::string& from_text,
                                          const std::string& to_text) {
  const auto from = parse_timestamp_exact(from_text);
  const auto to = parse_timestamp_exact(to_text);
  if (from && to) return ocdfg_elapsed_seconds(*from, *to);
  const auto from_seconds = parse_timestamp(from_text);
  const auto to_seconds = parse_timestamp(to_text);
  if (from_seconds && to_seconds) return *to_seconds - *from_seconds;
  return std::nullopt;
}

struct Json {
  using Object = std::map<std::string, Json>;
  using Array = std::vector<Json>;
  std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string, Array,
               Object>
      value;
  // An object's keys in document order, as the reference's dict keeps them.
  std::vector<std::string> keys;
  bool is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
  bool is_bool() const { return std::holds_alternative<bool>(value); }
  bool is_int() const { return std::holds_alternative<std::int64_t>(value); }
  bool is_uint() const { return std::holds_alternative<std::uint64_t>(value); }
  bool is_number() const { return is_int() || std::holds_alternative<double>(value); }
  bool is_string() const { return std::holds_alternative<std::string>(value); }
  bool is_scalar() const {
    return is_null() || is_bool() || is_number() || is_string();
  }
  bool is_array() const { return std::holds_alternative<Array>(value); }
  bool is_object() const { return std::holds_alternative<Object>(value); }
  const Object& object() const { return std::get<Object>(value); }
  const Array& array() const { return std::get<Array>(value); }
  const std::string& string() const { return std::get<std::string>(value); }
};

class Parser {
 public:
  Parser(std::string_view input, const OcelJsonLimits& limits,
         bool unsigned_graph_counts = false)
      : input_(input), limits_(limits), unsigned_graph_counts_(unsigned_graph_counts) {
    if (input_.size() > limits_.max_bytes) fail("JSON exceeds size limit");
  }
  Json parse() {
    skip();
    Json result = value(0);
    skip();
    if (pos_ != input_.size()) fail("trailing JSON data");
    return result;
  }

 private:
  [[noreturn]] void fail(const char* message) const {
    throw std::invalid_argument(std::string("invalid OCEL JSON at ") +
                                std::to_string(pos_) + ": " + message);
  }
  void skip() {
    while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\n' ||
                                    input_[pos_] == '\r' || input_[pos_] == '\t'))
      ++pos_;
  }
  void expect(char c) {
    skip();
    if (pos_ >= input_.size() || input_[pos_] != c) fail("unexpected character");
    ++pos_;
  }
  static void append_utf8(std::string& out, unsigned code) {
    if (code <= 0x7f) out.push_back(static_cast<char>(code));
    else if (code <= 0x7ff) { out.push_back(static_cast<char>(0xc0 | (code >> 6))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
    else if (code <= 0xffff) { out.push_back(static_cast<char>(0xe0 | (code >> 12))); out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
    else { out.push_back(static_cast<char>(0xf0 | (code >> 18))); out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f))); out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
  }
  unsigned unicode_escape() {
    if (pos_ + 4 > input_.size()) fail("short unicode escape");
    unsigned code = 0;
    for (int i = 0; i < 4; ++i) {
      const char h = input_[pos_++];
      code <<= 4;
      if (h >= '0' && h <= '9') code += h - '0';
      else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
      else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
      else fail("bad unicode escape");
    }
    return code;
  }
  std::string string_value() {
    expect('"');
    std::string out;
    while (pos_ < input_.size()) {
      unsigned char c = static_cast<unsigned char>(input_[pos_++]);
      if (c == '"') {
        if (out.size() > limits_.max_string_bytes) fail("string exceeds size limit");
        return out;
      }
      if (c < 0x20) fail("control character in string");
      if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
      if (pos_ >= input_.size()) fail("unterminated escape");
      const char esc = input_[pos_++];
      switch (esc) {
        case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break; case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break; case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break;
        case 'u': {
          unsigned code = unicode_escape();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (input_.substr(pos_, 2) != "\\u")
              fail("missing low surrogate");
            pos_ += 2;
            const unsigned low = unicode_escape();
            if (low < 0xdc00 || low > 0xdfff) fail("bad low surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
          } else if (code >= 0xdc00 && code <= 0xdfff) {
            fail("unexpected low surrogate");
          }
          append_utf8(out, code); break;
        }
        default: fail("unknown escape");
      }
      if (out.size() > limits_.max_string_bytes) fail("string exceeds size limit");
    }
    fail("unterminated string");
  }
  Json value(std::size_t depth) {
    if (depth > limits_.max_depth) fail("nesting exceeds depth limit");
    if (++nodes_ > limits_.max_nodes) fail("JSON node limit exceeded");
    skip();
    if (pos_ >= input_.size()) fail("unexpected end");
    const char c = input_[pos_];
    if (c == '"') return Json{string_value()};
    if (c == '{') {
      ++pos_; Json::Object object; std::vector<std::string> keys; skip();
      if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; return Json{std::move(object), {}}; }
      while (true) {
        skip(); if (pos_ >= input_.size() || input_[pos_] != '"') fail("object key must be string");
        auto key = string_value(); if (object.find(key) != object.end()) fail("duplicate object key");
        keys.push_back(key);
        expect(':'); object.emplace(std::move(key), value(depth + 1)); skip();
        if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; break; }
        expect(',');
      }
      return Json{std::move(object), std::move(keys)};
    }
    if (c == '[') {
      ++pos_; Json::Array array; skip();
      if (pos_ < input_.size() && input_[pos_] == ']') { ++pos_; return Json{std::move(array)}; }
      while (true) { array.push_back(value(depth + 1)); skip(); if (pos_ < input_.size() && input_[pos_] == ']') { ++pos_; break; } expect(','); }
      return Json{std::move(array)};
    }
    if (input_.substr(pos_, 4) == "true") { pos_ += 4; return Json{true}; }
    if (input_.substr(pos_, 5) == "false") { pos_ += 5; return Json{false}; }
    if (input_.substr(pos_, 4) == "null") { pos_ += 4; return Json{nullptr}; }
    const std::size_t start = pos_;
    if (input_[pos_] == '-') ++pos_;
    if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("expected value");
    if (input_[pos_] == '0') ++pos_; else while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
    bool real = false;
    if (pos_ < input_.size() && input_[pos_] == '.') { real = true; ++pos_; if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("bad number"); while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_; }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) { real = true; ++pos_; if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_; if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("bad exponent"); while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_; }
    const auto token = input_.substr(start, pos_ - start);
    if (real) { std::string text(token); char* end = nullptr; const double n = std::strtod(text.c_str(), &end); if (end != text.c_str() + text.size() || !std::isfinite(n)) fail("bad number"); return Json{n}; }
    std::int64_t n = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), n);
    if (result.ec == std::errc() && result.ptr == token.data() + token.size())
      return Json{n};
    if (unsigned_graph_counts_ && token.front() != '-') {
      std::uint64_t count = 0;
      const auto unsigned_result =
          std::from_chars(token.data(), token.data() + token.size(), count);
      if (unsigned_result.ec == std::errc() &&
          unsigned_result.ptr == token.data() + token.size())
        return Json{count};
    }
    fail("integer out of range");
  }
  std::string_view input_; const OcelJsonLimits& limits_; std::size_t pos_ = 0; std::size_t nodes_ = 0;
  bool unsigned_graph_counts_ = false;
};

const Json& required(const Json::Object& o, std::string_view key) {
  auto it = o.find(std::string(key)); if (it == o.end()) throw std::invalid_argument("missing OCEL field: " + std::string(key)); return it->second;
}
const Json* optional(const Json::Object& o, std::string_view key) { auto it = o.find(std::string(key)); return it == o.end() ? nullptr : &it->second; }
std::string str(const Json& j, std::string_view field) { if (!j.is_string()) throw std::invalid_argument("OCEL field must be string: " + std::string(field)); return j.string(); }
OcelValue scalar(const Json& j, std::string_view field) {
  if (j.is_null()) return nullptr; if (j.is_bool()) return std::get<bool>(j.value); if (j.is_int()) return std::get<std::int64_t>(j.value); if (std::holds_alternative<double>(j.value)) return std::get<double>(j.value); if (j.is_string()) return j.string(); throw std::invalid_argument("OCEL attribute must be scalar: " + std::string(field));
}
void attrs(const Json* j, std::vector<OcelAttribute>& out, std::string_view field) {
  if (!j) return; if (!j->is_object()) throw std::invalid_argument("OCEL attributes must be object: " + std::string(field));
  for (const auto& [key, val] : j->object()) { if (key.rfind("ocel:", 0) == 0) continue; out.push_back({key, scalar(val, key)}); }
}
const Json::Object& obj(const Json& j, std::string_view field) { if (!j.is_object()) throw std::invalid_argument("OCEL field must be object: " + std::string(field)); return j.object(); }

std::size_t size_value(const Json& value, std::string_view field) {
  if (value.is_uint()) {
    const auto count = std::get<std::uint64_t>(value.value);
    if (count <= std::numeric_limits<std::size_t>::max())
      return static_cast<std::size_t>(count);
  }
  if (!value.is_int() || std::get<std::int64_t>(value.value) < 0 ||
      static_cast<std::uint64_t>(std::get<std::int64_t>(value.value)) >
          static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    throw std::invalid_argument("graph JSON field must be a non-negative integer: " +
                                std::string(field));
  return static_cast<std::size_t>(std::get<std::int64_t>(value.value));
}

double number_value(const Json& value, std::string_view field) {
  if (value.is_int()) return static_cast<double>(std::get<std::int64_t>(value.value));
  if (std::holds_alternative<double>(value.value)) {
    const auto result = std::get<double>(value.value);
    if (std::isfinite(result)) return result;
  }
  throw std::invalid_argument("graph JSON field must be a finite number: " +
                              std::string(field));
}

std::vector<std::string> string_array(const Json* raw,
                                      std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<std::string> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array())
    result.push_back(str(value, field));
  return result;
}

std::vector<std::pair<std::string, std::string>> pair_array(
    const Json* raw, std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array()) {
    if (!value.is_array() || value.array().size() != 2)
      throw std::invalid_argument("graph JSON pair must contain two strings: " +
                                  std::string(field));
    result.emplace_back(str(value.array()[0], field),
                        str(value.array()[1], field));
  }
  return result;
}

std::vector<std::tuple<std::string, std::string, std::string>> triple_array(
    const Json* raw, std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<std::tuple<std::string, std::string, std::string>> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array()) {
    if (!value.is_array() || value.array().size() != 3)
      throw std::invalid_argument("graph JSON triple must contain three strings: " +
                                  std::string(field));
    result.emplace_back(str(value.array()[0], field),
                        str(value.array()[1], field),
                        str(value.array()[2], field));
  }
  return result;
}

std::vector<double> number_array(const Json* raw, std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<double> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array()) result.push_back(number_value(value, field));
  return result;
}

using StandardAttributeTypes =
    std::map<std::string, std::map<std::string, std::string>>;

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

void parse_standard_type_table(const Json* raw, StandardAttributeTypes& types,
                               std::string_view field, bool literal_names) {
  if (!raw) return;
  if (!raw->is_array())
    throw std::invalid_argument("OCEL " + std::string(field) +
                                " must be an array");
  for (const auto& entry : raw->array()) {
    const auto& descriptor = obj(entry, field);
    const auto name = str(required(descriptor, "name"), "type name");
    if ((!literal_names && name.empty()) || types.contains(name))
      throw std::invalid_argument("duplicate or empty OCEL type name");
    std::map<std::string, std::string> attributes;
    if (const auto* raw_attributes = optional(descriptor, "attributes")) {
      if (!raw_attributes->is_array())
        throw std::invalid_argument("OCEL type attributes must be an array");
      for (const auto& raw_attribute : raw_attributes->array()) {
        const auto& attribute = obj(raw_attribute, "type attributes[]");
        const auto attribute_name =
            str(required(attribute, "name"), "attribute name");
        const auto attribute_type =
            str(required(attribute, "type"), "attribute type");
        if ((!literal_names && attribute_name.empty()) || attribute_type.empty() ||
            !attributes.emplace(attribute_name, attribute_type).second)
          throw std::invalid_argument("duplicate or empty OCEL attribute type");
      }
    }
    types.emplace(name, std::move(attributes));
  }
}

void parse_classic_type_catalog(const Json* raw, OcelTypeCatalog& catalog,
                                std::string_view field, bool literal_names) {
  if (!raw) return;
  if (!raw->is_object())
    throw std::invalid_argument("OCEL " + std::string(field) +
                                " must be an object");
  for (const auto& [type, raw_attributes] : raw->object()) {
    if (!literal_names && type.empty())
      throw std::invalid_argument("OCEL type name is empty");
    const auto& attributes = obj(raw_attributes, field);
    auto& declared = catalog[type];
    for (const auto& [name, raw_type] : attributes) {
      if (!literal_names && name.empty())
        throw std::invalid_argument("OCEL attribute name is empty");
      const auto attribute_type = str(raw_type, "OCEL attribute type");
      if (attribute_type.empty())
        throw std::invalid_argument("OCEL attribute type is empty");
      const auto [it, inserted] = declared.emplace(name, attribute_type);
      if (!inserted && it->second != attribute_type)
        throw std::invalid_argument("conflicting OCEL attribute type");
    }
  }
}

const std::string* standard_attribute_type(const StandardAttributeTypes& types,
                                            std::string_view owner,
                                            std::string_view attribute) {
  const auto owner_it = types.find(std::string(owner));
  if (owner_it == types.end()) return nullptr;
  const auto attribute_it = owner_it->second.find(std::string(attribute));
  return attribute_it == owner_it->second.end() ? nullptr : &attribute_it->second;
}

OcelValue standard_scalar(const Json& value, std::string_view field,
                          const std::string* declared_type) {
  if (declared_type == nullptr || declared_type->empty())
    return scalar(value, field);
  const auto type = lowercase(*declared_type);
  if (value.is_null()) return nullptr;
  if (type.find("bool") != std::string::npos) {
    if (value.is_bool()) return std::get<bool>(value.value);
    if (value.is_string()) {
      const auto text = lowercase(value.string());
      if (text == "true" || text == "1") return true;
      if (text == "false" || text == "0") return false;
    }
    throw std::invalid_argument("OCEL boolean attribute has invalid value: " +
                                std::string(field));
  }
  if (type.find("int") != std::string::npos ||
      type.find("long") != std::string::npos) {
    if (value.is_int()) return std::get<std::int64_t>(value.value);
    if (value.is_string()) {
      std::int64_t result = 0;
      const auto text = value.string();
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                          result);
      if (parsed.ec == std::errc() &&
          parsed.ptr == text.data() + text.size())
        return result;
    }
    if (std::holds_alternative<double>(value.value)) {
      const double number = std::get<double>(value.value);
      if (std::isfinite(number) && std::floor(number) == number &&
          number >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
          number <= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
        return static_cast<std::int64_t>(number);
    }
    throw std::invalid_argument("OCEL integer attribute has invalid value: " +
                                std::string(field));
  }
  if (type.find("float") != std::string::npos ||
      type.find("double") != std::string::npos ||
      type == "number") {
    if (std::holds_alternative<double>(value.value))
      return std::get<double>(value.value);
    if (value.is_int())
      return static_cast<double>(std::get<std::int64_t>(value.value));
    if (value.is_string()) {
      const auto text = value.string();
      char* end = nullptr;
      const double number = std::strtod(text.c_str(), &end);
      if (end == text.c_str() + text.size() && std::isfinite(number))
        return number;
    }
    throw std::invalid_argument("OCEL floating attribute has invalid value: " +
                                std::string(field));
  }
  if (type.find("date") != std::string::npos ||
      type.find("time") != std::string::npos || type == "string" ||
      type == "str") {
    if (value.is_string()) return value.string();
    throw std::invalid_argument("OCEL textual attribute has invalid value: " +
                                std::string(field));
  }
  return scalar(value, field);
}

Ocel parse_classic(const Json::Object& root, bool literal_names) {
  Ocel log; std::unordered_set<std::string> object_ids, event_ids; std::unordered_map<std::string, std::string> types;
  parse_classic_type_catalog(optional(root, "ocel:objectTypes"),
                             log.object_type_attributes, "ocel:objectTypes",literal_names);
  parse_classic_type_catalog(optional(root, "ocel:eventTypes"),
                             log.event_type_attributes, "ocel:eventTypes",literal_names);
  // Objects and events are read in document order; the stable timestamp
  // sort then keeps that order among equal instants.
  const auto& objects_json = required(root, "ocel:objects");
  const auto& objects = obj(objects_json, "ocel:objects");
  for (const auto& id : objects_json.keys) {
    const auto& raw = objects.at(id);
    if (!object_ids.insert(id).second) throw std::invalid_argument("duplicate object id");
    const auto& x = obj(raw, "object"); OcelObject o{id, str(required(x, "ocel:type"), "ocel:type"), {}}; attrs(optional(x, "ocel:ovmap"), o.attributes, "ocel:ovmap"); log.objects.push_back(std::move(o)); log.object_type_attributes.try_emplace(log.objects.back().type); types.emplace(id, log.objects.back().type);
    if (const auto* rs = optional(x, "ocel:o2o")) { if (!rs->is_array()) throw std::invalid_argument("ocel:o2o must be array"); for (const auto& r : rs->array()) { const auto& rr = obj(r, "ocel:o2o"); log.o2o.push_back({id, str(required(rr, "ocel:oid"), "ocel:oid"), optional(rr, "ocel:qualifier") ? str(*optional(rr, "ocel:qualifier"), "ocel:qualifier") : ""}); } }
  }
  const auto& events_json = required(root, "ocel:events");
  const auto& events = obj(events_json, "ocel:events");
  for (const auto& id : events_json.keys) {
    const auto& raw = events.at(id);
    if (!event_ids.insert(id).second) throw std::invalid_argument("duplicate event id");
    const auto& x = obj(raw, "event"); OcelEvent e{id, str(required(x, "ocel:activity"), "ocel:activity"), str(required(x, "ocel:timestamp"), "ocel:timestamp"), {}}; attrs(optional(x, "ocel:vmap"), e.attributes, "ocel:vmap"); log.events.push_back(std::move(e)); log.event_type_attributes.try_emplace(log.events.back().activity);
    const auto* omap = optional(x, "ocel:omap"); if (!omap || !omap->is_array()) throw std::invalid_argument("ocel:omap must be array");
    std::unordered_set<std::string> seen;
    std::map<std::string,std::vector<std::size_t>> occurrences;
    for (const auto& oidj : omap->array()) {
      const auto oid = str(oidj, "ocel:omap");
      if (!types.count(oid)) throw std::invalid_argument("relation references unknown object");
      const bool first=seen.insert(oid).second;
      if(literal_names || first){occurrences[oid].push_back(log.relations.size());log.relations.push_back({id,oid,""});}
    }
    if (const auto* typed = optional(x, "ocel:typedOmap")) {
      if (!typed->is_array()) throw std::invalid_argument("ocel:typedOmap must be array");
      std::map<std::string,std::size_t> assigned;
      for (const auto& r : typed->array()) {
        const auto& rr=obj(r,"ocel:typedOmap");
        const auto oid=str(required(rr,"ocel:oid"),"ocel:oid");
        if(!types.count(oid)||!seen.count(oid))throw std::invalid_argument("typed relation references unknown object");
        const auto qualifier=optional(rr,"ocel:qualifier")?str(*optional(rr,"ocel:qualifier"),"ocel:qualifier"):"";
        if(literal_names){
          const auto index=assigned[oid]++;
          if(index<occurrences[oid].size())log.relations[occurrences[oid][index]].qualifier=qualifier;
          else log.relations.push_back({id,oid,qualifier});
        }else for(auto& rel:log.relations)if(rel.event_id==id&&rel.object_id==oid)rel.qualifier=qualifier;
      }
    }
  }
  if (const auto* raw_e2e = optional(root, "ocel:e2e")) {
    if (!raw_e2e->is_array())
      throw std::invalid_argument("ocel:e2e must be array");
    for (const auto& raw : raw_e2e->array()) {
      const auto& relation = obj(raw, "ocel:e2e");
      const auto* source = optional(relation, "ocel:eid");
      const auto* target = optional(relation, "ocel:eid_2");
      if (source == nullptr || target == nullptr)
        throw std::invalid_argument("ocel:e2e requires source and target event ids");
      const auto source_id = str(*source, "ocel:eid");
      const auto target_id = str(*target, "ocel:eid_2");
      if (!event_ids.count(source_id) || !event_ids.count(target_id))
        throw std::invalid_argument("ocel:e2e references unknown event");
      const auto* qualifier = optional(relation, "ocel:qualifier");
      log.e2e.push_back({source_id, target_id,
                         qualifier == nullptr ? std::string{} : str(*qualifier, "ocel:qualifier")});
    }
  }
  if (const auto* ch = optional(root, "ocel:objectChanges")) { if (!ch->is_array()) throw std::invalid_argument("ocel:objectChanges must be array"); for (const auto& r : ch->array()) { const auto& rr = obj(r, "ocel:objectChanges"); OcelObjectChange c; c.object_id = str(required(rr, "ocel:oid"), "ocel:oid"); if (!types.count(c.object_id)) throw std::invalid_argument("object change references unknown object"); c.object_type = optional(rr, "ocel:type") ? str(*optional(rr, "ocel:type"), "ocel:type") : types[c.object_id]; c.timestamp = str(required(rr, "ocel:timestamp"), "ocel:timestamp"); c.field = str(required(rr, "ocel:field"), "ocel:field"); c.value = scalar(required(rr, c.field), c.field); log.object_changes.push_back(std::move(c)); } }
  for (const auto& category : {"ocel:global-log", "ocel:global-event", "ocel:global-object"}) if (const auto* g = optional(root, category)) { for (const auto& [key, value] : obj(*g, category)) { if (value.is_scalar()) log.globals[std::string(category) + "." + key] = scalar(value, key); } }
  for (const auto& r : log.o2o) if (!types.count(r.target_id)) throw std::invalid_argument("o2o relation references unknown object");
  return log;
}

Ocel parse_standard(const Json::Object& root, bool literal_names) {
  Ocel log;
  std::unordered_map<std::string, std::string> types;
  std::unordered_set<std::string> event_ids;
  StandardAttributeTypes object_attribute_types;
  StandardAttributeTypes event_attribute_types;
  parse_standard_type_table(optional(root, "objectTypes"), object_attribute_types,
                            "objectTypes",literal_names);
  parse_standard_type_table(optional(root, "eventTypes"), event_attribute_types,
                            "eventTypes",literal_names);
  log.object_type_attributes = object_attribute_types;
  log.event_type_attributes = event_attribute_types;
  const auto& object_array = required(root, "objects");
  if (!object_array.is_array())
    throw std::invalid_argument("objects must be array");
  struct StandardAttribute {
    std::string name;
    OcelValue value;
    bool has_time = false;
    std::string time;
  };
  for (const auto& raw : object_array.array()) {
    const auto& x = obj(raw, "objects[]");
    const auto id = str(required(x, "id"), "id");
    if ((!literal_names && id.empty()) || types.count(id))
      throw std::invalid_argument("duplicate or empty object id");
    const auto type = str(required(x, "type"), "type");
    types.emplace(id, type);
    OcelObject object{id, type, {}};
    if (const auto* aa = optional(x, "attributes")) {
      if (!aa->is_array())
        throw std::invalid_argument("object attributes must be array");
      std::map<std::string, std::vector<StandardAttribute>> grouped;
      for (const auto& ar : aa->array()) {
        const auto& a = obj(ar, "attributes[]");
        const auto name = str(required(a, "name"), "name");
        if (!literal_names && name.empty())
          throw std::invalid_argument("OCEL object attribute name is empty");
        StandardAttribute entry{name,
                                standard_scalar(
                                    required(a, "value"), "value",
                                    standard_attribute_type(object_attribute_types,
                                                            type, name))};
        if (const auto* time = optional(a, "time")) {
          entry.has_time = true;
          entry.time = str(*time, "time");
        }
        grouped[name].push_back(std::move(entry));
      }
      for (auto& [name, entries] : grouped) {
        if (entries.empty()) continue;
        object.attributes.push_back({name, entries.front().value});
        for (std::size_t i = 1; i < entries.size(); ++i) {
          if (!entries[i].has_time)
            throw std::invalid_argument(
                "OCEL object attribute change requires a timestamp");
          log.object_changes.push_back(
              {id, type, entries[i].time, name, entries[i].value});
        }
      }
    }
    log.objects.push_back(std::move(object));
    log.object_type_attributes.try_emplace(type);
    if (const auto* rr = optional(x, "relationships")) {
      if (!rr->is_array())
        throw std::invalid_argument("object relationships must be array");
      for (const auto& r : rr->array()) {
        const auto& y = obj(r, "relationship");
        log.o2o.push_back(
            {id, str(required(y, "objectId"), "objectId"),
             optional(y, "qualifier")
                 ? str(*optional(y, "qualifier"), "qualifier")
                 : ""});
      }
    }
  }
  const auto& event_array = required(root, "events");
  if (!event_array.is_array())
    throw std::invalid_argument("events must be array");
  for (const auto& raw : event_array.array()) {
    const auto& x = obj(raw, "events[]");
    const auto id = str(required(x, "id"), "id");
    if ((!literal_names && id.empty()) || !event_ids.insert(id).second)
      throw std::invalid_argument("duplicate or empty event id");
    const auto activity = str(required(x, "type"), "type");
    OcelEvent event{id, activity, str(required(x, "time"), "time"), {}};
    if (const auto* aa = optional(x, "attributes")) {
      if (!aa->is_array())
        throw std::invalid_argument("event attributes must be array");
      for (const auto& ar : aa->array()) {
        const auto& a = obj(ar, "attributes[]");
        const auto name = str(required(a, "name"), "name");
        event.attributes.push_back(
            {name, standard_scalar(
                       required(a, "value"), "value",
                       standard_attribute_type(event_attribute_types, activity,
                                               name))});
      }
    }
    log.events.push_back(std::move(event));
    log.event_type_attributes.try_emplace(activity);
    if (const auto* rels = optional(x, "relationships")) {
      if (!rels->is_array())
        throw std::invalid_argument("event relationships must be array");
      for (const auto& r : rels->array()) {
        const auto& y = obj(r, "relationship");
        const auto oid = str(required(y, "objectId"), "objectId");
        if (!types.count(oid))
          throw std::invalid_argument("relation references unknown object");
        log.relations.push_back(
            {id, oid, optional(y, "qualifier")
                         ? str(*optional(y, "qualifier"), "qualifier")
                         : ""});
      }
    }
  }
  const Json* raw_e2e = optional(root, "eventRelationships");
  if (raw_e2e == nullptr) raw_e2e = optional(root, "e2e");
  if (raw_e2e != nullptr) {
    if (!raw_e2e->is_array())
      throw std::invalid_argument("eventRelationships must be array");
    for (const auto& raw : raw_e2e->array()) {
      const auto& relation = obj(raw, "eventRelationships[]");
      const auto* source = optional(relation, "sourceEventId");
      if (source == nullptr) source = optional(relation, "source");
      const auto* target = optional(relation, "targetEventId");
      if (target == nullptr) target = optional(relation, "target");
      if (source == nullptr || target == nullptr)
        throw std::invalid_argument("eventRelationships requires sourceEventId and targetEventId");
      const auto source_id = str(*source, "sourceEventId");
      const auto target_id = str(*target, "targetEventId");
      if (!event_ids.count(source_id) || !event_ids.count(target_id))
        throw std::invalid_argument("eventRelationships references unknown event");
      const auto* qualifier = optional(relation, "qualifier");
      log.e2e.push_back({source_id, target_id,
                         qualifier == nullptr ? std::string{} : str(*qualifier, "qualifier")});
    }
  }
  if (const auto* globals = optional(root, "globals")) {
    for (const auto& [key, value] : obj(*globals, "globals")) {
      log.globals[key] = scalar(value, key);
    }
  }
  for (const auto& r : log.o2o) if (!types.count(r.target_id)) throw std::invalid_argument("o2o relation references unknown object");
  return log;
}

void write_string(std::ostringstream& out, std::string_view s) { out << '"'; for (unsigned char c : s) { switch (c) { case '"': out << "\\\""; break; case '\\': out << "\\\\"; break; case '\b': out << "\\b"; break; case '\f': out << "\\f"; break; case '\n': out << "\\n"; break; case '\r': out << "\\r"; break; case '\t': out << "\\t"; break; default: if (c < 0x20) { out << "\\u00" << "0123456789abcdef"[(c >> 4) & 15] << "0123456789abcdef"[c & 15]; } else out << static_cast<char>(c); } } out << '"'; }
void write_value(std::ostringstream& out, const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) {
    out << "null";
  } else if (const auto* bool_value = std::get_if<bool>(&value)) {
    out << (*bool_value ? "true" : "false");
  } else if (const auto* int_value = std::get_if<std::int64_t>(&value)) {
    out << *int_value;
  } else if (const auto* double_value = std::get_if<double>(&value)) {
    out.precision(17);
    out << *double_value;
  } else {
    write_string(out, std::get<std::string>(value));
  }
}
void write_attrs(std::ostringstream& out, const std::vector<OcelAttribute>& attrs) { std::map<std::string, const OcelValue*> sorted; for (const auto& a : attrs) sorted[a.key] = &a.value; out << '{'; bool first = true; for (const auto& [k, v] : sorted) { if (!first) out << ','; first = false; write_string(out, k); out << ':'; write_value(out, *v); } out << '}'; }

std::string classic_value_type(const OcelValue& value) {
  if (std::holds_alternative<bool>(value)) return "boolean";
  if (std::holds_alternative<std::int64_t>(value)) return "int";
  if (std::holds_alternative<double>(value)) return "float";
  return "string";
}

void collect_classic_types(const Ocel& log, OcelTypeCatalog& object_types,
                           OcelTypeCatalog& event_types) {
  object_types = log.object_type_attributes;
  event_types = log.event_type_attributes;
  for (const auto& object : log.objects) {
    auto& attributes = object_types[object.type];
    for (const auto& attribute : object.attributes) {
      auto& declared = attributes[attribute.key];
      if (declared.empty()) declared = classic_value_type(attribute.value);
    }
  }
  for (const auto& change : log.object_changes) {
    auto& declared = object_types[change.object_type][change.field];
    if (declared.empty()) declared = classic_value_type(change.value);
  }
  for (const auto& event : log.events) {
    auto& attributes = event_types[event.activity];
    for (const auto& attribute : event.attributes) {
      auto& declared = attributes[attribute.key];
      if (declared.empty()) declared = classic_value_type(attribute.value);
    }
  }
}

void write_classic_type_catalog(std::ostringstream& out,
                                const OcelTypeCatalog& catalog) {
  out << '{';
  bool first_type = true;
  for (const auto& [type, attributes] : catalog) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, type);
    out << ':';
    out << '{';
    bool first_attribute = true;
    for (const auto& [name, declared_type] : attributes) {
      if (!first_attribute) out << ',';
      first_attribute = false;
      write_string(out, name);
      out << ':';
      write_string(out, declared_type.empty() ? "string" : declared_type);
    }
    out << '}';
  }
  out << '}';
}

std::string standard_value_type(const OcelValue& value) {
  if (std::holds_alternative<bool>(value)) return "boolean";
  if (std::holds_alternative<std::int64_t>(value)) return "int";
  if (std::holds_alternative<double>(value)) return "float";
  return "string";
}

void merge_standard_type(std::string& current, const OcelValue& value) {
  const auto next = standard_value_type(value);
  if (current.empty() || current == next) {
    current = next;
  } else if ((current == "int" && next == "float") ||
             (current == "float" && next == "int")) {
    current = "float";
  } else {
    current = "string";
  }
}

using StandardTypeTable =
    std::map<std::string, std::map<std::string, std::string>>;

void collect_standard_types(const Ocel& log, StandardTypeTable& object_types,
                            StandardTypeTable& event_types) {
  object_types = log.object_type_attributes;
  event_types = log.event_type_attributes;
  for (const auto& object : log.objects) {
    auto& attributes = object_types[object.type];
    for (const auto& attribute : object.attributes) {
      auto& declared = attributes[attribute.key];
      if (declared.empty()) merge_standard_type(declared, attribute.value);
    }
  }
  for (const auto& change : log.object_changes) {
    auto& declared = object_types[change.object_type][change.field];
    if (declared.empty()) merge_standard_type(declared, change.value);
  }
  for (const auto& event : log.events) {
    auto& attributes = event_types[event.activity];
    for (const auto& attribute : event.attributes) {
      auto& declared = attributes[attribute.key];
      if (declared.empty()) merge_standard_type(declared, attribute.value);
    }
  }
}

void write_standard_type_table(std::ostringstream& out,
                               const StandardTypeTable& types) {
  out << '[';
  bool first_type = true;
  for (const auto& [type, attributes] : types) {
    if (!first_type) out << ',';
    first_type = false;
    out << "{\"name\":";
    write_string(out, type);
    out << ",\"attributes\":[";
    bool first_attribute = true;
    for (const auto& [name, declared_type] : attributes) {
      if (!first_attribute) out << ',';
      first_attribute = false;
      out << "{\"name\":";
      write_string(out, name);
      out << ",\"type\":";
      write_string(out, declared_type.empty() ? "string" : declared_type);
      out << '}';
    }
    out << "]}";
  }
  out << ']';
}

void write_o2o(std::ostringstream& out, const Ocel& log, std::string_view source) {
  std::vector<const OcelO2oRelation*> relations;
  for (const auto& relation : log.o2o) {
    if (relation.source_id == source) relations.push_back(&relation);
  }
  std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                   const auto* right) {
    return std::tie(left->target_id, left->qualifier) <
           std::tie(right->target_id, right->qualifier);
  });
  out << '[';
  bool first = true;
  for (const auto* relation : relations) {
    if (!first) out << ',';
    first = false;
    out << "{\"ocel:oid\":";
    write_string(out, relation->target_id);
    out << ",\"ocel:qualifier\":";
    write_string(out, relation->qualifier);
    out << '}';
  }
  out << ']';
}

void write_e2e(std::ostringstream& out, const Ocel& log,
               bool standard_names) {
  std::vector<const OcelE2eRelation*> relations;
  relations.reserve(log.e2e.size());
  for (const auto& relation : log.e2e) relations.push_back(&relation);
  std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                   const auto* right) {
    return std::tie(left->source_id, left->target_id, left->qualifier) <
           std::tie(right->source_id, right->target_id, right->qualifier);
  });
  out << '[';
  for (std::size_t i = 0; i < relations.size(); ++i) {
    if (i != 0) out << ',';
    const auto& relation = *relations[i];
    if (standard_names) {
      out << "{\"sourceEventId\":";
      write_string(out, relation.source_id);
      out << ",\"targetEventId\":";
      write_string(out, relation.target_id);
      out << ",\"qualifier\":";
      write_string(out, relation.qualifier);
      out << '}';
    } else {
      out << "{\"ocel:eid\":";
      write_string(out, relation.source_id);
      out << ",\"ocel:eid_2\":";
      write_string(out, relation.target_id);
      out << ",\"ocel:qualifier\":";
      write_string(out, relation.qualifier);
      out << '}';
    }
  }
  out << ']';
}

void write_global_category(std::ostringstream& out, const Ocel& log,
                           std::string_view category) {
  std::map<std::string, const OcelValue*> values;
  const std::string prefix = std::string(category) + ".";
  for (const auto& [key, value] : log.globals) {
    if (key.rfind(prefix, 0) == 0) values.emplace(key.substr(prefix.size()), &value);
  }
  out << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) out << ',';
    first = false;
    write_string(out, key);
    out << ':';
    write_value(out, *value);
  }
  out << '}';
}

void write_object_changes(std::ostringstream& out, const Ocel& log) {
  std::vector<const OcelObjectChange*> changes;
  changes.reserve(log.object_changes.size());
  for (const auto& change : log.object_changes) changes.push_back(&change);
  std::sort(changes.begin(), changes.end(), [](const auto* left, const auto* right) {
    return std::tie(left->object_id, left->timestamp, left->field,
                    left->object_type) <
           std::tie(right->object_id, right->timestamp, right->field,
                    right->object_type);
  });
  out << '[';
  bool first = true;
  for (const auto* change : changes) {
    if (!first) out << ',';
    first = false;
    out << "{\"ocel:oid\":";
    write_string(out, change->object_id);
    out << ",\"ocel:type\":";
    write_string(out, change->object_type);
    out << ",\"ocel:timestamp\":";
    write_string(out, change->timestamp);
    out << ",\"ocel:field\":";
    write_string(out, change->field);
    out << ',';
    write_string(out, change->field);
    out << ':';
    write_value(out, change->value);
    out << '}';
  }
  out << ']';
}

}  // namespace

namespace {

const OcelValue* find_attribute(const std::vector<OcelAttribute>& attributes,
                                std::string_view key) {
  for (const auto& attribute : attributes) {
    if (attribute.key == key) return &attribute.value;
  }
  return nullptr;
}

std::string selector_value(const OcelValue& value, std::string_view selector,
                           std::string_view record_kind,
                           std::string_view record_id,
                           std::string_view family) {
  if (std::holds_alternative<std::nullptr_t>(value))
    throw std::invalid_argument(std::string(family) + " " +
                                std::string(record_kind) +
                                " selector '" + std::string(selector) +
                                "' is null for " + std::string(record_id));
  if (const auto* text = std::get_if<std::string>(&value)) {
    if (text->empty())
      throw std::invalid_argument(std::string(family) + " " +
                                  std::string(record_kind) +
                                  " selector '" + std::string(selector) +
                                  "' is empty for " + std::string(record_id));
    return *text;
  }
  if (const auto* boolean = std::get_if<bool>(&value))
    return *boolean ? "true" : "false";
  if (const auto* integer = std::get_if<std::int64_t>(&value))
    return std::to_string(*integer);
  const auto number = std::get<double>(value);
  if (!std::isfinite(number))
    throw std::invalid_argument(std::string(family) + " " +
                                std::string(record_kind) +
                                " selector '" + std::string(selector) +
                                "' is non-finite for " +
                                std::string(record_id));
  std::ostringstream rendered;
  rendered << std::setprecision(std::numeric_limits<double>::max_digits10)
           << number;
  return rendered.str();
}

std::string selected_event_activity(const OcelEvent& event,
                                    std::string_view selector,
                                    std::string_view family) {
  if (selector.empty() || selector == "activity" ||
      selector == "concept:name" || selector == "ocel:activity")
    return event.activity;
  const auto* value = find_attribute(event.attributes, selector);
  if (value == nullptr)
    throw std::invalid_argument(std::string(family) +
                                " event activity attribute '" +
                                std::string(selector) + "' is missing for " +
                                event.id);
  return selector_value(*value, selector, "event activity", event.id, family);
}

std::string selected_object_type(const OcelObject& object,
                                 std::string_view selector,
                                 std::string_view family) {
  if (selector.empty() || selector == "type" || selector == "object_type" ||
      selector == "ocel:type")
    return object.type;
  const auto* value = find_attribute(object.attributes, selector);
  if (value == nullptr)
    throw std::invalid_argument(std::string(family) +
                                " object type attribute '" +
                                std::string(selector) + "' is missing for " +
                                object.id);
  return selector_value(*value, selector, "object type", object.id, family);
}

}  // namespace

Ocel project_ocel_labels(const Ocel& log, std::string_view activity_attribute,
                         std::string_view object_type_attribute,
                         std::string_view family) {
  Ocel projected = log;
  for (auto& event : projected.events)
    event.activity = selected_event_activity(event, activity_attribute, family);

  std::map<std::string, std::string> object_types;
  for (auto& object : projected.objects) {
    object.type = selected_object_type(object, object_type_attribute, family);
    object_types[object.id] = object.type;
  }
  for (auto& change : projected.object_changes) {
    const auto it = object_types.find(change.object_id);
    if (it != object_types.end()) change.object_type = it->second;
  }
  return projected;
}

Ocel project_ocpn_labels(const Ocel& log,
                         const OcpnDiscoveryOptions& options) {
  return project_ocel_labels(log, options.activity_attribute,
                             options.object_type_attribute, "OCPN");
}

static Ocel parse_ocel_json_names(std::string_view json, const OcelJsonLimits& limits,
                                 bool literal_names) {
  const Json root = Parser(json, limits).parse();
  if (!root.is_object()) throw std::invalid_argument("OCEL root must be object");
  const auto& r = root.object();
  if(const auto* marker=optional(r,"minera:literalNames")) {
    if(!marker->is_bool())throw std::invalid_argument("minera:literalNames must be boolean");
    literal_names=literal_names || std::get<bool>(marker->value);
  }
  const bool standard = r.find("events") != r.end() || r.find("objects") != r.end();
  Ocel result = standard ? parse_standard(r,literal_names) : parse_classic(r,literal_names);
  // The reference OCEL 2.0 JSON reader keeps one relation per event and
  // object; the last one wins (its qualifier and its place). The classic
  // format stays lossless because native adapters use it as transport.
  if (standard) {
    std::set<std::pair<std::string, std::string>> seen;
    std::vector<OcelRelation> kept;
    for (auto it = result.relations.rbegin(); it != result.relations.rend(); ++it)
      if (seen.emplace(it->event_id, it->object_id).second) kept.push_back(std::move(*it));
    result.relations.assign(std::make_move_iterator(kept.rbegin()),
                            std::make_move_iterator(kept.rend()));
  }
  sort_ocel_events_by_timestamp(result, standard ? OcelTimestampReader::kIsoFormat
                                                 : OcelTimestampReader::kIsoFormatOrFileOrder);
  if (const auto* raw = optional(r, "minera:tableColumns")) {
    const auto& columns = obj(*raw, "minera:tableColumns");
    const auto read = [&](const char* name,
                          std::optional<std::vector<std::string>>& target) {
      if (const auto* value = optional(columns, name))
        target = string_array(value, name);
    };
    read("events", result.table_columns.events);
    read("objects", result.table_columns.objects);
    read("relations", result.table_columns.relations);
  }
  return result;
}

namespace {

bool iso_digits(std::string_view text, std::size_t position, std::size_t count, int& value) {
  if (position + count > text.size()) return false;
  value = 0;
  for (std::size_t index = position; index < position + count; ++index) {
    if (std::isdigit(static_cast<unsigned char>(text[index])) == 0) return false;
    value = value * 10 + (text[index] - '0');
  }
  return true;
}

// Clock fields HH[:MM[:SS]] (or HHMMSS) with an optional .f/,f fraction, as
// the reference reads them (observed black-box): -1 is malformed, 0 read the
// whole text, 1 left one stray character after the last field, which the
// reference accepts only in front of a UTC offset.
int iso_clock(std::string_view text, int (&fields)[3], std::int64_t& micro) {
  fields[0] = fields[1] = fields[2] = 0;
  micro = 0;
  std::size_t position = 0;
  bool separated = false;
  bool fraction = false;
  for (int field = 0; field < 3 && !fraction; ++field) {
    if (!iso_digits(text, position, 2, fields[field])) return -1;
    position += 2;
    if (position >= text.size()) return 0;
    const char next = text[position++];
    if (field == 0) separated = next == ':';
    if (position >= text.size()) return 1;
    if (separated && next == ':') continue;
    if (next == '.' || next == ',') fraction = true;
    else if (!separated) --position;
    else return -1;
  }
  const std::size_t remaining = text.size() - position;
  const std::size_t count = std::min<std::size_t>(remaining, 6);
  int digit = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (!iso_digits(text, position + index, 1, digit)) return -1;
    micro = micro * 10 + digit;
  }
  for (std::size_t index = count; index < 6; ++index) micro *= 10;
  position += count;
  while (position < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
  // Text after six fraction digits is stray too.
  return position < text.size() ? 1 : 0;
}

std::int64_t iso_days_from_civil(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int year_of_era = year - era * 400;
  const int day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return static_cast<std::int64_t>(era) * 146097 + day_of_era - 719468;
}

int iso_monday_index(std::int64_t days) {  // Monday 0 .. Sunday 6
  return static_cast<int>(((days % 7) + 7 + 3) % 7);
}

bool iso_leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

// Wall-clock microseconds since 1970 of an ISO 8601 text as the reference
// OCEL JSON importers read it: extended or basic calendar and week dates, any
// single separator character, then clock fields and an optional Z or +/-
// offset that is validated and dropped. Throws with the reference message.
// The fixtures in minera_recheck_regression_tests and about 7,800 generated
// strings (shapes, separators, fractions, offsets) were checked against the
// pinned reference: every acceptance, value and message agrees.
std::int64_t iso_wall_microseconds(std::string_view original) {
  // A trailing Z reads as "+00:00" (so "2024-01-01Z" is midnight and a stray
  // character may precede it), also in the message, as observed.
  std::string spelled(original);
  if (!spelled.empty() && spelled.back() == 'Z') spelled.replace(spelled.size() - 1, 1, "+00:00");
  const auto invalid = [&](const std::string& reason = {}) {
    throw std::invalid_argument(reason.empty()
        ? "Invalid isoformat string: '" + spelled + "'" : reason);
  };
  const std::string_view text = spelled;
  if (text.size() < 7) invalid();
  // Where the date ends, from the shapes YYYY-MM-DD, YYYYMMDD, YYYY-Www[-D]
  // and YYYYWww[D].
  std::size_t date_end = 0;
  if (text.size() == 7) date_end = 7;
  else if (text[4] == '-') {
    if (text[5] == 'W') {
      if (text.size() < 8) invalid();
      if (text.size() > 8 && text[8] == '-') {
        if (text.size() == 9) invalid();
        date_end = text.size() > 10 && std::isdigit(static_cast<unsigned char>(text[10])) != 0 ? 8 : 10;
      } else date_end = 8;
    } else date_end = 10;
  } else if (text[4] == 'W') {
    std::size_t index = 7;
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) ++index;
    date_end = index < 9 ? index : (index % 2 == 0 ? 7 : 8);
  } else date_end = 8;
  if (date_end > text.size()) invalid();
  const auto date = text.substr(0, date_end);
  int year = 0, month = 1, day = 1, week = 0, weekday = 1;
  bool week_date = false;
  if (!iso_digits(date, 0, 4, year)) invalid();
  if (date.size() > 5 && date[4] == '-' && date[5] == 'W') {
    week_date = true;
    if (!iso_digits(date, 6, 2, week)) invalid();
    if (date.size() == 10) {
      if (date[8] != '-' || !iso_digits(date, 9, 1, weekday)) invalid();
    } else if (date.size() != 8) invalid();
  } else if (date.size() > 4 && date[4] == '-') {
    if (date.size() != 10 || date[7] != '-' || !iso_digits(date, 5, 2, month) ||
        !iso_digits(date, 8, 2, day)) invalid();
  } else if (date.size() > 4 && date[4] == 'W') {
    week_date = true;
    if (!iso_digits(date, 5, 2, week)) invalid();
    if (date.size() == 8) {
      if (!iso_digits(date, 7, 1, weekday)) invalid();
    } else if (date.size() != 7) invalid();
  } else if (date.size() != 8 || !iso_digits(date, 4, 2, month) ||
             !iso_digits(date, 6, 2, day)) invalid();
  // A week date is checked with its shape, before the clock and offset.
  if (week_date) {
    const auto january_first = iso_monday_index(iso_days_from_civil(year, 1, 1));
    const int weeks = january_first == 3 || (january_first == 2 && iso_leap(year)) ? 53 : 52;
    if (week < 1 || week > weeks || weekday < 1 || weekday > 7) invalid();
  }
  int clock[3] = {0, 0, 0};
  std::int64_t micro = 0;
  if (date_end < text.size()) {
    const auto time = text.substr(date_end + 1);  // any one separator character
    const auto zone = time.find_first_of("Z+-");
    const int read = iso_clock(time.substr(0, zone), clock, micro);
    if (read < 0 || (zone == std::string_view::npos && read != 0)) invalid();
    if (zone != std::string_view::npos) {
      if (time[zone] == 'Z') {
        if (zone + 1 != time.size()) invalid();
      } else {
        int offset[3] = {0, 0, 0};
        std::int64_t offset_micro = 0;
        if (iso_clock(time.substr(zone + 1), offset, offset_micro) != 0) invalid();
        if (offset[0] * 3600 + offset[1] * 60 + offset[2] >= 86400) {
          // The message ends with the offset's timedelta repr.
          const std::int64_t sign = time[zone] == '-' ? -1 : 1;
          const std::int64_t total = sign * ((offset[0] * 3600LL + offset[1] * 60LL + offset[2]) * 1000000 + offset_micro);
          const std::int64_t unit = 86400LL * 1000000;
          const std::int64_t days = total >= 0 ? total / unit : -((-total + unit - 1) / unit);
          const std::int64_t rest = total - days * unit;
          std::string repr = "datetime.timedelta(days=" + std::to_string(days);
          if (rest / 1000000 != 0) repr += ", seconds=" + std::to_string(rest / 1000000);
          if (rest % 1000000 != 0) repr += ", microseconds=" + std::to_string(rest % 1000000);
          invalid("offset must be a timedelta strictly between -timedelta(hours=24) and timedelta(hours=24), not " + repr + ").");
        }
      }
    }
  }
  // Calendar ranges are checked after the whole shape, as observed.
  // Year 0 fails as a calendar date; as a week date it fails converting to one.
  if (year < 1) invalid(week_date ? "month must be in 1..12" : "year " + std::to_string(year) + " is out of range");
  std::int64_t days = 0;
  if (week_date) {
    const auto january_fourth = iso_days_from_civil(year, 1, 4);
    days = january_fourth - iso_monday_index(january_fourth) + (week - 1) * 7 + (weekday - 1);
  } else {
    if (month < 1 || month > 12) invalid("month must be in 1..12");
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int last = month == 2 && iso_leap(year) ? 29 : kDays[month - 1];
    if (day < 1 || day > last) invalid("day is out of range for month");
    days = iso_days_from_civil(year, month, day);
  }
  if (clock[0] > 23) invalid("hour must be in 0..23");
  if (clock[1] > 59) invalid("minute must be in 0..59");
  if (clock[2] > 59) invalid("second must be in 0..59");
  return (days * 86400 + clock[0] * 3600 + clock[1] * 60 + clock[2]) * 1000000 + micro;
}

// OCEL 2.0 XML falls back to dateutil when fromisoformat fails. That value
// keeps its own offset (it is not made naive and stamped UTC like the ISO
// reading), so pandas compares an offset value by its instant, and a value
// without one is naive, which the reference cannot sort next to aware ones.
struct LenientTimestamp {
  std::int64_t microseconds;
  bool aware;
};

constexpr std::string_view kMonthAbbreviations[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                                     "jul", "aug", "sep", "oct", "nov", "dec"};
constexpr std::string_view kMonthNames[] = {"january", "february", "march",     "april",
                                            "may",     "june",     "july",      "august",
                                            "september", "october", "november", "december"};
constexpr std::string_view kWeekdayNames[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun",
                                              "monday", "tuesday", "wednesday", "thursday",
                                              "friday", "saturday", "sunday"};

std::string lower_ascii(std::string_view text) {
  std::string out(text);
  for (auto& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

int month_from_word(std::string_view word) {
  const auto lowered = lower_ascii(word);
  for (int month = 0; month < 12; ++month)
    if (lowered == kMonthAbbreviations[month] || lowered == kMonthNames[month]) return month + 1;
  return lowered == "sept" ? 9 : 0;
}

// A subset of dateutil.parser.parse (default dayfirst/yearfirst) observed
// black-box against the pinned reference: YYYY/MM/DD and MM/DD/YYYY with
// '/', '.' or '-' (a first field above 12 reads day-first), "Jan 2 2020",
// "2 Jan 2020" and "January 2, 2020" (month names in any case), then a clock
// H:MM[:SS[.f]] after 'T' or spaces, then an optional Z, UTC, GMT, +HH,
// +HHMM or +HH:MM offset. Anything else is nullopt (unknown to native).
std::optional<LenientTimestamp> dateutil_subset(std::string_view text) {
  std::size_t p = 0, n = text.size();
  const auto space = [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; };
  const auto digit = [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; };
  const auto alpha = [](char ch) { return std::isalpha(static_cast<unsigned char>(ch)) != 0; };
  while (p < n && space(text[p])) ++p;
  while (n > p && space(text[n - 1])) --n;
  const auto number = [&](std::size_t low, std::size_t high, int& value) {
    const std::size_t begin = p;
    value = 0;
    while (p < n && digit(text[p])) {
      if (p - begin == high) return false;
      value = value * 10 + (text[p++] - '0');
    }
    return p - begin >= low;
  };
  const auto word = [&]() {
    const std::size_t begin = p;
    while (p < n && alpha(text[p])) ++p;
    return text.substr(begin, p - begin);
  };
  const auto skip_spaces = [&]() { while (p < n && space(text[p])) ++p; };
  int year = 0, month = 0, day = 0;
  if (p < n && alpha(text[p])) {  // Jan 2[,] 2020
    month = month_from_word(word());
    skip_spaces();
    if (!month || !number(1, 2, day)) return std::nullopt;
    if (p < n && text[p] == ',') ++p;
    skip_spaces();
    if (!number(4, 4, year)) return std::nullopt;
  } else {
    const std::size_t begin = p;
    int first = 0;
    if (!number(1, 4, first)) return std::nullopt;
    const std::size_t length = p - begin;
    if (length == 4) {  // YYYY/MM/DD
      if (p >= n || (text[p] != '/' && text[p] != '.' && text[p] != '-')) return std::nullopt;
      const char separator = text[p++];
      year = first;
      if (!number(1, 2, month) || p >= n || text[p++] != separator || !number(1, 2, day))
        return std::nullopt;
    } else if (length <= 2 && p < n && space(text[p])) {  // 2 Jan[,] 2020
      skip_spaces();
      day = first;
      month = month_from_word(word());
      if (!month) return std::nullopt;
      if (p < n && text[p] == ',') ++p;
      skip_spaces();
      if (!number(4, 4, year)) return std::nullopt;
    } else if (length <= 2 && p < n && (text[p] == '/' || text[p] == '.' || text[p] == '-')) {
      const char separator = text[p++];  // MM/DD/YYYY
      int second = 0;
      if (!number(1, 2, second) || p >= n || text[p++] != separator || !number(4, 4, year))
        return std::nullopt;
      month = first;
      day = second;
      if (month > 12 && day <= 12) std::swap(month, day);
    } else {
      return std::nullopt;
    }
  }
  int hour = 0, minute = 0, second = 0;
  std::int64_t micro = 0;
  if (p < n) {
    if (text[p] == 'T') ++p;
    else if (space(text[p])) skip_spaces();
    else return std::nullopt;
    if (!number(1, 2, hour) || p >= n || text[p++] != ':' || !number(2, 2, minute)) return std::nullopt;
    if (p < n && text[p] == ':') {
      ++p;
      if (!number(2, 2, second)) return std::nullopt;
      if (p < n && (text[p] == '.' || text[p] == ',')) {
        ++p;
        std::size_t count = 0;
        while (p < n && digit(text[p])) {
          if (count < 6) micro = micro * 10 + (text[p] - '0');
          ++count;
          ++p;
        }
        if (!count) return std::nullopt;
        for (std::size_t index = count; index < 6; ++index) micro *= 10;
      }
    }
  }
  skip_spaces();
  bool aware = false;
  std::int64_t offset_seconds = 0;
  if (p < n) {
    aware = true;
    if (text[p] == 'Z') {
      ++p;
    } else if (text[p] == '+' || text[p] == '-') {
      const std::int64_t sign = text[p++] == '-' ? -1 : 1;
      const auto pair = [&](int& value) {  // exactly two digits, more may follow
        if (p + 2 > n || !digit(text[p]) || !digit(text[p + 1])) return false;
        value = (text[p] - '0') * 10 + (text[p + 1] - '0');
        p += 2;
        return true;
      };
      int hours = 0, minutes = 0;
      if (!pair(hours)) return std::nullopt;
      if (p < n && text[p] == ':') ++p;
      if (p < n && digit(text[p]) && !pair(minutes)) return std::nullopt;
      if (hours > 23 || minutes > 59) return std::nullopt;
      offset_seconds = sign * (hours * 3600 + minutes * 60);
    } else {
      const auto zone = lower_ascii(word());
      if (zone != "utc" && zone != "gmt") return std::nullopt;
    }
    if (p != n) return std::nullopt;
  }
  if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59)
    return std::nullopt;
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (day < 1 || day > (month == 2 && iso_leap(year) ? 29 : kDays[month - 1])) return std::nullopt;
  const std::int64_t wall =
      (iso_days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second) * 1000000 + micro;
  return LenientTimestamp{wall - offset_seconds * 1000000, aware};
}

// Whether dateutil's fuzzy reading certainly finds no date: no digit and no
// month or weekday word.
bool certainly_not_a_date(std::string_view text) {
  if (std::any_of(text.begin(), text.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }))
    return false;
  for (std::size_t p = 0; p < text.size();) {
    if (std::isalpha(static_cast<unsigned char>(text[p])) == 0) { ++p; continue; }
    const std::size_t begin = p;
    while (p < text.size() && std::isalpha(static_cast<unsigned char>(text[p])) != 0) ++p;
    const auto token = text.substr(begin, p - begin);
    if (month_from_word(token)) return false;
    const auto lowered = lower_ascii(token);
    for (const auto name : kWeekdayNames) if (lowered == name) return false;
  }
  return true;
}

// "YYYY-MM-DDTHH:MM:SS[.ffffff|.fffffffff]+00:00" for a UTC value, the
// isoformat of what the reference stores; native readers take it as is.
std::string utc_timestamp_text(std::int64_t seconds, std::uint32_t nanoseconds) {
  std::int64_t days = seconds >= 0 ? seconds / 86400 : -((-seconds + 86399) / 86400);
  const std::int64_t clock = seconds - days * 86400;
  // civil_from_days (Howard Hinnant).
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const std::int64_t day_of_era = days - era * 146097;
  const std::int64_t year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
  const std::int64_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  const std::int64_t mp = (5 * day_of_year + 2) / 153;
  const std::int64_t day = day_of_year - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t year = year_of_era + era * 400 + (month <= 2);
  char buffer[64];
  int length = std::snprintf(buffer, sizeof buffer, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld",
                             static_cast<long long>(year), static_cast<long long>(month),
                             static_cast<long long>(day), static_cast<long long>(clock / 3600),
                             static_cast<long long>(clock / 60 % 60), static_cast<long long>(clock % 60));
  std::string out(buffer, static_cast<std::size_t>(length));
  if (nanoseconds % 1000 != 0) {
    length = std::snprintf(buffer, sizeof buffer, ".%09u", nanoseconds);
    out.append(buffer, static_cast<std::size_t>(length));
  } else if (nanoseconds != 0) {
    length = std::snprintf(buffer, sizeof buffer, ".%06u", nanoseconds / 1000);
    out.append(buffer, static_cast<std::size_t>(length));
  }
  return out + "+00:00";
}

std::string utc_timestamp_text(std::int64_t microseconds) {
  const std::int64_t seconds =
      microseconds >= 0 ? microseconds / 1000000 : -((-microseconds + 999999) / 1000000);
  return utc_timestamp_text(seconds, static_cast<std::uint32_t>((microseconds - seconds * 1000000) * 1000));
}

// Whether the native readers take `text` as the UTC value `microseconds`
// with no non-zero offset of its own; otherwise the stored text is replaced
// by utc_timestamp_text so filters, features, statistics and exporters see
// the value the reference stores.
bool natively_utc(std::string_view text, std::int64_t microseconds) {
  const auto exact = parse_timestamp_exact(text);
  if (!exact || (exact->timezone_aware && exact->offset_minutes != 0)) return false;
  const auto native = parse_timestamp_microseconds(text);
  return native && *native == microseconds;
}

}  // namespace

void sort_ocel_events_by_timestamp(Ocel& log, bool nanoseconds) {
  sort_ocel_events_by_timestamp(log, nanoseconds ? OcelTimestampReader::kInstantNanoseconds
                                                 : OcelTimestampReader::kIsoFormat);
}

void sort_ocel_events_by_timestamp(Ocel& log, OcelTimestampReader reader) {
  // The key is the timestamp at the importer's precision: SQLite keeps
  // nanoseconds of the instant, the JSON and XML importers microseconds of
  // the ISO wall-clock fields (an OCEL 2.0 XML dateutil fallback with an
  // offset keys on its instant).
  std::vector<std::tuple<std::int64_t, std::int64_t, std::size_t>> keys;
  keys.reserve(log.events.size());
  bool aware = false, naive = false, unknown = false;  // kDateutil readings
  // Each stored text becomes the value the reference stores: the wall clock
  // stamped UTC for an ISO reading, the UTC instant for SQLite and for an
  // aware dateutil value, the wall clock for a naive one.
  std::vector<std::pair<std::size_t, std::string>> rewrites;
  const auto store = [&](std::size_t index, std::int64_t microseconds) {
    if (!natively_utc(log.events[index].timestamp, microseconds))
      rewrites.emplace_back(index, utc_timestamp_text(microseconds));
  };
  for (std::size_t index = 0; index < log.events.size(); ++index) {
    const auto& text = log.events[index].timestamp;
    if (reader == OcelTimestampReader::kInstantNanoseconds) {
      const auto instant = parse_timestamp_exact(text);
      if (!instant) { unknown = true; continue; }
      if (instant->timezone_aware && instant->offset_minutes != 0)
        rewrites.emplace_back(index, utc_timestamp_text(instant->seconds, instant->nanoseconds));
      keys.emplace_back(instant->seconds, instant->nanoseconds, index);
    } else if (reader == OcelTimestampReader::kIsoFormatOrFileOrder) {
      try {
        const auto wall = iso_wall_microseconds(text);
        store(index, wall);
        keys.emplace_back(wall, 0, index);
      } catch (const std::invalid_argument&) {
        unknown = true;
      }
    } else if (reader == OcelTimestampReader::kDateutil) {
      std::optional<std::int64_t> wall;
      try {
        wall = iso_wall_microseconds(text);
      } catch (const std::invalid_argument&) {
      }
      if (wall) {
        store(index, *wall);
        keys.emplace_back(*wall, 0, index);
        aware = true;
      } else if (const auto lenient = dateutil_subset(text)) {
        (lenient->aware ? aware : naive) = true;
        store(index, lenient->microseconds);
        keys.emplace_back(lenient->microseconds, 0, index);
      } else if (certainly_not_a_date(text)) {
        throw std::invalid_argument("String does not contain a date: " + text);
      } else {
        unknown = true;  // a shape native does not read: keep file order
      }
    } else {
      const auto wall = iso_wall_microseconds(text);
      store(index, wall);
      keys.emplace_back(wall, 0, index);
    }
  }
  for (auto& [index, text] : rewrites) log.events[index].timestamp = std::move(text);
  // Object changes take the same reading as events, so a change keeps its
  // value through every export format. A text the reader cannot take, and a
  // 1970-01-01T00:00:00 base entry (kept raw by the reference), stays as is.
  for (auto& change : log.object_changes) {
    const auto& text = change.timestamp;
    if (text.starts_with("1970-01-01T00:00:00")) continue;
    if (reader == OcelTimestampReader::kInstantNanoseconds) {
      const auto instant = parse_timestamp_exact(text);
      if (instant && instant->timezone_aware && instant->offset_minutes != 0)
        change.timestamp = utc_timestamp_text(instant->seconds, instant->nanoseconds);
      continue;
    }
    std::optional<std::int64_t> microseconds;
    try {
      microseconds = iso_wall_microseconds(text);
    } catch (const std::invalid_argument&) {
      if (reader == OcelTimestampReader::kDateutil)
        if (const auto lenient = dateutil_subset(text)) microseconds = lenient->microseconds;
    }
    if (microseconds && !natively_utc(text, *microseconds))
      change.timestamp = utc_timestamp_text(*microseconds);
  }
  if (unknown) return;
  // pandas cannot order tz-naive values next to tz-aware ones (the message
  // the reference raises from sort_values).
  if (aware && naive)
    throw std::invalid_argument("'values' is not ordered, please explicitly specify the categories "
                                "order by passing in a categories argument.");
  // Relations follow their events' order even when the events already are
  // in order.
  std::stable_sort(keys.begin(), keys.end());
  std::vector<OcelEvent> events;
  events.reserve(log.events.size());
  std::unordered_map<std::string, std::size_t> position;
  for (const auto& key : keys) {
    const auto index = std::get<2>(key);
    position.emplace(log.events[index].id, events.size());
    events.push_back(std::move(log.events[index]));
  }
  log.events = std::move(events);
  const auto rank = [&](const OcelRelation& relation) {
    const auto found = position.find(relation.event_id);
    return found == position.end() ? position.size() : found->second;
  };
  std::stable_sort(log.relations.begin(), log.relations.end(),
                   [&](const auto& left, const auto& right) {
                     return rank(left) < rank(right);
                   });
}

Ocel parse_ocel_json(std::string_view json, const OcelJsonLimits& limits) {
  return parse_ocel_json_names(json,limits,false);
}
Ocel parse_ocel_json_literal_names(std::string_view json, const OcelJsonLimits& limits) {
  return parse_ocel_json_names(json,limits,true);
}

std::string serialize_ocel_json(const Ocel& log, OcelJsonFormat format) {
  std::ostringstream out;
  const auto write_table_columns = [&] {
    const auto& columns = log.table_columns;
    if (!columns.events && !columns.objects && !columns.relations) return;
    out << ",\"minera:tableColumns\":{";
    bool first = true;
    const auto write = [&](const char* name,
                            const std::optional<std::vector<std::string>>& values) {
      if (!values) return;
      if (!first) out << ',';
      first = false;
      write_string(out, name);
      out << ":[";
      for (std::size_t i = 0; i < values->size(); ++i) {
        if (i) out << ',';
        write_string(out, (*values)[i]);
      }
      out << ']';
    };
    write("events", columns.events);
    write("objects", columns.objects);
    write("relations", columns.relations);
    out << '}';
  };
  std::vector<const OcelEvent*> events;
  for (const auto& event : log.events) events.push_back(&event);
  std::sort(events.begin(), events.end(), [](const auto* left, const auto* right) {
    return left->id < right->id;
  });
  std::vector<const OcelObject*> objects;
  for (const auto& object : log.objects) objects.push_back(&object);
  std::sort(objects.begin(), objects.end(), [](const auto* left, const auto* right) {
    return left->id < right->id;
  });

  // Relations and O2O links indexed once by their event / source id, in log
  // order, so serialising stays linear in the log size.
  std::unordered_map<std::string_view, std::vector<const OcelRelation*>> relations_by_event;
  for (const auto& relation : log.relations)
    relations_by_event[relation.event_id].push_back(&relation);
  std::unordered_map<std::string_view, std::vector<const OcelO2oRelation*>> o2o_by_source;
  for (const auto& relation : log.o2o) o2o_by_source[relation.source_id].push_back(&relation);
  auto event_relations = [&](const OcelEvent& event) {
    std::vector<const OcelRelation*> relations;
    if (const auto found = relations_by_event.find(event.id); found != relations_by_event.end())
      relations = found->second;
    std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                     const auto* right) {
      return std::tie(left->object_id, left->qualifier) <
             std::tie(right->object_id, right->qualifier);
    });
    return relations;
  };
  auto write_standard_attributes = [&](const std::vector<OcelAttribute>& attrs) {
    std::vector<const OcelAttribute*> sorted;
    for (const auto& attribute : attrs) sorted.push_back(&attribute);
    std::sort(sorted.begin(), sorted.end(), [](const auto* left, const auto* right) {
      return left->key < right->key;
    });
    out << '[';
    bool first = true;
    for (const auto* attribute : sorted) {
      if (!first) out << ',';
      first = false;
      out << "{\"name\":";
      write_string(out, attribute->key);
      out << ",\"value\":";
      write_value(out, attribute->value);
      out << '}';
    }
    out << ']';
  };
  auto write_standard_o2o = [&](std::string_view source) {
    std::vector<const OcelO2oRelation*> relations;
    if (const auto found = o2o_by_source.find(source); found != o2o_by_source.end())
      relations = found->second;
    std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                     const auto* right) {
      return std::tie(left->target_id, left->qualifier) <
             std::tie(right->target_id, right->qualifier);
    });
    out << '[';
    bool first = true;
    for (const auto* relation : relations) {
      if (!first) out << ',';
      first = false;
      out << "{\"objectId\":";
      write_string(out, relation->target_id);
      out << ",\"qualifier\":";
      write_string(out, relation->qualifier);
      out << '}';
    }
    out << ']';
  };

  if (format == OcelJsonFormat::kStandard) {
    StandardTypeTable object_types;
    StandardTypeTable event_types;
    collect_standard_types(log, object_types, event_types);
    out << "{\"ocelVersion\":\"2.0\",\"objectTypes\":";
    write_standard_type_table(out, object_types);
    out << ",\"eventTypes\":";
    write_standard_type_table(out, event_types);
    out << ",\"events\":[";
    bool first = true;
    first = true;
    for (const auto* event : events) {
      if (!first) out << ',';
      first = false;
      out << "{\"id\":";
      write_string(out, event->id);
      out << ",\"type\":";
      write_string(out, event->activity);
      out << ",\"time\":";
      write_string(out, event->timestamp);
      out << ",\"attributes\":";
      write_standard_attributes(event->attributes);
      out << ",\"relationships\":[";
      bool relation_first = true;
      for (const auto* relation : event_relations(*event)) {
        if (!relation_first) out << ',';
        relation_first = false;
        out << "{\"objectId\":";
        write_string(out, relation->object_id);
        out << ",\"qualifier\":";
        write_string(out, relation->qualifier);
        out << '}';
      }
      out << "]}";
    }
    out << "],\"objects\":[";
    first = true;
    for (const auto* object : objects) {
      if (!first) out << ',';
      first = false;
      out << "{\"id\":";
      write_string(out, object->id);
      out << ",\"type\":";
      write_string(out, object->type);
      out << ",\"attributes\":[";
      bool attribute_first = true;
      std::vector<const OcelAttribute*> static_attributes;
      for (const auto& attribute : object->attributes)
        static_attributes.push_back(&attribute);
      std::sort(static_attributes.begin(), static_attributes.end(),
                [](const auto* left, const auto* right) {
                  return left->key < right->key;
                });
      std::set<std::string> static_fields;
      for (const auto* attribute : static_attributes) {
        if (!attribute_first) out << ',';
        attribute_first = false;
        static_fields.insert(attribute->key);
        out << "{\"name\":";
        write_string(out, attribute->key);
        out << ",\"time\":\"1970-01-01T00:00:00Z\"";
        out << ",\"value\":";
        write_value(out, attribute->value);
        out << '}';
      }
      std::vector<const OcelObjectChange*> changes;
      for (const auto& change : log.object_changes) {
        if (change.object_id == object->id) changes.push_back(&change);
      }
      std::sort(changes.begin(), changes.end(), [](const auto* left, const auto* right) {
        return std::tie(left->timestamp, left->field, left->object_type) <
               std::tie(right->timestamp, right->field, right->object_type);
      });
      std::set<std::string> seeded_fields;
      for (const auto* change : changes) {
        if (!static_fields.count(change->field) &&
            seeded_fields.insert(change->field).second) {
          if (!attribute_first) out << ',';
          attribute_first = false;
          out << "{\"name\":";
          write_string(out, change->field);
          out << ",\"time\":\"1970-01-01T00:00:00Z\",\"value\":null}";
        }
        if (!attribute_first) out << ',';
        attribute_first = false;
        out << "{\"name\":";
        write_string(out, change->field);
        out << ",\"value\":";
        write_value(out, change->value);
        out << ",\"time\":";
        write_string(out, change->timestamp);
        out << '}';
      }
      out << "],\"relationships\":";
      write_standard_o2o(object->id);
      out << '}';
    }
    out << ']';
    if (!log.e2e.empty()) {
      out << ",\"eventRelationships\":";
      write_e2e(out, log, true);
    }
    if (!log.globals.empty()) {
      out << ",\"globals\":{";
      first = true;
      for (const auto& [key, value] : log.globals) {
        if (!first) out << ',';
        first = false;
        write_string(out, key);
        out << ':';
        write_value(out, value);
      }
      out << '}';
    }
    write_table_columns();
    out << '}';
    return out.str();
  }

  OcelTypeCatalog classic_object_types;
  OcelTypeCatalog classic_event_types;
  collect_classic_types(log, classic_object_types, classic_event_types);

  out << "{\"ocel:events\":{";
  bool first = true;
  for (const auto* event : events) {
    if (!first) out << ',';
    first = false;
    write_string(out, event->id);
    out << ":{\"ocel:activity\":";
    write_string(out, event->activity);
    out << ",\"ocel:timestamp\":";
    write_string(out, event->timestamp);
    const auto relations = event_relations(*event);
    out << ",\"ocel:omap\":[";
    bool relation_first = true;
    for (const auto* relation : relations) {
      if (!relation_first) out << ',';
      relation_first = false;
      write_string(out, relation->object_id);
    }
    out << "],\"ocel:typedOmap\":[";
    relation_first = true;
    for (const auto* relation : relations) {
      if (!relation_first) out << ',';
      relation_first = false;
      out << "{\"ocel:oid\":";
      write_string(out, relation->object_id);
      out << ",\"ocel:qualifier\":";
      write_string(out, relation->qualifier);
      out << '}';
    }
    out << "],\"ocel:vmap\":";
    write_attrs(out, event->attributes);
    out << '}';
  }
  out << "},\"ocel:eventTypes\":";
  write_classic_type_catalog(out, classic_event_types);
  out << ",\"ocel:objectTypes\":";
  write_classic_type_catalog(out, classic_object_types);
  if (!log.e2e.empty()) {
    out << ",\"ocel:e2e\":";
    write_e2e(out, log, false);
  }
  out << ",\"ocel:objects\":{";
  first = true;
  for (const auto* object : objects) {
    if (!first) out << ',';
    first = false;
    write_string(out, object->id);
    out << ":{\"ocel:type\":";
    write_string(out, object->type);
    out << ",\"ocel:ovmap\":";
    write_attrs(out, object->attributes);
    out << ",\"ocel:o2o\":";
    write_o2o(out, log, object->id);
    out << '}';
  }
  out << "},\"ocel:objectChanges\":";
  write_object_changes(out, log);
  out << ",\"ocel:global-log\":";
  write_global_category(out, log, "ocel:global-log");
  out << ",\"ocel:global-event\":";
  write_global_category(out, log, "ocel:global-event");
  out << ",\"ocel:global-object\":";
  write_global_category(out, log, "ocel:global-object");
  write_table_columns();
  out << '}';
  return out.str();
}

EtotGraph discover_etot_classic(const Ocel& log) {
  EtotGraph result; std::set<std::string> activities, types; std::map<std::pair<std::string, std::string>, std::size_t> counts; std::unordered_map<std::string, std::string> lookup; std::unordered_map<std::string, std::string> event_activity; for (const auto& o : log.objects) lookup[o.id] = o.type; for (const auto& e : log.events) event_activity[e.id] = e.activity; for (const auto& r : log.relations) { auto it = lookup.find(r.object_id); auto a = event_activity.find(r.event_id); if (it == lookup.end() || a == event_activity.end()) continue; activities.insert(a->second); types.insert(it->second); ++counts[{a->second, it->second}]; } result.activities.assign(activities.begin(), activities.end()); result.object_types.assign(types.begin(), types.end()); for (const auto& [key, frequency] : counts) result.edges.push_back({key.first, key.second, frequency}); return result;
}

OtgGraph discover_otg_classic(const Ocel& log) {
  OtgGraph result; std::set<std::string> types; std::unordered_map<std::string, std::string> type; for (const auto& o : log.objects) { type[o.id] = o.type; types.insert(o.type); } result.object_types.assign(types.begin(), types.end());
  std::unordered_map<std::string, std::vector<std::string>> by_event;
  for (const auto& r : log.relations)
    by_event[r.event_id].push_back(r.object_id);
  std::vector<std::string> order;
  for (const auto& e : log.events) {
    const auto it = by_event.find(e.id);
    if (it == by_event.end()) throw std::out_of_range("'" + e.id + "'");
    order.push_back(e.id);
    auto& objects = it->second;
    std::sort(objects.begin(), objects.end());
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
  }
  std::map<std::tuple<std::string, std::string, std::string>, std::size_t> counts; auto add = [&](const std::string& a, const std::string& rel, const std::string& b) { if (type.count(a) && type.count(b)) ++counts[{type[a], rel, type[b]}]; };
  std::set<std::pair<std::string, std::string>> interactions;
  for (const auto& ev : order) {
    const auto& objects = by_event.at(ev);
    for (std::size_t i = 0; i < objects.size(); ++i)
      for (std::size_t j = i + 1; j < objects.size(); ++j)
        interactions.emplace(objects[i], objects[j]);
  }
  for (const auto& [source, target] : interactions)
    add(source, "object_interaction", target);
  std::unordered_set<std::string> seen; for (const auto& ev : order) { std::unordered_set<std::string> old(seen.begin(), seen.end()); auto& v = by_event[ev]; for (const auto& a : v) if (old.count(a)) for (const auto& b : v) if (!old.count(b)) add(a, "object_descendants", b); for (const auto& a : v) seen.insert(a); }
  seen.clear(); for (const auto& ev : order) { auto& v = by_event[ev]; std::vector<std::string> fresh; for (const auto& a : v) if (!seen.count(a)) fresh.push_back(a); for (std::size_t i = 0; i < fresh.size(); ++i) for (std::size_t j = i + 1; j < fresh.size(); ++j) add(fresh[i], "object_cobirth", fresh[j]); for (const auto& a : fresh) seen.insert(a); }
  seen.clear(); for (auto it = order.rbegin(); it != order.rend(); ++it) { auto& v = by_event[*it]; std::vector<std::string> fresh; for (const auto& a : v) if (!seen.count(a)) fresh.push_back(a); for (std::size_t i = 0; i < fresh.size(); ++i) for (std::size_t j = i + 1; j < fresh.size(); ++j) add(fresh[i], "object_codeath", fresh[j]); for (const auto& a : fresh) seen.insert(a); }
  std::unordered_map<std::string, std::size_t> first, last; for (std::size_t i = 0; i < order.size(); ++i) for (const auto& a : by_event[order[i]]) { if (!first.count(a)) first[a] = i; last[a] = i; } std::set<std::pair<std::string, std::string>> inheritance; for (std::size_t i = 0; i < order.size(); ++i) { for (const auto& a : by_event[order[i]]) if (last[a] == i) for (const auto& b : by_event[order[i]]) if (first[b] == i && a != b) inheritance.insert({a, b}); } for (const auto& [a, b] : inheritance) if (!inheritance.count({b, a})) add(a, "object_inheritance", b);
  for (const auto& [key, frequency] : counts) result.edges.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key), frequency}); return result;
}

std::string etot_json(const EtotGraph& graph) {
  std::ostringstream out;
  out << "{\"activities\":[";
  for (std::size_t i = 0; i < graph.activities.size(); ++i) {
    if (i != 0) out << ',';
    write_string(out, graph.activities[i]);
  }
  out << "],\"object_types\":[";
  for (std::size_t i = 0; i < graph.object_types.size(); ++i) {
    if (i != 0) out << ',';
    write_string(out, graph.object_types[i]);
  }
  out << "],\"edges\":[";
  for (std::size_t i = 0; i < graph.edges.size(); ++i) {
    if (i != 0) out << ',';
    const auto& edge = graph.edges[i];
    out << "{\"activity\":";
    write_string(out, edge.activity);
    out << ",\"object_type\":";
    write_string(out, edge.object_type);
    out << ",\"frequency\":" << edge.frequency << '}';
  }
  out << "]}";
  return out.str();
}

std::string otg_json(const OtgGraph& graph) {
  std::ostringstream out;
  out << "{\"object_types\":[";
  for (std::size_t i = 0; i < graph.object_types.size(); ++i) {
    if (i != 0) out << ',';
    write_string(out, graph.object_types[i]);
  }
  out << "],\"edges\":[";
  for (std::size_t i = 0; i < graph.edges.size(); ++i) {
    if (i != 0) out << ',';
    const auto& edge = graph.edges[i];
    out << "{\"source_type\":";
    write_string(out, edge.source_type);
    out << ",\"relation\":";
    write_string(out, edge.relation);
    out << ",\"target_type\":";
    write_string(out, edge.target_type);
    out << ",\"frequency\":" << edge.frequency << '}';
  }
  out << "]}";
  return out.str();
}

EtotGraph parse_etot_json(std::string_view json, const OcelJsonLimits& limits) {
  const auto parsed = Parser(json, limits, true).parse();
  const auto& root = obj(parsed, "ETOT graph");
  if (root.find("activities_indep") != root.end() ||
      root.find("activities_ot") != root.end() ||
      root.find("start_activities") != root.end() ||
      root.find("end_activities") != root.end() ||
      root.find("performance_computed") != root.end()) {
    throw std::invalid_argument("ETOT graph envelope does not match the selected kind");
  }
  EtotGraph result;
  result.activities = string_array(&required(root, "activities"),
                                   "activities");
  result.object_types = string_array(&required(root, "object_types"),
                                     "object_types");
  const auto& raw_edges = required(root, "edges");
  if (!raw_edges.is_array())
    throw std::invalid_argument("ETOT edges must be an array");
  result.edges.reserve(raw_edges.array().size());
  for (const auto& raw : raw_edges.array()) {
    const auto& edge = obj(raw, "ETOT edge");
    result.edges.push_back(
        {str(required(edge, "activity"), "activity"),
         str(required(edge, "object_type"), "object_type"),
         size_value(required(edge, "frequency"), "frequency")});
  }
  return result;
}

OtgGraph parse_otg_json(std::string_view json, const OcelJsonLimits& limits) {
  const auto parsed = Parser(json, limits, true).parse();
  const auto& root = obj(parsed, "OTG graph");
  if (root.find("activities") != root.end() ||
      root.find("activities_indep") != root.end() ||
      root.find("activities_ot") != root.end() ||
      root.find("start_activities") != root.end() ||
      root.find("end_activities") != root.end() ||
      root.find("performance_computed") != root.end()) {
    throw std::invalid_argument("OTG graph envelope does not match the selected kind");
  }
  OtgGraph result;
  result.object_types = string_array(&required(root, "object_types"),
                                     "object_types");
  const auto& raw_edges = required(root, "edges");
  if (!raw_edges.is_array())
    throw std::invalid_argument("OTG edges must be an array");
  result.edges.reserve(raw_edges.array().size());
  for (const auto& raw : raw_edges.array()) {
    const auto& edge = obj(raw, "OTG edge");
    result.edges.push_back(
        {str(required(edge, "source_type"), "source_type"),
         str(required(edge, "relation"), "relation"),
         str(required(edge, "target_type"), "target_type"),
         size_value(required(edge, "frequency"), "frequency")});
  }
  return result;
}

OcdfgGraph discover_ocdfg_classic(const Ocel& log, bool compute_edges_performance) {
  OcdfgDiscoveryOptions options;
  options.compute_edges_performance = compute_edges_performance;
  return discover_ocdfg_classic_with_options(log, options);
}

OcdfgGraph discover_ocdfg_classic_with_options(
    const Ocel& log, const OcdfgDiscoveryOptions& options) {
  const auto column = [](const std::string& selected, bool event) {
    if (selected.empty() || (event && (selected == "activity" ||
                                      selected == "concept:name")) ||
        (!event && (selected == "type" || selected == "object_type")))
      return std::string(event ? "ocel:activity" : "ocel:type");
    return selected;
  };
  std::set<std::string> event_columns{"ocel:eid", "ocel:activity", "ocel:timestamp"};
  std::set<std::string> object_columns{"ocel:oid", "ocel:type"};
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes) event_columns.insert(attribute.key);
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes) object_columns.insert(attribute.key);
  for (const auto& [type, schema] : log.event_type_attributes)
    for (const auto& [name, value_type] : schema) event_columns.insert(name);
  for (const auto& [type, schema] : log.object_type_attributes)
    for (const auto& [name, value_type] : schema) object_columns.insert(name);
  std::set<std::string> relation_columns = event_columns;
  relation_columns.insert(object_columns.begin(), object_columns.end());
  relation_columns.insert("ocel:qualifier");
  const auto require_column = [](const auto& declared, const auto& inferred,
                                  const std::string& name) {
    const bool found = declared
        ? std::find(declared->begin(), declared->end(), name) != declared->end()
        : inferred.count(name) != 0;
    if (!found) throw std::out_of_range("'" + name + "'");
  };
  const auto activity_column = column(options.activity_attribute, true);
  const auto type_column = column(options.object_type_attribute, false);
  require_column(log.table_columns.events, event_columns, activity_column);
  require_column(log.table_columns.objects, object_columns, type_column);
  require_column(log.table_columns.relations, relation_columns, activity_column);
  require_column(log.table_columns.relations, relation_columns, type_column);
  const Ocel projected = project_ocel_labels(
      log, options.activity_attribute, options.object_type_attribute, "OCDFG");
  OcdfgGraph result;
  result.performance_computed = options.compute_edges_performance;
  std::map<std::string, std::string> activity;
  std::map<std::string, std::string> timestamp;
  std::map<std::string, std::size_t> event_order;
  for (std::size_t i = 0; i < projected.events.size(); ++i) {
    activity[projected.events[i].id] = projected.events[i].activity;
    timestamp[projected.events[i].id] = projected.events[i].timestamp;
    event_order[projected.events[i].id] = i;
    result.activities.push_back(projected.events[i].activity);
  }
  std::sort(result.activities.begin(), result.activities.end());
  result.activities.erase(std::unique(result.activities.begin(), result.activities.end()), result.activities.end());
  std::map<std::string, std::string> object_type;
  for (const auto& object : projected.objects) object_type[object.id] = object.type;
  for (const auto& object : projected.objects) result.object_types.push_back(object.type);
  std::sort(result.object_types.begin(), result.object_types.end());
  result.object_types.erase(std::unique(result.object_types.begin(), result.object_types.end()), result.object_types.end());

  struct Rel { std::string event; std::string object; std::size_t order; };
  std::vector<Rel> rels;
  for (std::size_t i = 0; i < projected.relations.size(); ++i) {
    const auto& relation = projected.relations[i];
    const auto event_it = event_order.find(relation.event_id);
    const auto type_it = object_type.find(relation.object_id);
    if (event_it == event_order.end() || type_it == object_type.end()) continue;
    rels.push_back({relation.event_id, relation.object_id, event_it->second * (projected.relations.size() + 1) + i});
  }
  auto add_association = [&](std::map<std::string, OcdfgActivityMetric>& target,
                             const std::string& act, const std::string& event,
                             const std::string& object) {
    auto& metric = target[act];
    metric.events.push_back(event);
    metric.unique_objects.push_back(object);
    metric.event_objects.emplace_back(event, object);
    metric.total_objects.emplace_back(event, object);
  };
  for (const auto& relation : rels) {
    const auto act = activity[relation.event];
    const auto type = object_type[relation.object];
    add_association(result.activities_indep, act, relation.event, relation.object);
    add_association(result.activities_ot[type], act, relation.event, relation.object);
  }
  auto normalize_activity = [](OcdfgActivityMetric& metric) {
    std::sort(metric.events.begin(), metric.events.end());
    metric.events.erase(std::unique(metric.events.begin(), metric.events.end()), metric.events.end());
    std::sort(metric.unique_objects.begin(), metric.unique_objects.end());
    metric.unique_objects.erase(std::unique(metric.unique_objects.begin(), metric.unique_objects.end()), metric.unique_objects.end());
    // Association lists retain the relation-row multiplicity and order.
  };
  for (auto& [act, metric] : result.activities_indep) normalize_activity(metric);
  for (auto& [type, metrics] : result.activities_ot) for (auto& [act, metric] : metrics) normalize_activity(metric);

  std::map<std::string, std::vector<Rel>> by_object;
  for (const auto& relation : rels) by_object[relation.object].push_back(relation);
  for (auto& [object, sequence] : by_object) {
    std::sort(sequence.begin(), sequence.end(), [](const Rel& a, const Rel& b) { return a.order < b.order; });
    if (sequence.empty()) continue;
    const auto type = object_type[object];
    add_association(result.start_activities[type], activity[sequence.front().event], sequence.front().event, object);
    add_association(result.end_activities[type], activity[sequence.back().event], sequence.back().event, object);
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      const auto source = activity[sequence[i - 1].event];
      const auto target = activity[sequence[i].event];
      auto& edge = result.edges[type][{source, target}];
      edge.event_couples.emplace_back(sequence[i - 1].event, sequence[i].event);
      edge.unique_objects.push_back(object);
      edge.event_objects.emplace_back(sequence[i - 1].event, sequence[i].event, object);
      edge.total_objects.emplace_back(sequence[i - 1].event, sequence[i].event,
                                      object);
    }
  }
  for (auto& [type, metrics] : result.start_activities) for (auto& [act, metric] : metrics) normalize_activity(metric);
  for (auto& [type, metrics] : result.end_activities) for (auto& [act, metric] : metrics) normalize_activity(metric);
  for (auto& [type, metrics] : result.edges) for (auto& [pair, edge] : metrics) {
    std::sort(edge.event_couples.begin(), edge.event_couples.end()); edge.event_couples.erase(std::unique(edge.event_couples.begin(), edge.event_couples.end()), edge.event_couples.end());
    std::sort(edge.unique_objects.begin(), edge.unique_objects.end()); edge.unique_objects.erase(std::unique(edge.unique_objects.begin(), edge.unique_objects.end()), edge.unique_objects.end());
    std::sort(edge.event_objects.begin(), edge.event_objects.end()); edge.event_objects.erase(std::unique(edge.event_objects.begin(), edge.event_objects.end()), edge.event_objects.end());
    std::sort(edge.total_objects.begin(), edge.total_objects.end()); edge.total_objects.erase(std::unique(edge.total_objects.begin(), edge.total_objects.end()), edge.total_objects.end());
    edge.performance.clear();
    edge.performance_event_couples.clear();
    edge.performance_total_objects.clear();
    if (options.compute_edges_performance && options.edge_performance_options) {
      const auto& timing_options = *options.edge_performance_options;
      edge.performance_event_couples = calculate_ocdfg_edge_performance(
          log, edge.event_couples, timing_options);
      std::vector<std::pair<std::string, std::string>> object_couples;
      object_couples.reserve(edge.total_objects.size());
      for (const auto& triple : edge.total_objects)
        object_couples.emplace_back(std::get<0>(triple), std::get<1>(triple));
      edge.performance_total_objects = calculate_ocdfg_edge_performance(
          log, object_couples, timing_options);
      edge.performance = edge.performance_total_objects;
    } else if (options.compute_edges_performance) {
      for (const auto& couple : edge.event_couples) {
        if (const auto seconds = legacy_edge_elapsed(timestamp[couple.first],
                                                     timestamp[couple.second]))
          edge.performance_event_couples.push_back(*seconds);
      }
      for (const auto& triple : edge.total_objects) {
        if (const auto seconds = legacy_edge_elapsed(timestamp[std::get<0>(triple)],
                                                     timestamp[std::get<1>(triple)]))
          edge.performance_total_objects.push_back(*seconds);
      }
      // Keep the historic field as the total-object projection.
      edge.performance = edge.performance_total_objects;
    }
    std::sort(edge.performance_event_couples.begin(),
              edge.performance_event_couples.end());
    std::sort(edge.performance_total_objects.begin(),
              edge.performance_total_objects.end());
    std::sort(edge.performance.begin(), edge.performance.end());
  }
  return result;
}

std::string serialize_ocdfg_json(const OcdfgGraph& graph) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  const auto performance_number = [&](double value) {
    // A bare -0 is an integer token and loses its sign when read as JSON.
    if (value == 0.0 && std::signbit(value)) out << "-0.0";
    else out << value;
  };
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto activity_metric = [&](const OcdfgActivityMetric& metric) {
    out << "{\"events\":";
    strings(metric.events);
    out << ",\"unique_objects\":";
    strings(metric.unique_objects);
    out << ",\"event_objects\":[";
    for (std::size_t i = 0; i < metric.event_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, metric.event_objects[i].first);
      out << ',';
      write_string(out, metric.event_objects[i].second);
      out << ']';
    }
    out << "],\"total_objects\":[";
    const auto& total_objects = metric.total_objects.empty()
                                    ? metric.event_objects
                                    : metric.total_objects;
    for (std::size_t i = 0; i < total_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, total_objects[i].first);
      out << ',';
      write_string(out, total_objects[i].second);
      out << ']';
    }
    out << "]}";
  };
  const auto activity_map = [&](const auto& metrics) {
    out << '{';
    bool first = true;
    for (const auto& [activity_name, metric] : metrics) {
      if (!first) out << ',';
      first = false;
      write_string(out, activity_name);
      out << ':';
      activity_metric(metric);
    }
    out << '}';
  };
  const auto nested_activity_map = [&](const auto& metrics) {
    out << '{';
    bool first_type = true;
    for (const auto& [object_type_name, activities_for_type] : metrics) {
      if (!first_type) out << ',';
      first_type = false;
      write_string(out, object_type_name);
      out << ':';
      activity_map(activities_for_type);
    }
    out << '}';
  };

  out << "{\"activities\":";
  strings(graph.activities);
  out << ",\"object_types\":";
  strings(graph.object_types);
  out << ",\"activities_indep\":";
  activity_map(graph.activities_indep);
  out << ",\"activities_ot\":";
  nested_activity_map(graph.activities_ot);
  out << ",\"start_activities\":";
  nested_activity_map(graph.start_activities);
  out << ",\"end_activities\":";
  nested_activity_map(graph.end_activities);
  out << ",\"edges\":{";
  bool first_type = true;
  for (const auto& [object_type_name, edges_for_type] : graph.edges) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, object_type_name);
    out << ":{";
    bool first_edge = true;
    for (const auto& [activity_pair, edge] : edges_for_type) {
      if (!first_edge) out << ',';
      first_edge = false;
      write_string(out, activity_pair.first + "->" + activity_pair.second);
      out << ":{\"source_activity\":";
      write_string(out, activity_pair.first);
      out << ",\"target_activity\":";
      write_string(out, activity_pair.second);
      out << ",\"event_couples\":[";
      for (std::size_t i = 0; i < edge.event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, edge.event_couples[i].first);
        out << ',';
        write_string(out, edge.event_couples[i].second);
        out << ']';
      }
      out << "],\"unique_objects\":";
      strings(edge.unique_objects);
      out << ",\"event_objects\":[";
      for (std::size_t i = 0; i < edge.event_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<1>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<2>(edge.event_objects[i]));
        out << ']';
      }
      out << "],\"total_objects\":[";
      const auto& total_objects = edge.total_objects.empty()
                                      ? edge.event_objects
                                      : edge.total_objects;
      for (std::size_t i = 0; i < total_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(total_objects[i]));
        out << ',';
        write_string(out, std::get<1>(total_objects[i]));
        out << ',';
        write_string(out, std::get<2>(total_objects[i]));
        out << ']';
      }
      out << "],\"performance\":[";
      for (std::size_t i = 0; i < edge.performance.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        performance_number(edge.performance[i]);
      }
      out << "],\"performance_event_couples\":[";
      for (std::size_t i = 0; i < edge.performance_event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        performance_number(edge.performance_event_couples[i]);
      }
      out << "],\"performance_total_objects\":[";
      for (std::size_t i = 0; i < edge.performance_total_objects.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        performance_number(edge.performance_total_objects[i]);
      }
      out << "]}";
    }
    out << '}';
  }
  out << "},\"performance_computed\":"
      << (graph.performance_computed ? "true" : "false") << '}';
  return out.str();
}

OcdfgGraph parse_ocdfg_json(std::string_view json,
                            const OcelJsonLimits& limits) {
  const auto parsed = Parser(json, limits).parse();
  const auto& root = obj(parsed, "OCDFG graph");
  OcdfgGraph result;
  result.activities =
      string_array(&required(root, "activities"), "activities");
  result.object_types =
      string_array(&required(root, "object_types"), "object_types");

  const auto parse_activity_metric = [&](const Json& raw,
                                          std::string_view context) {
    const auto& fields = obj(raw, context);
    OcdfgActivityMetric metric;
    metric.events = string_array(optional(fields, "events"), "events");
    metric.unique_objects =
        string_array(optional(fields, "unique_objects"), "unique_objects");
    metric.event_objects =
        pair_array(optional(fields, "event_objects"), "event_objects");
    metric.total_objects =
        pair_array(optional(fields, "total_objects"), "total_objects");
    return metric;
  };

  const auto parse_activity_map = [&](const Json* raw,
                                      std::string_view context,
                                      std::map<std::string, OcdfgActivityMetric>&
                                          target) {
    if (raw == nullptr) return;
    const auto& values = obj(*raw, context);
    for (const auto& [name, metric] : values) {
      target.emplace(name, parse_activity_metric(
                              metric, std::string(context) + ":" + name));
    }
  };
  const auto parse_nested_activity_map =
      [&](const Json* raw, std::string_view context,
          std::map<std::string,
                   std::map<std::string, OcdfgActivityMetric>>& target) {
        if (raw == nullptr) return;
        const auto& values = obj(*raw, context);
        for (const auto& [type, activities] : values) {
          const auto& activity_values = obj(
              activities, std::string(context) + ":" + type);
          auto& destination = target[type];
          for (const auto& [name, metric] : activity_values) {
            destination.emplace(
                name, parse_activity_metric(
                          metric, std::string(context) + ":" + type + ":" +
                                      name));
          }
        }
      };

  parse_activity_map(optional(root, "activities_indep"), "activities_indep",
                     result.activities_indep);
  parse_nested_activity_map(optional(root, "activities_ot"), "activities_ot",
                            result.activities_ot);
  parse_nested_activity_map(optional(root, "start_activities"),
                            "start_activities", result.start_activities);
  parse_nested_activity_map(optional(root, "end_activities"), "end_activities",
                            result.end_activities);

  const auto parse_edge_metric = [&](const Json& raw,
                                     std::string_view context) {
    const auto& fields = obj(raw, context);
    OcdfgEdgeMetric metric;
    metric.event_couples =
        pair_array(optional(fields, "event_couples"), "event_couples");
    metric.unique_objects =
        string_array(optional(fields, "unique_objects"), "unique_objects");
    metric.event_objects =
        triple_array(optional(fields, "event_objects"), "event_objects");
    metric.total_objects =
        triple_array(optional(fields, "total_objects"), "total_objects");
    metric.performance =
        number_array(optional(fields, "performance"), "performance");
    metric.performance_event_couples = number_array(
        optional(fields, "performance_event_couples"),
        "performance_event_couples");
    metric.performance_total_objects = number_array(
        optional(fields, "performance_total_objects"),
        "performance_total_objects");
    return metric;
  };

  const auto& edge_types = obj(required(root, "edges"), "edges");
  for (const auto& [type, raw_for_type] : edge_types) {
    const auto& raw_map = obj(raw_for_type, "edges type " + type);
    auto& destination = result.edges[type];
    for (const auto& [key, raw_metric] : raw_map) {
      const auto& fields = obj(raw_metric, "OCDFG edge " + key);
      std::string source;
      std::string target;
      if (const auto* value = optional(fields, "source_activity"))
        source = str(*value, "source_activity");
      if (const auto* value = optional(fields, "target_activity"))
        target = str(*value, "target_activity");
      if (source.empty() || target.empty()) {
        const auto separator = key.find("->");
        if (separator == std::string::npos || separator == 0 ||
            separator + 2 >= key.size())
          throw std::invalid_argument(
              "OCDFG edge requires source_activity and target_activity");
        source = key.substr(0, separator);
        target = key.substr(separator + 2);
      }
      auto metric = parse_edge_metric(
          raw_metric, "OCDFG edge " + type + ":" + key);
      if (!destination.emplace(std::make_pair(source, target),
                               std::move(metric))
               .second)
        throw std::invalid_argument("duplicate OCDFG edge");
    }
  }
  if (const auto* performance = optional(root, "performance_computed")) {
    if (!performance->is_bool())
      throw std::invalid_argument(
          "OCDFG performance_computed must be boolean");
    result.performance_computed = std::get<bool>(performance->value);
  }
  return result;
}

OcpnGraph discover_ocpn(const Ocel& log, const OcpnDiscoveryOptions& options) {
  if (!std::isfinite(options.double_arc_threshold) ||
      options.double_arc_threshold < 0.0 || options.double_arc_threshold > 1.0)
    throw std::invalid_argument("OCPN double_arc_threshold must be in [0,1]");
  if (!std::isfinite(options.noise_threshold) || options.noise_threshold < 0.0 ||
      options.noise_threshold > 1.0)
    throw std::invalid_argument("OCPN noise_threshold must be in [0,1]");

  const Ocel projected = project_ocpn_labels(log, options);
  OcpnGraph result;
  result.annotated = options.annotated;
  std::map<std::string, const OcelEvent*> event_by_id;
  std::map<std::string, std::size_t> event_order;
  for (std::size_t index = 0; index < projected.events.size(); ++index) {
    event_by_id[projected.events[index].id] = &projected.events[index];
    event_order[projected.events[index].id] = index;
    result.activities.push_back(projected.events[index].activity);
  }
  std::sort(result.activities.begin(), result.activities.end());
  result.activities.erase(
      std::unique(result.activities.begin(), result.activities.end()),
      result.activities.end());
  std::map<std::string, std::string> object_type;
  for (const auto& object : projected.objects) {
    object_type[object.id] = object.type;
    result.object_types.push_back(object.type);
  }
  std::sort(result.object_types.begin(), result.object_types.end());
  result.object_types.erase(
      std::unique(result.object_types.begin(), result.object_types.end()),
      result.object_types.end());

  // Preserve the object slots observed on each event.  The per-type nets
  // remain useful projections, while this relation map lets synchronized
  // playout bind one transition to the exact object types that co-occurred.
  std::map<std::string,
           std::map<std::string, std::map<std::string, std::size_t>>>
      relation_counts;
  std::map<std::string, std::map<std::string, std::set<std::string>>>
      object_ids_by_activity_event;
  for (const auto& relation : projected.relations) {
    const auto event_it = event_by_id.find(relation.event_id);
    const auto type_it = object_type.find(relation.object_id);
    if (event_it == event_by_id.end() || type_it == object_type.end()) continue;
    const auto& activity = event_it->second->activity;
    auto& object_ids = object_ids_by_activity_event[activity][relation.event_id];
    if (object_ids.insert(relation.object_id).second)
      ++relation_counts[activity][type_it->second][relation.event_id];
  }
  for (const auto& [activity, by_type] : relation_counts) {
    for (const auto& [type, by_event] : by_type) {
      const bool variable = std::any_of(
          by_event.begin(), by_event.end(),
          [](const auto& entry) { return entry.second > 1; });
      if (variable) {
        result.activity_variable_object_types[activity].push_back(type);
      } else {
        result.activity_object_types[activity].push_back(type);
      }
    }
  }

  if (options.annotated) {
    result.annotations = discover_ocdfg_classic(projected, false);
  }
  if (options.require_related_object_types) {
    std::set<std::string> related;
    for (const auto& relation : projected.relations) {
      const auto type_it = object_type.find(relation.object_id);
      if (type_it != object_type.end() && event_by_id.contains(relation.event_id))
        related.insert(type_it->second);
    }
    for (const auto& type : result.object_types)
      if (!related.contains(type))
        throw std::invalid_argument("OCPN object type '" + type + "' has no object related to an event");
  }
  // Each type's log is flattened from the objects whose original type equals
  // the type label: with an object-type selector the selected labels name
  // the types, but the flattening still matches the canonical types.
  std::map<std::string, std::string> flattening_type;
  for (const auto& object : log.objects) flattening_type[object.id] = object.type;
  for (const auto& type : result.object_types) {
    OcpnTokenReplayDiagnostics* token_replay_diagnostics = nullptr;
    if (options.diagnostics_with_token_based_replay) {
      auto& diagnostics = result.tbr_results[type];
      diagnostics.enabled = true;
      token_replay_diagnostics = &diagnostics;
    }
    EventLog flattened;
    // Each object's events by time, and in relation order (the first relation
    // of an event to the object places it) for the replay diagnostics.
    EventLog replay_flattened;
    std::map<std::string, std::set<std::string>> object_event_ids;
    std::map<std::string, std::vector<std::string>> replay_event_ids;
    for (const auto& relation : projected.relations) {
      const auto type_it = flattening_type.find(relation.object_id);
      if (type_it == flattening_type.end() || type_it->second != type ||
          !object_type.contains(relation.object_id))
        continue;
      if (event_by_id.count(relation.event_id)) {
        object_event_ids[relation.object_id].insert(relation.event_id);
        auto& ordered = replay_event_ids[relation.object_id];
        if (std::find(ordered.begin(), ordered.end(), relation.event_id) == ordered.end())
          ordered.push_back(relation.event_id);
      }
    }
    for (const auto& [object_id, event_ids] : object_event_ids) {
      Trace trace;
      trace.case_id = object_id;
      for (const auto& event_id : event_ids) {
        const auto event_it = event_by_id.find(event_id);
        if (event_it == event_by_id.end()) continue;
        const auto* event = event_it->second;
        trace.events.push_back(Event{object_id, event->activity, event->timestamp,
                                     {}, event_order[event_id], {}, {}});
      }
      order_events_by_time(trace.events);
      if (!trace.events.empty()) flattened.traces.push_back(std::move(trace));
    }

    for (const auto& [object_id, event_ids] : replay_event_ids) {
      Trace trace;
      trace.case_id = object_id;
      for (const auto& event_id : event_ids) {
        const auto* event = event_by_id.at(event_id);
        trace.events.push_back(Event{object_id, event->activity, event->timestamp, {},
                                     event_order[event_id], {}, {}});
      }
      replay_flattened.traces.push_back(std::move(trace));
    }

    InductiveMinerOptions miner_options;
    miner_options.noise_threshold = options.noise_threshold;
    miner_options.disable_fallthroughs = options.disable_fallthroughs;
    miner_options.disable_strict_sequence_cut =
        options.disable_strict_sequence_cut;
    miner_options.variant = options.variant == OcpnInductiveVariant::kDfg
                                ? InductiveMinerVariant::kDfg
                                : InductiveMinerVariant::kLog;
    // The classic miner reads each object's events by time; the DFG variant
    // (and SAW-net discovery) reads them in relation order.
    const bool relation_order =
        options.mine_in_relation_order || options.variant == OcpnInductiveVariant::kDfg;
    const auto tree = discover_process_tree(relation_order ? replay_flattened : flattened, miner_options);
    result.petri_nets[type] = process_tree_reference_net(tree);

    if (token_replay_diagnostics != nullptr) {
      const auto replay = replay_token_diagnostics(
          replay_flattened, result.petri_nets.at(type));
      token_replay_diagnostics->trace_count = replay.trace_count;
      token_replay_diagnostics->fitting_traces = replay.fitting_traces;
      token_replay_diagnostics->transition_activations =
          std::move(replay.transition_activations);
      token_replay_diagnostics->places.reserve(replay.places.size());
      for (const auto& place : replay.places) {
        token_replay_diagnostics->places.push_back(
            {place.missing, place.remaining, place.consumed, place.produced});
      }
    }

    if (options.annotated) {
      const auto type_it = result.annotations.activities_ot.find(type);
      if (type_it != result.annotations.activities_ot.end()) {
        for (const auto& [activity, metric] : type_it->second) {
          std::map<std::string, std::size_t> event_counts;
          for (const auto& event_object : metric.event_objects)
            ++event_counts[event_object.first];
          std::size_t single_events = 0;
          for (const auto& [event, count] : event_counts) {
            (void)event;
            if (count == 1) ++single_events;
          }
          const double ratio = event_counts.empty()
                                   ? 0.0
                                   : static_cast<double>(single_events) /
                                         static_cast<double>(event_counts.size());
          result.double_arcs_on_activity[type][activity] =
              ratio <= options.double_arc_threshold;
        }
      }
    }
  }
  return result;
}

namespace {

void write_ocpn_annotation_fields(std::ostringstream& out,
                                  const OcdfgGraph& graph) {
  const auto performance_number = [&](double value) {
    if (value == 0.0 && std::signbit(value)) out << "-0.0";
    else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  };
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto activity_metric = [&](const OcdfgActivityMetric& metric) {
    out << "{\"events\":";
    strings(metric.events);
    out << ",\"unique_objects\":";
    strings(metric.unique_objects);
    out << ",\"event_objects\":[";
    for (std::size_t i = 0; i < metric.event_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, metric.event_objects[i].first);
      out << ',';
      write_string(out, metric.event_objects[i].second);
      out << ']';
    }
    out << "],\"total_objects\":[";
    const auto& total_objects = metric.total_objects.empty()
                                    ? metric.event_objects
                                    : metric.total_objects;
    for (std::size_t i = 0; i < total_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, total_objects[i].first);
      out << ',';
      write_string(out, total_objects[i].second);
      out << ']';
    }
    out << "]}";
  };
  const auto activity_map = [&](const auto& metrics) {
    out << '{';
    bool first = true;
    for (const auto& [activity_name, metric] : metrics) {
      if (!first) out << ',';
      first = false;
      write_string(out, activity_name);
      out << ':';
      activity_metric(metric);
    }
    out << '}';
  };
  const auto nested_activity_map = [&](const auto& metrics) {
    out << '{';
    bool first_type = true;
    for (const auto& [object_type_name, activities_for_type] : metrics) {
      if (!first_type) out << ',';
      first_type = false;
      write_string(out, object_type_name);
      out << ':';
      activity_map(activities_for_type);
    }
    out << '}';
  };

  out << "\"activities_indep\":";
  activity_map(graph.activities_indep);
  out << ",\"activities_ot\":";
  nested_activity_map(graph.activities_ot);
  out << ",\"start_activities\":";
  nested_activity_map(graph.start_activities);
  out << ",\"end_activities\":";
  nested_activity_map(graph.end_activities);
  out << ",\"edges\":{";
  bool first_type = true;
  for (const auto& [object_type_name, edges_for_type] : graph.edges) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, object_type_name);
    out << ":{";
    bool first_edge = true;
    for (const auto& [activity_pair, edge] : edges_for_type) {
      if (!first_edge) out << ',';
      first_edge = false;
      write_string(out, activity_pair.first + "->" + activity_pair.second);
      out << ":{\"source_activity\":";
      write_string(out, activity_pair.first);
      out << ",\"target_activity\":";
      write_string(out, activity_pair.second);
      out << ",\"event_couples\":[";
      for (std::size_t i = 0; i < edge.event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, edge.event_couples[i].first);
        out << ',';
        write_string(out, edge.event_couples[i].second);
        out << ']';
      }
      out << "],\"event_pairs\":[";
      for (std::size_t i = 0; i < edge.event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, edge.event_couples[i].first);
        out << ',';
        write_string(out, edge.event_couples[i].second);
        out << ']';
      }
      out << "],\"unique_objects\":";
      strings(edge.unique_objects);
      out << ",\"event_objects\":[";
      for (std::size_t i = 0; i < edge.event_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<1>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<2>(edge.event_objects[i]));
        out << ']';
      }
      out << "],\"total_objects\":[";
      const auto& total_objects = edge.total_objects.empty()
                                      ? edge.event_objects
                                      : edge.total_objects;
      for (std::size_t i = 0; i < total_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(total_objects[i]));
        out << ',';
        write_string(out, std::get<1>(total_objects[i]));
        out << ',';
        write_string(out, std::get<2>(total_objects[i]));
        out << ']';
      }
      out << ']';
      out << ",\"performance\":[";
      for (std::size_t i = 0; i < edge.performance.size(); ++i) {
        if (i != 0) out << ',';
        performance_number(edge.performance[i]);
      }
      out << "],\"performance_event_couples\":[";
      for (std::size_t i = 0; i < edge.performance_event_couples.size(); ++i) {
        if (i != 0) out << ',';
        performance_number(edge.performance_event_couples[i]);
      }
      out << "],\"performance_total_objects\":[";
      for (std::size_t i = 0; i < edge.performance_total_objects.size(); ++i) {
        if (i != 0) out << ',';
        performance_number(edge.performance_total_objects[i]);
      }
      out << "]}";
    }
    out << '}';
  }
  out << "},\"performance_computed\":"
      << (graph.performance_computed ? "true" : "false");
}

}  // namespace

std::string serialize_ocpn_json(const OcpnGraph& graph) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto numbers = [&](const std::vector<std::size_t>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      out << values[i];
    }
    out << ']';
  };
  out << "{\"activities\":";
  strings(graph.activities);
  out << ",\"object_types\":";
  strings(graph.object_types);
  out << ",\"activity_object_types\":{";
  bool first_activity_binding = true;
  for (const auto& [activity, types] : graph.activity_object_types) {
    if (!first_activity_binding) out << ',';
    first_activity_binding = false;
    write_string(out, activity);
    out << ':';
    strings(types);
  }
  out << "},\"activity_variable_object_types\":{";
  bool first_variable_activity = true;
  for (const auto& [activity, types] : graph.activity_variable_object_types) {
    if (!first_variable_activity) out << ',';
    first_variable_activity = false;
    write_string(out, activity);
    out << ':';
    strings(types);
  }
  out << "},\"petri_nets\":{";
  bool first_type = true;
  for (const auto& [type, net] : graph.petri_nets) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, type);
    out << ":{\"place_count\":" << net.place_count()
        << ",\"source\":" << net.source() << ",\"sink\":"
        << net.sink() << ",\"activities\":";
    strings(net.activities());
    out << ",\"transitions\":[";
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      if (index != 0) out << ',';
      const auto& transition = net.transitions()[index];
      out << "{\"activity\":";
      if (transition.activity) out << *transition.activity;
      else out << "null";
      out << ",\"inputs\":";
      numbers(transition.inputs);
      out << ",\"outputs\":";
      numbers(transition.outputs);
      out << '}';
    }
    out << "]}";
  }
  out << "},\"double_arcs_on_activity\":{";
  first_type = true;
  for (const auto& [type, activities] : graph.double_arcs_on_activity) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, type);
    out << ":{";
    bool first_activity = true;
    for (const auto& [activity, is_double] : activities) {
      if (!first_activity) out << ',';
      first_activity = false;
      write_string(out, activity);
      out << ':' << (is_double ? "true" : "false");
    }
    out << '}';
  }
  out << "},\"tbr_results\":{";
  bool first_diagnostic_type = true;
  for (const auto& [type, diagnostic] : graph.tbr_results) {
    if (!diagnostic.enabled) continue;
    if (!first_diagnostic_type) out << ',';
    first_diagnostic_type = false;
    write_string(out, type);
    out << ":{";
    out << "\"enabled\":" << (diagnostic.enabled ? "true" : "false")
        << ",\"trace_count\":" << diagnostic.trace_count
        << ",\"fitting_traces\":" << diagnostic.fitting_traces
        << ",\"places\":[";
    for (std::size_t place = 0; place < diagnostic.places.size(); ++place) {
      if (place != 0) out << ',';
      const auto& counts = diagnostic.places[place];
      out << "{\"m\":" << counts.missing
          << ",\"r\":" << counts.remaining
          << ",\"c\":" << counts.consumed
          << ",\"p\":" << counts.produced << '}';
    }
    out << "],\"transition_activations\":[";
    for (std::size_t transition = 0;
         transition < diagnostic.transition_activations.size(); ++transition) {
      if (transition != 0) out << ',';
      out << diagnostic.transition_activations[transition];
    }
    out << "]}";
  }
  out << "},\"annotations\":";
  if (graph.annotated) {
    out << '{';
    write_ocpn_annotation_fields(out, graph.annotations);
    out << '}';
  } else {
    out << "null";
  }
  out << '}';
  return out.str();
}

OcelTupleDescription describe_ocel_tuples(const Ocel& log,
                                           const OcelDescriptionOptions& options) {
  OcelTupleDescription result;
  result.includes_timestamps = options.include_timestamps;
  std::map<std::string, const OcelEvent*> events;
  for (const auto& event : log.events) events[event.id] = &event;
  std::map<std::string, std::string> object_types;
  for (const auto& object : log.objects) object_types[object.id] = object.type;
  std::map<std::string, std::vector<std::string>> related_by_event;
  std::map<std::string, std::vector<std::string>> events_by_object;
  for (const auto& relation : log.relations) {
    if (!events.count(relation.event_id) || !object_types.count(relation.object_id))
      continue;
    related_by_event[relation.event_id].push_back(relation.object_id);
    events_by_object[relation.object_id].push_back(relation.event_id);
  }
  for (auto& [event_id, objects] : related_by_event) {
    std::sort(objects.begin(), objects.end());
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
  }
  const auto event_less = [&](const std::string& left_id,
                              const std::string& right_id) {
    const auto* left = events.at(left_id);
    const auto* right = events.at(right_id);
    const auto left_time = parse_timestamp(left->timestamp);
    const auto right_time = parse_timestamp(right->timestamp);
    if (left_time && right_time && *left_time != *right_time)
      return *left_time < *right_time;
    if (left->timestamp != right->timestamp)
      return left->timestamp < right->timestamp;
    if (left->activity != right->activity)
      return left->activity < right->activity;
    return left_id < right_id;
  };
  std::vector<std::string> event_ids;
  for (const auto& [event_id, event] : events) {
    (void)event;
    event_ids.push_back(event_id);
  }
  std::sort(event_ids.begin(), event_ids.end(), event_less);
  for (const auto& event_id : event_ids) {
    const auto* event = events.at(event_id);
    OcelTupleEventDescription description;
    description.activity = event->activity;
    description.related_objects = related_by_event[event_id];
    if (options.include_timestamps) description.timestamp = event->timestamp;
    result.events.push_back(std::move(description));
  }

  for (auto& [object_id, object_events] : events_by_object) {
    std::sort(object_events.begin(), object_events.end(), event_less);
    object_events.erase(std::unique(object_events.begin(), object_events.end()),
                        object_events.end());
  }
  for (const auto& object : log.objects) {
    OcelTupleObjectDescription description;
    description.id = object.id;
    description.type = object.type;
    if (options.include_timestamps) {
      const auto it = events_by_object.find(object.id);
      if (it != events_by_object.end() && !it->second.empty()) {
        const auto* first = events.at(it->second.front());
        const auto* last = events.at(it->second.back());
        description.lifecycle_start = first->timestamp;
        description.lifecycle_end = last->timestamp;
        const auto start = parse_timestamp(first->timestamp);
        const auto end = parse_timestamp(last->timestamp);
        if (!start || !end)
          throw std::invalid_argument("OCEL description requires valid timestamps");
        description.lifecycle_seconds = *end - *start;
        description.has_lifecycle = true;
      }
    }
    result.objects.push_back(std::move(description));
  }
  std::sort(result.objects.begin(), result.objects.end(),
            [](const auto& left, const auto& right) { return left.id < right.id; });
  return result;
}

std::string describe_ocel_text(const Ocel& log,
                               const OcelDescriptionOptions& options) {
  const auto description = describe_ocel_tuples(log, options);
  std::ostringstream out;
  out << "\n\nevents:\n";
  for (const auto& event : description.events) {
    out << '\n' << event.activity << " ( related objects: ";
    for (std::size_t index = 0; index < event.related_objects.size(); ++index) {
      if (index != 0) out << ", ";
      out << event.related_objects[index];
    }
    out << " ) ";
    if (description.includes_timestamps)
      out << " timestamp: " << event.timestamp;
  }
  out << "\n\nobjects:\n";
  for (const auto& object : description.objects) {
    out << '\n' << object.id << " object type: " << object.type;
    if (description.includes_timestamps && object.has_lifecycle) {
      out << " ( lifecycle start: " << object.lifecycle_start
          << " ; lifecycle end: " << object.lifecycle_end
          << " ; lifecycle duration: " << std::setprecision(17)
          << object.lifecycle_seconds << " )";
    }
  }
  return out.str();
}

std::string serialize_ocel_description_json(
    const OcelTupleDescription& description) {
  std::ostringstream out;
  out << "{\"includes_timestamps\":"
      << (description.includes_timestamps ? "true" : "false")
      << ",\"events\":[";
  for (std::size_t index = 0; index < description.events.size(); ++index) {
    if (index != 0) out << ',';
    const auto& event = description.events[index];
    out << "{\"activity\":";
    write_string(out, event.activity);
    out << ",\"related_objects\":[";
    for (std::size_t object_index = 0;
         object_index < event.related_objects.size(); ++object_index) {
      if (object_index != 0) out << ',';
      write_string(out, event.related_objects[object_index]);
    }
    out << "]";
    if (description.includes_timestamps) {
      out << ",\"timestamp\":";
      write_string(out, event.timestamp);
    }
    out << '}';
  }
  out << "],\"objects\":[";
  for (std::size_t index = 0; index < description.objects.size(); ++index) {
    if (index != 0) out << ',';
    const auto& object = description.objects[index];
    out << "{\"id\":";
    write_string(out, object.id);
    out << ",\"type\":";
    write_string(out, object.type);
    out << ",\"has_lifecycle\":"
        << (object.has_lifecycle ? "true" : "false");
    if (description.includes_timestamps) {
      out << ",\"lifecycle_start\":";
      write_string(out, object.lifecycle_start);
      out << ",\"lifecycle_end\":";
      write_string(out, object.lifecycle_end);
      out << ",\"lifecycle_seconds\":" << std::setprecision(17)
          << object.lifecycle_seconds;
    }
    out << '}';
  }
  out << "]}";
  return out.str();
}

const char* ocel_description_variant_name(OcelDescriptionVariant variant) {
  switch (variant) {
    case OcelDescriptionVariant::kVariant1: return "VARIANT1";
    case OcelDescriptionVariant::kVariant2: return "VARIANT2";
  }
  throw std::invalid_argument("OCEL description variant is invalid");
}

std::string serialize_ocel_description_json(
    const Ocel& log, OcelDescriptionVariant variant,
    const OcelDescriptionOptions& options) {
  if (variant != OcelDescriptionVariant::kVariant1 &&
      variant != OcelDescriptionVariant::kVariant2)
    throw std::invalid_argument("OCEL description variant is invalid");
  std::ostringstream out;
  out << "{\"family\":\"OCEL_DESCRIPTION\",\"variant\":\""
      << ocel_description_variant_name(variant)
      << "\",\"includes_timestamps\":"
      << (options.include_timestamps ? "true" : "false")
      << ",\"event_count\":" << log.events.size()
      << ",\"object_count\":" << log.objects.size();
  if (variant == OcelDescriptionVariant::kVariant1) {
    out << ",\"text\":";
    write_string(out, describe_ocel_text(log, options));
    out << '}';
    return out.str();
  }
  const auto description = describe_ocel_tuples(log, options);
  out << ",\"events\":[";
  for (std::size_t index = 0; index < description.events.size(); ++index) {
    if (index != 0) out << ',';
    const auto& event = description.events[index];
    out << '[';
    write_string(out, event.activity);
    for (const auto& object : event.related_objects) {
      out << ',';
      write_string(out, object);
    }
    if (description.includes_timestamps) {
      out << ',';
      write_string(out, event.timestamp);
    }
    out << ']';
  }
  out << "],\"event_object_counts\":[";
  for (std::size_t index = 0; index < description.events.size(); ++index) {
    if (index != 0) out << ',';
    out << description.events[index].related_objects.size();
  }
  out << "],\"objects\":[";
  for (std::size_t index = 0; index < description.objects.size(); ++index) {
    if (index != 0) out << ',';
    const auto& object = description.objects[index];
    out << '[';
    write_string(out, object.id);
    out << ',';
    write_string(out, object.type);
    if (description.includes_timestamps) {
      out << ',' << (object.has_lifecycle ? "true" : "false");
      out << ',';
      write_string(out, object.lifecycle_start);
      out << ',';
      write_string(out, object.lifecycle_end);
      out << ',' << std::setprecision(17) << object.lifecycle_seconds;
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

namespace {

double conformance_count_ratio(std::size_t numerator, std::size_t denominator) {
  if (denominator == 0) throw std::domain_error("division by zero");
  constexpr std::uint64_t exact_double_limit = std::uint64_t{1} << 53;
  if (numerator <= exact_double_limit && denominator <= exact_double_limit)
    return static_cast<double>(numerator) / static_cast<double>(denominator);

  // Round the integer quotient itself. Converting the operands separately
  // can lose a one-event difference or change the last bit of the ratio.
  const std::uint64_t divisor = denominator;
  const std::uint64_t quotient = numerator / divisor;
  std::uint64_t remainder = numerator % divisor;
  const auto next_bit = [&]() {
    // Double the remainder without overflowing a full-width counter.
    const bool bit = remainder >= divisor - remainder;
    remainder = bit ? remainder - (divisor - remainder) : remainder + remainder;
    return bit;
  };
  int exponent;
  std::uint64_t significand;
  if (quotient != 0) {
    exponent = static_cast<int>(std::bit_width(quotient)) - 1;
    if (exponent > 52) {
      const int shift = exponent - 52;
      significand = quotient >> shift;
      const auto discarded = quotient & ((std::uint64_t{1} << shift) - 1);
      const auto halfway = std::uint64_t{1} << (shift - 1);
      if (discarded > halfway ||
          (discarded == halfway && (remainder != 0 || (significand & 1))))
        ++significand;
      return std::ldexp(static_cast<double>(significand), exponent - 52);
    }
    significand = quotient;
    for (int i = exponent; i < 52; ++i)
      significand = (significand << 1) | next_bit();
  } else {
    if (numerator == 0) return 0.0;
    exponent = -1;
    while (!next_bit()) --exponent;
    significand = 1;
    for (int i = 0; i < 52; ++i)
      significand = (significand << 1) | next_bit();
  }
  if (remainder > divisor - remainder ||
      (remainder == divisor - remainder && (significand & 1)))
    ++significand;
  return std::ldexp(static_cast<double>(significand), exponent - 52);
}

double conformance_count_delta(std::size_t real, std::size_t normative) {
  return conformance_count_ratio(real >= normative ? real - normative
                                                   : normative - real,
                                 normative);
}

void write_conformance_number(std::ostringstream& out, double value) {
  if (!std::isfinite(value)) {
    out << "null";
    return;
  }
  // A bare -0 is an integer JSON token and discards the floating sign in
  // readers that distinguish integers from reals.
  if (value == 0.0 && std::signbit(value)) out << "-0.0";
  else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
}

template <typename T>
std::vector<T> sorted_difference(const std::set<T>& left,
                                 const std::set<T>& right) {
  std::vector<T> result;
  std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                      std::back_inserter(result));
  return result;
}

std::map<std::pair<std::string, std::string>, std::size_t> etot_frequencies(
    const EtotGraph& graph) {
  std::map<std::pair<std::string, std::string>, std::size_t> result;
  for (const auto& edge : graph.edges)
    result[{edge.activity, edge.object_type}] += edge.frequency;
  return result;
}

std::map<std::tuple<std::string, std::string, std::string>, std::size_t>
otg_frequencies(const OtgGraph& graph) {
  std::map<std::tuple<std::string, std::string, std::string>, std::size_t>
      result;
  for (const auto& edge : graph.edges)
    result[{edge.source_type, edge.relation, edge.target_type}] += edge.frequency;
  return result;
}

std::set<std::pair<std::string, std::string>> ocdfg_flows(
    const OcdfgGraph& graph) {
  std::set<std::pair<std::string, std::string>> result;
  for (const auto& [object_type, edges] : graph.edges)
    for (const auto& [flow, metric] : edges) {
      (void)object_type;
      (void)metric;
      result.insert(flow);
    }
  return result;
}

std::size_t ocdfg_activity_measure(const OcdfgGraph& graph,
                                   const std::string& activity) {
  const auto it = graph.activities_indep.find(activity);
  return it == graph.activities_indep.end() ? 0u : it->second.events.size();
}

std::size_t ocdfg_flow_measure(const OcdfgGraph& graph,
                               const std::pair<std::string, std::string>& flow) {
  std::size_t result = 0;
  for (const auto& [object_type, edges] : graph.edges) {
    (void)object_type;
    const auto it = edges.find(flow);
    if (it != edges.end()) result += it->second.event_couples.size();
  }
  return result;
}

}  // namespace

EtotConformanceResult compare_etot(const EtotGraph& real,
                                   const EtotGraph& normative,
                                   const EtotConformanceOptions& options) {
  const std::set<std::string> real_activities(real.activities.begin(),
                                               real.activities.end());
  const std::set<std::string> normative_activities(normative.activities.begin(),
                                                    normative.activities.end());
  const std::set<std::string> real_types(real.object_types.begin(),
                                         real.object_types.end());
  const std::set<std::string> normative_types(normative.object_types.begin(),
                                               normative.object_types.end());
  const auto real_frequency = etot_frequencies(real);
  const auto normative_frequency = etot_frequencies(normative);
  std::set<std::pair<std::string, std::string>> real_edges;
  std::set<std::pair<std::string, std::string>> normative_edges;
  for (const auto& [edge, frequency] : real_frequency) {
    (void)frequency;
    real_edges.insert(edge);
  }
  for (const auto& [edge, frequency] : normative_frequency) {
    (void)frequency;
    normative_edges.insert(edge);
  }

  EtotConformanceResult result;
  result.missing_activities = sorted_difference(normative_activities, real_activities);
  result.additional_activities = sorted_difference(real_activities, normative_activities);
  result.missing_object_types = sorted_difference(normative_types, real_types);
  result.additional_object_types = sorted_difference(real_types, normative_types);
  result.missing_edges = sorted_difference(normative_edges, real_edges);
  result.additional_edges = sorted_difference(real_edges, normative_edges);

  std::size_t relative_mismatches = 0;
  for (const auto& edge : normative_edges) {
    if (!real_edges.count(edge)) continue;
    const auto normative_count = normative_frequency.at(edge);
    const auto real_count = real_frequency.at(edge);
    const double delta = conformance_count_delta(real_count, normative_count);
    result.relative_frequency_delta[edge] = delta;
    if (delta > options.theta_relative) ++relative_mismatches;
  }

  const double normalizer =
      options.alpha * static_cast<double>(normative_activities.size() + normative_types.size()) +
      options.beta * static_cast<double>(normative_edges.size()) +
      options.gamma * static_cast<double>(normative_edges.size());
  if (normalizer == 0.0) {
    throw std::domain_error("division by zero");
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_activities.size() +
                                            result.missing_object_types.size()) +
        options.beta * static_cast<double>(result.missing_edges.size()) +
        options.gamma * static_cast<double>(relative_mismatches);
    result.fitness = 1.0 - numerator / normalizer;
  }
  return result;
}

EtotConformanceResult compare_etot(const Ocel& real,
                                   const EtotGraph& normative,
                                   const EtotConformanceOptions& options) {
  return compare_etot(discover_etot_classic(real), normative, options);
}

OtgConformanceResult compare_otg(const OtgGraph& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options) {
  const std::set<std::string> real_types(real.object_types.begin(),
                                         real.object_types.end());
  const std::set<std::string> normative_types(normative.object_types.begin(),
                                               normative.object_types.end());
  const auto real_frequency = otg_frequencies(real);
  const auto normative_frequency = otg_frequencies(normative);
  std::set<std::tuple<std::string, std::string, std::string>> real_edges;
  std::set<std::tuple<std::string, std::string, std::string>> normative_edges;
  for (const auto& [edge, frequency] : real_frequency) {
    (void)frequency;
    real_edges.insert(edge);
  }
  for (const auto& [edge, frequency] : normative_frequency) {
    (void)frequency;
    normative_edges.insert(edge);
  }

  OtgConformanceResult result;
  result.missing_object_types = sorted_difference(normative_types, real_types);
  result.additional_object_types = sorted_difference(real_types, normative_types);
  result.missing_edges = sorted_difference(normative_edges, real_edges);
  result.additional_edges = sorted_difference(real_edges, normative_edges);
  std::size_t non_conforming = 0;
  for (const auto& edge : normative_edges) {
    if (!real_edges.count(edge)) continue;
    const auto normative_count = normative_frequency.at(edge);
    const auto real_count = real_frequency.at(edge);
    const double delta = normative_count == 0
                             ? std::numeric_limits<double>::infinity()
                             : conformance_count_delta(real_count, normative_count);
    const auto relation = std::get<1>(edge);
    const auto theta_it = options.theta_by_relation.find(relation);
    const double theta = theta_it == options.theta_by_relation.end()
                             ? options.theta
                             : theta_it->second;
    if (delta > theta) {
      result.non_conforming_edges[edge] = delta;
      ++non_conforming;
    }
  }
  const double normalizer =
      options.alpha * static_cast<double>(normative_types.size()) +
      options.beta * static_cast<double>(normative_edges.size()) +
      options.gamma * static_cast<double>(normative_edges.size());
  if (normalizer == 0.0) {
    throw std::domain_error("division by zero");
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_object_types.size()) +
        options.beta * static_cast<double>(result.missing_edges.size()) +
        options.gamma * static_cast<double>(non_conforming);
    result.fitness = 1.0 - numerator / normalizer;
  }
  return result;
}

OtgConformanceResult compare_otg(const Ocel& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options) {
  return compare_otg(discover_otg_classic(real), normative, options);
}

OcdfgConformanceResult compare_ocdfg(const OcdfgGraph& real,
                                     const OcdfgGraph& normative,
                                     const OcdfgConformanceOptions& options) {
  const std::set<std::string> real_activities(real.activities.begin(),
                                               real.activities.end());
  const std::set<std::string> normative_activities(normative.activities.begin(),
                                                    normative.activities.end());
  const auto real_flows = ocdfg_flows(real);
  const auto normative_flows = ocdfg_flows(normative);
  OcdfgConformanceResult result;
  result.missing_activities = sorted_difference(normative_activities, real_activities);
  result.additional_activities = sorted_difference(real_activities, normative_activities);
  result.missing_flows = sorted_difference(normative_flows, real_flows);
  result.additional_flows = sorted_difference(real_flows, normative_flows);

  std::set<std::string> all_activities = real_activities;
  all_activities.insert(normative_activities.begin(), normative_activities.end());
  std::size_t non_conforming_activities = 0;
  for (const auto& activity : all_activities) {
    const auto real_measure = ocdfg_activity_measure(real, activity);
    const auto normative_measure = ocdfg_activity_measure(normative, activity);
    const auto difference = static_cast<std::int64_t>(
        normative_measure >= real_measure ? normative_measure - real_measure
                                           : real_measure - normative_measure);
    result.activity_measure_differences[activity] = difference;
    if (static_cast<double>(difference) > options.theta_activity) {
      result.non_conforming_activities_in_measure.push_back(activity);
      ++non_conforming_activities;
    }
  }

  std::set<std::pair<std::string, std::string>> all_flows = real_flows;
  all_flows.insert(normative_flows.begin(), normative_flows.end());
  std::size_t non_conforming_flows = 0;
  for (const auto& flow : all_flows) {
    const auto real_measure = ocdfg_flow_measure(real, flow);
    const auto normative_measure = ocdfg_flow_measure(normative, flow);
    const auto difference = static_cast<std::int64_t>(
        normative_measure >= real_measure ? normative_measure - real_measure
                                           : real_measure - normative_measure);
    result.flow_measure_differences[flow] = difference;
    if (static_cast<double>(difference) > options.theta_flow) {
      result.non_conforming_flows_in_measure.push_back(flow);
      ++non_conforming_flows;
    }
  }

  const double normalizer =
      options.alpha * static_cast<double>(all_activities.size()) +
      options.beta * static_cast<double>(all_flows.size()) +
      options.gamma * static_cast<double>(all_activities.size()) +
      options.delta * static_cast<double>(all_flows.size());
  if (normalizer == 0.0) {
    result.fitness = 1.0;
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_activities.size()) +
        options.beta * static_cast<double>(result.missing_flows.size()) +
        options.gamma * static_cast<double>(non_conforming_activities) +
        options.delta * static_cast<double>(non_conforming_flows);
    // Undefined weighted arithmetic clips to the lower bound as well.
    result.fitness = std::min(1.0, std::max(0.0, 1.0 - numerator / normalizer));
  }
  return result;
}

OcdfgConformanceResult compare_ocdfg(
    const Ocel& real, const OcdfgGraph& normative,
    const OcdfgConformanceOptions& options) {
  OcdfgDiscoveryOptions discovery;
  discovery.compute_edges_performance = false;
  return compare_ocdfg(
      discover_ocdfg_classic_with_options(real, discovery), normative,
      options);
}

std::string etot_conformance_json(const EtotConformanceResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto edges = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& edge : values) {
      if (!first) out << ',';
      first = false;
      out << '[';
      write_string(out, edge.first);
      out << ',';
      write_string(out, edge.second);
      out << ']';
    }
    out << ']';
  };
  out << "{\"fitness\":";
  write_conformance_number(out, result.fitness);
  out << ",\"missing_activities\":";
  strings(result.missing_activities);
  out << ",\"additional_activities\":";
  strings(result.additional_activities);
  out << ",\"missing_object_types\":";
  strings(result.missing_object_types);
  out << ",\"additional_object_types\":";
  strings(result.additional_object_types);
  out << ",\"missing_edges\":";
  edges(result.missing_edges);
  out << ",\"additional_edges\":";
  edges(result.additional_edges);
  out << ",\"relative_frequency_delta\":[";
  bool first = true;
  for (const auto& [edge, delta] : result.relative_frequency_delta) {
    if (!first) out << ',';
    first = false;
    out << "{\"activity\":";
    write_string(out, edge.first);
    out << ",\"object_type\":";
    write_string(out, edge.second);
    out << ",\"delta\":";
    write_conformance_number(out, delta);
    out << '}';
  }
  out << "]}";
  return out.str();
}

std::string otg_conformance_json(const OtgConformanceResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto edges = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& edge : values) {
      if (!first) out << ',';
      first = false;
      out << '[';
      write_string(out, std::get<0>(edge));
      out << ',';
      write_string(out, std::get<1>(edge));
      out << ',';
      write_string(out, std::get<2>(edge));
      out << ']';
    }
    out << ']';
  };
  out << "{\"fitness\":";
  write_conformance_number(out, result.fitness);
  out << ",\"missing_object_types\":";
  strings(result.missing_object_types);
  out << ",\"additional_object_types\":";
  strings(result.additional_object_types);
  out << ",\"missing_edges\":";
  edges(result.missing_edges);
  out << ",\"additional_edges\":";
  edges(result.additional_edges);
  out << ",\"non_conforming_edges\":[";
  bool first = true;
  for (const auto& [edge, delta] : result.non_conforming_edges) {
    if (!first) out << ',';
    first = false;
    out << "{\"source_type\":";
    write_string(out, std::get<0>(edge));
    out << ",\"relation\":";
    write_string(out, std::get<1>(edge));
    out << ",\"target_type\":";
    write_string(out, std::get<2>(edge));
    out << ",\"delta\":";
    write_conformance_number(out, delta);
    out << '}';
  }
  out << "]}";
  return out.str();
}

std::string ocdfg_conformance_json(const OcdfgConformanceResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto flows = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& flow : values) {
      if (!first) out << ',';
      first = false;
      out << '[';
      write_string(out, flow.first);
      out << ',';
      write_string(out, flow.second);
      out << ']';
    }
    out << ']';
  };
  out << "{\"fitness\":";
  write_conformance_number(out, result.fitness);
  out << ",\"missing_activities\":";
  strings(result.missing_activities);
  out << ",\"additional_activities\":";
  strings(result.additional_activities);
  out << ",\"missing_flows\":";
  flows(result.missing_flows);
  out << ",\"additional_flows\":";
  flows(result.additional_flows);
  out << ",\"activity_measure_differences\":{";
  bool first = true;
  for (const auto& [activity, difference] : result.activity_measure_differences) {
    if (!first) out << ',';
    first = false;
    write_string(out, activity);
    out << ':' << difference;
  }
  out << "},\"non_conforming_activities_in_measure\":";
  strings(result.non_conforming_activities_in_measure);
  out << ",\"flow_measure_differences\":[";
  first = true;
  for (const auto& [flow, difference] : result.flow_measure_differences) {
    if (!first) out << ',';
    first = false;
    out << "{\"source_activity\":";
    write_string(out, flow.first);
    out << ",\"target_activity\":";
    write_string(out, flow.second);
    out << ",\"difference\":" << difference << '}';
  }
  out << "],\"non_conforming_flows_in_measure\":";
  flows(result.non_conforming_flows_in_measure);
  out << '}';
  return out.str();
}

}  // namespace minera
