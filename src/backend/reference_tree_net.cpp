#include "reference_tree_net.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace minera {
namespace {

struct Builder {
  struct Step {
    std::optional<std::size_t> activity;
    std::vector<std::size_t> inputs, outputs;
    bool removed = false;
  };
  std::size_t places = 0;
  std::vector<Step> steps;

  std::size_t place() { return places++; }
  void step(std::optional<std::size_t> activity, std::vector<std::size_t> inputs,
            std::vector<std::size_t> outputs) {
    steps.push_back({activity, std::move(inputs), std::move(outputs)});
  }

  void add(const ProcessTreeNode& node, std::size_t entry, std::size_t exit) {
    const auto& children = node.children;
    switch (node.op) {
      case ProcessTreeOperator::kActivity:
        step(node.activity, {entry}, {exit});
        return;
      case ProcessTreeOperator::kTau:
        step(std::nullopt, {entry}, {exit});
        return;
      case ProcessTreeOperator::kSequence: {
        auto current = entry;
        for (std::size_t index = 0; index < children.size(); ++index) {
          const auto next = index + 1 == children.size() ? exit : place();
          add(children[index], current, next);
          current = next;
        }
        return;
      }
      case ProcessTreeOperator::kExclusive:
        for (const auto& child : children) add(child, entry, exit);
        return;
      case ProcessTreeOperator::kParallel: {
        std::vector<std::size_t> starts, ends;
        for (std::size_t index = 0; index < children.size(); ++index) starts.push_back(place());
        for (std::size_t index = 0; index < children.size(); ++index) ends.push_back(place());
        step(std::nullopt, {entry}, starts);
        for (std::size_t index = 0; index < children.size(); ++index)
          add(children[index], starts[index], ends[index]);
        step(std::nullopt, ends, {exit});
        return;
      }
      case ProcessTreeOperator::kLoop: {
        if (children.empty()) throw std::invalid_argument("loop without a body");
        const auto body_start = place();
        const auto body_end = place();
        step(std::nullopt, {entry}, {body_start});
        add(children.front(), body_start, body_end);
        const auto redo = place();
        for (std::size_t index = 1; index < children.size(); ++index) add(children[index], body_end, redo);
        step(std::nullopt, {redo}, {body_start});
        step(std::nullopt, {body_end}, {exit});
        return;
      }
      default:
        throw std::invalid_argument("OCPN nets support sequence, choice, parallel and loop operators only");
    }
  }

  // Folds silent transitions through places with exactly one input and one
  // output arc, until none is left.
  void reduce(std::size_t source, std::size_t sink) {
    for (bool changed = true; changed;) {
      changed = false;
      for (std::size_t q = 0; q < places && !changed; ++q) {
        if (q == source || q == sink) continue;
        Step* producer = nullptr;
        Step* consumer = nullptr;
        std::size_t produced = 0, consumed = 0;
        for (auto& candidate : steps) {
          if (candidate.removed) continue;
          const auto out = std::count(candidate.outputs.begin(), candidate.outputs.end(), q);
          const auto in = std::count(candidate.inputs.begin(), candidate.inputs.end(), q);
          produced += static_cast<std::size_t>(out);
          consumed += static_cast<std::size_t>(in);
          if (out) producer = &candidate;
          if (in) consumer = &candidate;
        }
        if (produced != 1 || consumed != 1 || producer == consumer) continue;
        if (!consumer->activity && consumer->inputs.size() == 1) {
          std::erase(producer->outputs, q);
          producer->outputs.insert(producer->outputs.end(), consumer->outputs.begin(), consumer->outputs.end());
          consumer->removed = true;
        } else if (!producer->activity && producer->outputs.size() == 1) {
          std::erase(consumer->inputs, q);
          consumer->inputs.insert(consumer->inputs.end(), producer->inputs.begin(), producer->inputs.end());
          producer->removed = true;
        } else {
          continue;
        }
        changed = true;
      }
    }
  }
};

bool starts_with_loop(const ProcessTreeNode& node) {
  if (node.op == ProcessTreeOperator::kLoop) return true;
  return !node.children.empty() && starts_with_loop(node.children.front());
}

bool ends_with_loop(const ProcessTreeNode& node) {
  if (node.op == ProcessTreeOperator::kLoop) return true;
  return !node.children.empty() && ends_with_loop(node.children.back());
}

bool needs_initial_step(const ProcessTreeNode& first) {
  if (starts_with_loop(first)) return true;
  const auto* node = &first;
  while (node->op == ProcessTreeOperator::kSequence) node = &node->children.front();
  return (node->op == ProcessTreeOperator::kExclusive || node->op == ProcessTreeOperator::kParallel) &&
         ends_with_loop(first);
}

}  // namespace

PetriNet process_tree_reference_net(const ProcessTree& tree) {
  Builder builder;
  const auto source = builder.place();
  const auto sink = builder.place();
  const auto& root = tree.root();
  bool initial = false, final = false;
  if (root.op == ProcessTreeOperator::kSequence && !root.children.empty()) {
    initial = needs_initial_step(root.children.front());
    final = ends_with_loop(root.children.back());
  } else if (!root.children.empty()) {
    initial = final = true;
  }
  auto entry = source, exit = sink;
  if (initial) {
    entry = builder.place();
    builder.step(std::nullopt, {source}, {entry});
  }
  if (final) {
    exit = builder.place();
    builder.step(std::nullopt, {exit}, {sink});
  }
  builder.add(root, entry, exit);
  builder.reduce(source, sink);

  // Renumber the surviving places, source and sink first.
  std::vector<bool> used(builder.places, false);
  used[source] = used[sink] = true;
  for (const auto& step : builder.steps) {
    if (step.removed) continue;
    for (const auto place : step.inputs) used[place] = true;
    for (const auto place : step.outputs) used[place] = true;
  }
  PetriNet net;
  net.set_activities(tree.activities());
  std::vector<std::size_t> index(builder.places, 0);
  for (std::size_t place = 0; place < builder.places; ++place)
    if (used[place]) index[place] = net.add_place();
  net.set_source(index[source]);
  net.set_sink(index[sink]);
  for (const auto& step : builder.steps) {
    if (step.removed) continue;
    std::vector<std::size_t> inputs, outputs;
    for (const auto place : step.inputs) inputs.push_back(index[place]);
    for (const auto place : step.outputs) outputs.push_back(index[place]);
    net.add_transition(step.activity, std::move(inputs), std::move(outputs));
  }
  return net;
}

}  // namespace minera
