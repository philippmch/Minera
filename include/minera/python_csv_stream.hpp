#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// One row of the reference CSV event stream: csv.DictReader over a file
// opened in text mode. Keys keep header order (a repeated name keeps its first
// position and its last value); a short row gives nullopt for its missing
// trailing columns, and a long row's surplus cells land in `extras`.
struct PythonCsvRecord {
  std::vector<std::pair<std::string, std::optional<std::string>>> fields;
  std::vector<std::string> extras;
  [[nodiscard]] const std::optional<std::string>* find(std::string_view key) const;
};

// Parses like the reference streaming CSV importer: universal newlines (CRLF
// and lone CR read as LF, also inside quotes), the excel dialect (",", '"',
// doubled quotes, no trimming, quotes inside an unquoted field are literal,
// text after a closing quote is appended), blank lines skipped, the first
// row is the header, and an unterminated quote ends its field at end of file.
[[nodiscard]] std::vector<PythonCsvRecord> read_python_csv_event_stream(std::istream& input);
[[nodiscard]] std::vector<PythonCsvRecord> read_python_csv_event_stream(
    const std::filesystem::path& path);

// A stream event whose attributes are the record's string cells (a missing
// trailing cell is a present null). First-class fields stay missing, so
// selectors read the literal column.
[[nodiscard]] Event python_csv_record_event(const PythonCsvRecord& record,
                                            std::size_t input_order = 0);

// The case key of a record for streaming state: nullopt when the column is
// absent (the reference skips such events); a null cell reads "None", the
// same case as the text "None".
[[nodiscard]] std::optional<std::string> python_csv_case_key(
    const PythonCsvRecord& record, std::string_view column);

}  // namespace minera
