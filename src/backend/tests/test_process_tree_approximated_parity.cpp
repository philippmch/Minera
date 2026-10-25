// algo.conformance.alignments.process_tree.variants.approximated (original,
// utilities, calculate_a_sa_ea_sets) against the public seeded bank
// docs/process-tree-approximated-bank.txt.gz
// (scripts/capture-process-tree-approximated-bank.py):
// - the binary tree, and every node's activity, start and end sets, tau flag
//   and height, must be equal exactly;
// - every program a sequence, parallel or loop node hands PuLP must be equal
//   exactly (objective, constant, rows in order), the native optimum must
//   equal the objective of the reference's solution, and the split read from
//   that solution must lead to the reference's child calls;
// - every choice must pick the reference's child, and every operator must
//   compose the reference's child alignments into its returned alignment;
// - replaying the reference's solutions and base alignments, apply must give
//   the reference's alignment, cost and fitness for every trace;
// - each base alignment the reference reports must be optimal: the native
//   Dijkstra alignment has the same deviation and tau-leaf counts (which
//   optimal alignment comes back varies with object addresses there, so
//   identical ones are tallied);
// - the fully native apply must give a well-formed alignment of every trace
//   whose cost and fitness follow from it (CBC may pick another optimal split,
//   so equal costs are tallied).
#include "minera/process_tree_approximated_reference.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
};

class JsonReader {
 public:
  explicit JsonReader(const char* text) : at_(text) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space(); ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kString;
      for (++at_; *at_ != '"'; ++at_) {
        if (*at_ == '\\') {
          ++at_;
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_;
          continue;
        }
        out.text += *at_;
      }
      ++at_;
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      out.kind = Json::kNumber;
      char* end = nullptr;
      out.number = std::strtod(at_, &end);
      at_ = end;
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};

// Parses the reference's tree notation: 'label', tau, ->( ), X( ), +( ), *( ).
struct TreeParser {
  std::string text;
  std::size_t at = 0;
  std::vector<std::string> activities;

  void skip() {
    while (at < text.size() && (text[at] == ' ' || text[at] == ',')) ++at;
  }

  ProcessTreeNode node() {
    skip();
    if (text.compare(at, 3, "tau") == 0) {
      at += 3;
      return ProcessTreeNode::tau();
    }
    if (text[at] == '\'') {
      const auto end = text.find('\'', at + 1);
      const auto label = text.substr(at + 1, end - at - 1);
      at = end + 1;
      std::size_t index = 0;
      while (index < activities.size() && activities[index] != label) ++index;
      if (index == activities.size()) activities.push_back(label);
      return ProcessTreeNode::leaf(index);
    }
    ProcessTreeOperator op;
    if (text.compare(at, 2, "->") == 0) {
      op = ProcessTreeOperator::kSequence;
      at += 2;
    } else if (text[at] == 'X') {
      op = ProcessTreeOperator::kExclusive;
      ++at;
    } else if (text[at] == '+') {
      op = ProcessTreeOperator::kParallel;
      ++at;
    } else if (text[at] == '*') {
      op = ProcessTreeOperator::kLoop;
      ++at;
    } else {
      throw std::invalid_argument("unexpected tree text: " + text.substr(at));
    }
    skip();
    ++at;  // (
    ProcessTreeNode result{op, 0, {}};
    for (;;) {
      skip();
      if (text[at] == ')') {
        ++at;
        return result;
      }
      result.children.push_back(node());
    }
  }
};

ProcessTree parse_tree(const std::string& text) {
  TreeParser parser{text};
  auto root = parser.node();
  return ProcessTree(parser.activities, std::move(root));
}

std::string text_of(const ProcessTree& tree, const ProcessTreeNode& node) {
  switch (node.op) {
    case ProcessTreeOperator::kTau:
      return "tau";
    case ProcessTreeOperator::kActivity:
      return "'" + tree.activities().at(node.activity) + "'";
    default:
      break;
  }
  std::string out = node.op == ProcessTreeOperator::kSequence    ? "->( "
                    : node.op == ProcessTreeOperator::kExclusive ? "X( "
                    : node.op == ProcessTreeOperator::kParallel  ? "+( "
                                                                 : "*( ";
  for (std::size_t k = 0; k < node.children.size(); ++k) {
    if (k) out += ", ";
    out += text_of(tree, node.children[k]);
  }
  return out + " )";
}

