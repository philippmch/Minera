#include "minera/xes_export.hpp"
#include "root_cause_trace_key.hpp"

#include "gzip_codec.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <system_error>
#include <string>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

// Escapes the five XML metacharacters so case ids, activity names, and
// resources containing '&', '<', quotes, etc. produce well-formed XES.
void write_xml_escaped(std::ostream& out, std::string_view value) {
  for (const char c : value) {
    switch (c) {
      case '&': out << "&amp;"; break;
      case '<': out << "&lt;"; break;
      case '>': out << "&gt;"; break;
      case '"': out << "&quot;"; break;
      case '\'': out << "&apos;"; break;
      default: out << c;
    }
  }
}

void write_string_attribute(std::ostream& out, std::string_view indent,
                            std::string_view key, std::string_view value,
                            bool is_null = false) {
  out << indent << "<string key=\"";
  write_xml_escaped(out, key);
  if (is_null) {
    out << "\" null=\"true\"/>\n";
  } else {
    out << "\" value=\"";
    write_xml_escaped(out, value);
    out << "\"/>\n";
  }
}

std::string_view element_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
    case AttributeKind::kBytes: return "string";
    case AttributeKind::kNA:
      throw std::invalid_argument(
          "XES cannot represent a native NA attribute");
  }
  return "string";
}

bool parse_int_value(std::string_view value, std::int64_t& result) {
  if (value.empty()) return false;
  const char* first = value.data();
  const char* last = value.data() + value.size();
  if (*first == '+') ++first;
  if (first == last) return false;
  const auto parsed = std::from_chars(first, last, result, 10);
  return parsed.ec == std::errc{} && parsed.ptr == last;
}

bool parse_uint_value(std::string_view value, std::uint64_t& result) {
  if (value.empty()) return false;
  const char* first = value.data();
  const char* last = value.data() + value.size();
  if (*first == '+') ++first;
  if (first == last) return false;
  const auto parsed = std::from_chars(first, last, result, 10);
  return parsed.ec == std::errc{} && parsed.ptr == last;
}

bool parse_float_value(std::string_view value, double& result) {
  if (value == "INF" || value == "+INF") {
    result = std::numeric_limits<double>::infinity();
    return true;
  }
  if (value == "-INF") {
    result = -std::numeric_limits<double>::infinity();
    return true;
  }
  if (value == "NaN") {
    result = std::numeric_limits<double>::quiet_NaN();
    return true;
  }
  const char* first = value.data();
  if (!value.empty() && *first == '+') ++first;
  const auto parsed = std::from_chars(
      first, value.data() + value.size(), result,
      std::chars_format::general);
  return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

std::string xes_date_lexical_value(std::string_view text,
                                 NativeDateResolution resolution) {
  if (!parse_timestamp(text) || text.substr(0, 4) == "0000")
    throw std::invalid_argument("XES date attribute has an invalid value: " +
                                std::string(text));
  // Serialize the source fields, never a floating-point epoch conversion: that
  // would discard offsets, timezone absence, and distant-date nanoseconds.
  std::string result(text.substr(0, 10));
  result += 'T';
  std::size_t position = 10;
  if (position < text.size() &&
      (text[position] == 'T' || text[position] == 't' || text[position] == ' ')) {
    ++position;
    result += text.substr(position, 5);
    position += 5;
    if (position < text.size() && text[position] == ':') {
      result += text.substr(position, 3);
      position += 3;
    } else result += ":00";
  } else result += "00:00:00";
  std::string fraction;
  if (position < text.size() && text[position] == '.') {
    const auto begin = ++position;
    while (position < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[position]))) ++position;
    fraction = text.substr(begin, position - begin);
  }
  const auto precision = resolution == NativeDateResolution::kNanosecond ? 9u : 6u;
  fraction.resize(precision, '0');
  if (precision == 9 && fraction.substr(6) == "000") fraction.resize(6);
  if (fraction.find_first_not_of('0') != std::string::npos)
    result += '.' + fraction;
  if (position < text.size() && (text[position] == 'Z' || text[position] == 'z'))
    result += "+00:00";
  else if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
    const char sign = text[position++];
    const auto hours = text.substr(position, 2);
    position += 2;
    if (position < text.size() && text[position] == ':') ++position;
    const auto minutes = position + 2 <= text.size() &&
        std::isdigit(static_cast<unsigned char>(text[position]))
        ? text.substr(position, 2) : std::string_view("00");
    result += hours == "00" && minutes == "00" ? '+' : sign;
    result += hours;
    result += ':';
    result += minutes;
  }
  return result;
}

