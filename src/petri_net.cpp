#include "minera/petri_net.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace minera {
namespace {

// Wires the fragment for `node` between the given entry and exit places,
// appending the transitions (and any intermediate places) to `net`.
void build(const ProcessTreeNode& node,
           std::size_t in,
           std::size_t out,
           PetriNet& net) {
  switch (node.op) {
    case ProcessTreeOperator::kActivity:
      net.add_transition(node.activity, {in}, {out});
      return;
    case ProcessTreeOperator::kTau:
      net.add_transition(std::nullopt, {in}, {out});
      return;
    case ProcessTreeOperator::kSequence: {
      std::size_t prev = in;
      for (std::size_t i = 0; i < node.children.size(); ++i) {
        const std::size_t next =
            i + 1 < node.children.size() ? net.add_place() : out;
        build(node.children[i], prev, next, net);
        prev = next;
      }
      return;
    }
    case ProcessTreeOperator::kExclusive:
      // Every branch shares the entry and exit places, so the single entry
      // token enables exactly one branch's first transition.
      for (const auto& child : node.children) {
        build(child, in, out, net);
      }
      return;
    case ProcessTreeOperator::kParallel: {
      // Silent AND-split forks a token into every branch; the AND-join waits
      // for all branches before producing the exit token.
      std::vector<std::size_t> branch_in;
      std::vector<std::size_t> branch_out;
      branch_in.reserve(node.children.size());
      branch_out.reserve(node.children.size());
      for (const auto& child : node.children) {
        const std::size_t ci = net.add_place();
        const std::size_t co = net.add_place();
        build(child, ci, co, net);
        branch_in.push_back(ci);
        branch_out.push_back(co);
      }
      net.add_transition(std::nullopt, {in}, branch_in);
      net.add_transition(std::nullopt, branch_out, {out});
      return;
    }
    case ProcessTreeOperator::kLoop: {
      // child[0] is the body (executed at least once between in and out);
      // child[1..] are redo paths that lead from the exit back to the entry.
      if (node.children.empty()) {
        net.add_transition(std::nullopt, {in}, {out});
        return;
      }
      build(node.children[0], in, out, net);
      for (std::size_t i = 1; i < node.children.size(); ++i) {
        build(node.children[i], out, in, net);
      }
      return;
    }
  }
}

}  // namespace

PetriNet to_petri_net(const ProcessTree& tree) {
  PetriNet net;
  net.set_activities(tree.activities());
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  build(tree.root(), source, sink, net);
  return net;
}

}  // namespace minera
