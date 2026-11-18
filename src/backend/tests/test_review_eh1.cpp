// Review EH1: pm4py 2.7.22 on NaT event times from the OCEL CSV and OCEL 1.0
// XML readers, a time column of None (an OCEL 1.0 XML file without any
// timestamp element), and the writers' handling of events and objects
// without relations. Observed black-box (review-harness round-eh: csv/
// cgen3.py and xg/xgen.py files through rt/rsw.py and rt/xsw.py, w/ and w2/
// writer probes, p2/ hand probes):
//
// - read_ocel_csv reads an empty time cell, a read_csv NA spelling and
//   to_datetime's NaT spellings as NaT, sorts them after every time in file
//   order, and sorts the relations frame by time too (stably, NaT last);
// - the edge association performance measures a pair with a NaT end as 0;
//   with business hours it raises "cannot convert float NaN to integer";
// - read_ocel_xml on a file whose events all lack the timestamp element
//   holds a time column of None, where the time routes raise other errors
//   than NaT gives (AttributeError on .timestamp/.isoformat/.replace,
//   TypeError on - and <);
// - every writer leaves out the events and objects without an event-object
//   relation, with their O2O links and object changes;
// - a NaT time keeps the column's shared offset in the writers.
//
// See docs/reference-2.7.22-parity.md, Review EH1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
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

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

std::string event_ids(const Ocel& log) {
  std::string out;
  for (const auto& event : log.events) out += event.id + (event.timestamp.empty() ? "-" : "") + ",";
  return out;
}

const std::string kCsv =
    "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n"
    "e1,2024-01-01 10:00:00+02:00,a,['o1']\n"
    "e2,,b,['o1']\n"
    "e3,NaT,c,['o2']\n"
    "e4,2024-01-01 09:00:00+02:00,d,\"['o1', 'o2']\"\n"
    "e5,2024-01-01 11:00:00+02:00,x,[]\n";

// pm4py's write_ocel_csv of that file: e5 has no relation.
const std::string kCsvWritten =
    "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n"
    "e4,2024-01-01 09:00:00+02:00,d,\"['o1', 'o2']\"\n"
    "e1,2024-01-01 10:00:00+02:00,a,['o1']\n"
    "e2,,b,['o1']\n"
    "e3,,c,['o2']\n";

void csv_nat_times() {
  const auto log = parse_ocel_csv(kCsv);
  expect(event_ids(log) == "e4,e1,e5,e2-,e3-,", "NaT times last in file order: " + event_ids(log));
  std::string relations;
  for (const auto& relation : log.relations) relations += relation.event_id + ">" + relation.object_id + ",";
  expect(relations == "e4>o1,e4>o2,e1>o1,e2>o1,e3>o2,", "relations in time order: " + relations);
  // A guessed format reads an NA spelling as NaT too.
  const auto guessed = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,01/02/2024,a,['o1']\ne2,null,a,['o1']\n");
  expect(event_ids(guessed) == "e1,e2-,", "guessed NA time: " + event_ids(guessed));
  // read_csv's NA spellings and to_datetime's NaT spellings.
  for (const std::string cell : {"nan", "NaN", "NAN", "nat", "NAT", "NULL", "n/a", "None"}) {
    const auto spelled = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                                        "e1," + cell + ",a,['o1']\ne2,2024-01-01 10:00:00,b,['o1']\n");
    expect(event_ids(spelled) == "e2,e1-,", "NaT spelling " + cell + ": " + event_ids(spelled));
  }
  // An all-NaT column is datetime64[s].
  const auto all = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,,a,['o1']\n");
  expect(all.naive_event_time_unit == "s", "all-NaT unit: " + all.naive_event_time_unit);
}