std::string scalar_lexical_value(const EventAttribute& attribute) {
  if (attribute.is_null) return {};
  if (attribute.kind != AttributeKind::kBytes) {
    if (const auto* value = std::get_if<std::int64_t>(&attribute.typed_value)) {
      if (attribute.kind != AttributeKind::kInt)
        throw std::invalid_argument("XES attribute has an incompatible integer native value");
      return std::to_string(*value);
    }
    if (const auto* value = std::get_if<std::uint64_t>(&attribute.typed_value)) {
      if (attribute.kind != AttributeKind::kInt)
        throw std::invalid_argument("XES attribute has an incompatible unsigned integer native value");
      return std::to_string(*value);
    }
    if (const auto* value = std::get_if<double>(&attribute.typed_value)) {
      if (attribute.kind != AttributeKind::kFloat)
        throw std::invalid_argument("XES attribute has an incompatible float native value");
      if (std::isnan(*value)) return "NaN";
      if (std::isinf(*value)) return *value < 0 ? "-INF" : "INF";
      char buffer[128];
      const auto converted = std::to_chars(
          buffer, buffer + sizeof(buffer), *value,
          std::chars_format::general, std::numeric_limits<double>::max_digits10);
      if (converted.ec != std::errc{})
        throw std::invalid_argument("XES float attribute cannot be serialized");
      return std::string(buffer, converted.ptr);
    }
    if (const auto* value = std::get_if<bool>(&attribute.typed_value)) {
      if (attribute.kind != AttributeKind::kBoolean)
        throw std::invalid_argument("XES attribute has an incompatible boolean native value");
      return *value ? "true" : "false";
    }
    if (const auto* value = std::get_if<NativeDate>(&attribute.typed_value)) {
      if (attribute.kind != AttributeKind::kDate)
        throw std::invalid_argument("XES attribute has an incompatible date native value");
      return xes_date_lexical_value(value->value, value->resolution);
    }
  }

  switch (attribute.kind) {
    case AttributeKind::kInt: {
      std::int64_t parsed = 0;
      if (parse_int_value(attribute.value, parsed)) return std::to_string(parsed);
      std::uint64_t unsigned_parsed = 0;
      if (parse_uint_value(attribute.value, unsigned_parsed))
        return std::to_string(unsigned_parsed);
      throw std::invalid_argument("XES int attribute has an invalid value: " +
                                  attribute.value);
    }
    case AttributeKind::kFloat: {
      double parsed = 0.0;
      if (!parse_float_value(attribute.value, parsed))
        throw std::invalid_argument(
            "XES float attribute has an invalid value: " + attribute.value);
      return attribute.value;
    }
    case AttributeKind::kBoolean:
      if (attribute.value != "true" && attribute.value != "false" &&
          attribute.value != "1" && attribute.value != "0")
        throw std::invalid_argument(
            "XES boolean attribute has an invalid value: " + attribute.value);
      return attribute.value == "true" || attribute.value == "1" ? "true" : "false";
    case AttributeKind::kDate: {
      return xes_date_lexical_value(attribute.value, NativeDateResolution::kMicrosecond);
    }
    case AttributeKind::kString:
    case AttributeKind::kId:
      return attribute.value;
    case AttributeKind::kBytes: {
      auto scalar = attribute;
      scalar.children.clear();
      scalar.scalar_wrapper = false;
      scalar.scalar_children_list = false;
      scalar.container = AttributeContainerKind::kScalar;
      return root_cause_attribute_category(scalar);
    }
    case AttributeKind::kNA:
      throw std::invalid_argument(
          "XES cannot represent a native NA attribute");
  }
  throw std::invalid_argument("XES attribute has an unsupported kind");
}

std::vector<const EventAttribute*> mapped_children(const EventAttribute& attribute) {
  std::vector<const EventAttribute*> children;
  std::map<std::string, std::size_t> positions;
  for (const auto& child : attribute.children) {
    const auto [position, inserted] = positions.emplace(child.key, children.size());
    if (inserted) children.push_back(&child);
    else children[position->second] = &child;
  }
  return children;
}

std::pair<std::string_view, std::string_view> mapping_key_entry(std::string_view key) {
  std::size_t position = 0;
  const auto next = [&]() {
    if (position == key.size())
      throw std::invalid_argument("XES mapping child key requires at least two characters");
    const auto begin = position;
    const auto lead = static_cast<unsigned char>(key[position++]);
    const unsigned count = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 :
        (lead & 0xf0) == 0xe0 ? 3 : (lead & 0xf8) == 0xf0 ? 4 : 0;
    if (!count || begin + count > key.size())
      throw std::invalid_argument("XES mapping child key is not valid UTF-8");
    unsigned code = lead & (count == 1 ? 0x7f : (1u << (7 - count)) - 1);
    for (unsigned i = 1; i < count; ++i) {
      const auto byte = static_cast<unsigned char>(key[position++]);
      if ((byte & 0xc0) != 0x80)
        throw std::invalid_argument("XES mapping child key is not valid UTF-8");
      code = (code << 6) | (byte & 0x3f);
    }
    if ((count == 2 && code < 0x80) || (count == 3 && code < 0x800) ||
        (count == 4 && code < 0x10000) || code > 0x10ffff ||
        (code >= 0xd800 && code <= 0xdfff))
      throw std::invalid_argument("XES mapping child key is not valid UTF-8");
    return key.substr(begin, count);
  };
  const auto first = next();
  const auto second = next();
  return {first, second};
}

