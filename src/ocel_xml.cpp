#include "minera/ocel_xml.hpp"

#include "xml_pull_parser.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
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

struct Node {
  std::string name;
  std::vector<std::pair<std::string, std::string>> attrs;
  std::vector<Node> children;
  std::string text;
};

std::string local(std::string_view n) {
  const auto p = n.find_last_of(':');
  return std::string(p == std::string_view::npos ? n : n.substr(p + 1));
}
const std::string* attr(const Node& n, std::string_view key) {
  for (const auto& [k, v] : n.attrs) if (local(k) == key) return &v;
  return nullptr;
}
std::string attr_or(const Node& n, std::string_view key, std::string fallback = {}) {
  if (const auto* v = attr(n, key)) return *v;
  return fallback;
}

Node parse_tree(std::string_view xml) {
  detail::XmlPullParser parser(xml, true);
  Node root;
  std::vector<Node*> stack;
  while (true) {
    auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::CharacterData) {
      if (!stack.empty()) stack.back()->text += token.text;
      continue;
    }
    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      Node n{token.name, std::move(token.attributes), {}, {}};
      if (stack.empty()) {
        if (!root.name.empty()) throw std::invalid_argument("OCEL XML has multiple roots");
        root = std::move(n);
        if (!token.self_closing) stack.push_back(&root);
      } else {
        stack.back()->children.push_back(std::move(n));
        if (!token.self_closing) stack.push_back(&stack.back()->children.back());
      }
      continue;
    }
    if (stack.empty() || local(stack.back()->name) != local(token.name))
      throw std::invalid_argument("OCEL XML has mismatched closing tag");
    stack.pop_back();
  }
  if (root.name.empty() || !stack.empty()) throw std::invalid_argument("OCEL XML is incomplete");
  return root;
}

const Node* child(const Node& n, std::string_view name) {
  for (const auto& c : n.children) if (local(c.name) == name) return &c;
  return nullptr;
}
std::vector<const Node*> children(const Node& n, std::string_view name) {
  std::vector<const Node*> out;
  for (const auto& c : n.children) if (local(c.name) == name) out.push_back(&c);
  return out;
}

OcelValue value_node(const Node& n, bool reject_lists) {
  const auto name = local(n.name);
  std::string value = attr_or(n, "value", n.text);
  if (name == "attribute") {
    const auto type = attr_or(n, "type", "string");
    if (type == "string" || type == "date" || type == "timestamp") return value;
    Node typed = n;
    typed.name = type;
    return value_node(typed, reject_lists);
  }
  if (name == "null") return nullptr;
  if (name == "boolean" || name == "bool") {
    if (value == "true" || value == "1" || value == "TRUE") return true;
    if (value == "false" || value == "0" || value == "FALSE") return false;
    throw std::invalid_argument("invalid OCEL XML boolean value");
  }
  if (name == "int" || name == "integer" || name == "long") {
    std::int64_t i = 0; const auto r = std::from_chars(value.data(), value.data() + value.size(), i);
    if (r.ec != std::errc() || r.ptr != value.data() + value.size()) throw std::invalid_argument("invalid OCEL XML integer value");
    return i;
  }
  if (name == "float" || name == "double" || name == "number") {
    char* end = nullptr; const double d = std::strtod(value.c_str(), &end);
    if (end != value.c_str() + value.size() || !std::isfinite(d)) throw std::invalid_argument("invalid OCEL XML floating value");
    return d;
  }
  if (name == "list" || name == "array") {
    if (reject_lists) throw std::invalid_argument("OCEL XML list values are unsupported");
    std::string joined; bool first = true;
    for (const auto& c : n.children) {
      if (local(c.name) == "string" || local(c.name) == "int" || local(c.name) == "float" || local(c.name) == "boolean") {
        if (!first) joined += ","; first = false;
        const auto v = value_node(c, false);
        if (const auto* s = std::get_if<std::string>(&v)) joined += *s;
        else if (const auto* i = std::get_if<std::int64_t>(&v)) joined += std::to_string(*i);
        else if (const auto* d = std::get_if<double>(&v)) { std::ostringstream o; o << std::setprecision(std::numeric_limits<double>::max_digits10) << *d; joined += o.str(); }
        else if (const auto* b = std::get_if<bool>(&v)) joined += (*b ? "true" : "false");
      }
    }
    return joined;
  }
  return value;
}

