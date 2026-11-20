// Review EU1: pm4py 2.7.22's OCEL 2.0 XML reader on event times with offsets
// of a day or more, and its edge associations on repeated relation rows
// (review-harness round-eu: hc/x1 runs the XML cases through pm4py, bank
// d904 the repeated rows). Observed black-box:
//
// - read_ocel2_xml reads a time fromisoformat refuses through dateutil,
//   which keeps an offset of a day or more, and builds its events frame of
//   every event it read (also those it drops later: no id, no type,
//   unrelated). pandas keeps a time column whose values all share one such
//   offset, and raises the datetime module's "offset must be a timedelta
//   strictly between -timedelta(hours=24) and timedelta(hours=24)." when the
//   column mixes one with any other zone or with naive times; after the
//   errors of the walk over the file and after the error of a file without
//   relations. Date attributes and change times with such offsets read;
// - find_associations_per_edge keeps a (source, target, object) triple once
//   per pair of consecutive rows of the object, so an object an event lists
//   three times gives two self-edge triples; the aggregated sets (event
//   couples, event objects) and their performances keep one.
//
// See docs/reference-2.7.22-parity.md, Review EU1.
#include "minera/ocel.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_xml.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

const std::string kOffsetError =
    "offset must be a timedelta strictly between -timedelta(hours=24) and timedelta(hours=24).";

struct XmlEvent {
  std::string id;
  std::string time;
  bool related = true;
  std::string type = "x";
  std::string date;  // the "date"-declared attribute t, if any
};

// An OCEL 2.0 XML file with one object o1 (date attribute p, string s) and
// the given events (probe eu x1 xoff3.py/xoff5.py).
std::string xml_log(const std::vector<XmlEvent>& events, const std::string& object_attributes = {}) {
  std::string out =
      "<?xml version='1.0' encoding='UTF-8'?>\n<log><object-types><object-type name=\"A\"><attributes>"
      "<attribute name=\"p\" type=\"date\"/><attribute name=\"s\" type=\"string\"/></attributes>"
      "</object-type></object-types><event-types><event-type name=\"x\"><attributes>"
      "<attribute name=\"t\" type=\"date\"/></attributes></event-type></event-types>"
      "<objects><object id=\"o1\" type=\"A\"><attributes>" +
      object_attributes + "</attributes></object></objects><events>";
  for (const auto& event : events) {
    out += "<event id=\"" + event.id + "\" type=\"" + event.type + "\" time=\"" + event.time + "\">";
    if (!event.date.empty()) out += "<attributes><attribute name=\"t\">" + event.date + "</attribute></attributes>";
    out += "<objects>";
    if (event.related) out += "<relationship object-id=\"o1\" qualifier=\"q\"/>";
    out += "</objects></event>";
  }
  return out + "</events></log>";
}

std::string read_error(const std::vector<XmlEvent>& events, const std::string& object_attributes = {}) {
  return error_of([&] { (void)parse_ocel_xml(xml_log(events, object_attributes)); });
}

