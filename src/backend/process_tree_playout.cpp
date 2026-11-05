#include "minera/process_tree_playout.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <cmath>
#include <optional>
#include <numeric>
#include <unordered_map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/random.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

using Word = std::vector<std::string>;
using Words = std::vector<Word>;

// EXTENSIVE follows the reference (algo.simulation.playout.process_tree
// variants.extensive of PM4Py 2.7.22) as observed from its outputs. Every
// node's set of words is built bottom-up: a tau gives the empty word, an
// activity its label, an exclusive choice the union of its children, a
// sequence the concatenations, a parallel cut the interleavings and a loop
// do, do redo do, ... with up to max_loop_occ redos (only the first redo
// child is used). Activities, sequences, parallel cuts and loops then keep a
// word w when
//   |w| + (min_len(root) - min_len(node)) <= max_trace_length and
//   |w| + (max_len(root) - max_len(node)) >= min_trace_length,
// where min_len and max_len are the shortest and longest words of a
// subtree (the reference's lengths: a loop's shortest word is its do
// child's, its longest counts as a huge constant plus every child's
// longest). A tau and an exclusive choice keep everything, so the result
// can hold words outside the length bounds. Unset bounds default to the
// reference's: max_trace_length is min_len(root), max_loop_occ is half of
// max_trace_length.
struct ExtensiveLength {
  // Words of a loop are unbounded; the reference counts each loop as one
  // huge constant, so lengths are (loops, finite part) pairs compared
  // lexicographically.
  long long loops = 0;
  long long finite = 0;
  friend bool operator<(const ExtensiveLength& a, const ExtensiveLength& b) {
    return a.loops != b.loops ? a.loops < b.loops : a.finite < b.finite;
  }
  ExtensiveLength& operator+=(const ExtensiveLength& other) {
    loops += other.loops;
    finite += other.finite;
    return *this;
  }
};

struct ExtensiveBudget {
  std::size_t remaining = 250000;
  bool exhausted = false;
  // Takes one generated word from the budget; false once it is spent.
  bool take() {
    if (remaining == 0) {
      exhausted = true;
      return false;
    }
    --remaining;
    return true;
  }
};

bool node_has_operator(const ProcessTreeNode& n, ProcessTreeOperator op) {
  if (n.op == op) return true;
  for (const auto& child : n.children)
    if (node_has_operator(child, op)) return true;
  return false;
}

void validate_node(const ProcessTreeNode& n, const ProcessTree& t) {
  switch (n.op) {
    case ProcessTreeOperator::kActivity:
      if (!n.children.empty())
        throw std::invalid_argument("process-tree activity cannot have children");
      if (n.activity >= t.activities().size())
        throw std::invalid_argument("process-tree activity index is out of range");
      return;
    case ProcessTreeOperator::kTau:
      if (!n.children.empty())
        throw std::invalid_argument("process-tree tau cannot have children");
      return;
    case ProcessTreeOperator::kSequence:
    case ProcessTreeOperator::kExclusive:
    case ProcessTreeOperator::kInclusiveChoice:
    case ProcessTreeOperator::kParallel:
    case ProcessTreeOperator::kInterleaving:
      if (n.children.empty())
        throw std::invalid_argument("process-tree operator requires at least one child");
      break;
    case ProcessTreeOperator::kLoop:
      if (n.children.size() < 2)
        throw std::invalid_argument("process-tree loop requires a body and redo child");
      break;
    default:
      throw std::invalid_argument("unsupported process-tree operator");
  }
  for (const auto& c : n.children) validate_node(c, t);
}

bool has_single_child_block(const ProcessTreeNode& n) {
  if ((n.op == ProcessTreeOperator::kSequence ||
       n.op == ProcessTreeOperator::kParallel) &&
      n.children.size() == 1)
    return true;
  for (const auto& child : n.children)
    if (has_single_child_block(child)) return true;
  return false;
}

// Whether an inclusive choice is read by its parent: anywhere except as the
// third or later child of a loop, which the reference never reads.
bool inclusive_choice_read(const ProcessTreeNode& n) {
  if (n.op == ProcessTreeOperator::kInclusiveChoice) return true;
  for (std::size_t i = 0; i < n.children.size(); ++i) {
    const auto& child = n.children[i];
    if (n.op == ProcessTreeOperator::kLoop && i >= 2) {
      if (child.op == ProcessTreeOperator::kInclusiveChoice) continue;
    }
    if (inclusive_choice_read(child)) return true;
  }
  return false;
}

