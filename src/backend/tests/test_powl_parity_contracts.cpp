#include "powl/semantics.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using namespace minera;
using namespace minera::powl_detail;

namespace {

VariantLog variant_log(
    std::initializer_list<std::initializer_list<const char*>> sequences) {
  VariantLog result;
  for (const auto sequence : sequences) {
    Sequence value;
    for (const auto* activity : sequence) value.emplace_back(activity);
    const auto [it, inserted] =
        result.index.emplace(value, result.entries.size());
    if (inserted) {
      result.entries.emplace_back(std::move(value), 1);
    } else {
      ++result.entries[it->second].second;
    }
  }
  return result;
}

void add_variant(VariantLog& log, std::initializer_list<const char*> sequence,
                 std::uint64_t count) {
  Sequence value;
  for (const auto* activity : sequence) value.emplace_back(activity);
  const auto [it, inserted] = log.index.emplace(value, log.entries.size());
  if (inserted) {
    log.entries.emplace_back(std::move(value), count);
  } else {
    log.entries[it->second].second += count;
  }
}

State state_for(PowlDiscoveryVariant variant) {
  PowlDiscoveryOptions options;
  options.variant = variant;
  return State{options, 0, false};
}

void base_case_abc_contract() {
  const auto result = BaseCase::inspect(variant_log({{}, {"A"}}));
  assert(result.empty_weight == 1);
  assert(result.nonempty_weight == 1);
  assert(result.has_empty());
  assert(!result.all_empty());
}

void base_case_empty_log_contract() {
  assert(EmptyLogBaseCase::detects(variant_log({{}})));
  assert(!EmptyLogBaseCase::detects(variant_log({{"A"}})));
}

void base_case_factory_contract() {
  assert(BaseCaseFactory::empty().kind() == PowlNodeKind::kSilent);
  assert(BaseCaseFactory::single("A", false).kind() ==
         PowlNodeKind::kTransition);
  assert(BaseCaseFactory::single("A", true).kind() == PowlNodeKind::kLoop);
}

void base_case_single_activity_contract() {
  const auto result = SingleActivityBaseCase::resolve(variant_log({{"A"}}));
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kTransition);
  assert(result->label() == "A");
  const auto repeated =
      SingleActivityBaseCase::resolve(variant_log({{"A", "A"}}));
  assert(!repeated.has_value());
}

