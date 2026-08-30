#include "minera/xes_export.hpp"

#include "gzip_codec.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
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
                            std::string_view key, std::string_view value) {
  out << indent << "<string key=\"";
  write_xml_escaped(out, key);
  out << "\" value=\"";
  write_xml_escaped(out, value);
  out << "\"/>\n";
}

std::string_view element_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
  }
  return "string";
}

// Writes a generic event or case attribute under its imported XES type,
// normalizing dates like the timestamp; a date that no longer parses degrades
// to a string attribute rather than emitting an invalid xs:dateTime.
void write_typed_attribute(std::ostream& out, std::string_view indent,
                           const EventAttribute& attribute) {
  // XES scalar elements have no null representation.  Empty strings are
  // valid values and are intentionally emitted; only explicit nulls vanish.
  if (attribute.is_null) return;
  if (attribute.kind == AttributeKind::kDate) {
    if (const auto instant = parse_timestamp(attribute.value)) {
      out << indent << "<date key=\"";
      write_xml_escaped(out, attribute.key);
      out << "\" value=\"" << format_timestamp(*instant) << "\"/>\n";
      return;
    }
    write_string_attribute(out, indent, attribute.key, attribute.value);
    return;
  }
  out << indent << '<' << element_name(attribute.kind) << " key=\"";
  write_xml_escaped(out, attribute.key);
  out << "\" value=\"";
  write_xml_escaped(out, attribute.value);
  out << "\"/>\n";
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
  // Generated attributes keep their canonical key prefixes, so only a stored
  // declaration for that exact prefix can make the generated declaration
  // redundant. A matching display name or URI with a different prefix does
  // not declare the keys emitted below.
  return !stored.prefix.empty() && stored.prefix == generated.prefix;
}

bool has_classifier_name(const std::vector<LogClassifier>& classifiers,
                         std::string_view name) {
  return std::any_of(classifiers.begin(), classifiers.end(),
                     [&](const auto& classifier) {
                       return classifier.name == name;
                     });
}

bool is_event_projection_key(std::string_view key) {
  return key == "concept:name" || key == "time:timestamp" ||
         key == "org:resource" || key == "lifecycle:transition";
}

void write_global(std::ostream& out, std::string_view scope,
                  const std::vector<EventAttribute>& attributes) {
  out << "  <global scope=\"";
  write_xml_escaped(out, scope);
  out << "\">\n";
  bool has_concept_name = false;
  for (const auto& attribute : attributes) {
    has_concept_name = has_concept_name ||
                       (attribute.key == "concept:name" &&
                        !attribute.is_null);
    write_typed_attribute(out, "    ", attribute);
  }
  if (!has_concept_name) {
    write_string_attribute(out, "    ", "concept:name", "");
  }
  out << "  </global>\n";
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

void write_xes(std::ostream& out, const EventLog& log, std::string_view name) {
  bool any_resource = false;
  bool any_lifecycle = false;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      any_resource = any_resource || !event.resource.empty();
      any_lifecycle = any_lifecycle || !event.lifecycle.empty();
    }
    if (any_resource && any_lifecycle) break;
  }

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<log xes.version=\"1.0\" xes.features=\"\" "
         "xmlns=\"http://www.xes-standard.org/\">\n";
  const std::vector<LogExtension> generated_extensions = {
      {"Concept", "concept", "http://www.xes-standard.org/concept.xesext"},
      {"Time", "time", "http://www.xes-standard.org/time.xesext"},
  };
  for (const auto& extension : log.metadata.extensions) {
    write_extension(out, extension);
  }
  for (const auto& extension : generated_extensions) {
    const auto found = std::any_of(
        log.metadata.extensions.begin(), log.metadata.extensions.end(),
        [&](const auto& stored) { return extension_matches(stored, extension); });
    if (!found) write_extension(out, extension);
  }
  if (any_resource) {
    const LogExtension extension{
        "Organizational", "org", "http://www.xes-standard.org/org.xesext"};
    const auto found = std::any_of(
        log.metadata.extensions.begin(), log.metadata.extensions.end(),
        [&](const auto& stored) { return extension_matches(stored, extension); });
    if (!found) write_extension(out, extension);
  }
  if (any_lifecycle) {
    const LogExtension extension{
        "Lifecycle", "lifecycle",
        "http://www.xes-standard.org/lifecycle.xesext"};
    const auto found = std::any_of(
        log.metadata.extensions.begin(), log.metadata.extensions.end(),
        [&](const auto& stored) { return extension_matches(stored, extension); });
    if (!found) write_extension(out, extension);
  }

  if (log.metadata.trace_globals.empty()) {
    write_global(out, "trace", {});
  } else {
    write_global(out, "trace", log.metadata.trace_globals);
  }
  if (log.metadata.event_globals.empty()) {
    write_global(out, "event", {});
  } else {
    write_global(out, "event", log.metadata.event_globals);
  }

  for (const auto& classifier : log.metadata.classifiers) {
    write_classifier(out, classifier);
  }
  if (!has_classifier_name(log.metadata.classifiers, "Activity")) {
    write_classifier(out, {"Activity", {"concept:name"}});
  }
  write_string_attribute(out, "  ", "concept:name", name);
  for (const auto& attribute : log.attributes) {
    // Avoid two log-name attributes when a caller constructed one manually;
    // the explicit `name` argument remains authoritative.
    if (attribute.key != "concept:name") {
      write_typed_attribute(out, "  ", attribute);
    }
  }

  for (const auto& trace : log.traces) {
    out << "  <trace>\n";
    write_string_attribute(out, "    ", "concept:name", trace.case_id);
    for (const auto& attribute : trace.attributes) {
      // The first-class case id is always emitted above.  A manually supplied
      // canonical trace attribute must not create a duplicate XES key.
      if (attribute.key != "concept:name")
        write_typed_attribute(out, "    ", attribute);
    }
    for (const auto& event : trace.events) {
      out << "    <event>\n";
      write_string_attribute(out, "      ", "concept:name", event.activity);
      if (!event.timestamp.empty()) {
        if (const auto instant = parse_timestamp(event.timestamp)) {
          out << "      <date key=\"time:timestamp\" value=\""
              << format_timestamp(*instant) << "\"/>\n";
        } else {
          // Not a recognizable instant: keep the original text as a string
          // attribute rather than emitting an invalid xs:dateTime or losing
          // the value.
          write_string_attribute(out, "      ", "time:timestamp",
                                 event.timestamp);
        }
      }
      if (!event.resource.empty()) {
        write_string_attribute(out, "      ", "org:resource", event.resource);
      }
      if (!event.lifecycle.empty()) {
        write_string_attribute(out, "      ", "lifecycle:transition",
                               event.lifecycle);
      }
      for (const auto& attribute : event.attributes) {
        // First-class projections above are authoritative for canonical XES
        // keys; generic aliases remain available for custom consumers.
        if (!is_event_projection_key(attribute.key))
          write_typed_attribute(out, "      ", attribute);
      }
      out << "    </event>\n";
    }
    out << "  </trace>\n";
  }
  out << "</log>\n";
}

std::string to_xes(const EventLog& log, std::string_view name) {
  std::ostringstream out;
  write_xes(out, log, name);
  return out.str();
}

std::string serialize_xes(const EventLog& log,
                          const XesExportOptions& options) {
  (void)xes_export_variant_name(options.variant);
  const auto xml = to_xes(log, options.name);
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
