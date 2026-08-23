#include "minera/ocel.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace minera {
namespace {

struct Json {
  using Object = std::map<std::string, Json>;
  using Array = std::vector<Json>;
  std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array,
               Object>
      value;
  bool is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
  bool is_bool() const { return std::holds_alternative<bool>(value); }
  bool is_int() const { return std::holds_alternative<std::int64_t>(value); }
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
  Parser(std::string_view input, const OcelJsonLimits& limits)
      : input_(input), limits_(limits) {
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
          if (pos_ + 4 > input_.size()) fail("short unicode escape");
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) { const char h = input_[pos_++]; code <<= 4; if (h >= '0' && h <= '9') code += h - '0'; else if (h >= 'a' && h <= 'f') code += h - 'a' + 10; else if (h >= 'A' && h <= 'F') code += h - 'A' + 10; else fail("bad unicode escape"); }
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
      ++pos_; Json::Object object; skip();
      if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; return Json{std::move(object)}; }
      while (true) {
        skip(); if (pos_ >= input_.size() || input_[pos_] != '"') fail("object key must be string");
        auto key = string_value(); if (object.find(key) != object.end()) fail("duplicate object key");
        expect(':'); object.emplace(std::move(key), value(depth + 1)); skip();
        if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; break; }
        expect(',');
      }
      return Json{std::move(object)};
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
    std::int64_t n = 0; const auto result = std::from_chars(token.data(), token.data() + token.size(), n); if (result.ec != std::errc() || result.ptr != token.data() + token.size()) fail("integer out of range"); return Json{n};
  }
  std::string_view input_; const OcelJsonLimits& limits_; std::size_t pos_ = 0; std::size_t nodes_ = 0;
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

using StandardAttributeTypes =
    std::map<std::string, std::map<std::string, std::string>>;

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

void parse_standard_type_table(const Json* raw, StandardAttributeTypes& types,
                               std::string_view field) {
  if (!raw) return;
  if (!raw->is_array())
    throw std::invalid_argument("OCEL " + std::string(field) +
                                " must be an array");
  for (const auto& entry : raw->array()) {
    const auto& descriptor = obj(entry, field);
    const auto name = str(required(descriptor, "name"), "type name");
    if (name.empty() || types.contains(name))
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
        if (attribute_name.empty() || attribute_type.empty() ||
            !attributes.emplace(attribute_name, attribute_type).second)
          throw std::invalid_argument("duplicate or empty OCEL attribute type");
      }
    }
    types.emplace(name, std::move(attributes));
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

bool is_standard_epoch(std::string_view value) {
  return value == "0" || value == "1970-01-01T00:00:00Z" ||
         value == "1970-01-01T00:00:00.000Z";
}

