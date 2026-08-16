#pragma once

#include <filesystem>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

#include "minera/ocel.hpp"

namespace minera {

// Options for the extended-table OCEL interchange.  The event table contains
// one row per event and one `ocel:type:<type>` column for each object type;
// object references are represented as a bracketed list in those columns.
struct OcelCsvImportOptions {
  std::string event_id_column = "ocel:eid";
  std::string activity_column = "ocel:activity";
  std::string timestamp_column = "ocel:timestamp";
  std::string object_type_prefix = "ocel:type:";
  std::string object_id_column = "ocel:oid";
  std::string object_type_column = "ocel:type";
  char delimiter = ',';
  bool has_header = true;
  bool trim_fields = true;
  // The tabular format has no representation for qualified relations,
  // object-to-object/event-to-event links, object changes, or globals.  Rejecting those
  // during a combined import prevents accidental loss of information.
  bool reject_unsupported = true;
  // Reference extended tables are chronologically ordered after import.
  bool sort_events_by_timestamp = true;
};

struct OcelCsvExportOptions {
  std::string event_id_column = "ocel:eid";
  std::string activity_column = "ocel:activity";
  std::string timestamp_column = "ocel:timestamp";
  std::string object_type_prefix = "ocel:type:";
  std::string object_id_column = "ocel:oid";
  std::string object_type_column = "ocel:type";
  char delimiter = ',';
  bool emit_objects = true;
  // Set this only when the caller intentionally accepts fields that the
  // extended table cannot carry (qualifiers, O2O/E2E links, changes, globals).
  bool allow_lossy = false;
};

[[nodiscard]] Ocel parse_ocel_csv(
    std::string_view event_table,
    const OcelCsvImportOptions& options = {});

[[nodiscard]] Ocel parse_ocel_csv(
    std::string_view event_table,
    std::string_view object_table,
    const OcelCsvImportOptions& options = {});

[[nodiscard]] Ocel import_ocel_csv(
    std::istream& event_input,
    const OcelCsvImportOptions& options = {});

[[nodiscard]] Ocel import_ocel_csv(
    std::istream& event_input,
    std::istream& object_input,
    const OcelCsvImportOptions& options = {});

[[nodiscard]] Ocel import_ocel_csv(
    const std::filesystem::path& event_path,
    const OcelCsvImportOptions& options = {});

[[nodiscard]] Ocel import_ocel_csv(
    const std::filesystem::path& event_path,
    const std::filesystem::path& object_path,
    const OcelCsvImportOptions& options = {});

void write_ocel_csv(std::ostream& out,
                    const Ocel& log,
                    const OcelCsvExportOptions& options = {});

void write_ocel_objects_csv(std::ostream& out,
                            const Ocel& log,
                            const OcelCsvExportOptions& options = {});

[[nodiscard]] std::string serialize_ocel_csv(
    const Ocel& log,
    const OcelCsvExportOptions& options = {});

[[nodiscard]] std::string serialize_ocel_objects_csv(
    const Ocel& log,
    const OcelCsvExportOptions& options = {});

void export_ocel_csv(const Ocel& log,
                     const std::filesystem::path& event_path,
                     const OcelCsvExportOptions& options = {});

void export_ocel_csv(const Ocel& log,
                     const std::filesystem::path& event_path,
                     const std::filesystem::path& object_path,
                     const OcelCsvExportOptions& options = {});

}  // namespace minera
