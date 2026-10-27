// algo.connectors against the public seeded bank docs/connector-bank.txt.gz
// (scripts/capture-connector-bank.py): every connector, fed the same stubbed
// source as the reference, must return the reference's dataframe exactly
// (columns, dtypes, row labels and every value) or raise the same Python
// exception type.
#include "minera/connector_reference.hpp"

#include "../gzip_codec.hpp"

#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
};

class JsonReader {
 public:
  explicit JsonReader(const char* text) : at_(text) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space(); ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kString;
      for (++at_; *at_ != '"'; ++at_) {
        if (*at_ == '\\') {
          ++at_;
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_;
          continue;
        }
        out.text += *at_;
      }
      ++at_;
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      out.kind = Json::kNumber;
      char* end = nullptr;
      out.number = std::strtod(at_, &end);
      at_ = end;
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};


struct BankCase {
  std::string name, kind;
  Json source, frame;
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    auto value = [&] { return JsonReader(line.c_str() + 2).value(); };
    switch (line[0]) {
      case 'C': cases.emplace_back(); cases.back().name = line.substr(2); break;
      case 'K': cases.back().kind = line.substr(2); break;
      case 'S': cases.back().source = value(); break;
      case 'F': cases.back().frame = value(); break;
      default: break;
    }
  }
  return cases;
}

ConnectorReferenceFrame frame_of(const Json& value) {
  ConnectorReferenceFrame out;
  for (const auto& c : value.get("columns")->items) out.columns.push_back(c.text);
  for (const auto& c : value.get("dtypes")->items) out.dtypes.push_back(c.text);
  for (const auto& c : value.get("index")->items) out.index.push_back(static_cast<std::size_t>(c.number));
  for (const auto& row : value.get("rows")->items) {
    out.rows.emplace_back();
    for (const auto& cell : row.items)
      out.rows.back().push_back(cell.kind == Json::kString ? std::optional(cell.text) : std::nullopt);
  }
  return out;
}

std::string show(const std::vector<std::optional<std::string>>& row) {
  std::string out;
  for (const auto& cell : row) out += (cell ? *cell : "None") + " | ";
  return out;
}

std::string difference(const ConnectorReferenceFrame& mine, const ConnectorReferenceFrame& expected) {
  if (mine.columns != expected.columns) return "columns differ";
  if (mine.dtypes != expected.dtypes) {
    for (std::size_t k = 0; k < mine.dtypes.size(); ++k)
      if (mine.dtypes[k] != expected.dtypes[k]) return "dtype of " + mine.columns[k] + ": " + mine.dtypes[k] + " vs " + expected.dtypes[k];
  }
  if (mine.index != expected.index) return "row order differs";
  for (std::size_t r = 0; r < mine.rows.size() && r < expected.rows.size(); ++r)
    if (mine.rows[r] != expected.rows[r]) return "row " + std::to_string(r) + ": " + show(mine.rows[r]) + " vs " + show(expected.rows[r]);
  return mine.rows.size() == expected.rows.size() ? "" : "row count differs";
}

struct Tally {
  std::map<std::string, std::size_t> frames, raising;
};

// Builds the SQLite file the reference read.
void write_database(const std::filesystem::path& path, const Json& dump) {
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("cannot create test database");
  std::string script;
  for (const auto& statement : dump.items) script += statement.text + "\n";
  char* error = nullptr;
  const int status = sqlite3_exec(db, script.c_str(), nullptr, nullptr, &error);
  sqlite3_free(error);
  sqlite3_close(db);
  if (status != SQLITE_OK) throw std::runtime_error("cannot replay database dump");
}

