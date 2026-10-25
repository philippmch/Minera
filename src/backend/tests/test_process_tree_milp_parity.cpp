// algo.conformance.alignments.process_tree.variants.milp against the public
// seeded bank docs/process-tree-milp-bank.txt.gz
// (scripts/capture-process-tree-milp-bank.py):
// - the flow graph (node order and data, edge order, labels, capacities and
//   costs) and every program handed to the solver must be equal exactly;
// - the (cost, alignment) read from the reference's own solution must be
//   equal exactly;
// - the native optimum must give the reference cost to 1e-6 (the reference
//   solver reports values to about 1e-8), and apply's costs and fitness too;
//   alignments from tied optimal solutions are tallied, and checked to list
//   every event once with a synchronous move only on an equal label.
// The empty trace on a root loop with a tau do gives an infeasible program,
// where the reference reports whatever its solver left; native raises there
// (tallied).
#include "minera/process_tree_milp_reference.hpp"

#include "../gzip_codec.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
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

struct BankCase {
  std::string name, tree;
  Json graph, apply;
  std::vector<Json> records;
};

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
      cases.back().tree = line.substr(2);
    } else if (line[0] == 'G') {
      cases.back().graph = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'L') {
      cases.back().records.push_back(JsonReader(line.c_str() + 2).value());
    } else if (line[0] == 'P') {
      cases.back().apply = JsonReader(line.c_str() + 2).value();
    }
  }
  return cases;
}

struct Tally {
  std::size_t graphs = 0, programs = 0, extracted = 0, solved = 0, same_alignment = 0, tied_alignment = 0,
              cached = 0, infeasible = 0, no_cost = 0, applied = 0, apply_raises = 0, apply_infeasible = 0;
};

std::vector<std::string> strings(const Json& value) {
  std::vector<std::string> out;
  for (const auto& item : value.items) out.push_back(item.text);
  return out;
}

bool flag(const Json& data, const char* key) {
  const Json* value = data.get(key);
  return value && value->boolean;
}

ProcessTreeMilpGraph graph_of(const Json& value) {
  ProcessTreeMilpGraph out;
  for (const auto& entry : value.items[0].items) {
    ProcessTreeMilpGraphNode node;
    node.id = static_cast<std::size_t>(entry.items[0].number);
    const Json& data = entry.items[1];
    node.source = flag(data, "source");
    node.sink = flag(data, "sink");
    node.split = flag(data, "is_split");
    node.join = flag(data, "is_join");
    if (const Json* iac = data.get("iac")) node.iac = static_cast<long long>(iac->number);
    if (const Json* shuffle = data.get("shuffle")) {
      for (const auto& group : shuffle->items) {
        auto& edges = node.shuffle.emplace_back();
        for (const auto& e : group.items) {
          edges.push_back({static_cast<std::size_t>(e.items[0].number), static_cast<std::size_t>(e.items[1].number),
                           static_cast<std::size_t>(e.items[2].number)});
        }
      }
    }
    out.nodes.push_back(std::move(node));
  }
  for (const auto& e : value.items[1].items) {
    ProcessTreeMilpGraphEdge edge;
    edge.from = static_cast<std::size_t>(e.items[0].number);
    edge.to = static_cast<std::size_t>(e.items[1].number);
    if (e.items[2].kind == Json::kString) edge.label = e.items[2].text;
    edge.capacity = e.items[3].number;
    edge.cost = static_cast<long long>(e.items[4].number);
    out.edges.push_back(std::move(edge));
  }
  return out;
}

std::vector<double> numbers(const Json& value) {
  std::vector<double> out;
  for (const auto& item : value.items) out.push_back(item.number);
  return out;
}

std::vector<ProcessTreeMilpRow> rows_of(const Json& value) {
  std::vector<ProcessTreeMilpRow> out;
  for (const auto& row : value.items) {
    auto& sparse = out.emplace_back();
    for (const auto& entry : row.items) {
      sparse.emplace_back(static_cast<std::size_t>(entry.items[0].number), entry.items[1].number);
    }
  }
  return out;
}

bool same_program(const ProcessTreeMilpProgram& actual, const Json& expected) {
  std::vector<bool> integer;
  for (const auto& item : expected.get("int")->items) integer.push_back(item.number != 0);
  return actual.objective == numbers(*expected.get("c")) && actual.lower == numbers(*expected.get("lb")) &&
         actual.upper == numbers(*expected.get("ub")) && actual.integer == integer &&
         actual.equalities == rows_of(*expected.get("eq")) && actual.equality_rhs == numbers(*expected.get("beq")) &&
         actual.inequalities == rows_of(*expected.get("aub")) &&
         actual.inequality_rhs == numbers(*expected.get("bub"));
}

std::vector<ProcessTreeMilpMove> moves_of(const Json& value) {
  std::vector<ProcessTreeMilpMove> out;
  for (const auto& move : value.items) {
    ProcessTreeMilpMove entry{move.items[0].text, std::nullopt};
    if (move.items[1].text != ">>") entry.model = move.items[1].text;
    out.push_back(std::move(entry));
  }
  return out;
}

bool well_formed(const std::vector<ProcessTreeMilpMove>& alignment, const std::vector<std::string>& trace) {
  if (alignment.size() != trace.size()) return false;
  for (std::size_t k = 0; k < trace.size(); ++k) {
    if (alignment[k].log != trace[k] || (alignment[k].model && *alignment[k].model != trace[k])) return false;
  }
  return true;
}

