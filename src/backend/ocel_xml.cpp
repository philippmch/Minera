#include "minera/ocel_xml.hpp"

#include "ocel_frame_types.hpp"
#include "ocel_xml_times.hpp"
#include "python_float_text.hpp"
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
#include <utility>

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
    // A date keeps its text as written: read_ocel2_xml hands it to
    // fromisoformat, which refuses surrounding white space, and then to
    // dateutil (" 2024-02-01" is a naive midnight there, not the ISO date
    // stamped UTC); convert_ocel_date_attributes does the same.
    if (type == "date" || type == "timestamp" || type == "string") return value;
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

// `dated` (when given) receives the index in `out` of each value read from a
// <date> or <timestamp> element.
void append_values_filtered(const Node& n, std::vector<OcelAttribute>& out,
                            bool reject_lists,
                            const std::unordered_set<std::string>& excluded,
                            std::vector<std::size_t>* dated = nullptr,
                            bool lists_only = false) {
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
                             std::unordered_set<std::string>{}, dated);
      continue;
    }
    if (!is_value_node_name(name) || lists_only) continue;
    if (key.empty() || excluded.count(key)) continue;
    if (dated != nullptr && (name == "date" || name == "timestamp")) dated->push_back(out.size());
    out.push_back({key, value_node(c, reject_lists)});
  }
}

// The keys of <date> (and <timestamp>) values append_values_filtered reads
// from `n`: pm4py.read_ocel_xml converts them, so they are catalogued as
// dates (the reference frame column is datetime64).
void collect_date_keys(const Node& n, OcelTypeSchema& schema,
                       const std::unordered_set<std::string>& excluded,
                       bool lists_only = false) {
  for (const auto& c : n.children) {
    const auto name = local(c.name);
    const auto key = attr_or(c, "key", attr_or(c, "name"));
    if (name == "omap" || ((name == "list" || name == "array") && key == "omap")) continue;
    if (name == "attributes" || name == "vmap" || name == "ovmap" ||
        ((name == "list" || name == "array") && (key == "vmap" || key == "ovmap"))) {
      collect_date_keys(c, schema, {});
      continue;
    }
    if (lists_only || (name != "date" && name != "timestamp") || key.empty() || excluded.count(key)) continue;
    schema[key] = "date";
  }
}

void append_classic_relations(const Node& omap, std::string_view event_id,
                              const std::unordered_map<std::string, std::string>& types,
                              Ocel& log) {
  // read_ocel_xml relates an event to an object once, where its omap first
  // lists it (observed black-box); the former dialect's relationship
  // elements, which carry qualifiers, keep every listing.
  std::unordered_set<std::string> listed;
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
    // An empty id is an id: a relation to a listed object with an empty id
    // goes with that object, one to no listed object raises.
    if (!types.count(oid)) throw std::invalid_argument("OCEL XML relation references unknown object");
    if (is_value_node_name(name) && key == "object-id" && !listed.insert(oid).second) continue;
    log.relations.push_back({std::string(event_id), oid, attr_or(rel, "qualifier")});
  }
}

// lxml's element.get: the attribute named exactly `key` (a prefixed one is
// another attribute).
const std::string* exact_attr(const Node& n, std::string_view key) {
  for (const auto& [k, v] : n.attrs) if (k == key) return &v;
  return nullptr;
}

// read_ocel_xml reads a reference-shaped event's relations from every child
// keyed "omap" (whatever its tag): each element in it names an object by its
// value attribute, whatever its tag or key; one without a value, or naming no
// listed object, raises. An event relates to an object once, where it is
// first listed, with the qualifier attribute of its last listing ("" when
// that one has none).
// Observed black-box, round EC.
void append_reference_omap(const Node& event, std::string_view event_id,
                           const std::unordered_map<std::string, std::string>& types, Ocel& log) {
  std::unordered_map<std::string, std::size_t> listed;
  for (const auto& omap : event.children) {
    if (const auto* key = exact_attr(omap, "key"); key == nullptr || *key != "omap") continue;
    for (const auto& rel : omap.children) {
      const auto* oid = exact_attr(rel, "value");
      if (oid == nullptr || !types.count(*oid))
        throw std::invalid_argument("OCEL XML relation references unknown object");
      const auto* qualifier = exact_attr(rel, "qualifier");
      std::string text = qualifier ? *qualifier : std::string{};
      const auto [at, fresh] = listed.emplace(*oid, log.relations.size());
      if (fresh)
        log.relations.push_back({std::string(event_id), *oid, std::move(text)});
      else
        log.relations[at->second].qualifier = std::move(text);
    }
  }
}

// How read_ocel_xml reads a keyed value of a reference-shaped event or object
// (its time, a vmap or ovmap entry), by the element's tag: one naming "float"
// in any case is float(value), else one naming "date" is fromisoformat(value)
// stamped UTC, else the value attribute's text, "None" without one (an <int>
// or <boolean> stays a text, so does a nested list). Observed black-box,
// round ED.
enum class ReferenceValueKind { kText, kFloat, kDate };
ReferenceValueKind reference_value_kind(const Node& n) {
  std::string tag = local(n.name);
  for (auto& ch : tag) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (tag.find("float") != std::string::npos) return ReferenceValueKind::kFloat;
  if (tag.find("date") != std::string::npos) return ReferenceValueKind::kDate;
  return ReferenceValueKind::kText;
}
double reference_float(const Node& n) {
  const auto* value = exact_attr(n, "value");
  if (value == nullptr)
    throw std::invalid_argument("float() argument must be a string or a real number, not 'NoneType'");
  if (const auto parsed = detail::parse_python_float_text(*value)) return *parsed;
  throw std::invalid_argument("could not convert string to float: " + ocel_frame::python_str_repr(*value));
}
const std::string& reference_date_text(const Node& n) {
  const auto* value = exact_attr(n, "value");
  if (value == nullptr) throw std::invalid_argument("'NoneType' object has no attribute 'endswith'");
  return *value;
}
std::string reference_text(const Node& n) {
  const auto* value = exact_attr(n, "value");
  return value != nullptr ? *value : std::string("None");
}
OcelValue reference_value(const Node& n, bool change = false) {
  switch (reference_value_kind(n)) {
    case ReferenceValueKind::kFloat:
      return reference_float(n);
    case ReferenceValueKind::kDate: {
      OcelValue value = reference_date_text(n);
      convert_ocel_date_value(value, OcelTimestampReader::kIsoFormat, change);
      return value;
    }
    case ReferenceValueKind::kText:
      break;
  }
  return reference_text(n);
}

// An id, activity or type child's value as read_ocel_xml takes it: the value
// attribute as written, whatever the tag (a <float key="id" value="7"/> is
// "7"); none is None, which drops the event or object (native: empty).
std::string reference_name(const Node& n) {
  const auto* value = exact_attr(n, "value");
  return value != nullptr ? *value : std::string{};
}