void csv_nat_routes_and_writers() {
  const auto log = parse_ocel_csv(kCsv);
  const auto edges = edge_associations(log);
  const auto& order = edges.at("order");
  expect(order.at({"d", "a"}).performance == std::vector<double>{3600.0}, "d->a 3600 s");
  expect(order.at({"a", "b"}).performance == std::vector<double>{0.0}, "a->b with a NaT end is 0");
  expect(order.at({"d", "c"}).performance == std::vector<double>{0.0}, "d->c with a NaT end is 0");
  OcelEdgeAssociationOptions business;
  business.use_business_hours = true;
  expect(error_of([&] { (void)edge_associations(log, business); }) ==
             "ValueError: cannot convert float NaN to integer",
         "business hours on a NaT pair");
  // The writers leave out e5 (no relation); the NaT times keep the offset.
  const auto csv = serialize_ocel_csv(log);
  expect(csv == kCsvWritten, "CSV download:\n" + csv);
  const auto routed = parse_ocel_json(serialize_ocel_json(log));
  expect(event_ids(routed) == "e4,e1,e5,e2-,e3-,", "the transport keeps e5: " + event_ids(routed));
  expect(serialize_ocel_csv(routed) == kCsvWritten, "CSV download after a route");
  const auto classic = serialize_ocel_json(routed, OcelJsonFormat::kClassic, false);
  expect(classic.find("\"e5\"") == std::string::npos && classic.find("keepUnrelated") == std::string::npos,
         "classic JSON download without e5");
  const auto xml = serialize_ocel_xml(routed);
  expect(xml.find("\"e5\"") == std::string::npos, "OCEL 1.0 XML download without e5");
}

// An OCEL 1.0 XML file whose events have no timestamp element: pm4py's
// events frame holds a time column of None.
const std::string kNoneXml =
    "<?xml version='1.0' encoding='UTF-8'?>\n<log>\n"
    "<global scope=\"event\"><string key=\"ocel:activity\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"object\"><string key=\"ocel:type\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"log\"><list key=\"attribute-names\"/><list key=\"object-types\"><string "
    "key=\"object-type\" value=\"t\"/></list><string key=\"version\" value=\"1.0\"/><string "
    "key=\"ordering\" value=\"timestamp\"/></global>\n<events>\n"
    "<event><string key=\"id\" value=\"e1\"/><string key=\"activity\" value=\"a\"/><list "
    "key=\"omap\"><string key=\"object-id\" value=\"o1\"/><string key=\"object-id\" value=\"o2\"/>"
    "</list><list key=\"vmap\"/></event>\n"
    "<event><string key=\"id\" value=\"e2\"/><string key=\"activity\" value=\"b\"/><list "
    "key=\"omap\"><string key=\"object-id\" value=\"o1\"/></list><list key=\"vmap\"/></event>\n"
    "</events>\n<objects>\n"
    "<object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\"/></object>\n"
    "<object><string key=\"id\" value=\"o2\"/><string key=\"type\" value=\"u\"/><list key=\"ovmap\"/></object>\n"
    "</objects>\n</log>\n";