// The reference walks its nodes bottom-up from a set of nodes that compares
// subtrees by value: one representative per equal subtree (the first in
// breadth-first order) is kept, and a parent is added when one of its own
// children is visited and every child (by value) is already present. When
// equal subtrees sit under different parents, a parent can be skipped and
// the root never reached (the reference then raises KeyError).
bool reference_root_reachable(const ProcessTree& tree) {
  struct Flat {
    std::size_t key = 0;
    std::optional<std::size_t> parent;
    std::vector<std::size_t> children;
  };
  std::vector<Flat> nodes;
  std::map<std::pair<std::pair<int, std::string>, std::vector<std::size_t>>, std::size_t> keys;
  std::function<std::size_t(const ProcessTreeNode&, std::optional<std::size_t>)> flatten;
  flatten = [&](const ProcessTreeNode& n, std::optional<std::size_t> parent) {
    const std::size_t id = nodes.size();
    nodes.push_back(Flat{0, parent, {}});
    std::vector<std::size_t> child_keys;
    for (const auto& child : n.children) {
      const auto child_id = flatten(child, id);
      nodes[id].children.push_back(child_id);
      child_keys.push_back(nodes[child_id].key);
    }
    std::string label;
    if (n.op == ProcessTreeOperator::kActivity) label = tree.activities().at(n.activity);
    const auto inserted = keys.emplace(
        std::make_pair(std::make_pair(static_cast<int>(n.op), std::move(label)),
                       std::move(child_keys)),
        keys.size());
    nodes[id].key = inserted.first->second;
    return id;
  };
  flatten(tree.root(), std::nullopt);
  std::vector<std::size_t> visit;
  std::set<std::size_t> seen;
  std::vector<std::size_t> queue{0};
  for (std::size_t head = 0; head < queue.size(); ++head) {
    const auto id = queue[head];
    if (seen.insert(nodes[id].key).second && nodes[id].children.empty())
      visit.push_back(id);
    for (const auto child : nodes[id].children) queue.push_back(child);
  }
  std::set<std::size_t> present;
  for (const auto id : visit) present.insert(nodes[id].key);
  for (std::size_t i = 0; i < visit.size(); ++i) {
    const auto parent = nodes[visit[i]].parent;
    if (!parent || present.contains(nodes[*parent].key)) continue;
    bool ready = true;
    for (const auto child : nodes[*parent].children)
      if (!present.contains(nodes[child].key)) {
        ready = false;
        break;
      }
    if (ready) {
      visit.push_back(*parent);
      present.insert(nodes[*parent].key);
    }
  }
  return present.contains(nodes[0].key);
}

class ReferenceExtensive {
 public:
  ReferenceExtensive(const ProcessTree& tree, std::size_t min_length,
                     std::optional<std::size_t> max_length,
                     std::optional<std::size_t> max_loops,
                     std::size_t max_traces, std::size_t budget)
      : tree_(tree), min_length_(static_cast<long long>(min_length)),
        max_traces_(max_traces), budget_{budget} {
    root_min_ = min_len(tree.root());
    root_max_ = max_len(tree.root());
    max_length_ = max_length ? static_cast<long long>(*max_length) : root_min_;
    max_loops_ = max_loops ? *max_loops : static_cast<std::size_t>(max_length_ / 2);
  }

  std::set<Word> run() { return play(tree_.root()); }
  [[nodiscard]] bool truncated() const { return budget_.exhausted || capped_; }

 private:
  long long min_len(const ProcessTreeNode& n) {
    if (const auto it = min_.find(&n); it != min_.end()) return it->second;
    long long value = 0;
    switch (n.op) {
      case ProcessTreeOperator::kTau: value = 0; break;
      case ProcessTreeOperator::kActivity: value = 1; break;
      case ProcessTreeOperator::kExclusive:
      case ProcessTreeOperator::kInclusiveChoice:
        value = std::numeric_limits<long long>::max();
        for (const auto& child : n.children) value = std::min(value, min_len(child));
        break;
      case ProcessTreeOperator::kSequence:
      case ProcessTreeOperator::kParallel:
      case ProcessTreeOperator::kInterleaving:
        for (const auto& child : n.children) value += min_len(child);
        break;
      case ProcessTreeOperator::kLoop:
        value = min_len(n.children.front());
        for (std::size_t i = 1; i < n.children.size(); ++i) (void)min_len(n.children[i]);
        break;
    }
    min_.emplace(&n, value);
    return value;
  }

