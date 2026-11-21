// Review FK1 (round FK, re-review of FJ): checks for the fixes of this round.
//
// - FK1-1 An aware events frame at nanoseconds (an OCEL CSV log with an
//   offset, or an OCEL SQLite log, where any time of the column has more
//   than six fraction digits, rows the reader drops included) keeps that
//   unit when filters, splits or the reader remove those rows, so the exact
//   filters' converted value array keeps its nanoseconds. Native took the
//   unit from the events left and floored the values to microseconds.
// - FK1-2 The edge metrics with business hours read each time at its
//   microsecond, floored, as pm4py's BusinessHours does with a Timestamp
//   at nanoseconds. Native rounded to the nearest microsecond.
//
// See docs/reference-2.7.22-parity.md, Review FK1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(MINERA_HAS_SQLITE)
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

using Ids = std::vector<std::string>;
using Values = std::vector<OcelEventAttributeFilterValue>;

ParsedExactTimestamp at(const char* text) { return *parse_timestamp_exact(text); }

// fk t1/h1.csv: e0's time alone has nanoseconds; the activity filter keeps
// e1 to e3 (e3 without a time).
Ocel h1(const std::string& zone) {
  return parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                        "e0,2024-02-02 10:00:00.123456789" + zone + ",b,['t0']\n"
                        "e1,2024-02-02 11:00:00.5" + zone + ",a,['t0']\n"
                        "e2,2024-02-02 12:00:00.250000" + zone + ",a,['t1']\n"
                        "e3,,a,['t1']\n"
                        "e4,2024-02-02 13:00:00.000001" + zone + ",c,['t0']\n",
                        "ocel:oid,ocel:type\nt0,t\nt1,t\n");
}

void fk1_1_csv_after_filters() {
  const auto log = h1("+00:00");
  expect(log.nanosecond_event_times, "FK1-1 an aware CSV column with a nanosecond time is at nanoseconds");
  const auto kept = filter_ocel_event_attribute(log, "ocel:activity", {std::string("a")}, true);
  expect(event_ids(kept) == Ids{"e1", "e2", "e3"}, "FK1-1 the activity filter keeps e1 to e3");
  expect(kept.nanosecond_event_times, "FK1-1 the filter keeps the unit");
  const auto exact = [&](const Ocel& from, const Values& values) {
    return event_ids(filter_ocel_event_attribute_exact(from, "ocel:timestamp", values, true));
  };
  const auto finer = at("2024-02-02T11:00:00.5000005+00:00");
  expect(exact(kept, Values{finer, OcelAttributeNaT{}}) == Ids{"e3"},
         "FK1-1 a finer datetime next to NaT keeps its nanoseconds after the filter");
  expect(exact(kept, Values{finer}).empty(), "FK1-1 the finer datetime alone too");
  expect(exact(kept, Values{at("2024-02-02T11:00:00.5+00:00"), OcelAttributeNaT{}}) == Ids{"e1", "e3"},
         "FK1-1 the cell's own datetime still matches");
  // The transport keeps the unit.
  const auto json = serialize_ocel_json(kept);
  expect(json.find("\"minera:nanosecondEventTimes\":true") != std::string::npos, "FK1-1 the transport carries the unit");
  const auto back = parse_ocel_json(json);
  expect(back.nanosecond_event_times, "FK1-1 the transport's reader restores the unit");
  expect(exact(back, Values{finer, OcelAttributeNaT{}}) == Ids{"e3"}, "FK1-1 and the filter keeps the nanoseconds");
  // Another offset: the unit comes from the texts all the same.
  const auto plus2 = filter_ocel_event_attribute(h1("+02:00"), "ocel:activity", {std::string("a")}, true);
  expect(plus2.nanosecond_event_times, "FK1-1 an offset of +02:00 too");
  expect(exact(plus2, Values{at("2024-02-02T11:00:00.5000005+02:00")}).empty(), "FK1-1 the +02:00 column keeps nanoseconds");
  // Controls: a column at microseconds floors (FJ1-3); a naive one keeps its
  // own unit (Ocel::naive_event_time_unit) and has no aware flag.
  const auto micro = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                                    "e1,2024-02-02 11:00:00.5+00:00,a,['t0']\ne3,,a,['t0']\n",
                                    "ocel:oid,ocel:type\nt0,t\n");
  expect(!micro.nanosecond_event_times, "FK1-1 a column at microseconds is not at nanoseconds");
  expect(exact(micro, Values{finer, OcelAttributeNaT{}}) == Ids{"e1", "e3"}, "FK1-1 microseconds floor the finer datetime");
  const auto naive = filter_ocel_event_attribute(h1(""), "ocel:activity", {std::string("a")}, true);
  expect(!naive.nanosecond_event_times && naive.naive_event_time_unit == "ns", "FK1-1 a naive column keeps its own unit");
  expect(exact(naive, Values{at("2024-02-02T11:00:00.5000005"), OcelAttributeNaT{}}) == Ids{"e3"},
         "FK1-1 the naive column keeps nanoseconds too");
}

