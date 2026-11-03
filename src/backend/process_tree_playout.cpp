#include "minera/process_tree_playout.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
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

// Enumeration is deliberately budgeted before the final cap is applied. A
// parallel cut can have factorially many interleavings, so materializing the
// entire language and truncating afterward is not a safe implementation.
struct EnumerationBudget {
  std::size_t remaining = 250000;
  bool exhausted = false;
};

bool contains_operator(const ProcessTreeNode& n, ProcessTreeOperator op) {
  if (n.op == op) return true;
  for (const auto& child : n.children)
    if (contains_operator(child, op)) return true;
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

void append_unique(Words& out, Word word, std::size_t max_length,
                   EnumerationBudget& budget) {
  if (word.size() <= max_length && budget.remaining != 0) {
    --budget.remaining;
    out.push_back(std::move(word));
    if (budget.remaining == 0) budget.exhausted = true;
  } else if (budget.remaining == 0) {
    budget.exhausted = true;
  }
}

Words interleave_words(const Words& prefixes, const Words& suffixes,
                       std::size_t max_length, EnumerationBudget& budget) {
  Words out;
  for (const auto& prefix : prefixes) {
    if (budget.remaining == 0) break;
    for (const auto& suffix : suffixes) {
      if (budget.remaining == 0) break;
      std::function<void(std::size_t, std::size_t, Word&)> interleave;
      Word merged;
      interleave = [&](std::size_t i, std::size_t j, Word&) {
        if (budget.remaining == 0) return;
        if (i == prefix.size() && j == suffix.size()) {
          append_unique(out, merged, max_length, budget);
          return;
        }
        if (i < prefix.size()) {
          merged.push_back(prefix[i]);
          interleave(i + 1, j, merged);
          merged.pop_back();
        }
        if (j < suffix.size() && budget.remaining != 0) {
          merged.push_back(suffix[j]);
          interleave(i, j + 1, merged);
          merged.pop_back();
        }
      };
      interleave(0, 0, merged);
    }
  }
  return out;
}

Words combine_parallel(const Words& current, const Words& child,
                       std::size_t max_length, EnumerationBudget& budget) {
  return interleave_words(current, child, max_length, budget);
}

std::optional<Word> first_word(const ProcessTreeNode& n,
                               const ProcessTree& tree) {
  switch (n.op) {
    case ProcessTreeOperator::kTau:
      return Word{};
    case ProcessTreeOperator::kActivity:
      if (n.activity >= tree.activities().size()) return std::nullopt;
      return Word{tree.activities()[n.activity]};
    case ProcessTreeOperator::kExclusive:
      if (n.children.empty()) return std::nullopt;
      return first_word(n.children.front(), tree);
    case ProcessTreeOperator::kInclusiveChoice:
      if (n.children.empty()) return std::nullopt;
      return first_word(n.children.front(), tree);
    case ProcessTreeOperator::kSequence: {
      Word result;
      for (const auto& child : n.children) {
        const auto word = first_word(child, tree);
        if (!word) return std::nullopt;
        result.insert(result.end(), word->begin(), word->end());
      }
      return result;
    }
    case ProcessTreeOperator::kParallel: {
      Word result;
      for (const auto& child : n.children) {
        const auto word = first_word(child, tree);
        if (!word) return std::nullopt;
        result.insert(result.end(), word->begin(), word->end());
      }
      return result;
    }
    case ProcessTreeOperator::kInterleaving: {
      Word result;
      for (const auto& child : n.children) {
        const auto word = first_word(child, tree);
        if (!word) return std::nullopt;
        result.insert(result.end(), word->begin(), word->end());
      }
      return result;
    }
    case ProcessTreeOperator::kLoop:
      if (n.children.empty()) return std::nullopt;
      return first_word(n.children.front(), tree);
  }
  return std::nullopt;
}

Words enumerate(const ProcessTreeNode& n, const ProcessTree& tree,
                std::size_t max_length, std::size_t max_loops,
                EnumerationBudget& budget) {
  if (budget.remaining == 0) return {};
  switch (n.op) {
    case ProcessTreeOperator::kTau: {
      Words out;
      append_unique(out, {}, max_length, budget);
      return out;
    }
    case ProcessTreeOperator::kActivity: {
      Words out;
      if (n.activity < tree.activities().size() && max_length > 0)
        append_unique(out, Word{tree.activities()[n.activity]}, max_length,
                      budget);
      return out;
    }
    case ProcessTreeOperator::kExclusive: {
      Words out;
      for (const auto& c : n.children) {
        auto child = enumerate(c, tree, max_length, max_loops, budget);
        out.insert(out.end(), std::make_move_iterator(child.begin()),
                   std::make_move_iterator(child.end()));
      }
      return out;
    }
    case ProcessTreeOperator::kInclusiveChoice: {
      // An inclusive choice selects every non-empty subset of its children;
      // selected children execute concurrently, just like a parallel cut.
      Words out;
      std::function<void(std::size_t, Words, std::size_t)> visit;
      visit = [&](std::size_t index, Words current, std::size_t selected) {
        if (budget.remaining == 0) return;
        if (index == n.children.size()) {
          if (selected != 0)
            out.insert(out.end(), std::make_move_iterator(current.begin()),
                       std::make_move_iterator(current.end()));
          return;
        }
        visit(index + 1, current, selected);
        Words child = enumerate(n.children[index], tree, max_length,
                               max_loops, budget);
        Words next = combine_parallel(current, child, max_length, budget);
        visit(index + 1, std::move(next), selected + 1);
      };
      visit(0, Words{Word{}}, 0);
      return out;
    }
    case ProcessTreeOperator::kSequence: {
      Words out{Word{}};
      for (const auto& c : n.children) {
        Words child = enumerate(c, tree, max_length, max_loops, budget);
        Words next;
        for (const auto& a : out)
          for (const auto& b : child) {
            Word w = a;
            w.insert(w.end(), b.begin(), b.end());
            append_unique(next, std::move(w), max_length, budget);
          }
        out = std::move(next);
      }
      return out;
    }
    case ProcessTreeOperator::kParallel: {
      Words out{Word{}};
      for (const auto& c : n.children) {
        Words child = enumerate(c, tree, max_length, max_loops, budget);
        out = combine_parallel(out, child, max_length, budget);
      }
      return out;
    }
    case ProcessTreeOperator::kInterleaving: {
      std::vector<Words> children;
      children.reserve(n.children.size());
      for (const auto& child : n.children)
        children.push_back(enumerate(child, tree, max_length, max_loops, budget));
      std::vector<std::size_t> order(n.children.size());
      std::iota(order.begin(), order.end(), 0);
      Words out;
      do {
        Words current{Word{}};
        for (const auto index : order) {
          Words next;
          for (const auto& prefix : current) {
            for (const auto& suffix : children[index]) {
              Word word = prefix;
              word.insert(word.end(), suffix.begin(), suffix.end());
              append_unique(next, std::move(word), max_length, budget);
            }
          }
          current = std::move(next);
        }
        out.insert(out.end(), std::make_move_iterator(current.begin()),
                   std::make_move_iterator(current.end()));
      } while (budget.remaining != 0 &&
               std::next_permutation(order.begin(), order.end()));
      return out;
    }
    case ProcessTreeOperator::kLoop: {
      Words body = enumerate(n.children[0], tree, max_length, max_loops, budget);
      Words out;
      std::function<void(std::size_t, Word)> expand;
      expand = [&](std::size_t repeats, Word prefix) {
        append_unique(out, prefix, max_length, budget);
        if (repeats >= max_loops) return;
        for (std::size_t r = 1; r < n.children.size(); ++r) {
          Words redo = enumerate(n.children[r], tree, max_length, max_loops,
                                budget);
          for (const auto& rw : redo) {
            for (const auto& bw : body) {
              Word next = prefix;
              next.insert(next.end(), rw.begin(), rw.end());
              next.insert(next.end(), bw.begin(), bw.end());
              // Repeating a silent loop cannot create a new visible word and
              // would otherwise recurse once per configured loop bound.
              if (next == prefix) continue;
              if (next.size() <= max_length)
                expand(repeats + 1, std::move(next));
            }
          }
        }
      };
      for (const auto& bw : body) expand(0, bw);
      return out;
    }
  }
  return {};
}

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
  if (options.max_length > 10000 || options.max_loop_occurrences > 10000)
    throw std::invalid_argument(
        "process-tree playout length and loop bounds must not exceed 10000");
  if (options.min_length > options.max_length)
    throw std::invalid_argument("process-tree playout min_length exceeds max_length");
  if (options.return_set_strings &&
      options.variant != ProcessTreePlayoutVariant::kExtensive)
    throw std::invalid_argument(
        "process-tree return_set_strings requires extensive playout");
  if (!std::isfinite(options.start) || !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval))
    throw std::invalid_argument("process-tree playout timestamps and intervals must be finite");
  if (options.variant == ProcessTreePlayoutVariant::kExtensive) {
    constexpr std::size_t kEnumerationBudget = 250000;
    std::size_t budget_limit = kEnumerationBudget;
    if (options.max_traces < kEnumerationBudget / 64)
      budget_limit = std::max<std::size_t>(10000, options.max_traces * 64);
    EnumerationBudget budget{budget_limit};
    Words words = enumerate(tree.root(), tree, options.max_length,
                            options.max_loop_occurrences, budget);
    const bool truncated = budget.exhausted;
    std::set<Word> unique(words.begin(), words.end());
    words.assign(unique.begin(), unique.end());
    std::stable_sort(words.begin(), words.end(), [](const Word& a, const Word& b) {
      return a.size() == b.size() ? a < b : a.size() < b.size();
    });
    words.erase(std::remove_if(words.begin(), words.end(), [&](const Word& w) {
      return w.size() < options.min_length;
    }), words.end());
    if (words.size() > options.max_traces) words.resize(options.max_traces);
    ProcessTreePlayoutResult result;
    result.log = words_to_log(words, options);
    if (options.return_set_strings) result.trace_strings = std::move(words);
    result.truncated = truncated;
    return result;
  }
  if (options.variant == ProcessTreePlayoutVariant::kTopBottom &&
      contains_operator(tree.root(), ProcessTreeOperator::kInclusiveChoice))
    throw std::invalid_argument(
        "TOPBOTTOM process-tree playout does not support inclusive choice (the reference raises "
        "TypeError)");
  Words words;
  rng::PythonRandom random(options.seed);
  SampleBounds bounds;
  bounds.cap_loops = options.cap_sampled_loops;
  bounds.max_loops = options.max_loop_occurrences;
  const std::size_t limit = std::min(options.max_steps, options.max_length);
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