void write_typed_attribute(std::ostream& out, std::string_view indent,
                           const EventAttribute& attribute, XesExportVariant variant, bool preserve_nulls) {
  validate_native_na_attribute(attribute, "XES export");
  if (contains_native_na_attribute(attribute))
    throw std::invalid_argument(
        "XES cannot represent a native NA attribute");
  if (attribute.container == AttributeContainerKind::kTuple ||
      attribute.container == AttributeContainerKind::kSet ||
      attribute.container == AttributeContainerKind::kFrozenSet) {
    write_typed_attribute(out, indent, EventAttribute{attribute.key, root_cause_attribute_category(attribute),
        AttributeKind::kString, attribute.is_null}, variant, preserve_nulls);
    return;
  }
  if (attribute.is_null && !preserve_nulls) {
    const bool wrapped = attribute.scalar_wrapper || attribute.scalar_children_list ||
        !attribute.children.empty() || attribute.container != AttributeContainerKind::kScalar;
    if (!wrapped) return;
    auto list = attribute;
    list.is_null = false;
    list.container = attribute.scalar_children_list ? AttributeContainerKind::kList :
        (attribute.container == AttributeContainerKind::kList ? AttributeContainerKind::kList : AttributeContainerKind::kContainer);
    list.scalar_wrapper = false;
    list.scalar_children_list = false;
    write_typed_attribute(out, indent, list, variant, preserve_nulls);
    return;
  }
  if (attribute.container != AttributeContainerKind::kScalar) {
    const bool native_null_container = attribute.is_null &&
        attribute.container == AttributeContainerKind::kContainer;
    const auto element = native_null_container ? "container" : "list";
    out << indent << '<' << element << " key=\"";
    write_xml_escaped(out, attribute.key);
    if (attribute.is_null && attribute.children.empty()) {
      out << "\" null=\"true\"/>\n";
      return;
    }
    if (attribute.is_null)
      out << "\" null=\"true\">\n";
    else
      out << "\">\n";
    const auto child_indent = std::string(indent) + "  ";
    if (!native_null_container) out << child_indent << "<values>\n";
    if (native_null_container) {
      for (const auto* child : mapped_children(attribute))
        write_typed_attribute(out, child_indent, *child, variant, preserve_nulls);
    } else if (attribute.container == AttributeContainerKind::kList) {
      for (const auto& child : attribute.children)
        write_typed_attribute(out, child_indent + "  ", child, variant, preserve_nulls);
    } else {
      for (const auto* child : mapped_children(attribute)) {
        if (variant == XesExportVariant::kElementTree)
          write_typed_attribute(out, child_indent + "  ", *child, variant, preserve_nulls);
        else {
          const auto [key, value] = mapping_key_entry(child->key);
          write_typed_attribute(out, child_indent + "  ",
              EventAttribute{std::string(key), std::string(value)}, variant, preserve_nulls);
        }
      }
    }
    if (!native_null_container) out << child_indent << "</values>\n";
    out << indent << "</" << element << ">\n";
    return;
  }
  if (attribute.scalar_children_list && variant == XesExportVariant::LINE_BY_LINE)
    throw std::invalid_argument("LINE_BY_LINE cannot export scalar list children");
  if (attribute.is_null) {
    out << indent << '<' << element_name(attribute.kind) << " key=\"";
    write_xml_escaped(out, attribute.key);
    if (attribute.children.empty()) {
      out << "\" null=\"true\"/>\n";
    } else {
      out << "\" null=\"true\">\n";
      const auto child_indent = std::string(indent) + "  ";
      if (attribute.scalar_children_list) {
        for (const auto& child : attribute.children)
          write_typed_attribute(out, child_indent, child, variant, preserve_nulls);
      } else {
        for (const auto* child : mapped_children(attribute))
          write_typed_attribute(out, child_indent, *child, variant, preserve_nulls);
      }
      out << indent << "</" << element_name(attribute.kind) << ">\n";
    }
    return;
  }
  auto lexical = scalar_lexical_value(attribute);
  // ETREE uses Python's boolean spelling for scalar values with child metadata.
  if (variant == XesExportVariant::ETREE &&
      attribute.kind == AttributeKind::kBoolean &&
      (attribute.scalar_wrapper || attribute.scalar_children_list || !attribute.children.empty()))
    lexical = lexical == "true" ? "True" : "False";
  if (variant == XesExportVariant::ETREE &&
      attribute.kind == AttributeKind::kDate &&
      (attribute.scalar_wrapper || attribute.scalar_children_list || !attribute.children.empty()))
    lexical[10] = ' ';
  out << indent << '<' << element_name(attribute.kind) << " key=\"";
  write_xml_escaped(out, attribute.key);
  out << "\" value=\"";
  write_xml_escaped(out, lexical);
  if (attribute.children.empty() &&
      !(variant == XesExportVariant::LINE_BY_LINE && attribute.scalar_wrapper)) {
    out << "\"/>\n";
  } else {
    out << "\">\n";
    const auto child_indent = std::string(indent) + "  ";
    if (attribute.scalar_children_list) {
      for (const auto& child : attribute.children)
        write_typed_attribute(out, child_indent, child, variant, preserve_nulls);
    } else {
      for (const auto* child : mapped_children(attribute))
        write_typed_attribute(out, child_indent, *child, variant, preserve_nulls);
    }
    out << indent << "</" << element_name(attribute.kind) << ">\n";
  }
}

