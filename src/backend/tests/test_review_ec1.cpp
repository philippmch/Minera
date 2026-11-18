// Review EC1: pd.to_datetime's readings that rounds EA and EB left outside
// the model, for the OCEL CSV reader (pm4py 2.7.22's read_ocel_csv, a
// format guessed from the first text or every text on its own) and the
// OCEL 2.0 SQLite reader (pd.to_datetime(utc=True, format="mixed")).
// Observed black-box on pandas 3.0.5 and dateutil 2.9 (review-harness
// round-ec, csv/ecmodel.py: 0 differences over 21,000 fuzzed columns of
// this round and the 149,000 of rounds DZ and EA; drv/natcmp.py: the C++
// port matches the model and pandas on all of them):
//
// - a text that starts with a time reads on today's date at midnight,
//   with pandas' dateutil checks (an unknown zone, a weekday without a day
//   and a day past the month raise);
// - pandas refuses the single letters a A m M p P t T and texts that read
//   as a number below 1000 ("not likely a datetime"); larger numbers go on
//   to dateutil ("2024.5" is May 2024);
// - quarters ("2024Q2", "1Q24", "4Q-2024") read as the quarter's first day,
//   the quarter digit taken as a character code, so "2024Q5" and "12345Q"
//   raise;
// - years 1 to 9999 read in microseconds; a column turns nanoseconds at its
//   first nanosecond text, values after it must fit nanoseconds at once,
//   values before it in a second pass after the zone check;
// - an offset of a day or more raises (dateutil's tzoffset error without a
//   format, the timedelta error with one); without a format, offsets past 59
//   minutes and digit runs of five to seven go to dateutil.
//
// EC1-4: read_ocel_xml reads a reference-shaped event's relations from every
// child keyed "omap", whatever its tag, and every element in it by its value
// attribute (cl2/fz.py: EB's binary differed on 17 of 40 fuzzed files).
//
// With a bank path (docs/ocel-csv-timestamp-ec-bank.tsv.gz) the test also
// replays 6,000 fuzzed columns read on 2026-11-18: texts joined by "|", a
// tab, then the outcome as the harness prints it.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/timestamp.hpp"
#include "../gzip_codec.hpp"
#include "../pandas_to_datetime.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;
namespace ptd = minera::pandas_to_datetime;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::vector<std::string> split(const std::string& joined, char separator) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto at = joined.find(separator, start);
    out.push_back(joined.substr(start, at == std::string::npos ? std::string::npos : at - start));
    if (at == std::string::npos) break;
    start = at + 1;
  }
  return out;
}

long long days_from_civil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civil_from_days(long long z, long long& y, unsigned& m, unsigned& d) {
  z += 719468;
  const long long era = (z >= 0 ? z : z - 146096) / 146097;
  const auto doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = static_cast<long long>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y += m <= 2;
}

std::string offset_text(int minutes) {
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "%c%02d:%02d", minutes >= 0 ? '+' : '-', std::abs(minutes) / 60,
                std::abs(minutes) % 60);
  return buffer;
}