bool reference_shaped_event(const Node& n) { return attr_or(n, "id", attr_or(n, "event-id")).empty(); }
bool reference_shaped_object(const Node& n) { return attr_or(n, "id", attr_or(n, "object-id")).empty(); }

// An object's ovmap entry whose key the object already holds (ocel:oid and
// ocel:type from the start, an earlier entry's key, a second keyless entry)
// is an object change at its `timestamp` attribute, which read_ocel_xml reads
// as it reads a date (none raises AttributeError). Observed black-box,
// round ED.
std::string reference_change_time(const Node& entry) {
  const auto* time = exact_attr(entry, "timestamp");
  if (time == nullptr) throw std::invalid_argument("'NoneType' object has no attribute 'endswith'");
  OcelValue checked = *time;
  convert_ocel_date_value(checked, OcelTimestampReader::kIsoFormat, true);
  return *time;
}

// read_ocel_xml reads each keyed value as it walks the file, so the first one
// float() or fromisoformat refuses raises, in document order (an event's time
// and vmap, an object's ovmap with each change's time after its value).
// Relations are resolved after the walk. Observed black-box, round ED.
void check_reference_values(const Node& root) {
  for (const auto& container : root.children) {
    const auto name = local(container.name);
    const bool events = name == "events";
    if (!events && name != "objects") continue;
    for (const auto& element : container.children) {
      if (events ? !reference_shaped_event(element) : !reference_shaped_object(element)) continue;
      std::vector<const Node*> entries;
      for (const auto& c : element.children) {
        const auto* key = exact_attr(c, "key");
        if (key == nullptr) continue;
        if (events && *key == "timestamp") {
          (void)reference_value(c);
        } else if (*key == (events ? "vmap" : "ovmap")) {
          for (const auto& entry : c.children) {
            (void)reference_value(entry, !events);
            entries.push_back(&entry);
          }
        }
      }
      // An object's change times are read after all of its values.
      if (events) continue;
      std::unordered_set<std::string> held{"ocel:oid", "ocel:type"};
      bool keyless = false;
      for (const auto* entry : entries) {
        const auto* entry_key = exact_attr(*entry, "key");
        const bool repeated = entry_key == nullptr ? std::exchange(keyless, true)
                                                   : !held.insert(*entry_key).second;
        if (repeated) (void)reference_change_time(*entry);
      }
    }
  }
}

// The vmap or ovmap entries of a reference-shaped event or object: every
// child's elements, each under its key whatever its tag, the last value of a
// repeated key at its first position. A keyless entry (a NaN column, which
// the reference's writers and attribute listings then fail on) is dropped
// (declared).
struct ReferenceAttributes {
  std::vector<OcelAttribute> values;
  std::unordered_map<std::string, std::size_t> at;
  std::unordered_set<std::string> dated;
  void read(const Node& list, bool reject_lists) {
    for (const auto& entry : list.children) {
      const auto* key = exact_attr(entry, "key");
      const auto entry_name = local(entry.name);
      if (reject_lists && (entry_name == "list" || entry_name == "array"))
        throw std::invalid_argument("OCEL XML list values are unsupported");
      if (key == nullptr) continue;
      auto value = reference_value(entry);
      if (reference_value_kind(entry) == ReferenceValueKind::kDate) dated.insert(*key);
      else dated.erase(*key);
      const auto [found, fresh] = at.emplace(*key, values.size());
      if (fresh) values.push_back({*key, std::move(value)});
      else values[found->second].value = std::move(value);
    }
  }
};

// read_ocel_xml's reading of an object whose id is a keyed child: children
// count by their key attribute whatever their tag ("id", "type" and every
// "ovmap"; "object-id", "object-type" or a name= attribute are no keys), the
// last of a repeated "id" or "type" wins. An ovmap entry keeps its first
// value; a later one under the same key is an object change at its
// timestamp attribute (see reference_change_time), in file order. A change
// of "ocel:oid" or "ocel:type" goes into the change frame's own column (an
// "ocel:oid" change's row names the object its value names), so the row
// survives the reference's filtering only when that object does: such a
// change is set aside in `held` (the object its row names) and refused after
// the filtering when it survives (declared). Keyless entries are dropped
// (declared, as for events). Observed black-box, round ED.
struct HeldColumnChange {
  std::string object_id, key;
};

void read_reference_object(const Node& on, Ocel& log, bool reject_lists,
                           std::unordered_map<std::string, std::string>& types,
                           std::vector<HeldColumnChange>& held_changes) {
  bool has_id = false;
  std::string id, type;
  std::vector<OcelAttribute> attributes;
  std::unordered_set<std::string> held{"ocel:oid", "ocel:type"};
  std::unordered_set<std::string> dated;
  struct Change { std::string time, field; OcelValue value; };
  std::vector<Change> changes;
  std::vector<std::size_t> pending_type_changes;
  for (const auto& c : on.children) {
    const auto* key = exact_attr(c, "key");
    if (key == nullptr) continue;
    if (*key == "id") {
      id = reference_name(c);
      has_id = exact_attr(c, "value") != nullptr;
    } else if (*key == "type") {
      type = reference_name(c);
    } else if (*key == "ovmap") {
      for (const auto& entry : c.children) {
        const auto entry_name = local(entry.name);
        if (reject_lists && (entry_name == "list" || entry_name == "array"))
          throw std::invalid_argument("OCEL XML list values are unsupported");
        const auto* entry_key = exact_attr(entry, "key");
        if (entry_key == nullptr) continue;
        if (held.insert(*entry_key).second) {
          if (reference_value_kind(entry) == ReferenceValueKind::kDate) dated.insert(*entry_key);
          attributes.push_back({*entry_key, reference_value(entry)});
          continue;
        }
        if (*entry_key == "ocel:oid" || *entry_key == "ocel:type") {
          (void)reference_change_time(entry);
          auto value = reference_value(entry, true);
          // The row names the object it changes, or for "ocel:oid" the
          // object its value names (none for a float or a date).
          if (*entry_key == "ocel:type") {
            held_changes.push_back({std::string{}, *entry_key});
            pending_type_changes.push_back(held_changes.size() - 1);
          } else if (const auto* text = std::get_if<std::string>(&value)) {
            held_changes.push_back({*text, *entry_key});
          }
          continue;
        }
        changes.push_back({reference_change_time(entry), *entry_key, reference_value(entry, true)});
      }
    }
  }
  // An object without an id is dropped and names no relation target (the
  // reference's None is no "None" and no ""); an empty id or type is
  // dropped too, after relations to it are read.
  if (has_id) {
    if (const auto [it, added] = types.emplace(id, type); !added && !id.empty())
      throw_repeated_xml_object(id, it->second, type);
  }
  auto& schema = log.object_type_attributes[type];
  for (const auto& key : dated) schema[key] = "date";
  if (!has_id) id.clear();
  for (const auto index : pending_type_changes) held_changes[index].object_id = id;
  for (auto& change : changes)
    log.object_changes.push_back({id, type, std::move(change.time), std::move(change.field), std::move(change.value)});
  log.objects.push_back({std::move(id), std::move(type), std::move(attributes)});
}

