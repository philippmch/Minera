#include "minera/ocel.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
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
  if (const auto* ch = optional(root, "ocel:objectChanges")) { if (!ch->is_array()) throw std::invalid_argument("ocel:objectChanges must be array"); for (const auto& r : ch->array()) { const auto& rr = obj(r, "ocel:objectChanges"); OcelObjectChange c; c.object_id = str(required(rr, "ocel:oid"), "ocel:oid"); if (!types.count(c.object_id)) throw std::invalid_argument("object change references unknown object"); c.object_type = optional(rr, "ocel:type") ? str(*optional(rr, "ocel:type"), "ocel:type") : types[c.object_id]; c.timestamp = str(required(rr, "ocel:timestamp"), "ocel:timestamp"); c.field = str(required(rr, "ocel:field"), "ocel:field"); c.value = scalar(required(rr, c.field), c.field); log.object_changes.push_back(std::move(c)); } }
  for (const auto& category : {"ocel:global-log", "ocel:global-event", "ocel:global-object"}) if (const auto* g = optional(root, category)) { for (const auto& [key, value] : obj(*g, category)) { if (value.is_scalar()) log.globals[std::string(category) + "." + key] = scalar(value, key); } }
  for (const auto& r : log.o2o) if (!types.count(r.target_id)) throw std::invalid_argument("o2o relation references unknown object");
  return log;
}

Ocel parse_standard(const Json::Object& root) {
  Ocel log; std::unordered_map<std::string, std::string> types; std::unordered_set<std::string> event_ids;
  const auto& object_array = required(root, "objects");
  if (!object_array.is_array()) throw std::invalid_argument("objects must be array");
  for (const auto& raw : object_array.array()) { const auto& x = obj(raw, "objects[]"); const auto id = str(required(x, "id"), "id"); if (types.count(id)) throw std::invalid_argument("duplicate object id"); const auto type = str(required(x, "type"), "type"); types.emplace(id, type); OcelObject o{id, type, {}}; if (const auto* aa = optional(x, "attributes")) { if (!aa->is_array()) throw std::invalid_argument("object attributes must be array"); for (const auto& ar : aa->array()) { const auto& a = obj(ar, "attributes[]"); const auto name = str(required(a, "name"), "name"); const auto val = scalar(required(a, "value"), "value"); if (!optional(a, "time")) o.attributes.push_back({name, val}); else { OcelObjectChange c{id, type, str(*optional(a, "time"), "time"), name, val}; log.object_changes.push_back(std::move(c)); } } } log.objects.push_back(std::move(o)); if (const auto* rr = optional(x, "relationships")) { if (!rr->is_array()) throw std::invalid_argument("object relationships must be array"); for (const auto& r : rr->array()) { const auto& y = obj(r, "relationship"); log.o2o.push_back({id, str(required(y, "objectId"), "objectId"), optional(y, "qualifier") ? str(*optional(y, "qualifier"), "qualifier") : ""}); } } }
  const auto& event_array = required(root, "events");
  if (!event_array.is_array()) throw std::invalid_argument("events must be array");
  for (const auto& raw : event_array.array()) { const auto& x = obj(raw, "events[]"); const auto id = str(required(x, "id"), "id"); if (!event_ids.insert(id).second) throw std::invalid_argument("duplicate event id"); OcelEvent e{id, str(required(x, "type"), "type"), str(required(x, "time"), "time"), {}}; if (const auto* aa = optional(x, "attributes")) { if (!aa->is_array()) throw std::invalid_argument("event attributes must be array"); for (const auto& ar : aa->array()) { const auto& a = obj(ar, "attributes[]"); e.attributes.push_back({str(required(a, "name"), "name"), scalar(required(a, "value"), "value")}); } } log.events.push_back(std::move(e)); const auto* rels = optional(x, "relationships"); if (!rels || !rels->is_array()) throw std::invalid_argument("event relationships must be array"); std::unordered_set<std::string> seen; for (const auto& r : rels->array()) { const auto& y = obj(r, "relationship"); const auto oid = str(required(y, "objectId"), "objectId"); if (!types.count(oid)) throw std::invalid_argument("relation references unknown object"); if (seen.insert(oid).second) log.relations.push_back({id, oid, optional(y, "qualifier") ? str(*optional(y, "qualifier"), "qualifier") : ""}); } }
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
    out << "{\"ocelVersion\":\"2.0\",\"objectTypes\":[";
    std::set<std::string> object_types;
    for (const auto& object : log.objects) object_types.insert(object.type);
    bool first = true;
    for (const auto& type : object_types) {
      if (!first) out << ',';
      first = false;
      out << "{\"name\":";
      write_string(out, type);
      out << ",\"attributes\":[]}";
    }
    out << "],\"eventTypes\":[],\"events\":[";
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
      for (const auto& attribute : object->attributes) {
        if (!attribute_first) out << ',';
        attribute_first = false;
        out << "{\"name\":";
        write_string(out, attribute.key);
        out << ",\"value\":";
        write_value(out, attribute.value);
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
      for (const auto* change : changes) {
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
  out << "},\"ocel:objects\":{";
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

}  // namespace minera