// pd.to_datetime's outcome as the harness prints it: "ok datetime64[unit,
// zone] values" with isoformat values, or "ERR <type>: <message>" up to
// ". You might want to try:".
std::string reading(const std::vector<std::string>& texts) {
  const std::vector<std::string_view> views(texts.begin(), texts.end());
  try {
    const auto result = ptd::to_datetime(views, ptd::Date{2026, 11, 18});
    if (!result) return "outside";
    std::string zone;
    if (result->offset_minutes) zone = *result->offset_minutes == 0 ? ", UTC" : ", UTC" + offset_text(*result->offset_minutes);
    std::string out = std::string("ok datetime64[") + (result->nanoseconds ? "ns" : "us") + zone + "]";
    for (const auto& value : result->values) {
      out += " ";
      if (!value) {
        out += "NaT";
        continue;
      }
      long long days = days_from_civil(value->year, static_cast<unsigned>(value->month),
                                       static_cast<unsigned>(value->day));
      long long seconds = value->hour * 3600LL + value->minute * 60LL + value->second;
      days += seconds / 86400;
      seconds %= 86400;
      long long year;
      unsigned month, day;
      civil_from_days(days, year, month, day);
      char buffer[64];
      std::snprintf(buffer, sizeof buffer, "%04lld-%02u-%02uT%02lld:%02lld:%02lld", year, month, day, seconds / 3600,
                    seconds / 60 % 60, seconds % 60);
      out += buffer;
      std::uint32_t nanoseconds = value->nanoseconds;
      if (!result->nanoseconds) nanoseconds = nanoseconds / 1000 * 1000;
      if (nanoseconds % 1000) {
        std::snprintf(buffer, sizeof buffer, ".%09u", nanoseconds);
        out += buffer;
      } else if (nanoseconds) {
        std::snprintf(buffer, sizeof buffer, ".%06u", nanoseconds / 1000);
        out += buffer;
      }
      if (result->offset_minutes) out += offset_text(*result->offset_minutes);
    }
    return out;
  } catch (const std::invalid_argument& error) {
    std::string message = error.what();
    const auto cut = message.find(". You might");
    return "ERR " + (cut == std::string::npos ? message : message.substr(0, cut));
  }
}

void check(const std::vector<std::string>& texts, const std::string& want) {
  const auto got = reading(texts);
  expect(got == want, "to_datetime " + texts.front() + ": got \"" + got + "\", want \"" + want + "\"");
}