Ocel parse_classic(const Json::Object& root) {
  Ocel log; std::unordered_set<std::string> object_ids, event_ids; std::unordered_map<std::string, std::string> types;
  const auto& objects = obj(required(root, "ocel:objects"), "ocel:objects");
  for (const auto& [id, raw] : objects) {
    if (!object_ids.insert(id).second) throw std::invalid_argument("duplicate object id");
    const auto& x = obj(raw, "object"); OcelObject o{id, str(required(x, "ocel:type"), "ocel:type"), {}}; attrs(optional(x, "ocel:ovmap"), o.attributes, "ocel:ovmap"); log.objects.push_back(std::move(o)); types.emplace(id, log.objects.back().type);
    if (const auto* rs = optional(x, "ocel:o2o")) { if (!rs->is_array()) throw std::invalid_argument("ocel:o2o must be array"); for (const auto& r : rs->array()) { const auto& rr = obj(r, "ocel:o2o"); log.o2o.push_back({id, str(required(rr, "ocel:oid"), "ocel:oid"), optional(rr, "ocel:qualifier") ? str(*optional(rr, "ocel:qualifier"), "ocel:qualifier") : ""}); } }
  }
  const auto& events = obj(required(root, "ocel:events"), "ocel:events");
  for (const auto& [id, raw] : events) {
    if (!event_ids.insert(id).second) throw std::invalid_argument("duplicate event id");
    const auto& x = obj(raw, "event"); OcelEvent e{id, str(required(x, "ocel:activity"), "ocel:activity"), str(required(x, "ocel:timestamp"), "ocel:timestamp"), {}}; attrs(optional(x, "ocel:vmap"), e.attributes, "ocel:vmap"); log.events.push_back(std::move(e));
    const auto* omap = optional(x, "ocel:omap"); if (!omap || !omap->is_array()) throw std::invalid_argument("ocel:omap must be array");
    std::unordered_set<std::string> seen;
    for (const auto& oidj : omap->array()) { const auto oid = str(oidj, "ocel:omap"); if (!types.count(oid)) throw std::invalid_argument("relation references unknown object"); if (seen.insert(oid).second) log.relations.push_back({id, oid, ""}); }
    if (const auto* typed = optional(x, "ocel:typedOmap")) { if (!typed->is_array()) throw std::invalid_argument("ocel:typedOmap must be array"); for (const auto& r : typed->array()) { const auto& rr = obj(r, "ocel:typedOmap"); const auto oid = str(required(rr, "ocel:oid"), "ocel:oid"); if (!types.count(oid) || !seen.count(oid)) throw std::invalid_argument("typed relation references unknown object"); for (auto& rel : log.relations) if (rel.event_id == id && rel.object_id == oid) rel.qualifier = optional(rr, "ocel:qualifier") ? str(*optional(rr, "ocel:qualifier"), "ocel:qualifier") : ""; } }
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

Ocel parse_standard(const Json::Object& root) {
  Ocel log;
  std::unordered_map<std::string, std::string> types;
  std::unordered_set<std::string> event_ids;
  StandardAttributeTypes object_attribute_types;
  StandardAttributeTypes event_attribute_types;
  parse_standard_type_table(optional(root, "objectTypes"), object_attribute_types,
                            "objectTypes");
  parse_standard_type_table(optional(root, "eventTypes"), event_attribute_types,
                            "eventTypes");
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
    if (id.empty() || types.count(id))
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
        if (name.empty())
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
          if (!entries[i].has_time || is_standard_epoch(entries[i].time))
            throw std::invalid_argument(
                "OCEL object attribute has duplicate static value");
          log.object_changes.push_back(
              {id, type, entries[i].time, name, entries[i].value});
        }
      }
    }
    log.objects.push_back(std::move(object));
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
    if (id.empty() || !event_ids.insert(id).second)
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
  for (const auto& object : log.objects) {
    auto& attributes = object_types[object.type];
    for (const auto& attribute : object.attributes)
      merge_standard_type(attributes[attribute.key], attribute.value);
  }
  for (const auto& change : log.object_changes)
    merge_standard_type(object_types[change.object_type][change.field],
                        change.value);
  for (const auto& event : log.events) {
    auto& attributes = event_types[event.activity];
    for (const auto& attribute : event.attributes)
      merge_standard_type(attributes[attribute.key], attribute.value);
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

Ocel parse_ocel_json(std::string_view json, const OcelJsonLimits& limits) {
  const Json root = Parser(json, limits).parse();
  if (!root.is_object()) throw std::invalid_argument("OCEL root must be object");
  const auto& r = root.object();
  if (r.find("events") != r.end() || r.find("objects") != r.end()) return parse_standard(r);
  return parse_classic(r);
}

std::string serialize_ocel_json(const Ocel& log, OcelJsonFormat format) {
  std::ostringstream out;
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

  auto event_relations = [&](const OcelEvent& event) {
    std::vector<const OcelRelation*> relations;
    for (const auto& relation : log.relations) {
      if (relation.event_id == event.id) relations.push_back(&relation);
    }
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
    out << '}';
    return out.str();
  }

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
  out << '}';
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
  out << '}';
  return out.str();
}

EtotGraph discover_etot_classic(const Ocel& log) {
  EtotGraph result; std::set<std::string> activities, types; std::map<std::pair<std::string, std::string>, std::size_t> counts; std::unordered_map<std::string, std::string> lookup; std::unordered_map<std::string, std::string> event_activity; for (const auto& o : log.objects) lookup[o.id] = o.type; for (const auto& e : log.events) event_activity[e.id] = e.activity; for (const auto& r : log.relations) { auto it = lookup.find(r.object_id); auto a = event_activity.find(r.event_id); if (it == lookup.end() || a == event_activity.end()) continue; activities.insert(a->second); types.insert(it->second); ++counts[{a->second, it->second}]; } result.activities.assign(activities.begin(), activities.end()); result.object_types.assign(types.begin(), types.end()); for (const auto& [key, frequency] : counts) result.edges.push_back({key.first, key.second, frequency}); return result;
}

OtgGraph discover_otg_classic(const Ocel& log) {
  OtgGraph result; std::set<std::string> types; std::unordered_map<std::string, std::string> type; for (const auto& o : log.objects) { type[o.id] = o.type; types.insert(o.type); } result.object_types.assign(types.begin(), types.end());
  std::unordered_map<std::string, std::vector<std::string>> by_event; for (const auto& r : log.relations) by_event[r.event_id].push_back(r.object_id); std::vector<std::string> order; for (const auto& e : log.events) { order.push_back(e.id); auto& v = by_event[e.id]; std::sort(v.begin(), v.end()); v.erase(std::unique(v.begin(), v.end()), v.end()); }
  std::map<std::tuple<std::string, std::string, std::string>, std::size_t> counts; auto add = [&](const std::string& a, const std::string& rel, const std::string& b) { if (type.count(a) && type.count(b)) ++counts[{type[a], rel, type[b]}]; };
  for (const auto& ev : order) { auto v = by_event[ev]; for (std::size_t i = 0; i < v.size(); ++i) for (std::size_t j = i + 1; j < v.size(); ++j) add(v[i], "object_interaction", v[j]); }
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

OcdfgGraph discover_ocdfg_classic(const Ocel& log, bool compute_edges_performance) {
  OcdfgGraph result;
  result.performance_computed = compute_edges_performance;
  std::map<std::string, std::string> activity;
  std::map<std::string, std::string> timestamp;
  std::map<std::string, std::size_t> event_order;
  for (std::size_t i = 0; i < log.events.size(); ++i) {
    activity[log.events[i].id] = log.events[i].activity;
    timestamp[log.events[i].id] = log.events[i].timestamp;
    event_order[log.events[i].id] = i;
    result.activities.push_back(log.events[i].activity);
  }
  std::sort(result.activities.begin(), result.activities.end());
  result.activities.erase(std::unique(result.activities.begin(), result.activities.end()), result.activities.end());
  std::map<std::string, std::string> object_type;
  for (const auto& object : log.objects) object_type[object.id] = object.type;
  for (const auto& object : log.objects) result.object_types.push_back(object.type);
  std::sort(result.object_types.begin(), result.object_types.end());
  result.object_types.erase(std::unique(result.object_types.begin(), result.object_types.end()), result.object_types.end());

  struct Rel { std::string event; std::string object; std::size_t order; };
  std::vector<Rel> rels;
  for (std::size_t i = 0; i < log.relations.size(); ++i) {
    const auto& relation = log.relations[i];
    const auto event_it = event_order.find(relation.event_id);
    const auto type_it = object_type.find(relation.object_id);
    if (event_it == event_order.end() || type_it == object_type.end()) continue;
    rels.push_back({relation.event_id, relation.object_id, event_it->second * (log.relations.size() + 1) + i});
  }
  auto add_association = [&](std::map<std::string, OcdfgActivityMetric>& target,
                             const std::string& act, const std::string& event,
                             const std::string& object) {
    auto& metric = target[act];
    metric.events.push_back(event);
    metric.unique_objects.push_back(object);
    metric.event_objects.emplace_back(event, object);
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
    std::sort(metric.event_objects.begin(), metric.event_objects.end());
    metric.event_objects.erase(std::unique(metric.event_objects.begin(), metric.event_objects.end()), metric.event_objects.end());
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
      if (compute_edges_performance) {
        const auto from = parse_timestamp(timestamp[sequence[i - 1].event]);
        const auto to = parse_timestamp(timestamp[sequence[i].event]);
        if (from && to) edge.performance.push_back(*to - *from);
      }
    }
  }
  for (auto& [type, metrics] : result.start_activities) for (auto& [act, metric] : metrics) normalize_activity(metric);
  for (auto& [type, metrics] : result.end_activities) for (auto& [act, metric] : metrics) normalize_activity(metric);
  for (auto& [type, metrics] : result.edges) for (auto& [pair, edge] : metrics) {
    std::sort(edge.event_couples.begin(), edge.event_couples.end()); edge.event_couples.erase(std::unique(edge.event_couples.begin(), edge.event_couples.end()), edge.event_couples.end());
    std::sort(edge.unique_objects.begin(), edge.unique_objects.end()); edge.unique_objects.erase(std::unique(edge.unique_objects.begin(), edge.unique_objects.end()), edge.unique_objects.end());
    std::sort(edge.event_objects.begin(), edge.event_objects.end()); edge.event_objects.erase(std::unique(edge.event_objects.begin(), edge.event_objects.end()), edge.event_objects.end());
    std::sort(edge.performance.begin(), edge.performance.end());
  }
  return result;
}

std::string serialize_ocdfg_json(const OcdfgGraph& graph) {
  std::ostringstream out;
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
      out << "],\"performance\":[";
      for (std::size_t i = 0; i < edge.performance.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        out << edge.performance[i];
      }
      out << "]}";
    }
    out << '}';
  }
  out << "},\"performance_computed\":"
      << (graph.performance_computed ? "true" : "false") << '}';
  return out.str();
}

