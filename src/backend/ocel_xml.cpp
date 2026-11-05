#include "minera/ocel_xml.hpp"

#include "ocel_frame_types.hpp"
#include "xml_pull_parser.hpp"

#include <algorithm>
#include <cctype>
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

// The reference XML readers keep an object id listed twice as two object
// rows (with their own types and attributes) while typing every relation to
// it by the last listing, and their statistics, OC-DFG and flattening then
// disagree on its type (observed black-box); a native OCEL holds one object
// per id, so such a file is refused (declared).
// The OCEL 1.0 XML event date: the UTC instant with a "Z", which the
// reference writer appends to a naive time too (a naive OCEL CSV
// timestamp is written "2024-01-01T00:00:00Z", observed black-box on
// pm4py.write_ocel_xml).
std::string classic_xml_timestamp_text(std::string_view text) {
  auto out = ocel_frame::utc_timestamp_text(text, 'T', "Z");
  const auto parsed = parse_timestamp_exact(text);
  if (parsed && !parsed->timezone_aware) out += 'Z';
  return out;
}

// An attribute named "ocel:..." (an OCEL CSV column such as "ocel:foo" or
// "ocel:eid.1"): pm4py.write_ocel2_xml drops it, and pm4py.write_ocel_xml
// drops it from objects and writes it on the event itself without the
// prefix, outside "vmap" (observed black-box).
bool reserved_name(std::string_view key) { return key.starts_with("ocel:"); }

// The offset every event time of `log` shares, when it is not zero.
std::optional<std::int32_t> event_time_offset(const Ocel& log) {
  std::vector<std::string_view> texts;
  texts.reserve(log.events.size());
  for (const auto& event : log.events) texts.emplace_back(event.timestamp);
  return ocel_frame::shared_nonzero_offset(texts);
}

[[noreturn]] void throw_repeated_xml_object(const std::string& id, const std::string& first,
                                            const std::string& last) {
  throw std::invalid_argument(
      "invalid or duplicate OCEL XML object: id " + id + " is listed twice (types " + first +
      " and " + last + "); pm4py keeps both object rows and types its relations by the last "
      "one, which a native OCEL cannot hold");
}

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
  // Coalesced character data keeps whitespace-only and CDATA text: OCEL 2.0
  // carries attribute values as element text, and the reference keeps a value
  // such as "  " as is. Only leaf value nodes read their text, so the
  // indentation this adds to container nodes is never used.
  detail::XmlPullParser parser(xml, true, true);
  Node root;
  std::vector<Node*> stack;
  while (true) {
    auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::ProcessingInstruction) continue;
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

// Python's float()/int() and the reference's date parser accept text with
// surrounding whitespace (a pretty-printed "<attribute>\n  4.5\n</attribute>"),
// so typed values are read without it; strings keep theirs.
std::string strip_xml_space(std::string value) {
  constexpr std::string_view space = " \t\n\r\v\f";
  const auto first = value.find_first_not_of(space);
  if (first == std::string::npos) return {};
  return value.substr(first, value.find_last_not_of(space) - first + 1);
}