#if defined(MINERA_HAS_SQLITE)
// fk t2/mk.py: e0 related, e1 related, the nanosecond time on `nanosecond`
// (an event without relations, or a type-table row the event table does not
// list), which the reader drops.
Ocel sqlite_log(const std::string& nanosecond_row, bool related) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_fk1.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  std::string sql =
      "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT);"
      "CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
      "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
      "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
      "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
      "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);"
      "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP);"
      "CREATE TABLE object_T (ocel_id TEXT, ocel_time TIMESTAMP, ocel_changed_field TEXT);"
      "INSERT INTO event_map_type VALUES ('a','A');"
      "INSERT INTO object_map_type VALUES ('t','T');"
      "INSERT INTO object VALUES ('o0','t');"
      "INSERT INTO object_T VALUES ('o0','1970-01-01 00:00:00',NULL);"
      "INSERT INTO event VALUES ('e0','a'),('e1','a');"
      "INSERT INTO event_A VALUES ('e0','2024-02-02 10:00:00.5'),('e1','2024-02-02 11:00:00');"
      "INSERT INTO event_object VALUES ('e0','o0','q'),('e1','o0','q');";
  if (!nanosecond_row.empty()) {
    if (related) sql += "INSERT INTO event VALUES ('" + nanosecond_row + "','a');";
    sql += "INSERT INTO event_A VALUES ('" + nanosecond_row + "','2024-02-02 12:00:00.123456789');";
  }
  const bool written = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
  sqlite3_close(db);
  expect(written, "FK1-1 the SQLite file is written");
  auto read = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return read;
}

void fk1_1_sqlite_dropped_rows() {
  const auto finer = at("2024-02-02T10:00:00.5000005+00:00");
  const auto exact = [&](const Ocel& from, const Values& values) {
    return event_ids(filter_ocel_event_attribute_exact(from, "ocel:timestamp", values, true));
  };
  // An event without relations (dropped), and a type-table row of an id the
  // event table does not list: both type the column.
  for (const auto& [row, listed] : std::vector<std::pair<std::string, bool>>{{"e2", true}, {"zz", false}}) {
    const auto log = sqlite_log(row, listed);
    expect(event_ids(log) == Ids{"e0", "e1"}, "FK1-1 the reader keeps e0 and e1 (" + row + ")");
    expect(log.nanosecond_event_times, "FK1-1 the dropped row's nanoseconds type the column (" + row + ")");
    expect(exact(log, Values{finer}).empty(), "FK1-1 a finer datetime does not match (" + row + ")");
    expect(exact(log, Values{finer, nullptr}).empty(), "FK1-1 next to None neither (" + row + ")");
  }
  // Control: without the nanosecond row the column is at microseconds.
  const auto micro = sqlite_log("", false);
  expect(!micro.nanosecond_event_times, "FK1-1 a SQLite column at microseconds");
  expect(exact(micro, Values{finer}) == Ids{"e0"}, "FK1-1 microseconds floor the finer datetime");
}
#endif

// fk t4/b1.csv: one object through four events on a Friday afternoon, times
// at nanoseconds; pm4py's edge performance with business hours.
void fk1_2_edge_business_hours() {
  const auto log = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                                  "e0,2024-02-02 14:57:26.123456789+00:00,a,['t0']\n"
                                  "e1,2024-02-02 16:00:00.000000000+00:00,b,['t0']\n"
                                  "e2,2024-02-02 16:00:00.000000900+00:00,c,['t0']\n"
                                  "e3,2024-02-02 16:00:00.000001100+00:00,d,['t0']\n",
                                  "ocel:oid,ocel:type\nt0,t\n");
  OcelEdgeAssociationOptions options;
  options.use_business_hours = true;
  const auto edges = edge_associations(log, options);
  const auto sample = [&](const char* source, const char* target) -> double {
    const auto type = edges.find("t");
    if (type == edges.end()) return -1;
    const auto edge = type->second.find({source, target});
    if (edge == type->second.end() || edge->second.performance_event_couples.size() != 1) return -1;
    return edge->second.performance_event_couples.front();
  };
  expect(std::abs(sample("a", "b") - 3753.876544) < 1e-7, "FK1-2 the start time is floored to its microsecond");
  expect(std::abs(sample("b", "c")) < 1e-9, "FK1-2 two times within one microsecond measure 0");
  expect(std::abs(sample("c", "d") - 1e-6) < 1e-8, "FK1-2 two times across a microsecond measure one microsecond");
}

}  // namespace

int main() {
  fk1_1_csv_after_filters();
  fk1_2_edge_business_hours();
#if defined(MINERA_HAS_SQLITE)
  fk1_1_sqlite_dropped_rows();
#endif
  if (failures) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review FK1 checks passed\n";
  return 0;
}
