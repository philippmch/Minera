// Review EB2: the OCEL 2.0 SQLite reader's event times as pandas reads them.
// pm4py 2.7.22's read_ocel2_sqlite reads each event type table with
// read_sql (int64 when every time is an integer, float64 for numbers with
// NULLs or reals, else object), concatenates them, keeps a column of numbers
// as numbers, and reads an object column with pd.to_datetime(utc=True,
// format="mixed"): every value on its own, integers and floats as
// nanoseconds since the epoch, a text as pandas reads it without a format,
// one that starts with a time and has no date on today's date through
// dateutil. When that raises the column stays as it is, and the events sort
// by it: numbers first, then texts as Python sorts them, NULL last. Observed
// black-box (review-harness round-eb, sq/p5.py: 1,093 fuzzed files read
// alike besides texts beyond ASCII).
//
// With a bank path (docs/ocel2-sqlite-time-eb-bank.tsv.gz) the test also
// builds and reads fuzzed files: declared column type, then the type tables
// (";"-joined, values ","-joined: n, i<int>, f<hex of the double>,
// s<hex of the text>), then pandas' dtype and each event in order with its
// time (I<seconds>.<nanoseconds>, N for NaT or a missing text, S<hex> for a
// text the column keeps, U for a number or NaN in a column of numbers). Only
// columns whose reading does not move with the date. Round EC adds
// docs/ocel2-sqlite-time-ec-bank.tsv.gz (the texts of Review EC1: numbers of
// 1000 or more, quarters, years before 1000, offsets of a day, nanosecond
// bounds), in the same format.
#include "minera/ocel.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/timestamp.hpp"
#include "../gzip_codec.hpp"
#include "../pandas_to_datetime.hpp"
#include "../sqlite_real_text.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
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
namespace ptd = minera::pandas_to_datetime;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::string unhex(const std::string& hex) {
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
  return out;
}

std::string hex_of(const std::string& text) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  for (const unsigned char c : text) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
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

// ------------------------------------------------------------ to_datetime_utc_mixed

std::string reading(const std::vector<ptd::Cell>& cells) {
  try {
    const auto values = ptd::to_datetime_utc_mixed(cells, ptd::Date{2026, 11, 18});
    if (!values) return "outside";
    std::string out = "ok";
    for (const auto& value : *values) {
      out += " ";
      if (!value) {
        out += "NaT";
        continue;
      }
      out += std::to_string(value->seconds) + "." + std::to_string(value->nanoseconds) + (value->aware ? "A" : "");
    }
    return out;
  } catch (const std::invalid_argument&) {
    return "raises";
  }
}

ptd::Cell text(const char* value) {
  ptd::Cell cell;
  cell.kind = ptd::Cell::Kind::kText;
  cell.text = value;
  return cell;
}

ptd::Cell integer(std::int64_t value) {
  ptd::Cell cell;
  cell.kind = ptd::Cell::Kind::kInteger;
  cell.integer = value;
  return cell;
}

ptd::Cell real(double value) {
  ptd::Cell cell;
  cell.kind = ptd::Cell::Kind::kReal;
  cell.real = value;
  return cell;
}

void check_reading(const std::vector<ptd::Cell>& cells, const std::string& want, const std::string& what) {
  const auto got = reading(cells);
  expect(got == want, what + ": got \"" + got + "\", want \"" + want + "\"");
}

