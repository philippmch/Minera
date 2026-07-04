#include "minera/xes_export.hpp"

#include <sstream>

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
  out << indent << "<string key=\"" << key << "\" value=\"";
  write_xml_escaped(out, value);
  out << "\"/>\n";
}

}  // namespace

void write_xes(std::ostream& out, const EventLog& log, std::string_view name) {
  bool any_resource = false;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      if (!event.resource.empty()) {
        any_resource = true;
        break;
      }
    }
    if (any_resource) break;
  }

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<log xes.version=\"1.0\" xes.features=\"\" "
         "xmlns=\"http://www.xes-standard.org/\">\n";
  out << "  <extension name=\"Concept\" prefix=\"concept\" "
         "uri=\"http://www.xes-standard.org/concept.xesext\"/>\n";
  out << "  <extension name=\"Time\" prefix=\"time\" "
         "uri=\"http://www.xes-standard.org/time.xesext\"/>\n";
  if (any_resource) {
    out << "  <extension name=\"Organizational\" prefix=\"org\" "
           "uri=\"http://www.xes-standard.org/org.xesext\"/>\n";
  }
  out << "  <global scope=\"trace\">\n"
         "    <string key=\"concept:name\" value=\"\"/>\n"
         "  </global>\n";
  out << "  <global scope=\"event\">\n"
         "    <string key=\"concept:name\" value=\"\"/>\n"
         "  </global>\n";
  out << "  <classifier name=\"Activity\" keys=\"concept:name\"/>\n";
  write_string_attribute(out, "  ", "concept:name", name);

  for (const auto& trace : log.traces) {
    out << "  <trace>\n";
    write_string_attribute(out, "    ", "concept:name", trace.case_id);
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

}  // namespace minera
