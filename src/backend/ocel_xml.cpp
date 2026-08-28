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

bool is_value_node_name(std::string_view name) {
  return name == "string" || name == "int" || name == "integer" ||
         name == "long" || name == "float" || name == "double" ||
         name == "number" || name == "boolean" || name == "bool" ||
         name == "null" || name == "date" || name == "timestamp" ||
         name == "id" || name == "list" || name == "array" ||
         name == "attribute";
}

std::string keyed_value(const Node& n, std::string_view wanted) {
  for (const auto& c : n.children) {
    if (!is_value_node_name(local(c.name))) continue;
    const auto key = attr_or(c, "key", attr_or(c, "name"));
    if (key == wanted) return attr_or(c, "value", c.text);
  }
  return {};
}

void append_values_filtered(const Node& n, std::vector<OcelAttribute>& out,
                            bool reject_lists,
                            const std::unordered_set<std::string>& excluded) {
  for (const auto& c : n.children) {
    const auto name = local(c.name);
    const auto key = attr_or(c, "key", attr_or(c, "name"));
    if (name == "omap" ||
        ((name == "list" || name == "array") && key == "omap")) {
      continue;
    }
    if (name == "attributes" || name == "vmap" || name == "ovmap" ||
        ((name == "list" || name == "array") &&
         (key == "vmap" || key == "ovmap"))) {
      append_values_filtered(c, out, reject_lists,
                             std::unordered_set<std::string>{});
      continue;
    }
    if (!is_value_node_name(name)) continue;
    if (key.empty() || excluded.count(key)) continue;
    out.push_back({key, value_node(c, reject_lists)});
  }
}

void append_classic_relations(const Node& omap, std::string_view event_id,
                              const std::unordered_map<std::string, std::string>& types,
                              Ocel& log) {
  for (const auto& rel : omap.children) {
    const auto name = local(rel.name);
    const std::string key = attr_or(rel, "key", attr_or(rel, "name"));
    if (name != "object" && name != "relationship" &&
        !(is_value_node_name(name) && key == "object-id")) {
      continue;
    }
    const std::string oid = attr_or(rel, "object-id",
                                    attr_or(rel, "objectId",
                                            attr_or(rel, "id",
                                                    key == "object-id"
                                                        ? attr_or(rel, "value", rel.text)
                                                        : std::string{})));
    if (oid.empty()) throw std::invalid_argument("invalid OCEL XML event relation");
    if (!types.count(oid)) throw std::invalid_argument("OCEL XML relation references unknown object");
    log.relations.push_back({std::string(event_id), oid, attr_or(rel, "qualifier")});
  }
}

using StandardXmlAttributeTypes =
    std::map<std::string, std::map<std::string, std::string>>;

void parse_standard_xml_type_table(
    const Node& root, std::string_view container_name,
    std::string_view type_name, StandardXmlAttributeTypes& result) {
  for (const auto* container : children(root, container_name)) {
    for (const auto* descriptor : children(*container, type_name)) {
      const auto name = attr_or(*descriptor, "name");
      if (name.empty() || result.contains(name))
        throw std::invalid_argument("invalid or duplicate OCEL XML type");
      std::map<std::string, std::string> attributes;
      if (const auto* attribute_container = child(*descriptor, "attributes")) {
        for (const auto* attribute : children(*attribute_container, "attribute")) {
          const auto attribute_name = attr_or(*attribute, "name", attr_or(*attribute, "key"));
          const auto attribute_type = attr_or(*attribute, "type", "string");
          if (attribute_name.empty() || attribute_type.empty() ||
              !attributes.emplace(attribute_name, attribute_type).second)
            throw std::invalid_argument("invalid or duplicate OCEL XML attribute type");
        }
      }
      result.emplace(name, std::move(attributes));
    }
  }
}

OcelValue standard_xml_value(const Node& node, const std::string* declared_type,
                             bool reject_lists) {
  if (declared_type == nullptr || attr(node, "type") != nullptr)
    return value_node(node, reject_lists);
  Node typed = node;
  typed.attrs.emplace_back("type", *declared_type);
  return value_node(typed, reject_lists);
}

