#include "inductive/semantics.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace minera;
using namespace minera::inductive_detail;

namespace {

InductiveDfg dfg_with_chain() {
  InductiveDfg dfg;
  dfg.to_global = {0, 1};
  dfg.to_local = {{0, 0}, {1, 1}};
  dfg.follows = {{0, 1}, {0, 0}};
  dfg.succ = {{1}, {}};
  dfg.is_start = {1, 0};
  dfg.is_end = {0, 1};
  dfg.edge_count = {{0, 1}, {0, 0}};
  dfg.start_count = {1, 0};
  dfg.end_count = {0, 1};
  return dfg;
}

InductiveDfg dfg_with_exclusive_choices() {
  InductiveDfg dfg;
  dfg.to_global = {0, 1};
  dfg.to_local = {{0, 0}, {1, 1}};
  dfg.follows = {{0, 0}, {0, 0}};
  dfg.succ = {{}, {}};
  dfg.is_start = {1, 1};
  dfg.is_end = {1, 1};
  dfg.edge_count = {{0, 0}, {0, 0}};
  dfg.start_count = {1, 1};
  dfg.end_count = {1, 1};
  return dfg;
}

InductiveDfg dfg_with_concurrent_branches() {
  InductiveDfg dfg;
  dfg.to_global = {0, 1};
  dfg.to_local = {{0, 0}, {1, 1}};
  dfg.follows = {{0, 1}, {1, 0}};
  dfg.succ = {{1}, {0}};
  dfg.is_start = {1, 1};
  dfg.is_end = {1, 1};
  dfg.edge_count = {{0, 1}, {1, 0}};
  dfg.start_count = {1, 1};
  dfg.end_count = {1, 1};
  return dfg;
}

InductiveDfg dfg_with_loop_redo() {
  InductiveDfg dfg;
  dfg.to_global = {0, 1};
  dfg.to_local = {{0, 0}, {1, 1}};
  dfg.follows = {{0, 1}, {1, 0}};
  dfg.succ = {{1}, {0}};
  dfg.is_start = {1, 0};
  dfg.is_end = {1, 0};
  dfg.edge_count = {{0, 1}, {1, 0}};
  dfg.start_count = {1, 0};
  dfg.end_count = {1, 0};
  return dfg;
}

InductiveDfg dfg_with_single_activity() {
  InductiveDfg dfg;
  dfg.to_global = {0};
  dfg.to_local = {{0, 0}};
  dfg.follows = {{1}};
  dfg.succ = {{0}};
  dfg.is_start = {1};
  dfg.is_end = {1};
  dfg.edge_count = {{1}};
  dfg.start_count = {1};
  dfg.end_count = {1};
  return dfg;
}

InductiveDfg dfg_for_candidate_split() {
  InductiveDfg dfg;
  dfg.to_global = {0, 1, 2};
  dfg.to_local = {{0, 0}, {1, 1}, {2, 2}};
  dfg.follows = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  dfg.succ = {{}, {}, {}};
  dfg.is_start = {0, 0, 0};
  dfg.is_end = {0, 0, 0};
  dfg.edge_count = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  dfg.start_count = {0, 0, 0};
  dfg.end_count = {0, 0, 0};
  return dfg;
}

ActivityLabels labels() { return {"a", "b", "c"}; }

WeightedSublog chain_log() {
  return {WeightedTrace{{0, 1}, 2}};
}

InductiveDataStructure chain_data() {
  InductiveDataStructure data;
  data.variants = chain_log();
  return data;
}

void im_selector_contract() {
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IM;
  const auto data = chain_data();
  const auto direct = InductiveMinerLogVariant::discover(data, options,
                                                          labels());
  const auto public_result = discover_process_tree(
      InductiveMinerVariantLog{{{"a", "b"}, 2}}, options);
  assert(direct.root().op == ProcessTreeOperator::kSequence);
  assert(public_result.root().op == ProcessTreeOperator::kSequence);
}

void imf_selector_contract() {
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IMF;
  options.noise_threshold = 0.2;
  const auto data = chain_data();
  const auto direct = InductiveMinerInfrequentVariant::discover(
      data, options, labels());
  const auto public_result = discover_process_tree(
      InductiveMinerVariantLog{{{"a", "b"}, 2}}, options);
  assert(direct.root().op == ProcessTreeOperator::kSequence);
  assert(public_result.root().op == ProcessTreeOperator::kSequence);
  assert(InductiveMinerInfrequentVariant::threshold(options) == 0.2);
}

void imd_selector_contract() {
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IMD;
  const auto data = chain_data();
  const auto direct = InductiveMinerDfgVariant::discover(data, options,
                                                          labels());
  const auto public_result = discover_process_tree(
      InductiveMinerVariantLog{{{"a", "b"}, 2}}, options);
  assert(direct.root().op == ProcessTreeOperator::kSequence);
  assert(public_result.root().op == ProcessTreeOperator::kSequence);
}

void base_case_abc_contract() {
  const auto result = BaseCase::inspect(
      WeightedSublog{WeightedTrace{{}, 3}, WeightedTrace{{0}, 2}});
  assert(result.empty_weight == 3);
  assert(result.nonempty_weight == 2);
  assert(result.has_empty());
  assert(!result.all_empty());
}

void base_case_empty_log_contract() {
  const WeightedSublog empty_log = {WeightedTrace{{}, 2}};
  const WeightedSublog populated = {WeightedTrace{{0}, 1}};
  assert(EmptyLogBaseCase::detects(empty_log));
  assert(!EmptyLogBaseCase::detects(populated));
}

void base_case_factory_contract() {
  assert(BaseCaseFactory::empty().op == ProcessTreeOperator::kTau);
  const auto repeated = BaseCaseFactory::single(0, true);
  assert(repeated.op == ProcessTreeOperator::kLoop);
  assert(repeated.children.size() == 2);
}

void base_case_single_activity_contract() {
  const auto result = SingleActivityBaseCase::resolve(
      WeightedSublog{WeightedTrace{{0, 0}, 4}}, dfg_with_single_activity());
  assert(result.has_value());
  assert(result->op == ProcessTreeOperator::kLoop);
}

void disabled_fallthrough_single_activity_contract() {
  InductiveMinerOptions options;
  options.disable_fallthroughs = true;
  const auto result = discover_process_tree(
      InductiveMinerVariantLog{{{"a", "a"}, 1}}, options);
  assert(result.root().op == ProcessTreeOperator::kLoop);
  assert(result.root().children.size() == 2);
  assert(result.root().children[0].op == ProcessTreeOperator::kTau);
  assert(result.root().children[1].op == ProcessTreeOperator::kActivity);
}

void event_log_optional_metadata_contract() {
  EventLog log;
  Trace trace;
  trace.case_id = "already-grouped";
  Event first;
  first.activity = "b";
  trace.events.push_back(std::move(first));
  Event second;
  second.activity = "a";
  trace.events.push_back(std::move(second));
  log.traces.push_back(std::move(trace));

  InductiveMinerOptions options;
  options.case_id_key = "missing-case-field";
  options.timestamp_key = "missing-time-field";
  options.sort_events_by_timestamp = true;
  const auto prepared = prepare_inductive_event_log(log, options);
  assert(prepared.traces.size() == 1);
  assert(prepared.traces.front().events.size() == 2);
  assert(prepared.traces.front().events[0].activity == "b");
  assert(prepared.traces.front().events[1].activity == "a");
}

void cuts_abc_contract() {
  Cut cut;
  cut.kind = CutKind::kSequence;
  cut.groups = {{0}, {1}};
  assert(cut.valid());
  assert(cut.operation() == ProcessTreeOperator::kSequence);
}

void cuts_concurrency_contract() {
  const auto cut = ConcurrencyCut::detect(dfg_with_concurrent_branches());
  assert(cut.has_value());
  assert(cut->kind == CutKind::kParallel);
  assert(cut->groups.size() == 2);
}

void cuts_factory_contract() {
  InductiveMinerOptions options;
  const auto cut = CutFactory::find(dfg_with_chain(), options);
  assert(cut.has_value());
  assert(cut->operation() == ProcessTreeOperator::kSequence);
}

void cuts_loop_contract() {
  const auto cut = LoopCut::detect(dfg_with_loop_redo());
  assert(cut.has_value());
  assert(cut->kind == CutKind::kLoop);
  assert(cut->groups.size() == 2);
}

void cuts_sequence_contract() {
  const auto cut = SequenceCut::detect(dfg_with_chain());
  assert(cut.has_value());
  assert((cut->groups == std::vector<std::vector<ActivityId>>{{0}, {1}}));
}

void cuts_utils_contract() {
  const auto closure = CutUtils::reachability(dfg_with_chain());
  assert(closure.size() == 2);
  assert(closure[0][1]);
  assert(!closure[1][0]);
}

void cuts_xor_contract() {
  const auto cut = ExclusiveChoiceCut::detect(dfg_with_exclusive_choices());
  assert(cut.has_value());
  assert(cut->kind == CutKind::kExclusive);
  assert(cut->groups.size() == 2);
}

void dtypes_im_dfg_contract() {
  const InductiveDfg dfg = dfg_with_chain();
  assert(dfg.size() == 2);
  assert(dfg.has_edge(0, 1));
  assert(!dfg.has_edge(1, 0));
}

void dtypes_im_ds_contract() {
  const InductiveDataStructure empty;
  const auto data = chain_data();
  assert(empty.empty());
  assert(!data.empty());
  assert(data.variants.front().count == 2);
}

void fall_through_abc_contract() {
  FallThrough result{ProcessTreeNode::leaf(0)};
  FallThrough none;
  assert(result.handled());
  assert(!none.handled());
}

void fall_through_activity_concurrent_contract() {
  const WeightedSublog log = {
      WeightedTrace{{0, 0, 1}, 1}, WeightedTrace{{0, 2, 2}, 1}};
  InductiveMinerOptions options;
  const auto result = ActivityConcurrentFallThrough::apply(
      log, dfg_for_candidate_split(), 0.0, options, labels());
  assert(result.has_value());
  assert(result->op == ProcessTreeOperator::kParallel);
}

void fall_through_activity_once_contract() {
  const WeightedSublog log = {
      WeightedTrace{{0, 1}, 1}, WeightedTrace{{0, 2}, 1}};
  InductiveMinerOptions options;
  const auto result = ActivityOncePerTraceFallThrough::apply(
      log, dfg_for_candidate_split(), 0.0, options, labels());
  assert(result.has_value());
  assert(result->op == ProcessTreeOperator::kParallel);
}

void fall_through_empty_trace_contract() {
  const auto result = EmptyTraceFallThrough::split(
      WeightedSublog{WeightedTrace{{}, 2}, WeightedTrace{{0}, 3}});
  assert(result.has_empty());
  assert(result.empty_weight == 2);
  assert(result.nonempty_weight == 3);
  assert(result.nonempty.size() == 1);
}

void fall_through_factory_contract() {
  InductiveMinerOptions options;
  const auto result = FallThroughFactory::apply(
      WeightedSublog{WeightedTrace{{0, 1}, 1}, WeightedTrace{{0, 2}, 1}},
      dfg_for_candidate_split(), 0.0, options, labels());
  assert(result.handled());
  assert(result.node->op == ProcessTreeOperator::kParallel);
}

void fall_through_flower_contract() {
  const auto result = FlowerFallThrough::build({0, 1, 2}, labels());
  assert(result.op == ProcessTreeOperator::kLoop);
  assert(result.children.size() == 2);
  assert(result.children[1].op == ProcessTreeOperator::kExclusive);
}

void fall_through_strict_tau_loop_contract() {
  auto dfg = dfg_with_loop_redo();
  dfg.is_end = {0, 1};
  dfg.end_count = {0, 1};
  const auto result = StrictTauLoopFallThrough::project(
      WeightedSublog{WeightedTrace{{0, 1, 0}, 1}}, dfg);
  assert(result.size() == 2);
  assert(result.front().activities == std::vector<ActivityId>({0, 1}));
}

void fall_through_tau_loop_contract() {
  const auto result = TauLoopFallThrough::project(
      WeightedSublog{WeightedTrace{{0, 1, 0}, 1}}, dfg_with_loop_redo());
  assert(result.size() == 2);
  assert(!result.front().empty());
}

void variants_abc_contract() {
  InductiveMinerOptions options;
  const auto result = InductiveMinerFramework::discover(
      chain_data(), options, labels());
  assert(result.root().op == ProcessTreeOperator::kSequence);
}

void variants_im_contract() {
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IM;
  const auto result = InductiveMinerLogVariant::discover(
      chain_data(), options, labels());
  assert(result.root().op == ProcessTreeOperator::kSequence);
}

void variants_imd_contract() {
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IMD;
  const auto result = InductiveMinerDfgVariant::discover(
      chain_data(), options, labels());
  assert(result.root().op == ProcessTreeOperator::kSequence);
}

void variants_imf_contract() {
  InductiveMinerOptions options;
  options.variant = InductiveMinerVariant::IMF;
  options.noise_threshold = 0.5;
  const auto result = InductiveMinerInfrequentVariant::discover(
      chain_data(), options, labels());
  assert(result.root().op == ProcessTreeOperator::kSequence);
}

void variants_instances_contract() {
  const auto variants = InductiveMinerInstances::all();
  assert(variants.size() == 3);
  assert(InductiveMinerInstances::supports(InductiveMinerVariant::IM));
  assert(InductiveMinerInstances::supports(InductiveMinerVariant::IMF));
  assert(InductiveMinerInstances::supports(InductiveMinerVariant::IMD));
}

}  // namespace

int main() {
  im_selector_contract();
  imf_selector_contract();
  imd_selector_contract();
  base_case_abc_contract();
  base_case_empty_log_contract();
  base_case_factory_contract();
  base_case_single_activity_contract();
  disabled_fallthrough_single_activity_contract();
  event_log_optional_metadata_contract();
  cuts_abc_contract();
  cuts_concurrency_contract();
  cuts_factory_contract();
  cuts_loop_contract();
  cuts_sequence_contract();
  cuts_utils_contract();
  cuts_xor_contract();
  dtypes_im_dfg_contract();
  dtypes_im_ds_contract();
  fall_through_abc_contract();
  fall_through_activity_concurrent_contract();
  fall_through_activity_once_contract();
  fall_through_empty_trace_contract();
  fall_through_factory_contract();
  fall_through_flower_contract();
  fall_through_strict_tau_loop_contract();
  fall_through_tau_loop_contract();
  variants_abc_contract();
  variants_im_contract();
  variants_imd_contract();
  variants_imf_contract();
  variants_instances_contract();
  return 0;
}
