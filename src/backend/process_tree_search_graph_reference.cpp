#include "minera/process_tree_search_graph_reference.hpp"
#include "minera/cancellation.hpp"

#include <algorithm>
#include <map>
#include <tuple>

namespace minera {

namespace {

constexpr std::size_t kNone = ProcessTreeReplaySemantics::kNoParent;

using Op = ProcessTreeOperator;

bool is_leaf(Op op) { return op == Op::kActivity || op == Op::kTau; }

std::optional<ProcessTreeReplayPath> concat(std::optional<ProcessTreeReplayPath> head,
                                            const ProcessTreeReplayPath& tail) {
  if (head) head->insert(head->end(), tail.begin(), tail.end());
  return head;
}

ProcessTreeReplayResult failed() { return {}; }

}  // namespace

ProcessTreeReplaySemantics::ProcessTreeReplaySemantics(const ProcessTree& tree) {
  std::map<std::tuple<int, std::size_t, std::vector<std::size_t>>, std::size_t> classes;
  auto visit = [&](auto&& self, const ProcessTreeNode& node, std::size_t parent) -> void {
    if (!is_leaf(node.op) && node.op != Op::kSequence && node.op != Op::kExclusive &&
        node.op != Op::kParallel && node.op != Op::kLoop) {
      throw ProcessTreeSearchGraphReferenceError(
          "the search-graph replay covers sequence, choice, parallel and loop operators");
    }
    if (node.op == Op::kLoop && node.children.size() != 2) {
      throw ProcessTreeSearchGraphReferenceError("the search-graph replay covers binary loops");
    }
    if (!is_leaf(node.op) && node.children.empty()) {
      throw ProcessTreeSearchGraphReferenceError("operator without children");
    }
    const std::size_t index = nodes_.size();
    Node entry;
    entry.op = node.op;
    entry.parent = parent;
    if (node.op == Op::kActivity) entry.activity = node.activity;
    nodes_.push_back(entry);
    if (parent != kNone) nodes_[parent].children.push_back(index);
    for (const auto& child : node.children) self(self, child, index);
    nodes_[index].end = nodes_.size();
    std::vector<std::size_t> child_classes;
    for (std::size_t child : nodes_[index].children) child_classes.push_back(nodes_[child].structure);
    const std::size_t label = node.op == Op::kActivity ? node.activity : kNone;
    const auto key = std::make_tuple(static_cast<int>(node.op), label, std::move(child_classes));
    const auto found = classes.emplace(key, classes.size());
    nodes_[index].structure = found.first->second;
  };
  visit(visit, tree.root(), kNone);
}

std::string ProcessTreeReplaySemantics::initial_state() const {
  std::string state(nodes_.size(), 'F');
  state[0] = 'E';
  return state;
}

void ProcessTreeReplaySemantics::check(std::size_t node, const std::string& state) const {
  if (node >= nodes_.size()) throw ProcessTreeSearchGraphReferenceError("node out of range");
  if (state.size() != nodes_.size()) throw ProcessTreeSearchGraphReferenceError("state size mismatch");
}

std::size_t ProcessTreeReplaySemantics::first_equal_sibling(std::size_t node) const {
  const auto& siblings = nodes_[nodes_[node].parent].children;
  for (std::size_t k = 0; k < siblings.size(); ++k) {
    if (nodes_[siblings[k]].structure == nodes_[node].structure) return k;
  }
  return 0;
}

bool ProcessTreeReplaySemantics::can_enable(std::size_t node, const std::string& state) const {
  check(node, state);
  const std::size_t parent = nodes_[node].parent;
  if (parent == kNone) return false;
  if (state[node] != 'F' || state[parent] != 'O') return false;
  const auto& siblings = nodes_[parent].children;
  switch (nodes_[parent].op) {
    case Op::kParallel:
      return true;
    case Op::kExclusive:
      return std::all_of(siblings.begin(), siblings.end(), [&](std::size_t c) { return state[c] == 'F'; });
    case Op::kSequence: {
      const std::size_t k = first_equal_sibling(node);
      return k == 0 || state[siblings[k - 1]] == 'C';
    }
    case Op::kLoop:
      return nodes_[node].structure == nodes_[siblings[1]].structure && state[siblings[0]] == 'C';
    default:
      return false;
  }
}

bool ProcessTreeReplaySemantics::can_open(std::size_t node, const std::string& state) const {
  check(node, state);
  return state[node] == 'E';
}

bool ProcessTreeReplaySemantics::can_close(std::size_t node, const std::string& state) const {
  check(node, state);
  const auto& children = nodes_[node].children;
  auto all = [&](auto predicate) { return std::all_of(children.begin(), children.end(), predicate); };
  switch (nodes_[node].op) {
    case Op::kActivity:
    case Op::kTau:
      return state[node] == 'O';
    case Op::kSequence:
    case Op::kParallel:
      return all([&](std::size_t c) { return state[c] == 'C'; });
    case Op::kExclusive:
      return all([&](std::size_t c) { return state[c] == 'C' || state[c] == 'F'; }) &&
             std::any_of(children.begin(), children.end(), [&](std::size_t c) { return state[c] == 'C'; });
    case Op::kLoop:
      return state[children[0]] == 'C' && state[children[1]] == 'F';
    default:
      return false;
  }
}

void ProcessTreeReplaySemantics::assign(std::string& state, ProcessTreeReplayPath& path, std::size_t node,
                                        char value) const {
  state[node] = value;
  path.push_back({node, value});
}

void ProcessTreeReplaySemantics::assign_subtree(std::string& state, ProcessTreeReplayPath& path,
                                                std::size_t node, char value) const {
  for (std::size_t x = node; x < nodes_[node].end; ++x) {
    if (state[x] != value) assign(state, path, x, value);
  }
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::transform_tree(std::size_t node, char target,
                                                                   const std::string& state) const {
  check(node, state);
  ProcessTreeReplayResult result{ProcessTreeReplayPath{}, state};
  assign_subtree(result.state, *result.path, node, target);
  return result;
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::enable_vertex(std::size_t node,
                                                                  const std::string& state) const {
  check(node, state);
  if (state[node] == 'E') return {ProcessTreeReplayPath{}, state};
  if (!can_enable(node, state)) return failed();
  ProcessTreeReplayResult result{ProcessTreeReplayPath{}, state};
  assign(result.state, *result.path, node, 'E');
  const std::size_t parent = nodes_[node].parent;
  const auto& siblings = nodes_[parent].children;
  if (nodes_[parent].op == Op::kExclusive) {
    for (std::size_t c : siblings) {
      if (c != node) assign_subtree(result.state, *result.path, c, 'C');
    }
  } else if (nodes_[parent].op == Op::kLoop && siblings[1] == node) {
    assign_subtree(result.state, *result.path, siblings[0], 'F');
  }
  return result;
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::open_vertex(std::size_t node,
                                                                const std::string& state) const {
  check(node, state);
  if (!can_open(node, state)) return failed();
  ProcessTreeReplayResult result{ProcessTreeReplayPath{}, state};
  assign(result.state, *result.path, node, 'O');
  const Op op = nodes_[node].op;
  const auto& children = nodes_[node].children;
  for (std::size_t k = 0; k < children.size(); ++k) {
    const bool first_runs = (op == Op::kSequence || op == Op::kLoop) && k == 0;
    assign(result.state, *result.path, children[k], first_runs ? 'E' : 'F');
  }
  return result;
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::close_vertex(std::size_t node,
                                                                 const std::string& state) const {
  check(node, state);
  if (!can_close(node, state)) return failed();
  ProcessTreeReplayResult result{ProcessTreeReplayPath{}, state};
  if (nodes_[node].op == Op::kLoop) assign_subtree(result.state, *result.path, nodes_[node].children[1], 'C');
  assign(result.state, *result.path, node, 'C');
  const std::size_t parent = nodes_[node].parent;
  if (parent != kNone && nodes_[parent].op == Op::kLoop && nodes_[parent].children[1] == node &&
      result.state[parent] == 'O') {
    assign(result.state, *result.path, nodes_[parent].children[0], 'E');
    assign_subtree(result.state, *result.path, node, 'F');
  }
  return result;
}

namespace {

// Runs `step` on the current state when the path so far succeeded; a failed
// step fails the whole path.
template <typename Step>
void then(std::optional<ProcessTreeReplayPath>& path, std::string& state, Step&& step) {
  if (!path) return;
  ProcessTreeReplayResult next = step(state);
  if (!next.path) {
    path.reset();
    return;
  }
  path->insert(path->end(), next.path->begin(), next.path->end());
  state = std::move(next.state);
}

}  // namespace

ProcessTreeReplayResult ProcessTreeReplaySemantics::shortest_path_to_open(std::size_t node,
                                                                          const std::string& state) const {
  check(node, state);
  if (state[node] == 'O') return {ProcessTreeReplayPath{}, state};
  std::optional<ProcessTreeReplayPath> path = ProcessTreeReplayPath{};
  std::string current = state;
  if (current[node] != 'E') {
    then(path, current, [&](const std::string& s) { return shortest_path_to_enable(node, s); });
    if (!path) return failed();
  }
  ProcessTreeReplayResult opened = open_vertex(node, current);
  if (!opened.path) return failed();
  return {concat(std::move(path), *opened.path), std::move(opened.state)};
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::reset(std::size_t node, const std::string& state) const {
  // The nearest enclosing loop that can run the node again: a closed redo
  // child does not count.
  std::size_t a = node;
  while (nodes_[a].parent != kNone) {
    const std::size_t p = nodes_[a].parent;
    if (nodes_[p].op == Op::kLoop && !(nodes_[p].children[1] == a && state[a] == 'C')) break;
    a = p;
  }
  const std::size_t loop = nodes_[a].parent;
  if (loop == kNone) return failed();
  const std::size_t body = nodes_[loop].children[0];
  const std::size_t redo = nodes_[loop].children[1];
  std::optional<ProcessTreeReplayPath> path = ProcessTreeReplayPath{};
  std::string current = state;
  if (current[loop] != 'O') {
    then(path, current, [&](const std::string& s) { return shortest_path_to_open(loop, s); });
    if (!path) return failed();
    return {std::move(path), std::move(current)};
  }
  if (a == redo || current[redo] == 'E' || current[redo] == 'O') {
    then(path, current, [&](const std::string& s) { return shortest_path_to_close(redo, s); });
  } else {
    then(path, current, [&](const std::string& s) { return shortest_path_to_close(body, s); });
    then(path, current, [&](const std::string& s) { return shortest_path_to_enable(redo, s); });
    then(path, current, [&](const std::string& s) { return shortest_path_to_open(redo, s); });
    then(path, current, [&](const std::string& s) { return shortest_path_to_close(redo, s); });
  }
  if (!path) return failed();
  return {std::move(path), std::move(current)};
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::shortest_path_to_enable(std::size_t node,
                                                                            const std::string& state) const {
  check(node, state);
  if (state[node] == 'E') return {ProcessTreeReplayPath{}, state};
  if (can_enable(node, state)) return enable_vertex(node, state);
  const std::size_t parent = nodes_[node].parent;
  if (parent == kNone) return failed();
  std::optional<ProcessTreeReplayPath> path = ProcessTreeReplayPath{};
  std::string current = state;
  if (current[node] == 'O') {
    then(path, current, [&](const std::string& s) { return shortest_path_to_close(node, s); });
    then(path, current, [&](const std::string& s) { return shortest_path_to_enable(node, s); });
    if (!path) return failed();
    return {std::move(path), std::move(current)};
  }
  if (current[node] == 'C') {
    then(path, current, [&](const std::string& s) { return reset(node, s); });
    if (!path) return failed();
  }
  if (current[node] == 'E') return {std::move(path), std::move(current)};
  if (current[parent] != 'O') {
    then(path, current, [&](const std::string& s) { return shortest_path_to_open(parent, s); });
    if (!path) return failed();
  }
  if (current[node] == 'E') return {std::move(path), std::move(current)};
  const auto& siblings = nodes_[parent].children;
  switch (nodes_[parent].op) {
    case Op::kSequence:
      for (std::size_t c : siblings) {
        if (c == node || !path) break;
        if (current[c] != 'C') {
          then(path, current, [&](const std::string& s) { return shortest_path_to_close(c, s); });
        }
      }
      break;
    case Op::kLoop:
      if (node == siblings[1]) {
        if (current[siblings[0]] != 'C') {
          then(path, current, [&](const std::string& s) { return shortest_path_to_close(siblings[0], s); });
        }
      } else if (current[siblings[1]] == 'E' || current[siblings[1]] == 'O') {
        then(path, current, [&](const std::string& s) { return shortest_path_to_close(siblings[1], s); });
      }
      break;
    case Op::kExclusive:
      if (std::any_of(siblings.begin(), siblings.end(),
                      [&](std::size_t c) { return c != node && current[c] != 'F'; })) {
        then(path, current, [&](const std::string& s) { return reset(node, s); });
      }
      break;
    default:
      break;
  }
  if (!path) return failed();
  if (current[node] == 'E') return {std::move(path), std::move(current)};
  ProcessTreeReplayResult enabled = enable_vertex(node, current);
  if (!enabled.path) return failed();
  return {concat(std::move(path), *enabled.path), std::move(enabled.state)};
}

ProcessTreeReplayResult ProcessTreeReplaySemantics::shortest_path_to_close(std::size_t node,
                                                                           const std::string& state) const {
  check(node, state);
  if (state[node] == 'C') return {ProcessTreeReplayPath{}, state};
  std::optional<ProcessTreeReplayPath> path = ProcessTreeReplayPath{};
  std::string current = state;
  if (current[node] == 'E' || current[node] == 'F') {
    then(path, current, [&](const std::string& s) { return shortest_path_to_open(node, s); });
    if (!path) return failed();
  }
  const auto& children = nodes_[node].children;
  auto close = [&](std::size_t child) {
    then(path, current, [&](const std::string& s) { return shortest_path_to_close(child, s); });
  };
  switch (nodes_[node].op) {
    case Op::kSequence:
    case Op::kParallel:
      for (std::size_t c : children) {
        if (current[c] != 'C') close(c);
        if (!path) return failed();
      }
      break;
    case Op::kExclusive: {
      const auto active = std::find_if(children.begin(), children.end(),
                                       [&](std::size_t c) { return current[c] == 'E' || current[c] == 'O'; });
      if (active != children.end()) {
        close(*active);
      } else if (std::none_of(children.begin(), children.end(), [&](std::size_t c) { return current[c] == 'C'; })) {
        // The first child whose own closing opens the fewest visible leaves.
        std::size_t best = kNone;
        std::size_t best_cost = 0;
        for (std::size_t c : children) {
          const ProcessTreeReplayResult trial = shortest_path_to_close(c, current);
          if (!trial.path) continue;
          std::size_t cost = 0;
          for (const auto& step : *trial.path) {
            if (step.state == 'O' && nodes_[step.node].op == Op::kActivity) ++cost;
          }
          if (best == kNone || cost < best_cost) {
            best = c;
            best_cost = cost;
          }
        }
        if (best == kNone) {
          throw ProcessTreeSearchGraphReferenceError("no child of the choice can close (TypeError)");
        }
        close(best);
      }
      if (!path) return failed();
      break;
    }
    case Op::kLoop:
      if (current[children[1]] == 'E' || current[children[1]] == 'O') {
        close(children[1]);
        if (!path) return failed();
      }
      if (current[children[0]] != 'C') {
        close(children[0]);
        if (!path) return failed();
      }
      break;
    default:
      break;
  }
  ProcessTreeReplayResult closed = close_vertex(node, current);
  if (!closed.path) return failed();
  return {concat(std::move(path), *closed.path), std::move(closed.state)};
}

std::vector<std::optional<ProcessTreeFrequency>> reference_search_graph_frequency_annotation(
    const ProcessTree& tree, const std::vector<std::vector<ProcessTreeSearchSegment>>& alignments) {
  const ProcessTreeReplaySemantics replay(tree);
  const std::size_t n = replay.size();
  auto parent = [&](std::size_t x) { return replay.parent(x); };
  auto children = [&](std::size_t x) -> const std::vector<std::size_t>& { return replay.children(x); };
  // get_bottomup_nodes: leaves breadth first, one per structural class.
  std::vector<std::size_t> breadth{0};
  for (std::size_t k = 0; k < breadth.size(); ++k) {
    for (std::size_t c : children(breadth[k])) breadth.push_back(c);
  }
  std::vector<std::size_t> kept;
  std::vector<bool> class_kept(n, false);
  for (std::size_t x : breadth) {
    if (!children(x).empty() || class_kept[replay.structure(x)]) continue;
    class_kept[replay.structure(x)] = true;
    kept.push_back(x);
  }
  for (bool changed = true; changed;) {
    changed = false;
    for (std::size_t k = 0; k < kept.size(); ++k) {
      const std::size_t up = parent(kept[k]);
      if (up == kNone || class_kept[replay.structure(up)]) continue;
      if (std::all_of(children(up).begin(), children(up).end(),
                      [&](std::size_t c) { return class_kept[replay.structure(c)]; })) {
        class_kept[replay.structure(up)] = true;
        kept.push_back(up);
        changed = true;
      }
    }
  }
  std::vector<std::optional<ProcessTreeFrequency>> result(n);
  for (std::size_t x : kept) {
    const std::size_t structure = replay.structure(x);
    ProcessTreeFrequency frequency;
    for (const auto& chain : alignments) {
      long long enabled = 0, opened = 0;
      // The root search state's path is not read.
      for (std::size_t k = 1; k < chain.size(); ++k) {
        for (const auto& step : chain[k].path) {
          if (step.node >= n) throw ProcessTreeSearchGraphReferenceError("node out of range");
          if (replay.structure(step.node) != structure) continue;
          if (step.state == 'E') ++enabled;
          if (step.state == 'O') ++opened;
        }
      }
      const long long count = enabled > 0 ? enabled : opened;
      frequency.num_events += count;
      if (count > 0) ++frequency.num_cases;
    }
    result[x] = frequency;
  }
  return result;
}

namespace {

struct SearchState {
  long long costs = 0;
  std::size_t index = 0;
  std::string state;
  std::vector<std::size_t> leaves;
  std::size_t parent = kNone;
  std::vector<std::size_t> children;
};

class SearchGraph {
 public:
  SearchGraph(const ProcessTree& tree, const std::vector<std::string>& variant,
              const std::vector<std::size_t>& order)
      : tree_(tree), replay_(tree), variant_(variant), order_(order) {
    if (order_.empty()) {
      for (std::size_t x = 0; x < replay_.size(); ++x) {
        if (replay_.children(x).empty()) order_.push_back(x);
      }
    }
    for (std::size_t x : order_) {
      if (x >= replay_.size() || !replay_.children(x).empty()) {
        throw ProcessTreeSearchGraphReferenceError("candidate order lists a node that is not a leaf");
      }
    }
  }

  ProcessTreeSearchGraphAlignment run() {
    const std::size_t result = search();
    ProcessTreeSearchGraphAlignment out;
    out.cost = states_[result].costs;
    out.alignment = construct(result);
    return out;
  }

 private:
  bool less(std::size_t a, std::size_t b) const {
    return states_[a].costs < states_[b].costs ||
           (states_[a].costs == states_[b].costs && states_[a].index > states_[b].index);
  }

  // heapq._siftdown / _siftup / heappush / heappop.
  void sift_down(std::size_t start, std::size_t pos) {
    const std::size_t item = heap_[pos];
    while (pos > start) {
      const std::size_t up = (pos - 1) >> 1;
      if (!less(item, heap_[up])) break;
      heap_[pos] = heap_[up];
      pos = up;
    }
    heap_[pos] = item;
  }
  void sift_up(std::size_t pos) {
    const std::size_t end = heap_.size();
    const std::size_t start = pos;
    const std::size_t item = heap_[pos];
    std::size_t child = 2 * pos + 1;
    while (child < end) {
      const std::size_t right = child + 1;
      if (right < end && !less(heap_[child], heap_[right])) child = right;
      heap_[pos] = heap_[child];
      pos = child;
      child = 2 * pos + 1;
    }
    heap_[pos] = item;
    sift_down(start, pos);
  }
  void push(std::size_t x) {
    heap_.push_back(x);
    sift_down(0, heap_.size() - 1);
  }
  std::size_t pop() {
    const std::size_t last = heap_.back();
    heap_.pop_back();
    if (heap_.empty()) return last;
    const std::size_t top = heap_[0];
    heap_[0] = last;
    sift_up(0);
    return top;
  }

  bool visible(std::size_t x) const { return replay_.op(x) == Op::kActivity; }
  bool labelled(std::size_t x, const std::string& label) const {
    return visible(x) && tree_.activities().at(replay_.activity(x)) == label;
  }
  std::vector<std::size_t> opened_leaves(const ProcessTreeReplayPath& path, bool tau) const {
    std::vector<std::size_t> out;
    for (const auto& step : path) {
      if (step.state == 'O' && replay_.children(step.node).empty() && (tau || visible(step.node))) {
        out.push_back(step.node);
      }
    }
    return out;
  }

  // _need_log_move: whether the enabling path does more than enable the
  // target through sequence/parallel/choice bookkeeping.
  bool needs_log_move(const std::string& before, const ProcessTreeReplayPath& path, std::size_t target) const {
    for (const auto& step : path) {
      const char from = before[step.node];
      const char to = step.state;
      bool allowed = false;
      if (step.node == target) {
        allowed = from == 'F' && (to == 'E' || to == 'F');
      } else {
        switch (replay_.op(step.node)) {
          case Op::kActivity:
          case Op::kTau:
            allowed = from == 'F' && (to == 'C' || to == 'F');
            break;
          case Op::kSequence:
          case Op::kParallel:
            allowed = from == 'F' || (from == 'E' && to == 'O') || (from == 'O' && to == 'C');
            break;
          case Op::kExclusive:
            allowed = (from == 'E' && to == 'O') || (from == 'F' && to == 'F') || (from == 'O' && to == 'C');
            break;
          default:
            allowed = from == 'F' && to == 'F';
            break;
        }
      }
      if (!allowed) return true;
    }
    return false;
  }

  void lower_costs(long long delta, const std::vector<std::size_t>& states) {
    for (std::size_t x : states) {
      states_[x].costs -= delta;
      lower_costs(delta, states_[x].children);
    }
  }

  bool merge(const SearchState& fresh, const std::vector<std::size_t>& collection) {
    for (std::size_t x : collection) {
      SearchState& old = states_[x];
      if (old.index != fresh.index || old.state != fresh.state) continue;
      if (fresh.costs < old.costs) {
        const long long delta = old.costs - fresh.costs;
        old.costs = fresh.costs;
        old.parent = fresh.parent;
        old.leaves = fresh.leaves;
        lower_costs(delta, old.children);
      }
      return true;
    }
    return false;
  }

  void add(SearchState fresh) {
    if (merge(fresh, closed_) || merge(fresh, heap_)) return;
    const std::size_t parent = fresh.parent;
    states_.push_back(std::move(fresh));
    states_[parent].children.push_back(states_.size() - 1);
    push(states_.size() - 1);
  }

  std::size_t search() {
    states_.push_back(SearchState{0, 0, replay_.initial_state(), {}, kNone, {}});
    heap_.push_back(0);
    while (!heap_.empty()) {
      const std::size_t current = pop();
      closed_.push_back(current);
      if (states_[current].index == variant_.size()) {
        const std::string& state = states_[current].state;
        if (std::all_of(state.begin(), state.end(), [](char c) { return c == 'C'; })) return current;
        ProcessTreeReplayResult closing = replay_.shortest_path_to_close(0, state);
        if (!closing.path) throw ProcessTreeSearchGraphReferenceError("the tree cannot be closed");
        states_[current].costs += static_cast<long long>(opened_leaves(*closing.path, false).size());
        const auto all = opened_leaves(*closing.path, true);
        states_[current].leaves.insert(states_[current].leaves.end(), all.begin(), all.end());
        states_[current].state = std::move(closing.state);
        push(current);
        continue;
      }
      const std::string state = states_[current].state;
      const long long costs = states_[current].costs;
      const std::size_t index = states_[current].index;
      bool need_log = false;
      bool candidate = false;
      for (std::size_t leaf : order_) {
        if (!labelled(leaf, variant_[index])) continue;
        candidate = true;
        ProcessTreeReplayResult enabled = replay_.shortest_path_to_enable(leaf, state);
        if (!enabled.path) {
          need_log = true;
          continue;
        }
        if (!need_log) need_log = needs_log_move(state, *enabled.path, leaf);
        ProcessTreeReplayResult closed = replay_.shortest_path_to_close(leaf, enabled.state);
        if (!closed.path) throw ProcessTreeSearchGraphReferenceError("an enabled leaf cannot be closed");
        ProcessTreeReplayPath path = *enabled.path;
        path.insert(path.end(), closed.path->begin(), closed.path->end());
        const auto cost = static_cast<long long>(opened_leaves(path, false).size());
        add(SearchState{costs + cost - 1, index + 1, std::move(closed.state), opened_leaves(path, true), current, {}});
      }
      if (need_log || !candidate) add(SearchState{costs + 1, index + 1, state, {}, current, {}});
    }
    throw ProcessTreeSearchGraphReferenceError("the search ran out of states");
  }

  std::vector<ProcessTreeSearchGraphMove> construct(std::size_t result) const {
    std::vector<std::vector<ProcessTreeSearchGraphMove>> parts;
    for (std::size_t x = result; states_[x].parent != kNone; x = states_[x].parent) {
      const SearchState& s = states_[x];
      const std::string& event = variant_.at(s.index - 1);
      std::vector<ProcessTreeSearchGraphMove> moves;
      if (s.leaves.empty()) {
        moves.push_back({event, std::nullopt});
      } else if (x == result) {
        std::optional<std::size_t> sync;
        for (std::size_t k = s.leaves.size(); k-- > 0;) {
          if (labelled(s.leaves[k], event)) {
            sync = k;
            break;
          }
        }
        for (std::size_t k = 0; k < s.leaves.size(); ++k) {
          moves.push_back({sync == k ? std::optional<std::string>(event) : std::nullopt, s.leaves[k]});
        }
        if (!sync && s.costs - states_[s.parent].costs > static_cast<long long>(s.leaves.size())) {
          moves.push_back({event, std::nullopt});
        }
      } else {
        for (std::size_t k = 0; k + 1 < s.leaves.size(); ++k) moves.push_back({std::nullopt, s.leaves[k]});
        moves.push_back({event, s.leaves.back()});
      }
      parts.push_back(std::move(moves));
    }
    std::vector<ProcessTreeSearchGraphMove> out;
    for (auto part = parts.rbegin(); part != parts.rend(); ++part) out.insert(out.end(), part->begin(), part->end());
    return out;
  }

  const ProcessTree& tree_;
  ProcessTreeReplaySemantics replay_;
  const std::vector<std::string>& variant_;
  std::vector<std::size_t> order_;
  std::vector<SearchState> states_;
  std::vector<std::size_t> heap_;
  std::vector<std::size_t> closed_;
};

}  // namespace

ProcessTreeSearchGraphAlignment reference_search_graph_align_variant(const ProcessTree& tree,
                                                                     const std::vector<std::string>& variant,
                                                                     const std::vector<std::size_t>& candidate_order) {
  return SearchGraph(tree, variant, candidate_order).run();
}

std::vector<ProcessTreeSearchGraphAlignment> reference_search_graph_alignments(
    const ProcessTree& tree, const std::vector<std::vector<std::string>>& traces,
    const std::function<bool()>& cancelled) {
  const long long best_worst = reference_search_graph_align_variant(tree, {}).cost;
  std::vector<ProcessTreeSearchGraphAlignment> out;
  out.reserve(traces.size());
  for (const auto& trace : traces) {
    throw_if_cancelled(cancelled, "process-tree alignment was cancelled");
    ProcessTreeSearchGraphAlignment aligned = reference_search_graph_align_variant(tree, trace);
    const long long denominator = best_worst + static_cast<long long>(trace.size());
    aligned.fitness =
        denominator > 0 ? 1.0 - static_cast<double>(aligned.cost) / static_cast<double>(denominator) : 0.0;
    out.push_back(std::move(aligned));
  }
  return out;
}

}  // namespace minera
