// Fixture matrix from the 61ebeee0 parity recheck: selector routing through
// the evaluators, usage comparison and miners (J1, J4), physical parallel-arc
// usage (J10), forward and backwards replay (J24), and the extended marking
// equation with repeated split points (J23). Expectations are the saved public
// reference outputs in recheck_61ebeee0_cases.inc; the first argument is the
// fixture directory.
#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/network_analysis.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "minera/pnml.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "../tools/pnml_model_options.hpp"
#include "../tools/replay_input_options.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct SelectorCase {
  const char* fixture;
  const char* mode;
  const char* variant;
  const char* key;
  const char* projection;
  const char* expected;
};

struct ReplayCase {
  const char* log;
  const char* model;
  const char* variant;
  const char* expected;
};

struct AnalysisCase {
  const char* family;
  const char* model;
  const char* trace;
  const char* splits;
  std::size_t max_k;
  const char* expected;
};

struct UsageCase {
  const char* log1;
  const char* log2;
  const char* model;
  const char* expected;
};

struct OrgAttr {
  const char* key;
  const char* value;
  char kind;  // i int64, u uint64, f double, b bool, d date, s string, n None, a pd.NA
};

struct OrgEvent {
  const char* activity;
  const char* resource;
  const char* timestamp;
  const char* lifecycle;
  std::vector<OrgAttr> attributes;
};

struct OrgTrace {
  const char* id;
  std::vector<OrgAttr> attributes;
  std::vector<OrgEvent> events;
};

struct OrgCase {
  const char* id;
  const char* family;
  std::vector<OrgTrace> log;
  std::vector<std::pair<const char*, const char*>> params;
  bool performance;
  const char* metric;
  const char* activity;
  const char* resource;
  const char* resource2;
  const char* t1;
  const char* t2;
  const char* expected;
};

#include "recheck_61ebeee0_cases.inc"

std::filesystem::path root;
int failures = 0;

std::string number(double value) {
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

std::vector<std::string> tokens(const std::string& text) {
  // Brackets and commas are separators of their own, so "[9.0]" and "[9]"
  // compare as numbers.
  std::string spaced;
  for (const char c : text) {
    if (c == '[' || c == ']' || c == ',') spaced += std::string(" ") + c + " ";
    else spaced += c;
  }
  std::vector<std::string> out;
  std::istringstream in(spaced);
  for (std::string token; in >> token;) out.push_back(token);
  return out;
}

// Whitespace-separated tokens must match; numeric tokens compare within the
// 1e-12 tolerance the recheck used.
bool same(const std::string& expected, const std::string& actual) {
  const auto a = tokens(expected), b = tokens(actual);
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i] == b[i]) continue;
    char* end_a = nullptr;
    char* end_b = nullptr;
    const double x = std::strtod(a[i].c_str(), &end_a);
    const double y = std::strtod(b[i].c_str(), &end_b);
    if (*end_a || *end_b || end_a == a[i].c_str() || end_b == b[i].c_str()) return false;
    if (std::fabs(x - y) > 1e-12 * std::max({1.0, std::fabs(x), std::fabs(y)})) return false;
  }
  return true;
}

void check(const std::string& name, const std::string& expected, const std::string& actual) {
  if (same(expected, actual)) return;
  ++failures;
  std::cerr << "FAIL " << name << "\n  expected: " << expected << "\n  actual:   " << actual << '\n';
}

minera::PnmlDocument model(const std::string& file) {
  minera::PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.require_workflow_boundaries = false;
  return minera::import_pnml_document(root / file, options);
}

minera::EventLog xes(const std::string& file, const std::string& projection = {}) {
  minera::XesImportOptions options;
  if (!projection.empty()) options.activity_attribute = projection;
  return minera::import_xes_event_log(root / file, options);
}

std::string labels(const minera::PetriNet& net) {
  std::vector<std::string> names(net.activities().begin(), net.activities().end());
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  std::string out;
  for (const auto& name : names) out += (out.empty() ? "" : ",") + name;
  return out;
}

