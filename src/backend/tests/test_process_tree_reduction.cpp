#include "minera/process_tree_reduction.hpp"

#include <cassert>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using minera::Event;
using minera::EventAttribute;
using minera::ProcessTree;
using minera::ProcessTreeNode;
using minera::ProcessTreeOperator;
using minera::Trace;

ProcessTreeNode seq(std::vector<ProcessTreeNode> children) {
  return {ProcessTreeOperator::kSequence, 0, std::move(children)};
}
ProcessTreeNode xor_node(std::vector<ProcessTreeNode> children) {
  return {ProcessTreeOperator::kExclusive, 0, std::move(children)};
}
ProcessTreeNode loop(std::vector<ProcessTreeNode> children) {
  return {ProcessTreeOperator::kLoop, 0, std::move(children)};
}

bool same(const ProcessTreeNode& lhs, const ProcessTreeNode& rhs) {
  if (lhs.op != rhs.op || lhs.activity != rhs.activity ||
      lhs.children.size() != rhs.children.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.children.size(); ++i) {
    if (!same(lhs.children[i], rhs.children[i])) return false;
  }
  return true;
}

Trace trace(std::initializer_list<std::string> names) {
  Trace result;
  for (const auto& name : names) result.events.push_back(Event{"", name});
  return result;
}

