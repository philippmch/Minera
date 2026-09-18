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