void append_values(const Node& n, std::vector<OcelAttribute>& out, bool reject_lists) {
  for (const auto& c : n.children) {
    const auto name = local(c.name);
    if (name == "string" || name == "int" || name == "integer" || name == "long" || name == "float" || name == "double" || name == "number" || name == "boolean" || name == "bool" || name == "null" || name == "date" || name == "timestamp" || name == "list" || name == "array") {
      std::string key = attr_or(c, "key", attr_or(c, "name"));
      if (key.empty()) continue;
      out.push_back({std::move(key), value_node(c, reject_lists)});
    } else if (name == "attributes" || name == "vmap" || name == "ovmap") {
      append_values(c, out, reject_lists);
    } else if (name == "attribute") {
      const std::string key = attr_or(c, "name", attr_or(c, "key"));
      if (key.empty()) continue;
      out.push_back({key, value_node(c, reject_lists)});
    }
  }
}

void parse_classic(const Node& root, Ocel& log, bool reject_lists) {
  std::unordered_map<std::string, std::string> types;
  if (const auto* objects = child(root, "objects")) {
    for (const auto* on : children(*objects, "object")) {
      const std::string id = attr_or(*on, "id", attr_or(*on, "object-id"));
      const std::string type = attr_or(*on, "type", attr_or(*on, "object-type"));
      if (id.empty() || type.empty() || !types.emplace(id, type).second) throw std::invalid_argument("invalid or duplicate OCEL XML object");
      OcelObject o{id, type, {}}; append_values(*on, o.attributes, reject_lists); log.objects.push_back(std::move(o));
      if (const auto* links = child(*on, "o2o")) for (const auto* rel : children(*links, "relation")) {
        log.o2o.push_back({id, attr_or(*rel, "target-id", attr_or(*rel, "object-id", attr_or(*rel, "target"))), attr_or(*rel, "qualifier")});
      }
    }
  }
  if (const auto* events = child(root, "events")) {
    for (const auto* en : children(*events, "event")) {
      OcelEvent e{attr_or(*en, "id", attr_or(*en, "event-id")), attr_or(*en, "activity", attr_or(*en, "type")), attr_or(*en, "timestamp", attr_or(*en, "time")), {}};
      if (e.id.empty() || e.activity.empty() || e.timestamp.empty()) throw std::invalid_argument("invalid OCEL XML event");
      append_values(*en, e.attributes, reject_lists); log.events.push_back(std::move(e));
      if (const auto* omap = child(*en, "omap")) for (const auto* rel : children(*omap, "object")) {
        const std::string oid = attr_or(*rel, "object-id", attr_or(*rel, "id"));
        if (!types.count(oid)) throw std::invalid_argument("OCEL XML relation references unknown object");
        log.relations.push_back({log.events.back().id, oid, attr_or(*rel, "qualifier")});
      }
    }
  }
  if (const auto* links = child(root, "o2o")) for (const auto* rel : children(*links, "relation")) {
    log.o2o.push_back({attr_or(*rel, "source-id", attr_or(*rel, "source")), attr_or(*rel, "target-id", attr_or(*rel, "target")), attr_or(*rel, "qualifier")});
  }
  if (const auto* changes = child(root, "objectChanges")) for (const auto* cn : children(*changes, "change")) {
    OcelObjectChange c{attr_or(*cn, "object-id", attr_or(*cn, "objectId")), attr_or(*cn, "object-type", attr_or(*cn, "type")), attr_or(*cn, "timestamp", attr_or(*cn, "time")), attr_or(*cn, "field"), nullptr};
    if (c.object_type.empty() && types.count(c.object_id)) c.object_type = types[c.object_id];
    for (const auto& v : cn->children) if (local(v.name) != "field") { c.value = value_node(v, reject_lists); break; }
    if (c.object_id.empty() || c.field.empty()) throw std::invalid_argument("invalid OCEL XML object change");
    log.object_changes.push_back(std::move(c));
  }
  for (const auto& g : root.children) if (local(g.name) == "global") {
    const std::string scope = attr_or(g, "scope"); std::vector<OcelAttribute> vals; append_values(g, vals, reject_lists);
    for (auto& a : vals) log.globals[(scope.empty() ? std::string("global") : "ocel:global-" + scope) + "." + a.key] = std::move(a.value);
  }
}