// A vmap entry keyed "ocel:eid", "ocel:activity" or "ocel:timestamp" sets
// that events-frame cell while the relations frame keeps the event's own
// value (observed black-box). Round EG: an entry equal to the event's own
// value changes nothing (a date the same instant); an empty or missing id or
// activity, or an id that is no str or that no relation names, drops the row
// from the events frame, so its relations go too while the objects they name
// stay, as in classic JSON's filtering (round EE). Any other such entry
// whose row remains (an activity or time a native OCEL cannot hold beside
// the relations frame's, an id naming another event's rows), and a time
// that is no date even on a dropped row (it changes the frame's time column),
// is refused after the filtering (declared).
struct XmlFrameOverride {
  std::string event_id;                 // the event's own id
  std::optional<std::string> frame_id;  // a str id other than the own one
  bool drop = false;                    // the events-frame row is dropped
  std::string held;                     // what a native OCEL cannot hold if the row remains
  bool column = false;                  // refused whenever an event remains
};

// read_ocel_xml's reading of an event whose id is a keyed child: "id",
// "activity", "timestamp", every "omap" and every "vmap" by key whatever the
// tag ("type", "time" or "event-id" are no keys), the last of a repeated key
// winning. The time is a typed value (see sort_ocel_xml_events); none is a
// missing time. A vmap entry keyed "ocel:eid", "ocel:activity" or
// "ocel:timestamp" overrides that events-frame column while the relations
// frame keeps the event's own (observed black-box), which a native OCEL
// cannot hold: such a file is refused (declared).
void read_reference_event(const Node& en, Ocel& log, bool reject_lists,
                          const std::unordered_map<std::string, std::string>& types,
                          std::vector<OcelXmlTime>& times,
                          std::vector<XmlFrameOverride>& overrides) {
  std::string id, activity;
  const Node* time = nullptr;
  ReferenceAttributes attributes;
  for (const auto& c : en.children) {
    const auto* key = exact_attr(c, "key");
    if (key == nullptr) continue;
    if (*key == "id") id = reference_name(c);
    else if (*key == "activity") activity = reference_name(c);
    else if (*key == "timestamp") time = &c;
    else if (*key == "vmap") attributes.read(c, reject_lists);
  }
  XmlFrameOverride frame;
  bool overridden = false;
  const auto hold = [&](const std::string& key) {
    if (frame.held.empty())
      frame.held = "OCEL XML event vmap entry " + key +
                   ": pm4py puts it in the events frame and keeps the event's own value in the "
                   "relations frame, which a native OCEL cannot hold";
  };
  for (const auto& attribute : attributes.values) {
    const auto* text = std::get_if<std::string>(&attribute.value);
    const auto* real = std::get_if<double>(&attribute.value);
    if (attribute.key == "ocel:eid") {
      overridden = true;
      if (text == nullptr || text->empty()) frame.drop = true;
      else if (*text != id) frame.frame_id = *text;
    } else if (attribute.key == "ocel:activity") {
      overridden = true;
      if ((text != nullptr && text->empty()) || (real != nullptr && std::isnan(*real))) frame.drop = true;
      else if (text == nullptr || *text != activity) hold(attribute.key);
    } else if (attribute.key == "ocel:timestamp") {
      overridden = true;
      bool same = false;
      if (attributes.dated.count(attribute.key) && text != nullptr && time != nullptr &&
          reference_value_kind(*time) == ReferenceValueKind::kDate)
        same = reference_value(*time) == attribute.value;
      if (!attributes.dated.count(attribute.key)) {
        frame.column = true;
        hold(attribute.key);
      } else if (!same) {
        hold(attribute.key);
      }
    }
  }
  std::erase_if(attributes.values, [](const OcelAttribute& attribute) {
    return attribute.key == "ocel:eid" || attribute.key == "ocel:activity" || attribute.key == "ocel:timestamp";
  });
  for (const auto* key : {"ocel:eid", "ocel:activity", "ocel:timestamp"}) attributes.dated.erase(key);
  if (overridden) {
    frame.event_id = id;
    overrides.push_back(std::move(frame));
  }
  OcelXmlTime typed;
  std::string text;
  if (time == nullptr) {
    typed.kind = OcelXmlTime::Kind::kMissing;
  } else {
    switch (reference_value_kind(*time)) {
      case ReferenceValueKind::kFloat:
        typed.kind = OcelXmlTime::Kind::kFloat;
        typed.real = reference_float(*time);
        break;
      case ReferenceValueKind::kDate:
        typed.kind = OcelXmlTime::Kind::kDate;
        text = reference_date_text(*time);
        break;
      case ReferenceValueKind::kText:
        typed.kind = OcelXmlTime::Kind::kText;
        text = reference_text(*time);
        break;
    }
  }
  auto& schema = log.event_type_attributes[activity];
  for (const auto& key : attributes.dated) schema[key] = "date";
  log.events.push_back({id, activity, std::move(text), std::move(attributes.values)});
  times.push_back(typed);
  append_reference_omap(en, log.events.back().id, types, log);
}

using StandardXmlAttributeTypes =
    std::map<std::string, std::map<std::string, std::string>>;

// Declared attribute names whose entry has no type attribute: read_ocel2_xml
// keeps None as their type and raises "argument of type 'NoneType' is not
// iterable" when it reads a value under one (round EJ, observed black-box).
using StandardXmlUntypedNames = std::map<std::string, std::set<std::string>>;

// One <object-types> or <event-types> section, read where the walk meets it
// (round EJ: read_ocel2_xml reads the file's sections in document order, so
// values listed before the table are read without declared types).
void parse_standard_xml_type_table(
    const Node& container, std::string_view type_name, StandardXmlAttributeTypes& result,
    StandardXmlUntypedNames& untyped) {
  for (const auto* descriptor : children(container, type_name)) {
    // read_ocel2_xml takes a type listed twice from its last listing and
    // an attribute listed twice from its last entry; an empty type name is
    // no error (its events or objects are dropped), observed black-box.
    // Values under an empty attribute name are skipped, so is its entry.
    const auto name = attr_or(*descriptor, "name");
    std::map<std::string, std::string> attributes;
    std::set<std::string> untyped_names;
    if (const auto* attribute_container = child(*descriptor, "attributes")) {
      for (const auto* attribute : children(*attribute_container, "attribute")) {
        // Round EJ: the name attribute alone (an empty one is a name; none
        // declares None, which only a keyless value could use).
        const auto* named = attr(*attribute, "name");
        if (named == nullptr) continue;
        const auto& attribute_name = *named;
        const auto* declared = attr(*attribute, "type");
        if (declared == nullptr) {
          untyped_names.insert(attribute_name);
          attributes.insert_or_assign(attribute_name, "string");
          continue;
        }
        untyped_names.erase(attribute_name);
        if (declared->empty()) attributes.erase(attribute_name);
        else attributes.insert_or_assign(attribute_name, *declared);
      }
    }
    result.insert_or_assign(name, std::move(attributes));
    untyped.insert_or_assign(name, std::move(untyped_names));
  }
}

