// Escaping-edges precision on the conformance dashboard path must follow the
// reference ETConformance prefix rules. Expected values were captured from the
// pinned 2.7.22 reference (precision_token_based_replay).
#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

#include "../petri_replay_internal.hpp"

#include <cassert>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace {

minera::EventLog log_from_variants(const std::vector<std::vector<std::string>>& variants) {
  minera::EventLog log;
  std::size_t case_index = 0;
  for (const auto& variant : variants) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& activity : variant) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool near(double actual, double expected) { return std::abs(actual - expected) < 1e-9; }

minera::ProcessTreeNode node(minera::ProcessTreeOperator op,
                             std::vector<minera::ProcessTreeNode> children) {
  return minera::ProcessTreeNode{op, 0, std::move(children)};
}

double dashboard_precision(const minera::EventLog& log, const minera::PetriNet& net) {
  const auto result = minera::check_conformance(log, net, minera::TokenReplayOptions{});
  assert(result.precision_computed);
  return result.precision;
}

double evaluator_precision(const minera::EventLog& log, const minera::PetriNet& net) {
  return minera::evaluate_precision(log, net, minera::PrecisionVariant::ETCONFORMANCE_TOKEN).precision;
}

// Model A -> B, log B, A, B: the very first event misses a token, so no proper
// prefix is valid and the reference scores 0, whatever the fitness replay's
// own stop-on-unfit setting is.
void invalid_prefixes_never_contribute() {
  const auto net = minera::to_petri_net(minera::ProcessTree(
      {"A", "B"}, node(minera::ProcessTreeOperator::kSequence,
                       {minera::ProcessTreeNode::leaf(0), minera::ProcessTreeNode::leaf(1)})));
  const auto log = log_from_variants({{"B", "A", "B"}});
  assert(near(dashboard_precision(log, net), 0.0));
  minera::TokenReplayOptions keep_going;
  keep_going.stop_immediately_unfit = false;
  assert(near(minera::check_conformance(log, net, keep_going).precision, 0.0));
  assert(near(evaluator_precision(log, net), 0.0));
}

// Labels absent from the model keep their identity in prefixes: X and Y are
// different prefixes (reference 0.25), while a repeated X is shared (0.5).
void unknown_labels_keep_prefix_identity() {
  const auto net = minera::to_petri_net(minera::ProcessTree(
      {"B", "C"}, node(minera::ProcessTreeOperator::kExclusive,
                       {minera::ProcessTreeNode::leaf(0), minera::ProcessTreeNode::leaf(1)})));
  const auto distinct = log_from_variants({{"X", "B"}, {"Y", "C"}});
  assert(near(dashboard_precision(distinct, net), 0.25));
  assert(near(evaluator_precision(distinct, net), 0.25));
  const auto shared = log_from_variants({{"X", "B"}, {"X", "C"}});
  assert(near(dashboard_precision(shared, net), 0.5));
  assert(near(evaluator_precision(shared, net), 0.5));
}

// A silent split into `branches` parallel branches, each doing A<i> or a
// silent skip, joined by visible Z. Every A<i> and Z is eventually enabled at
// the initial marking; 13 branches reach 8192 silent markings.
minera::PetriNet wide_optional_split(std::size_t branches) {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  std::vector<std::string> activities;
  std::vector<std::size_t> starts, ends;
  for (std::size_t i = 0; i < branches; ++i) {
    activities.push_back("A" + std::to_string(i));
    starts.push_back(net.add_place());
    ends.push_back(net.add_place());
  }
  activities.push_back("Z");
  net.set_activities(activities);
  net.add_transition(std::nullopt, {source}, starts);
  for (std::size_t i = 0; i < branches; ++i) {
    net.add_transition(i, {starts[i]}, {ends[i]});
    net.add_transition(std::nullopt, {starts[i]}, {ends[i]});
  }
  net.add_transition(branches, ends, {sink});
  return net;
}

void silent_search_is_complete_or_refuses() {
  for (const std::size_t branches : {12u, 13u}) {
    const auto net = wide_optional_split(branches);
    minera::TokenReplayOptions options;
    const auto enabled = minera::detail::initial_enabled_token_activities(net, options);
    assert(enabled.size() == branches + 1);
    const auto log = log_from_variants({{"Z"}});
    assert(near(dashboard_precision(log, net), 1.0 / double(branches + 1)));
    assert(near(evaluator_precision(log, net), 1.0 / double(branches + 1)));
  }

  // A forced small budget cannot produce an ordinary number. The reference
  // worklist reaches Z after 14 silent firings (each re-enabled skip fires only
  // from the newest marking), so the budget must be below that.
  const minera::detail::ScopedEnabledSearchBudget small(4);
  const auto net = wide_optional_split(13);
  const auto result =
      minera::check_conformance(log_from_variants({{"Z"}}), net, minera::TokenReplayOptions{});
  assert(!result.precision_computed && std::isnan(result.precision));
  bool refused = false;
  try { (void)minera::detail::initial_enabled_token_activities(net, minera::TokenReplayOptions{}); }
  catch (const minera::detail::SilentSearchBudgetExhausted&) { refused = true; }
  assert(refused);
  refused = false;
  try { (void)evaluator_precision(log_from_variants({{"Z"}}), net); }
  catch (const std::runtime_error&) { refused = true; }
  assert(refused);
}

}  // namespace

int main() {
  invalid_prefixes_never_contribute();
  unknown_labels_keep_prefix_identity();
  silent_search_is_complete_or_refuses();
  return 0;
}
