#include "minera/process_tree_playout.hpp"

#include <algorithm>
#include <functional>
#include <cmath>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    case ProcessTreeOperator::kParallel:
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
        Words next;
        for (const auto& prefix : out) {
          if (budget.remaining == 0) break;
          for (const auto& suffix : child) {
            if (budget.remaining == 0) break;
            std::function<void(std::size_t, std::size_t, Word&)> interleave;
            Word merged;
            interleave = [&](std::size_t i, std::size_t j, Word&) {
              if (budget.remaining == 0) return;
              if (i == prefix.size() && j == suffix.size()) {
                append_unique(next, merged, max_length, budget);
                return;
              }
              if (i < prefix.size()) {
                merged.push_back(prefix[i]); interleave(i + 1, j, merged); merged.pop_back();
              }
              if (j < suffix.size() && budget.remaining != 0) {
                merged.push_back(suffix[j]); interleave(i, j + 1, merged); merged.pop_back();
              }
            };
            interleave(0, 0, merged);
          }
        }
        out = std::move(next);
      }
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

bool sample_node(const ProcessTreeNode& n, const ProcessTree& tree,
                 std::size_t max_length, std::size_t max_loops,
                 std::mt19937_64& rng, Word& out, bool top_bottom) {
  auto choose = [&](std::size_t size) {
    return std::uniform_int_distribution<std::size_t>(0, size - 1)(rng);
  };
  switch (n.op) {
    case ProcessTreeOperator::kTau: return true;
    case ProcessTreeOperator::kActivity:
      if (max_length == 0) return false;
      out.push_back(tree.activities()[n.activity]); return true;
    case ProcessTreeOperator::kExclusive: {
      if (n.children.empty()) return false;
      const auto start = choose(n.children.size());
      for (std::size_t k = 0; k < n.children.size(); ++k) {
        Word candidate;
        if (sample_node(n.children[(start + k) % n.children.size()], tree,
                        max_length, max_loops, rng, candidate, top_bottom) &&
            candidate.size() <= max_length) { out = std::move(candidate); return true; }
      }
      return false;
    }
    case ProcessTreeOperator::kSequence:
      for (const auto& c : n.children) {
        Word part;
        if (!sample_node(c, tree, max_length - std::min(max_length, out.size()),
                         max_loops, rng, part, top_bottom)) return false;
        out.insert(out.end(), part.begin(), part.end());
      }
      return out.size() <= max_length;
    case ProcessTreeOperator::kParallel: {
      std::vector<Word> parts;
      std::size_t total = 0;
      for (const auto& c : n.children) {
        Word p;
        if (!sample_node(c, tree, max_length - std::min(max_length, total),
                         max_loops, rng, p, top_bottom)) return false;
        total += p.size(); parts.push_back(std::move(p));
      }
      while (!parts.empty()) {
        std::vector<std::size_t> ready;
        for (std::size_t i = 0; i < parts.size(); ++i) if (!parts[i].empty()) ready.push_back(i);
        if (ready.empty()) break;
        const auto i = ready[choose(ready.size())];
        out.push_back(parts[i].front()); parts[i].erase(parts[i].begin());
      }
      return out.size() <= max_length;
    }
    case ProcessTreeOperator::kLoop: {
      if (top_bottom) {
        // The top-bottom variant follows the reference's geometric loop
        // choice: after each body there is a 50% chance of taking a redo
        // branch, subject to the caller's safety bound.
        std::size_t repeats = 0;
        while (true) {
          Word body;
          if (!sample_node(n.children[0], tree,
                           max_length - std::min(max_length, out.size()),
                           max_loops, rng, body, top_bottom))
            return false;
          out.insert(out.end(), body.begin(), body.end());
          if (repeats >= max_loops ||
              std::uniform_int_distribution<int>(0, 1)(rng) == 0)
            break;
          const auto r = 1 + choose(n.children.size() - 1);
          Word redo;
          if (!sample_node(n.children[r], tree,
                           max_length - std::min(max_length, out.size()),
                           max_loops, rng, redo, top_bottom))
            return false;
          out.insert(out.end(), redo.begin(), redo.end());
          ++repeats;
        }
        return out.size() <= max_length;
      }
      const auto repeats = max_loops == 0 ? 0 : std::uniform_int_distribution<std::size_t>(0, max_loops)(rng);
      for (std::size_t i = 0; i <= repeats; ++i) {
        Word body;
        if (!sample_node(n.children[0], tree,
                         max_length - std::min(max_length, out.size()),
                         max_loops, rng, body, top_bottom))
          return false;
        out.insert(out.end(), body.begin(), body.end());
        if (i < repeats) {
          const auto r = 1 + choose(n.children.size() - 1);
          Word redo;
          if (!sample_node(n.children[r], tree,
                           max_length - std::min(max_length, out.size()),
                           max_loops, rng, redo, top_bottom))
            return false;
          out.insert(out.end(), redo.begin(), redo.end());
        }
      }
      return out.size() <= max_length;
    }
  }
  return false;
}

