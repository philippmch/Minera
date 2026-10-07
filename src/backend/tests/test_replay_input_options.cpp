#include "replay_input_options.hpp"

#include <cassert>
#include <sstream>
#include "minera/alpha_miner.hpp"
#include "minera/csv_event_log_importer.hpp"

int main() {
  std::istringstream csv("case_id,activity,timestamp\nc1,A,2026-10-01T09:00:00Z\nc1,B,2026-10-01T10:00:00Z\n");
  const auto log = minera::import_csv_event_log(csv);
  const auto net = minera::discover_petri_net(log);
  const auto automatic = minera::tools::default_replay_input_options(log, {});
  assert(automatic.activity_attribute == "activity");
  const auto result = minera::check_conformance(log, net, automatic);
  assert(result.activity_attribute == "activity");
  assert(result.trace_count == 1 && result.fitting_traces == 1);
  assert(log.traces[0].events[0].activity_source_key == "activity");

  const auto explicit_default = minera::tools::default_replay_input_options(log, {}, true);
  assert(explicit_default.activity_attribute == "concept:name");
  bool rejected = false;
  try { (void)minera::check_conformance(log, net, explicit_default); }
  catch (const std::exception& error) { rejected = std::string(error.what()) == "'concept:name'"; }
  assert(rejected);

  auto named = automatic;
  named.activity_attribute = "other";
  named.variant = minera::TokenReplayVariant::kBackwards;
  named.stop_immediately_unfit = true;
  const auto unchanged = minera::tools::default_replay_input_options(log, named);
  assert(unchanged.activity_attribute == "other");
  assert(unchanged.variant == named.variant && unchanged.stop_immediately_unfit);

  auto collision = log;
  collision.traces[0].events[0].attributes.push_back({"concept:name", "different"});
  assert(minera::tools::default_replay_input_options(collision, {}).activity_attribute == "concept:name");
  collision.traces[0].events[0].attributes.back().is_null = true;
  assert(minera::tools::default_replay_input_options(collision, {}).activity_attribute == "concept:name");

  std::istringstream custom_csv("case_id,task,timestamp\nc1,A,2026-10-01T09:00:00Z\nc1,B,2026-10-01T10:00:00Z\n");
  minera::CsvImportOptions import_options;
  import_options.activity_column = "task";
  const auto custom = minera::import_csv_event_log(custom_csv, import_options);
  const auto custom_replay = minera::tools::default_replay_input_options(custom, {});
  assert(custom_replay.activity_attribute == "task");
  assert(minera::check_conformance(custom, net, custom_replay).fitting_traces == 1);

  auto mixed = log;
  mixed.traces[0].events[1].activity_source_key = "other";
  assert(minera::tools::default_replay_input_options(mixed, {}).activity_attribute == "concept:name");
  auto canonical = log;
  for (auto& event : canonical.traces[0].events) event.activity_source_key = "concept:name";
  assert(minera::tools::default_replay_input_options(canonical, {}).activity_attribute == "concept:name");
  assert(minera::tools::default_replay_input_options(minera::EventLog{}, {}).activity_attribute == "concept:name");
}