std::string selector(const SelectorCase& c) {
  const std::string mode = c.mode, variant = c.variant;
  const auto log = xes(std::string(c.fixture) + ".xes", c.projection);
  if (mode == "ilp") {
    minera::ILPMinerOptions options;
    options.activity_key = c.key;
    const auto mined = minera::mine_ilp(log, options);
    return mined.diagnostics.computed ? labels(mined.net) : "error";
  }
  if (mode == "genetic") {
    minera::GeneticMinerOptions options;
    options.activity_key = c.key;
    options.population_size = 4;
    options.generations = 0;
    options.elitism_min_sample = 1;
    options.seed = 75431;
    const auto mined = minera::mine_genetic(log, options);
    return mined.diagnostics.computed ? labels(mined.net) : "error";
  }
  const auto doc = model("selector-route.pnml");
  if (mode == "precision") {
    minera::PrecisionOptions options;
    options.activity_attribute = c.key;
    options.max_states = 5000;
    const auto kind = variant == "align" ? minera::PrecisionVariant::ALIGN_ETCONFORMANCE
        : variant == "automaton" ? minera::PrecisionVariant::AUTOMATON_AFTER_ALIGN
        : minera::PrecisionVariant::ETCONFORMANCE_TOKEN;
    const auto result = minera::evaluate_precision(log, doc, kind, options);
    return result.computed && result.error.empty() ? number(result.precision) : "error";
  }
  if (mode == "fitness") {
    minera::ReplayFitnessOptions options;
    options.activity_attribute = c.key;
    options.alignment_options.max_states = 5000;
    const auto kind = variant == "align" ? minera::ReplayFitnessVariant::ALIGNMENT_BASED
                                         : minera::ReplayFitnessVariant::TOKEN_BASED;
    const auto result = minera::evaluate_replay_fitness(log, doc, kind, options);
    if (!result.computed || !result.error.empty()) return "error";
    return number(result.percentage_fit_traces) + " " + number(result.average_trace_fitness) +
           " " + number(result.log_fitness);
  }
  // usage_reference
  const auto other = xes(std::string(c.fixture) + "-other.xes", c.projection);
  auto options = minera::tools::pnml_comparison_options(doc);
  options.activity_attribute = c.key;
  const auto usage = minera::compare_petri_usage_reference(doc.net, log, other, options);
  std::string out;
  for (const auto& t : usage.transitions)
    out += "t" + std::to_string(t.id) + ":" + std::to_string(t.log1_occurrences) + "," +
           std::to_string(t.log2_occurrences) + " ";
  return out + "arcs=" + std::to_string(usage.arcs.size());
}

std::string replay(const ReplayCase& c) {
  const auto doc = model(c.model);
  const auto log = xes(c.log);
  minera::TokenReplayOptions base;
  if (std::string(c.variant) == "backwards") base.variant = minera::TokenReplayVariant::kBackwards;
  const auto options = minera::tools::default_replay_input_options(
      log, minera::tools::pnml_replay_options(doc, base), false);
  const auto traces = minera::replay_token_traces(log, doc.net, options);
  const auto names = [&](const std::vector<std::size_t>& ids) {
    std::string out = "[";
    for (std::size_t i = 0; i < ids.size(); ++i)
      out += (i ? "," : "") + doc.transition_ids.at(ids[i]);
    return out + "]";
  };
  std::string out;
  for (const auto& t : traces) {
    if (!out.empty()) out += " | ";
    out += std::string(t.trace_is_fit ? "true" : "false") + " " + number(t.trace_fitness) + " " +
           t.produced.decimal() + " " + t.consumed.decimal() + " " + t.missing.decimal() + " " +
           t.remaining.decimal() + " " + names(t.activated_transitions) + " " +
           names(t.transitions_with_problems);
  }
  return out;
}

std::vector<std::string> split(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream in(text);
  for (std::string part; std::getline(in, part, '|');) out.push_back(part);
  return out;
}