void write_extension(std::ostream& out, const LogExtension& extension) {
  out << "  <extension name=\"";
  write_xml_escaped(out, extension.name);
  out << "\" prefix=\"";
  write_xml_escaped(out, extension.prefix);
  out << "\" uri=\"";
  write_xml_escaped(out, extension.uri);
  out << "\"/>\n";
}

bool extension_matches(const LogExtension& stored,
                       const LogExtension& generated) {
  return !stored.prefix.empty() && stored.prefix == generated.prefix;
}

struct StandardExtension {
  std::string_view name;
  std::string_view prefix;
  std::string_view uri;
};

constexpr StandardExtension kStandardExtensions[] = {
    {"Concept", "concept", "http://www.xes-standard.org/concept.xesext"},
    {"Time", "time", "http://www.xes-standard.org/time.xesext"},
    {"Organizational", "org", "http://www.xes-standard.org/org.xesext"},
    {"Lifecycle", "lifecycle", "http://www.xes-standard.org/lifecycle.xesext"},
    {"Cost", "cost", "http://www.xes-standard.org/cost.xesext"},
    {"Identity", "identity", "http://www.xes-standard.org/identity.xesext"},
    {"Artifact Lifecycle", "artifactlifecycle",
     "http://www.xes-standard.org/artifactlifecycle.xesext"},
    {"Micro", "micro", "http://www.xes-standard.org/micro.xesext"},
    {"Semantic", "semantic", "http://www.xes-standard.org/semantic.xesext"},
    {"Software Communication", "swcomm",
     "http://www.xes-standard.org/swcomm.xesext"},
    {"Software Event", "swevent",
     "http://www.xes-standard.org/swevent.xesext"},
    {"Software Telemetry", "swtelemetry",
     "http://www.xes-standard.org/swtelemetry.xesext"},
};

bool has_classifier_name(const std::vector<LogClassifier>& classifiers,
                         std::string_view name) {
  return std::any_of(classifiers.begin(), classifiers.end(),
                     [&](const auto& classifier) {
                       return classifier.name == name;
                     });
}

void write_global(std::ostream& out, std::string_view scope,
                  const std::vector<EventAttribute>& attributes,
                  bool infer_default_name, XesExportVariant variant, bool preserve_nulls) {
  out << "  <global scope=\"";
  write_xml_escaped(out, scope);
  out << "\">\n";
  bool has_concept_name = false;
  for (const auto& attribute : attributes) {
    has_concept_name = has_concept_name ||
                       (attribute.key == "concept:name" &&
                        !attribute.is_null);
    write_typed_attribute(out, "    ", attribute, variant, preserve_nulls);
  }
  if (!has_concept_name && infer_default_name) {
    write_string_attribute(out, "    ", "concept:name", "");
  }
  out << "  </global>\n";
}

bool has_non_null_concept_name(const LogGlobal& global) {
  return std::any_of(
      global.attributes.begin(), global.attributes.end(),
      [](const auto& attribute) {
        return attribute.key == "concept:name" && !attribute.is_null;
      });
}

std::vector<LogGlobal> complete_inferred_globals(
    const std::vector<LogGlobal>& globals) {
  auto result = globals;
  const auto has_scope_concept = [&](std::string_view scope) {
    return std::any_of(result.begin(), result.end(), [&](const auto& global) {
      return global.scope == scope && has_non_null_concept_name(global);
    });
  };
  // Append only missing declarations. Existing custom/repeated globals and
  // every attribute within them remain byte-for-byte ordered and untouched.
  if (!has_scope_concept("trace"))
    result.push_back({"trace", {{"concept:name", ""}}});
  if (!has_scope_concept("event"))
    result.push_back({"event", {{"concept:name", ""}}});
  return result;
}