struct BankCase {
  std::string name;
  Json head, nodes, apply;
  std::map<std::vector<std::string>, Json> variants;
};

std::vector<std::string> strings(const Json& value) {
  std::vector<std::string> out;
  for (const auto& item : value.items) out.push_back(item.text);
  return out;
}

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    if (line[0] == 'C') {
      cases.emplace_back();
      cases.back().name = line.substr(2);
    } else if (line[0] == 'T') {
      cases.back().head = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'N') {
      cases.back().nodes = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'V') {
      Json variant = JsonReader(line.c_str() + 2).value();
      cases.back().variants.emplace(strings(variant.items[0]), std::move(variant.items[1]));
    } else if (line[0] == 'P') {
      cases.back().apply = JsonReader(line.c_str() + 2).value();
    }
  }
  return cases;
}

ProcessTreeApproximatedMoves moves_of(const Json& value) {
  ProcessTreeApproximatedMoves out;
  for (const auto& move : value.items) {
    ProcessTreeApproximatedMove m{move.items[0].text, std::nullopt};
    if (move.items[1].kind == Json::kString) m.model = move.items[1].text;
    out.push_back(std::move(m));
  }
  return out;
}

std::string show(const ProcessTreeApproximatedMoves& moves) {
  std::string out = "[";
  for (const auto& m : moves) out += "(" + m.log + "," + (m.model ? *m.model : "None") + ")";
  return out + "]";
}

std::string show(const std::vector<std::string>& trace) {
  std::string out;
  for (const auto& e : trace) out += e + " ";
  return out;
}

using Terms = std::vector<std::pair<std::string, double>>;

Terms terms_of(const Json& value) {
  Terms out;
  // PuLP writes an all-zero objective as one __dummy term with coefficient 0.
  for (const auto& term : value.items) {
    if (term.items[1].number != 0) out.emplace_back(term.items[0].text, term.items[1].number);
  }
  std::sort(out.begin(), out.end());
  return out;
}

Terms sorted(Terms terms) {
  std::sort(terms.begin(), terms.end());
  return terms;
}

const char* kind_name(ProcessTreeApproximatedSplitKind kind) {
  return kind == ProcessTreeApproximatedSplitKind::kSequence   ? "sequence"
         : kind == ProcessTreeApproximatedSplitKind::kParallel ? "parallel"
                                                               : "loop";
}

std::size_t tau_moves(const ProcessTreeApproximatedMoves& moves) {
  std::size_t out = 0;
  for (const auto& m : moves) out += !m.model;
  return out;
}

bool replays(const ProcessTreeApproximatedMoves& moves, const std::vector<std::string>& trace) {
  std::vector<std::string> logged;
  for (const auto& m : moves) {
    if (m.log != ">>") {
      logged.push_back(m.log);
      if (m.model && *m.model != ">>" && *m.model != m.log) return false;
    } else if (m.model && *m.model == ">>") {
      return false;
    }
  }
  return logged == trace;
}

struct Tally {
  std::size_t trees = 0, raising = 0, nodes = 0, variants = 0, programs = 0, choices = 0, bases = 0,
              same_bases = 0, returns = 0, traces = 0, none = 0, native_same_cost = 0, native_traces = 0,
              native_none_differs = 0, short_of_rewards = 0;
};

// Feeds the recorded reference events to the recursion and checks each one.
class Replay {
 public:
  Replay(const BankCase& item, Tally& tally, std::vector<std::string>& failures, const ProcessTree& binary)
      : item_(item), tally_(tally), failures_(failures), binary_(binary) {
    index(binary.root());
  }