template <typename Connector>
void database_file_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                        std::vector<std::string>& failures, Connector connector) {
  const auto path = directory / (item.name + ".db");
  write_database(path, item.source);
  const auto* raises = item.frame.get("raises");
  try {
    const auto mine = connector(path);
    if (raises) {
      failures.push_back(item.name + ": native returns where the reference raises " + raises->text);
      return;
    }
    const auto problem = difference(mine, frame_of(item.frame));
    if (!problem.empty()) failures.push_back(item.name + ": " + problem);
    else ++tally.frames[item.kind];
  } catch (const ConnectorReferenceError& error) {
    if (!raises) failures.push_back(item.name + ": native raises " + error.what());
    else if (raises->text.rfind(error.type() + ":", 0) != 0) failures.push_back(item.name + ": raises " + error.what() + " vs " + raises->text);
    else ++tally.raising[item.kind];
  }
  std::filesystem::remove(path);
}

void chrome_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                 std::vector<std::string>& failures) {
  database_file_case(item, directory, tally, failures,
                     [](const std::filesystem::path& path) { return reference_chrome_history(path); });
}

void firefox_case(const BankCase& item, const std::filesystem::path& directory, Tally& tally,
                  std::vector<std::string>& failures) {
  database_file_case(item, directory, tally, failures,
                     [](const std::filesystem::path& path) { return reference_firefox_history(path); });
}

template <typename Connector>
void database_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures, Connector connector) {
  const auto& source = item.source;
  const auto prefix = source.get("prefix")->text;
  ConnectorReferenceResultSet result;
  if (const auto* names = source.get("names"); names && names->kind == Json::kArray)
    for (const auto& n : names->items) result.names.push_back(n.text);
  for (const auto& row : source.get("rows")->items) {
    result.rows.emplace_back();
    for (const auto& cell : row.items) {
      if (cell.kind == Json::kString) result.rows.back().emplace_back(cell.text);
      else if (cell.kind == Json::kNumber) result.rows.back().emplace_back(static_cast<std::int64_t>(cell.number));
      else result.rows.back().emplace_back(std::monostate{});
    }
  }
  std::string problem;
  bool closed = false;
  ConnectorReferenceConnection connection;
  connection.execute = [&](const std::string& sql) {
    if (sql != source.get("sql")->text) problem = "SQL text differs";
    return result;
  };
  connection.close = [&] { closed = true; };
  const auto* raises = item.frame.get("raises");
  try {
    const auto mine = connector(connection, prefix);
    if (raises) problem = "native returns where the reference raises " + raises->text;
    else if (problem.empty()) problem = difference(mine, frame_of(item.frame));
  } catch (const ConnectorReferenceError& error) {
    if (!raises) problem = std::string("native raises ") + error.what();
  }
  if (problem.empty() && closed != source.get("closed")->boolean) problem = "connection close differs";
  if (!problem.empty()) failures.push_back(item.name + ": " + problem);
  else ++(raises ? tally.raising : tally.frames)[item.kind];
}

void camunda_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  database_case(item, tally, failures,
                [](const ConnectorReferenceConnection& c, const std::string&) { return reference_camunda_workflow(c); });
}

void sap_accounting_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  database_case(item, tally, failures,
                [](const ConnectorReferenceConnection& c, const std::string& p) { return reference_sap_accounting(c, p); });
}

void sap_o2c_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  database_case(item, tally, failures,
                [](const ConnectorReferenceConnection& c, const std::string& p) { return reference_sap_o2c(c, p); });
}

ConnectorReferenceJson response_of(const Json& value) {
  ConnectorReferenceJson out;
  switch (value.kind) {
    case Json::kNull: break;
    case Json::kBool: out.kind = ConnectorReferenceJson::kBool; out.boolean = value.boolean; break;
    case Json::kNumber:
      if (value.number == static_cast<double>(static_cast<std::int64_t>(value.number))) {
        out.kind = ConnectorReferenceJson::kInt;
        out.integer = static_cast<std::int64_t>(value.number);
      } else {
        out.kind = ConnectorReferenceJson::kFloat;
        out.real = value.number;
      }
      break;
    case Json::kString: out.kind = ConnectorReferenceJson::kText; out.text = value.text; break;
    case Json::kArray:
      out.kind = ConnectorReferenceJson::kArray;
      for (const auto& item : value.items) out.items.push_back(response_of(item));
      break;
    case Json::kObject:
      out.kind = ConnectorReferenceJson::kObject;
      for (const auto& [key, item] : value.fields) out.members.emplace_back(key, response_of(item));
      break;
  }
  return out;
}