std::vector<LogGlobal> effective_globals(const EventDataMetadata& metadata) {
  const auto projection = [](const std::vector<LogGlobal>& globals,
                             std::string_view scope) {
    std::vector<EventAttribute> result;
    for (const auto& global : globals) {
      if (global.scope != scope) continue;
      result.insert(result.end(), global.attributes.begin(),
                    global.attributes.end());
    }
    return result;
  };

  // Rebuild one legacy scope while retaining every original declaration and
  // its position. The compatibility vector is a concatenation, so the saved
  // declaration sizes are the only information available to redistribute an
  // edited vector without moving custom scopes or repeated declarations.
  const auto redistribute_scope =
      [](std::vector<LogGlobal> globals, std::string_view scope,
         const std::vector<EventAttribute>& flattened,
         const std::vector<LogGlobal>& baseline) {
        std::vector<std::size_t> baseline_sizes;
        baseline_sizes.reserve(baseline.size());
        for (const auto& global : baseline) {
          if (global.scope == scope)
            baseline_sizes.push_back(global.attributes.size());
        }

        std::size_t declaration = 0;
        std::size_t offset = 0;
        std::size_t last_scope = globals.size();
        for (std::size_t index = 0; index < globals.size(); ++index) {
          auto& global = globals[index];
          if (global.scope != scope) continue;
          last_scope = index;
          const auto original_size =
              declaration < baseline_sizes.size()
                  ? baseline_sizes[declaration]
                  : std::size_t{0};
          const auto available =
              offset < flattened.size()
                  ? std::min(original_size, flattened.size() - offset)
                  : std::size_t{0};
          global.attributes.assign(flattened.begin() + offset,
                                   flattened.begin() + offset + available);
          offset += available;
          ++declaration;
        }

        if (last_scope == globals.size()) {
          if (!flattened.empty())
            globals.push_back({std::string(scope), flattened});
        } else if (offset < flattened.size()) {
          auto& attributes = globals[last_scope].attributes;
          attributes.insert(attributes.end(), flattened.begin() + offset,
                            flattened.end());
        }
        return globals;
      };

  if (metadata.compatibility_snapshot_valid) {
    const bool globals_changed =
        metadata.globals != metadata.compatibility_globals_snapshot;
    const bool trace_changed =
        metadata.trace_globals != metadata.compatibility_trace_globals_snapshot;
    const bool event_changed =
        metadata.event_globals != metadata.compatibility_event_globals_snapshot;

    // Direct edits to the document-order representation are authoritative,
    // including intentional deletion of every declaration.
    if (globals_changed) return metadata.globals;

    auto result = metadata.globals;
    if (trace_changed)
      result = redistribute_scope(
          std::move(result), "trace", metadata.trace_globals,
          metadata.compatibility_globals_snapshot);
    if (event_changed)
      result = redistribute_scope(
          std::move(result), "event", metadata.event_globals,
          metadata.compatibility_globals_snapshot);
    return result;
  }

  // Without an importer snapshot, a populated document-order representation
  // is authoritative when a legacy projection is empty. Otherwise a manually
  // constructed `globals` entry would be mistaken for an explicit deletion.
  const bool trace_changed = !metadata.trace_globals.empty() &&
      projection(metadata.globals, "trace") != metadata.trace_globals;
  const bool event_changed = !metadata.event_globals.empty() &&
      projection(metadata.globals, "event") != metadata.event_globals;
  auto result = metadata.globals;
  // Without an importer snapshot, the current declarations provide the best
  // available size baseline. This keeps repeated scopes stable for manually
  // assembled metadata while retaining the old flattened-vector fallback.
  if (trace_changed)
    result = redistribute_scope(std::move(result), "trace",
                                metadata.trace_globals, metadata.globals);
  if (event_changed)
    result = redistribute_scope(std::move(result), "event",
                                metadata.event_globals, metadata.globals);
  return result;
}

bool classifier_key_needs_quotes(std::string_view key) {
  if (key.empty()) return true;
  for (const char character : key) {
    if (std::isspace(static_cast<unsigned char>(character)) ||
        character == '\'' || character == '"' || character == '\\') {
      return true;
    }
  }
  return key.front() == '\'' || key.front() == '"';
}

std::string serialize_classifier_keys(const std::vector<std::string>& keys) {
  std::string serialized;
  for (const auto& key : keys) {
    if (!serialized.empty()) serialized.push_back(' ');
    if (!classifier_key_needs_quotes(key)) {
      serialized += key;
      continue;
    }
    serialized.push_back('\'');
    for (const char character : key) {
      if (character == '\'' || character == '\\') serialized.push_back('\\');
      serialized.push_back(character);
    }
    serialized.push_back('\'');
  }
  return serialized;
}

void write_classifier(std::ostream& out, const LogClassifier& classifier) {
  out << "  <classifier name=\"";
  write_xml_escaped(out, classifier.name);
  out << "\" keys=\"";
  write_xml_escaped(out, serialize_classifier_keys(classifier.keys));
  out << "\"/>\n";
}

}  // namespace

XesExportVariant parse_xes_export_variant(std::string_view value) {
  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  if (normalized == "line_by_line") return XesExportVariant::kLineByLine;
  if (normalized == "etree") return XesExportVariant::kElementTree;
  throw std::invalid_argument(
      "XES export variant must be line_by_line or etree");
}