  ProcessTreeApproximatedOracle oracle() {
    ProcessTreeApproximatedOracle o;
    o.solve = [this](std::size_t node, ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
                     const ProcessTreeApproximatedProgram& program) {
      const Json& e = next(node, trace, "L");
      std::map<std::string, double> solution;
      if (e.items[2].text != kind_name(kind) || strings(e.items[3]) != trace) {
        fail("program call differs at node " + std::to_string(node));
        return solution;
      }
      ++tally_.programs;
      if (sorted(program.objective) != terms_of(e.items[4]) || program.constant != e.items[5].number) {
        fail("objective differs at node " + std::to_string(node) + " for " + show(trace));
      }
      const auto& rows = e.items[6].items;
      if (rows.size() != program.rows.size()) {
        fail("row count " + std::to_string(program.rows.size()) + " vs " + std::to_string(rows.size()));
      } else {
        for (std::size_t r = 0; r < rows.size(); ++r) {
          const auto& row = program.rows[r];
          if (sorted(row.terms) != terms_of(rows[r].items[0]) || row.sense != rows[r].items[1].number ||
              row.constant != rows[r].items[2].number) {
            fail("row _C" + std::to_string(r + 1) + " differs at node " + std::to_string(node) + " for " +
                 show(trace));
            break;
          }
        }
      }
      for (const auto& value : e.items[7].items) solution[value.items[0].text] = value.items[1].number;
      if (e.items[8].number != 1) fail("reference program not optimal");
      std::vector<ProcessTreeApproximatedSets> children;
      for (std::size_t child : children_[node]) children.push_back(sets_[child]);
      const double native = reference_approximated_optimal_split(kind, trace, children).second;
      const double reference = reference_approximated_objective(program, solution);
      // CBC treats the objective as integral (its cutoff increment grows to
      // 0.9999), so it may stop short of the -1e-05 rewards for empty tau
      // executions; native must then be better by those rewards only.
      if (native < reference - 1e-9 && reference - native < 1e-3) {
        ++tally_.short_of_rewards;
      } else if (std::abs(native - reference) > 1e-9) {
        fail("native optimum " + std::to_string(native) + " vs reference " + std::to_string(reference) +
             " at node " + std::to_string(node) + " for " + show(trace));
      }
      return solution;
    };
    o.base = [this](std::size_t node, const std::vector<std::string>& trace) {
      const Json& e = next(node, trace, "B");
      if (e.items[3].kind != Json::kArray) fail("reference base alignment missing");
      return moves_of(e.items[3]);
    };
    o.choice = [this](std::size_t node, const std::vector<std::string>& trace, std::size_t child) {
      const Json& e = next(node, trace, "C");
      ++tally_.choices;
      if (strings(e.items[2]) != trace || static_cast<std::size_t>(e.items[3].number) != child) {
        fail("choice at node " + std::to_string(node) + " for " + show(trace));
      }
    };
    o.returned = [this](std::size_t node, const std::optional<ProcessTreeApproximatedMoves>& alignment) {
      const Json& e = next(node, {}, "R");
      ++tally_.returns;
      const bool none = e.items[3].kind != Json::kArray;
      if (none != !alignment || (alignment && moves_of(e.items[3]) != *alignment)) {
        fail("returned alignment at node " + std::to_string(node) + ": native " +
             (alignment ? show(*alignment) : "None") + " reference " + (none ? "None" : show(moves_of(e.items[3]))));
      }
    };
    return o;
  }

  void finish() {
    if (events_ && cursor_ != events_->items.size()) fail("reference events left over");
  }

 private:
  void index(const ProcessTreeNode& node) {
    const std::size_t slot = pointers_.size();
    pointers_.push_back(&node);
    children_.emplace_back();
    for (const auto& child : node.children) {
      children_[slot].push_back(pointers_.size());
      index(child);
    }
    if (slot == 0) sets_ = reference_approximated_sets(binary_);
  }

  const Json& next(std::size_t node, const std::vector<std::string>& trace, const char* type) {
    static const Json empty;
    if (node == 0 && (!events_ || cursor_ == events_->items.size())) {
      finish();
      const auto found = item_.variants.find(trace);
      if (found == item_.variants.end()) {
        fail("no reference variant for " + show(trace));
        return empty;
      }
      events_ = &found->second;
      cursor_ = 0;
      ++tally_.variants;
    }
    if (!events_ || cursor_ >= events_->items.size()) {
      fail(std::string("missing reference event ") + type);
      return empty;
    }
    const Json& e = events_->items[cursor_++];
    if (e.items[0].text != type || static_cast<std::size_t>(e.items[1].number) != node) {
      fail(std::string("expected ") + type + " at node " + std::to_string(node) + ", reference has " +
           e.items[0].text + " at node " + std::to_string(static_cast<std::size_t>(e.items[1].number)));
      throw std::runtime_error("replay out of step");
    }
    return e;
  }