OcelValue value_node(const Node& n, bool reject_lists) {
  const auto name = local(n.name);
  std::string value = attr_or(n, "value", n.text);
  if (name == "boolean" || name == "bool" || name == "int" || name == "integer" ||
      name == "long" || name == "float" || name == "double" || name == "number" ||
      name == "date" || name == "timestamp")
    value = strip_xml_space(std::move(value));
  if (name == "attribute") {
    const auto type = attr_or(n, "type", "string");
    if (type == "date" || type == "timestamp") return strip_xml_space(std::move(value));
    if (type == "string") return value;
    Node typed = n;
    typed.name = type;
    return value_node(typed, reject_lists);
  }
  if (name == "null") return nullptr;
  if (name == "boolean" || name == "bool") {
    if (value == "true" || value == "1" || value == "TRUE" || value == "True") return true;
    if (value == "false" || value == "0" || value == "FALSE" || value == "False") return false;
    throw std::invalid_argument("invalid OCEL XML boolean value");
  }
  if (name == "int" || name == "integer" || name == "long") {
    std::int64_t i = 0; const auto r = std::from_chars(value.data(), value.data() + value.size(), i);
    if (r.ec != std::errc() || r.ptr != value.data() + value.size()) throw std::invalid_argument("invalid OCEL XML integer value");
    return i;
  }
  if (name == "float" || name == "double" || name == "number") {
    char* end = nullptr; const double d = std::strtod(value.c_str(), &end);
    if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(d)) throw std::invalid_argument("invalid OCEL XML floating value");
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

// The keys of <date> (and <timestamp>) values append_values_filtered reads
// from `n`: pm4py.read_ocel_xml converts them, so they are catalogued as
// dates (the reference frame column is datetime64).
void collect_date_keys(const Node& n, OcelTypeSchema& schema,
                       const std::unordered_set<std::string>& excluded) {
  for (const auto& c : n.children) {
    const auto name = local(c.name);
    const auto key = attr_or(c, "key", attr_or(c, "name"));
    if (name == "omap" || ((name == "list" || name == "array") && key == "omap")) continue;
    if (name == "attributes" || name == "vmap" || name == "ovmap" ||
        ((name == "list" || name == "array") && (key == "vmap" || key == "ovmap"))) {
      collect_date_keys(c, schema, {});
      continue;
    }
    if ((name != "date" && name != "timestamp") || key.empty() || excluded.count(key)) continue;
    schema[key] = "date";
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
  if (attr(node, "type") != nullptr) return value_node(node, reject_lists);
  // Reference capture (pm4py 2.7.22 read_ocel2_xml): an attribute element
  // without text reads as the text "None" unless its type is a float (which
  // raises there and here). Native files name each value's type and keep an
  // empty string.
  if (attr(node, "value") == nullptr && node.text.empty() && node.children.empty() &&
      (declared_type == nullptr ||
       (*declared_type != "float" && *declared_type != "double" && *declared_type != "number")))
    return std::string("None");
  if (declared_type == nullptr) return value_node(node, reject_lists);
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
      if (id.empty() || type.empty()) throw std::invalid_argument("invalid or duplicate OCEL XML object");
      if (const auto [it, added] = types.emplace(id, type); !added) throw_repeated_xml_object(id, it->second, type);
      OcelObject o{id, type, {}};
      append_values_filtered(*on, o.attributes, reject_lists,
                             {"id", "object-id", "type", "object-type"});
      log.objects.push_back(std::move(o));
      collect_date_keys(*on, log.object_type_attributes[type],
                        {"id", "object-id", "type", "object-type"});
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
      collect_date_keys(*en, log.event_type_attributes[log.events.back().activity],
                        {"id", "event-id", "activity", "type", "timestamp", "time"});
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
      if (id.empty() || type.empty()) throw std::invalid_argument("invalid or duplicate OCEL XML object");
      if (const auto [it, added] = types.emplace(id, type); !added) throw_repeated_xml_object(id, it->second, type);
      OcelObject o{id, type, {}};
      if (const auto* a = child(*on, "attributes")) {
        // Grouped by name in first-appearance order, like the reference's
        // frame columns.
        std::vector<std::pair<std::string, std::vector<StandardAttribute>>> grouped;
        std::unordered_map<std::string, std::size_t> group_index;
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
          const auto [slot, added] = group_index.try_emplace(key, grouped.size());
          if (added) grouped.emplace_back(key, std::vector<StandardAttribute>{});
          grouped[slot->second].second.push_back(std::move(entry));
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

// Attribute values as lxml escapes them: markup characters and the
// whitespace an XML parser would otherwise normalize to spaces.
std::string esc(std::string_view s) {
  std::string o;
  o.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      case '\n': o += "&#10;"; break;
      case '\r': o += "&#13;"; break;
      case '\t': o += "&#9;"; break;
      default: o += c; break;
    }
  }
  return o;
}
// Element text as lxml escapes it.
std::string esc_text(std::string_view s) {
  std::string o;
  o.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '\r': o += "&#13;"; break;
      default: o += c; break;
    }
  }
  return o;
}
std::string value_text(const OcelValue& v) { if (std::holds_alternative<std::nullptr_t>(v)) return {}; if (const auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false"; if (const auto* i = std::get_if<std::int64_t>(&v)) return std::to_string(*i); if (const auto* d = std::get_if<double>(&v)) return ocel_frame::float_repr(*d); return std::get<std::string>(v); }
std::string value_tag(const OcelValue& v) { if (std::holds_alternative<std::nullptr_t>(v)) return "null"; if (std::holds_alternative<bool>(v)) return "boolean"; if (std::holds_alternative<std::int64_t>(v)) return "int"; if (std::holds_alternative<double>(v)) return "float"; return "string"; }

// One owner's attribute declarations in output order.
using StandardXmlDeclarations = std::vector<std::pair<std::string, std::string>>;
using StandardXmlTypeTable = std::map<std::string, StandardXmlDeclarations>;

// The reference writer declares what its frame column holds: "float" for a
// float64 column (an integer column with a missing cell is one), "date" for
// a timestamp column (here: a catalog date declaration), otherwise
// "string". Each type lists its attributes with a value in global column
// order (changed fields after the static ones); declared-only catalog entries
// follow with their declared type, so empty types and attributes survive.
void collect_standard_types(const Ocel& log, StandardXmlTypeTable& object_types,
                            StandardXmlTypeTable& event_types) {
  ocel_frame::Columns object_columns, change_columns, event_columns;
  for (const auto& object : log.objects) object_columns.observe_row(object.attributes);
  for (const auto& change : log.object_changes) {
    change_columns.add_rows(1);
    change_columns.observe(change.field, change.value);
  }
  for (const auto& event : log.events) event_columns.observe_row(event.attributes);
  const auto declare = [](const OcelTypeCatalog& catalog, const std::string& owner,
                          const std::string& key, ocel_frame::Dtype dtype) {
    if (const auto schema = catalog.find(owner); schema != catalog.end())
      if (const auto found = schema->second.find(key); found != schema->second.end()) {
        std::string lowered = found->second;
        for (auto& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lowered.find("date") != std::string::npos || lowered.find("time") != std::string::npos)
          return found->second;
      }
    return std::string(dtype == ocel_frame::Dtype::kFloat64 ? "float" : "string");
  };
  using Present = std::map<std::string, std::unordered_set<std::string>>;
  const auto build = [&](const OcelTypeCatalog& catalog, const Present& present,
                         const std::vector<const ocel_frame::Columns*>& order,
                         StandardXmlTypeTable& table) {
    for (const auto& [owner, keys] : present) {
      auto& declarations = table[owner];
      std::unordered_set<std::string> written;
      for (const auto* columns : order)
        for (const auto& name : columns->names()) {
          if (!keys.contains(name) || !written.insert(name).second) continue;
          auto dtype = columns->dtype(name);
          if (columns == &change_columns) dtype = change_columns.dtype(name);
          else if (dtype == ocel_frame::Dtype::kEmpty) dtype = change_columns.dtype(name);
          declarations.emplace_back(name, declare(catalog, owner, name, dtype));
        }
    }
    for (const auto& [owner, schema] : catalog) {
      auto& declarations = table[owner];
      for (const auto& [name, type] : schema) {
        const bool listed = std::any_of(declarations.begin(), declarations.end(),
                                        [&](const auto& entry) { return entry.first == name; });
        if (!listed) declarations.emplace_back(name, type.empty() ? "string" : type);
      }
    }
  };
  Present object_present, event_present;
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes)
      if (!std::holds_alternative<std::nullptr_t>(attribute.value))
        object_present[object.type].insert(attribute.key);
  for (const auto& change : log.object_changes)
    if (!std::holds_alternative<std::nullptr_t>(change.value))
      object_present[change.object_type].insert(change.field);
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes)
      if (!std::holds_alternative<std::nullptr_t>(attribute.value))
        event_present[event.activity].insert(attribute.key);
  build(log.object_type_attributes, object_present, {&object_columns, &change_columns}, object_types);
  build(log.event_type_attributes, event_present, {&event_columns}, event_types);
  for (auto* table : {&object_types, &event_types})
    for (auto& [owner, declarations] : *table)
      std::erase_if(declarations, [](const auto& entry) { return reserved_name(entry.first); });
  for (const auto& object : log.objects) object_types[object.type];
  for (const auto& event : log.events) event_types[event.activity];
}
void write_standard_type_table(std::ostream& out, std::string_view container,
                               std::string_view element,
                               const StandardXmlTypeTable& types) {
  if (types.empty()) {
    out << "  <" << container << "/>\n";
    return;
  }
  out << "  <" << container << ">\n";
  for (const auto& [type, attributes] : types) {
    out << "    <" << element << " name=\"" << esc(type) << "\">\n";
    if (attributes.empty()) {
      out << "      <attributes/>\n";
    } else {
      out << "      <attributes>\n";
      for (const auto& [name, declared_type] : attributes)
        out << "        <attribute name=\"" << esc(name) << "\" type=\""
            << esc(declared_type) << "\"/>\n";
      out << "      </attributes>\n";
    }
    out << "    </" << element << ">\n";
  }
  out << "  </" << container << ">\n";
}
// An OCEL 2.0 attribute: the value as element text, as the reference writes
// and reads it, with the value's own type (which the reference ignores in
// favour of the declaration) so a native reader keeps it. Booleans read as
// Python's str(True)/str(False). The reference leaves null values out; its
// reader calls float(None) on an empty float element.
// A value of an attribute the catalog declares a date (`date`) is written as
// the reference writes its datetime64 column: the UTC isoformat
// ("2024-02-01T08:30:00+00:00", fractions kept).
void write_standard_value(std::ostream& out, const OcelValue& v, std::string_view key,
                          std::string_view indent, std::string_view time = {},
                          bool date = false, bool drop_reserved = false) {
  if (std::holds_alternative<std::nullptr_t>(v)) return;
  if (drop_reserved && reserved_name(key)) return;
  out << indent << "<attribute name=\"" << esc(key) << "\"";
  if (!time.empty()) out << " time=\"" << esc(time) << "\"";
  if (const auto* text = date ? ocel_frame::date_value_text(v) : nullptr) {
    out << " type=\"date\">"
        << esc_text(ocel_frame::utc_timestamp_text(*text, 'T', "+00:00")) << "</attribute>\n";
    return;
  }
  out << " type=\"" << value_tag(v) << "\">";
  const auto* flag = std::get_if<bool>(&v);
  out << esc_text(flag ? (*flag ? "True" : "False") : value_text(v)) << "</attribute>\n";
}
// An OCEL 1.0 XML value as the reference writer emits it: floats as <float> with
// Python's repr (an integer in a float64 frame column is a float there),
// every other scalar as <string> with Python's str. Nulls are left out.
// A value of an attribute the catalog declares a date (`date`) is a <date>
// in UTC with a "Z", as the reference writes a datetime64 column.
void write_classic_value(std::ostream& out, const OcelValue& v, std::string_view key,
                         std::string_view indent, bool float_column = false,
                         bool date = false) {
  if (std::holds_alternative<std::nullptr_t>(v)) return;
  if (const auto* text = date ? ocel_frame::date_value_text(v) : nullptr) {
    out << indent << "<date key=\"" << esc(key) << "\" value=\""
        << esc(classic_xml_timestamp_text(*text)) << "\"/>\n";
    return;
  }
  const auto* integer = std::get_if<std::int64_t>(&v);
  if (const auto* real = std::get_if<double>(&v); real != nullptr || (float_column && integer)) {
    const double number = real != nullptr ? *real : static_cast<double>(*integer);
    if (!std::isfinite(number)) return;
    out << indent << "<float key=\"" << esc(key) << "\" value=\""
        << ocel_frame::float_repr(number) << "\"/>\n";
    return;
  }
  out << indent << "<string key=\"" << esc(key) << "\" value=\""
      << esc(ocel_frame::python_str(v)) << "\"/>\n";
}

}  // namespace