std::string_view xes_export_variant_name(XesExportVariant variant) {
  switch (variant) {
    case XesExportVariant::kLineByLine: return "line_by_line";
    case XesExportVariant::kElementTree: return "etree";
  }
  throw std::invalid_argument("XES export: unsupported variant");
}

bool present(FirstClassAttributeState state, std::string_view value) {
  return state != FirstClassAttributeState::kMissing || !value.empty();
}

std::string projection_key(std::string_view source_key,
                           std::string_view canonical_key) {
  return source_key.empty() ? std::string(canonical_key)
                            : std::string(source_key);
}

void add_key_prefix(std::set<std::string>& prefixes, std::string_view key) {
  const auto separator = key.find(':');
  if (separator != std::string_view::npos && separator != 0)
    prefixes.emplace(key.substr(0, separator));
}

void add_attribute_prefixes(std::set<std::string>& prefixes,
                            const std::vector<EventAttribute>& attributes);

void add_attribute_prefix(std::set<std::string>& prefixes,
                          const EventAttribute& attribute) {
  add_key_prefix(prefixes, attribute.key);
  add_attribute_prefixes(prefixes, attribute.children);
}

void add_attribute_prefixes(std::set<std::string>& prefixes,
                            const std::vector<EventAttribute>& attributes) {
  for (const auto& attribute : attributes)
    add_attribute_prefix(prefixes, attribute);
}

void add_projection_prefixes(std::set<std::string>& prefixes,
                             const Event& event) {
  if (present(event.activity_state, event.activity))
    add_key_prefix(prefixes,
                   projection_key(event.activity_source_key, "concept:name"));
  if (present(event.timestamp_state, event.timestamp))
    add_key_prefix(prefixes, projection_key(event.timestamp_source_key,
                                             "time:timestamp"));
  if (present(event.resource_state, event.resource))
    add_key_prefix(prefixes,
                   projection_key(event.resource_source_key, "org:resource"));
  if (present(event.lifecycle_state, event.lifecycle))
    add_key_prefix(prefixes, projection_key(event.lifecycle_source_key,
                                             "lifecycle:transition"));
}

void write_projection(std::ostream& out, std::string_view indent,
                       std::string_view key, std::string_view value,
                       AttributeKind kind, FirstClassAttributeState state,
                       XesExportVariant variant, bool preserve_nulls,
                       const EventAttribute* nested_metadata = nullptr) {
  EventAttribute attribute{std::string(key), std::string(value), kind,
                           state == FirstClassAttributeState::kNull};
  if (nested_metadata) {
    attribute.children = nested_metadata->children;
    attribute.scalar_wrapper = nested_metadata->scalar_wrapper;
    attribute.scalar_children_list = nested_metadata->scalar_children_list;
    // The parent container is part of the selected attribute's type. Keep it
    // even when the convenient first-class text projection is used for value.
    attribute.container = nested_metadata->container;
    if (nested_metadata->value == value &&
        nested_metadata->is_null == attribute.is_null) {
      attribute.kind = nested_metadata->kind;
      attribute.typed_value = nested_metadata->typed_value;
    }
  }
  write_typed_attribute(out, indent, attribute, variant, preserve_nulls);
}

const EventAttribute* nested_projection_metadata(
    const std::vector<EventAttribute>& attributes, std::string_view key) {
  const auto found = std::find_if(
      attributes.begin(), attributes.end(), [&](const auto& attribute) {
        return attribute.key == key &&
               (attribute.scalar_wrapper || attribute.scalar_children_list || !attribute.children.empty());
      });
  return found == attributes.end() ? nullptr : &*found;
}

const EventAttribute* first_class_metadata(
    const std::optional<EventAttribute>& selected,
    const std::vector<EventAttribute>& attributes, std::string_view key) {
  if (selected) return &*selected;
  return nested_projection_metadata(attributes, key);
}

bool is_selected_copy(const EventAttribute& attribute,
                      const EventAttribute* selected,
                      std::string_view key) {
  return selected && attribute.key == key &&
         attribute.value == selected->value &&
         attribute.is_null == selected->is_null &&
         attribute.kind == selected->kind &&
         attribute.container == selected->container &&
         attribute.scalar_wrapper == selected->scalar_wrapper &&
         attribute.scalar_children_list == selected->scalar_children_list &&
         attribute.children == selected->children;
}