void parse_standard(const Node& root, Ocel& log, bool reject_lists) {
  std::unordered_map<std::string, std::string> types;
  if (const auto* objects = child(root, "objects")) for (const auto* on : children(*objects, "object")) {
    const std::string id = attr_or(*on, "id"), type = attr_or(*on, "type");
    if (id.empty() || type.empty() || !types.emplace(id, type).second) throw std::invalid_argument("invalid or duplicate OCEL XML object");
    OcelObject o{id, type, {}};
    if (const auto* a = child(*on, "attributes")) {
      for (const auto& av : a->children) if (local(av.name) == "attribute") {
        const std::string key = attr_or(av, "name", attr_or(av, "key"));
        if (key.empty()) continue;
        const auto value = value_node(av, reject_lists);
        if (const auto* when = attr(av, "time")) log.object_changes.push_back({id, type, *when, key, value});
        else o.attributes.push_back({key, value});
      }
    } else append_values(*on, o.attributes, reject_lists);
    log.objects.push_back(std::move(o));
    if (const auto* rs = child(*on, "relationships")) for (const auto* r : children(*rs, "relationship")) log.o2o.push_back({id, attr_or(*r, "object-id", attr_or(*r, "objectId")), attr_or(*r, "qualifier")});
  }
  if (const auto* events = child(root, "events")) for (const auto* en : children(*events, "event")) {
    OcelEvent e{attr_or(*en, "id"), attr_or(*en, "type", attr_or(*en, "activity")), attr_or(*en, "time", attr_or(*en, "timestamp")), {}};
    if (e.id.empty() || e.activity.empty() || e.timestamp.empty()) throw std::invalid_argument("invalid OCEL XML event");
    if (const auto* a = child(*en, "attributes")) append_values(*a, e.attributes, reject_lists); log.events.push_back(std::move(e));
    if (const auto* rs = child(*en, "objects")) for (const auto* r : children(*rs, "relationship")) { const std::string oid = attr_or(*r, "object-id", attr_or(*r, "objectId")); if (!types.count(oid)) throw std::invalid_argument("OCEL XML relation references unknown object"); log.relations.push_back({log.events.back().id, oid, attr_or(*r, "qualifier")}); }
  }
  if (const auto* globals = child(root, "globals")) { std::vector<OcelAttribute> vals; append_values(*globals, vals, reject_lists); for (auto& a : vals) log.globals[a.key] = std::move(a.value); }
}