  ExtensiveLength max_len(const ProcessTreeNode& n) {
    if (const auto it = max_.find(&n); it != max_.end()) return it->second;
    ExtensiveLength value;
    switch (n.op) {
      case ProcessTreeOperator::kTau: break;
      case ProcessTreeOperator::kActivity: value.finite = 1; break;
      case ProcessTreeOperator::kExclusive:
        for (std::size_t i = 0; i < n.children.size(); ++i) {
          const auto child = max_len(n.children[i]);
          if (i == 0 || value < child) value = child;
        }
        break;
      case ProcessTreeOperator::kSequence:
      case ProcessTreeOperator::kParallel:
      case ProcessTreeOperator::kInclusiveChoice:
      case ProcessTreeOperator::kInterleaving:
        for (const auto& child : n.children) value += max_len(child);
        break;
      case ProcessTreeOperator::kLoop:
        value.loops = 1;
        for (const auto& child : n.children) value += max_len(child);
        break;
    }
    max_.emplace(&n, value);
    return value;
  }

  // Longest word the node keeps, from the first condition.
  long long upper(const ProcessTreeNode& n) {
    return max_length_ - (root_min_ - min_len(n));
  }

  std::set<Word> keep(const ProcessTreeNode& n, std::set<Word> words) {
    const long long bound = upper(n);
    ExtensiveLength rest = root_max_;
    const auto own = max_len(n);
    rest.loops -= own.loops;
    rest.finite -= own.finite;
    for (auto it = words.begin(); it != words.end();) {
      const auto size = static_cast<long long>(it->size());
      const bool short_enough = size <= bound;
      const bool long_enough = rest.loops > 0 || size + rest.finite >= min_length_;
      if (short_enough && long_enough) ++it;
      else it = words.erase(it);
    }
    return words;
  }

  void add(std::set<Word>& out, Word word) {
    if (budget_.take()) out.insert(std::move(word));
  }

  static void interleavings(const Word& a, const Word& b, std::size_t i,
                            std::size_t j, Word& current,
                            const std::function<void(const Word&)>& emit) {
    if (i == a.size() && j == b.size()) {
      emit(current);
      return;
    }
    if (i < a.size()) {
      current.push_back(a[i]);
      interleavings(a, b, i + 1, j, current, emit);
      current.pop_back();
    }
    if (j < b.size()) {
      current.push_back(b[j]);
      interleavings(a, b, i, j + 1, current, emit);
      current.pop_back();
    }
  }