EventLog words_to_log(const Words& words, const ProcessTreePlayoutOptions& o) {
  EventLog log;
  log.traces.reserve(words.size());
  for (std::size_t i = 0; i < words.size(); ++i) {
    Trace t; t.case_id = "case_" + std::to_string(i + 1);
    double now = o.start + static_cast<double>(i) * o.case_interval;
    for (std::size_t j = 0; j < words[i].size(); ++j) {
      Event e; e.case_id = t.case_id; e.activity = words[i][j];
      e.timestamp = format_timestamp(now); e.input_order = j;
      t.events.push_back(std::move(e)); now += o.event_interval;
    }
    log.traces.push_back(std::move(t));
  }
  return log;
}

}  // namespace

EventLog process_tree_playout(const ProcessTree& tree, ProcessTreePlayoutOptions options) {
  validate_node(tree.root(), tree);
  if (options.max_length > 10000 || options.max_loop_occurrences > 10000)
    throw std::invalid_argument(
        "process-tree playout length and loop bounds must not exceed 10000");
  if (options.min_length > options.max_length)
    throw std::invalid_argument("process-tree playout min_length exceeds max_length");
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
    if (budget.exhausted) {
      // A safety budget may cut off a composition after its intermediate
      // candidates have been consumed. Never return a silently partial word
      // (or an empty language) in that case: retain one deterministic valid
      // execution instead and let the caller's trace cap apply normally.
      words.clear();
      if (const auto fallback = first_word(tree.root(), tree);
          fallback && fallback->size() >= options.min_length &&
          fallback->size() <= options.max_length) {
        words.push_back(*fallback);
      } else {
        throw std::runtime_error(
            "process-tree extensive playout generation budget exhausted "
            "before a trace within the requested length bounds");
      }
    }
    std::set<Word> unique(words.begin(), words.end());
    words.assign(unique.begin(), unique.end());
    std::stable_sort(words.begin(), words.end(), [](const Word& a, const Word& b) {
      return a.size() == b.size() ? a < b : a.size() < b.size();
    });
    words.erase(std::remove_if(words.begin(), words.end(), [&](const Word& w) {
      return w.size() < options.min_length;
    }), words.end());
    if (words.size() > options.max_traces) words.resize(options.max_traces);
    return words_to_log(words, options);
  }
  Words words;
  std::mt19937_64 rng(options.seed);
  const std::size_t limit = std::min(options.max_steps, options.max_length);
  words.reserve(options.traces);
  for (std::size_t i = 0; i < options.traces; ++i) {
    Word w;
    if (sample_node(tree.root(), tree, limit, options.max_loop_occurrences,
                    rng, w,
                    options.variant == ProcessTreePlayoutVariant::kTopBottom))
      words.push_back(std::move(w));
  }
  return words_to_log(words, options);
}

}  // namespace minera
