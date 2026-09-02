#include "../footprints/conformance_evaluation.hpp"
#include "../footprints/conformance_log_extensive.hpp"
#include "../footprints/conformance_log_model.hpp"
#include "../footprints/conformance_trace_extensive.hpp"
#include "../footprints/conformance_tree_visualization.hpp"
#include "minera/footprints.hpp"

#include <cassert>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using namespace minera;
using namespace minera::footprints_conformance;

namespace {

Footprint footprint(std::initializer_list<const char*> trace) {
  Footprint result;
  result.trace = std::vector<std::string>{};
  for (const auto* activity : trace) result.trace->emplace_back(activity);
  result.activities.insert(result.trace->begin(), result.trace->end());
  if (!result.trace->empty()) {
    result.start_activities.insert(result.trace->front());
    result.end_activities.insert(result.trace->back());
    result.min_trace_length = result.trace->size();
    for (std::size_t index = 1; index < result.trace->size(); ++index) {
      ++result.dfg[{(*result.trace)[index - 1], (*result.trace)[index]}];
    }
  } else {
    result.min_trace_length = 0;
  }
  for (const auto& [pair, count] : result.dfg) {
    (void)count;
    if (result.dfg.contains({pair.second, pair.first})) {
      result.parallel.insert(pair);
    } else {
      result.sequence.insert(pair);
    }
  }
  return result;
}

void evaluation_utility_contract() {
  const auto one = footprint({"A", "B"});
  const auto two = footprint({"B", "C"});
  const auto flattened = EvaluationUtility::flatten({one, two});
  assert(flattened.dfg.at({"A", "B"}) == 1);
  assert(flattened.dfg.at({"B", "C"}) == 1);
  const std::set<std::string> expected_activities{"A", "B", "C"};
  assert(flattened.activities == expected_activities);
  assert(EvaluationUtility::precision(one, one) == 1.0);
  FootprintsExtensiveResult first;
  first.footprints.insert({"A", "B"});
  first.start_activities.insert("A");
  FootprintsExtensiveResult second;
  second.footprints.insert({"B", "C"});
  second.end_activities.insert("C");
  second.min_length_fit = false;
  const auto flattened_conformance =
      EvaluationUtility::flatten_conformance({first, second});
  assert(flattened_conformance.footprints.size() == 2);
  assert(flattened_conformance.start_activities.count("A") == 1);
  assert(flattened_conformance.end_activities.count("C") == 1);
  assert(!flattened_conformance.min_length_fit &&
         !flattened_conformance.is_fit);
  const auto conformance = LogExtensiveVariant::apply(one, one);
  const auto summary =
      EvaluationUtility::summarize({one, two}, one, {conformance, conformance});
  assert(summary.perc_fit_traces == 100.0);
  assert(summary.log_fitness >= 0.0 && summary.log_fitness <= 1.0);
  const auto empty_summary = EvaluationUtility::summarize(
      {}, one, std::vector<FootprintsExtensiveResult>{});
  assert(empty_summary.perc_fit_traces == 0.0 &&
         empty_summary.log_fitness == 1.0);
}

void tree_visualization_projection_contract() {
  Footprint source;
  source.activities = {"A"};
  source.sequence.insert({"A", "B"});
  source.parallel.insert({"B", "C"});
  source.atomic_interleaving.insert({"C", "D"});
  const auto projection = FootprintRelationProjection::project(source);
  assert(projection.relations.size() == 3);
  assert(projection.activities.count("A") == 1);
  assert(projection.activities.count("D") == 1);
  assert(FootprintRelationProjection::relation_activities(
             projection.relations)
             .size() == 4);

  ProcessTreeNode root;
  root.op = ProcessTreeOperator::kSequence;
  root.children = {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)};
  const ProcessTree tree({"A", "B"}, std::move(root));
  FootprintViolations violations;
  violations.insert({"A", "B"});
  const auto decorations = TreeVisualizationProjection::apply(
      tree, violations, {"A"}, {});
  assert(decorations.size() == 1);
  assert(decorations[0].preorder_index == 1);
  assert(decorations[0].operation == ProcessTreeOperator::kActivity);
  assert(decorations[0].activity == "A" && decorations[0].color == "blue");
  FootprintsExtensiveResult extensive;
  extensive.activities_always_happening.insert("B");
  assert(TreeVisualizationProjection::decorate(tree, extensive).empty());
  extensive.end_activities.insert("B");
  const auto boundary = TreeVisualizationProjection::decorate(tree, extensive);
  assert(boundary.size() == 1 && boundary[0].preorder_index == 2);
  assert(boundary[0].activity == "B" && boundary[0].color == "blue");
  const auto clean = TreeVisualizationProjection::decorate(tree, {}, {}, {});
  assert(clean.empty());