  std::set<Word> play(const ProcessTreeNode& n) {
    switch (n.op) {
      case ProcessTreeOperator::kTau:
        return {Word{}};
      case ProcessTreeOperator::kActivity:
        return keep(n, {Word{tree_.activities().at(n.activity)}});
      case ProcessTreeOperator::kExclusive: {
        std::set<Word> out;
        for (const auto& child : n.children) out.merge(play(child));
        return out;
      }
      case ProcessTreeOperator::kSequence: {
        // Words only grow, so a prefix above the node's bound is dropped
        // early; the result is unchanged.
        const long long bound = upper(n);
        std::set<Word> current{Word{}};
        for (const auto& child : n.children) {
          const auto words = play(child);
          std::set<Word> next;
          for (const auto& prefix : current)
            for (const auto& word : words) {
              if (static_cast<long long>(prefix.size() + word.size()) > bound) continue;
              if (budget_.exhausted) break;
              Word joined = prefix;
              joined.insert(joined.end(), word.begin(), word.end());
              add(next, std::move(joined));
            }
          current = std::move(next);
        }
        return keep(n, std::move(current));
      }
      case ProcessTreeOperator::kParallel: {
        const long long bound = upper(n);
        std::set<Word> current{Word{}};
        for (const auto& child : n.children) {
          const auto words = play(child);
          std::set<Word> next;
          for (const auto& left : current)
            for (const auto& right : words) {
              if (static_cast<long long>(left.size() + right.size()) > bound) continue;
              if (budget_.exhausted) break;
              Word scratch;
              interleavings(left, right, 0, 0, scratch,
                            [&](const Word& word) { add(next, word); });
            }
          current = std::move(next);
        }
        auto kept = keep(n, std::move(current));
        // The reference stops a parallel cut at max_limit_num_traces words;
        // which words it keeps follows its set order. Here the first ones
        // in word order are kept and the result is marked truncated.
        if (kept.size() > max_traces_) {
          capped_ = true;
          kept.erase(std::next(kept.begin(), static_cast<std::ptrdiff_t>(max_traces_)), kept.end());
        }
        return kept;
      }
      case ProcessTreeOperator::kLoop: {
        const long long bound = upper(n);
        const auto body = play(n.children[0]);
        const auto redo = play(n.children[1]);
        for (std::size_t i = 2; i < n.children.size(); ++i) {
          // The reference builds every child but reads only the first two.
          if (n.children[i].op != ProcessTreeOperator::kInclusiveChoice)
            (void)play(n.children[i]);
        }
        std::set<Word> out;
        for (const auto& word : body)
          if (static_cast<long long>(word.size()) <= bound) add(out, word);
        std::set<Word> current = out;
        for (std::size_t round = 0; round < max_loops_ && !current.empty() && !budget_.exhausted; ++round) {
          std::set<Word> next;
          for (const auto& prefix : current)
            for (const auto& r : redo)
              for (const auto& d : body) {
                if (static_cast<long long>(prefix.size() + r.size() + d.size()) > bound) continue;
                if (budget_.exhausted) break;
                Word joined = prefix;
                joined.insert(joined.end(), r.begin(), r.end());
                joined.insert(joined.end(), d.begin(), d.end());
                add(next, std::move(joined));
              }
          // A round that adds no new word leaves the later rounds unchanged.
          bool grew = false;
          for (const auto& word : next) grew = out.insert(word).second || grew;
          if (!grew) break;
          current = std::move(next);
        }
        return keep(n, std::move(out));
      }
      case ProcessTreeOperator::kInclusiveChoice:
      case ProcessTreeOperator::kInterleaving:
        break;
    }
    throw std::invalid_argument("process-tree EXTENSIVE playout: unsupported operator");
  }

  const ProcessTree& tree_;
  long long min_length_ = 1;
  long long max_length_ = 0;
  std::size_t max_loops_ = 0;
  std::size_t max_traces_ = 100000;
  long long root_min_ = 0;
  ExtensiveLength root_max_;
  ExtensiveBudget budget_;
  bool capped_ = false;
  std::unordered_map<const ProcessTreeNode*, long long> min_;
  std::unordered_map<const ProcessTreeNode*, ExtensiveLength> max_;
};

// A sampled element: an activity index, or kTauElement for a silent leaf.
// Silent leaves are kept while a trace is built because the reference
// shuffles them together with the visible events of a parallel block.
using Element = std::int64_t;
constexpr Element kTauElement = -1;
using Elements = std::vector<Element>;

// Generation stops with an error once one trace holds this many elements
// (visible and silent); the reference itself has no bound.
constexpr std::size_t kElementBudget = 10000000;

struct SampleBounds {
  bool cap_loops = false;
  std::size_t max_loops = 0;
  std::size_t elements = 0;
};

void count_element(SampleBounds& bounds) {
  if (++bounds.elements > kElementBudget)
    throw std::invalid_argument(
        "process-tree playout: a sampled trace exceeds the ten-million element safety bound");
}