  void fail(const std::string& message) { failures_.push_back(item_.name + ": " + message); }

  const BankCase& item_;
  Tally& tally_;
  std::vector<std::string>& failures_;
  const ProcessTree& binary_;
  std::vector<const ProcessTreeNode*> pointers_;
  std::vector<std::vector<std::size_t>> children_;
  std::vector<ProcessTreeApproximatedSets> sets_;
  const Json* events_ = nullptr;
  std::size_t cursor_ = 0;
};

// The binary tree and the sets, tau flags and heights of its nodes.
void sets_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  auto fail = [&](const std::string& message) { failures.push_back(item.name + ": " + message); };
  ++tally.trees;
  const ProcessTree binary = reference_approximated_binary_tree(parse_tree(item.head.items[0].text));
  if (item.head.items[1].text == "raises") {
    ++tally.raising;
    try {
      (void)reference_approximated_sets(binary);
      fail("native sets do not raise");
    } catch (const ProcessTreeApproximatedReferenceError&) {
    }
    return;
  }
  // The reference prints a leaf root without quotes.
  std::string binary_text = text_of(binary, binary.root());
  if (binary.root().op == ProcessTreeOperator::kActivity) binary_text = binary_text.substr(1, binary_text.size() - 2);
  if (binary_text != item.head.items[1].text) {
    fail("binary tree " + binary_text + " vs " + item.head.items[1].text);
    return;
  }
  const auto sets = reference_approximated_sets(binary);
  if (sets.size() != item.nodes.items.size()) fail("node count differs");
  for (std::size_t k = 0; k < sets.size() && k < item.nodes.items.size(); ++k) {
    const auto& n = item.nodes.items[k].items;
    ++tally.nodes;
    const ProcessTreeApproximatedSets expected{strings(n[0]), strings(n[1]), strings(n[2]), n[3].boolean,
                                               static_cast<std::size_t>(n[4].number)};
    if (!(sets[k] == expected)) fail("sets of node " + std::to_string(k) + " differ");
  }
}

// Each reference base alignment against the native Dijkstra alignment of the
// same subtree.
void base_alignment_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  auto fail = [&](const std::string& message) { failures.push_back(item.name + ": " + message); };
  if (item.head.items[1].text == "raises") return;
  const ProcessTree binary = reference_approximated_binary_tree(parse_tree(item.head.items[0].text));
  std::vector<const ProcessTreeNode*> nodes;
  std::vector<const ProcessTreeNode*> stack{&binary.root()};
  while (!stack.empty()) {
    const ProcessTreeNode* node = stack.back();
    stack.pop_back();
    nodes.push_back(node);
    for (auto child = node->children.rbegin(); child != node->children.rend(); ++child) stack.push_back(&*child);
  }
  for (const auto& [variant, events] : item.variants) {
    for (const auto& e : events.items) {
      if (e.items[0].text != "B") continue;
      ++tally.bases;
      const auto node = static_cast<std::size_t>(e.items[1].number);
      const auto trace = strings(e.items[2]);
      if (e.items[3].kind != Json::kArray) {
        fail("reference base alignment missing");
        continue;
      }
      const auto reference = moves_of(e.items[3]);
      const auto [native, cost] = reference_approximated_optimal_alignment(binary, *nodes.at(node), trace);
      if (!replays(reference, trace) || !replays(native, trace)) fail("base alignment does not replay " + show(trace));
      if (reference_approximated_cost(native) != reference_approximated_cost(reference) ||
          tau_moves(native) != tau_moves(reference) ||
          static_cast<long long>(reference_approximated_cost(native)) != cost / 10000) {
        fail("base alignment at node " + std::to_string(node) + " for " + show(trace) + ": native " +
             show(native) + " reference " + show(reference));
      }
      tally.same_bases += native == reference;
    }
  }
}