void readings() {
  // A text that starts with a time: today's date at midnight, dateutil's
  // fields over it, pandas' checks.
  check({"10:00"}, "ok datetime64[us] 2026-11-18T10:00:00");
  check({"10:00 May 2024"}, "ok datetime64[us] 2024-05-18T10:00:00");
  check({"10:00 PM May 31"}, "ok datetime64[us] 2026-05-31T22:00:00");
  check({"10:00 99"}, "ok datetime64[us] 1999-11-18T10:00:00");
  check({"10:00 EST"}, "ERR ValueError: Parsed string \"10:00 EST\" included an un-recognized timezone \"EST\".");
  check({"10:00 Mon"}, "ERR ValueError: Parsing datetimes with weekday but no day information is not supported");
  check({"10:00 Feb 30"}, "ERR DateParseError: day is out of range for month: 10:00 Feb 30");
  check({"10:00 UTC", "10:00"},
        "ERR ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to "
        "convert to a common timezone.");
  // Quarters.
  check({"2024Q2"}, "ok datetime64[us] 2024-04-01T00:00:00");
  check({"1Q24"}, "ok datetime64[us] 2024-01-01T00:00:00");
  check({"4Q-2024"}, "ok datetime64[us] 2024-10-01T00:00:00");
  check({"1Q-2"}, "ok datetime64[us] 1998-01-01T00:00:00");
  check({"2024Q5"}, "ERR DateParseError: Incorrect quarterly string is given, quarter must be between 1 and 4: 2024Q5");
  check({"12345Q"}, "ERR DateParseError: Incorrect quarterly string is given, quarter must be between 1 and 4: 12345Q");
  // Numbers.
  check({"2024.5"}, "ok datetime64[us] 2024-05-01T00:00:00");
  check({"12345"}, "ERR DateParseError: year 12345 is out of range: 12345");
  check({"999"}, "ERR ValueError: Given date string \"999\" not likely a datetime");
  check({"-198013445166"}, "ERR ValueError: Given date string \"-198013445166\" not likely a datetime");
  check({"1e5"}, "ERR DateParseError: Unknown datetime string format, unable to parse: 1e5");
  check({"a"}, "ERR ValueError: Given date string \"a\" not likely a datetime");
  // Years 1 to 9999.
  check({"0999-05-01"}, "ok datetime64[us] 0999-05-01T00:00:00");
  check({"999-05-01 10:00"}, "ok datetime64[us] 0999-05-01T10:00:00");
  check({"05/01/0999"}, "ok datetime64[us] 0999-05-01T00:00:00");
  check({"May 1 0999"}, "ok datetime64[us] 0999-05-01T00:00:00");
  check({"2024-01-01", "0050-01-01"}, "ok datetime64[us] 2024-01-01T00:00:00 0050-01-01T00:00:00");
  check({"10000-01-01"}, "ERR DateParseError: year 10000 is out of range: 10000-01-01");
  // Offsets.
  check({"2024-01-01 10:00 +2500"},
        "ERR ValueError: Parsed string \"2024-01-01 10:00 +2500\" gives an invalid tzoffset, which must be between "
        "-timedelta(hours=24) and timedelta(hours=24)");
  check({"2024-01-01T10:00+05:75"}, "ok datetime64[us, UTC+06:15] 2024-01-01T10:00:00+06:15");
  check({"2024-01-01T10:00+05:00", "2024-01-01T10:00+24:00"},
        "ERR ValueError: offset must be a timedelta strictly between -timedelta(hours=24) and timedelta(hours=24), "
        "not datetime.timedelta(days=1).");
  check({"2024-01-01T10:00+05:00", "2024-01-01T10:00-25:30"},
        "ERR ValueError: offset must be a timedelta strictly between -timedelta(hours=24) and timedelta(hours=24), "
        "not datetime.timedelta(days=-2, seconds=81000).");
  check({"2024-01-01T10:00+05:00", "2024-01-01T10:00+24:00x"},
        "ERR ValueError: unconverted data remains when parsing with format \"%Y-%m-%dT%H:%M%z\": \"x\"");
  // The nanosecond passes.
  check({"0999-05-01", "2000-01-01 10:00:00.123456789"},
        "ERR OutOfBoundsDatetime: Out of bounds nanosecond timestamp: 0999-05-01");
  check({"0999-05-01", "2000-01-01 10:00:00.123456789", "0998-01-01"},
        "ERR OutOfBoundsDatetime: Out of bounds nanosecond timestamp: 0998-01-01");
  check({"0999-05-01", "1500-01-01 10:00:00.123456789"},
        "ERR OutOfBoundsDatetime: Out of bounds nanosecond timestamp: 1500-01-01 10:00:00");
  check({"1500-01-01 10:00:00.5", "2000-01-01 10:00:00.123456789"},
        "ERR OutOfBoundsDatetime: Out of bounds nanosecond timestamp: 1500-01-01 10:00:00.5");
  check({"0999-05-01T10:00:30.5+01:00", "2000-01-01 10:00:00.123456789"},
        "ERR ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to "
        "convert to a common timezone.");
  check({"05/25/0500 16:14:01.746000623"}, "ERR OutOfBoundsDatetime: Out of bounds nanosecond timestamp");
  // Outside: year 0 and negative years.
  check({"/04/19"}, "outside");
  check({"-1500"}, "outside");
}

// ------------------------------------------------------------ SQLite (utc, mixed)

std::string mixed(const std::vector<std::string>& texts) {
  std::vector<ptd::Cell> cells;
  for (const auto& text : texts) {
    ptd::Cell cell;
    cell.kind = ptd::Cell::Kind::kText;
    cell.text = text;
    cells.push_back(cell);
  }
  try {
    const auto values = ptd::to_datetime_utc_mixed(cells, ptd::Date{2026, 11, 18});
    if (!values) return "outside";
    std::string out = "ok";
    for (const auto& value : *values)
      out += value ? " " + std::to_string(value->seconds) + "." + std::to_string(value->nanoseconds) : " NaT";
    return out;
  } catch (const std::invalid_argument&) {
    return "raises";
  }
}

void check_mixed(const std::vector<std::string>& texts, const std::string& want) {
  const auto got = mixed(texts);
  expect(got == want, "utc mixed " + texts.front() + ": got \"" + got + "\", want \"" + want + "\"");
}

