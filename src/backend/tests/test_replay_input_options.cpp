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

  const auto evaluation = minera::tools::default_evaluation_input_params(log, {});
  assert(evaluation.replay_fitness_options.activity_attribute == "activity");
  assert(evaluation.precision_options.activity_attribute == "activity");
  assert(evaluation.generalization_options.activity_attribute == "activity");
  assert(evaluation.composite_options.activity_attribute == "activity");
  const auto quality = minera::evaluate_composite(log, net, evaluation.composite_options);
  assert(quality.computed && quality.fitness == 1.0 && quality.precision == 1.0);
  const auto explicit_evaluation = minera::tools::default_evaluation_input_params(log, {}, true);
  assert(explicit_evaluation.composite_options.activity_attribute == "concept:name");
  rejected = false;
  try { (void)minera::evaluate_composite(log, net, explicit_evaluation.composite_options); }
  catch (const std::exception& error) { rejected = std::string(error.what()) == "'concept:name'"; }
  assert(rejected);

  auto selected_log = canonical;
  for (auto& event : selected_log.traces[0].events)
    event.attributes.push_back({"chosen", event.activity});
  const auto selected = minera::tools::default_evaluation_input_params(selected_log, {}, false, "chosen");
  assert(selected.composite_options.activity_attribute == "chosen");
  assert(minera::evaluate_composite(selected_log, net, selected.composite_options).computed);
  minera::EvaluationParams configured;
  configured.composite_options.fitness_weight = 0.75;
  configured.composite_options.max_rows_to_materialize = 10;
  const auto preserved = minera::tools::default_evaluation_input_params(log, configured);
  assert(preserved.composite_options.fitness_weight == 0.75);
  assert(preserved.composite_options.max_rows_to_materialize == 10);
  assert(minera::tools::default_evaluation_input_params(canonical, {}).composite_options.activity_attribute == "concept:name");
  const auto custom_evaluation = minera::tools::default_evaluation_input_params(custom, {});
  const auto custom_input = minera::tools::evaluation_input_log(custom, "task");
  assert(custom_input && custom_input->traces[0].events[0].attribute("task"));
  assert(!custom.traces[0].events[0].attribute("task"));
  assert(minera::evaluate_replay_fitness(*custom_input, net, minera::ReplayFitnessVariant::AUTO,
      custom_evaluation.replay_fitness_options).computed);
  assert(minera::evaluate_composite(*custom_input, net, custom_evaluation.composite_options).computed);
  assert(!minera::tools::evaluation_input_log(log, "activity"));
  assert(!minera::tools::evaluation_input_log(custom, "missing"));
  auto existing_custom = custom;
  existing_custom.traces[0].events[0].attributes.push_back({"task", ""});
  existing_custom.traces[0].events[0].attributes.back().is_null = true;
  const auto existing_input = minera::tools::evaluation_input_log(existing_custom, "task");
  assert(existing_input && existing_input->traces[0].events[0].attributes.back().is_null);
  auto missing_custom = custom;
  for (auto& event : missing_custom.traces[0].events) {
    event.activity.clear();
    event.activity_state = minera::FirstClassAttributeState::kMissing;
  }
  assert(!minera::tools::evaluation_input_log(missing_custom, "task"));
}
