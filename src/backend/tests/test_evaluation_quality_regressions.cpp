// Regressions against captured pm4py 2.7.22 results for model-quality and
// model-to-log comparison (fixtures under tests/fixtures/quality_reference,
// generated from random pm4py process trees; each expectation was stable
// across two PYTHONHASHSEED values in fresh reference processes).
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "minera/evaluation.hpp"
#include "minera/footprints.hpp"
#include "minera/pnml.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/ptml.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {
using namespace minera;
namespace fs = std::filesystem;

fs::path fixtures;
std::string export_binary;

bool close(double left, double right) {
  return std::abs(left - right) <= 1e-12 * std::max(1.0, std::abs(right));
}

std::string format(const ProcessTree& tree, const ProcessTreeNode& node) {
  using O = ProcessTreeOperator;
  if (node.op == O::kActivity) return "'" + tree.activities()[node.activity] + "'";
  if (node.op == O::kTau) return "tau";
  const std::string op = node.op == O::kSequence ? "->"
                         : node.op == O::kExclusive ? "X"
                         : node.op == O::kParallel ? "+"
                         : node.op == O::kLoop ? "*"
                         : node.op == O::kInclusiveChoice ? "O" : "<>";
  std::string out = op + "( ";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    if (i) out += ", ";
    out += format(tree, node.children[i]);
  }
  return out + " )";
}

PnmlDocument document(const std::string& name) {
  PnmlImportOptions options;
  return import_pnml_document(fixtures / name, options);
}

std::string run_export(const std::vector<std::string>& arguments) {
  const auto output = fs::temp_directory_path() /
                      ("minera_quality_regression_" + std::to_string(std::rand()) + ".json");
  std::string command = "\"" + export_binary + "\"";
  for (const auto& argument : arguments) command += " \"" + argument + "\"";
  command += " -o \"" + output.string() + "\"";
#ifdef _WIN32
  // cmd /c strips the first and last quote of a line that starts with one;
  // an outer pair keeps the quoted program path and arguments intact.
  command = "\"" + command + "\"";
#endif
  const int status = std::system(command.c_str());
  assert(status == 0);
  std::string text;
  {
    std::ifstream input(output);
    text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  }
  // Closed first: Windows cannot remove a file that is still open, and
  // fs::remove throws there (review AY3).
  fs::remove(output);
  return text;
}

double number_after(const std::string& text, const std::string& anchor, const std::string& key) {
  const auto at = text.find(anchor);
  assert(at != std::string::npos);
  const auto field = text.find("\"" + key + "\": ", at);
  assert(field != std::string::npos);
  return std::stod(text.substr(field + key.size() + 4));
}

// Align-ETConformance on an imported PNML scans eventually enabled labels in
// PNML-ID order. The CLI used to pass the bare net (index order).
void align_etconformance_uses_pnml_ids() {
  const auto log = import_xes_event_log(fixtures / "c22_log.xes");
  const auto model = document("c22_disc.pnml");
  const auto native = evaluate_precision(log, model, PrecisionVariant::ALIGN_ETCONFORMANCE, {});
  assert(native.computed && close(native.precision, 0.9428571428571428));
  if (export_binary.empty()) return;
  const auto json = run_export({(fixtures / "c22_log.xes").string(), "--check-pnml",
                                (fixtures / "c22_disc.pnml").string(), "--no-alignments",
                                "--evaluation-family", "precision", "--precision-variant",
                                "align_etconformance"});
  const auto evaluation = json.find("\"evaluation\"");
  assert(evaluation != std::string::npos);
  const auto imported = json.find("\"imported\": {\"precision\"", evaluation);
  assert(imported != std::string::npos);
  assert(close(number_after(json.substr(imported), "\"variant\"", "precision"),
               0.9428571428571428));
}

// pm4py compares trace-variant probabilities; equal trace counts used to give
// a distance scaled by the count (5.8333 instead of 0.3889 here).
void emd_compares_variant_probabilities() {
  const auto left = import_xes_event_log(fixtures / "c19_log.xes");
  const auto right = import_xes_event_log(fixtures / "c19_log2.xes");
  const auto result = evaluate_emd(derive_stochastic_language(left, true),
                                   derive_stochastic_language(right, true));
  assert(result.computed && std::abs(result.distance - 0.3888888888888889) < 1e-9);
  if (export_binary.empty()) return;
  const auto json = run_export({(fixtures / "c19_log.xes").string(), "--no-alignments",
                                "--emd-reference", (fixtures / "c19_log2.xes").string()});
  assert(std::abs(number_after(json, "\"earth_movers_distance\"", "distance") -
                  0.3888888888888889) < 1e-9);
}