void sqlite_readings() {
  // Round EB read texts like a number of 1000 or more as errors, and texts
  // that start with a time through plain dateutil.
  check_mixed({"2024.5"}, "ok 1714521600.0");
  check_mixed({"2024-01-01", "5000.0"}, "raises");
  check_mixed({"10:00 EST"}, "raises");
  check_mixed({"10:00 Mon"}, "raises");
  check_mixed({"10:00 Feb 30"}, "raises");
  check_mixed({"10:00 May 2024"}, "ok 1716026400.0");
  check_mixed({"10:00 +2500"}, "raises");
  check_mixed({"2024Q2", "1Q24"}, "ok 1711929600.0 1704067200.0");
  check_mixed({"999-05-01"}, "ok -30631392000.0");
  check_mixed({"2024-01-01T10:00+05:75"}, "ok 1704080700.0");
  check_mixed({"12345"}, "raises");
  check_mixed({"999"}, "raises");
  // A sign before digits that are no ISO text is no negative year: the
  // not-likely check raises. A sign before an ISO text is (outside).
  check_mixed({"-198013445166"}, "raises");
  check_mixed({"2024-01-01", "-2007012"}, "raises");
  check_mixed({"-1393"}, "outside");
  // An integer past 18 digits in dateutil's reading always raises.
  check_mixed({"1000000000000000004"}, "raises");
  check_mixed({"2024-01-01", "May 1000000000000000004"}, "raises");
  check_mixed({"May 5 2024 10:00:00.7996066272231780569"}, "ok 1714903200.799606000");
  check_mixed({"1e5"}, "raises");
  check_mixed({"0999-05-01", "2020-01-01 10:00:00.123456789"}, "raises");
}

// ------------------------------------------------------------ the CSV reader

// The event times parse_ocel_csv keeps, "|"-joined, or the error up to the
// suggestions.
std::string imported(const std::vector<std::string>& texts) {
  std::string csv = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n";
  for (std::size_t i = 0; i < texts.size(); ++i)
    csv += "e" + std::to_string(i) + ",\"" + texts[i] + "\",a,['o1']\n";
  OcelCsvImportOptions options;
  options.trim_fields = false;
  try {
    const auto log = parse_ocel_csv(csv, options);
    std::string out;
    for (std::size_t i = 0; i < texts.size(); ++i) {
      if (i) out += "|";
      for (const auto& event : log.events)
        if (event.id == "e" + std::to_string(i)) out += event.timestamp;
    }
    return out;
  } catch (const std::exception& error) {
    std::string message = error.what();
    const auto cut = message.find(". You might want to try:");
    return cut == std::string::npos ? message : message.substr(0, cut);
  }
}

void check_import(const std::vector<std::string>& texts, const std::string& want) {
  const auto got = imported(texts);
  expect(got == want, "read_ocel_csv " + texts.front() + ": got \"" + got + "\", want \"" + want + "\"");
}

void csv_reader() {
  // An ISO cell native reads as the same instant keeps its text.
  check_import({"0999-05-01", "May 1 0999"}, "0999-05-01|0999-05-01T00:00:00");
  check_import({"2024Q2", "1Q24"}, "2024-04-01T00:00:00|2024-01-01T00:00:00");
  check_import({"2024.5"}, "2024-05-01T00:00:00");
  check_import({"999"}, "ValueError: Given date string \"999\" not likely a datetime");
  check_import({"2024Q5"}, "DateParseError: Incorrect quarterly string is given, quarter must be between 1 and 4: 2024Q5");
  check_import({"2024-01-01 10:00 +2500"},
               "ValueError: Parsed string \"2024-01-01 10:00 +2500\" gives an invalid tzoffset, which must be "
               "between -timedelta(hours=24) and timedelta(hours=24)");
  check_import({"0999-05-01", "2000-01-01 10:00:00.123456789"},
               "OutOfBoundsDatetime: Out of bounds nanosecond timestamp: 0999-05-01");
  check_import({"2024-01-01T10:00+05:75", "2024-01-02T10:00+05:75"},
               "2024-01-01T10:00:00+06:15|2024-01-02T10:00:00+06:15");
}