  ProcessTreeNode duplicate_root;
  duplicate_root.op = ProcessTreeOperator::kSequence;
  duplicate_root.children = {ProcessTreeNode::leaf(0),
                             ProcessTreeNode::leaf(0)};
  const ProcessTree duplicate_tree({"A"}, std::move(duplicate_root));
  const auto duplicate = TreeVisualizationProjection::apply(
      duplicate_tree, FootprintViolations{}, {"A"});
  assert(duplicate.size() == 1 && duplicate[0].preorder_index == 2);
}

void log_model_variant_contract() {
  const auto log = footprint({"A", "B"});
  auto model = log;
  model.sequence.clear();
  model.parallel.insert({"A", "B"});
  assert(LogModelVariant::apply(log, model).empty());
  assert(LogModelVariant::apply(log, model, {true}).count({"A", "B"}) == 1);
  const auto tracewise = LogModelVariant::apply_tracewise({log}, model);
  assert(tracewise.size() == 1 && tracewise.front().count({"A", "B"}) == 0);
  EventLog source;
  source.traces.push_back(Trace{"case-1", {Event{"case-1", "A"}}});
  source.traces.front().attributes.push_back(
      EventAttribute{"external_case", "selected-case"});
  const auto diagnostics = LogModelVariant::diagnostics(
      source, tracewise, "external_case");
  assert(diagnostics.size() == 1 &&
         diagnostics.front().case_id == "selected-case");
  assert(diagnostics.front().is_fit);
}

void log_extensive_variant_contract() {
  const auto log = footprint({"A", "B"});
  auto model = log;
  model.start_activities = {"B"};
  model.end_activities = {"A"};
  model.min_trace_length = 3;
  const auto result = LogExtensiveVariant::apply(log, model);
  assert(result.start_activities.count("A") == 1);
  assert(result.end_activities.count("B") == 1);
  assert(!result.min_length_fit && !result.is_fit);
  model.activities_always_happening = std::set<std::string>{"A", "C"};
  auto ignored = LogExtensiveVariant::compare(log, model, true);
  assert(ignored.activities_always_happening.count("C") == 1);
  assert(ignored.is_fit == result.is_fit);
  LogExtensiveVariant::finalize(ignored);
  assert(!ignored.is_fit);
}

void trace_extensive_variant_contract() {
  const auto good = footprint({"A", "B"});
  const auto bad = footprint({"B", "A"});
  auto model = good;
  model.activities_always_happening = std::set<std::string>{"A", "B"};
  const auto enabled = TraceExtensiveVariant::apply({good, bad}, model);
  assert(enabled.size() == 2);
  assert(enabled[0].is_fit);
  assert(!enabled[1].is_fit);
  const auto disabled = TraceExtensiveVariant::apply(
      {bad}, model, FootprintsTraceExtensiveOptions{false});
  assert(disabled.size() == 1);
  assert(disabled[0].activities_always_happening.empty());
}

void log_model_selector_contract() {
  const auto log = footprint({"B", "A"});
  const auto model = footprint({"A", "B"});
  const auto result = check_footprints_log_model(log, model);
  assert(result.size() == 1);
  assert(result.count({"B", "A"}) == 1);
}

void log_extensive_selector_contract() {
  const auto log = footprint({"B", "A"});
  const auto model = footprint({"A", "B"});
  const auto result = check_footprints_log_extensive(log, model);
  assert(result.footprints.count({"B", "A"}) == 1);
  assert(!result.is_fit);
}

void trace_extensive_selector_contract() {
  const auto model = footprint({"A", "B"});
  const auto result = check_footprints_trace_extensive(
      {footprint({"A", "B"}), footprint({"B", "A"})}, model);
  assert(result.size() == 2);
  assert(result[0].is_fit && !result[1].is_fit);
}

}  // namespace

int main() {
  evaluation_utility_contract();
  tree_visualization_projection_contract();
  log_model_variant_contract();
  log_extensive_variant_contract();
  trace_extensive_variant_contract();
  log_model_selector_contract();
  log_extensive_selector_contract();
  trace_extensive_selector_contract();
}