// read_ocel2_xml's reading of a value element's text (round EJ, observed
// black-box): a declared type naming "date" is dateutil's (the text stays,
// convert_ocel_date_attributes converts it), one naming "float" is float()'s,
// anything else (integer, boolean, double, ...) and an undeclared name keep
// the text as written; no text is None ("None" as a text, an error for a
// date or float). Native files name each value's type and keep their own
// reading.
OcelValue standard_xml_value(const Node& node, const std::string* declared_type,
                             bool reject_lists, bool untyped = false) {
  if (attr(node, "type") != nullptr) return value_node(node, reject_lists);
  if (attr(node, "value") != nullptr) {
    if (declared_type == nullptr) return value_node(node, reject_lists);
    Node typed = node;
    typed.attrs.emplace_back("type", *declared_type);
    return value_node(typed, reject_lists);
  }
  if (untyped) throw std::invalid_argument("argument of type 'NoneType' is not iterable");
  const bool none = node.text.empty() && node.children.empty();
  if (declared_type != nullptr && declared_type->find("date") != std::string::npos) {
    if (none) throw std::invalid_argument("Parser must be a string or character stream, not NoneType");
    check_ocel2_xml_date_text(node.text);
    return node.text;
  }
  if (declared_type != nullptr && declared_type->find("float") != std::string::npos) {
    if (none)
      throw std::invalid_argument("float() argument must be a string or a real number, not 'NoneType'");
    if (const auto parsed = detail::parse_python_float_text(node.text)) return *parsed;
    throw std::invalid_argument("could not convert string to float: " + ocel_frame::python_str_repr(node.text));
  }
  if (none) return std::string("None");
  return node.text;
}

