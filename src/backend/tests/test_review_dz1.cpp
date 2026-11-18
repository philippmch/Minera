// Review DZ1: the OCEL CSV reader's timestamp shapes outside the ISO family.
// pm4py 2.7.22's read_ocel_csv parses the timestamp column with
// pd.to_datetime, which guesses one format from the first text (through
// dateutil: "%m/%d/%Y" for 01/02/2024, "%d.%m.%Y" when the first field
// passes 12, a literal "z" or "GMT", "%Z" for UTC, " %z" for an offset after
// a space), checks the first text against it and parses every text with it:
// an ISO format (a prefix of "%Y-%m-%d %H:%M:%S.%f%z" with any of the date
// separators " /\-." or none, and " " or "T") goes to an ISO parser first,
// and whatever that parser refuses (a lowercase "t", seconds 60 and 61,
// which roll over, a day past the month) goes to the strptime rules, whose
// errors are the ones raised. Observed black-box (review-harness round-dz,
// ts/tsmodel.py: 0 differences over 83,647 fuzzed columns; ts/natfz.py:
// server imports).
//
// With a bank path (docs/ocel-csv-timestamp-dz-bank.tsv.gz) the test also
// replays 5,000 fuzzed columns: texts joined by "|", a tab, then
// "ok <us|ns|aware> <pandas' values joined by |>" or the ValueError's text
// up to ". You might want to try:".
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/timestamp.hpp"
#include "../gzip_codec.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
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