Ocel parse_ocel_xml(std::string_view xml, const OcelXmlImportOptions& options) {
  if (xml.size() > options.max_bytes) throw std::invalid_argument("OCEL XML file exceeds the " + ocel_limit_text(options.max_bytes) + " OCEL limit");
  const Node root = parse_tree(xml); Ocel log;
  const bool standard = child(root, "object-types") != nullptr || child(root, "event-types") != nullptr || attr(root, "ocelVersion") != nullptr;
  if (standard) parse_standard(root, log, options.reject_unsupported); else parse_classic(root, log, options.reject_unsupported);
  validate_references(log);
  // The OCEL 2.0 XML reader reads dates leniently, the classic one as ISO.
  sort_ocel_events_by_timestamp(log, standard ? OcelTimestampReader::kDateutil : OcelTimestampReader::kIsoFormat);
  // Both XML readers convert date values (declared in 2.0, <date> in 1.0).
  convert_ocel_date_attributes(log, standard ? OcelTimestampReader::kDateutil : OcelTimestampReader::kIsoFormat);
  // Both reference XML readers drop events and objects without relations.
  drop_unrelated_ocel_entries(log);
  return log;
}

Ocel import_ocel_xml(std::istream& input, const OcelXmlImportOptions& options) { std::ostringstream s; s << input.rdbuf(); return parse_ocel_xml(s.str(), options); }
Ocel import_ocel_xml(const std::filesystem::path& path, const OcelXmlImportOptions& options) { std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error("Unable to open OCEL XML: " + path.string()); return import_ocel_xml(f, options); }