void readings() {
  // 2026-11-18 10:00 UTC is 1794996000.
  check_reading({text("10:00")}, "ok 1794996000.0", "a time on today's date");
  check_reading({text("10:00 GMT+3"), text("2020-01-01")}, "ok 1795006800.0A 1577836800.0",
                "dateutil's GMT+3 is three hours west");
  check_reading({text("2020-01-01 10:00+02:00"), text("2020-01-01 09:00")}, "ok 1577865600.0A 1577869200.0",
                "each value converts to UTC on its own");
  check_reading({integer(15), real(1.5), real(-1.5), real(2.9999999)}, "ok 0.15 0.1 -1.999999999 0.2",
                "numbers are nanoseconds, floats truncated");
  check_reading({ptd::Cell{}, text(""), text("NaT"), text("nan"), real(std::nan(""))}, "ok NaT NaT NaT NaT NaT",
                "NULL, NaN and NaT spellings");
  check_reading({text("15"), text("2020-01-02")}, "raises", "a number text raises");
  check_reading({text("57594")}, "raises", "five digits raise");
  check_reading({text("None")}, "raises", "None is no NaT spelling");
  check_reading({text("1:37 +2500")}, "raises", "an offset of a day raises");
  check_reading({text("2020-01-01 10:00 +2500")}, "raises", "an ISO offset of a day raises");
  check_reading({text("3539-07-04 15:19:50")}, "ok 49528768790.0", "microseconds hold year 3539");
  check_reading({text("3539-07-04"), integer(5)}, "raises", "a number makes the column nanoseconds");
  check_reading({text("Wednesday 8 OCTOBER 1993 11:25:32.0524 -05:00"), text("3076-12-24T20:40:10.435524741")},
                "raises", "a nanosecond text past 2262");
  check_reading({text("18h28m -1459")}, "ok -62135476380.0A", "dateutil's year 1");
  ptd::Cell blob;
  blob.kind = ptd::Cell::Kind::kBlob;
  check_reading({text("2020-01-01"), blob}, "raises", "a blob raises");
  check_reading({text("\xc3\xa9")}, "outside", "texts beyond ASCII");
  check_reading({text("\xc3\xa9"), text("x")}, "raises", "an error raises whatever the other values are");
}

// ------------------------------------------------------------ files
#if defined(MINERA_HAS_SQLITE)

struct Value {
  char kind = 'n';  // n, i, f, s
  std::int64_t integer = 0;
  double real = 0;
  std::string text;
};

void exec_sql(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    ++failures;
    std::cerr << "SQLite setup failed: " << (error ? error : "") << "\n";
    if (error) sqlite3_free(error);
  }
}

// One file: event types a0, a1, ... with tables event_A0, ..., ids e0, e1, ...
// in table order, each related to o1.
void write_file(const std::filesystem::path& path, const std::string& coltype,
                const std::vector<std::vector<Value>>& tables) {
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
    ++failures;
    std::cerr << "cannot create " << path << "\n";
    return;
  }
  exec_sql(db,
           "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT);"
           "CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
           "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
           "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
           "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
           "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);"
           "INSERT INTO object_map_type VALUES ('T','T');"
           "CREATE TABLE \"object_T\" (ocel_id TEXT, ocel_time TIMESTAMP, ocel_changed_field TEXT);"
           "INSERT INTO object VALUES ('o1','T');"
           "INSERT INTO \"object_T\" VALUES ('o1','1970-01-01 00:00:00',NULL);BEGIN;");
  // A REAL stored in a column of TEXT affinity becomes the library's text
  // of it, which moves with the library (Windows' winsqlite3 spells a value
  // that does not round-trip in 15 digits with 17), so such a cell is
  // stored as the text the reference's SQLite 3.45 makes of it (the Windows
  // CI runs of rounds EB to ED; the reader reads that text as it is).
  std::string upper = coltype;
  for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  const bool text_affinity = upper.find("INT") == std::string::npos &&
                             (upper.find("CHAR") != std::string::npos || upper.find("CLOB") != std::string::npos ||
                              upper.find("TEXT") != std::string::npos);
  std::size_t n = 0;
  for (std::size_t k = 0; k < tables.size(); ++k) {
    const auto type = "a" + std::to_string(k), table = "A" + std::to_string(k);
    exec_sql(db, "INSERT INTO event_map_type VALUES ('" + type + "','" + table + "');CREATE TABLE \"event_" + table +
                     "\" (ocel_id TEXT, ocel_time " + coltype + ");");
    for (const auto& value : tables[k]) {
      const auto id = "e" + std::to_string(n++);
      exec_sql(db, "INSERT INTO event VALUES ('" + id + "','" + type + "');INSERT INTO event_object VALUES ('" + id +
                       "','o1','');");
      sqlite3_stmt* statement = nullptr;
      sqlite3_prepare_v2(db, ("INSERT INTO \"event_" + table + "\" VALUES (?,?)").c_str(), -1, &statement, nullptr);
      sqlite3_bind_text(statement, 1, id.c_str(), -1, SQLITE_TRANSIENT);
      if (value.kind == 'i') sqlite3_bind_int64(statement, 2, value.integer);
      else if (value.kind == 'f' && text_affinity && std::isfinite(value.real))
        sqlite3_bind_text(statement, 2, sqlite_legacy_real_text(value.real).c_str(), -1, SQLITE_TRANSIENT);
      else if (value.kind == 'f') sqlite3_bind_double(statement, 2, value.real);
      else if (value.kind == 's')
        sqlite3_bind_text(statement, 2, value.text.data(), static_cast<int>(value.text.size()), SQLITE_TRANSIENT);
      else sqlite3_bind_null(statement, 2);
      if (sqlite3_step(statement) != SQLITE_DONE) {
        ++failures;
        std::cerr << "SQLite insert failed\n";
      }
      sqlite3_finalize(statement);
    }
  }
  exec_sql(db, "COMMIT;");
  sqlite3_close(db);
}