// The reference's normalized Levenshtein divides by zero for two empty traces,
// which every PYEMD matrix over a language with an empty trace contains.
void emd_rejects_empty_trace_like_reference() {
  const auto log = import_xes_event_log(fixtures / "c00_log.xes");
  const auto language = derive_stochastic_language(log, true);
  const auto result = evaluate_emd(language, language);
  assert(!result.computed && std::isnan(result.distance));
  assert(result.error == "normalized Levenshtein distance is undefined for two empty traces");
  StochasticLanguage with_empty{{{{}, 0.5}, {{"a"}, 0.5}}, 1.0};
  StochasticLanguage plain{{{{"a"}, 1.0}}, 1.0};
  assert(!evaluate_emd(plain, with_empty).computed);
  assert(evaluate_emd(plain, plain).computed);
  EmdOptions exact;
  exact.variant = EmdVariant::EXACT;
  assert(evaluate_emd(plain, with_empty, exact).computed);
}

std::vector<std::uint64_t> marking(const PnmlDocument& model) {
  std::vector<std::uint64_t> out;
  for (const auto& tokens : model.initial_marking) out.push_back(static_cast<std::uint64_t>(tokens));
  return out;
}

std::set<FootprintPair> pairs(std::initializer_list<FootprintPair> values) { return values; }

// Reachability footprints: symmetric sequence pairs (self pairs included) are
// parallel, and the eventually-enabled scan follows the reference worklist.
void petri_footprints_follow_reference() {
  {
    const auto model = document("loop_ab.pnml");  // *( 'a', 'b' )
    FootprintsDiscoveryOptions options;
    options.initial_marking = marking(model);
    options.transition_names = model.transition_ids;
    const auto fp = discover_footprints(model.net, options);
    assert(fp.sequence.empty());
    assert(fp.parallel == pairs({{"a", "b"}, {"b", "a"}}));
  }
  const auto model = document("c00_disc.pnml");
  FootprintsDiscoveryOptions options;
  options.initial_marking = marking(model);
  options.transition_names = model.transition_ids;
  const auto fp = discover_footprints(model.net, options);
  assert(fp.sequence == pairs({{"a", "zz_unknown"}, {"b", "a"}, {"b", "d"}, {"b", "e"},
                               {"c", "a"}, {"c", "d"}, {"c", "e"}, {"e", "zz_unknown"}}));
  assert(fp.parallel == pairs({{"a", "d"}, {"a", "e"}, {"d", "a"}, {"d", "d"},
                               {"d", "e"}, {"e", "a"}, {"e", "d"}, {"e", "e"}}));
  assert(fp.start_activities == (std::set<std::string>{"b", "c"}));
  const auto log = discover_footprints(import_xes_event_log(fixtures / "c00_log.xes"));
  assert(close(fp_precision(log, fp), 0.5625));
}

// The reference PTML importer ends with tree_sort (choice/parallel children by
// label-digest sum), which reduction and alignments then observe.
void ptml_import_sorts_like_reference() {
  const auto tree = import_ptml(fixtures / "c00_tree.ptml");
  assert(format(tree, tree.root()) == "+( X( X( tau, 'a' ), 'c' ), ->( 'b', *( 'e', 'd' ) ) )");
  Trace trace;
  for (const char* activity : {"b", "e", "a", "zz_unknown"}) {
    Event event;
    event.activity = activity;
    trace.events.push_back(event);
  }
  const auto reduced = reduce_process_tree(tree, trace);
  assert(format(reduced, reduced.root()) == "+( X( tau, 'a', 'c' ), ->( 'b', *( 'e', 'd' ) ) )");
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " FIXTURE_DIR [MINERA_EXPORT]\n";
    return 2;
  }
  fixtures = argv[1];
  if (argc > 2) export_binary = argv[2];
  try {
    align_etconformance_uses_pnml_ids();
    emd_compares_variant_probabilities();
    emd_rejects_empty_trace_like_reference();
    petri_footprints_follow_reference();
    ptml_import_sorts_like_reference();
  } catch (const std::exception& error) {
    std::cerr << "unexpected exception: " << error.what() << "\n";
    return 1;
  }
  std::cout << "evaluation quality regressions passed\n";
  return 0;
}