void cuts_concurrency_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = ConcurrencyCut::detect(
      variant_log({{"A", "B"}, {"B", "A"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->children().size() == 2);
  assert(result->order().empty());
}

void cuts_factory_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = CutFactory::find(variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->has_edge(0, 1));
}

void cuts_loop_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result =
      LoopCut::detect(variant_log({{"A", "B", "A"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kLoop);
  assert(result->children()[0].label() == "A");
  assert(result->children()[1].label() == "B");
}

void cuts_sequence_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = SequenceCut::detect(variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->has_edge(0, 1));
}

void cuts_xor_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = ExclusiveChoiceCut::detect(
      variant_log({{"A"}, {"B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kXor);
  assert(result->children().size() == 2);
}

void fall_through_activity_concurrent_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = ActivityConcurrentFallThrough::apply(
      variant_log({{"B", "A", "A"}, {"A", "A", "B"},
                   {"A", "A", "C"}}),
      state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->children().size() == 2);
}

void fall_through_activity_once_per_trace_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = ActivityOncePerTraceFallThrough::apply(
      variant_log({{"A", "B"}, {"B", "A"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->children().size() == 2);
  assert(result->order().empty());
}

void fall_through_empty_traces_contract() {
  const auto result = EmptyTraceFallThrough::split(
      variant_log({{}, {"A"}, {"A"}}));
  assert(result.has_empty());
  assert(result.empty_weight == 1);
  assert(result.nonempty_weight == 2);
  assert(result.nonempty.size() == 1);
  assert(result.nonempty.entries.front().second == 2);
}

void fall_through_factory_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result = FallThroughFactory::apply(
      variant_log({{"B", "A", "A"}, {"A", "A", "B"},
                   {"A", "A", "C"}}),
      state);
  assert(result.handled());
  assert(result.node->kind() == PowlNodeKind::kStrictPartialOrder);
}

void fall_through_flower_contract() {
  const auto result = FlowerFallThrough::build({"A", "B"});
  assert(result.kind() == PowlNodeKind::kLoop);
  assert(result.children()[0].kind() == PowlNodeKind::kXor);
  assert(result.children()[1].kind() == PowlNodeKind::kSilent);
}

void fall_through_strict_tau_loop_contract() {
  const auto result = StrictTauLoopFallThrough::project(
      variant_log({{"A", "B", "A", "B"}}));
  assert(result.size() == 1);
  assert(result.entries.front().first == Sequence({"A", "B"}));
  assert(result.entries.front().second == 2);
}

void fall_through_tau_loop_contract() {
  const auto result =
      TauLoopFallThrough::project(variant_log({{"A", "B", "A"}}));
  assert(result.size() == 2);
  assert(result.entries.front().first == Sequence({"A", "B"}));
}

void filtering_contract() {
  VariantLog log;
  add_variant(log, {"A", "B"}, 5);
  add_variant(log, {"B", "A"}, 1);
  PowlDiscoveryOptions options;
  options.filtering_threshold = 0.5;
  const auto result = PowlFiltering::apply(log, options);
  assert(result.has_value());
  assert(result->size() == 1);
  assert(result->entries.front().second == 5);
}

void variants_brute_force_partial_order_cut_contract() {
  auto state = state_for(PowlDiscoveryVariant::kBruteForce);
  const auto result = BruteForcePartialOrderCut::detect(
      variant_log({{"A", "B", "C"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->children().size() == 3);
}

void variants_brute_force_factory_contract() {
  auto state = state_for(PowlDiscoveryVariant::kBruteForce);
  const auto result = BruteForceCutFactory::find(
      variant_log({{"A", "B", "C"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_dynamic_clustering_frequency_partial_order_cut_contract() {
  auto state = state_for(PowlDiscoveryVariant::kDynamicClustering);
  const auto result = DynamicClusteringFrequencyPartialOrderCut::detect(
      variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->has_edge(0, 1));
}

void variants_dynamic_clustering_frequency_factory_contract() {
  auto state = state_for(PowlDiscoveryVariant::kDynamicClustering);
  const auto result = DynamicClusteringFrequencyCutFactory::find(
      variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_dynamic_clustering_partial_order_cut_contract() {
  auto state = state_for(PowlDiscoveryVariant::kDynamicClusteringPlain);
  const auto result = DynamicClusteringPartialOrderCut::detect(
      variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result->has_edge(0, 1));
}

void variants_dynamic_clustering_factory_contract() {
  auto state = state_for(PowlDiscoveryVariant::kDynamicClusteringPlain);
  const auto result = DynamicClusteringCutFactory::find(
      variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_im_brute_force_contract() {
  auto state = state_for(PowlDiscoveryVariant::kBruteForce);
  const auto result =
      PowlBruteForceVariant::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_im_dynamic_clustering_contract() {
  auto state = state_for(PowlDiscoveryVariant::kDynamicClusteringPlain);
  const auto result = PowlDynamicClusteringVariant::discover(
      variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_im_dynamic_clustering_frequencies_contract() {
  auto state = state_for(PowlDiscoveryVariant::kDynamicClustering);
  const auto result = PowlDynamicClusteringFrequencyVariant::discover(
      variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_im_maximal_contract() {
  auto state = state_for(PowlDiscoveryVariant::kMaximal);
  const auto result =
      PowlMaximalVariant::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_im_tree_contract() {
  auto state = state_for(PowlDiscoveryVariant::kTree);
  const auto result =
      PowlTreeVariant::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_maximal_factory_contract() {
  auto state = state_for(PowlDiscoveryVariant::kMaximal);
  const auto result = MaximalCutFactory::find(
      variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_maximal_partial_order_cut_contract() {
  auto state = state_for(PowlDiscoveryVariant::kMaximal);
  const auto result = MaximalPartialOrderCut::detect(
      variant_log({{"A", "B"}}), state);
  assert(result.has_value());
  assert(result->kind() == PowlNodeKind::kStrictPartialOrder);
}

void variants_powl_discovery_varaints_contract() {
  const auto& descriptors = PowlSelectorRegistry::all();
  assert(descriptors.size() == 4);
  assert(PowlSelectorRegistry::supports(PowlDiscoveryVariant::kTree));
  assert(PowlSelectorRegistry::supports(
      PowlDiscoveryVariant::kDynamicClusteringPlain));
  for (const auto& descriptor : descriptors) {
    auto state = state_for(descriptor.variant);
    const auto result =
        PowlSelectorRegistry::discover(variant_log({{"A", "B"}}), state);
    assert(result.kind() != PowlNodeKind::kSilent);
  }
}

void tree_selector_contract() {
  auto state = state_for(PowlDiscoveryVariant::TREE);
  const auto result =
      PowlSelectorRegistry::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result.has_edge(0, 1));
}

void brute_force_selector_contract() {
  auto state = state_for(PowlDiscoveryVariant::BRUTE_FORCE);
  const auto result =
      PowlSelectorRegistry::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result.has_edge(0, 1));
}

void maximal_selector_contract() {
  auto state = state_for(PowlDiscoveryVariant::MAXIMAL);
  const auto result =
      PowlSelectorRegistry::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result.has_edge(0, 1));
}

void dynamic_clustering_selector_contract() {
  auto state = state_for(PowlDiscoveryVariant::DYNAMIC_CLUSTERING);
  const auto result =
      PowlSelectorRegistry::discover(variant_log({{"A", "B"}}), state);
  assert(result.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(result.has_edge(0, 1));
}

}  // namespace

int main() {
  base_case_abc_contract();
  base_case_empty_log_contract();
  base_case_factory_contract();
  base_case_single_activity_contract();
  cuts_concurrency_contract();
  cuts_factory_contract();
  cuts_loop_contract();
  cuts_sequence_contract();
  cuts_xor_contract();
  fall_through_activity_concurrent_contract();
  fall_through_activity_once_per_trace_contract();
  fall_through_empty_traces_contract();
  fall_through_factory_contract();
  fall_through_flower_contract();
  fall_through_strict_tau_loop_contract();
  fall_through_tau_loop_contract();
  filtering_contract();
  variants_brute_force_partial_order_cut_contract();
  variants_brute_force_factory_contract();
  variants_dynamic_clustering_frequency_partial_order_cut_contract();
  variants_dynamic_clustering_frequency_factory_contract();
  variants_dynamic_clustering_partial_order_cut_contract();
  variants_dynamic_clustering_factory_contract();
  variants_im_brute_force_contract();
  variants_im_dynamic_clustering_contract();
  variants_im_dynamic_clustering_frequencies_contract();
  variants_im_maximal_contract();
  variants_im_tree_contract();
  variants_maximal_factory_contract();
  variants_maximal_partial_order_cut_contract();
  variants_powl_discovery_varaints_contract();
  tree_selector_contract();
  brute_force_selector_contract();
  maximal_selector_contract();
  dynamic_clustering_selector_contract();
}