void expect_invalid(const ProcessTree& tree) {
  bool threw = false;
  try {
    (void)minera::reduce_process_tree(tree, Trace{});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  const ProcessTree optional(
      {"A", "B"}, seq({xor_node({ProcessTreeNode::tau(),
                                  ProcessTreeNode::leaf(0)}),
                          ProcessTreeNode::leaf(1)}));
  const auto original = optional.root();

  // An absent optional branch is replaced and then removed from the sequence.
  const auto reduced = minera::reduce_process_tree(optional, trace({"B", "B"}));
  assert(same(reduced.root(), ProcessTreeNode::leaf(1)));
  // A present activity keeps its optional branch intact.
  const auto retained = minera::reduce_process_tree(optional, trace({"A"}));
  assert(same(retained.root(), original));
  // The source tree is a value input and remains untouched.
  assert(same(optional.root(), original));

  // Unknown labels and duplicate labels do not prevent reduction.
  const auto unknown = minera::reduce_process_tree(
      optional, trace({"B", "B", "not-in-the-model"}));
  assert(same(unknown.root(), ProcessTreeNode::leaf(1)));

  // XOR keeps one tau branch after redundant optional branches collapse.
  const ProcessTree two_optional(
      {"A", "B"}, xor_node({xor_node({ProcessTreeNode::tau(),
                                        ProcessTreeNode::leaf(0)}),
                              xor_node({ProcessTreeNode::tau(),
                                        ProcessTreeNode::leaf(1)})}));
  const auto one_tau = minera::reduce_process_tree(two_optional, trace({"C"}));
  assert(one_tau.root().op == ProcessTreeOperator::kTau);

  // The root is never replaced directly.
  const ProcessTree root_optional(
      {"A"}, xor_node({ProcessTreeNode::tau(), ProcessTreeNode::leaf(0)}));
  const auto root_kept = minera::reduce_process_tree(root_optional, trace({"B"}));
  assert(same(root_kept.root(), root_optional.root()));

  // Deduplicating XOR tau branches preserves the position of the first tau.
  const ProcessTree tau_middle(
      {"A", "B"}, xor_node({ProcessTreeNode::leaf(0), ProcessTreeNode::tau(),
                              ProcessTreeNode::leaf(1)}));
  assert(same(minera::reduce_process_tree(tau_middle, trace({"A", "B"})).root(),
              tau_middle.root()));
  const ProcessTree tau_last(
      {"A", "B"}, xor_node({ProcessTreeNode::leaf(0),
                              ProcessTreeNode::leaf(1), ProcessTreeNode::tau()}));
  assert(same(minera::reduce_process_tree(tau_last, trace({"A", "B"})).root(),
              tau_last.root()));
  const ProcessTree repeated_tau(
      {"A", "B"}, xor_node({ProcessTreeNode::leaf(0), ProcessTreeNode::tau(),
                              ProcessTreeNode::leaf(1), ProcessTreeNode::tau()}));
  const ProcessTreeNode first_tau_retained =
      xor_node({ProcessTreeNode::leaf(0), ProcessTreeNode::tau(),
                ProcessTreeNode::leaf(1)});
  assert(same(minera::reduce_process_tree(repeated_tau, trace({"A", "B"})).root(),
              first_tau_retained));

  // Folding keeps mixed binary loops intact, whichever child is tau.
  const ProcessTree tau_redo(
      {"A"}, loop({ProcessTreeNode::leaf(0), ProcessTreeNode::tau()}));
  assert(same(minera::reduce_process_tree(tau_redo, trace({"A"})).root(),
              tau_redo.root()));
  const ProcessTree tau_body(
      {"B"}, loop({ProcessTreeNode::tau(), ProcessTreeNode::leaf(0)}));
  assert(same(minera::reduce_process_tree(tau_body, trace({"B"})).root(),
              tau_body.root()));

  // Mandatory subtrees are left alone.
  const ProcessTree mandatory({"A", "B"},
                              seq({ProcessTreeNode::leaf(0),
                                   ProcessTreeNode::leaf(1)}));
  assert(same(minera::reduce_process_tree(mandatory, trace({"A"})).root(),
              mandatory.root()));
  const ProcessTree interleaving(
      {"A", "B"}, ProcessTreeNode{ProcessTreeOperator::kInterleaving, 0,
                                    {ProcessTreeNode::leaf(0),
                                     ProcessTreeNode::leaf(1)}});
  assert(same(minera::reduce_process_tree(interleaving, trace({"A", "B"})).root(),
              interleaving.root()));

  // Custom activity keys accept strings and reject missing/non-string values.
  const ProcessTree custom({"A"}, ProcessTreeNode::leaf(0));
  Trace custom_trace;
  custom_trace.events.push_back(
      Event{"", "ignored", "", "", 0,
            {EventAttribute{"my.activity", "A", minera::AttributeKind::kString}}});
  assert(same(minera::reduce_process_tree(
                  custom, custom_trace,
                  minera::ProcessTreeReductionOptions{
                      minera::ProcessTreeReductionVariant::kTreeTrBased,
                      "my.activity"})
                  .root(),
              custom.root()));

  Trace missing;
  missing.events.push_back(Event{"", "A"});
  bool threw = false;
  try {
    (void)minera::reduce_process_tree(
        custom, missing,
        minera::ProcessTreeReductionOptions{
            minera::ProcessTreeReductionVariant::kTreeTrBased, "my.activity"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  Trace non_string;
  non_string.events.push_back(
      Event{"", "A", "", "", 0,
            {EventAttribute{"my.activity", "A", minera::AttributeKind::kInt}}});
  threw = false;
  try {
    (void)minera::reduce_process_tree(
        custom, non_string,
        minera::ProcessTreeReductionOptions{
            minera::ProcessTreeReductionVariant::kTreeTrBased, "my.activity"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // Malformed trees and unsupported variants are rejected before reduction.
  expect_invalid(ProcessTree(
      {"A"}, ProcessTreeNode{ProcessTreeOperator::kActivity, 0,
                              {ProcessTreeNode::tau()}}));
  expect_invalid(ProcessTree(
      {"A"}, ProcessTreeNode{ProcessTreeOperator::kTau, 0,
                              {ProcessTreeNode::leaf(0)}}));
  expect_invalid(ProcessTree(
      {}, ProcessTreeNode{ProcessTreeOperator::kSequence, 0, {}}));
  expect_invalid(ProcessTree(
      {}, ProcessTreeNode{ProcessTreeOperator::kParallel, 0, {}}));
  expect_invalid(ProcessTree(
      {}, ProcessTreeNode{ProcessTreeOperator::kInterleaving, 0, {}}));
  expect_invalid(ProcessTree(
      {}, ProcessTreeNode{ProcessTreeOperator::kExclusive, 0, {}}));
  expect_invalid(ProcessTree(
      {}, ProcessTreeNode{ProcessTreeOperator::kLoop, 0, {}}));
  expect_invalid(ProcessTree({"A"}, ProcessTreeNode::leaf(1)));
  expect_invalid(ProcessTree(
      {}, ProcessTreeNode{static_cast<ProcessTreeOperator>(99), 0, {}}));

  threw = false;
  try {
    (void)minera::reduce_process_tree(
        custom, Trace{},
        minera::ProcessTreeReductionOptions{
            static_cast<minera::ProcessTreeReductionVariant>(99), {}});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  return 0;
}