void parse_classic(const Node& root, Ocel& log, bool reject_lists) {
  std::unordered_map<std::string, std::string> types;
  for (const auto* objects : children(root, "objects")) {
    for (const auto* on : children(*objects, "object")) {
      const std::string id = attr_or(*on, "id", attr_or(*on, "object-id")).empty()
                                 ? keyed_value(*on, "id")
                                 : attr_or(*on, "id", attr_or(*on, "object-id"));
      const std::string type = attr_or(*on, "type", attr_or(*on, "object-type")).empty()
                                   ? (keyed_value(*on, "type").empty()
                                          ? keyed_value(*on, "object-type")
                                          : keyed_value(*on, "type"))
                                   : attr_or(*on, "type", attr_or(*on, "object-type"));
      if (id.empty() || type.empty() || !types.emplace(id, type).second) throw std::invalid_argument("invalid or duplicate OCEL XML object");
      OcelObject o{id, type, {}};
      append_values_filtered(*on, o.attributes, reject_lists,
                             {"id", "object-id", "type", "object-type"});
      log.objects.push_back(std::move(o));
      log.object_type_attributes.try_emplace(type);
      for (const auto* links : children(*on, "o2o"))
        for (const auto* rel : children(*links, "relation")) {
          log.o2o.push_back({id, attr_or(*rel, "target-id",
                                        attr_or(*rel, "object-id",
                                                attr_or(*rel, "target"))),
                            attr_or(*rel, "qualifier")});
        }
    }
  }
  for (const auto* events : children(root, "events")) {
    for (const auto* en : children(*events, "event")) {
      const std::string id = attr_or(*en, "id", attr_or(*en, "event-id")).empty()
                                 ? keyed_value(*en, "id")
                                 : attr_or(*en, "id", attr_or(*en, "event-id"));
      const std::string activity =
          attr_or(*en, "activity", attr_or(*en, "type")).empty()
              ? (keyed_value(*en, "activity").empty()
                     ? keyed_value(*en, "type")
                     : keyed_value(*en, "activity"))
              : attr_or(*en, "activity", attr_or(*en, "type"));
      const std::string timestamp =
          attr_or(*en, "timestamp", attr_or(*en, "time")).empty()
              ? (keyed_value(*en, "timestamp").empty()
                     ? keyed_value(*en, "time")
                     : keyed_value(*en, "timestamp"))
              : attr_or(*en, "timestamp", attr_or(*en, "time"));
      OcelEvent e{id, activity, timestamp, {}};
      if (e.id.empty() || e.activity.empty() || e.timestamp.empty()) throw std::invalid_argument("invalid OCEL XML event");
      append_values_filtered(*en, e.attributes, reject_lists,
                             {"id", "event-id", "activity", "type", "timestamp", "time"});
      log.events.push_back(std::move(e));
      log.event_type_attributes.try_emplace(log.events.back().activity);
      for (const auto& child_node : en->children) {
        const auto name = local(child_node.name);
        const auto key = attr_or(child_node, "key", attr_or(child_node, "name"));
        if (name == "omap" || (name == "list" && key == "omap"))
          append_classic_relations(child_node, log.events.back().id, types, log);
      }
    }
  }
  for (const auto* links : children(root, "o2o"))
    for (const auto* rel : children(*links, "relation")) {
      log.o2o.push_back({attr_or(*rel, "source-id", attr_or(*rel, "source")),
                        attr_or(*rel, "target-id", attr_or(*rel, "target")),
                        attr_or(*rel, "qualifier")});
    }
  for (const auto* links : children(root, "e2e"))
    for (const auto* rel : children(*links, "relation")) {
      log.e2e.push_back({attr_or(*rel, "source-id", attr_or(*rel, "source")),
                        attr_or(*rel, "target-id", attr_or(*rel, "target")),
                        attr_or(*rel, "qualifier")});
    }
  for (const auto* changes : children(root, "objectChanges"))
    for (const auto* cn : children(*changes, "change")) {
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
  StandardXmlAttributeTypes object_attribute_types;
  StandardXmlAttributeTypes event_attribute_types;
  parse_standard_xml_type_table(root, "object-types", "object-type",
                                object_attribute_types);
  parse_standard_xml_type_table(root, "event-types", "event-type",
                                event_attribute_types);
  log.object_type_attributes = object_attribute_types;
  log.event_type_attributes = event_attribute_types;
  struct StandardAttribute {
    std::string name;
    OcelValue value;
    bool has_time = false;
    std::string time;
  };
  for (const auto* objects : children(root, "objects"))
    for (const auto* on : children(*objects, "object")) {
      const std::string id = attr_or(*on, "id"), type = attr_or(*on, "type");
      if (id.empty() || type.empty() || !types.emplace(id, type).second) throw std::invalid_argument("invalid or duplicate OCEL XML object");
      OcelObject o{id, type, {}};
      if (const auto* a = child(*on, "attributes")) {
        std::map<std::string, std::vector<StandardAttribute>> grouped;
        for (const auto& av : a->children) if (local(av.name) == "attribute") {
          const std::string key = attr_or(av, "name", attr_or(av, "key"));
          if (key.empty()) continue;
          const auto* declared = [&]() -> const std::string* {
            const auto owner = object_attribute_types.find(type);
            if (owner == object_attribute_types.end()) return nullptr;
            const auto found = owner->second.find(key);
            return found == owner->second.end() ? nullptr : &found->second;
          }();
          StandardAttribute entry{key, standard_xml_value(av, declared, reject_lists)};
          if (const auto* when = attr(av, "time")) {
            entry.has_time = true;
            entry.time = *when;
          }
          grouped[key].push_back(std::move(entry));
        }
        for (auto& [key, entries] : grouped) {
          if (entries.empty()) continue;
          o.attributes.push_back({key, entries.front().value});
          for (std::size_t i = 1; i < entries.size(); ++i) {
            if (!entries[i].has_time)
              throw std::invalid_argument("OCEL XML object attribute change requires a timestamp");
            log.object_changes.push_back({id, type, entries[i].time, key,
                                           entries[i].value});
          }
        }
      } else append_values(*on, o.attributes, reject_lists);
      log.objects.push_back(std::move(o));
      log.object_type_attributes.try_emplace(type);
      for (const auto* rs : children(*on, "relationships"))
        for (const auto* r : children(*rs, "relationship"))
          log.o2o.push_back({id, attr_or(*r, "object-id", attr_or(*r, "objectId")), attr_or(*r, "qualifier")});
      for (const auto* rs : children(*on, "objects"))
        for (const auto* r : children(*rs, "relationship"))
          log.o2o.push_back({id, attr_or(*r, "object-id", attr_or(*r, "objectId")), attr_or(*r, "qualifier")});
    }
  for (const auto* events : children(root, "events"))
    for (const auto* en : children(*events, "event")) {
      OcelEvent e{attr_or(*en, "id"), attr_or(*en, "type", attr_or(*en, "activity")), attr_or(*en, "time", attr_or(*en, "timestamp")), {}};
      if (e.id.empty() || e.activity.empty() || e.timestamp.empty()) throw std::invalid_argument("invalid OCEL XML event");
      if (const auto* a = child(*en, "attributes")) {
        for (const auto& av : a->children) if (local(av.name) == "attribute") {
          const auto key = attr_or(av, "name", attr_or(av, "key"));
          if (key.empty()) continue;
          const auto owner = event_attribute_types.find(e.activity);
          const auto found = owner == event_attribute_types.end()
                                 ? std::map<std::string, std::string>::const_iterator{}
                                 : owner->second.find(key);
          const auto* declared = owner == event_attribute_types.end() ||
                                         found == owner->second.end()
                                     ? nullptr
                                     : &found->second;
          e.attributes.push_back({key, standard_xml_value(av, declared,
                                                           reject_lists)});
        }
      }
      log.events.push_back(std::move(e));
      log.event_type_attributes.try_emplace(log.events.back().activity);
      for (const auto* rs : children(*en, "objects"))
        for (const auto* r : children(*rs, "relationship")) {
          const std::string oid = attr_or(*r, "object-id", attr_or(*r, "objectId"));
          if (!types.count(oid)) throw std::invalid_argument("OCEL XML relation references unknown object");
          log.relations.push_back({log.events.back().id, oid, attr_or(*r, "qualifier")});
        }
    }
  if (const auto* globals = child(root, "globals")) { std::vector<OcelAttribute> vals; append_values(*globals, vals, reject_lists); for (auto& a : vals) log.globals[a.key] = std::move(a.value); }
  for (const auto* links : children(root, "e2e"))
    for (const auto* rel : children(*links, "relationship")) {
      log.e2e.push_back({attr_or(*rel, "source-event-id", attr_or(*rel, "source-id")),
                        attr_or(*rel, "target-event-id", attr_or(*rel, "target-id")),
                        attr_or(*rel, "qualifier")});
    }
}

void validate_references(const Ocel& log) {
  std::unordered_set<std::string> object_ids;
  std::unordered_set<std::string> event_ids;
  for (const auto& object : log.objects) object_ids.insert(object.id);
  for (const auto& event : log.events) event_ids.insert(event.id);
  for (const auto& relation : log.relations) {
    if (!event_ids.count(relation.event_id) ||
        !object_ids.count(relation.object_id)) {
      throw std::invalid_argument("OCEL XML relation references unknown id");
    }
  }
  for (const auto& relation : log.o2o) {
    if (!object_ids.count(relation.source_id) ||
        !object_ids.count(relation.target_id)) {
      throw std::invalid_argument("OCEL XML O2O relation references unknown id");
    }
  }
  for (const auto& relation : log.e2e) {
    if (!event_ids.count(relation.source_id) ||
        !event_ids.count(relation.target_id)) {
      throw std::invalid_argument("OCEL XML E2E relation references unknown id");
    }
  }
  for (const auto& change : log.object_changes) {
    if (!object_ids.count(change.object_id)) {
      throw std::invalid_argument("OCEL XML object change references unknown id");
    }
  }
}

std::string esc(std::string_view s) { std::string o; for (char c : s) { if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '"') o += "&quot;"; else if (c == '\'') o += "&apos;"; else o += c; } return o; }
std::string value_text(const OcelValue& v) { if (std::holds_alternative<std::nullptr_t>(v)) return {}; if (const auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false"; if (const auto* i = std::get_if<std::int64_t>(&v)) return std::to_string(*i); if (const auto* d = std::get_if<double>(&v)) { std::ostringstream o; o << std::setprecision(std::numeric_limits<double>::max_digits10) << *d; return o.str(); } return std::get<std::string>(v); }
std::string value_tag(const OcelValue& v) { if (std::holds_alternative<std::nullptr_t>(v)) return "null"; if (std::holds_alternative<bool>(v)) return "boolean"; if (std::holds_alternative<std::int64_t>(v)) return "int"; if (std::holds_alternative<double>(v)) return "float"; return "string"; }
std::vector<const OcelAttribute*> sorted_attrs(const std::vector<OcelAttribute>& attrs) { std::vector<const OcelAttribute*> out; for (const auto& a : attrs) out.push_back(&a); std::sort(out.begin(), out.end(), [](auto* a, auto* b) { return a->key < b->key; }); return out; }
using StandardXmlTypeTable =
    std::map<std::string, std::map<std::string, std::string>>;
void merge_standard_type(std::string& current, const OcelValue& value) {
  const auto next = value_tag(value);
  if (current.empty() || current == next) current = next;
  else if ((current == "int" && next == "float") ||
           (current == "float" && next == "int")) current = "float";
  else current = "string";
}
void collect_standard_types(const Ocel& log, StandardXmlTypeTable& object_types,
                            StandardXmlTypeTable& event_types) {
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
void write_standard_type_table(std::ostream& out, std::string_view container,
                               std::string_view element,
                               const StandardXmlTypeTable& types) {
  out << "  <" << container << ">\n";
  for (const auto& [type, attributes] : types) {
    out << "    <" << element << " name=\"" << esc(type) << "\">\n";
    out << "      <attributes>\n";
    for (const auto& [name, declared_type] : attributes)
      out << "        <attribute name=\"" << esc(name) << "\" type=\""
          << esc(declared_type.empty() ? "string" : declared_type)
          << "\"/>\n";
    out << "      </attributes>\n    </" << element << ">\n";
  }
  out << "  </" << container << ">\n";
}
void write_value(std::ostream& out, const OcelValue& v, std::string_view key, std::string_view indent, bool standard = false, std::string_view time = {}) { const auto tag = standard ? "attribute" : value_tag(v); out << indent << '<' << tag << " name=\"" << esc(key) << "\""; if (standard) out << " type=\"" << value_tag(v) << "\""; else out << " key=\"" << esc(key) << "\""; if (!time.empty()) out << " time=\"" << esc(time) << "\""; out << " value=\"" << esc(value_text(v)) << "\"/>\n"; }

}  // namespace

Ocel parse_ocel_xml(std::string_view xml, const OcelXmlImportOptions& options) {
  if (xml.size() > options.max_bytes) throw std::invalid_argument("OCEL XML exceeds size limit");
  const Node root = parse_tree(xml); Ocel log;
  const bool standard = child(root, "object-types") != nullptr || child(root, "event-types") != nullptr || attr(root, "ocelVersion") != nullptr;
  if (standard) parse_standard(root, log, options.reject_unsupported); else parse_classic(root, log, options.reject_unsupported);
  validate_references(log);
  return log;
}

Ocel import_ocel_xml(std::istream& input, const OcelXmlImportOptions& options) { std::ostringstream s; s << input.rdbuf(); return parse_ocel_xml(s.str(), options); }
Ocel import_ocel_xml(const std::filesystem::path& path, const OcelXmlImportOptions& options) { std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error("Unable to open OCEL XML: " + path.string()); return import_ocel_xml(f, options); }

void write_ocel_xml_legacy(std::ostream& out, const Ocel& log, const OcelXmlExportOptions& options) {
  if (!options.allow_lossy) {
    std::set<std::string> object_ids, event_ids;
    for (const auto& o : log.objects) object_ids.insert(o.id);
    for (const auto& e : log.events) event_ids.insert(e.id);
    for (const auto& r : log.relations) if (!event_ids.count(r.event_id) || !object_ids.count(r.object_id)) throw std::invalid_argument("OCEL XML relation references unknown id");
    for (const auto& r : log.o2o) if (!object_ids.count(r.source_id) || !object_ids.count(r.target_id)) throw std::invalid_argument("OCEL XML O2O relation references unknown id");
    for (const auto& r : log.e2e) if (!event_ids.count(r.source_id) || !event_ids.count(r.target_id)) throw std::invalid_argument("OCEL XML E2E relation references unknown id");
    for (const auto& c : log.object_changes) if (!object_ids.count(c.object_id)) throw std::invalid_argument("OCEL XML object change references unknown id");
  }
  if (options.include_declaration) out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  std::vector<const OcelObject*> objects; for (const auto& o : log.objects) objects.push_back(&o); std::sort(objects.begin(), objects.end(), [](auto* a, auto* b){ return a->id < b->id; });
  std::vector<const OcelEvent*> events; for (const auto& e : log.events) events.push_back(&e); std::sort(events.begin(), events.end(), [](auto* a, auto* b){ return a->id < b->id; });
  const bool standard = options.format == OcelXmlFormat::kStandard;
  out << "<log" << (standard ? " ocelVersion=\"2.0\"" : "") << ">\n";
  if (standard) {
    StandardXmlTypeTable object_types;
    StandardXmlTypeTable event_types;
    collect_standard_types(log, object_types, event_types);
    write_standard_type_table(out, "object-types", "object-type", object_types);
    write_standard_type_table(out, "event-types", "event-type", event_types);
    out << "  <objects>\n"; for (auto* o : objects) { out << "    <object id=\"" << esc(o->id) << "\" type=\"" << esc(o->type) << "\">\n      <attributes>\n"; for (auto* a : sorted_attrs(o->attributes)) write_value(out, a->value, a->key, "        ", true); for (const auto& c : log.object_changes) if (c.object_id == o->id) write_value(out, c.value, c.field, "        ", true, c.timestamp); out << "      </attributes>\n"; std::vector<const OcelO2oRelation*> rs; for (const auto& r : log.o2o) if (r.source_id == o->id) rs.push_back(&r); if (!rs.empty()) { out << "      <relationships>\n"; std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->target_id,a->qualifier) < std::tie(b->target_id,b->qualifier); }); for (auto* r : rs) out << "        <relationship object-id=\"" << esc(r->target_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "      </relationships>\n"; } out << "    </object>\n"; } out << "  </objects>\n  <events>\n";
    for (auto* e : events) { out << "    <event id=\"" << esc(e->id) << "\" type=\"" << esc(e->activity) << "\" time=\"" << esc(e->timestamp) << "\">\n      <attributes>\n"; for (auto* a : sorted_attrs(e->attributes)) write_value(out, a->value, a->key, "        ", true); out << "      </attributes>\n      <objects>\n"; std::vector<const OcelRelation*> rs; for (const auto& r : log.relations) if (r.event_id == e->id) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->object_id,a->qualifier) < std::tie(b->object_id,b->qualifier); }); for (auto* r : rs) out << "        <relationship object-id=\"" << esc(r->object_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "      </objects>\n    </event>\n"; } out << "  </events>\n";
    if (!log.e2e.empty()) { out << "  <e2e>\n"; std::vector<const OcelE2eRelation*> rs; for (const auto& r : log.e2e) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->source_id,a->target_id,a->qualifier) < std::tie(b->source_id,b->target_id,b->qualifier); }); for (auto* r : rs) out << "    <relationship source-event-id=\"" << esc(r->source_id) << "\" target-event-id=\"" << esc(r->target_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "  </e2e>\n"; }
    if (!log.globals.empty()) { out << "  <globals>\n"; for (const auto& [k,v] : log.globals) write_value(out,v,k,"    ",true); out << "  </globals>\n"; }
  } else {
    out << "  <objects>\n"; for (auto* o : objects) { out << "    <object id=\"" << esc(o->id) << "\" type=\"" << esc(o->type) << "\">\n"; for (auto* a : sorted_attrs(o->attributes)) write_value(out,a->value,a->key,"      "); out << "    </object>\n"; } out << "  </objects>\n  <events>\n"; for (auto* e : events) { out << "    <event id=\"" << esc(e->id) << "\" activity=\"" << esc(e->activity) << "\" timestamp=\"" << esc(e->timestamp) << "\">\n      <vmap>\n"; for (auto* a : sorted_attrs(e->attributes)) write_value(out,a->value,a->key,"        "); out << "      </vmap>\n      <omap>\n"; std::vector<const OcelRelation*> rs; for (const auto& r : log.relations) if (r.event_id == e->id) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a, auto* b){ return std::tie(a->object_id,a->qualifier) < std::tie(b->object_id,b->qualifier); }); for (auto* r : rs) out << "        <object object-id=\"" << esc(r->object_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "      </omap>\n    </event>\n"; } out << "  </events>\n";
    if (!log.o2o.empty()) { out << "  <o2o>\n"; std::vector<const OcelO2oRelation*> rs; for (const auto& r : log.o2o) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a,auto* b){ return std::tie(a->source_id,a->target_id,a->qualifier) < std::tie(b->source_id,b->target_id,b->qualifier); }); for (auto* r : rs) out << "    <relation source-id=\"" << esc(r->source_id) << "\" target-id=\"" << esc(r->target_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "  </o2o>\n"; }
    if (!log.e2e.empty()) { out << "  <e2e>\n"; std::vector<const OcelE2eRelation*> rs; for (const auto& r : log.e2e) rs.push_back(&r); std::sort(rs.begin(), rs.end(), [](auto* a,auto* b){ return std::tie(a->source_id,a->target_id,a->qualifier) < std::tie(b->source_id,b->target_id,b->qualifier); }); for (auto* r : rs) out << "    <relation source-id=\"" << esc(r->source_id) << "\" target-id=\"" << esc(r->target_id) << "\" qualifier=\"" << esc(r->qualifier) << "\"/>\n"; out << "  </e2e>\n"; }
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