void xml_day_offsets() {
  const std::string day = "2024-01-01T10:00:00+24:00";
  const std::string utc = "2024-01-03T10:00:00Z";
  // Next to a UTC time, a naive one or another day offset: the frame raises.
  expect(read_error({{"e1", day}, {"e2", utc}}) == kOffsetError, "day offset next to UTC raises");
  expect(read_error({{"e1", "2024-01-03T10:00:00"}, {"e2", "2024-01-01T10:00:00-25:30"}}) == kOffsetError,
         "day offset next to a naive time raises");
  expect(read_error({{"e1", day}, {"e2", "2024-01-01T10:00:00+25:00"}}) == kOffsetError,
         "two different day offsets raise");
  // Spellings dateutil reads with such an offset.
  for (const auto& spelled : {"2024-01-01 10:00:00+24:00", "2024-01-01T10:00:00.5+24:00", "2024-01-01T10:00+24:00",
                              "2024-01-01T10:00:00+2400", "2024-01-01T10:00:00+24", "2024-01-01T10:00:00+23:60",
                              "2024-01-01T10:00:00+00:1440"})
    expect(read_error({{"e1", spelled}, {"e2", utc}}) == kOffsetError, std::string("spelling raises: ") + spelled);
  // The frame holds the events the reader drops later.
  expect(read_error({{"", day}, {"e2", utc}}) == kOffsetError, "event without id still counts");
  expect(read_error({{"e1", day, true, ""}, {"e2", utc}}) == kOffsetError, "event without type still counts");
  expect(read_error({{"e1", day, false}, {"e2", utc}}) == kOffsetError, "unrelated event still counts");
  // One shared day offset reads (pandas keeps the column as it is).
  expect(read_error({{"e1", day}}).empty(), "one event with a day offset reads");
  expect(read_error({{"e1", day}, {"e2", "2024-01-02T10:00:00+2400"}}).empty(), "one shared day offset reads");
  // Offsets under a day read as before.
  expect(read_error({{"e1", "2024-01-01T10:00:00+23:59"}, {"e2", utc}}).empty(), "offset under a day reads");
  // The walk's errors and a file without relations come first.
  expect(read_error({{"e1", day}, {"e2", "garbage"}}) == "String does not contain a date: garbage",
         "a later unreadable time raises first");
  expect(read_error({{"e1", day, false}}) == "'NoneType' object has no attribute 'index'",
         "a file without relations raises first");
  // Date attributes and change times with a day offset read (declared:
  // pm4py keeps the value, its writers raise or print it their own way).
  expect(read_error({{"e1", utc, true, "x", day}, {"e2", utc}}).empty(), "event date attribute reads");
  expect(read_error({{"e1", utc}},
                    "<attribute name=\"p\" time=\"1970-01-01T00:00:00Z\">" + day + "</attribute>")
             .empty(),
         "object date attribute reads");
  expect(read_error({{"e1", utc}},
                    "<attribute name=\"s\" time=\"1970-01-01T00:00:00Z\">v</attribute>"
                    "<attribute name=\"s\" time=\"" + day + "\">w</attribute>")
             .empty(),
         "change time reads");
}

void repeated_relation_edges() {
  Ocel log;
  log.events.push_back({"e1", "a", "2024-01-01T10:00:00Z", {}});
  log.events.push_back({"e2", "b", "2024-01-01T11:00:00Z", {}});
  log.objects.push_back({"o1", "t", {}});
  log.objects.push_back({"o2", "t", {}});
  // e1 lists o1 three times (bank d904, d904_0001: "['t0', 't0', 't1', 't0']").
  log.relations = {{"e1", "o1", ""}, {"e1", "o1", ""}, {"e1", "o2", ""}, {"e1", "o1", "q"},
                   {"e2", "o1", ""}, {"e2", "o2", ""}};
  const auto result = edge_associations(log);
  const auto& self = result.at("t").at({"a", "a"});
  using Triple = std::tuple<std::string, std::string, std::string>;
  expect(self.total_objects == std::vector<Triple>{{"e1", "e1", "o1"}, {"e1", "e1", "o1"}},
         "two self-edge triples for an object listed three times");
  expect(self.event_objects == std::vector<Triple>{{"e1", "e1", "o1"}}, "event objects keep one triple");
  expect(self.event_couples.size() == 1, "event couples keep one pair");
  expect(self.performance_total_objects == std::vector<double>{0.0}, "one performance per distinct triple");
  const auto& next = result.at("t").at({"a", "b"});
  expect(next.total_objects == std::vector<Triple>{{"e1", "e2", "o1"}, {"e1", "e2", "o2"}},
         "the edges to the next event once per object");
  expect(next.performance_total_objects == std::vector<double>{3600.0, 3600.0}, "their performances");
}

}  // namespace

int main() {
  xml_day_offsets();
  repeated_relation_edges();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EU1 tests passed\n";
  return 0;
}
