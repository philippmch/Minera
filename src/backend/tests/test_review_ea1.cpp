// Review EA1: the OCEL CSV reader's timestamp columns beyond round DZ's
// shapes. pm4py 2.7.22's read_ocel_csv reads the timestamp column as texts
// (read_csv's NA spellings missing) and parses it with pd.to_datetime, which
// guesses one format from the first text through dateutil (month and weekday
// names, AM/PM, a comma before the fraction) or none at all; without a
// format every text is read on its own (an ISO parser, pandas' MM/DD/YYYY
// and month-year readers, then dateutil: two-digit years, "GMT+2" as two
// hours west, a lone one-digit hour refused). pandas' NaT spellings are
// missing, for the guess too. Observed black-box (review-harness round-ea,
// ea1/eamodel.py: 0 differences over 37,249 fuzzed columns of this round and
// 86,729 of round DZ; ea2/natcmp.py: the C++ port matches the model on those
// and on 19,000 fresh columns).
//
// With a bank path (docs/ocel-csv-timestamp-ea-bank.tsv.gz) the test also
// replays 5,000 fuzzed columns whose reading does not move with the current
// year: texts joined by "|", a tab, then "ok <us|ns|aware> <pandas' values
// joined by |>" or the error's text up to ". You might want to try:".
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/timestamp.hpp"
#include "../gzip_codec.hpp"
#include <algorithm>
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

// Texts pandas reads as missing: read_csv's NA spellings and to_datetime's
// NaT spellings. Their rows have no event id, so they are dropped.
bool missing(const std::string& text) {
  static const std::vector<std::string> kTexts = {
      "",     "#N/A", "#N/A N/A", "#NA", "-1.#IND", "-1.#QNAN", "-NaN", "-nan", "1.#IND", "1.#QNAN", "<NA>",
      "N/A",  "NA",   "NULL",     "NaN", "None",    "n/a",      "nan",  "null", "NaT",    "nat",     "NAT",
      "NAN"};
  return std::find(kTexts.begin(), kTexts.end(), text) != kTexts.end();
}

std::string csv_of(const std::vector<std::string>& texts) {
  std::string csv = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n";
  for (std::size_t i = 0; i < texts.size(); ++i) {
    const std::string cell = "\"" + texts[i] + "\"";
    csv += missing(texts[i]) ? "," + cell + ",a,\n" : "e" + std::to_string(i) + "," + cell + ",a,['o1']\n";
  }
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
  OcelCsvImportOptions options;
  options.trim_fields = false;  // as read_csv, which keeps spaces
  try {
    log = parse_ocel_csv(csv_of(texts), options);
  } catch (const std::exception& error) {
    std::string message = error.what();
    const auto cut = message.find(". You might want to try:");
    return cut == std::string::npos ? message : message.substr(0, cut);
  }
  std::string out = "ok " + (log.naive_event_time_unit.empty() ? std::string("aware") : log.naive_event_time_unit) + " ";
  for (std::size_t i = 0; i < texts.size(); ++i) {
    if (i) out += "|";
    if (missing(texts[i])) {
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

std::vector<std::string> values_of(const std::string& expected) {
  if (expected.rfind("ok ", 0) != 0) return {};
  return split(expected.substr(expected.find(' ', 3) + 1), '|');
}

void check(const std::vector<std::string>& texts, const std::string& expected) {
  const auto got = outcome(texts, values_of(expected));
  expect(got == expected, "timestamps " + texts.front() + ": got \"" + got + "\", want \"" + expected + "\"");
}

void shapes() {
  // Month names, AM/PM, a comma before the fraction, two-digit years.
  check({"Jan 5, 2024 3:04 PM", "Feb 29, 2024 11:00 AM"}, "ok us 2024-01-05T15:04:00|2024-02-29T11:00:00");
  check({"2024-01-02 13:45:07,5", "2024-01-03 08:00:00,25"},
        "ok us 2024-01-02T13:45:07.500000|2024-01-03T08:00:00.250000");
  check({"02/01/24 13:45", "03/01/24 08:00"}, "ok us 2024-02-01T13:45:00|2024-03-01T08:00:00");
  // dateutil reads "GMT+2" as two hours west.
  check({"5 May 2024 10:00 GMT+2"}, "ok aware 2024-05-05T10:00:00-02:00");
  // No format guessed: every text on its own.
  check({"Jan 5 2024 3PM", "6 January 2024 16:30"}, "ok us 2024-01-05T15:00:00|2024-01-06T16:30:00");
  check({"12/31/2024 11:59:59 PM", "01/01/2025 12:00:00 AM"}, "ok us 2024-12-31T23:59:59|2025-01-01T00:00:00");
  check({"2024-02-30"}, "DateParseError: day is out of range for month: 2024-02-30");
  // A guessed format holds every later text.
  check({"Tuesday, 02 January 2024 13:45", "Wed 3 Jan 2024 08:00"},
        "ValueError: time data \"Wed 3 Jan 2024 08:00\" doesn't match format \"%A, %d %B %Y %H:%M\"");
  check({"3rd of March 2024", "March 4th, 2024"},
        "ValueError: time data \"March 4th, 2024\" doesn't match format \"%drd of %B %Y\"");
  // NA and NaT spellings are missing, for the guess too.
  check({"NaT", "01/02/2024", "01/03/2024"}, "ok us NaT|2024-01-02T00:00:00|2024-01-03T00:00:00");
  check({"NA", "01/02/2024"}, "ok us NaT|2024-01-02T00:00:00");
  check({"01/02/2024", "Nat"}, "ValueError: time data \"Nat\" doesn't match format \"%m/%d/%Y\"");
  // An event whose time is missing is NaT in the reference: an empty native
  // time (round EH; it was refused before).
  {
    const auto log = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,01/02/2024,a,['o1']\n"
                                    "e2,null,a,['o1']\n");
    expect(log.events.size() == 2 && log.events[1].id == "e2" && log.events[1].timestamp.empty(),
           "an NA event time is NaT");
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
    const auto want = line.substr(tab + 1);
    const auto got = outcome(texts, values_of(want));
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
  std::cout << "review EA1 tests passed\n";
  return 0;
}