void write_ocel_xml(std::ostream& out, const Ocel& log,
                    const OcelXmlExportOptions& options) {
  if (options.format != OcelXmlFormat::kStandard) {
    write_ocel_xml_legacy(out, log, options);
    return;
  }
  if (!options.allow_lossy) validate_references(log);
  if (options.include_declaration)
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

  std::vector<const OcelObject*> objects;
  for (const auto& object : log.objects) objects.push_back(&object);
  std::sort(objects.begin(), objects.end(),
            [](const auto* left, const auto* right) {
              return left->id < right->id;
            });
  std::vector<const OcelEvent*> events;
  for (const auto& event : log.events) events.push_back(&event);
  std::sort(events.begin(), events.end(),
            [](const auto* left, const auto* right) {
              return left->id < right->id;
            });

  StandardXmlTypeTable object_types;
  StandardXmlTypeTable event_types;
  collect_standard_types(log, object_types, event_types);
  out << "<log ocelVersion=\"2.0\">\n";
  write_standard_type_table(out, "object-types", "object-type", object_types);
  write_standard_type_table(out, "event-types", "event-type", event_types);

  out << "  <objects>\n";
  for (const auto* object : objects) {
    out << "    <object id=\"" << esc(object->id) << "\" type=\""
        << esc(object->type) << "\">\n      <attributes>\n";
    const auto static_attributes = sorted_attrs(object->attributes);
    std::set<std::string> static_fields;
    for (const auto* attribute : static_attributes) {
      static_fields.insert(attribute->key);
      write_value(out, attribute->value, attribute->key, "        ", true,
                  "1970-01-01T00:00:00Z");
    }
    std::vector<const OcelObjectChange*> changes;
    for (const auto& change : log.object_changes)
      if (change.object_id == object->id) changes.push_back(&change);
    std::sort(changes.begin(), changes.end(), [](const auto* left,
                                                 const auto* right) {
      return std::tie(left->timestamp, left->field, left->object_type) <
             std::tie(right->timestamp, right->field, right->object_type);
    });
    std::set<std::string> seeded_fields;
    for (const auto* change : changes) {
      if (!static_fields.count(change->field) &&
          seeded_fields.insert(change->field).second) {
        OcelValue missing = nullptr;
        write_value(out, missing, change->field, "        ", true,
                    "1970-01-01T00:00:00Z");
      }
      write_value(out, change->value, change->field, "        ", true,
                  change->timestamp);
    }
    out << "      </attributes>\n";
    std::vector<const OcelO2oRelation*> relations;
    for (const auto& relation : log.o2o)
      if (relation.source_id == object->id) relations.push_back(&relation);
    std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                    const auto* right) {
      return std::tie(left->target_id, left->qualifier) <
             std::tie(right->target_id, right->qualifier);
    });
    if (!relations.empty()) {
      out << "      <relationships>\n";
      for (const auto* relation : relations)
        out << "        <relationship object-id=\""
            << esc(relation->target_id) << "\" qualifier=\""
            << esc(relation->qualifier) << "\"/>\n";
      out << "      </relationships>\n";
    }
    out << "    </object>\n";
  }
  out << "  </objects>\n  <events>\n";
  for (const auto* event : events) {
    out << "    <event id=\"" << esc(event->id) << "\" type=\""
        << esc(event->activity) << "\" time=\"" << esc(event->timestamp)
        << "\">\n      <attributes>\n";
    for (const auto* attribute : sorted_attrs(event->attributes))
      write_value(out, attribute->value, attribute->key, "        ", true);
    out << "      </attributes>\n      <objects>\n";
    std::vector<const OcelRelation*> relations;
    for (const auto& relation : log.relations)
      if (relation.event_id == event->id) relations.push_back(&relation);
    std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                    const auto* right) {
      return std::tie(left->object_id, left->qualifier) <
             std::tie(right->object_id, right->qualifier);
    });
    for (const auto* relation : relations)
      out << "        <relationship object-id=\"" << esc(relation->object_id)
          << "\" qualifier=\"" << esc(relation->qualifier) << "\"/>\n";
    out << "      </objects>\n    </event>\n";
  }
  out << "  </events>\n";
  if (!log.e2e.empty()) {
    out << "  <e2e>\n";
    std::vector<const OcelE2eRelation*> relations;
    for (const auto& relation : log.e2e) relations.push_back(&relation);
    std::sort(relations.begin(), relations.end(), [](const auto* left,
                                                    const auto* right) {
      return std::tie(left->source_id, left->target_id, left->qualifier) <
             std::tie(right->source_id, right->target_id, right->qualifier);
    });
    for (const auto* relation : relations)
      out << "    <relationship source-event-id=\""
          << esc(relation->source_id) << "\" target-event-id=\""
          << esc(relation->target_id) << "\" qualifier=\""
          << esc(relation->qualifier) << "\"/>\n";
    out << "  </e2e>\n";
  }
  if (!log.globals.empty()) {
    out << "  <globals>\n";
    for (const auto& [key, value] : log.globals)
      write_value(out, value, key, "    ", true);
    out << "  </globals>\n";
  }
  out << "</log>\n";
}

std::string serialize_ocel_xml(const Ocel& log, const OcelXmlExportOptions& options) { std::ostringstream out; write_ocel_xml(out, log, options); return out.str(); }
void export_ocel_xml(const Ocel& log, const std::filesystem::path& path, const OcelXmlExportOptions& options) { std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("Unable to create OCEL XML: " + path.string()); write_ocel_xml(out, log, options); }

}  // namespace minera
