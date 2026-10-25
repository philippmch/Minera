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

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
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

[[nodiscard]] ConnectorReferenceFrame reference_chrome_history(const std::filesystem::path& database);
[[nodiscard]] ConnectorReferenceFrame reference_firefox_history(const std::filesystem::path& database);

}  // namespace minera
