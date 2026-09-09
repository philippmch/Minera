#pragma once

#include <filesystem>
#include <istream>
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
};

// The CSV adapter keeps both projections of one parse.  `log` retains the
// historical case-grouped import used by the rest of the backend, while
// `table` retains the source header order and one rectangular row per parsed
// event for analyses whose semantics are defined on flat data.
struct CsvEventData {
  EventLog log;
  EventTable table;
};

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
