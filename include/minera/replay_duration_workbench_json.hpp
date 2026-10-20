#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "minera/conformance.hpp"
namespace minera {
// Saved diagnostic structures deliberately project only fields consumed by
// duration computation. The complete original JSON remains in raw_json.
struct ReplayDurationWorkbenchInput {
  std::string mode;
  std::string raw_json;
  std::optional<std::string> activity_attribute, timestamp_attribute;
  std::vector<TokenReplayTransitionDurationCohort> transition_cohorts;
  std::vector<TokenReplayUnknownActivityDurationCohort> unknown_activity_cohorts;
  std::vector<TokenReplayTraceDiagnostics> saved_diagnostics;
  bool has_saved_diagnostics = false;
  std::string saved_input_shape = "native_rows";
  std::size_t trace_index = 0;
  std::vector<std::size_t> trace_indices;
};
[[nodiscard]] ReplayDurationWorkbenchInput parse_replay_duration_workbench_json(std::string_view);
[[nodiscard]] std::vector<TokenReplayTraceDiagnostics> parse_saved_replay_duration_diagnostics_json(std::string_view, std::string_view input_shape);
// Executes only the chosen native duration helper, never replay. Exceptions
// preserve the native caller contract; the HTTP route retains them separately.
[[nodiscard]] std::string serialize_replay_duration_workbench_result_json(
 const EventLog&, const PetriNet*, const ReplayDurationWorkbenchInput&, const TokenReplayOptions&);
[[nodiscard]] std::string serialize_replay_duration_diagnostics_json(const TokenReplayDurationDiagnostics&);
}
