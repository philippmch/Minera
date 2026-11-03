#pragma once

// Include after <sqlite3.h> (or <winsqlite/winsqlite3.h>), inside the
// MINERA_HAS_SQLITE guard.
//
// SQLite files Minera reads come from outside: uploads, or paths the user
// picks. A crafted file can carry views and triggers that call
// side-effecting SQL functions when Minera queries it, or can corrupt
// memory through writable_schema and direct writes to shadow tables.
// Defensive mode and an untrusted schema close both, and ordinary reads are
// unaffected. Each option is applied only when the SQLite headers define it
// (DEFENSIVE needs 3.26 or newer, TRUSTED_SCHEMA needs 3.31 or newer).

namespace minera::detail {

inline void harden_untrusted_sqlite(sqlite3* db) noexcept {
  if (db == nullptr) return;
#ifdef SQLITE_DBCONFIG_DEFENSIVE
  sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr);
#endif
#ifdef SQLITE_DBCONFIG_TRUSTED_SCHEMA
  sqlite3_db_config(db, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr);
#endif
}

}  // namespace minera::detail
