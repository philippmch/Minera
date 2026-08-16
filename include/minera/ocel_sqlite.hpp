#pragma once

#include <filesystem>

#include "minera/ocel.hpp"

namespace minera {

// SQLite interchange follows the OCEL 2 relational layout for the core
// event, object, event-object, and object-object tables.  The importer and
// exporter add compact auxiliary tables for typed attributes, event links,
// object changes, and globals that are not represented by the base layout.
struct OcelSqliteOptions {
  // Validate all relation references while importing and before exporting.
  bool validate_references = true;
  // Keep type names readable in the event_<type>/object_<type> table names.
  // Names that are not safe SQLite identifiers are encoded deterministically.
  bool readable_type_table_names = true;
};

[[nodiscard]] Ocel import_ocel_sqlite(
    const std::filesystem::path& path,
    const OcelSqliteOptions& options = {});

void export_ocel_sqlite(
    const Ocel& log, const std::filesystem::path& path,
    const OcelSqliteOptions& options = {});

}  // namespace minera