namespace {

// Per-id indexes in log order, so the writers stay linear in the log size.
template <typename T, typename Key>
std::unordered_map<std::string_view, std::vector<const T*>> index_by(
    const std::vector<T>& rows, Key key) {
  std::unordered_map<std::string_view, std::vector<const T*>> result;
  for (const auto& row : rows) result[key(row)].push_back(&row);
  return result;
}
template <typename T>
const std::vector<const T*>& rows_of(
    const std::unordered_map<std::string_view, std::vector<const T*>>& index,
    std::string_view id) {
  static const std::vector<const T*> none;
  const auto found = index.find(id);
  return found == index.end() ? none : found->second;
}

// The OCEL 1.0 XML global log names the reference writer computes.
bool computed_classic_global(std::string_view key) {
  if (key.starts_with("ocel:global-event.") || key.starts_with("ocel:global-object."))
    return true;  // written back in their scopes
  static const std::set<std::string, std::less<>> computed = {
      "ocel:global-log.version", "ocel:global-log.ordering",
      "ocel:global-log.ocel:version", "ocel:global-log.ocel:ordering",
      "ocel:global-log.attribute-names", "ocel:global-log.object-types",
      "ocel:global-log.ocel:attribute-names", "ocel:global-log.ocel:object-types"};
  return computed.contains(key);
}

}  // namespace