void none_time_column() {
  const auto read = parse_ocel_xml(kNoneXml);
  expect(read.none_event_times, "a file without timestamps holds None times");
  const auto log = parse_ocel_json(serialize_ocel_json(read));
  expect(log.none_event_times, "the transport keeps the None column");
  expect(serialize_ocel_json(log, OcelJsonFormat::kClassic, false).find("noneEventTimes") ==
             std::string::npos,
         "a download leaves the marker out");
  expect(error_of([&] { (void)describe_ocel_tuples(log); }) ==
             "AttributeError: 'NoneType' object has no attribute 'timestamp'",
         "description of None times");
  OcelXmlExportOptions standard;
  standard.format = OcelXmlFormat::kStandard;
  expect(error_of([&] { (void)serialize_ocel_xml(log, standard); }) ==
             "AttributeError: 'NoneType' object has no attribute 'isoformat'",
         "OCEL 2.0 XML of None times");
  expect(error_of([&] { (void)edge_associations(log); }) ==
             "TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'",
         "edge performance of None times");
  OcelEdgeAssociationOptions business;
  business.use_business_hours = true;
  expect(error_of([&] { (void)edge_associations(log, business); }) ==
             "AttributeError: 'NoneType' object has no attribute 'replace'",
         "business hours on None times");
  OcelSplitOptions split;
  split.variant = OcelSplitVariant::kAncestorsDescendants;
  split.object_type = "t";
  expect(error_of([&] { (void)split_ocel(log, split); }) ==
             "TypeError: '<' not supported between instances of 'NoneType' and 'NoneType'",
         "ancestors/descendants on None times");
  // One timestamp element is enough for a datetime column (NaT, not None).
  std::string one = kNoneXml;
  const std::string anchor = "<string key=\"activity\" value=\"b\"/>";
  one.insert(one.find(anchor) + anchor.size(),
             "<date key=\"timestamp\" value=\"2024-01-01T10:00:00Z\"/>");
  const auto dated = parse_ocel_xml(one);
  expect(!dated.none_event_times && event_ids(dated) == "e2,e1-,", "one time: " + event_ids(dated));
  // o2 has only e1 (NaT): the NaT error, not the None one.
  expect(error_of([&] { (void)describe_ocel_tuples(dated); }) ==
             "ValueError: NaTType does not support timestamp",
         "one time: NaT error");
}

void writers_leave_out_unrelated() {
  Ocel log;
  log.events = {{"e1", "a", "2023-01-01T10:00:00+00:00", {}},
                {"e2", "b", "2023-01-01T11:00:00+00:00", {}},
                {"e3", "c", "2023-01-01T12:00:00+00:00", {}}};
  log.objects = {{"o1", "order", {}}, {"o2", "order", {}}, {"o9", "ghost", {}}};
  log.relations = {{"e1", "o1", "q"}, {"e2", "o2", "q"}};
  log.o2o = {{"o1", "o9", "x"}, {"o9", "o2", "y"}};
  log.object_changes = {{"o9", "ghost", "2023-01-02T00:00:00+00:00", "f", std::string("v9")},
                        {"o1", "order", "2023-01-03T00:00:00+00:00", "f", std::string("v1")}};
  expect(ocel_has_unrelated_entries(log), "e3 and o9 have no relation");
  std::optional<Ocel> storage;
  const auto& written = ocel_written_entries(log, storage);
  expect(event_ids(written) == "e1,e2,", "written events");
  expect(written.objects.size() == 2 && written.o2o.empty() && written.object_changes.size() == 1 &&
             written.object_changes[0].object_id == "o1",
         "o9 goes with its links and changes");
  expect(log.events.size() == 3 && log.objects.size() == 3, "the log itself stays");
  const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
  expect(standard.find("o9") == std::string::npos && standard.find("\"e3\"") == std::string::npos &&
             standard.find("ghost") == std::string::npos && standard.find("v1") != std::string::npos,
         "OCEL 2.0 JSON download: " + standard);
  OcelXmlExportOptions xml;
  xml.format = OcelXmlFormat::kStandard;
  xml.allow_lossy = true;
  const auto xml2 = serialize_ocel_xml(log, xml);
  expect(xml2.find("o9") == std::string::npos && xml2.find("\"e3\"") == std::string::npos,
         "OCEL 2.0 XML download");
  // The transport keeps them.
  const auto transport = serialize_ocel_json(log, OcelJsonFormat::kStandard);
  expect(transport.find("o9") != std::string::npos && transport.find("keepUnrelated") != std::string::npos,
         "the transport keeps o9");
  std::optional<Ocel> same;
  expect(&ocel_written_entries(written, same) == &written && !same, "nothing to drop: no copy");
}

}  // namespace

int main() {
  csv_nat_times();
  csv_nat_routes_and_writers();
  none_time_column();
  writers_leave_out_unrelated();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EH1 tests passed\n";
  return 0;
}