std::string analysis(const AnalysisCase& c) {
  const auto doc = model(c.model);
  minera::Marking initial, final;
  for (const auto x : doc.initial_marking) initial.push_back(static_cast<std::int64_t>(x));
  for (const auto x : doc.final_marking) final.push_back(static_cast<std::int64_t>(x));
  if (std::string(c.family) == "woflan") {
    minera::SoundnessOptions options;
    options.variant = minera::SoundnessVariant::kWoflan;
    options.initial_marking = initial;
    options.final_marking = final;
    options.max_states = 5000;
    options.invariant_search_limit = 5000;
    const auto result = minera::analyze_soundness(doc.net, options);
    if (!result.computed) return "error";
    return result.status == minera::SoundnessStatus::SOUND ? "true" : "false";
  }
  minera::ExtendedMarkingEquationOptions options;
  options.initial_marking = initial;
  options.final_marking = final;
  options.realizability_limit = 10000;
  options.max_k_value = c.max_k;
  for (const auto& s : split(c.splits)) options.split_indices.push_back(std::stoull(s));
  const auto result = minera::solve_extended_marking_equation(split(c.trace), doc.net, options);
  if (!result.computed) return "error";
  return result.feasible ? number(result.objective) : "none";
}

std::string physical_usage(const UsageCase& c) {
  const auto doc = model(c.model);
  const auto options = minera::tools::pnml_comparison_options(doc);
  const auto usage =
      minera::compare_petri_usage_reference(doc.net, xes(c.log1), xes(c.log2), options);
  std::vector<std::string> rows;
  const auto row = [&](const std::string& kind, const std::string& element, const auto& r) {
    rows.push_back(kind + " " + element + " " + std::to_string(r.log1_occurrences) + " " +
                   std::to_string(r.log2_occurrences) + " " + std::to_string(r.total_occurrences) +
                   " " + r.label + " " + number(r.direction) + " " + r.color);
  };
  for (const auto& r : usage.arcs) {
    const auto p = "p" + std::to_string(r.place), t = "t" + std::to_string(r.transition);
    row("arc", r.input ? p + "->" + t : t + "->" + p, r);
  }
  for (const auto& r : usage.places) row("place", "p" + std::to_string(r.id), r);
  for (const auto& r : usage.transitions) row("transition", "t" + std::to_string(r.id), r);
  std::sort(rows.begin(), rows.end());
  std::string out;
  for (const auto& r : rows) out += (out.empty() ? "" : " | ") + r;
  return out;
}

minera::EventAttribute org_attribute(const OrgAttr& a) {
  minera::EventAttribute out;
  out.key = a.key;
  out.value = a.value;
  switch (a.kind) {
    case 'i':
      out.kind = minera::AttributeKind::kInt;
      out.typed_value = static_cast<std::int64_t>(std::stoll(a.value));
      break;
    case 'u':
      out.kind = minera::AttributeKind::kInt;
      out.typed_value = static_cast<std::uint64_t>(std::stoull(a.value));
      break;
    case 'f':
      out.kind = minera::AttributeKind::kFloat;
      out.typed_value = std::string(a.value) == "nan" ? std::nan("") : std::stod(a.value);
      break;
    case 'b':
      out.kind = minera::AttributeKind::kBoolean;
      out.typed_value = std::string(a.value) == "true";
      break;
    case 'd':
      out.kind = minera::AttributeKind::kDate;
      out.typed_value = minera::NativeDate{a.value};
      break;
    case 'n':
      out.is_null = true;
      break;
    case 'a':
      out.typed_value = minera::NativeNA{};
      break;
    default:
      break;
  }
  return out;
}

