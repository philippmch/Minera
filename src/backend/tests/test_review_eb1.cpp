// Review EB1: pm4py 2.7.22's dateutil fallback in the OCEL 2.0 readers. A
// time or declared date value that fromisoformat refuses goes to
// dateutil.parser.parse without a default (today's date at midnight fills
// the missing fields), fuzzy in read_ocel2_xml (unknown words and numbers
// that fit no field are skipped) and not fuzzy in read_ocel2_json (which
// keeps a text dateutil refuses). read_ocel2_xml raises dateutil's error for
// the first unreadable text in document order; read_ocel2_json stamps the
// naive values of a change field UTC only when that field's column holds
// nothing else. Observed black-box on dateutil 2.9 (review-harness round-eb:
// du/duplain.py models dateutil, 0 differences over about 149,000 texts;
// fz/fz.py reads random documents with pm4py and minera_ocel, 0 differences
// over 1,096 XML and 1,033 JSON documents).
//
// Bank paths (optional): docs/ocel-dateutil-eb-bank.tsv.gz (texts with
// dateutil's outcome, fuzzy and not, on the capture date its first line
// names) and docs/ocel2-dateutil-eb-bank.tsv.gz (documents whose texts read
// the same on any date, with pm4py's outcome in the canonical text below).
#include "minera/ocel.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/timestamp.hpp"
#include "../gzip_codec.hpp"
#include "../pandas_to_datetime.hpp"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
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

// As the round's driver prints a reading: "ok Y M D h mi s us TZ" (TZ naive,
// utc or off:N) or "err MESSAGE"; "outside" when the port declines.
std::string reading(std::string_view text, bool fuzzy, ptd::Date today) {
  try {
    const auto r = ptd::dateutil_parser_parse(text, fuzzy, today);
    if (!r) return "outside";
    const auto& v = r->value;
    std::ostringstream out;
    out << "ok " << v.year << ' ' << v.month << ' ' << v.day << ' ' << v.hour << ' ' << v.minute << ' '
        << v.second << ' ' << v.nanoseconds / 1000 << ' ';
    if (!r->offset_seconds) out << "naive";
    else if (*r->offset_seconds == 0) out << "utc";
    else out << "off:" << *r->offset_seconds;
    return out.str();
  } catch (const std::invalid_argument& error) {
    return std::string("err ") + error.what();
  }
}

void parse_cases() {
  const ptd::Date today{2026, 11, 18};
  const auto check = [&](std::string_view text, bool fuzzy, const std::string& want, ptd::Date day = {2026, 11, 18}) {
    const auto got = reading(text, fuzzy, day);
    expect(got == want, "dateutil " + std::string(text) + (fuzzy ? " (fuzzy)" : "") + ": " + got + " != " + want);
  };
  // Missing fields come from today at midnight; a weekday moves forward.
  check("10:00", false, "ok 2026 11 18 10 0 0 0 naive");
  check("Monday 10:00", false, "ok 2026 11 23 10 0 0 0 naive");
  check("Wed", true, "ok 2026 11 18 0 0 0 0 naive");
  check("Feb 2024", false, "ok 2024 2 18 0 0 0 0 naive");
  check("Feb 2024", false, "ok 2024 2 29 0 0 0 0 naive", ptd::Date{2026, 1, 31});
  check("Feb 2023", false, "ok 2023 2 28 0 0 0 0 naive", ptd::Date{2026, 1, 31});
  check("Jan 2020 Monday", true, "ok 2020 1 20 0 0 0 0 naive");
  check("5", false, "ok 2026 11 5 0 0 0 0 naive");
  // Two-digit years around today's year.
  check("01/02/75", false, "ok 2075 1 2 0 0 0 0 naive");
  check("01/02/77", false, "ok 1977 1 2 0 0 0 0 naive");
  // Zones: UTC names and offsets; an unknown name stays naive; an offset of
  // a day or more is kept.
  check("2020-01-01 10:00 UTC", false, "ok 2020 1 1 10 0 0 0 utc");
  check("2020-01-01 10:00 EST", false, "ok 2020 1 1 10 0 0 0 naive");
  check("2020-01-01 10:00 GMT+3", false, "ok 2020 1 1 10 0 0 0 off:-10800");
  check("2020-01-01 10:00 +25:00", false, "ok 2020 1 1 10 0 0 0 off:90000");
  check("2020-01-01 10:00 +0530 (IST)", false, "ok 2020 1 1 10 0 0 0 off:19800");
  // Fuzzy skips what plain parsing refuses.
  check("2020-01-01 10:00 x", false, "err Unknown string format: 2020-01-01 10:00 x");
  check("2020-01-01 10:00 x", true, "ok 2020 1 1 10 0 0 0 naive");
  check("on 2020-01-01 hello 10:00", true, "ok 2020 1 1 10 0 0 0 naive");
  check("10 PM PM", true, "ok 2026 11 18 22 0 0 0 naive");
  check("10 PM PM", false, "err Unknown string format: 10 PM PM");
  check("2020-01-01 99", true, "err hour must be in 0..23: 2020-01-01 99");
  check("Jan Feb 2020", true, "err Unknown string format: Jan Feb 2020");
  // dateutil's errors.
  check("x", true, "err String does not contain a date: x");
  check("", true, "err String does not contain a date: ");
  check("  ", false, "err String does not contain a date:   ");
  check("Feb 30 2024", false, "err day is out of range for month: Feb 30 2024");
  check("Aug 10000", true, "err year 10000 is out of range: Aug 10000");
  check("13/2020", false, "err bad month number 13; must be 1-12: 13/2020");
  check("99999999999 Jan", true, "err signed integer is greater than maximum");
  check("Saturday Dec 9999", false, "err date value out of range", ptd::Date{2026, 11, 30});
  // Outside the port: characters beyond printable ASCII.
  check("2020-01-01 10:00 \xc3\xa9", true, "outside");
  (void)today;
}