void write_event_attributes(std::ostream& out, const Event& event, XesExportVariant variant, bool preserve_nulls) {
  std::set<const EventAttribute*> consumed;
  if (present(event.activity_state, event.activity)) {
    const auto key =
        projection_key(event.activity_source_key, "concept:name");
    const auto* nested =
        first_class_metadata(event.activity_attribute, event.attributes, key);
    write_projection(out, "      ", key,
                     event.activity,
                     AttributeKind::kString, event.activity_state, variant, preserve_nulls, nested);
    if (nested && !event.activity_attribute) consumed.insert(nested);
  }
  if (present(event.timestamp_state, event.timestamp)) {
    const auto key =
        projection_key(event.timestamp_source_key, "time:timestamp");
    const auto* nested = first_class_metadata(event.timestamp_attribute,
                                              event.attributes, key);
    write_projection(out, "      ", key,
                     event.timestamp,
                     event.timestamp_state == FirstClassAttributeState::kNull ||
                             (!event.timestamp.empty() &&
                               parse_timestamp(event.timestamp))
                          ? AttributeKind::kDate
                          : AttributeKind::kString,
                     event.timestamp_state, variant, preserve_nulls, nested);
    if (nested && !event.timestamp_attribute) consumed.insert(nested);
  }
  if (present(event.resource_state, event.resource)) {
    const auto key =
        projection_key(event.resource_source_key, "org:resource");
    const auto* nested =
        first_class_metadata(event.resource_attribute, event.attributes, key);
    write_projection(out, "      ", key,
                     event.resource,
                     AttributeKind::kString, event.resource_state, variant, preserve_nulls, nested);
    if (nested && !event.resource_attribute) consumed.insert(nested);
  }
  if (present(event.lifecycle_state, event.lifecycle)) {
    const auto key =
        projection_key(event.lifecycle_source_key, "lifecycle:transition");
    const auto* nested = first_class_metadata(event.lifecycle_attribute,
                                              event.attributes, key);
    write_projection(out, "      ", key,
                     event.lifecycle,
                     AttributeKind::kString, event.lifecycle_state, variant, preserve_nulls, nested);
    if (nested && !event.lifecycle_attribute) consumed.insert(nested);
  }

  // Imported first-class attributes are removed from the generic bag after
  // selector ranking.  Any attributes left under the same key are genuine
  // conflicts and must remain visible; this also preserves manually-built
  // logs whose generic bag intentionally contains a duplicate projection.
  for (const auto& attribute : event.attributes) {
    const auto selected_copy =
        (event.activity_attribute &&
         is_selected_copy(attribute, &*event.activity_attribute,
                          projection_key(event.activity_source_key,
                                         "concept:name"))) ||
        (event.timestamp_attribute &&
         is_selected_copy(attribute, &*event.timestamp_attribute,
                          projection_key(event.timestamp_source_key,
                                         "time:timestamp"))) ||
        (event.resource_attribute &&
         is_selected_copy(attribute, &*event.resource_attribute,
                          projection_key(event.resource_source_key,
                                         "org:resource"))) ||
        (event.lifecycle_attribute &&
         is_selected_copy(attribute, &*event.lifecycle_attribute,
                          projection_key(event.lifecycle_source_key,
                                         "lifecycle:transition")));
    if (!consumed.contains(&attribute) && !selected_copy)
      write_typed_attribute(out, "      ", attribute, variant, preserve_nulls);
  }
}

const EventAttribute* stored_log_name(const EventLog& log) {
  for (const auto& attribute : log.attributes) {
    if (attribute.key == "concept:name") return &attribute;
  }
  return nullptr;
}