minera::EventLog org_log(const OrgCase& c) {
  minera::EventLog log;
  for (const auto& t : c.log) {
    minera::Trace trace;
    trace.case_id = t.id;
    for (const auto& a : t.attributes) trace.attributes.push_back(org_attribute(a));
    for (const auto& e : t.events) {
      minera::Event event;
      event.case_id = t.id;
      event.activity = e.activity;
      event.resource = e.resource;
      event.timestamp = e.timestamp;
      event.lifecycle = e.lifecycle;
      event.case_id_state = event.activity_state = event.resource_state =
          event.timestamp_state = minera::FirstClassAttributeState::kPresent;
      if (!event.lifecycle.empty())
        event.lifecycle_state = minera::FirstClassAttributeState::kPresent;
      for (const auto& a : e.attributes) event.attributes.push_back(org_attribute(a));
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::string param(const OrgCase& c, const std::string& name, const std::string& fallback) {
  for (const auto& [key, value] : c.params)
    if (name == key) return value;
  return fallback;
}

// J1 resource-profile selectors and J5/J6 network sorting and missing values.
std::string organizational(const OrgCase& c) {
  const auto log = org_log(c);
  if (std::string(c.family) == "profile") {
    minera::ResourceProfilesOptions options;
    options.metric = minera::resource_profile_metric_from_string(c.metric);
    options.activity_attribute = param(c, "activity_key", options.activity_attribute);
    options.resource_attribute = param(c, "resource_key", options.resource_attribute);
    options.timestamp_attribute = param(c, "timestamp_key", options.timestamp_attribute);
    options.t1 = c.t1;
    options.t2 = c.t2;
    options.resource = c.resource;
    options.activity = c.activity;
    options.resource2 = c.resource2;
    options.resource_given = options.activity_given = options.resource2_given = true;
    const auto result = minera::discover_resource_profiles(log, options);
    if (result.rows.size() != 1) return "error";
    return number(result.rows.front().value);
  }
  minera::NetworkAnalysisOptions options;
  options.sorting_attribute = param(c, "sorting_column", options.sorting_attribute);
  options.source_node_attribute = param(c, "node_column_source", options.source_node_attribute);
  options.target_node_attribute = param(c, "node_column_target", options.target_node_attribute);
  options.edge_attribute = param(c, "edge_column", options.edge_attribute);
  options.out_relation_attribute = param(c, "out_column", options.out_relation_attribute);
  options.in_relation_attribute = param(c, "in_column", options.in_relation_attribute);
  options.include_performance = c.performance;
  const auto result = minera::discover_network_analysis(log, options);
  std::vector<std::string> edges;
  for (const auto& e : result.edges) {
    std::string perf;
    for (const auto v : e.performance) perf += (perf.empty() ? "" : ",") + number(v);
    edges.push_back(e.source + " " + e.target + " " + e.activity + " " + std::to_string(e.count) +
                    " [" + perf + "]");
  }
  std::sort(edges.begin(), edges.end());
  std::string out;
  for (const auto& e : edges) out += (out.empty() ? "" : " | ") + e;
  return out;
}

template <class F>
std::string guarded(F&& f) {
  try {
    return f();
  } catch (const std::exception&) {
    return "error";
  }
}

// J10: one transition with parallel input arcs of weights 2 and 5 and an
// output of weight 7. Source-place usage follows the reference 10/5/15.
void parallel_minimized() {
  const auto doc = model("parallel-minimized.pnml");
  const auto options = minera::tools::pnml_comparison_options(doc);
  const auto usage = minera::compare_petri_usage_reference(
      doc.net, xes("parallel-min-first.xes"), xes("parallel-min-second.xes"), options);
  std::string places;
  for (const auto& p : usage.places)
    places += p.label + " ";
  check("J10 parallel-minimized places", "(10/5/15) (14/7/21)", places);
  check("J10 parallel-minimized arcs", "3", std::to_string(usage.arcs.size()));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <fixture-directory>\n";
    return 2;
  }
  root = argv[1];
  std::size_t total = 0;
  for (const auto& c : kSelectorCases) {
    ++total;
    check(std::string("selector ") + c.fixture + " " + c.mode + " " + c.variant + " key='" +
              c.key + "'",
          c.expected, guarded([&] { return selector(c); }));
  }
  for (const auto& c : kReplayCases) {
    ++total;
    check(std::string("replay ") + c.log + " " + c.model + " " + c.variant, c.expected,
          guarded([&] { return replay(c); }));
  }
  for (const auto& c : kAnalysisCases) {
    ++total;
    check(std::string("analysis ") + c.family + " " + c.model + " trace=" + c.trace +
              " splits=" + c.splits + " k=" + std::to_string(c.max_k),
          c.expected, guarded([&] { return analysis(c); }));
  }
  for (const auto& c : kPhysicalUsageCases) {
    ++total;
    check(std::string("physical usage ") + c.model, c.expected,
          guarded([&] { return physical_usage(c); }));
  }
  for (const auto& c : kOrgCases) {
    ++total;
    check(std::string("organizational ") + c.id, c.expected,
          guarded([&] { return organizational(c); }));
  }
  ++total;
  parallel_minimized();
  if (failures) {
    std::cerr << failures << " of " << total << " recheck fixture cases differ\n";
    return 1;
  }
  std::cout << "recheck fixture cases passed: " << total << '\n';
  return 0;
}