// OCEL 1.0 XML as the reference writer emits it: the computed global scopes,
// then events and objects in log order with keyed <string>/<float>/<date>
// values. The format has no qualifiers, object-to-object or event-to-event
// links, object changes or other globals; without allow_lossy an OCEL holding
// any of them is rejected instead of losing them silently.
void write_ocel_xml_legacy(std::ostream& out, const Ocel& log, const OcelXmlExportOptions& options) {
  if (!options.allow_lossy) {
    validate_references(log);
    const bool qualified = std::any_of(log.relations.begin(), log.relations.end(),
                                       [](const auto& r) { return !r.qualifier.empty(); });
    const bool globals = std::any_of(log.globals.begin(), log.globals.end(),
                                     [](const auto& g) { return !computed_classic_global(g.first); });
    if (qualified || !log.o2o.empty() || !log.e2e.empty() || !log.object_changes.empty() || globals)
      throw std::invalid_argument(
          "OCEL 1.0 XML cannot represent relation qualifiers, O2O/E2E links, object "
          "changes or globals; export OCEL 2.0 XML or allow lossy export");
  }
  if (options.include_declaration) out << "<?xml version='1.0' encoding='UTF-8'?>\n";
  out << "<log>\n";
  // The event and object scopes hold the log's globals of those scopes (an
  // OCEL read from OCEL 2.0 has none); the log scope is computed.
  for (const std::string scope : {"event", "object"}) {
    const auto prefix = "ocel:global-" + scope + ".";
    std::ostringstream entries;
    for (const auto& [key, value] : log.globals)
      if (key.starts_with(prefix))
        write_classic_value(entries, value, std::string_view(key).substr(prefix.size()), "    ");
    const auto text = entries.str();
    if (text.empty()) out << "  <global scope=\"" << scope << "\"/>\n";
    else out << "  <global scope=\"" << scope << "\">\n" << text << "  </global>\n";
  }
  std::set<std::string> attribute_names;
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes)
      if (!reserved_name(attribute.key)) attribute_names.insert(attribute.key);
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes)
      if (!reserved_name(attribute.key)) attribute_names.insert(attribute.key);
  std::vector<std::string> object_types;
  std::unordered_set<std::string> seen_types;
  for (const auto& object : log.objects)
    if (seen_types.insert(object.type).second) object_types.push_back(object.type);
  out << "  <global scope=\"log\">\n";
  if (attribute_names.empty()) {
    out << "    <list key=\"attribute-names\"/>\n";
  } else {
    out << "    <list key=\"attribute-names\">\n";
    for (const auto& name : attribute_names)
      out << "      <string key=\"attribute-name\" value=\"" << esc(name) << "\"/>\n";
    out << "    </list>\n";
  }
  if (object_types.empty()) {
    out << "    <list key=\"object-types\"/>\n";
  } else {
    out << "    <list key=\"object-types\">\n";
    for (const auto& type : object_types)
      out << "      <string key=\"object-type\" value=\"" << esc(type) << "\"/>\n";
    out << "    </list>\n";
  }
  out << "    <string key=\"version\" value=\"1.0\"/>\n    <string key=\"ordering\" value=\"timestamp\"/>\n  </global>\n";
  const auto relations = index_by(log.relations, [](const OcelRelation& r) -> std::string_view { return r.event_id; });
  ocel_frame::Columns event_frame, object_frame;
  for (const auto& event : log.events) event_frame.observe_row(event.attributes);
  for (const auto& object : log.objects) object_frame.observe_row(object.attributes);
  const auto write_list = [&](std::string_view key, const auto& body, bool empty) {
    if (empty) {
      out << "      <list key=\"" << key << "\"/>\n";
      return;
    }
    out << "      <list key=\"" << key << "\">\n";
    body();
    out << "      </list>\n";
  };
  const auto has_value = [](const std::vector<OcelAttribute>& attributes) {
    return std::any_of(attributes.begin(), attributes.end(), [](const auto& a) {
      if (std::holds_alternative<std::nullptr_t>(a.value) || reserved_name(a.key)) return false;
      const auto* real = std::get_if<double>(&a.value);
      return real == nullptr || std::isfinite(*real);
    });
  };
  // A CSV log whose times share an offset is written in that wall clock,
  // labelled "Z" as the reference does (see shared_nonzero_offset).
  const auto event_offset = event_time_offset(log);
  if (log.events.empty()) {
    out << "  <events/>\n";
  } else {
    out << "  <events>\n";
    for (const auto& event : log.events) {
      // Fields in the column order of a frame read from OCEL JSON.
      out << "    <event>\n      <string key=\"id\" value=\"" << esc(event.id)
          << "\"/>\n      <date key=\"timestamp\" value=\""
          << esc(event_offset ? ocel_frame::offset_timestamp_text(event.timestamp, 'T',
                                                                  event_offset, "Z")
                              : classic_xml_timestamp_text(event.timestamp))
          << "\"/>\n      <string key=\"activity\" value=\"" << esc(event.activity) << "\"/>\n";
      for (const auto& attribute : event.attributes)
        if (reserved_name(attribute.key))
          write_classic_value(out, attribute.value, std::string_view(attribute.key).substr(5),
                              "      ",
                              event_frame.dtype(attribute.key) == ocel_frame::Dtype::kFloat64);
      const auto& rows = rows_of(relations, event.id);
      std::unordered_set<std::string_view> listed;
      write_list("omap", [&] {
        for (const auto* relation : rows)
          if (listed.insert(relation->object_id).second)
            out << "        <string key=\"object-id\" value=\"" << esc(relation->object_id) << "\"/>\n";
      }, rows.empty());
      write_list("vmap", [&] {
        for (const auto& attribute : event.attributes)
          if (!reserved_name(attribute.key))
            write_classic_value(out, attribute.value, attribute.key, "        ",
                                event_frame.dtype(attribute.key) == ocel_frame::Dtype::kFloat64,
                                ocel_frame::declared_date(log.event_type_attributes,
                                                          event.activity, attribute.key));
      }, !has_value(event.attributes));
      out << "    </event>\n";
    }
    out << "  </events>\n";
  }
  if (log.objects.empty()) {
    out << "  <objects/>\n";
  } else {
    out << "  <objects>\n";
    for (const auto& object : log.objects) {
      out << "    <object>\n      <string key=\"id\" value=\"" << esc(object.id)
          << "\"/>\n      <string key=\"type\" value=\"" << esc(object.type) << "\"/>\n";
      write_list("ovmap", [&] {
        for (const auto& attribute : object.attributes)
          if (!reserved_name(attribute.key))
            write_classic_value(out, attribute.value, attribute.key, "        ",
                                object_frame.dtype(attribute.key) == ocel_frame::Dtype::kFloat64,
                                ocel_frame::declared_date(log.object_type_attributes,
                                                          object.type, attribute.key));
      }, !has_value(object.attributes));
      out << "    </object>\n";
    }
    out << "  </objects>\n";
  }
  out << "</log>\n";
}

