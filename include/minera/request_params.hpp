#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/log_filter.hpp"
#include "minera/transition_system.hpp"

// Pure request-parsing helpers shared by the local server (tools/minera_server)
// and its tests. Nothing here depends on the HTTP layer: form fields arrive
// through a plain string-keyed lookup, so the parsing can be exercised directly.
namespace minera {

// Case-insensitive suffix match (the suffix is assumed already lowercase).
[[nodiscard]] bool ends_with_ci(std::string_view value, std::string_view suffix);

// Decides whether an upload is XES rather than CSV: a ".xes" name, or content
// that begins (after an optional UTF-8 BOM and whitespace) with "<?xml" or
// "<log" so extension-less uploads still route to the XES importer.
[[nodiscard]] bool looks_like_xes(std::string_view filename,
                                  std::string_view content);

// Splits a comma-separated field into trimmed, non-empty parts. Spaces
// surrounding each part are dropped (so "a , b" yields "a","b"); empty parts
// (e.g. from "a,,b" or a trailing comma) are skipped entirely.
[[nodiscard]] std::vector<std::string> split_csv(std::string_view value);

// Serializes a message as a one-key JSON error object, escaping quotes and
// backslashes so the message can never break the JSON.
[[nodiscard]] std::string error_json(std::string_view message);

// The discovery options parsed from a mine request's form fields.
struct MineParams {
  CsvImportOptions csv;
  LogFilterOptions filter;
  InductiveMinerOptions inductive;
  AlphaMinerOptions alpha;
  HeuristicMinerOptions heuristic;
  TransitionSystemOptions transition_system;
};

// A lookup returning a form field's value, or "" when the field is absent.
using FieldLookup = std::function<std::string(std::string_view)>;

// Builds the discovery options from a mine request's form fields. Absent or
// empty fields leave each option at its default, so a bare upload mines with the
// stock settings. A `time_mode` value that is not one of the three known modes
// throws std::invalid_argument, so the caller can report it instead of silently
// mining with the wrong filter.
[[nodiscard]] MineParams parse_mine_params(const FieldLookup& field);

}  // namespace minera