void write_xes_with_options(std::ostream& out, const EventLog& log,
                            std::string_view requested_name,
                            bool infer_metadata, XesExportVariant variant, bool preserve_nulls) {
  const auto globals = effective_globals(log.metadata);
  std::set<std::string> inferred_prefixes;
  if (infer_metadata) {
    // The inferred trace/event concept:name defaults always require the
    // standard Concept declaration, even for an otherwise empty log.
    inferred_prefixes.emplace("concept");
    add_attribute_prefixes(inferred_prefixes, log.attributes);
    add_attribute_prefixes(inferred_prefixes, log.metadata.trace_globals);
    add_attribute_prefixes(inferred_prefixes, log.metadata.event_globals);
    for (const auto& global : log.metadata.globals)
      add_attribute_prefixes(inferred_prefixes, global.attributes);
    for (const auto& trace : log.traces) {
      add_attribute_prefixes(inferred_prefixes, trace.attributes);
      if (present(trace.case_id_state, trace.case_id))
        add_key_prefix(inferred_prefixes,
                       projection_key(trace.case_id_source_key,
                                      "concept:name"));
      for (const auto& event : trace.events) {
        add_attribute_prefixes(inferred_prefixes, event.attributes);
        add_projection_prefixes(inferred_prefixes, event);
      }
    }
  }

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<log xes.version=\"1849-2016\" "
         "xes.features=\"nested-attributes\" "
         "xmlns=\"http://www.xes-standard.org/\">\n";
  for (const auto& extension : log.metadata.extensions) {
    write_extension(out, extension);
  }
  if (infer_metadata) {
    for (const auto& extension : kStandardExtensions) {
      if (!inferred_prefixes.contains(std::string(extension.prefix))) continue;
      const LogExtension generated{std::string(extension.name),
                                   std::string(extension.prefix),
                                   std::string(extension.uri)};
      const auto found = std::any_of(
          log.metadata.extensions.begin(), log.metadata.extensions.end(),
          [&](const auto& stored) { return extension_matches(stored, generated); });
      if (!found) write_extension(out, generated);
    }
  }

  if (!globals.empty()) {
    if (infer_metadata) {
      const auto completed = complete_inferred_globals(globals);
      for (const auto& global : completed)
        write_global(out, global.scope, global.attributes, false, variant, preserve_nulls);
    } else {
      for (const auto& global : globals)
        write_global(out, global.scope, global.attributes, false, variant, preserve_nulls);
    }
  } else {
    if (!log.metadata.trace_globals.empty())
      write_global(out, "trace", log.metadata.trace_globals, infer_metadata, variant, preserve_nulls);
    else if (infer_metadata)
      write_global(out, "trace", {}, true, variant, preserve_nulls);
    if (!log.metadata.event_globals.empty())
      write_global(out, "event", log.metadata.event_globals, infer_metadata, variant, preserve_nulls);
    else if (infer_metadata)
      write_global(out, "event", {}, true, variant, preserve_nulls);
  }

  for (const auto& classifier : log.metadata.classifiers) {
    write_classifier(out, classifier);
  }
  if (infer_metadata && !has_classifier_name(log.metadata.classifiers, "Activity")) {
    write_classifier(out, {"Activity", {"concept:name"}});
  }
  const EventAttribute* stored_name = stored_log_name(log);
  if (!requested_name.empty()) {
    write_string_attribute(out, "  ", "concept:name", requested_name);
  } else if (stored_name) {
    write_typed_attribute(out, "  ", *stored_name, variant, preserve_nulls);
  } else if (infer_metadata) {
    write_string_attribute(out, "  ", "concept:name", "log");
  }
  bool skipped_stored_name = false;
  for (const auto& attribute : log.attributes) {
    if (!skipped_stored_name &&
        ((stored_name && &attribute == stored_name) ||
         (!stored_name && !requested_name.empty() &&
          attribute.key == "concept:name"))) {
      skipped_stored_name = true;
      continue;
    }
    write_typed_attribute(out, "  ", attribute, variant, preserve_nulls);
  }

  for (const auto& trace : log.traces) {
    out << "  <trace>\n";
    const auto case_key =
        projection_key(trace.case_id_source_key, "concept:name");
    const auto* nested_case = first_class_metadata(
        trace.case_id_attribute, trace.attributes, case_key);
    if (present(trace.case_id_state, trace.case_id))
      write_projection(out, "    ", case_key, trace.case_id,
                       AttributeKind::kString, trace.case_id_state,
                       variant, preserve_nulls, nested_case);
    for (const auto& attribute : trace.attributes) {
      if (&attribute != nested_case &&
          !is_selected_copy(attribute,
                            trace.case_id_attribute
                                ? &*trace.case_id_attribute
                                : nullptr,
                            case_key))
        write_typed_attribute(out, "    ", attribute, variant, preserve_nulls);
    }
    for (const auto& event : trace.events) {
      out << "    <event>\n";
      write_event_attributes(out, event, variant, preserve_nulls);
      out << "    </event>\n";
    }
    out << "  </trace>\n";
  }
  out << "</log>\n";
}

void write_xes(std::ostream& out, const EventLog& log, std::string_view name) {
  write_xes_with_options(out, log, name, false, XesExportVariant::kLineByLine, false);
}

std::string to_xes(const EventLog& log, std::string_view name) {
  std::ostringstream out;
  write_xes(out, log, name);
  return out.str();
}

std::string serialize_xes(const EventLog& log,
                           const XesExportOptions& options) {
  (void)xes_export_variant_name(options.variant);
  std::ostringstream out;
  write_xes_with_options(out, log, options.name, options.infer_metadata, options.variant, options.preserve_nulls);
  const auto xml = out.str();
  return options.compress.value_or(false) ? detail::gzip_compress(xml) : xml;
}

std::filesystem::path export_xes_event_log(
    const std::filesystem::path& path, const EventLog& log,
    const XesExportOptions& options) {
  (void)xes_export_variant_name(options.variant);
  const auto lower_path = [&] {
    auto value = path.string();
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
    return value;
  }();
  const bool suffix_compressed = lower_path.ends_with(".gz");
  const bool compressed = options.compress.value_or(suffix_compressed);
  std::filesystem::path output_path = path;
  if (compressed && !suffix_compressed) output_path += ".gz";
  XesExportOptions effective = options;
  effective.compress = compressed;
  const auto bytes = serialize_xes(log, effective);
  std::ofstream output(output_path, std::ios::binary);
  if (!output)
    throw std::runtime_error("Unable to open XES output: " +
                             output_path.string());
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("Unable to write XES output: " +
                             output_path.string());
  return output_path;
}

}  // namespace minera