// The events in order with their times, as the bank spells them (a number
// is U whatever native keeps; S<hex> where the reference keeps texts).
std::string outcome(const std::filesystem::path& path, bool texts_kept, bool numbers) {
  Ocel log;
  try {
    log = import_ocel_sqlite(path);
  } catch (const std::exception& error) {
    return std::string("ERR ") + error.what();
  }
  std::string out;
  for (const auto& event : log.events) {
    if (!out.empty()) out += " ";
    out += event.id + "=";
    if (numbers) {
      out += "U";
    } else if (texts_kept) {
      out += event.timestamp.empty() ? "N" : "S" + hex_of(event.timestamp);
    } else if (event.timestamp.empty()) {
      out += "N";
    } else if (const auto parsed = parse_timestamp_exact(event.timestamp)) {
      char buffer[48];
      std::snprintf(buffer, sizeof buffer, "I%lld.%09u", static_cast<long long>(parsed->seconds),
                    static_cast<unsigned>(parsed->nanoseconds));
      out += buffer;
    } else {
      out += "?" + event.timestamp;
    }
  }
  return out;
}

const std::filesystem::path& file_path() {
  static const auto path = std::filesystem::temp_directory_path() / "minera_review_eb2.sqlite";
  return path;
}

std::vector<std::vector<Value>> tables_of(const std::string& spec) {
  std::vector<std::vector<Value>> tables;
  for (const auto& table : split(spec, ';')) {
    tables.emplace_back();
    if (table.empty()) continue;
    for (const auto& cell : split(table, ',')) {
      Value value;
      value.kind = cell[0];
      if (value.kind == 'i') value.integer = std::stoll(cell.substr(1));
      if (value.kind == 'f') {
        const auto bits = std::stoull(cell.substr(1), nullptr, 16);
        std::memcpy(&value.real, &bits, sizeof value.real);
      }
      if (value.kind == 's') value.text = unhex(cell.substr(1));
      tables.back().push_back(value);
    }
  }
  return tables;
}

// The same events in the same order. Where the reference keeps a column as
// it is, an empty text (S) is no time natively (N), and a number (U) is its
// text (declared).
bool same(const std::string& got, const std::string& want) {
  if (got == want) return true;
  const auto a = split(got, ' '), b = split(want, ' ');
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i] == b[i]) continue;
    const auto id = b[i].substr(0, b[i].find('=') + 1);
    if (!a[i].starts_with(id)) return false;
    const auto x = a[i].substr(id.size()), y = b[i].substr(id.size());
    if (!((x == "N" && y == "S") || (x.starts_with("S") && y == "U"))) return false;
  }
  return true;
}

