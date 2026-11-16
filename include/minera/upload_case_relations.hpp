#pragma once

// The relations of pm4py's timestamp interleavings on uploaded logs (round
// CZ). Kept apart from case_relations.hpp, which most TUs include.

#include "minera/case_relations.hpp"

#include <string>
#include <vector>

namespace minera {

// The columns of an upload's frame as pm4py's interleavings see them: a CSV
// log's read_csv header (`csv_text`) with format_dataframe's standard
// columns, an XES log's read_xes keys (every event attribute key and each
// trace attribute key under "case:").
[[nodiscard]] std::vector<std::string> interleavings_csv_frame_columns(const std::string& csv_text);
[[nodiscard]] std::vector<std::string> interleavings_xes_frame_columns(const EventLog& log);

// One side of the interleavings: its log, whether it came from read_xes,
// and its frame columns (above).
struct InterleavingsUploadSide {
  const EventLog& log;
  bool xes = false;
  std::vector<std::string> columns;
};

// pm4py's interleavings join the relations frame, as pandas.read_csv reads
// the relations file (`relations`, from read_csv_reference_table, the
// selected columns renamed case:concept:name_LEFT/_RIGHT), with two
// str-keyed frames: format_dataframe's "string" case column for a CSV log,
// read_xes's "str" one for an XES log (observed black-box on pandas 3.0.5).
// The left frame merges first (no suffixes), then pm4py drops the LEFT key,
// renames the RIGHT key case:concept:name_RIGHT_REL and merges the right
// frame (suffixes _LEFT/_RIGHT):
//   - a str or object relation column meets a case by the text of its str
//     cells; a NaN cell, a bool or a Python int meets nothing, and so does
//     a bool column;
//   - an int64, uint64 or float64 column (an all-NaN one too) fails its
//     merge with pandas' ValueError, unless exactly one of the two merged
//     frames is empty (pandas skips the check then);
//   - a relation column named as a column of the left frame fails the
//     first merge ("columns overlap but no suffix specified"), a repeated
//     key column fails as "not unique", and suffixed names that repeat a
//     column fail the second merge (MergeError).
// Rows whose keys meet no case are dropped; the others keep their file
// order. A missing or shared relation column is refused.
[[nodiscard]] std::vector<CaseRelation> upload_interleavings_relations(
    const EventTable& relations, const std::string& left_column,
    const std::string& right_column, const InterleavingsUploadSide& left,
    const InterleavingsUploadSide& right);

}  // namespace minera
