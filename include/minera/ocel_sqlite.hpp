#pragma once

#include <filesystem>
#include <optional>

#include "minera/ocel.hpp"

namespace minera {

// SQLite interchange follows the OCEL 2 relational layout for the core event,
// object, event-object, and object-object tables. Type-specific event/object
// tables carry typed columns and object-change rows; compact auxiliary tables
// mirror scalar attributes, event links, object changes, and globals so the
// native representation remains lossless for values outside the relational
// column contract (read back with read_native_tables).
struct OcelSqliteOptions {
  // Refuse relations, object links and changes that name an unknown event or
  // object. Unset: an import drops such rows, as the reference SQLite reader
  // does (and then the events and objects left without relations, observed
  // black-box), and an export refuses them.
  std::optional<bool> validate_references;
  // Keep type names readable in the event_<type>/object_<type> table names.
  // Names that are not safe SQLite identifiers are encoded deterministically.
  bool readable_type_table_names = true;
  // Read the native auxiliary tables back (declared types, attribute values,
  // changes, globals, event links), so that a native file round-trips
  // losslessly. Unset: an import reads every file through its type tables,
  // as the reference reader does, native files included (round EQ).
  bool read_native_tables = false;
};

[[nodiscard]] Ocel import_ocel_sqlite(
    const std::filesystem::path& path,
    const OcelSqliteOptions& options = {});

void export_ocel_sqlite(
    const Ocel& log, const std::filesystem::path& path,
    const OcelSqliteOptions& options = {});

}  // namespace minera
