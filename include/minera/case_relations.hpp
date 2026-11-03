#pragma once

#include <istream>
#include <string>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

struct CaseRelation {
  std::string left_case_id;
  std::string right_case_id;
  friend bool operator==(const CaseRelation&, const CaseRelation&) = default;
};

enum class CaseRelationsVariant {
  kPandas,
  PANDAS = kPandas,
};

struct CaseRelationsOptions {
  CaseRelationsVariant variant = CaseRelationsVariant::kPandas;
  std::string timestamp_key = "time:timestamp";
  // Name of the case identifier column represented by Trace::case_id.  A
  // custom key may instead resolve to a trace-level attribute.
  std::string case_id_key = "case_id";
  // Relation-file columns default to case_id_key plus these suffixes.  The
  // callers resolve those defaults before parsing the relation CSV.
  std::string left_suffix = "_LEFT";
  std::string right_suffix = "_RIGHT";
  // Optional exact relation-table selectors.  Empty values use
  // case_id_key + the corresponding suffix; supplied names are retained in
  // the merged output and are never renamed to the defaults.
  std::string relation_left_column;
  std::string relation_right_column;
};

[[nodiscard]] std::vector<CaseRelation> parse_case_relations_csv(
    std::istream& input, std::string left_column = "case_id_LEFT",
    std::string right_column = "case_id_RIGHT");

// Read a relation CSV without projecting away extra columns.  Every parsed
// cell is a string attribute, including an explicit trailing empty field;
// malformed or short rows are rejected so the returned table is rectangular.
[[nodiscard]] EventTable parse_case_relations_table_csv(std::istream& input);

// A relation file is read like the reference reads a relations frame
// (read_csv types each column), and its keys meet a batch-imported CSV log's
// case-id column, which read_csv typed the same way. These rewrite the
// relation key texts so that the text comparison of the merge follows the
// reference's typed comparison:
//   - A key joining a CSV log's case ids (the incoming side of a merge, both
//     sides of the interleavings) meets a case id of equal value: int64,
//     float64 and bool keys meet by value ("007" or "7.0" meet the case "7",
//     True meets 1), a str key never meets a bool, and an NA key meets
//     nothing. A str column joined with a numeric one is an error, as in
//     pandas.
//   - The base side's case ids and the LEFT keys become one concatenated
//     case column, so both are rendered in its dtype: an int64 log with a
//     float64 LEFT column (for example one with an NA cell) reads "7.0"
//     throughout, bool keys read 1/0 in an int64 column, and in an object
//     column numbers of equal value group as one case under the spelling
//     seen first.
// Columns of Python ints or literal text (read_csv's object columns) keep
// their typed text. An XES side keeps its literal keys; so does a str key
// sharing its spelling with a number in an object column, which the
// reference keeps apart.
//
// For a merge: `base_csv`/`incoming_csv` are the CSV sides whose case-id
// column is `case_key` (null otherwise); the base table's case cells may be
// re-rendered, and an incoming key that meets no case becomes a null cell.
void type_csv_case_relation_keys(EventTable& relations,
                                 const std::string& left_column,
                                 const std::string& right_column,
                                 EventTable* base_csv, EventTable* incoming_csv,
                                 const std::string& case_key);
// For the interleavings, joined on both sides: a relation whose key meets no
// case of a CSV side (null for an XES side) is dropped. The left side joins
// first; when no relation meets a left case, the result is empty and the
// right side is not joined (so it cannot fail), as in the reference.
void type_case_relations_like_csv_logs(std::vector<CaseRelation>& relations,
                                       const EventLog* left_csv,
                                       const EventLog* right_csv);

[[nodiscard]] EventLog merge_case_relations(
    const EventLog& base, const EventLog& incoming,
    const std::vector<CaseRelation>& relations,
    const CaseRelationsOptions& options = {});

// Merge flat event tables using the relation table's typed left/right key
// columns.  The returned table keeps base columns first, appends incoming-only
// columns, and then appends relation columns.  Rows from the base table are
// retained; every incoming row is expanded once for each matching relation.
[[nodiscard]] EventTable merge_case_relations(
    const EventTable& base, const EventTable& incoming,
    const EventTable& relations,
    const CaseRelationsOptions& options = {});

// Convenience adapter for CSV-style relation rows.  The identifiers are
// represented as string cells under the explicit relation selectors when
// supplied, otherwise case_id_key plus the configured suffixes, then follow
// the same typed-table merge path.
[[nodiscard]] EventTable merge_case_relations(
    const EventTable& base, const EventTable& incoming,
    const std::vector<CaseRelation>& relations,
    const CaseRelationsOptions& options = {});

}  // namespace minera
