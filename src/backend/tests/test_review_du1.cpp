// Review DU1: the reference frame shape of OCELs that routes build. pm4py
// 2.7.22's read_ocel_csv reads times without an offset as a naive
// datetime64[us] column (ns past six fraction digits, s for a file without
// rows), both playouts build naive datetime64[s] frames listed id, activity,
// timestamp, and filters, splits and enrichment keep the frames they get:
// filter_ocel_events_timestamp then raises TypeError, write_ocel_csv,
// write_ocel2_xml and write_ocel2_sqlite print the wall clock without
// offset, both JSON writers the wall clock with "Z", and a split sublog of an
// activity-first log keeps that column order. Observed black-box
// (review-harness round-du, nv/nv.py, nv/sp.py, nv/pl2.py, nv/pl3.py).
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/ocel_transform.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

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
  return {};
}

const std::string kNaive =
    "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:u\n"
    "e0,2024-01-01 10:30:00,a,['o1'],\n"
    "e1,2024-01-02 00:00:00,b,['o1'],['u1']\n"
    "e2,2024-01-03 08:00:15,c,['o1'],['u1']\n";

std::string first_line_after_header(const std::string& csv) {
  const auto start = csv.find('\n') + 1;
  return csv.substr(start, csv.find('\n', start) - start);
}

void csv_units() {
  expect(parse_ocel_csv(kNaive).naive_event_time_unit == "us", "a naive CSV reads as datetime64[us]");
  expect(parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                        "e0,2024-01-01 00:00:00.123456789,a,['o1']\n")
                 .naive_event_time_unit == "ns",
         "nine fraction digits read as datetime64[ns]");
  expect(parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n").naive_event_time_unit == "s",
         "a CSV without rows reads as datetime64[s]");
  expect(parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                        "e0,2024-01-01T00:00:00Z,a,['o1']\n")
             .naive_event_time_unit.empty(),
         "a CSV with offsets reads aware");
  expect(parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                        "e0,2024-01-01 00:00:00+02:00,a,['o1']\n")
             .naive_event_time_unit.empty(),
         "a CSV with a non-zero offset reads aware");
}

void transport() {
  const auto log = parse_ocel_csv(kNaive);
  const auto routed = parse_ocel_json(serialize_ocel_json(log));
  expect(routed.naive_event_time_unit == "us", "the transport keeps the unit");
  expect(!routed.events.empty() && routed.events[0].timestamp == "2024-01-01 10:30:00",
         "the transport keeps the naive texts");
  expect(first_line_after_header(serialize_ocel_csv(routed)) == "e0,2024-01-01 10:30:00,a,['o1'],",
         "the CSV writer prints the naive wall clock after the transport");
  const auto download = serialize_ocel_json(routed, OcelJsonFormat::kClassic, false);
  expect(download.find("\"2024-01-01T10:30:00Z\"") != std::string::npos &&
             download.find("minera:naiveEventTimes") == std::string::npos,
         "a download spells naive times with Z and no marker");
  const auto standard = serialize_ocel_json(routed, OcelJsonFormat::kStandard, false);
  expect(standard.find("\"time\":\"2024-01-02T00:00:00Z\"") != std::string::npos,
         "an OCEL 2.0 download spells naive times with Z");
  const auto open = parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kStandard));
  expect(open.naive_event_time_unit == "us" && !open.events.empty() &&
             open.events[0].timestamp == "2024-01-01 10:30:00",
         "the OCEL 2.0 transport keeps the unit and the texts");
  const auto reread = parse_ocel_json(download);
  expect(reread.naive_event_time_unit.empty(), "a downloaded file reads aware, as the reference reads it");
  expect(error_of([&] { (void)parse_ocel_json(
                     R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01","ocel:omap":["o1"],"ocel:vmap":{}}},)"
                     R"("ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},)"
                     R"("ocel:global-object":{},"minera:naiveEventTimes":"h"})"); })
                 .find("minera:naiveEventTimes") != std::string::npos,
         "an unknown unit is refused");
}

void timestamp_filter() {
  const auto log = parse_ocel_json(serialize_ocel_json(parse_ocel_csv(kNaive)));
  expect(error_of([&] { (void)filter_ocel_events_timestamp(log, "2000-01-01 00:00:00", "2100-01-01 00:00:00"); }) ==
             "TypeError: Invalid comparison between dtype=datetime64[us] and datetime",
         "the timestamp filter raises on a naive frame");
  const auto empty = filter_ocel_events(log, {"none"});
  expect(empty.naive_event_time_unit == "us", "an empty filter result keeps the naive frame");
  expect(!error_of([&] { (void)filter_ocel_events_timestamp(empty, "2000-01-01", "2100-01-01"); }).empty(),
         "the timestamp filter raises on an empty naive frame too");
  auto aware = log;
  aware.naive_event_time_unit.clear();
  expect(filter_ocel_events_timestamp(aware, "2000-01-01 00:00:00", "2100-01-01 00:00:00").events.size() == 3,
         "an aware frame filters");
}

void splits() {
  auto log = parse_ocel_csv(
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:t,ocel:type:u\n"
      "e0,a,2024-01-01 10:30:00,['o1'],\n"
      "e1,b,2024-01-02 00:00:00,['o1'],['u1']\n");
  expect(log.event_activity_first, "an activity-first CSV reads activity first");
  for (const auto variant : {OcelSplitVariant::kConnectedComponents, OcelSplitVariant::kAncestorsDescendants}) {
    OcelSplitOptions options;
    options.variant = variant;
    options.object_type = "t";
    const auto result = split_ocel(log, options);
    expect(!result.logs.empty(), "the split returns sublogs");
    for (const auto& part : result.logs) {
      expect(part.event_activity_first, "a sublog keeps the activity-first frame");
      expect(part.naive_event_time_unit == "us", "a sublog keeps the naive frame");
      expect(serialize_ocel_csv(part).rfind("ocel:eid,ocel:activity,ocel:timestamp", 0) == 0,
             "a sublog's CSV lists the activity first");
    }
  }
}

void playouts() {
  const auto log = parse_ocel_json(serialize_ocel_json(parse_ocel_csv(kNaive)));
  OcpnPlayoutOptions options;
  options.event_binding = OcpnEventBinding::kReference;
  options.max_bindings_per_activity = 1;
  const auto played = ocpn_extensive_playout_result(discover_ocpn(log), options).log;
  expect(!played.events.empty(), "the playout plays events");
  expect(played.event_activity_first && played.naive_event_time_unit == "s",
         "the reference OCPN playout builds an activity-first naive datetime64[s] frame");
  const auto routed = parse_ocel_json(serialize_ocel_json(played));
  expect(serialize_ocel_csv(routed).rfind("ocel:eid,ocel:activity,ocel:timestamp", 0) == 0,
         "a playout's CSV lists the activity first after the transport");
  expect(error_of([&] { (void)filter_ocel_events_timestamp(routed, "1970-01-01 00:00:00", "2100-01-01 00:00:00"); }) ==
             "TypeError: Invalid comparison between dtype=datetime64[s] and datetime",
         "the timestamp filter raises on a playout");
  expect(first_line_after_header(serialize_ocel_csv(routed)).find("+00:00") == std::string::npos,
         "a playout's CSV prints naive times");
}

}  // namespace

int main() {
  csv_units();
  transport();
  timestamp_filter();
  splits();
  playouts();
  if (failures != 0) {
    std::cerr << failures << " review DU1 check(s) failed\n";
    return 1;
  }
  std::cout << "review DU1 checks passed\n";
  return 0;
}