std::string csv_of(const std::vector<std::string>& texts) {
  std::string csv = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n";
  for (std::size_t i = 0; i < texts.size(); ++i)
    csv += texts[i].empty() ? std::string(",,a,\n") : "e" + std::to_string(i) + "," + texts[i] + ",a,['o1']\n";
  return csv;
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

// "ok <kind> <value|...>" with each event time spelled as pandas' value when
// native reads the same instant, or the error up to the suggestions.
std::string outcome(const std::vector<std::string>& texts, const std::vector<std::string>& expected_values) {
  Ocel log;
  try {
    log = parse_ocel_csv(csv_of(texts));
  } catch (const std::exception& error) {
    std::string message = error.what();
    const auto cut = message.find(". You might want to try:");
    return cut == std::string::npos ? message : message.substr(0, cut);
  }
  std::string out = "ok " + (log.naive_event_time_unit.empty() ? std::string("aware") : log.naive_event_time_unit) + " ";
  for (std::size_t i = 0; i < texts.size(); ++i) {
    if (i) out += "|";
    if (texts[i].empty()) {
      out += "NaT";
      continue;
    }
    const OcelEvent* event = nullptr;
    for (const auto& candidate : log.events)
      if (candidate.id == "e" + std::to_string(i)) event = &candidate;
    if (event == nullptr) {
      out += "missing";
      continue;
    }
    const auto got = parse_timestamp_exact(event->timestamp);
    const auto want = i < expected_values.size() ? parse_timestamp_exact(expected_values[i])
                                                 : std::optional<ParsedExactTimestamp>{};
    if (got && want && got->seconds == want->seconds && got->nanoseconds == want->nanoseconds &&
        got->timezone_aware == want->timezone_aware &&
        (!got->timezone_aware || got->offset_minutes == want->offset_minutes))
      out += expected_values[i];
    else
      out += event->timestamp;
  }
  return out;
}

std::string expected_outcome(const std::string& expected, std::vector<std::string>& values) {
  values.clear();
  if (expected.rfind("ok ", 0) != 0) return expected;
  const auto space = expected.find(' ', 3);
  values = split(expected.substr(space + 1), '|');
  return expected;
}

void check(const std::vector<std::string>& texts, const std::string& expected) {
  std::vector<std::string> values;
  const auto want = expected_outcome(expected, values);
  const auto got = outcome(texts, values);
  expect(got == want, "timestamps " + texts.front() + ": got \"" + got + "\", want \"" + want + "\"");
}

std::string value_error(const std::string& text) { return "ValueError: " + text; }

void shapes() {
  // Literal "z" and "GMT" stay naive; "+02", a space before the offset and
  // "UTC" are aware.
  check({"2024-01-02 13:45:07z"}, "ok us 2024-01-02T13:45:07");
  check({"2024-01-02 13:45:07 GMT", "2024-01-03 08:00:00 GMT"}, "ok us 2024-01-02T13:45:07|2024-01-03T08:00:00");
  check({"2024-01-02 13:45:07+02", "2024-01-03 13:45:07+0200"},
        "ok aware 2024-01-02T13:45:07+02:00|2024-01-03T13:45:07+02:00");
  check({"2024-01-02 13:45:07 +02:00"}, "ok aware 2024-01-02T13:45:07+02:00");
  check({"2024-01-02 13:45:07 UTC", "2024-01-03 13:45:07 UTC"},
        "ok aware 2024-01-02T13:45:07+00:00|2024-01-03T13:45:07+00:00");
  check({"2024-01-02T13:45:07 Z"}, "ok aware 2024-01-02T13:45:07+00:00");
  // Other date orders and separators, one-digit fields.
  check({"2024/01/02", "2024/1/3"}, "ok us 2024-01-02T00:00:00|2024-01-03T00:00:00");
  check({"2024.01.02 10:00"}, "ok us 2024-01-02T10:00:00");
  check({"01/02/2024 13:45", "12/31/2024 1:5"}, "ok us 2024-01-02T13:45:00|2024-12-31T01:05:00");
  check({"13/02/2024", "01/03/2024"}, "ok us 2024-02-13T00:00:00|2024-03-01T00:00:00");
  check({"2024-1-2 3:4:5"}, "ok us 2024-01-02T03:04:05");
  check({"2024-01-02t13:45:07", "2024-01-02T13:45:08"}, "ok us 2024-01-02T13:45:07|2024-01-02T13:45:08");
  // Seconds 60 and 61 roll over (the ISO parser refuses them, the strptime
  // rules take them).
  check({"2024-12-31 23:59:59", "2024-12-31 23:59:60", "2024-01-02 10:00:61"},
        "ok us 2024-12-31T23:59:59|2025-01-01T00:00:00|2024-01-02T10:01:01");
  // More than six fraction digits make the column nanoseconds.
  check({"01/02/2024 13:45:07.1234567"}, "ok ns 2024-01-02T13:45:07.123456700");
  // Errors come from the strptime rules.
  check({"2024-01-02", "2024-01-32"},
        value_error("unconverted data remains when parsing with format \"%Y-%m-%d\": \"2\""));
  check({"2024-01-02 10:00", "2024-01-02 24:00"},
        value_error("time data \"2024-01-02 24:00\" doesn't match format \"%Y-%m-%d %H:%M\""));
  check({"2024-01-02T10:00:00+0100", "2024-01-02T10:00:60+01"},
        value_error("time data \"2024-01-02T10:00:60+01\" doesn't match format \"%Y-%m-%dT%H:%M:%S%z\""));
  check({"2024-01-02t13:45:07", "2024-01-02 13:45:08"},
        value_error("time data \"2024-01-02 13:45:08\" doesn't match format \"%Y-%m-%dt%H:%M:%S\""));
  check({"2024-02-01 10:00:00z", "2024-02-30 10:00:00z"}, value_error("day is out of range for month"));
  check({"01/02/2024 13:45:07.1234567891"}, "event CSV timestamp is not ISO-8601: 01/02/2024 13:45:07.1234567891");
  check({"2024-01-02 13:45:07 +02:00", "2024-01-02 13:45:07 +03:00"},
        "ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to "
        "convert to a common timezone.");
  // Shapes pandas guesses no format for (dateutil element by element) keep
  // the native reading (declared): ten fraction digits after a non-ISO date,
  // a lone one-digit hour; so do lowercase zone names.
  check({"2024-01-02 3"}, "event CSV timestamp is not ISO-8601: 2024-01-02 3");
  check({"2024-01-02 13:45:07 utc"}, "event CSV timestamp is not ISO-8601: 2024-01-02 13:45:07 utc");
  // ISO family texts that native reads as pandas does keep their text.
  {
    const auto log = parse_ocel_csv(csv_of({"2024-01-02 13:45:07+0000", "2024-01-03 08:00:00Z"}));
    expect(log.events.size() == 2 && log.events[0].timestamp == "2024-01-02 13:45:07+0000" &&
               log.events[1].timestamp == "2024-01-03 08:00:00Z",
           "family texts are kept");
  }
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
    std::vector<std::string> values;
    const auto want = expected_outcome(line.substr(tab + 1), values);
    const auto got = outcome(texts, values);
    ++count;
    if (got != want && ++mismatches <= 10)
      std::cerr << "bank: " << line.substr(0, tab) << "\n  got  " << got << "\n  want " << want << '\n';
  }
  expect(count == 5000, "the bank holds 5,000 columns");
  expect(mismatches == 0, "bank mismatches: " + std::to_string(mismatches));
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  shapes();
  if (argc > 1 && replay_bank(argv[1]) != 0) return 1;
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review DZ1 tests passed\n";
  return 0;
}
