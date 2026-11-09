#pragma once

#include <filesystem>
#include <functional>
#include <istream>
#include <optional>
#include <vector>
#include <string>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct CsvImportOptions {
  std::string case_id_column = "case_id";
  std::string activity_column = "activity";
  std::string timestamp_column = "timestamp";
  std::string resource_column = "resource";  // optional; the org perspective
  std::string lifecycle_column = "lifecycle";  // optional; start/complete
  bool require_timestamp_column = false;
  bool has_header = true;
  bool trim_fields = true;
  char delimiter = ',';
  // Opt-in fallback between case_id and case:concept:name. An exact column
  // match always wins; custom case selectors remain exact.
  bool allow_case_id_aliases = false;
  // The batch import (read_csv + format_dataframe) types the case-id and
  // activity columns (reference_csv_column_text), drops a row whose case id
  // or activity is then missing, or whose timestamp is NA once the timestamp
  // column holds dates, orders cases by their case-id text and skips lines
  // of only spaces/tabs. The reference CSV
  // event stream reader (csv.DictReader) keeps every row and passes those
  // cells on as literal text, so stream adapters set this to keep them.
  bool keep_na_first_class_values = false;
  // Polled while the import parses rows and groups them into cases (every
  // few thousand rows); once it answers true the import throws
  // OperationCancelled. Unset: never cancels, and the result is unchanged.
  std::function<bool()> cancelled;
};

// The CSV adapter keeps both projections of one parse.  `log` retains the
// historical case-grouped import used by the rest of the backend, while
// `table` retains the source header order and one rectangular row per parsed
// event for analyses whose semantics are defined on flat data.
struct CsvEventData {
  EventLog log;
  EventTable table;
};

// The text each cell of one CSV column has once read_csv has typed the whole
// column and it is converted to strings (as format_dataframe does for the
// case-id and activity columns): "007" and "+7" read "7", an NA cell in an
// integer column turns the others into floats ("1.0"), true/false spellings
// read "True"/"False", other text is kept. nullopt marks a missing cell.
// Cells are compared as given (the importer passes trimmed fields).
[[nodiscard]] std::vector<std::optional<std::string>> reference_csv_column_text(
    const std::vector<std::string>& cells);

// The dtype read_csv gives such a column: int64, float64 (also an integer
// column with an NA cell, and an all-NA column), bool, str, or object (Python
// ints beyond 64 bits, or a column kept as literal text). Joins on typed keys
// follow this kind: int64 and float64 keys meet by value, bool keys meet only
// bool keys, and a str key cannot be joined with a numeric one.
enum class ReferenceCsvColumnKind { kInteger, kFloat, kBoolean, kText, kObject };
struct ReferenceCsvColumn {
  ReferenceCsvColumnKind kind = ReferenceCsvColumnKind::kText;
  std::vector<std::optional<std::string>> text;  // as reference_csv_column_text
};
[[nodiscard]] ReferenceCsvColumn reference_csv_column(
    const std::vector<std::string>& cells);

// The column names pandas.read_csv gives a header row's fields (observed
// black-box on pandas 3.0.5): an empty field is "Unnamed: i", i being its
// position, and a repeated name becomes "name.1", "name.2", ..., skipping
// names the header already holds. Named columns keep their names first, so a
// generated "Unnamed: 0" that a later field also spells is the one renamed
// (",Unnamed: 0" reads "Unnamed: 0.1", "Unnamed: 0"); "a,a,a.1" reads "a",
// "a.2", "a.1". Names are compared byte for byte (" a" and "a" differ).
[[nodiscard]] std::vector<std::string> reference_csv_header_names(
    std::vector<std::string> fields);

// Python's repr of a float ("7.0", "0.1", "inf"), the text a float64 key
// converted to str has.
[[nodiscard]] std::string reference_csv_float_text(double value);

// One text cell as pandas' to_datetime(format="mixed") reads it (the default
// conversion of format_dataframe): the ISO spelling of its date, with the
// fraction cut to microseconds where pandas reads the shape with dateutil, or
// nullopt when the cell is not a date to that parser. A cell with
// `nanosecond_fraction` (an ISO cell with more than six fraction digits)
// puts the column at nanosecond resolution, where a cell that is
// `outside_nanosecond_range` (before 1677-09-21 or after 2262-04-11, wall
// clock or UTC) makes the whole conversion fail.
struct ReferenceMixedTimestamp {
  std::string text;
  bool nanosecond_fraction = false;
  bool outside_nanosecond_range = false;
};
[[nodiscard]] std::optional<ReferenceMixedTimestamp> reference_mixed_timestamp(
    const std::string& cell);

[[nodiscard]] CsvEventData import_csv_event_data(
    std::istream& input,
    const CsvImportOptions& options = {});

[[nodiscard]] CsvEventData import_csv_event_data(
    const std::filesystem::path& path,
    const CsvImportOptions& options = {});

// Reconcile a filtered case-grouped log with its original flat CSV rows.
// Events retain their parser-assigned input_order, so this preserves source
// row order and all original cells while applying filter-side mutations. The
// filtered log must derive from the CsvEventData::log paired with `source` and
// must not replace parser-assigned input_order values.
[[nodiscard]] EventTable project_csv_event_table(
    const EventTable& source,
    const EventLog& filtered);

[[nodiscard]] EventLog import_csv_event_log(
    std::istream& input,
    const CsvImportOptions& options = {});

[[nodiscard]] EventLog import_csv_event_log(
    const std::filesystem::path& path,
    const CsvImportOptions& options = {});

}  // namespace minera