// The relations parse_ocel_xml reads from one reference-shaped event's
// children, "!" when it refuses the file, as "object:qualifier;" items.
std::string omap_relations(const std::string& children) {
  const std::string xml =
      "<log><events><event><string key=\"id\" value=\"e1\"/><string key=\"activity\" value=\"a\"/>"
      "<date key=\"timestamp\" value=\"2024-01-01T10:00:00\"/>" + children +
      "<list key=\"vmap\"/></event><event><string key=\"id\" value=\"e2\"/><string key=\"activity\" value=\"a\"/>"
      "<date key=\"timestamp\" value=\"2024-01-02T10:00:00\"/><list key=\"omap\"><string key=\"object-id\" "
      "value=\"o2\"/></list></event></events><objects>"
      "<object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\"/></object>"
      "<object><string key=\"id\" value=\"o2\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\"/></object>"
      "</objects></log>";
  try {
    const auto log = parse_ocel_xml(xml);
    std::string out;
    for (const auto& relation : log.relations)
      if (relation.event_id == "e1") out += relation.object_id + ":" + relation.qualifier + ";";
    return out;
  } catch (const std::invalid_argument&) {
    return "!";
  }
}

void classic_omap() {
  const auto check = [](const std::string& children, const std::string& want) {
    const auto got = omap_relations(children);
    expect(got == want, "omap " + children + ": " + got + " (want " + want + ")");
  };
  check("<list key=\"omap\"><string key=\"object-id\" value=\"o1\"/></list>", "o1:;");
  check("<list key=\"omap\"><string key=\"id\" value=\"o1\"/></list>", "o1:;");
  check("<list key=\"omap\"><string value=\"o1\"/></list>", "o1:;");
  check("<foo key=\"omap\"><object key=\"x\" value=\"o1\"/></foo>", "o1:;");
  check("<list key=\"omap\"><list key=\"a\" value=\"o1\"><string key=\"b\" value=\"o2\"/></list></list>", "o1:;");
  check("<list key=\"omap\"><string value=\"o1\"/></list><list key=\"omap\"><string value=\"o2\"/></list>",
        "o1:;o2:;");
  check("<list key=\"omap\"><string value=\"o2\" qualifier=\"x\"/><string value=\"o1\" qualifier=\"y\"/>"
        "<string value=\"o2\" qualifier=\"z\"/></list>",
        "o2:z;o1:y;");
  check("<list key=\"omap\"><string value=\"o1\" qualifier=\"a\"/><string value=\"o1\"/></list>", "o1:;");
  check("<list key=\"omap\"><string value=\"o1\" qualifier=\"a\"/></list>"
        "<list key=\"omap\"><string value=\"o1\" qualifier=\"\"/></list>",
        "o1:;");
  // Keyed some other way, or without a key, it is no omap list.
  check("<list name=\"omap\"><string value=\"o1\"/></list>", "");
  check("<omap><string key=\"object-id\" value=\"o1\"/></omap>", "");
  check("<list key=\"omap\" value=\"o1\"/>", "");
  // No value attribute (KeyError None), or an object the file does not list.
  check("<list key=\"omap\"><string key=\"object-id\">o1</string></list>", "!");
  check("<list key=\"omap\"><string value=\" o1\"/></list>", "!");
  check("<list key=\"omap\"><string value=\"o3\"/></list>", "!");
}

int replay_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << "cannot read " << path << '\n';
    return 1;
  }
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream lines(detail::gzip_decompress(bytes, std::size_t{1} << 28));
  std::string line;
  std::size_t count = 0, mismatches = 0;
  while (std::getline(lines, line)) {
    if (line.empty()) continue;
    const auto tab = line.find('\t');
    const auto texts = split(line.substr(0, tab), '|');
    const auto want = line.substr(tab + 1);
    const auto got = reading(texts);
    ++count;
    if (got != want && ++mismatches <= 10)
      std::cerr << "bank: " << line.substr(0, tab) << "\n  got  " << got << "\n  want " << want << '\n';
  }
  expect(count == 6000, "the bank holds 6,000 columns: " + std::to_string(count));
  expect(mismatches == 0, "bank mismatches: " + std::to_string(mismatches));
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  readings();
  sqlite_readings();
  csv_reader();
  classic_omap();
  if (argc > 1 && replay_bank(argv[1]) != 0) return 1;
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EC1 tests passed\n";
  return 0;
}