// apply, replaying the reference's solutions and base alignments, and fully
// native.
void approximated_case(const BankCase& item, Tally& tally, std::vector<std::string>& failures) {
  auto fail = [&](const std::string& message) { failures.push_back(item.name + ": " + message); };
  const ProcessTree tree = parse_tree(item.head.items[0].text);
  ProcessTreeApproximatedOptions options;
  options.max_trace_length = static_cast<std::size_t>(item.head.items[2].number);
  options.max_tree_height = static_cast<std::size_t>(item.head.items[3].number);
  std::vector<std::vector<std::string>> traces;
  for (const auto& trace : item.head.items[4].items) traces.push_back(strings(trace));
  const ProcessTree binary = reference_approximated_binary_tree(tree);
  if (item.head.items[1].text == "raises") {
    try {
      (void)reference_approximated_alignments(tree, traces, options);
      fail("native apply does not raise");
    } catch (const ProcessTreeApproximatedReferenceError&) {
    }
    if (item.apply.kind != Json::kString) fail("reference apply did not raise");
    return;
  }
  if (item.apply.kind != Json::kArray) {
    fail("reference apply raised");
    return;
  }
  Replay replay(item, tally, failures, binary);
  std::vector<std::optional<ProcessTreeApproximatedAlignment>> replayed;
  try {
    replayed = reference_approximated_alignments(tree, traces, options, replay.oracle());
    replay.finish();
  } catch (const std::runtime_error& error) {
    fail(error.what());
    return;
  }
  const auto native = reference_approximated_alignments(tree, traces, options);
  for (std::size_t t = 0; t < traces.size(); ++t) {
    const Json& expected = item.apply.items[t];
    ++tally.traces;
    if (expected.kind != Json::kArray) {
      ++tally.none;
      if (replayed[t]) fail("replayed trace " + std::to_string(t) + " is not None");
      tally.native_none_differs += native[t].has_value();
      continue;
    }
    const auto alignment = moves_of(expected.items[0]);
    const auto cost = static_cast<std::size_t>(expected.items[1].number);
    const double fitness = expected.items[2].number;
    if (!replayed[t] || replayed[t]->alignment != alignment || replayed[t]->cost != cost ||
        std::abs(replayed[t]->fitness - fitness) > 1e-12) {
      fail("replayed trace " + std::to_string(t) + ": " +
           (replayed[t] ? show(replayed[t]->alignment) + " cost " + std::to_string(replayed[t]->cost) : "None") +
           " vs " + show(alignment) + " cost " + std::to_string(cost));
    }
    if (!native[t]) {
      ++tally.native_none_differs;
      continue;
    }
    ++tally.native_traces;
    if (!replays(native[t]->alignment, traces[t]) ||
        native[t]->cost != reference_approximated_cost(native[t]->alignment)) {
      fail("native trace " + std::to_string(t) + " is not well formed: " + show(native[t]->alignment));
    }
    // fitness = 1 - cost / (bwc + len) with the same bwc
    const double bwc_reference = cost ? cost / (1.0 - fitness) - traces[t].size() : -1;
    if (cost && native[t]->cost &&
        std::abs(native[t]->cost / (1.0 - native[t]->fitness) - traces[t].size() - bwc_reference) > 1e-6) {
      fail("native fitness floor differs for trace " + std::to_string(t));
    }
    tally.native_same_cost += native[t]->cost == cost;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: minera_process_tree_approximated_parity_tests <bank>\n";
    return 2;
  }
  std::vector<std::string> failures;
  Tally tally;
  for (const auto& item : read_bank(argv[1])) {
    try {
      sets_case(item, tally, failures);
      base_alignment_case(item, tally, failures);
      approximated_case(item, tally, failures);
    } catch (const std::exception& error) {
      failures.push_back(item.name + ": " + error.what());
    }
  }
  for (std::size_t k = 0; k < failures.size() && k < 30; ++k) std::cerr << failures[k] << "\n";
  std::cout << "approximated original: " << tally.trees << " trees (" << tally.raising << " raising), " << tally.nodes
            << " node sets, " << tally.variants << " variants replayed with " << tally.programs << " programs (" << tally.short_of_rewards
            << " where CBC stops short of tau rewards), "
            << tally.choices << " choices, " << tally.bases << " base alignments (" << tally.same_bases
            << " identical), " << tally.returns << " returns; apply: " << tally.traces << " traces (" << tally.none
            << " None), native " << tally.native_traces << " (" << tally.native_same_cost << " same cost, "
            << tally.native_none_differs << " None where the reference is not or the other way round); "
            << failures.size() << " failing cases\n";
  return failures.empty() ? 0 : 1;
}