// TOPBOTTOM (exact under random.seed): leaves give their label (tau
// included); a sequence concatenates its children; an exclusive choice
// takes children[randbelow(n)]; a loop generates children[0], then while
// random() < 0.5 children[1] followed by children[0] (further children are
// ignored); an interleaving shuffles its child list in place (the shuffled
// order persists into later traces) and concatenates; a parallel block
// generates every child, lists child i once per element of its sequence,
// shuffles that list and takes the children's elements in the listed order.
void topbottom_node(ProcessTreeNode& n, rng::PythonRandom& random, Elements& out,
                    SampleBounds& bounds) {
  switch (n.op) {
    case ProcessTreeOperator::kTau:
      count_element(bounds);
      out.push_back(kTauElement);
      return;
    case ProcessTreeOperator::kActivity:
      count_element(bounds);
      out.push_back(static_cast<Element>(n.activity));
      return;
    case ProcessTreeOperator::kSequence:
      for (auto& child : n.children) topbottom_node(child, random, out, bounds);
      return;
    case ProcessTreeOperator::kExclusive:
      topbottom_node(n.children[random.choice_index(n.children.size())], random, out, bounds);
      return;
    case ProcessTreeOperator::kLoop: {
      topbottom_node(n.children[0], random, out, bounds);
      std::size_t repeats = 0;
      while (!(bounds.cap_loops && repeats >= bounds.max_loops) && random.random() < 0.5) {
        topbottom_node(n.children[1], random, out, bounds);
        topbottom_node(n.children[0], random, out, bounds);
        ++repeats;
      }
      return;
    }
    case ProcessTreeOperator::kInterleaving:
      random.shuffle(n.children);
      for (auto& child : n.children) topbottom_node(child, random, out, bounds);
      return;
    case ProcessTreeOperator::kParallel: {
      std::vector<Elements> parts(n.children.size());
      std::vector<std::size_t> order;
      for (std::size_t i = 0; i < n.children.size(); ++i) {
        topbottom_node(n.children[i], random, parts[i], bounds);
        order.insert(order.end(), parts[i].size(), i);
      }
      random.shuffle(order);
      std::vector<std::size_t> next(parts.size(), 0);
      for (const auto i : order) out.push_back(parts[i][next[i]++]);
      return;
    }
    case ProcessTreeOperator::kInclusiveChoice:
      throw std::invalid_argument(
          "TOPBOTTOM process-tree playout does not support inclusive choice (the reference raises "
          "TypeError)");
  }
}

// BASIC_PLAYOUT as a stochastic process. The reference keeps the enabled
// vertices in a Python set and picks one per step; set order follows the
// string hash, so only the distribution is reproducible. Every step picks
// one enabled vertex uniformly. Executing a leaf emits it; a sequence enables
// its first child, an exclusive choice one uniformly drawn child, a parallel
// block all children, an inclusive choice each child with probability 1/2
// (none: the block closes empty), an interleaving its first child after
// shuffling the child order, a loop its body. A closed child enables the
// next one (sequence, interleaving), closes an exclusive choice, and closes
// a parallel or inclusive block once all its enabled children closed. A
// closed loop body redoes (children[1], then the body again) or exits with
// probability 1/2 each; the exit is children[2] when present, otherwise one
// silent step.
class BasicPlayout {
 public:
  BasicPlayout(const ProcessTreeNode& root, rng::PythonRandom& random, SampleBounds& bounds)
      : root_(root), random_(random), bounds_(bounds) {
    index(root, nullptr, 0);
  }

  Elements run() {
    Elements out;
    enabled_.clear();
    enabled_.push_back({&root_, false});
    done_ = false;
    while (!done_) {
      if (enabled_.empty())
        throw std::logic_error("process-tree basic playout lost its enabled vertices");
      const auto at = static_cast<std::size_t>(random_.randbelow(enabled_.size()));
      const Vertex vertex = enabled_[at];
      enabled_.erase(enabled_.begin() + static_cast<std::ptrdiff_t>(at));
      count_element(bounds_);
      if (vertex.loop_exit) {
        close(*vertex.node);
        continue;
      }
      execute(*vertex.node, out);
    }
    return out;
  }

 private:
  struct Vertex {
    const ProcessTreeNode* node = nullptr;
    bool loop_exit = false;  // the implicit silent exit of a two-child loop
  };
  struct Place {
    const ProcessTreeNode* parent = nullptr;
    std::size_t child = 0;
  };
  struct State {
    std::size_t pending = 0;
    std::size_t position = 0;
    std::size_t repeats = 0;
    std::vector<std::size_t> order;
  };

  void index(const ProcessTreeNode& node, const ProcessTreeNode* parent, std::size_t child) {
    places_[&node] = {parent, child};
    for (std::size_t i = 0; i < node.children.size(); ++i) index(node.children[i], &node, i);
  }

  void enable(const ProcessTreeNode& node) { enabled_.push_back({&node, false}); }

