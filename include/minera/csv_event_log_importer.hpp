#pragma once

#include <filesystem>
#include <istream>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

struct CsvImportOptions {
  std::string case_id_column = "case_id";
  std::string activity_column = "activity";
  std::string timestamp_column = "timestamp";
  bool require_timestamp_column = false;
  bool has_header = true;
  bool trim_fields = true;
  char delimiter = ',';
};

[[nodiscard]] EventLog import_csv_event_log(
    std::istream& input,
    const CsvImportOptions& options = {});

[[nodiscard]] EventLog import_csv_event_log(
    const std::filesystem::path& path,
    const CsvImportOptions& options = {});

}  // namespace minera
