#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/process_tree.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path write_temp_log(const std::string& content) {
  auto path = std::filesystem::temp_directory_path() / "minera_core_test.csv";
  std::ofstream output(path);
  output << content;
  return path;
}

void imports_csv_and_discovers_directly_follows_graph() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:01:00\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,C,2026-01-01T00:01:00\n"
      "2,B,2026-01-01T00:02:00\n");

  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 2);
  assert(log.event_count() == 5);

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().size() == 3);
  assert(graph.edges().size() == 3);
  assert(graph.activity_count("A") == 2);
  assert(graph.activity_count("B") == 2);
  assert(graph.activity_count("C") == 1);
  assert(graph.start_activity_count("A") == 2);
  assert(graph.end_activity_count("B") == 2);
  assert(graph.edge_count("A", "B") == 1);
  assert(graph.edge_count("A", "C") == 1);
  assert(graph.edge_count("C", "B") == 1);
  assert(graph.edge_count("B", "A") == 0);
  assert(graph.variants().size() == 2);
}

void handles_quoted_csv_fields_and_timestamp_sorting() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "\"case,1\",Done,2026-01-01T00:02:00\n"
      "\"case,1\",\"Review \"\"fast\"\"\",2026-01-01T00:01:00\n"
      "\"case,1\",Start,2026-01-01T00:00:00\n");

  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.traces.front().case_id == "case,1");
  assert(log.traces.front().events[0].activity == "Start");
  assert(log.traces.front().events[1].activity == "Review \"fast\"");
  assert(log.traces.front().events[2].activity == "Done");

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("Start", "Review \"fast\"") == 1);
  assert(graph.edge_count("Review \"fast\"", "Done") == 1);
}

void aggregates_repeated_trace_variants() {
  const auto path = write_temp_log(
      "case_id,activity\n"
      "1,A\n"
      "1,B\n"
      "2,A\n"
      "2,B\n"
      "3,A\n"
      "3,C\n");

  minera::CsvImportOptions options;
  options.timestamp_column = "missing_timestamp";

  const auto log = minera::import_csv_event_log(path, options);
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 2);
  assert(graph.edge_count("A", "C") == 1);
  assert(graph.variants().size() == 2);
  assert(graph.variants()[0].count == 2);
  assert(graph.variants()[1].count == 1);
}

minera::EventLog log_from_variants(
    const std::vector<std::vector<std::string>>& variants) {
  minera::EventLog log;
  std::size_t case_index = 0;
  for (const auto& variant : variants) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& activity : variant) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

// Renders a process tree to a canonical S-expression: ->(a, b), X(a, b),
// +(a, b), *(a, b), leaf activity names, and "tau" for silent steps.
std::string render(const minera::ProcessTree& tree,
                   const minera::ProcessTreeNode& node) {
  using Op = minera::ProcessTreeOperator;
  switch (node.op) {
    case Op::kActivity:
      return tree.activities()[node.activity];
    case Op::kTau:
      return "tau";
    default:
      break;
  }

  std::string symbol;
  switch (node.op) {
    case Op::kSequence: symbol = "->"; break;
    case Op::kExclusive: symbol = "X"; break;
    case Op::kParallel: symbol = "+"; break;
    case Op::kLoop: symbol = "*"; break;
    default: symbol = "?"; break;
  }

  std::string out = symbol + "(";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    out += render(tree, node.children[i]);
    if (i + 1 < node.children.size()) {
      out += ", ";
    }
  }
  out += ")";
  return out;
}

std::string mine_tree(const std::vector<std::vector<std::string>>& variants) {
  const auto log = log_from_variants(variants);
  const auto tree = minera::discover_process_tree(log);
  return render(tree, tree.root());
}

void inductive_miner_discovers_sequence() {
  assert(mine_tree({{"a", "b", "c"}}) == "->(a, b, c)");
}

void inductive_miner_discovers_exclusive_choice() {
  // a then either b or c then d.
  assert(mine_tree({{"a", "b", "d"}, {"a", "c", "d"}}) ==
         "->(a, X(b, c), d)");
}

void inductive_miner_discovers_parallel() {
  // b and c are concurrent between a and d.
  const auto tree = mine_tree({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}});
  assert(tree == "->(a, +(b, c), d)");
}

void inductive_miner_discovers_loop() {
  // a, then optionally repeat (b then a).
  const auto tree =
      mine_tree({{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}});
  assert(tree == "*(a, b)");
}

void inductive_miner_models_optional_skip() {
  // No cut applies, so the empty trace falls through to an explicit skip around
  // the rest of the behaviour (here a self-loop on the single activity).
  assert(mine_tree({{}, {"a"}, {"a", "a"}}) == "X(tau, *(a, tau))");
}

void inductive_miner_pushes_empty_trace_through_cut() {
  // The canonical miner detects the a->b sequence first; the empty trace rides
  // through the split and becomes an optional skip on each branch.
  assert(mine_tree({{}, {"a", "b"}}) == "->(X(tau, a), X(tau, b))");
}

void inductive_miner_falls_back_to_flower() {
  // A directly-follows cycle (a->b->c->a) has no exclusive, sequence, parallel
  // or loop cut, so it falls through to the flower model.
  const auto tree = mine_tree({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  assert(tree == "*(tau, a, b, c)");
}

}  // namespace

int main() {
  imports_csv_and_discovers_directly_follows_graph();
  handles_quoted_csv_fields_and_timestamp_sorting();
  aggregates_repeated_trace_variants();
  inductive_miner_discovers_sequence();
  inductive_miner_discovers_exclusive_choice();
  inductive_miner_discovers_parallel();
  inductive_miner_discovers_loop();
  inductive_miner_models_optional_skip();
  inductive_miner_pushes_empty_trace_through_cut();
  inductive_miner_falls_back_to_flower();
  return 0;
}