  void execute(const ProcessTreeNode& node, Elements& out) {
    switch (node.op) {
      case ProcessTreeOperator::kTau:
        close(node);
        return;
      case ProcessTreeOperator::kActivity:
        out.push_back(static_cast<Element>(node.activity));
        close(node);
        return;
      default:
        break;
    }
    auto& state = states_[&node];
    state = State{};
    switch (node.op) {
      case ProcessTreeOperator::kSequence:
      case ProcessTreeOperator::kLoop:
        enable(node.children[0]);
        return;
      case ProcessTreeOperator::kExclusive:
        enable(node.children[random_.choice_index(node.children.size())]);
        return;
      case ProcessTreeOperator::kParallel:
        state.pending = node.children.size();
        for (const auto& child : node.children) enable(child);
        return;
      case ProcessTreeOperator::kInclusiveChoice: {
        std::vector<const ProcessTreeNode*> selected;
        for (const auto& child : node.children)
          if (random_.random() < 0.5) selected.push_back(&child);
        if (selected.empty()) {
          close(node);
          return;
        }
        state.pending = selected.size();
        for (const auto* child : selected) enable(*child);
        return;
      }
      case ProcessTreeOperator::kInterleaving:
        state.order.resize(node.children.size());
        std::iota(state.order.begin(), state.order.end(), 0);
        random_.shuffle(state.order);
        enable(node.children[state.order[0]]);
        return;
      default:
        return;
    }
  }

  void close(const ProcessTreeNode& node) {
    if (&node == &root_) {
      done_ = true;
      return;
    }
    const auto place = places_.at(&node);
    const auto& parent = *place.parent;
    auto& state = states_[&parent];
    switch (parent.op) {
      case ProcessTreeOperator::kSequence:
        if (place.child + 1 < parent.children.size())
          enable(parent.children[place.child + 1]);
        else
          close(parent);
        return;
      case ProcessTreeOperator::kInterleaving:
        if (++state.position < parent.children.size())
          enable(parent.children[state.order[state.position]]);
        else
          close(parent);
        return;
      case ProcessTreeOperator::kExclusive:
        close(parent);
        return;
      case ProcessTreeOperator::kParallel:
      case ProcessTreeOperator::kInclusiveChoice:
        if (--state.pending == 0) close(parent);
        return;
      case ProcessTreeOperator::kLoop:
        if (place.child == 0) {
          const bool capped = bounds_.cap_loops && state.repeats >= bounds_.max_loops;
          if (!capped && random_.randint(1, 2) == 1) {
            ++state.repeats;
            enable(parent.children[1]);
          } else if (parent.children.size() >= 3) {
            enable(parent.children[2]);
          } else {
            enabled_.push_back({&parent, true});
          }
        } else if (place.child == 1) {
          enable(parent.children[0]);
        } else {
          close(parent);
        }
        return;
      default:
        return;
    }
  }

  const ProcessTreeNode& root_;
  rng::PythonRandom& random_;
  SampleBounds& bounds_;
  std::unordered_map<const ProcessTreeNode*, Place> places_;
  std::unordered_map<const ProcessTreeNode*, State> states_;
  std::vector<Vertex> enabled_;
  bool done_ = false;
};

Word visible_word(const Elements& elements, const ProcessTree& tree) {
  Word word;
  for (const auto element : elements)
    if (element != kTauElement) word.push_back(tree.activities()[static_cast<std::size_t>(element)]);
  return word;
}

// The reference envelopes: TOPBOTTOM and EXTENSIVE logs carry the activity
// only; BASIC_PLAYOUT (which plays out the converted net) names its cases
// "0", "1", ... and stamps the events one second apart from epoch
// 10,000,000, counting on across cases. add_envelope replaces either with
// Minera's synthetic case_N ids and configured timestamps.
EventLog words_to_log(const Words& words, const ProcessTreePlayoutOptions& o) {
  const bool basic = o.variant == ProcessTreePlayoutVariant::kBasicPlayout;
  EventLog log;
  log.traces.reserve(words.size());
  double reference_clock = 10000000.0;
  for (std::size_t i = 0; i < words.size(); ++i) {
    Trace t;
    if (o.add_envelope || basic) {
      t.case_id = o.add_envelope ? "case_" + std::to_string(i + 1) : std::to_string(i);
      t.case_id_state = FirstClassAttributeState::kPresent;
    }
    double now = o.start + static_cast<double>(i) * o.case_interval;
    for (std::size_t j = 0; j < words[i].size(); ++j) {
      Event e; e.case_id = t.case_id; e.activity = words[i][j];
      e.activity_state = FirstClassAttributeState::kPresent;
      if (o.add_envelope) {
        e.timestamp = format_timestamp(now);
        e.timestamp_state = FirstClassAttributeState::kPresent;
      } else if (basic) {
        e.timestamp = format_timestamp(reference_clock);
        e.timestamp_state = FirstClassAttributeState::kPresent;
        reference_clock += 1.0;
      }
      e.input_order = j;
      t.events.push_back(std::move(e)); now += o.event_interval;
    }
    log.traces.push_back(std::move(t));
  }
  return log;
}

}  // namespace