void github_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto& source = item.source;
  const auto owner = source.get("owner")->text, repository = source.get("repository")->text;
  const auto* token = source.get("token");
  const auto prefix = "https://api.github.com/repos/" + owner + "/" + repository + "/issues?state=all&per_page=100&page=";
  std::vector<std::pair<std::string, std::map<std::string, std::string>>> calls;
  const ConnectorReferenceGet get = [&](const std::string& url, const std::map<std::string, std::string>& headers) {
    calls.emplace_back(url, headers);
    const Json* found = nullptr;
    if (url.rfind(prefix, 0) == 0) found = source.get("pages")->get(url.substr(prefix.size()));
    else found = source.get("timelines")->get(url);
    return found ? response_of(*found) : ConnectorReferenceJson{ConnectorReferenceJson::kArray};
  };
  std::string problem;
  const auto* raises = item.frame.get("raises");
  try {
    const auto mine = reference_github_repo(
        owner, repository, token->kind == Json::kString ? std::optional(token->text) : std::nullopt, get);
    if (raises) problem = "native returns where the reference raises " + raises->text;
    else problem = difference(mine, frame_of(item.frame));
  } catch (const ConnectorReferenceError& error) {
    if (!raises) problem = std::string("native raises ") + error.what();
  }
  const auto& expected = source.get("calls")->items;
  if (problem.empty() && calls.size() != expected.size()) problem = "GET count differs";
  for (std::size_t k = 0; problem.empty() && k < calls.size(); ++k) {
    std::map<std::string, std::string> headers;
    for (const auto& [key, value] : expected[k].items[1].fields) headers[key] = value.text;
    if (calls[k].first != expected[k].items[0].text || calls[k].second != headers)
      problem = "GET " + std::to_string(k) + " differs: " + calls[k].first;
  }
  if (!problem.empty()) failures.push_back(item.name + ": " + problem);
  else ++(raises ? tally.raising : tally.frames)[item.kind];
}

// A COM value from the bank's tagged JSON.
ConnectorReferenceCom com_of(const Json& value) {
  ConnectorReferenceCom out;
  switch (value.kind) {
    case Json::kNull: break;
    case Json::kBool: out.kind = ConnectorReferenceCom::kBool; out.boolean = value.boolean; break;
    case Json::kNumber: out.kind = ConnectorReferenceCom::kInt; out.integer = static_cast<std::int64_t>(value.number); break;
    case Json::kString: out.kind = ConnectorReferenceCom::kText; out.text = value.text; break;
    case Json::kArray:
      out.kind = ConnectorReferenceCom::kList;
      for (const auto& item : value.items) out.items.push_back(com_of(item));
      break;
    case Json::kObject:
      if (const auto* real = value.get("float")) {
        out.kind = ConnectorReferenceCom::kFloat;
        out.real = real->number;
      } else if (const auto* time = value.get("dt")) {
        // datetime.isoformat(): YYYY-MM-DDTHH:MM:SS[.ffffff][+HH:MM]
        const auto& t = time->text;
        auto number = [&](std::size_t at, std::size_t count) { return std::stoll(t.substr(at, count)); };
        std::int64_t y = number(0, 4), m = number(5, 2), d = number(8, 2);
        y -= m <= 2;
        const std::int64_t era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
        const std::int64_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
        const std::int64_t days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
        out.kind = ConnectorReferenceCom::kTime;
        out.micros = days * 86400000000LL + ((number(11, 2) * 60 + number(14, 2)) * 60 + number(17, 2)) * 1000000LL;
        std::size_t at = 19;
        if (at < t.size() && t[at] == '.') {
          out.micros += number(20, 6);
          at = 26;
        }
        if (at < t.size()) {
          const int minutes = static_cast<int>(number(at + 1, 2) * 60 + number(at + 4, 2));
          out.offset_minutes = t[at] == '-' ? -minutes : minutes;
        }
      } else {
        out.kind = ConnectorReferenceCom::kObject;
        for (const auto& [key, item] : value.get("obj")->fields) out.members.emplace_back(key, com_of(item));
      }
      break;
  }
  return out;
}