std::string esc(std::string_view s) { std::string o; for (char c : s) { if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '"') o += "&quot;"; else if (c == '\'') o += "&apos;"; else o += c; } return o; }
std::string value_text(const OcelValue& v) { if (std::holds_alternative<std::nullptr_t>(v)) return {}; if (const auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false"; if (const auto* i = std::get_if<std::int64_t>(&v)) return std::to_string(*i); if (const auto* d = std::get_if<double>(&v)) { std::ostringstream o; o << std::setprecision(std::numeric_limits<double>::max_digits10) << *d; return o.str(); } return std::get<std::string>(v); }
std::string value_tag(const OcelValue& v) { if (std::holds_alternative<std::nullptr_t>(v)) return "null"; if (std::holds_alternative<bool>(v)) return "boolean"; if (std::holds_alternative<std::int64_t>(v)) return "int"; if (std::holds_alternative<double>(v)) return "float"; return "string"; }
std::vector<const OcelAttribute*> sorted_attrs(const std::vector<OcelAttribute>& attrs) { std::vector<const OcelAttribute*> out; for (const auto& a : attrs) out.push_back(&a); std::sort(out.begin(), out.end(), [](auto* a, auto* b) { return a->key < b->key; }); return out; }
void write_value(std::ostream& out, const OcelValue& v, std::string_view key, std::string_view indent, bool standard = false, std::string_view time = {}) { const auto tag = standard ? "attribute" : value_tag(v); out << indent << '<' << tag << " name=\"" << esc(key) << "\""; if (standard) out << " type=\"" << value_tag(v) << "\""; else out << " key=\"" << esc(key) << "\""; if (!time.empty()) out << " time=\"" << esc(time) << "\""; out << " value=\"" << esc(value_text(v)) << "\"/>\n"; }

}  // namespace

Ocel parse_ocel_xml(std::string_view xml, const OcelXmlImportOptions& options) {
  if (xml.size() > options.max_bytes) throw std::invalid_argument("OCEL XML exceeds size limit");
  const Node root = parse_tree(xml); Ocel log;
  const bool standard = child(root, "object-types") != nullptr || child(root, "event-types") != nullptr || attr(root, "ocelVersion") != nullptr;
  if (standard) parse_standard(root, log, options.reject_unsupported); else parse_classic(root, log, options.reject_unsupported);
  return log;
}

Ocel import_ocel_xml(std::istream& input, const OcelXmlImportOptions& options) { std::ostringstream s; s << input.rdbuf(); return parse_ocel_xml(s.str(), options); }
Ocel import_ocel_xml(const std::filesystem::path& path, const OcelXmlImportOptions& options) { std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error("Unable to open OCEL XML: " + path.string()); return import_ocel_xml(f, options); }

void write_ocel_xml(std::ostream& out, const Ocel& log, const OcelXmlExportOptions& options) {
  if (!options.allow_lossy) {
    std::set<std::string> object_ids, event_ids;
    for (const auto& o : log.objects) object_ids.insert(o.id);
    for (const auto& e : log.events) event_ids.insert(e.id);
    for (const auto& r : log.relations) if (!event_ids.count(r.event_id) || !object_ids.count(r.object_id)) throw std::invalid_argument("OCEL XML relation references unknown id");
    for (const auto& r : log.o2o) if (!object_ids.count(r.source_id) || !object_ids.count(r.target_id)) throw std::invalid_argument("OCEL XML O2O relation references unknown id");
    for (const auto& c : log.object_changes) if (!object_ids.count(c.object_id)) throw std::invalid_argument("OCEL XML object change references unknown id");
  }
  if (options.include_declaration) out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  std::vector<const OcelObject*> objects; for (const auto& o : log.objects) objects.push_back(&o); std::sort(objects.begin(), objects.end(), [](auto* a, auto* b){ return a->id < b->id; });
  std::vector<const OcelEvent*> events; for (const auto& e : log.events) events.push_back(&e); std::sort(events.begin(), events.end(), [](auto* a, auto* b){ return a->id < b->id; });
  const bool standard = options.format == OcelXmlFormat::kStandard;
  out << "<log" << (standard ? " ocelVersion=\"2.0\"" : "") << ">\n";
  if (standard) {
    out << "  <object-types>\n"; std::set<std::string> types; for (auto* o : objects) types.insert(o->type); for (const auto& t : types) out << "    <object-type name=\"" << esc(t) << "\"/>\n"; out << "  </object-types>\n  <event-types/>\n";
    out << "  <objects>\n"; for (auto* o : objects) { out << "    <object id=\"" << esc(o->id) << "\" type=\"" << esc(o->type) << "\">\n      <attributes>\n"; for (auto* a : sorted_attrs(o->attributes)) write_value(out, a->value, a->key, "        ", true); for (const auto& c : log.object_changes) if (c.object_id == o->id) write_value(out, c.value, c.field, "        ", true, c.timestamp); out << "      </attributes>\n"; std::vector<const OcelO2oRelation*> rs; for (const auto& r : log.o2o) if (r.source_id == o->id) rs.push_back(&r); if (!rs.empty()) { out << "      <relationships>\n"; std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->target_id,a->qualifier) < std::tie(b->target_id,b->qualifier); }); for (auto* r : rs) out << "        <relationship object-id=\"" << esc(r->target_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "      </relationships>\n"; } out << "    </object>\n"; } out << "  </objects>\n  <events>\n";
    for (auto* e : events) { out << "    <event id=\"" << esc(e->id) << "\" type=\"" << esc(e->activity) << "\" time=\"" << esc(e->timestamp) << "\">\n      <attributes>\n"; for (auto* a : sorted_attrs(e->attributes)) write_value(out, a->value, a->key, "        ", true); out << "      </attributes>\n      <objects>\n"; std::vector<const OcelRelation*> rs; for (const auto& r : log.relations) if (r.event_id == e->id) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->object_id,a->qualifier) < std::tie(b->object_id,b->qualifier); }); for (auto* r : rs) out << "        <relationship object-id=\"" << esc(r->object_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "      </objects>\n    </event>\n"; } out << "  </events>\n";
    if (!log.globals.empty()) { out << "  <globals>\n"; for (const auto& [k,v] : log.globals) write_value(out,v,k,"    ",true); out << "  </globals>\n"; }
  } else {
    out << "  <objects>\n"; for (auto* o : objects) { out << "    <object id=\"" << esc(o->id) << "\" type=\"" << esc(o->type) << "\">\n"; for (auto* a : sorted_attrs(o->attributes)) write_value(out,a->value,a->key,"      "); out << "    </object>\n"; } out << "  </objects>\n  <events>\n"; for (auto* e : events) { out << "    <event id=\"" << esc(e->id) << "\" activity=\"" << esc(e->activity) << "\" timestamp=\"" << esc(e->timestamp) << "\">\n      <vmap>\n"; for (auto* a : sorted_attrs(e->attributes)) write_value(out,a->value,a->key,"        "); out << "      </vmap>\n      <omap>\n"; std::vector<const OcelRelation*> rs; for (const auto& r : log.relations) if (r.event_id == e->id) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->object_id,a->qualifier) < std::tie(b->object_id,b->qualifier); }); for (auto* r : rs) out << "        <object object-id=\"" << esc(r->object_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "      </omap>\n    </event>\n"; } out << "  </events>\n";
    if (!log.o2o.empty()) { out << "  <o2o>\n"; std::vector<const OcelO2oRelation*> rs; for (const auto& r : log.o2o) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a,auto* b){ return std::tie(a->source_id,a->target_id,a->qualifier) < std::tie(b->source_id,b->target_id,b->qualifier); }); for (auto* r : rs) out << "    <relation source-id=\"" << esc(r->source_id) << "\" target-id=\"" << esc(r->target_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "  </o2o>\n"; }
    if (!log.object_changes.empty()) { out << "  <objectChanges>\n"; for (const auto& c : log.object_changes) { out << "    <change object-id=\"" << esc(c.object_id) << "\" object-type=\"" << esc(c.object_type) << "\" timestamp=\"" << esc(c.timestamp) << "\" field=\"" << esc(c.field) << "\">\n"; write_value(out,c.value,c.field,"      "); out << "    </change>\n"; } out << "  </objectChanges>\n"; }
    std::map<std::string, std::vector<std::pair<std::string, const OcelValue*>>> globals;
    for (const auto& [key, value] : log.globals) {
      std::string scope = "log", name = key;
      if (key.rfind("ocel:global-", 0) == 0) { const auto dot = key.find('.', 12); if (dot != std::string::npos) { scope = key.substr(12, dot - 12); name = key.substr(dot + 1); } }
      globals[scope].push_back({name, &value});
    }
    for (const auto& [scope, vals] : globals) { out << "  <global scope=\"" << esc(scope) << "\">\n"; for (const auto& [name, value] : vals) write_value(out, *value, name, "    "); out << "  </global>\n"; }
  }
  out << "</log>\n";
}

std::string serialize_ocel_xml(const Ocel& log, const OcelXmlExportOptions& options) { std::ostringstream out; write_ocel_xml(out, log, options); return out.str(); }
void export_ocel_xml(const Ocel& log, const std::filesystem::path& path, const OcelXmlExportOptions& options) { std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("Unable to create OCEL XML: " + path.string()); write_ocel_xml(out, log, options); }

}  // namespace minera