ProcessTreePlayoutResult process_tree_playout_result(
    const ProcessTree& tree, ProcessTreePlayoutOptions options) {
  validate_node(tree.root(), tree);
  if (options.max_length.value_or(0) > 10000 || options.max_loop_occurrences.value_or(0) > 10000)
    throw std::invalid_argument(
        "process-tree playout length and loop bounds must not exceed 10000");
  if (options.return_set_strings &&
      options.variant != ProcessTreePlayoutVariant::kExtensive)
    throw std::invalid_argument(
        "process-tree return_set_strings requires extensive playout");
  if (!std::isfinite(options.start) || !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval))
    throw std::invalid_argument("process-tree playout timestamps and intervals must be finite");
  if (options.variant == ProcessTreePlayoutVariant::kExtensive) {
    if (node_has_operator(tree.root(), ProcessTreeOperator::kInterleaving))
      throw std::invalid_argument(
          "EXTENSIVE process-tree playout does not support interleaving (the reference raises "
          "TypeError)");
    if (inclusive_choice_read(tree.root()))
      throw std::invalid_argument(
          "EXTENSIVE process-tree playout does not support inclusive choice (the reference raises "
          "KeyError)");
    if (!reference_root_reachable(tree))
      throw std::invalid_argument(
          "EXTENSIVE process-tree playout: equal subtrees under different parents leave the "
          "reference's bottom-up walk short of the root (the reference raises KeyError)");
    if (has_single_child_block(tree.root()))
      throw std::invalid_argument(
          "EXTENSIVE process-tree playout does not support a sequence or parallel cut with one "
          "child (the reference raises IndexError)");
    ReferenceExtensive engine(tree, options.min_length, options.max_length,
                              options.max_loop_occurrences, options.max_traces, 250000);
    const auto unique = engine.run();
    Words words(unique.begin(), unique.end());
    std::stable_sort(words.begin(), words.end(), [](const Word& a, const Word& b) {
      return a.size() < b.size();
    });
    ProcessTreePlayoutResult result;
    result.log = words_to_log(words, options);
    if (options.return_set_strings) result.trace_strings = std::move(words);
    result.truncated = engine.truncated();
    return result;
  }
  if (options.variant == ProcessTreePlayoutVariant::kTopBottom &&
      node_has_operator(tree.root(), ProcessTreeOperator::kInclusiveChoice))
    throw std::invalid_argument(
        "TOPBOTTOM process-tree playout does not support inclusive choice (the reference raises "
        "TypeError)");
  Words words;
  rng::PythonRandom random(options.seed);
  SampleBounds bounds;
  bounds.cap_loops = options.cap_sampled_loops;
  bounds.max_loops = options.max_loop_occurrences.value_or(3);
  const std::size_t limit = std::min(options.max_steps, options.max_length.value_or(options.max_steps));
  words.reserve(options.traces);
  // TOPBOTTOM shuffles interleaving children in place, so it runs on a copy
  // whose child order evolves across traces as the reference's tree does.
  ProcessTreeNode working = tree.root();
  std::optional<BasicPlayout> basic;
  if (options.variant == ProcessTreePlayoutVariant::kBasicPlayout)
    basic.emplace(tree.root(), random, bounds);
  for (std::size_t i = 0; i < options.traces; ++i) {
    bounds.elements = 0;
    Elements elements;
    if (basic) {
      elements = basic->run();
    } else {
      topbottom_node(working, random, elements, bounds);
    }
    auto word = visible_word(elements, tree);
    if (word.size() > limit)
      throw std::invalid_argument(
          "process-tree playout: a sampled trace has " + std::to_string(word.size()) +
          " visible events, above the max_steps / max_length safety bound of " +
          std::to_string(limit) + " (the reference has no bound; raise it to keep every trace)");
    words.push_back(std::move(word));
  }
  ProcessTreePlayoutResult result;
  result.log = words_to_log(words, options);
  return result;
}

EventLog process_tree_playout(const ProcessTree& tree,
                              ProcessTreePlayoutOptions options) {
  return process_tree_playout_result(tree, std::move(options)).log;
}

}  // namespace minera