OcpnGraph discover_ocpn(const Ocel& log, const OcpnDiscoveryOptions& options) {
  if (!std::isfinite(options.double_arc_threshold) ||
      options.double_arc_threshold < 0.0 || options.double_arc_threshold > 1.0)
    throw std::invalid_argument("OCPN double_arc_threshold must be in [0,1]");
  if (!std::isfinite(options.noise_threshold) || options.noise_threshold < 0.0 ||
      options.noise_threshold > 1.0)
    throw std::invalid_argument("OCPN noise_threshold must be in [0,1]");

  OcpnGraph result;
  result.annotated = options.annotated;
  std::map<std::string, const OcelEvent*> event_by_id;
  std::map<std::string, std::size_t> event_order;
  for (std::size_t index = 0; index < log.events.size(); ++index) {
    event_by_id[log.events[index].id] = &log.events[index];
    event_order[log.events[index].id] = index;
    result.activities.push_back(log.events[index].activity);
  }
  std::sort(result.activities.begin(), result.activities.end());
  result.activities.erase(
      std::unique(result.activities.begin(), result.activities.end()),
      result.activities.end());
  std::map<std::string, std::string> object_type;
  for (const auto& object : log.objects) {
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
  std::map<std::string, std::set<std::string>> object_ids_by_activity;
  for (const auto& relation : log.relations) {
    const auto event_it = event_by_id.find(relation.event_id);
    const auto type_it = object_type.find(relation.object_id);
    if (event_it == event_by_id.end() || type_it == object_type.end()) continue;
    if (!object_ids_by_activity[event_it->second->activity].insert(
            relation.object_id)
             .second)
      continue;
    result.activity_object_types[event_it->second->activity].push_back(
        type_it->second);
  }
  for (auto& [activity, types] : result.activity_object_types) {
    (void)activity;
    std::sort(types.begin(), types.end());
  }

  if (options.annotated) {
    result.annotations = discover_ocdfg_classic(log, false);
  }
  for (const auto& type : result.object_types) {
    EventLog flattened;
    std::map<std::string, std::set<std::string>> object_event_ids;
    for (const auto& relation : log.relations) {
      const auto type_it = object_type.find(relation.object_id);
      if (type_it == object_type.end() || type_it->second != type) continue;
      if (event_by_id.count(relation.event_id))
        object_event_ids[relation.object_id].insert(relation.event_id);
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

    InductiveMinerOptions miner_options;
    miner_options.noise_threshold = options.noise_threshold;
    miner_options.variant = options.variant == OcpnInductiveVariant::kDfg
                                ? InductiveMinerVariant::kDfg
                                : InductiveMinerVariant::kLog;
    const auto tree = discover_process_tree(flattened, miner_options);
    result.petri_nets[type] = to_petri_net(tree);

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
    for (std::size_t i = 0; i < metric.event_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, metric.event_objects[i].first);
      out << ',';
      write_string(out, metric.event_objects[i].second);
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
      out << ']';
      out << ",\"performance\":[";
      for (std::size_t i = 0; i < edge.performance.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        out << edge.performance[i];
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
      << (options.include_timestamps ? "true" : "false");
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
  out << "],\"objects\":[";
  for (std::size_t index = 0; index < description.objects.size(); ++index) {
    if (index != 0) out << ',';
    const auto& object = description.objects[index];
    out << '[';
    write_string(out, object.id);
    out << ',';
    write_string(out, object.type);
    if (description.includes_timestamps) {
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

void validate_conformance_number(double value, const char* name) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument(std::string(name) + " must be finite and non-negative");
}

void write_conformance_number(std::ostringstream& out, double value) {
  if (!std::isfinite(value)) {
    out << "null";
    return;
  }
  out << std::setprecision(17) << value;
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
  validate_conformance_number(options.alpha, "ETOT alpha");
  validate_conformance_number(options.beta, "ETOT beta");
  validate_conformance_number(options.gamma, "ETOT gamma");
  validate_conformance_number(options.theta_relative, "ETOT theta_relative");
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
    const double delta = normative_count == 0
                             ? std::numeric_limits<double>::infinity()
                             : std::abs(static_cast<double>(real_count) -
                                        static_cast<double>(normative_count)) /
                                   static_cast<double>(normative_count);
    result.relative_frequency_delta[edge] = delta;
    if (delta > options.theta_relative) ++relative_mismatches;
  }

  const double normalizer =
      options.alpha * static_cast<double>(normative_activities.size() + normative_types.size()) +
      options.beta * static_cast<double>(normative_edges.size()) +
      options.gamma * static_cast<double>(normative_edges.size());
  if (normalizer == 0.0) {
    result.fitness = 1.0;
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

OtgConformanceResult compare_otg(const OtgGraph& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options) {
  validate_conformance_number(options.alpha, "OTG alpha");
  validate_conformance_number(options.beta, "OTG beta");
  validate_conformance_number(options.gamma, "OTG gamma");
  validate_conformance_number(options.theta, "OTG theta");
  for (const auto& [relation, theta] : options.theta_by_relation) {
    (void)relation;
    validate_conformance_number(theta, "OTG relation theta");
  }
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
                             : std::abs(static_cast<double>(real_count) -
                                        static_cast<double>(normative_count)) /
                                   static_cast<double>(normative_count);
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
    result.fitness = 1.0;
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_object_types.size()) +
        options.beta * static_cast<double>(result.missing_edges.size()) +
        options.gamma * static_cast<double>(non_conforming);
    result.fitness = 1.0 - numerator / normalizer;
  }
  return result;
}

OcdfgConformanceResult compare_ocdfg(const OcdfgGraph& real,
                                     const OcdfgGraph& normative,
                                     const OcdfgConformanceOptions& options) {
  validate_conformance_number(options.theta_activity, "OCDFG theta_activity");
  validate_conformance_number(options.theta_flow, "OCDFG theta_flow");
  validate_conformance_number(options.alpha, "OCDFG alpha");
  validate_conformance_number(options.beta, "OCDFG beta");
  validate_conformance_number(options.gamma, "OCDFG gamma");
  validate_conformance_number(options.delta, "OCDFG delta");
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
    result.fitness = std::clamp(1.0 - numerator / normalizer, 0.0, 1.0);
  }
  return result;
}

std::string etot_conformance_json(const EtotConformanceResult& result) {
  std::ostringstream out;
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