std::vector<ConnectorReferenceCom> com_list(const Json& value) {
  std::vector<ConnectorReferenceCom> out;
  for (const auto& item : value.items) out.push_back(com_of(item));
  return out;
}

template <typename Connector>
void com_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures, Connector connector) {
  std::string problem;
  const auto* raises = item.frame.get("raises");
  try {
    const auto mine = connector(item.source);
    if (raises) problem = "native returns where the reference raises " + raises->text;
    else problem = difference(mine, frame_of(item.frame));
  } catch (const ConnectorReferenceError& error) {
    if (!raises) problem = std::string("native raises ") + error.what();
    else if (raises->text.rfind(error.type() + ":", 0) != 0) problem = std::string("native raises ") + error.what() + " vs " + raises->text;
  }
  if (!problem.empty()) failures.push_back(item.name + ": " + problem);
  else ++(raises ? tally.raising : tally.frames)[item.kind];
}

void outlook_calendar_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  com_case(item, tally, failures,
           [](const Json& source) {
             const auto* reminders = source.get("include_reminders");
             return reference_outlook_calendar(com_list(*source.get("items")),
                                               reminders->kind != Json::kBool || reminders->boolean);
           });
}

void outlook_mail_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  com_case(item, tally, failures, [](const Json& source) {
    return reference_outlook_mail(com_list(*source.get("sent")), com_list(*source.get("inbox")));
  });
}

void windows_events_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  com_case(item, tally, failures,
           [](const Json& source) { return reference_windows_events(com_list(*source.get("rows"))); });
}

void mail_connect_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  const auto* user = item.source.get("user");
  const auto mine = reference_mail_connect_calls(user->kind == Json::kString ? std::optional(user->text) : std::nullopt,
                                                 static_cast<int>(item.source.get("mailbox")->number));
  std::vector<std::string> expected;
  for (const auto& call : item.frame.get("calls")->items) expected.push_back(call.text);
  if (mine != expected) failures.push_back(item.name + ": COM calls differ");
  else ++tally.frames[item.kind];
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_connector_parity_tests <bank>\n";
    return 2;
  }
  const auto directory = std::filesystem::temp_directory_path() / ("minera-connector-parity-" + std::to_string(std::random_device{}()));
  std::filesystem::create_directories(directory);
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      if (item.kind == "chrome_history") chrome_case(item, directory, tally, failures);
      else if (item.kind == "firefox_history") firefox_case(item, directory, tally, failures);
      else if (item.kind == "camunda_workflow") camunda_case(item, tally, failures);
      else if (item.kind == "sap_accounting") sap_accounting_case(item, tally, failures);
      else if (item.kind == "sap_o2c") sap_o2c_case(item, tally, failures);
      else if (item.kind == "github_repo") github_case(item, tally, failures);
      else if (item.kind == "outlook_calendar") outlook_calendar_case(item, tally, failures);
      else if (item.kind == "outlook_mail_extractor") outlook_mail_case(item, tally, failures);
      else if (item.kind == "windows_events") windows_events_case(item, tally, failures);
      else if (item.kind == "mail_connect") mail_connect_case(item, tally, failures);
      else failures.push_back(item.name + ": unknown connector " + item.kind);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  std::filesystem::remove_all(directory);
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "connectors:";
  for (const auto& [kind, count] : tally.frames) std::cout << " " << kind << " " << count << " frames, " << tally.raising[kind] << " raising;";
  std::cout << " " << failures.size() << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