void check_file(const std::string& coltype, const std::string& spec, const std::string& want, const std::string& what) {
  const auto dtype = want.substr(0, want.find(' '));
  write_file(file_path(), coltype, tables_of(spec));
  const auto got = outcome(file_path(), dtype == "str" || dtype == "object", dtype == "int64" || dtype == "float64");
  const auto events = want.substr(want.find(' ') + 1);
  expect(same(got, events), what + ": got \"" + got + "\", want \"" + events + "\"");
}

std::string s(const std::string& text) { return "s" + hex_of(text); }
std::string S(const std::string& text) { return "S" + hex_of(text); }

void files() {
  // int64 and object tables make an object column: datetimes.
  check_file("BLOBX", "i1,i2;n", "datetime64[ns,UTC] e0=I0.000000001 e1=I0.000000002 e2=N",
             "an all-NULL table makes the column object");
  check_file("BLOBX", "i3,i1,i2,i1", "int64 e1=U e3=U e2=U e0=U", "integers sort as numbers, stably");
  check_file("BLOBX", "i1,i2;", "int64 e0=U e1=U", "a table without rows plays no part");
  check_file("BLOBX", "i1,i2;f3ff8000000000000", "float64 e0=U e2=U e1=U", "integers and reals are float64");
  check_file("BLOBX", "f400c000000000000,n,i1", "float64 e2=U e0=U e1=U", "NaN last");
  check_file("BLOBX", s("b") + "," + s("a") + "," + s("b") + "," + s("a"),
             "str e1=" + S("a") + " e3=" + S("a") + " e0=" + S("b") + " e2=" + S("b"), "texts sort as Python sorts them");
  check_file("BLOBX", s("x") + ",i5,f4004000000000000," + s("a"),
             "object e2=U e1=U e3=" + S("a") + " e0=" + S("x"), "numbers before texts");
  check_file("BLOBX", s("Jan 5 2020") + "," + s("2020/01/05 10:00") + "," + s("01/02/2020"),
             "datetime64[us,UTC] e2=I1577923200.000000000 e0=I1578182400.000000000 e1=I1578218400.000000000",
             "texts native did not read");
  // NUMERIC affinity stores "20200105" as an integer: nanoseconds.
  check_file("TIMESTAMP", s("20200105") + "," + s("2020-01-01"),
             "datetime64[ns,UTC] e0=I0.020200105 e1=I1577836800.000000000", "affinity turns digits into an integer");
  check_file("TEXT", s("20200105") + "," + s("2020-01-01"),
             "datetime64[us,UTC] e1=I1577836800.000000000 e0=I1578182400.000000000", "eight digits read as a date");
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
    const auto fields = split(line, '\t');
    if (fields.size() != 3) continue;
    const auto dtype = fields[2].substr(0, fields[2].find(' '));
    write_file(file_path(), fields[0], tables_of(fields[1]));
    const auto got = outcome(file_path(), dtype == "str" || dtype == "object", dtype == "int64" || dtype == "float64");
    const auto want = fields[2].substr(fields[2].find(' ') + 1);
    ++count;
    if (!same(got, want) && ++mismatches <= 10)
      std::cerr << "bank: " << fields[0] << " " << fields[1] << "\n  got  " << got << "\n  want " << want << '\n';
  }
  expect(count >= 1000, "the bank holds at least 1,000 files: " + std::to_string(count));
  expect(mismatches == 0, "bank mismatches: " + std::to_string(mismatches));
  return 0;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  readings();
#if defined(MINERA_HAS_SQLITE)
  files();
  for (int i = 1; i < argc; ++i)
    if (replay_bank(argv[i]) != 0) return 1;
  std::filesystem::remove(file_path());
#else
  (void)argc;
  (void)argv;
#endif
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EB2 tests passed\n";
  return 0;
}
