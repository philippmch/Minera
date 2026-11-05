#include "minera/process_tree_playout.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/bpmn.hpp"
#include "minera/cancellation.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_net.hpp"
#include "minera/ptml.hpp"
#include "minera/timestamp.hpp"
#include <set>

using namespace minera;

namespace {
ProcessTreeNode seq(std::initializer_list<ProcessTreeNode> c) {
  return ProcessTreeNode{ProcessTreeOperator::kSequence, 0,
                         std::vector<ProcessTreeNode>(c)};
}
ProcessTreeNode xor_node(std::initializer_list<ProcessTreeNode> c) {
  return ProcessTreeNode{ProcessTreeOperator::kExclusive, 0,
                         std::vector<ProcessTreeNode>(c)};
}
ProcessTreeNode or_node(std::initializer_list<ProcessTreeNode> c) {
  return ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice, 0,
                         std::vector<ProcessTreeNode>(c)};
}
std::vector<std::string> labels(const Trace& t) {
  std::vector<std::string> out;
  for (const auto& e : t.events) out.push_back(e.activity);
  return out;
}

void process_tree_basic_variant_contract() {
  ProcessTree tree(
      {"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  ProcessTreePlayoutOptions options;
  options.traces = 16;
  options.seed = 17;

  const auto basic = basic_process_tree_playout(tree, options);
  const auto topbottom = topbottom_process_tree_playout(tree, options);
  assert(basic.traces.size() == options.traces);
  assert(topbottom.traces.size() == options.traces);
  std::vector<std::vector<std::string>> basic_words;
  std::vector<std::vector<std::string>> topbottom_words;
  for (const auto& trace : basic.traces) basic_words.push_back(labels(trace));
  for (const auto& trace : topbottom.traces)
    topbottom_words.push_back(labels(trace));
  assert(basic_words != topbottom_words);
  for (const auto& word : basic_words) {
    assert(word.size() % 2 == 1);
    for (std::size_t index = 0; index < word.size(); ++index)
      assert(word[index] == (index % 2 == 0 ? "a" : "b"));
  }
}

void process_tree_extensive_variant_contract() {
  ProcessTree tree(
      {"a", "b"},
      xor_node({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}));
  ProcessTreePlayoutOptions options;
  options.max_length = 1;
  options.min_length = 1;
  options.max_traces = 4;

  const auto log = extensive_process_tree_playout(tree, options);
  assert(log.traces.size() == 2);
  assert(labels(log.traces[0]) == std::vector<std::string>{"a"});
  assert(labels(log.traces[1]) == std::vector<std::string>{"b"});
}

// Reads the reference's tree notation (->, X, +, *, O, <>, tau, 'label').
class TreeText {
 public:
  explicit TreeText(std::string text) : text_(std::move(text)) {}
  ProcessTree parse() {
    auto root = node();
    return ProcessTree(labels_, std::move(root));
  }

 private:
  void skip() { while (pos_ < text_.size() && text_[pos_] == ' ') ++pos_; }
  bool take(std::string_view token) {
    skip();
    if (text_.compare(pos_, token.size(), token) != 0) return false;
    pos_ += token.size();
    return true;
  }
  ProcessTreeNode node() {
    skip();
    if (take("tau")) return ProcessTreeNode::tau();
    if (take("'")) {
      const auto end = text_.find('\'', pos_);
      const auto label = text_.substr(pos_, end - pos_);
      pos_ = end + 1;
      const auto it = std::find(labels_.begin(), labels_.end(), label);
      if (it != labels_.end()) return ProcessTreeNode::leaf(static_cast<std::size_t>(it - labels_.begin()));
      labels_.push_back(label);
      return ProcessTreeNode::leaf(labels_.size() - 1);
    }
    ProcessTreeNode n;
    if (take("->(")) n.op = ProcessTreeOperator::kSequence;
    else if (take("X(")) n.op = ProcessTreeOperator::kExclusive;
    else if (take("+(")) n.op = ProcessTreeOperator::kParallel;
    else if (take("*(")) n.op = ProcessTreeOperator::kLoop;
    else if (take("O(")) n.op = ProcessTreeOperator::kInclusiveChoice;
    else if (take("<>(")) n.op = ProcessTreeOperator::kInterleaving;
    else throw std::invalid_argument("bad tree text");
    do n.children.push_back(node()); while (take(","));
    if (!take(")")) throw std::invalid_argument("bad tree text");
    return n;
  }
  std::string text_;
  std::size_t pos_ = 0;
  std::vector<std::string> labels_;
};

// Words of the reference EXTENSIVE playout (captured from pm4py 2.7.22),
// space-separated activities per word; "!" for a tree the reference rejects.
void process_tree_extensive_reference_contract() {
  struct Case {
    const char* tree;
    std::optional<std::size_t> max_length, max_loops, min_length;
    std::vector<std::string> words;
  };
  const std::vector<Case> cases = {
      // The default max_trace_length is the shortest word length (1), so the
      // loop gets max_trace_length / 2 = 0 redos.
      {"*('a', tau)", {}, {}, {}, {"a"}},
      {"*('a', tau)", 6, {}, {}, {"a", "a a", "a a a", "a a a a"}},
      {"*('a', tau)", 6, 2, {}, {"a", "a a", "a a a"}},
      {"*('a', tau)", 6, 3, 3, {"a a a", "a a a a"}},
      // Only the first redo child is read.
      {"*('x', 'y', 'z')", 5, {}, {}, {"x", "x y x", "x y x y x"}},
      // Shortest word 0: the default bound drops every non-empty sequence.
      {"->(X(tau, 'b'), X(tau, 'c'))", {}, {}, {}, {}},
      {"->(X(tau, 'b'), X(tau, 'c'))", {}, {}, 0, {""}},
      {"->(X(tau, 'b'), X(tau, 'c'))", 5, {}, {}, {"b", "c", "b c"}},
      // A choice keeps its children's words; the bounds are relative to the
      // root's shortest and longest words.
      {"X('a', ->('b', 'c'))", 1, {}, 1, {"a", "b c"}},
      {"X('a', 'b')", 2, {}, 2, {}},
      {"X('a', ->('b', 'c'))", 2, {}, 2, {"a", "b c"}},
      {"X('a', ->('b', 'c'))", 3, {}, 3, {}},
      {"X(tau, 'b')", 5, {}, 2, {""}},
      {"X('a', *(tau, tau))", {}, {}, {}, {"a"}},
      {"X('a', ->(tau, tau))", {}, {}, {}, {"", "a"}},
      {"X(*('a', 'b'), ->(*('c', 'd'), 'e', 'f'))", 6, {}, 3,
       {"a", "a b a", "c e f", "a b a b a", "c d c e f", "c d c d c e f"}},
      {"X(*('a', 'b'), *('c', ->('d', 'e')))", 6, {}, 2, {"a", "a b a", "c d e c", "a b a b a"}},
      {"X(*('a', 'b'), *('c', ->('d', 'e')))", 6, {}, 3, {"a b a", "c d e c", "a b a b a"}},
      {"X(*('a', 'b'), *(tau, 'd'))", 6, {}, 2, {"d", "d d", "a b a", "d d d", "a b a b a", "a b a b a b a"}},
      {"*(*('a', 'b'), tau)", 5, {}, 3,
       {"a a a", "a b a", "a a b a", "a b a a", "a a a b a", "a a b a a", "a b a a a", "a b a b a"}},
      {"->('a', X('b', ->('c', 'd')))", 3, {}, 3, {"a c d"}},
      {"+('a', X('b', ->('c', 'd')))", 3, {}, 3, {"a c d", "c a d", "c d a"}},
      {"->(->('a', 'b'), ->('a', 'b'))", {}, {}, {}, {"a b a b"}},
      {"->('a', X('a', 'b'))", 6, {}, {}, {"a a", "a b"}},
      // Rejected by the reference: one-child sequence or parallel cut
      // (IndexError), equal subtrees under different parents (KeyError),
      // inclusive choice (KeyError) and interleaving (TypeError).
      {"->('a')", {}, {}, {}, {"!"}},
      {"X(->('a'), 'b')", {}, {}, {}, {"!"}},
      {"->(->('a', 'b'), X('a', 'b'))", {}, {}, {}, {"!"}},
      {"->(+('a', 'b'), X(+('b', 'a'), 'c'))", {}, {}, {}, {"!"}},
      {"X(*('a', tau), *(tau, tau))", 6, {}, 2, {"!"}},
      {"O('a', 'b')", {}, {}, {}, {"!"}},
      {"<>('a', 'b')", {}, {}, {}, {"!"}},
      // An inclusive choice in a loop's third child is never read.
      {"X(*('a', 'b', O('c', 'd')), 'z')", 4, {}, {}, {"a", "z", "a b a"}},
  };
  for (const auto& c : cases) {
    const auto tree = TreeText(c.tree).parse();
    ProcessTreePlayoutOptions options;
    options.variant = ProcessTreePlayoutVariant::kExtensive;
    options.max_length = c.max_length;
    options.max_loop_occurrences = c.max_loops;
    if (c.min_length) options.min_length = *c.min_length;
    std::vector<std::string> words;
    try {
      for (const auto& trace : process_tree_playout(tree, options).traces) {
        std::string word;
        for (const auto& event : trace.events) word += (word.empty() ? "" : " ") + event.activity;
        words.push_back(word);
      }
    } catch (const std::invalid_argument&) {
      words = {"!"};
    }
    if (words != c.words) {
      std::fprintf(stderr, "extensive mismatch for %s\n", c.tree);
      assert(false);
    }
  }
}

void process_tree_topbottom_variant_contract() {
  ProcessTree tree(
      {"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  ProcessTreePlayoutOptions options;
  options.traces = 8;
  options.max_length = 3;
  options.max_loop_occurrences = 1;
  options.cap_sampled_loops = true;  // the opt-in redo bound
  options.seed = 23;

  const auto log = topbottom_process_tree_playout(tree, options);
  assert(log.traces.size() == 8);
  for (const auto& trace : log.traces) {
    assert(!trace.events.empty());
    assert(trace.events.front().activity == "a");
    assert(trace.events.size() == 1 || trace.events.size() == 3);
    if (trace.events.size() == 3) {
      assert(trace.events[1].activity == "b");
      assert(trace.events[2].activity == "a");
    }
  }
}

// The reference envelopes: TOPBOTTOM has neither case ids nor timestamps,
// BASIC_PLAYOUT names cases "0", "1", ... and stamps events one second apart
// from epoch 10,000,000 across cases; add_envelope is Minera's opt-in.
void process_tree_playout_envelopes() {
  ProcessTree tree({"a", "b"}, seq({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}));
  ProcessTreePlayoutOptions options;
  options.traces = 3;
  const auto topbottom = process_tree_playout(tree, options);
  for (const auto& trace : topbottom.traces) {
    assert(!trace.has_case_id());
    for (const auto& event : trace.events) assert(!event.has_timestamp());
  }
  options.variant = ProcessTreePlayoutVariant::BASIC_PLAYOUT;
  const auto basic = process_tree_playout(tree, options);
  for (std::size_t i = 0; i < basic.traces.size(); ++i) {
    assert(basic.traces[i].case_id == std::to_string(i));
    for (std::size_t j = 0; j < 2; ++j)
      assert(basic.traces[i].events[j].timestamp ==
             format_timestamp(10000000.0 + static_cast<double>(2 * i + j)));
  }
  assert(basic.traces[0].events[0].timestamp.rfind("1970-04-26T17:46:40", 0) == 0);
  options.add_envelope = true;
  const auto synthetic = process_tree_playout(tree, options);
  assert(synthetic.traces[0].case_id == "case_1");
  assert(synthetic.traces[0].events[0].timestamp == format_timestamp(options.start));
}

// Loops repeat without a bound unless cap_sampled_loops opts in; a trace
// above the visible-step safety bound raises instead of being dropped.
void process_tree_playout_loop_bounds() {
  ProcessTree loop({"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  ProcessTreePlayoutOptions options;
  options.traces = 4000;
  std::size_t longest = 0;
  for (const auto& trace : process_tree_playout(loop, options).traces)
    longest = std::max(longest, trace.events.size());
  assert(longest > 2 * options.max_loop_occurrences.value_or(3) + 1);
  options.cap_sampled_loops = true;
  for (const auto variant : {ProcessTreePlayoutVariant::TOPBOTTOM, ProcessTreePlayoutVariant::BASIC_PLAYOUT}) {
    options.variant = variant;
    for (const auto& trace : process_tree_playout(loop, options).traces)
      assert(trace.events.size() <= 2 * options.max_loop_occurrences.value_or(3) + 1);
  }
  options.cap_sampled_loops = false;
  options.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
  options.max_steps = 3;
  bool threw = false;
  try { (void)process_tree_playout(loop, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

// Safety bounds of EXTENSIVE and the sampled variants: the interleavings
// of two long words stop once the word budget is spent (C(32,16) shuffles
// otherwise), output_trace_cap caps the final words, a cancellation
// callback stops a run, and an unset max_length bounds a sampled trace at
// 1000 visible events.
void process_tree_playout_safety_bounds() {
  std::vector<std::string> names;
  ProcessTreeNode left{ProcessTreeOperator::kSequence, 0, {}};
  ProcessTreeNode right{ProcessTreeOperator::kSequence, 0, {}};
  for (std::size_t i = 0; i < 16; ++i) {
    names.push_back("a" + std::to_string(i));
    left.children.push_back(ProcessTreeNode::leaf(names.size() - 1));
    names.push_back("b" + std::to_string(i));
    right.children.push_back(ProcessTreeNode::leaf(names.size() - 1));
  }
  const ProcessTree branches(names, ProcessTreeNode{ProcessTreeOperator::kParallel, 0, {left, right}});
  ProcessTreePlayoutOptions ex;
  ex.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  ex.max_traces = 10000;
  const auto bounds = resolve_process_tree_extensive_bounds(branches, ex);
  assert(bounds.max_length == 32 && bounds.max_loop_occurrences == 16);
  const auto started = std::chrono::steady_clock::now();
  const auto result = process_tree_playout_result(branches, ex);
  const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  assert(seconds < 10.0);
  assert(result.truncated);
  assert(result.log.traces.size() == 10000);

  // Six exclusive choices of eight: 8^6 words, cut by the budget; the cap
  // keeps the first 500 in result order.
  std::vector<std::string> choice_names;
  ProcessTreeNode sequence{ProcessTreeOperator::kSequence, 0, {}};
  for (std::size_t c = 0; c < 6; ++c) {
    ProcessTreeNode choice{ProcessTreeOperator::kExclusive, 0, {}};
    for (std::size_t i = 0; i < 8; ++i) {
      choice_names.push_back(std::string(1, static_cast<char>('a' + c)) + std::to_string(i));
      choice.children.push_back(ProcessTreeNode::leaf(choice_names.size() - 1));
    }
    sequence.children.push_back(std::move(choice));
  }
  const ProcessTree choices(choice_names, sequence);
  ex.output_trace_cap = 500;
  ex.return_set_strings = true;
  const auto capped = process_tree_playout_result(choices, ex);
  assert(capped.truncated && capped.log.traces.size() == 500 && capped.trace_strings.size() == 500);
  ex.output_trace_cap = 0;
  ex.return_set_strings = false;
  const auto exact = process_tree_playout_result(ProcessTree({"a", "b"}, seq({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)})), ex);
  assert(!exact.truncated && exact.log.traces.size() == 1);

  std::size_t polls = 0;
  ex.cancelled = [&] { return ++polls > 3; };
  bool cancelled = false;
  try { (void)process_tree_playout_result(branches, ex); }
  catch (const OperationCancelled&) { cancelled = true; }
  assert(cancelled);
  ProcessTreePlayoutOptions sampled;
  sampled.traces = 3;
  sampled.cancelled = [] { return true; };
  cancelled = false;
  try { (void)process_tree_playout(branches, sampled); }
  catch (const OperationCancelled&) { cancelled = true; }
  assert(cancelled);

  std::vector<std::string> long_names;
  ProcessTreeNode long_sequence{ProcessTreeOperator::kSequence, 0, {}};
  for (std::size_t i = 0; i < 1001; ++i) {
    long_names.push_back("x" + std::to_string(i));
    long_sequence.children.push_back(ProcessTreeNode::leaf(i));
  }
  const ProcessTree long_tree(long_names, long_sequence);
  ProcessTreePlayoutOptions steps;
  steps.traces = 1;
  steps.max_steps = 5000;
  bool threw = false;
  try { (void)process_tree_playout(long_tree, steps); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  steps.max_length = 2000;
  assert(process_tree_playout(long_tree, steps).traces.front().events.size() == 1001);
}

// BASIC_PLAYOUT against the reference distribution
// (scripts/capture-process-tree-basic-distribution.py): a chi-square
// homogeneity test per tree at the 0.1% level, bins with fewer than ten
// pooled traces merged. The draws are seeded, so the outcome is fixed.
ProcessTree parse_tree(const std::string& text, std::size_t& at, std::vector<std::string>& names,
                       ProcessTreeNode& out) {
  static const std::vector<std::pair<std::string, ProcessTreeOperator>> kOps = {
      {"->(", ProcessTreeOperator::kSequence}, {"<>(", ProcessTreeOperator::kInterleaving},
      {"X(", ProcessTreeOperator::kExclusive}, {"+(", ProcessTreeOperator::kParallel},
      {"*(", ProcessTreeOperator::kLoop}, {"O(", ProcessTreeOperator::kInclusiveChoice}};
  for (const auto& [prefix, op] : kOps) {
    if (text.compare(at, prefix.size(), prefix) != 0) continue;
    at += prefix.size();
    out = ProcessTreeNode{op, 0, {}};
    do {
      if (text[at] == ',') ++at;
      out.children.emplace_back();
      (void)parse_tree(text, at, names, out.children.back());
    } while (text[at] == ',');
    ++at;  // ')'
    return {};
  }
  const auto end = text.find_first_of(",)", at);
  const auto label = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
  at = end == std::string::npos ? text.size() : end;
  if (label == "tau") {
    out = ProcessTreeNode::tau();
  } else {
    auto found = std::find(names.begin(), names.end(), label);
    out = ProcessTreeNode::leaf(static_cast<std::size_t>(found - names.begin()));
    if (found == names.end()) names.push_back(label);
  }
  return {};
}

void process_tree_basic_distribution(const char* path) {
  std::ifstream input(path);
  assert(input);
  std::vector<std::pair<std::string, std::map<std::string, double>>> trees;
  std::string line;
  while (std::getline(input, line)) {
    if (line.rfind("C ", 0) == 0) trees.push_back({line.substr(2), {}});
    if (line.rfind("F ", 0) == 0) {
      std::istringstream fields(line.substr(2));
      double count = 0;
      std::string word;
      fields >> count >> word;
      trees.back().second[word] = count;
    }
  }
  assert(trees.size() >= 10);
  for (const auto& [text, reference] : trees) {
    std::size_t at = 0;
    std::vector<std::string> names;
    ProcessTreeNode root;
    (void)parse_tree(text, at, names, root);
    ProcessTree tree(names, root);
    ProcessTreePlayoutOptions options;
    options.variant = ProcessTreePlayoutVariant::BASIC_PLAYOUT;
    options.traces = 40000;
    options.seed = 20261103;
    std::map<std::string, double> native;
    for (const auto& trace : process_tree_playout(tree, options).traces) {
      std::string word;
      for (const auto& event : trace.events) word += (word.empty() ? "" : ".") + event.activity;
      native[word.empty() ? "-" : word] += 1;
    }
    double reference_total = 0, native_total = 0;
    for (const auto& [word, count] : reference) reference_total += count;
    for (const auto& [word, count] : native) native_total += count;
    std::map<std::string, std::pair<double, double>> bins;
    std::pair<double, double> rest{0, 0};
    std::set<std::string> words;
    for (const auto& [word, count] : reference) words.insert(word);
    for (const auto& [word, count] : native) words.insert(word);
    for (const auto& word : words) {
      const double r = reference.contains(word) ? reference.at(word) : 0;
      const double n = native.contains(word) ? native.at(word) : 0;
      if (r + n < 10) { rest.first += r; rest.second += n; }
      else bins[word] = {r, n};
    }
    if (rest.first + rest.second > 0) bins["<rest>"] = rest;
    double statistic = 0;
    const double total = reference_total + native_total;
    for (const auto& [word, pair] : bins) {
      const double column = pair.first + pair.second;
      const double expected_r = column * reference_total / total;
      const double expected_n = column * native_total / total;
      statistic += (pair.first - expected_r) * (pair.first - expected_r) / expected_r +
                   (pair.second - expected_n) * (pair.second - expected_n) / expected_n;
    }
    const double df = std::max<double>(1.0, static_cast<double>(bins.size()) - 1.0);
    // Wilson-Hilferty upper 0.1% point of chi-square(df).
    const double z = 3.090232306;
    const double h = 2.0 / (9.0 * df);
    const double critical = df * std::pow(1.0 - h + z * std::sqrt(h), 3.0);
    if (!(statistic < critical))
      std::fprintf(stderr, "BASIC distribution of %s: chi2 %.2f >= %.2f (df %.0f)\n", text.c_str(),
                   statistic, critical, df);
    assert(statistic < critical);
  }
}
}  // namespace

int main(int argc, char** argv) {
  process_tree_playout_envelopes();
  process_tree_playout_loop_bounds();
  process_tree_playout_safety_bounds();
  process_tree_basic_distribution(
      argc > 1 ? argv[1] : "src/backend/tests/fixtures/process_tree_basic_distribution.txt");
  process_tree_basic_variant_contract();
  process_tree_extensive_variant_contract();
  process_tree_extensive_reference_contract();
  process_tree_topbottom_variant_contract();

  ProcessTree tree({"a", "b"}, seq({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}));
  ProcessTreePlayoutOptions ex;
  ex.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  ex.max_length = 3;
  ex.min_length = 2;
  ex.max_traces = 10;
  auto log = process_tree_playout(tree, ex);
  assert(log.traces.size() == 1);
  assert((labels(log.traces[0]) == std::vector<std::string>{"a", "b"}));

  ex.return_set_strings = true;
  const auto set_result = process_tree_playout_result(tree, ex);
  assert(set_result.log.traces.size() == 1);
  assert((set_result.trace_strings ==
         std::vector<std::vector<std::string>>{{"a", "b"}}));
  assert(!set_result.truncated);
  assert(serialize_process_tree_playout_json(set_result).find(
             "\"truncated\":false") != std::string::npos);

  ProcessTree duplicate({"a"}, xor_node({ProcessTreeNode::leaf(0),
                                          ProcessTreeNode::leaf(0)}));
  ex.max_length = 1;
  ex.min_length = 0;
  const auto duplicate_result = process_tree_playout_result(duplicate, ex);
  assert((duplicate_result.trace_strings ==
         std::vector<std::vector<std::string>>{{"a"}}));

  ProcessTree empty_tree({"a"}, ProcessTreeNode::tau());
  const auto empty_result = process_tree_playout_result(empty_tree, ex);
  assert((empty_result.trace_strings ==
         std::vector<std::vector<std::string>>{{}}));
  assert(empty_result.log.traces.size() == 1 &&
         empty_result.log.traces.front().events.empty());
  ex.return_set_strings = false;

  ProcessTree choice({"a", "b"}, xor_node({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}));
  ex.max_length = 1;
  ex.min_length = 0;
  ex.max_traces = 10;
  log = process_tree_playout(choice, ex);
  assert(log.traces.size() == 2);

  ex.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
  ex.traces = 3;
  auto topbottom = process_tree_playout(choice, ex);
  assert(topbottom.traces.size() == 3);

  ex.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  ex.max_length = 2;
  ProcessTree parallel({"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kParallel, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  log = process_tree_playout(parallel, ex);
  assert(log.traces.size() == 2);

  ProcessTree interleaving(
      {"a", "b", "c", "d"},
      ProcessTreeNode{ProcessTreeOperator::kInterleaving, 0,
                      {seq({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}),
                       seq({ProcessTreeNode::leaf(2), ProcessTreeNode::leaf(3)})}});
  ex.max_length = 4;
  ex.min_length = 4;
  ex.max_traces = 10;
  // The reference EXTENSIVE playout has no interleaving (TypeError).
  bool extensive_interleaving = false;
  try {
    (void)process_tree_playout(interleaving, ex);
  } catch (const std::invalid_argument&) {
    extensive_interleaving = true;
  }
  assert(extensive_interleaving);
  EventLog interleaving_log;
  for (const auto& word : {std::vector<std::string>{"a", "b", "c", "d"},
                           std::vector<std::string>{"c", "d", "a", "b"}}) {
    Trace trace;
    trace.case_id = std::to_string(interleaving_log.traces.size());
    for (const auto& activity : word) trace.events.push_back({trace.case_id, activity});
    interleaving_log.traces.push_back(std::move(trace));
  }
  const auto interleaving_net = to_petri_net(interleaving);
  assert(check_conformance(interleaving_log, interleaving_net).fitness == 1.0);
  EventLog mixed;
  Trace mixed_trace;
  mixed_trace.case_id = "mixed";
  for (const auto& activity : {"a", "c", "b", "d"})
    mixed_trace.events.push_back({mixed_trace.case_id, activity});
  mixed.traces.push_back(std::move(mixed_trace));
  assert(check_conformance(mixed, interleaving_net).fitting_traces == 0);
  const auto interleaving_ptml = to_ptml(interleaving);
  assert(interleaving_ptml.find("<interleaving ") != std::string::npos);
  const auto interleaving_round_trip = parse_ptml(interleaving_ptml);
  assert(interleaving_round_trip.root().op == ProcessTreeOperator::kInterleaving);
  assert(check_conformance(interleaving_log,
                           to_petri_net(interleaving_round_trip)).fitness == 1.0);
  const auto interleaving_json = random_tree_json(interleaving);
  assert(interleaving_json.find("\"interleaving\"") != std::string::npos);
  const auto interleaving_bpmn = parse_bpmn(to_bpmn(interleaving));
  // Roundtrip routing changes which duplicate is immediately enabled.
  // Exact alignment verifies the language independently of replay's choice.
  assert(check_alignment(interleaving_log, interleaving_bpmn).fitness == 1.0);
  assert(check_conformance(mixed, interleaving_bpmn).fitting_traces == 0);

  ProcessTree inclusive(
      {"a", "b"}, or_node({ProcessTreeNode::leaf(0),
                              ProcessTreeNode::leaf(1)}));
  ex.max_length = 2;
  ex.min_length = 0;
  ex.max_traces = 10;
  // The reference EXTENSIVE playout has no inclusive choice (KeyError).
  bool extensive_or = false;
  try {
    (void)process_tree_playout(inclusive, ex);
  } catch (const std::invalid_argument&) {
    extensive_or = true;
  }
  assert(extensive_or);

  // Basic sampling selects each child with probability 1/2 (possibly none,
  // as the reference does) and remains deterministic for a fixed seed.
  ProcessTreePlayoutOptions sampled;
  sampled.variant = ProcessTreePlayoutVariant::BASIC_PLAYOUT;
  sampled.traces = 24;
  sampled.max_length = 2;
  sampled.seed = 77;
  const auto sampled_first = process_tree_playout(inclusive, sampled);
  const auto sampled_second = process_tree_playout(inclusive, sampled);
  assert(sampled_first.traces.size() == sampled_second.traces.size());
  bool sampled_empty = false;
  for (std::size_t i = 0; i < sampled_first.traces.size(); ++i) {
    assert(labels(sampled_first.traces[i]) == labels(sampled_second.traces[i]));
    sampled_empty = sampled_empty || sampled_first.traces[i].events.empty();
    assert(sampled_first.traces[i].events.size() <= 2);
  }
  assert(sampled_empty);
  // TOPBOTTOM has no inclusive choice: the reference raises TypeError.
  bool topbottom_or = false;
  try {
    sampled.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
    (void)process_tree_playout(inclusive, sampled);
  } catch (const std::invalid_argument&) {
    topbottom_or = true;
  }
  assert(topbottom_or);

  const auto ptml = to_ptml(inclusive);
  assert(ptml.find("<or ") != std::string::npos);
  const auto ptml_round_trip = parse_ptml(ptml);
  assert(ptml_round_trip.root().op == ProcessTreeOperator::kInclusiveChoice);
  const auto json = random_tree_json(inclusive);
  assert(json.find("inclusive_or") != std::string::npos);

  // The workflow net accepts every non-empty subset, including the concurrent
  // a+b branch, without leaving residual tokens at completion.
  const auto inclusive_net = to_petri_net(inclusive);
  assert(final_marking_reachable(inclusive_net));
  assert(check_alignment(log, inclusive_net).fitness == 1.0);

  bool invalid_or = false;
  try {
    (void)process_tree_playout(
        ProcessTree({"a"}, ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice,
                                             0, {}}),
        ex);
  } catch (const std::invalid_argument&) {
    invalid_or = true;
  }
  assert(invalid_or);
  invalid_or = false;
  try {
    (void)to_petri_net(
        ProcessTree({"a"}, ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice,
                                             0, {}}));
  } catch (const std::invalid_argument&) {
    invalid_or = true;
  }
  assert(invalid_or);

  ProcessTree with_tau({"a"}, seq({ProcessTreeNode::tau(), ProcessTreeNode::leaf(0)}));
  log = process_tree_playout(with_tau, ex);
  assert(log.traces.size() == 1 && log.traces[0].events.size() == 1);

  ProcessTreePlayoutOptions basic;
  basic.traces = 5;
  basic.max_length = 3;
  basic.seed = 42;
  auto first = process_tree_playout(choice, basic);
  auto second = process_tree_playout(choice, basic);
  assert(first.traces.size() == second.traces.size());
  for (std::size_t i = 0; i < first.traces.size(); ++i)
    assert(labels(first.traces[i]) == labels(second.traces[i]));

  ProcessTree loop({"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  ProcessTreePlayoutOptions omitted;
  omitted.traces = 1;
  bool default_matches_topbottom = true;
  bool default_differs_from_basic = false;
  for (std::uint64_t seed = 1; seed <= 64; ++seed) {
    omitted.seed = seed;
    const auto default_log = process_tree_playout(loop, omitted);
    auto explicit_topbottom = omitted;
    explicit_topbottom.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
    auto explicit_basic = omitted;
    explicit_basic.variant = ProcessTreePlayoutVariant::BASIC_PLAYOUT;
    default_matches_topbottom =
        default_matches_topbottom &&
        labels(default_log.traces.front()) ==
            labels(process_tree_playout(loop, explicit_topbottom).traces.front());
    default_differs_from_basic =
        default_differs_from_basic ||
        labels(default_log.traces.front()) !=
            labels(process_tree_playout(loop, explicit_basic).traces.front());
  }
  assert(default_matches_topbottom && default_differs_from_basic);

  basic.traces = 1;
  basic.max_length = 1000;
  bool variants_differ = false;
  for (std::uint64_t seed = 1; seed <= 64 && !variants_differ; ++seed) {
    basic.seed = seed;
    basic.variant = ProcessTreePlayoutVariant::BASIC_PLAYOUT;
    const auto basic_loop = process_tree_playout(loop, basic);
    basic.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
    const auto topbottom_loop = process_tree_playout(loop, basic);
    variants_differ =
        labels(basic_loop.traces.front()) !=
        labels(topbottom_loop.traces.front());
  }
  assert(variants_differ);

  // A wide parallel cut has factorially many interleavings. The extensive
  // budget must stop generation before traversing the full language.
  std::vector<std::string> wide_names;
  std::vector<ProcessTreeNode> wide_children;
  for (std::size_t i = 0; i < 12; ++i) {
    wide_names.push_back("w" + std::to_string(i));
    wide_children.push_back(ProcessTreeNode::leaf(i));
  }
  ProcessTree wide_parallel(
      wide_names,
      ProcessTreeNode{ProcessTreeOperator::kParallel, 0,
                      std::move(wide_children)});
  ProcessTreePlayoutOptions bounded;
  bounded.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  bounded.max_length = 12;
  bounded.max_traces = 1;
  const auto bounded_log = process_tree_playout(wide_parallel, bounded);
  assert(bounded_log.traces.size() <= 1);
  if (!bounded_log.traces.empty())
    assert(bounded_log.traces.front().events.size() == 12);

  ProcessTree wide_choice(
      wide_names,
      ProcessTreeNode{ProcessTreeOperator::kExclusive, 0,
                      {ProcessTreeNode::tau(), wide_parallel.root()}});
  bounded.min_length = 1;
  bounded.return_set_strings = true;
  const auto bounded_result = process_tree_playout_result(wide_choice, bounded);
  assert(bounded_result.truncated);
  assert(serialize_process_tree_playout_json(bounded_result).find(
             "\"truncated\":true") != std::string::npos);

  bool threw = false;
  try { (void)process_tree_playout(ProcessTree({"a"}, ProcessTreeNode::leaf(3)), basic); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  return 0;
}