std::string read_gzip_file(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error(std::string("cannot open ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return detail::gzip_decompress(bytes, std::size_t{1} << 28);
}

std::string unhex(std::string_view hex) {
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(static_cast<char>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
  return out;
}

std::string hex(std::string_view text) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (const char c : text) {
    out += digits[static_cast<unsigned char>(c) >> 4];
    out += digits[static_cast<unsigned char>(c) & 15];
  }
  return out;
}

void text_bank(const char* path) {
  std::istringstream lines(read_gzip_file(path));
  std::string line;
  std::getline(lines, line);
  int year = 0, month = 0, day = 0;
  expect(std::sscanf(line.c_str(), "#today %d-%d-%d", &year, &month, &day) == 3, "text bank header");
  const ptd::Date today{year, month, day};
  std::size_t rows = 0, bad = 0;
  while (std::getline(lines, line)) {
    const auto first = line.find('\t'), second = line.find('\t', first + 1);
    const bool fuzzy = line.substr(0, first) == "1";
    const auto text = unhex(line.substr(first + 1, second - first - 1));
    const auto want = line.substr(second + 1);
    ++rows;
    const auto got = reading(text, fuzzy, today);
    if (got != want && ++bad <= 10) expect(false, "text bank " + text + ": " + got + " != " + want);
  }
  expect(bad == 0, "text bank: " + std::to_string(bad) + " of " + std::to_string(rows) + " rows differ");
  expect(rows > 10000, "text bank rows");
}

// pm4py's value in the round's canonical text: "A:<UTC ISO>" for an aware
// datetime, "N:<ISO>" for a naive one (pandas' isoformat), "S:<hex>" for a
// text, "M" for a missing value. Event times compare their clock only
// (native stamps naive ones when writing).
std::string code_text(const std::string& text, bool event) {
  static const std::regex iso(R"(^(\d{4})-(\d\d)-(\d\d)(?:[T ](\d\d):(\d\d)(?::(\d\d)(?:\.(\d+))?)?)?(Z|[+-]\d\d:?\d\d)?$)");
  std::smatch m;
  if (!std::regex_match(text, m, iso)) return "S:" + hex(text);
  const auto num = [&](int i) { return m[i].matched ? std::stoi(m[i].str()) : 0; };
  int y = num(1), mo = num(2), d = num(3), h = num(4), mi = num(5), s = num(6);
  std::string fraction = m[7].matched ? m[7].str() : "";
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  if (mo < 1 || mo > 12 || d < 1 || d > (mo == 2 && leap ? 29 : kDays[mo - 1]) || h > 23 || mi > 59 || s > 59 ||
      fraction.size() > 9)
    return "S:" + hex(text);
  fraction.resize(9, '0');
  const long nanos = std::stol(fraction);
  std::int64_t minutes = 0;
  if (m[8].matched && m[8].str() != "Z") {
    const auto zone = m[8].str();
    minutes = (zone[0] == '+' ? 1 : -1) * (std::stoi(zone.substr(1, 2)) * 60 + std::stoi(zone.substr(zone.size() - 2)));
  }
  // Days from civil and back (Howard Hinnant).
  const auto days_from = [](std::int64_t yy, unsigned mm, unsigned dd) {
    yy -= mm <= 2;
    const std::int64_t era = (yy >= 0 ? yy : yy - 399) / 400;
    const auto yoe = static_cast<unsigned>(yy - era * 400);
    const unsigned doy = (153 * (mm + (mm > 2 ? -3 : 9)) + 2) / 5 + dd - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
  };
  std::int64_t total = days_from(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400 + h * 3600 + mi * 60 + s -
                       minutes * 60;
  std::int64_t days = total >= 0 ? total / 86400 : -((-total + 86399) / 86400);
  const std::int64_t clock = total - days * 86400;
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const std::int64_t doe = days - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t dd = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t mm = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t yy = yoe + era * 400 + (mm <= 2);
  char buffer[64];
  int length = std::snprintf(buffer, sizeof buffer, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld", static_cast<long long>(yy),
                             static_cast<long long>(mm), static_cast<long long>(dd), static_cast<long long>(clock / 3600),
                             static_cast<long long>(clock / 60 % 60), static_cast<long long>(clock % 60));
  std::string out(buffer, static_cast<std::size_t>(length));
  if (nanos % 1000 != 0) out += "." + std::string(9 - std::to_string(nanos).size(), '0') + std::to_string(nanos);
  else if (nanos != 0) {
    const auto micro = std::to_string(nanos / 1000);
    out += "." + std::string(6 - micro.size(), '0') + micro;
  }
  const bool aware = m[8].matched;
  return std::string(aware || event ? "A:" : "N:") + out;
}

std::string code_value(const OcelValue* value) {
  if (value == nullptr || std::holds_alternative<std::nullptr_t>(*value)) return "M";
  if (const auto* text = std::get_if<std::string>(value)) return code_text(*text, false);
  return "V:?";
}

const OcelValue* attribute(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& entry : attributes)
    if (entry.key == key) return &entry.value;
  return nullptr;
}

std::string canonical(const Ocel& log) {
  std::string events;
  for (const auto& event : log.events) {
    if (!events.empty()) events += ';';
    events += event.id + "|" + code_text(event.timestamp, true) + "|" + code_value(attribute(event.attributes, "d"));
  }
  std::vector<std::string> objects, changes;
  for (const auto& object : log.objects) objects.push_back(object.id + "|" + code_value(attribute(object.attributes, "x")));
  for (const auto& change : log.object_changes)
    changes.push_back(change.object_id + "|" + code_text(change.timestamp, false) + "|" + code_value(&change.value));
  std::sort(objects.begin(), objects.end());
  std::sort(changes.begin(), changes.end());
  const auto join = [](const std::vector<std::string>& parts) {
    std::string out;
    for (const auto& part : parts) out += (out.empty() ? "" : ";") + part;
    return out;
  };
  return "OK " + events + " # " + join(objects) + " # " + join(changes);
}

std::string outcome(const std::string& format, const std::string& document) {
  try {
    return canonical(format == "xml" ? parse_ocel_xml(document) : parse_ocel_json(document));
  } catch (const std::exception& error) {
    return std::string("ERR ") + error.what();
  }
}

void file_bank(const char* path) {
  std::istringstream lines(read_gzip_file(path));
  std::string line;
  std::getline(lines, line);
  std::size_t rows = 0, bad = 0;
  while (std::getline(lines, line)) {
    const auto first = line.find('\t'), second = line.find('\t', first + 1);
    const auto format = line.substr(0, first);
    const auto document = unhex(line.substr(first + 1, second - first - 1));
    const auto want = line.substr(second + 1);
    ++rows;
    const auto got = outcome(format, document);
    if (got != want && ++bad <= 10)
      expect(false, "document bank (" + format + ") " + document + "\n  got  " + got + "\n  want " + want);
  }
  expect(bad == 0, "document bank: " + std::to_string(bad) + " of " + std::to_string(rows) + " rows differ");
  expect(rows >= 1000, "document bank rows");
}

std::string xml_document(const std::vector<std::string>& times, const std::vector<std::string>& values,
                         const std::string& static_value = "2021-01-01T00:00:00Z",
                         const std::vector<std::pair<std::string, std::string>>& changes = {}) {
  std::string out =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<log><object-types><object-type name=\"T\"><attributes>"
      "<attribute name=\"x\" type=\"date\"/></attributes></object-type></object-types><event-types>"
      "<event-type name=\"a\"><attributes><attribute name=\"d\" type=\"date\"/></attributes></event-type>"
      "</event-types><objects><object id=\"o1\" type=\"T\"><attributes>";
  out += "<attribute name=\"x\" time=\"1970-01-01T00:00:00Z\">" + static_value + "</attribute>";
  for (const auto& [time, value] : changes) out += "<attribute name=\"x\" time=\"" + time + "\">" + value + "</attribute>";
  out += "</attributes><objects/></object></objects><events>";
  for (std::size_t i = 0; i < times.size(); ++i) {
    out += "<event id=\"e" + std::to_string(i) + "\" type=\"a\" time=\"" + times[i] + "\"><attributes>";
    if (i < values.size()) out += "<attribute name=\"d\">" + values[i] + "</attribute>";
    out += "</attributes><objects><relationship object-id=\"o1\" qualifier=\"\"/></objects></event>";
  }
  return out + "</events></log>";
}

std::string json_document(const std::vector<std::pair<std::string, std::string>>& changes,
                          const std::string& event_value = "2021-02-03T00:00:00Z") {
  std::string attributes = "{\"name\":\"x\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"2021-01-01T00:00:00Z\"}";
  for (const auto& [field, value] : changes)
    attributes += ",{\"name\":\"" + field + "\",\"time\":\"2022-01-01T00:00:00Z\",\"value\":" +
                  (value == "null" ? value : "\"" + value + "\"") + "}";
  return "{\"objectTypes\":[{\"name\":\"T\",\"attributes\":[{\"name\":\"x\",\"type\":\"date\"}]}],"
         "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[{\"name\":\"d\",\"type\":\"date\"}]}],"
         "\"objects\":[{\"id\":\"o1\",\"type\":\"T\",\"attributes\":[" + attributes + "],\"relationships\":[]}],"
         "\"events\":[{\"id\":\"e0\",\"type\":\"a\",\"time\":\"2020-01-01T00:00:00Z\",\"attributes\":[{\"name\":\"d\","
         "\"value\":\"" + event_value + "\"}],\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"\"}]}]}";
}

std::string error_of(const std::string& document, bool xml = true) {
  try {
    if (xml) (void)parse_ocel_xml(document);
    else (void)parse_ocel_json(document);
  } catch (const std::exception& error) {
    return error.what();
  }
  return "no error";
}

void reader_cases() {
  // read_ocel2_xml keeps an object attribute's time "0" (and the
  // 1970-01-01T00:00:00 base entries) as its text, while "00" raises; an
  // element text's newlines read as spaces.
  {
    const auto kept = parse_ocel_xml(xml_document({"2020-01-01T00:00:00Z"}, {}, "2021-01-01T00:00:00Z", {{"0", "2022-01-01"}}));
    expect(kept.object_changes.size() == 1 && kept.object_changes[0].timestamp == "0", "change time 0 stays a text");
    expect(error_of(xml_document({"2020-01-01T00:00:00Z"}, {}, "2021-01-01T00:00:00Z", {{"00", "2022-01-01"}})) ==
               "day is out of range for month: 00",
           "change time 00 raises");
    expect(error_of(xml_document({"0"}, {})) == "day is out of range for month: 0", "event time 0 raises");
    const auto spaced = parse_ocel_xml(xml_document({"2020-01-01T00:00:00Z"}, {"\n    Jan 5\n 2024\n  "}));
    const auto* value = std::get_if<std::string>(&spaced.events[0].attributes[0].value);
    expect(value && *value == "2024-01-05T00:00:00", "newlines read as spaces: " + (value ? *value : std::string("?")));
  }
  // read_ocel2_xml: dateutil shapes sort by instant (an offset after an
  // unknown word read fuzzy), naive next to aware raises.
  {
    const auto log = parse_ocel_xml(xml_document({"2020-01-01T03:00:00Z", "2020-01-01 05:00 +0300 x"}, {}));
    expect(log.events.size() == 2 && log.events[0].id == "e1" && log.events[1].id == "e0", "fuzzy offset sorts by instant");
    expect(log.events[0].timestamp == "2020-01-01T02:00:00+00:00", "fuzzy offset stored as UTC: " + log.events[0].timestamp);
    const auto naive = parse_ocel_xml(xml_document({"Jan 5 2024 10:00 hello", "on 4 January 2024"}, {}));
    expect(naive.events[0].id == "e1" && naive.events[0].timestamp == "2024-01-04T00:00:00", "naive fuzzy values sort");
    expect(error_of(xml_document({"2020-01-01T00:00:00Z", "Jan 5 2024"}, {})).find("'values' is not ordered") == 0,
           "naive next to aware raises");
  }
  // The first unreadable text in document order raises dateutil's error:
  // the object section first here, an event's time before its attributes,
  // an attribute's value before its time.
  expect(error_of(xml_document({"xev"}, {}, "xst")) == "String does not contain a date: xst", "object before event");
  expect(error_of(xml_document({"2020-01-01T00:00:00Z", "xb"}, {"xa", "2020-01-01"})) == "String does not contain a date: xa",
         "attribute before the next event");
  expect(error_of(xml_document({"2020-01-01T00:00:00Z"}, {}, "2021-01-01T00:00:00Z", {{"xch", "xcv"}})) ==
             "String does not contain a date: xcv",
         "value before its time");
  expect(error_of(xml_document({"Feb 30 2024"}, {})) == "day is out of range for month: Feb 30 2024", "range error");
  expect(error_of(xml_document({"Jan Feb 2020"}, {})) == "Unknown string format: Jan Feb 2020", "unknown format");
  expect(error_of(xml_document({"99999999999 Jan"}, {})) == "signed integer is greater than maximum", "overflow");
  expect(error_of(xml_document({"2020-01-01T00:00:00Z"}, {""})) ==
             "Parser must be a string or character stream, not NoneType",
         "empty date element");
  // A change time read fuzzy, kept naive.
  {
    const auto log = parse_ocel_xml(xml_document({"2020-01-01T00:00:00Z"}, {}, "2021-01-01T00:00:00Z",
                                                 {{"Mar 3 2021 08:00 x", "2021-04-01T00:00:00Z"}}));
    expect(log.object_changes.size() == 1 && log.object_changes[0].timestamp == "2021-03-03T08:00:00",
           "fuzzy change time");
  }
  // read_ocel2_json: not fuzzy, an unreadable text stays.
  {
    const auto log = parse_ocel_json(json_document({}, "2020-01-01 10:00 x"));
    expect(std::get<std::string>(log.events[0].attributes[0].value) == "2020-01-01 10:00 x", "json keeps an unread text");
    const auto read = parse_ocel_json(json_document({}, "Feb 3 2020 10:00"));
    expect(std::get<std::string>(read.events[0].attributes[0].value) == "2020-02-03T10:00:00", "json naive event value");
  }
  // A change field of naive values (and missing ones) is stamped UTC; next
  // to a text or an aware value the naive value stays naive.
  {
    const auto stamped = parse_ocel_json(json_document({{"x", "29/7/1967 3PM"}, {"x", "null"}}));
    expect(std::get<std::string>(stamped.object_changes[0].value) == "1967-07-29T15:00:00+00:00", "naive column stamped");
    const auto with_text = parse_ocel_json(json_document({{"x", "29/7/1967 3PM"}, {"x", "x"}}));
    expect(std::get<std::string>(with_text.object_changes[0].value) == "1967-07-29T15:00:00", "next to a text");
    const auto with_aware = parse_ocel_json(json_document({{"x", "29/7/1967 3PM"}, {"x", "2020-01-01T00:00Z"}}));
    expect(std::get<std::string>(with_aware.object_changes[0].value) == "1967-07-29T15:00:00", "next to an aware value");
  }
}

}  // namespace

int main(int argc, char** argv) {
  parse_cases();
  reader_cases();
  if (argc > 1) text_bank(argv[1]);
  if (argc > 2) file_bank(argv[2]);
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EB1 tests passed\n";
  return 0;
}