std::string text_of(const std::vector<ProcessTreeMilpMove>& moves) {
  std::string out;
  for (const auto& move : moves) out += " (" + move.log + "," + move.model.value_or(">>") + ")";
  return out;
}

std::string milp_case(const BankCase& item, Tally& tally) {
  const ProcessTree tree = parse_tree(item.tree);
  std::string failure;
  const ProcessTreeMilpGraph graph = reference_milp_process_tree_graph(tree);
  const ProcessTreeMilpGraph expected_graph = graph_of(item.graph);
  std::set<std::vector<std::string>> infeasible;  // a cached repeat answers like the first run
  ++tally.graphs;
  if (graph.nodes != expected_graph.nodes || graph.edges != expected_graph.edges) failure += "\n  graph differs";
  for (const auto& record : item.records) {
    const auto trace = strings(record.items[0]);
    const Json& expected_program = record.items[1];
    const Json& solution = record.items[2];
    const Json& cost = record.items[3];
    const auto program = reference_milp_alignment_program(graph, trace);
    if (expected_program.kind == Json::kObject) {
      ++tally.programs;
      if (!same_program(program, expected_program)) failure += "\n  program differs for a trace of " + std::to_string(trace.size());
    } else if (cost.kind == Json::kNumber) {
      ++tally.cached;  // the aligner's cache answered without a solver call
    }
    bool complete = solution.kind == Json::kArray;
    for (const auto& value : solution.items) complete = complete && value.kind == Json::kNumber;
    if (complete && expected_program.kind == Json::kObject) {
      ++tally.extracted;
      const auto read = reference_milp_alignment_from_solution(graph, trace, program, numbers(solution));
      if (!read.cost || *read.cost != cost.number || read.alignment != moves_of(record.items[4])) {
        failure += "\n  reading the reference solution gives" + text_of(read.alignment);
      }
    }
    if (cost.kind == Json::kNull) {
      ++tally.no_cost;
      const auto aligned = reference_milp_align(tree, trace);
      if (aligned.cost) failure += "\n  native gives a cost where the reference gives None";
      continue;
    }
    if (cost.kind != Json::kNumber) continue;
    if ((expected_program.kind == Json::kObject && solution.kind == Json::kNull) || infeasible.count(trace)) {
      ++tally.infeasible;
      infeasible.insert(trace);
      try {
        (void)reference_milp_align(tree, trace);
        failure += "\n  native solves an infeasible program";
      } catch (const ProcessTreeMilpReferenceError&) {
      }
      continue;
    }
    ++tally.solved;
    const auto aligned = reference_milp_align(tree, trace);
    if (!aligned.cost || std::abs(*aligned.cost - cost.number) > 1e-6 || !well_formed(aligned.alignment, trace)) {
      failure += "\n  native cost " + (aligned.cost ? std::to_string(*aligned.cost) : std::string("None")) +
                 " vs " + std::to_string(cost.number) + text_of(aligned.alignment);
    } else if (aligned.alignment == moves_of(record.items[4])) {
      ++tally.same_alignment;
    } else {
      ++tally.tied_alignment;
    }
  }
  std::vector<std::vector<std::string>> traces;
  for (const auto& record : item.records) traces.push_back(strings(record.items[0]));
  if (item.apply.kind == Json::kString) {
    ++tally.apply_raises;
    try {
      (void)reference_milp_alignments(tree, traces);
      failure += "\n  native apply does not raise";
    } catch (const ProcessTreeMilpReferenceError&) {
    }
    return failure;
  }
  std::vector<ProcessTreeMilpAlignment> applied;
  try {
    applied = reference_milp_alignments(tree, traces);
  } catch (const ProcessTreeMilpReferenceError&) {
    ++tally.apply_infeasible;  // the empty trace's program is infeasible
    return failure;
  }
  for (std::size_t k = 0; k < traces.size(); ++k) {
    ++tally.applied;
    const Json& expected = item.apply.items[k];
    const double cost = applied[k].cost.value_or(std::nan(""));
    if (std::abs(cost - expected.items[0].number) > 1e-6 ||
        std::abs(applied[k].fitness - expected.items[2].number) > 1e-6) {
      failure += "\n  apply trace " + std::to_string(k) + ": cost " + std::to_string(cost) + " fitness " +
                 std::to_string(applied[k].fitness);
    }
  }
  return failure;
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " MILP-BANK.txt.gz\n";
    return 2;
  }
  Tally tally;
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    std::string failure;
    try {
      failure = milp_case(item, tally);
    } catch (const std::exception& error) {
      failure = std::string("\n  raised ") + error.what();
    }
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << " " << item.tree << ":" << failure << '\n';
  }
  std::cout << "milp: " << tally.graphs << " graphs, " << tally.programs << " programs, " << tally.extracted
            << " solutions read, " << tally.solved << " native optima (" << tally.same_alignment
            << " same alignment, " << tally.tied_alignment << " tied), " << tally.cached << " cached, "
            << tally.no_cost << " without cost, " << tally.infeasible << " infeasible; apply: " << tally.applied
            << " traces, " << tally.apply_raises << " raising, " << tally.apply_infeasible
            << " with an infeasible floor; " << failures << " failing cases\n";
  return failures ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
