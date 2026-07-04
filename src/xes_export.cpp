#include "minera/xes_export.hpp"

#include <cmath>
#include <cstdio>
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

// Formats seconds since the Unix epoch as ISO-8601 UTC with millisecond
// precision ("2024-01-01T10:00:00.000Z"), the xs:dateTime shape XES tools
// expect. Uses the days-to-civil-date algorithm directly so the result is
// independent of the process locale and timezone.
std::string format_utc(double epoch_seconds) {
  long long milliseconds = std::llround(epoch_seconds * 1000.0);
  long long seconds = milliseconds / 1000;
  long long milli = milliseconds % 1000;
  if (milli < 0) {
    milli += 1000;
    --seconds;
  }
  long long days = seconds / 86400;
  long long of_day = seconds % 86400;
  if (of_day < 0) {
    of_day += 86400;
    --days;
  }

  days += 719468;  // shift epoch from 1970-01-01 to 0000-03-01
  const long long era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned day = doy - (153 * mp + 2) / 5 + 1;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  const long long year = static_cast<long long>(yoe) + era * 400 + (month <= 2);

  char buffer[40];
  std::snprintf(buffer, sizeof(buffer),
                "%04lld-%02u-%02uT%02lld:%02lld:%02lld.%03lldZ", year, month,
                day, of_day / 3600, (of_day / 60) % 60, of_day % 60, milli);
  return buffer;
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
              << format_utc(*instant) << "\"/>\n";
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
