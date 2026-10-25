#pragma once

// algo.connectors of the PM4Py 2.7.22 reference, as observed from the outside
// with stubbed sources (SQLite files, database connections, HTTP responses
// and Windows automation objects). Each connector returns the dataframe the
// reference returns, as a ConnectorReferenceFrame: columns, pandas dtypes,
// the row labels and the rows in order, values rendered as the reference
// prints them (str of a timestamp, repr of a float, True/False, nullopt for a
// missing value).
//
// Browser history (variants.chrome_history and variants.firefox_history)
// reads one database file. Chrome opens it read-only, reads the visits
// joined to their urls (the SQL below) and maps from_visit through the urls
// table's ids (not the visits'), so a referrer is the url whose id equals the
// from_visit value, '' when there is none. Firefox reads moz_historyvisits
// joined to moz_places. Per row, in order, the reference converts the visit
// time (Chrome: 1601-01-01 plus visit_time microseconds, OverflowError
// outside years 1..9999; Firefox: datetime.fromtimestamp(visit_date / 10^6,
// utc), ValueError outside years 1..9999), splits the url (AttributeError
// when it is NULL) and, for Chrome, decodes the transition (low byte; NULL
// raises TypeError, as does a NULL visit time). The part after the last "//"
// is cut at the first '?' into url_wo_parameters, the activity is that
// without commas, and the domain is the part before the first '/'. Rows with
// an empty activity are dropped after these conversions. Chrome's title
// NULL becomes '', a NULL duration 0.0 seconds. Columns are typed as pandas
// infers them from the row values (ints with a NULL become float64, a column
// of NULLs object), and the frame is sorted by timestamp, ties by @@index.
//
// The database connectors (variants.camunda_workflow, sap_accounting and
// sap_o2c) issue one fixed SQL text through the DB-API connection they are
// given (the SAP ones with every "OWNED." replaced by the prefix parameter),
// read the result as pandas.read_sql does, close the connection and return
// pm4py.format_dataframe of that table with the default keys.
//
// variants.github_repo reads the issues of owner/repository page by page
// (GET https://api.github.com/repos/{owner}/{repository}/issues?state=all&
// per_page=100&page=N with {"Authorization": "Bearer " + token} as headers,
// {} without a token) until a page is empty, and fetches each issue's
// timeline_url (issues without that key are skipped). An issue gives a
// "created" row (its created_at, user.login, author_association, title and,
// when the key is present, pull_request.url as case:pull_request); the
// timeline is walked from its last event to its first, and an event holding
// created_at, event and actor gives a row of those, the others are skipped.
// Every key named here is read directly, so a missing one, a null user,
// actor or pull_request, or a created_at that is not a date stops the whole
// extraction: the reference catches it, and the frame holds the rows read
// until then. Dates are parsed by dateutil and the offset is dropped (the
// wall clock is labelled UTC); the native side accepts the ISO 8601 forms
// the API writes (YYYY-MM-DD, optionally followed by T or a space, HH:MM,
// :SS, a fraction truncated to microseconds and Z or an offset), and treats
// anything else as not a date. The frame is pm4py.format_dataframe of the
// rows: cases ordered by their first event (by timestamp, then row), rows
// by case, timestamp and row.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace minera {

// Where the reference raises: type is the Python exception class name.
class ConnectorReferenceError : public std::runtime_error {
 public:
  ConnectorReferenceError(std::string type, const std::string& message)
      : std::runtime_error(type + ": " + message), type_(std::move(type)) {}
  [[nodiscard]] const std::string& type() const noexcept { return type_; }

 private:
  std::string type_;
};

struct ConnectorReferenceFrame {
  std::vector<std::string> columns, dtypes;
  std::vector<std::size_t> index;
  std::vector<std::vector<std::optional<std::string>>> rows;
  bool operator==(const ConnectorReferenceFrame&) const = default;
};

// A DB-API value: None, int, float or str.
using ConnectorReferenceCell = std::variant<std::monostate, std::int64_t, double, std::string>;

struct ConnectorReferenceResultSet {
  std::vector<std::string> names;
  std::vector<std::vector<ConnectorReferenceCell>> rows;
};

// The connection a database connector is given: execute runs one SQL text
// and returns the cursor's result; close is called once afterwards.
struct ConnectorReferenceConnection {
  std::function<ConnectorReferenceResultSet(const std::string& sql)> execute;
  std::function<void()> close;
};

// The SQL text a database connector issues.
[[nodiscard]] std::string reference_database_connector_query(const std::string& connector, const std::string& prefix = {});

[[nodiscard]] ConnectorReferenceFrame reference_camunda_workflow(const ConnectorReferenceConnection& connection);
[[nodiscard]] ConnectorReferenceFrame reference_sap_accounting(const ConnectorReferenceConnection& connection,
                                                               const std::string& prefix = {});
[[nodiscard]] ConnectorReferenceFrame reference_sap_o2c(const ConnectorReferenceConnection& connection,
                                                        const std::string& prefix = {});

// A decoded JSON response: members keep their order.
struct ConnectorReferenceJson {
  enum Kind { kNull, kBool, kInt, kFloat, kText, kArray, kObject } kind = kNull;
  bool boolean = false;
  std::int64_t integer = 0;
  double real = 0.0;
  std::string text;
  std::vector<ConnectorReferenceJson> items;
  std::vector<std::pair<std::string, ConnectorReferenceJson>> members;
  // The member named key, nullptr when absent (or when this is no object).
  [[nodiscard]] const ConnectorReferenceJson* find(const std::string& key) const;
};

// requests.get(url, headers=headers).json()
using ConnectorReferenceGet =
    std::function<ConnectorReferenceJson(const std::string& url, const std::map<std::string, std::string>& headers)>;

[[nodiscard]] ConnectorReferenceFrame reference_github_repo(const std::string& owner, const std::string& repository,
                                                            const std::optional<std::string>& token,
                                                            const ConnectorReferenceGet& get);

[[nodiscard]] ConnectorReferenceFrame reference_chrome_history(const std::filesystem::path& database);
[[nodiscard]] ConnectorReferenceFrame reference_firefox_history(const std::filesystem::path& database);

}  // namespace minera
