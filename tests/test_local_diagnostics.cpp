#include "minera/local_diagnostics.hpp"
#include "minera/request_params.hpp"

#include <cassert>
#include <string>
#include <stdexcept>

using namespace minera;

int main() {
  EventLog log;
  Trace t; t.case_id = "c";
  t.events.push_back(Event{"c", "A", "", "r1", 0, {{"org:group", "g1", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "A", "", "r2", 1, {{"org:group", "g1", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "B", "", "r1", 2, {{"org:group", "g1", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "A", "", "r3", 3, {{"org:group", "g2", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "", "", "r4", 4, {{"org:group", "g2", AttributeKind::kString}}, {}});
  log.traces.push_back(std::move(t));
  const auto result = discover_local_diagnostics(log);
  assert(result.diagnostics.computed && result.diagnostics.skipped_events == 1);
  assert(result.group_member_contribution.at("g1").at("r1").at("A") == 1);
  assert(result.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);
  assert(result.group_relative_stake.at("g1").at("A") == 2.0 / 3.0);
  assert(result.group_coverage.at("g1").at("r1") == 2.0 / 3.0);

  EventLog denominator_log = log;
  denominator_log.traces[0].events.push_back(
      Event{"c", "A", "", "r5", 5, {}, {}});
  const auto denominator_result = discover_local_diagnostics(denominator_log);
  assert(denominator_result.group_relative_focus.at("g1").at("A") == 0.5);

  LocalDiagnosticsOptions explicit_mode;
  explicit_mode.explicit_groups = {{"g1", {"r1", "r2"}}};
  const auto explicit_result = discover_local_diagnostics(log, explicit_mode);
  assert(explicit_result.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);

  LocalDiagnosticsOptions capped;
  capped.max_rows = 1;
  const auto capped_result = discover_local_diagnostics(log, capped);
  assert(capped_result.diagnostics.output_truncated && capped_result.diagnostics.row_count == 1);
  assert(capped_result.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);

  const auto json = local_diagnostics_json(result);
  assert(json.find("LOCAL_DIAGNOSTICS") != std::string::npos);

  const auto parsed = parse_local_diagnostics_params([](std::string_view n) {
    if (n == "local_diagnostics_groups") return std::string("g1:r1,r2;g2:r3");
    return std::string{};
  });
  assert(parsed.explicit_groups.size() == 2 && parsed.explicit_groups.at("g1").size() == 2);
  for (const auto& invalid : {"g1:r1,,r2", "g1:r1,", "g1:r1;"}) {
    try {
      (void)parse_local_diagnostics_params([&](std::string_view n) {
        return n == "local_diagnostics_groups" ? std::string(invalid) : std::string{};
      });
      assert(false);
    } catch (const std::invalid_argument&) {
    }
  }
  return 0;
}