// Returns the typed times of the events, in file order, for
// sort_ocel_xml_events (a former-dialect event's time is a date).
std::vector<OcelXmlTime> parse_classic(const Node& root, Ocel& log, bool reject_lists,
                                       std::vector<HeldColumnChange>& held_changes,
                                       std::vector<XmlFrameOverride>& overrides) {
  check_reference_values(root);
  std::unordered_map<std::string, std::string> types;
  for (const auto* objects : children(root, "objects")) {
    for (const auto& element : objects->children) {
      const auto* on = &element;
      // read_ocel_xml reads every child of <objects> as an object.
      if (reference_shaped_object(*on)) {
        read_reference_object(*on, log, reject_lists, types, held_changes);
        continue;
      }
      if (local(on->name) != "object") continue;
      const std::string id = attr_or(*on, "id", attr_or(*on, "object-id"));
      const std::string type = attr_or(*on, "type", attr_or(*on, "object-type")).empty()
                                   ? (keyed_value(*on, "type").empty()
                                          ? keyed_value(*on, "object-type")
                                          : keyed_value(*on, "type"))
                                   : attr_or(*on, "type", attr_or(*on, "object-type"));
      // An empty type is no error: drop_empty_ocel_names drops the object
      // (observed black-box).
      if (const auto [it, added] = types.emplace(id, type); !added && !id.empty())
        throw_repeated_xml_object(id, it->second, type);
      OcelObject o{id, type, {}};
      // The former native dialect (id and type as XML attributes) keeps every
      // keyed child as an attribute.
      std::vector<std::size_t> dated;
      append_values_filtered(*on, o.attributes, reject_lists,
                             {"id", "object-id", "type", "object-type"}, &dated);
      for (const auto index : dated)
        convert_ocel_date_value(o.attributes[index].value, OcelTimestampReader::kIsoFormat);
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
  std::vector<OcelXmlTime> times;
  for (const auto* events : children(root, "events")) {
    for (const auto& element : events->children) {
      const auto* en = &element;
      // read_ocel_xml reads every child of <events> as an event.
      if (reference_shaped_event(*en)) {
        read_reference_event(*en, log, reject_lists, types, times, overrides);
        continue;
      }
      if (local(en->name) != "event") continue;
      const std::string id = attr_or(*en, "id", attr_or(*en, "event-id"));
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
      // As in the OCEL 2.0 reader: no activity drops the event later, no
      // timestamp raises.
      if (e.timestamp.empty()) throw std::invalid_argument("invalid OCEL XML event");
      std::vector<std::size_t> dated;
      append_values_filtered(*en, e.attributes, reject_lists,
                             {"id", "event-id", "activity", "type", "timestamp", "time"}, &dated);
      for (const auto index : dated)
        convert_ocel_date_value(e.attributes[index].value, OcelTimestampReader::kIsoFormat);
      log.events.push_back(std::move(e));
      times.push_back({});
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
    for (const auto& v : cn->children) if (local(v.name) != "field") {
      c.value = value_node(v, reject_lists);
      if (local(v.name) == "date" || local(v.name) == "timestamp")
        convert_ocel_date_value(c.value, OcelTimestampReader::kIsoFormat, true);
      break;
    }
    if (c.object_id.empty() || c.field.empty()) throw std::invalid_argument("invalid OCEL XML object change");
    log.object_changes.push_back(std::move(c));
    }
  // read_ocel_xml reads no <global> scope (observed black-box: a custom event
  // global and the written defaults both read as no globals), so the OCEL 1.0
  // writers then fill the event and object scopes with their defaults.
  return times;
}

// read_ocel2_xml reads every time and declared date value as it walks the
// file (round EB, observed black-box): objects and events in document order,
// an event's time before its attributes, an object attribute's value before
// its time, so the first unreadable text raises its dateutil error.
void parse_standard(const Node& root, Ocel& log, bool reject_lists) {
  std::unordered_map<std::string, std::string> types;
  StandardXmlAttributeTypes object_attribute_types;
  StandardXmlAttributeTypes event_attribute_types;
  StandardXmlUntypedNames object_untyped;
  StandardXmlUntypedNames event_untyped;
  // Round EJ: the names whose values were read under a "date" declaration,
  // and those read without one (a value met before its type table stays a
  // text in read_ocel2_xml, observed black-box).
  StandardXmlUntypedNames object_dates_read, object_texts_read;
  StandardXmlUntypedNames event_dates_read, event_texts_read;
  const auto note_read = [](StandardXmlUntypedNames& dates, StandardXmlUntypedNames& texts,
                            const std::string& owner, const std::string& key, const std::string* declared) {
    if (declared != nullptr && declared->find("date") != std::string::npos) dates[owner].insert(key);
    else texts[owner].insert(key);
  };
  struct StandardAttribute {
    std::string name;
    OcelValue value;
    bool has_time = false;
    std::string time;
  };
  const auto declared_in = [](const StandardXmlAttributeTypes& table, const std::string& owner,
                              const std::string& key) -> const std::string* {
    const auto found_owner = table.find(owner);
    if (found_owner == table.end()) return nullptr;
    const auto found = found_owner->second.find(key);
    return found == found_owner->second.end() ? nullptr : &found->second;
  };
  const auto untyped_in = [](const StandardXmlUntypedNames& table, const std::string& owner,
                             const std::string& key) {
    const auto found = table.find(owner);
    return found != table.end() && found->second.count(key) != 0;
  };
  // Round EJ: read_ocel2_xml walks the root's sections in document order and
  // reads each value where it meets it, so the first value it cannot read
  // raises: an object attribute's value (by its declared type), then its
  // time (none raises AttributeError, "0" and the 1970-01-01T00:00:00 base
  // entries stay texts, any other is dateutil's), an event's time, then its
  // attribute values (observed black-box).
  for (const auto& section : root.children) {
    const auto section_name = local(section.name);
    if (section_name == "object-types") {
      parse_standard_xml_type_table(section, "object-type", object_attribute_types, object_untyped);
      continue;
    }
    if (section_name == "event-types") {
      parse_standard_xml_type_table(section, "event-type", event_attribute_types, event_untyped);
      continue;
    }
    if (section_name == "objects") {
    for (const auto* on : children(section, "object")) {
      const std::string id = attr_or(*on, "id"), type = attr_or(*on, "type");
      // An empty id or type is no error: drop_empty_ocel_names drops the
      // object (observed black-box), so an empty id may repeat.
      if (const auto [it, added] = types.emplace(id, type); !added && !id.empty())
        throw_repeated_xml_object(id, it->second, type);
      OcelObject o{id, type, {}};
      if (const auto* a = child(*on, "attributes")) {
        // Grouped by name in first-appearance order, like the reference's
        // frame columns.
        std::vector<std::pair<std::string, std::vector<StandardAttribute>>> grouped;
        std::unordered_map<std::string, std::size_t> group_index;
        for (const auto& av : a->children) if (local(av.name) == "attribute") {
          // Round EJ: read_ocel2_xml takes the name attribute alone: an empty
          // name is a column, none a NaN-named one (keyless, declared:
          // skipped). Its value and time are still read.
          const auto* named = attr(av, "name");
          const std::string key = named != nullptr ? *named : std::string{};
          const auto* declared = declared_in(object_attribute_types, type, key);
          StandardAttribute entry{key, named == nullptr
                                           ? standard_xml_value(av, nullptr, reject_lists)
                                           : standard_xml_value(av, declared, reject_lists,
                                                                untyped_in(object_untyped, type, key))};
          if (named != nullptr) note_read(object_dates_read, object_texts_read, type, key, declared);
          const auto* when = attr(av, "time");
          if (when == nullptr) throw std::invalid_argument("'NoneType' object has no attribute 'startswith'");
          if (*when != "0" && !when->starts_with("1970-01-01T00:00:00")) check_ocel2_xml_date_text(*when);
          entry.has_time = true;
          entry.time = *when;
          if (named == nullptr) continue;
          const auto [slot, added] = group_index.try_emplace(key, grouped.size());
          if (added) grouped.emplace_back(key, std::vector<StandardAttribute>{});
          grouped[slot->second].second.push_back(std::move(entry));
        }
        for (auto& [key, entries] : grouped) {
          if (entries.empty()) continue;
          o.attributes.push_back({key, entries.front().value});
          for (std::size_t i = 1; i < entries.size(); ++i)
            log.object_changes.push_back({id, type, entries[i].time, key,
                                           entries[i].value});
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
      continue;
    }
    if (section_name != "events") continue;
    for (const auto* en : children(section, "event")) {
      OcelEvent e{attr_or(*en, "id"), attr_or(*en, "type", attr_or(*en, "activity")), attr_or(*en, "time", attr_or(*en, "timestamp")), {}};
      // An event without id or type is dropped later (drop_empty_ocel_names);
      // one without time raises, as the reference's date reading does: its
      // dateutil parser on None (round EI, observed black-box), an empty
      // time as dateutil does on "".
      const auto* when = attr(*en, "time") ? attr(*en, "time") : attr(*en, "timestamp");
      if (when == nullptr)
        throw std::invalid_argument("Parser must be a string or character stream, not NoneType");
      check_ocel2_xml_date_text(*when);
      if (const auto* a = child(*en, "attributes")) {
        for (const auto& av : a->children) if (local(av.name) == "attribute") {
          const auto* named = attr(av, "name");
          if (named == nullptr) {
            // A keyless value (declared: skipped) is still read, untyped.
            (void)standard_xml_value(av, nullptr, reject_lists);
            continue;
          }
          const auto& key = *named;
          const auto* declared = declared_in(event_attribute_types, e.activity, key);
          e.attributes.push_back({key, standard_xml_value(av, declared, reject_lists,
                                                           untyped_in(event_untyped, e.activity, key))});
          note_read(event_dates_read, event_texts_read, e.activity, key, declared);
        }
      }
      log.events.push_back(std::move(e));
      log.event_type_attributes.try_emplace(log.events.back().activity);
      // read_ocel2_xml drops a relation to an object it has not read yet
      // (and the event when it keeps none), observed black-box: an object
      // listed after the event, also in a later <objects> section, is not
      // related (round EJ).
      for (const auto* rs : children(*en, "objects"))
        for (const auto* r : children(*rs, "relationship")) {
          auto object_id = attr_or(*r, "object-id", attr_or(*r, "objectId"));
          if (!types.count(object_id)) continue;
          log.relations.push_back({log.events.back().id, std::move(object_id), attr_or(*r, "qualifier")});
        }
    }
  }
  // Round EJ: read_ocel2_xml converts a type naming "date" (case-sensitive)
  // alone; "time", "Date", "timestamp", ... stay texts, which the catalog
  // records as "string" so later routes (writers, flattening) do not take
  // them for timestamps as the OCEL 2.0 JSON reader's would be.
  // A "date" declared after all of its values were read (a type table that
  // follows the objects or events) left them texts: the catalog says
  // "string" too. Values read both ways are a mixed column (declared).
  const auto as_read = [&untyped_in](std::map<std::string, std::string> attributes, const std::string& owner,
                                     const StandardXmlUntypedNames& dates, const StandardXmlUntypedNames& texts) {
    for (auto& [key, type] : attributes) {
      if (type.find("date") != std::string::npos) {
        if (untyped_in(texts, owner, key) && !untyped_in(dates, owner, key)) type = "string";
        continue;
      }
      std::string lowered = type;
      for (auto& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      if (lowered.find("date") != std::string::npos || lowered.find("time") != std::string::npos) type = "string";
    }
    return attributes;
  };
  for (auto& [owner, attributes] : object_attribute_types)
    log.object_type_attributes[owner] = as_read(attributes, owner, object_dates_read, object_texts_read);
  for (auto& [owner, attributes] : event_attribute_types)
    log.event_type_attributes[owner] = as_read(attributes, owner, event_dates_read, event_texts_read);
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

using StandardXmlTypeTable = ocel_frame::TypeDeclarations;

void collect_standard_types(const Ocel& log, StandardXmlTypeTable& object_types,
                            StandardXmlTypeTable& event_types) {
  ocel_frame::reference_type_declarations(log, object_types, event_types);
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
// In a column that mixes dates with other values (`mixed_dates`), the frame
// column is an object column and the reference writes each date as the
// <string> of its isoformat ("2024-02-01T08:30:00+00:00"), observed black-box.
void write_classic_value(std::ostream& out, const OcelValue& v, std::string_view key,
                         std::string_view indent, bool float_column = false,
                         bool date = false, bool mixed_dates = false) {
  if (std::holds_alternative<std::nullptr_t>(v)) return;
  if (const auto* text = date ? ocel_frame::date_value_text(v) : nullptr) {
    if (mixed_dates) {
      out << indent << "<string key=\"" << esc(key) << "\" value=\""
          << esc(ocel_frame::utc_timestamp_text(*text, 'T', "+00:00")) << "\"/>\n";
      return;
    }
    out << indent << "<date key=\"" << esc(key) << "\" value=\""
        << esc(classic_xml_timestamp_text(*text)) << "\"/>\n";
    return;
  }
  const auto* integer = std::get_if<std::int64_t>(&v);
  // Round EK: a float of an object column (floats next to texts, booleans or
  // datetimes) is the <string> of its repr (observed black-box).
  if (const auto* real = std::get_if<double>(&v); float_column && (real != nullptr || integer)) {
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
  std::vector<OcelXmlTime> classic_times;
  std::vector<HeldColumnChange> held_changes;
  std::vector<XmlFrameOverride> overrides;
  if (standard) parse_standard(root, log, options.reject_unsupported);
  else classic_times = parse_classic(root, log, options.reject_unsupported, held_changes, overrides);
  // The rows a vmap ocel:* entry drops from the events frame (see
  // XmlFrameOverride): an id other than the own one drops the row unless a
  // relation names it (then it is held).
  std::unordered_set<std::string> dropped_events;
  if (!overrides.empty()) {
    std::unordered_set<std::string> related;
    for (const auto& relation : log.relations) related.insert(relation.event_id);
    for (auto& frame : overrides) {
      if (frame.frame_id) {
        if (related.count(*frame.frame_id)) {
          if (frame.held.empty())
            frame.held = "OCEL XML event " + frame.event_id + " vmap entry ocel:eid " +
                         ocel_frame::python_str_repr(*frame.frame_id) +
                         ": pm4py puts it in the events frame next to the rows it names, which a "
                         "native OCEL cannot hold";
        } else {
          frame.drop = true;
        }
      }
      if (frame.drop) dropped_events.insert(frame.event_id);
    }
    // An id listed by another event too names rows a native OCEL cannot
    // tell apart from the dropped one.
    std::unordered_map<std::string, std::size_t> listed;
    for (const auto& event : log.events) ++listed[event.id];
    for (const auto& id : dropped_events)
      if (listed[id] > 1)
        throw std::invalid_argument("OCEL XML event " + id +
                                    ": a vmap ocel:* entry drops one of the rows under this id, "
                                    "which a native OCEL cannot tell apart");
  }
  // Both reference XML readers build the events frame activity first.
  log.event_activity_first = true;
  // read_ocel2_xml also drops an object link to an unlisted object (the
  // classic reader raises on a relation to one).
  if (standard) {
    std::unordered_set<std::string> object_ids;
    for (const auto& object : log.objects) object_ids.insert(object.id);
    std::erase_if(log.o2o, [&](const OcelO2oRelation& relation) {
      return !object_ids.count(relation.source_id) || !object_ids.count(relation.target_id);
    });
  }
  validate_references(log);
  // Both reference XML readers fail on a file without an event-object
  // relation to a listed object (observed black-box: AttributeError).
  // Round EJ: read_ocel2_xml says "'NoneType' object has no attribute
  // 'index'" (observed black-box).
  if (log.relations.empty())
    throw std::invalid_argument(standard ? "'NoneType' object has no attribute 'index'"
                                         : "OCEL XML file has no event-object relation");
  // The OCEL 2.0 XML reader reads dates leniently, the classic one as ISO.
  if (standard) sort_ocel_events_by_timestamp(log, OcelTimestampReader::kDateutil);
  else sort_ocel_xml_events(log, classic_times);
  // Round EH: no event with a timestamp element leaves pm4py a time column
  // of None (see Ocel::none_event_times).
  if (!standard && !classic_times.empty() &&
      std::all_of(classic_times.begin(), classic_times.end(),
                  [](const OcelXmlTime& time) { return time.kind == OcelXmlTime::Kind::kMissing; }))
    log.none_event_times = true;
  // Both XML readers convert date values: OCEL 2.0 the declared ones here,
  // OCEL 1.0 each <date> element as parse_classic reads it.
  if (standard) convert_ocel_date_attributes(log, OcelTimestampReader::kDateutil);
  // Both drop events without id or activity and objects without id or type
  // (after reading every date, which can raise), then the events and objects
  // without relations.
  drop_empty_ocel_names(log);
  drop_unrelated_ocel_entries(log);
  // Round EG: the rows a vmap ocel:* entry drops go after the objects their
  // relations name were kept; with no relation left, no event or object
  // stays (see XmlFrameOverride).
  if (!dropped_events.empty()) {
    std::erase_if(log.events, [&](const OcelEvent& event) { return dropped_events.count(event.id) != 0; });
    std::erase_if(log.relations, [&](const OcelRelation& relation) {
      return dropped_events.count(relation.event_id) != 0;
    });
    std::erase_if(log.e2e, [&](const OcelE2eRelation& relation) {
      return dropped_events.count(relation.source_id) || dropped_events.count(relation.target_id);
    });
    if (log.relations.empty()) {
      log.events.clear();
      log.objects.clear();
      log.object_changes.clear();
      log.o2o.clear();
      held_changes.clear();
    }
  }
  for (const auto& frame : overrides) {
    if (frame.held.empty()) continue;
    const bool remains = std::any_of(log.events.begin(), log.events.end(), [&](const OcelEvent& event) {
      return frame.column ? true : event.id == frame.event_id;
    });
    if (remains && (frame.column || !frame.drop)) throw std::invalid_argument(frame.held);
  }
  // An OCEL 1.0 object change of ocel:oid or ocel:type whose row survives
  // (see read_reference_object) is refused (declared).
  if (!held_changes.empty()) {
    std::unordered_set<std::string> object_ids;
    for (const auto& object : log.objects) object_ids.insert(object.id);
    for (const auto& change : held_changes)
      if (object_ids.count(change.object_id))
        throw std::invalid_argument(
            "OCEL XML object change of " + change.key + ": pm4py writes it into the change "
            "frame's own column, which a native OCEL cannot hold");
  }
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
    auto text = entries.str();
    // write_ocel_xml fills an undeclared scope with its default.
    if (text.empty() && scope == "event" && !log.global_event_scope)
      text = "    <string key=\"ocel:activity\" value=\"__INVALID__\"/>\n";
    if (text.empty() && scope == "object" && !log.global_object_scope)
      text = "    <string key=\"ocel:type\" value=\"__INVALID__\"/>\n";
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
  // Round EM: with the rows the reader or writer dropped (OcelFrameHint).
  const auto event_frame = ocel_frame::event_frame(log);
  const auto object_frame = ocel_frame::object_frame(log);
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
  // The keys whose frame column mixes date values with other values (one
  // events frame and one objects frame, across activities and types).
  // Round EK: naive next to aware datetimes make an object column too.
  const auto mixed_date_keys = [](const auto& entries, const OcelTypeCatalog& catalog,
                                  const auto& owner_of, const OcelFrameHint& hint) {
    ocel_frame::Columns columns;
    columns.seed(hint);
    for (const auto& entry : entries) columns.observe_row(entry.attributes, catalog, owner_of(entry));
    std::set<std::string, std::less<>> mixed;
    for (const auto& key : columns.names())
      if (const auto* stats = columns.stats(key);
          stats != nullptr && stats->has_date() && columns.dtype(key) == ocel_frame::Dtype::kObject)
        mixed.insert(key);
    return mixed;
  };
  const auto mixed_event_dates = mixed_date_keys(
      log.events, log.event_type_attributes, [](const OcelEvent& event) -> const std::string& { return event.activity; },
      log.event_frame_hint);
  const auto mixed_object_dates = mixed_date_keys(
      log.objects, log.object_type_attributes, [](const OcelObject& object) -> const std::string& { return object.type; },
      log.object_frame_hint);
  // A CSV log whose times share an offset is written in that wall clock,
  // labelled "Z" as the reference does (see shared_nonzero_offset).
  const auto event_offset = event_time_offset(log);
  if (log.events.empty()) {
    out << "  <events/>\n";
  } else {
    out << "  <events>\n";
    for (const auto& event : log.events) {
      // Fields in the reference frame's column order: id, timestamp,
      // activity for a log read from JSON, activity first after XML.
      const auto activity_line = "      <string key=\"activity\" value=\"" + esc(event.activity) + "\"/>\n";
      out << "    <event>\n      <string key=\"id\" value=\"" << esc(event.id) << "\"/>\n";
      if (log.event_activity_first) out << activity_line;
      // Round EG: the reference writes no timestamp for a NaT time (an
      // OCEL 2.0 SQLite '' or NULL), as for any missing cell (observed
      // black-box).
      // Round EL: a raw time (OcelRawTime) as its Python value: a str as a
      // string, NaN left out, an int or float raises lxml's TypeError
      // (observed black-box, probe el wo.py).
      if (event.raw_time == OcelRawTime::kInt || event.raw_time == OcelRawTime::kFloat)
        throw std::invalid_argument(std::string("TypeError: Argument must be bytes or unicode, got '") +
                                    (event.raw_time == OcelRawTime::kInt ? "int" : "float") + "'");
      if (event.raw_time == OcelRawTime::kStr)
        out << "      <string key=\"timestamp\" value=\"" << esc(event.timestamp) << "\"/>\n";
      else if (event.raw_time == OcelRawTime::kNaN) {
      } else if (!ocel_frame::nat_time_text(event.timestamp))
        out << "      <date key=\"timestamp\" value=\""
            << esc(event_offset ? ocel_frame::offset_timestamp_text(event.timestamp, 'T',
                                                                    event_offset, "Z")
                                : classic_xml_timestamp_text(event.timestamp))
            << "\"/>\n";
      if (!log.event_activity_first) out << activity_line;
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
      // Round EK: in the frame's column order (observed black-box).
      write_list("vmap", [&] {
        for (const auto* attribute : ocel_frame::frame_row(event.attributes, event_frame))
          if (!reserved_name(attribute->key))
            write_classic_value(out, attribute->value, attribute->key, "        ",
                                event_frame.dtype(attribute->key) == ocel_frame::Dtype::kFloat64,
                                ocel_frame::declared_date(log.event_type_attributes,
                                                          event.activity, attribute->key),
                                mixed_event_dates.contains(attribute->key));
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
        for (const auto* attribute : ocel_frame::frame_row(object.attributes, object_frame))
          if (!reserved_name(attribute->key))
            write_classic_value(out, attribute->value, attribute->key, "        ",
                                object_frame.dtype(attribute->key) == ocel_frame::Dtype::kFloat64,
                                ocel_frame::declared_date(log.object_type_attributes,
                                                          object.type, attribute->key),
                                mixed_object_dates.contains(attribute->key));
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
void write_ocel_xml(std::ostream& out, const Ocel& input,
                    const OcelXmlExportOptions& options) {
  // Round EH: as both reference XML writers, without the events and objects
  // that have no event-object relation.
  std::optional<Ocel> written;
  const Ocel& log = ocel_written_entries(input, written);
  if (options.format != OcelXmlFormat::kStandard) {
    write_ocel_xml_legacy(out, log, options);
    return;
  }
  if (!options.allow_lossy) validate_references(log);
  // Round EK: write_ocel2_xml takes each change time's isoformat in log
  // order. A change-time column holding a text pm4py could not read is no
  // datetime64 column: the text raises "'str' object has no attribute
  // 'isoformat'", and a missing time is None next to read dates (an object
  // column) or NaN without any (pandas' str dtype), observed black-box (probe
  // ek t/p11.py). Without such a text a missing time prints "NaT" (EJ1-7).
  // Round EL: a change whose field is no column of the changes frame (no
  // object type declares it, no object and no other change gives it a
  // value: an OCEL 2.0 SQLite change of '' or of a field its table lacks)
  // raises KeyError at its row, before its time is read; a raw change time
  // (OcelRawTime) raises its Python type's error (observed black-box, probe
  // el wo.py).
  {
    const auto readable = [](const std::string& text) {
      const auto parsed = minera::parse_timestamp_exact(text);
      return parsed && ocel_frame::civil_timestamp(*parsed).has_value();
    };
    bool any_date = false, any_text = false;
    for (const auto& change : log.object_changes) {
      if (change.raw_time != OcelRawTime::kNone || ocel_frame::pandas_nat_time_text(change.timestamp)) continue;
      (readable(change.timestamp) ? any_date : any_text) = true;
    }
    std::unordered_set<std::string_view> columns;
    for (const auto& [type, schema] : log.object_type_attributes)
      for (const auto& [name, declared] : schema) columns.insert(name);
    for (const auto& object : log.objects)
      for (const auto& attribute : object.attributes) columns.insert(attribute.key);
    for (const auto& change : log.object_changes)
      if (!std::holds_alternative<std::nullptr_t>(change.value)) columns.insert(change.field);
    const auto type_error = [](const char* type) {
      throw std::invalid_argument(std::string("AttributeError: '") + type + "' object has no attribute 'isoformat'");
    };
    for (const auto& change : log.object_changes) {
      if (!columns.count(change.field))
        throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(change.field));
      if (change.raw_time != OcelRawTime::kNone) type_error(ocel_raw_time_type(change.raw_time));
      if (!any_text) continue;
      if (ocel_frame::pandas_nat_time_text(change.timestamp)) type_error(any_date ? "NoneType" : "float");
      if (!readable(change.timestamp)) type_error("str");
    }
  }
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
  // Round EK: a row's values in the frame's column order (observed
  // black-box), see ocel_frame::frame_row.
  // Round EM: with the rows the reader or writer dropped (OcelFrameHint).
  const auto standard_event_frame = ocel_frame::event_frame(log);
  const auto standard_object_frame = ocel_frame::object_frame(log);
  out << (log.objects.empty() ? "  <objects/>\n" : "  <objects>\n");
  for (const auto& object : log.objects) {
    out << "    <object id=\"" << esc(object.id) << "\" type=\""
        << esc(object.type) << "\">\n";
    std::ostringstream attributes;
    const auto object_date = [&](const std::string& key) {
      return ocel_frame::declared_date(log.object_type_attributes, object.type, key);
    };
    for (const auto* attribute : ocel_frame::frame_row(object.attributes, standard_object_frame))
      write_standard_value(attributes, attribute->value, attribute->key, "        ",
                           "1970-01-01T00:00:00Z", object_date(attribute->key), true);
    // Round EJ: an object's changes in log order, as write_ocel2_xml writes
    // them (observed black-box: not sorted by time).
    const std::vector<const OcelObjectChange*>& object_changes = rows_of(changes, object.id);
    // Like the reference, a field without a base value gets no null base
    // entry: its first change reads back as the base, as PM4Py reads its own
    // export.
    // A NaT change time (the empty text) prints "NaT" (observed black-box).
    // Round EK: a change of a null value prints Python's str of it,
    // "None" (observed black-box).
    // Round EM: the changes frame's time column that reaches the writer is
    // a UTC datetime64 column (every reader converts with utc=True; a column
    // pandas cannot convert raised above), so a naive time prints "+00:00"
    // too (observed black-box: g52_0107).
    const auto change_time_text = [](const std::string& text) {
      auto printed = ocel_frame::utc_timestamp_text(text, 'T', "+00:00");
      if (ocel_frame::naive_timestamp_text(text)) printed += "+00:00";
      return printed;
    };
    for (const auto* change : object_changes)
      write_standard_value(attributes,
                           std::holds_alternative<std::nullptr_t>(change->value) ? OcelValue(std::string("None"))
                                                                                  : change->value,
                           change->field, "        ",
                           ocel_frame::pandas_nat_time_text(change->timestamp)
                               ? std::string("NaT")
                               : change_time_text(change->timestamp),
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
  // Round EH: a time column of None (Ocel::none_event_times) fails on the
  // first event's isoformat (observed black-box).
  if (log.none_event_times && !log.events.empty())
    throw std::invalid_argument("AttributeError: 'NoneType' object has no attribute 'isoformat'");
  // Round EL: and so does a raw time column (OcelRawTime), with the first
  // event's Python type (observed black-box).
  if (!log.events.empty() && log.events.front().raw_time != OcelRawTime::kNone) {
    const auto kind = log.events.front().raw_time;
    throw std::invalid_argument(std::string("AttributeError: '") +
                                (kind == OcelRawTime::kStr ? "str" : kind == OcelRawTime::kInt ? "int" : "float") +
                                "' object has no attribute 'isoformat'");
  }
  out << (log.events.empty() ? "  <events/>\n" : "  <events>\n");
  for (const auto& event : log.events) {
    out << "    <event id=\"" << esc(event.id) << "\" type=\""
        << esc(event.activity) << "\" time=\""
        // Round EG: a NaT time (an OCEL 2.0 SQLite '' or NULL) is written
        // as pandas prints it (observed black-box).
        << (ocel_frame::nat_time_text(event.timestamp)
                ? std::string("NaT")
                : esc(ocel_frame::offset_timestamp_text(event.timestamp, 'T', event_offset,
                                                        event_offset ? std::nullopt
                                                                     : std::optional<std::string_view>("+00:00"))))
        << "\">\n";
    std::ostringstream attributes;
    for (const auto* attribute : ocel_frame::frame_row(event.attributes, standard_event_frame))
      write_standard_value(attributes, attribute->value, attribute->key, "        ", {},
                           ocel_frame::declared_date(log.event_type_attributes,
                                                     event.activity, attribute->key),
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
  // The reference OCEL 1.0 writers' own globals (scope defaults, computed
  // log scope) are left out, as the OCEL 2.0 JSON writer leaves them out.
  if (std::any_of(log.globals.begin(), log.globals.end(), [](const auto& global) {
        return !ocel_reference_generated_global(global.first, global.second);
      })) {
    out << "  <globals>\n";
    for (const auto& [key, value] : log.globals)
      if (!ocel_reference_generated_global(key, value))
        write_standard_value(out, value, key, "    ");
    out << "  </globals>\n";
  }
  out << "</log>\n";
}

std::string serialize_ocel_xml(const Ocel& log, const OcelXmlExportOptions& options) { std::ostringstream out; write_ocel_xml(out, log, options); return out.str(); }
void export_ocel_xml(const Ocel& log, const std::filesystem::path& path, const OcelXmlExportOptions& options) { std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("Unable to create OCEL XML: " + path.string()); write_ocel_xml(out, log, options); }

}  // namespace minera