// OCEL 2.0 XML as the reference writer emits it (object-to-object links in an
// <objects> child of each object), with events, objects, attributes and
// relations in log order. Event-to-event links and globals, which the
// reference format lacks, follow in their own sections.
void write_ocel_xml(std::ostream& out, const Ocel& log,
                    const OcelXmlExportOptions& options) {
  if (options.format != OcelXmlFormat::kStandard) {
    write_ocel_xml_legacy(out, log, options);
    return;
  }
  if (!options.allow_lossy) validate_references(log);
  if (options.include_declaration)
    out << "<?xml version='1.0' encoding='UTF-8'?>\n";

  StandardXmlTypeTable object_types;
  StandardXmlTypeTable event_types;
  collect_standard_types(log, object_types, event_types);
  out << "<log>\n";
  write_standard_type_table(out, "object-types", "object-type", object_types);
  write_standard_type_table(out, "event-types", "event-type", event_types);

  const auto changes = index_by(log.object_changes, [](const OcelObjectChange& c) -> std::string_view { return c.object_id; });
  const auto o2o = index_by(log.o2o, [](const OcelO2oRelation& r) -> std::string_view { return r.source_id; });
  const auto relations = index_by(log.relations, [](const OcelRelation& r) -> std::string_view { return r.event_id; });
  out << (log.objects.empty() ? "  <objects/>\n" : "  <objects>\n");
  for (const auto& object : log.objects) {
    out << "    <object id=\"" << esc(object.id) << "\" type=\""
        << esc(object.type) << "\">\n";
    std::ostringstream attributes;
    const auto object_date = [&](const std::string& key) {
      return ocel_frame::declared_date(log.object_type_attributes, object.type, key);
    };
    for (const auto& attribute : object.attributes)
      write_standard_value(attributes, attribute.value, attribute.key, "        ",
                           "1970-01-01T00:00:00Z", object_date(attribute.key), true);
    std::vector<const OcelObjectChange*> object_changes = rows_of(changes, object.id);
    std::stable_sort(object_changes.begin(), object_changes.end(),
                     [](const auto* left, const auto* right) {
                       return std::tie(left->timestamp, left->field) <
                              std::tie(right->timestamp, right->field);
                     });
    // Like the reference, a field without a base value gets no null base
    // entry: its first change reads back as the base, as PM4Py reads its own
    // export.
    for (const auto* change : object_changes)
      write_standard_value(attributes, change->value, change->field, "        ",
                           ocel_frame::utc_timestamp_text(change->timestamp, 'T', "+00:00"),
                           object_date(change->field), true);
    const auto text = attributes.str();
    if (text.empty()) out << "      <attributes/>\n";
    else out << "      <attributes>\n" << text << "      </attributes>\n";
    const auto& links = rows_of(o2o, object.id);
    if (!links.empty()) {
      out << "      <objects>\n";
      for (const auto* relation : links)
        out << "        <relationship object-id=\"" << esc(relation->target_id)
            << "\" qualifier=\"" << esc(relation->qualifier) << "\"/>\n";
      out << "      </objects>\n";
    }
    out << "    </object>\n";
  }
  if (!log.objects.empty()) out << "  </objects>\n";
  // A CSV log whose times share an offset is written in that offset, as
  // the reference does (see shared_nonzero_offset).
  const auto event_offset = event_time_offset(log);
  out << (log.events.empty() ? "  <events/>\n" : "  <events>\n");
  for (const auto& event : log.events) {
    out << "    <event id=\"" << esc(event.id) << "\" type=\""
        << esc(event.activity) << "\" time=\""
        << esc(ocel_frame::offset_timestamp_text(event.timestamp, 'T', event_offset,
                                                 event_offset ? std::nullopt
                                                              : std::optional<std::string_view>("+00:00")))
        << "\">\n";
    std::ostringstream attributes;
    for (const auto& attribute : event.attributes)
      write_standard_value(attributes, attribute.value, attribute.key, "        ", {},
                           ocel_frame::declared_date(log.event_type_attributes,
                                                     event.activity, attribute.key),
                           true);
    const auto text = attributes.str();
    if (text.empty()) out << "      <attributes/>\n";
    else out << "      <attributes>\n" << text << "      </attributes>\n";
    const auto& rows = rows_of(relations, event.id);
    if (rows.empty()) {
      out << "      <objects/>\n";
    } else {
      out << "      <objects>\n";
      for (const auto* relation : rows)
        out << "        <relationship object-id=\"" << esc(relation->object_id)
            << "\" qualifier=\"" << esc(relation->qualifier) << "\"/>\n";
      out << "      </objects>\n";
    }
    out << "    </event>\n";
  }
  if (!log.events.empty()) out << "  </events>\n";
  if (!log.e2e.empty()) {
    out << "  <e2e>\n";
    for (const auto& relation : log.e2e)
      out << "    <relationship source-event-id=\""
          << esc(relation.source_id) << "\" target-event-id=\""
          << esc(relation.target_id) << "\" qualifier=\""
          << esc(relation.qualifier) << "\"/>\n";
    out << "  </e2e>\n";
  }
  if (!log.globals.empty()) {
    out << "  <globals>\n";
    for (const auto& [key, value] : log.globals)
      write_standard_value(out, value, key, "    ");
    out << "  </globals>\n";
  }
  out << "</log>\n";
}

std::string serialize_ocel_xml(const Ocel& log, const OcelXmlExportOptions& options) { std::ostringstream out; write_ocel_xml(out, log, options); return out.str(); }
void export_ocel_xml(const Ocel& log, const std::filesystem::path& path, const OcelXmlExportOptions& options) { std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("Unable to create OCEL XML: " + path.string()); write_ocel_xml(out, log, options); }

}  // namespace minera
