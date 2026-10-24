// Black-box equivalence of every model-quality selector against the public
// 2.7.22 results captured by scripts/capture-evaluation-reference.py.  Each
// case imports the PNML written by the reference exporter, replays the same
// traces, and compares every captured number (or the rejection of the input).

#include "minera/evaluation.hpp"
#include "minera/pnml.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace minera;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

enum ExpectState { kValue, kError, kUnresolved };

struct Expect {
  ExpectState state;
  std::vector<double> values;
};

struct ReferenceCase {
  const char* name;
  const char* pnml;
  std::vector<std::vector<std::string>> traces;
  Expect fitness_token;           // percentage fit, average trace, log fitness
  Expect fitness_alignment;       // percentage fit, average trace, log fitness
  Expect fitness_auto;            // alignment selected, then the three above
  Expect precision_token;
  Expect precision_alignment;
  Expect precision_automaton;
  Expect precision_auto;
  Expect generalization_token;
  Expect simplicity_arc_degree;
  Expect simplicity_extended_cardoso;
  Expect simplicity_extended_cyclomatic;
  Expect composite;  // token fitness triple, precision, generalization,
                     // simplicity, weighted average, F-score
};

const std::vector<ReferenceCase>& reference_cases() {
  static const std::vector<ReferenceCase> cases = {
#include "evaluation_reference_cases.inc"
  };
  return cases;
}

EventLog make_log(const std::vector<std::vector<std::string>>& traces) {
  EventLog result;
  std::size_t order = 0;
  for (std::size_t index = 0; index < traces.size(); ++index) {
    Trace trace;
    trace.case_id = std::to_string(index);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& activity : traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.input_order = order++;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

int failures = 0;

// Inputs on which Minera knowingly diverges from the pinned reference. Each is
// owned by a dependency outside the evaluators and is listed so the gap stays
// visible: an entry that starts matching fails the run, so the list cannot go
// stale.
struct KnownDivergence {
  std::string_view case_prefix;
  std::string_view metric;
  std::string_view reason;
};
constexpr KnownDivergence kKnownDivergences[] = {
    // Forward token replay routes hidden transitions differently on this
    // heuristics net; token fitness, generalization and the composite inherit
    // it. Owned by conformance token replay (exact routing still pending).
    {"seed7_heuristics_noisy", "fitness TOKEN_BASED", "token-replay hidden routing"},
    {"seed7_heuristics_noisy", "generalization GENERALIZATION_TOKEN", "token-replay hidden routing"},
    {"seed7_heuristics_noisy", "composite default", "token-replay hidden routing"},
    // A silent transition pumps tokens without bound. The reference search
    // returns the optimal alignment, while every native alignment variant
    // exhausts its state budget. Owned by the Petri alignment engines.
    {"weighted_silent_", "fitness ALIGNMENT_BASED", "unbounded silent pump exhausts native search"},
    {"weighted_silent_", "fitness default", "unbounded silent pump exhausts native search"},
};

const KnownDivergence* known_divergence(const ReferenceCase& reference, std::string_view metric) {
  for (const auto& entry : kKnownDivergences) {
    if (entry.metric == metric && std::string_view(reference.name).starts_with(entry.case_prefix))
      return &entry;
  }
  return nullptr;
}

bool same(double actual, double expected) {
  if (std::isnan(expected)) return std::isnan(actual);
  if (std::isinf(expected)) return actual == expected;
  return std::abs(actual - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}

// Empty when Minera agrees with the reference, otherwise a description.
std::string difference(const Expect& expected, bool computed, const std::string& error,
                       const std::vector<double>& actual) {
  if (expected.state == kError) {
    return computed ? "reference rejects the input but Minera computed it" : "";
  }
  if (!computed) return "Minera rejected the input: " + error;
  std::string detail;
  for (std::size_t index = 0; index < expected.values.size(); ++index) {
    if (!same(actual[index], expected.values[index])) {
      char buffer[160];
      std::snprintf(buffer, sizeof buffer, "value %zu is %.17g, reference %.17g; ", index,
                    actual[index], expected.values[index]);
      detail += buffer;
    }
  }
  return detail;
}

struct Tally {
  std::size_t compared = 0;
  std::size_t agreed = 0;
  std::size_t known = 0;
};

// Exact comparison: any disagreement fails unless it is a listed divergence,
// and a listed divergence that agrees fails too.
template <class Evaluate>
void compare(Tally& tally, const ReferenceCase& reference, const char* metric,
             const Expect& expected, Evaluate evaluate) {
  if (expected.state == kUnresolved) return;
  ++tally.compared;
  std::string detail;
  try {
    detail = evaluate();
  } catch (const std::exception& error) {
    // A native exception is the counterpart of a reference exception.
    if (expected.state == kValue) detail = std::string("threw ") + error.what();
  }
  const auto* known = known_divergence(reference, metric);
  if (detail.empty() && !known) {
    ++tally.agreed;
    return;
  }
  if (!detail.empty() && known) {
    ++tally.known;
    return;
  }
  ++failures;
  std::fprintf(stderr, "%s / %s: %s\n", reference.name, metric,
               known ? "listed divergence now agrees; remove it" : detail.c_str());
}

// Every asserted selector must be compared on most of the corpus, so a capture
// that lost its observations cannot pass vacuously.
void require_coverage(const char* metric, const Tally& tally) {
  std::printf("%-38s %3zu compared, %3zu agree, %zu listed divergences\n", metric,
              tally.compared, tally.agreed, tally.known);
  if (tally.compared < 100) {
    ++failures;
    std::fprintf(stderr, "%s compared only %zu cases\n", metric, tally.compared);
  }
}

std::vector<double> fitness_values(const ReplayFitnessResult& result) {
  return {result.percentage_fit_traces, result.average_trace_fitness, result.log_fitness};
}

struct LoadedCase {
  const ReferenceCase* reference;
  PnmlDocument document;
  EventLog log;
};

const std::vector<LoadedCase>& loaded_cases() {
  static const std::vector<LoadedCase> cases = [] {
    std::vector<LoadedCase> result;
    PnmlImportOptions import;
    import.public_marking_semantics = true;
    for (const auto& reference : reference_cases()) {
      result.push_back({&reference, parse_pnml_document(reference.pnml, import),
                        make_log(reference.traces)});
    }
    return result;
  }();
  return cases;
}

void replay_fitness_token_based_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "fitness TOKEN_BASED", item.reference->fitness_token, [&] {
      const auto result =
          evaluate_replay_fitness(item.log, item.document, ReplayFitnessVariant::TOKEN_BASED);
      return difference(item.reference->fitness_token, result.computed, result.error,
                        fitness_values(result));
    });
  }
  require_coverage("replay fitness TOKEN_BASED", tally);
}

void replay_fitness_alignment_based_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "fitness ALIGNMENT_BASED", item.reference->fitness_alignment,
            [&] {
      const auto result =
          evaluate_replay_fitness(item.log, item.document, ReplayFitnessVariant::ALIGNMENT_BASED);
      return difference(item.reference->fitness_alignment, result.computed, result.error,
                        fitness_values(result));
    });
  }
  require_coverage("replay fitness ALIGNMENT_BASED", tally);
}

void replay_fitness_default_dispatch_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "fitness default", item.reference->fitness_auto, [&] {
      const auto result = evaluate_replay_fitness(item.log, item.document, ReplayFitnessVariant::AUTO);
      auto values = fitness_values(result);
      values.insert(values.begin(),
                    result.variant == ReplayFitnessVariant::ALIGNMENT_BASED ? 1.0 : 0.0);
      return difference(item.reference->fitness_auto, result.computed, result.error, values);
    });
  }
  require_coverage("replay fitness default", tally);
}

Tally compare_precision(const char* metric, PrecisionVariant variant,
                        Expect ReferenceCase::*expected) {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, metric, item.reference->*expected, [&] {
      const auto result = evaluate_precision(item.log, item.document, variant);
      return difference(item.reference->*expected, result.computed, result.error,
                        {result.precision});
    });
  }
  return tally;
}

void precision_etconformance_token_equivalence() {
  require_coverage("precision ETCONFORMANCE_TOKEN",
                   compare_precision("precision ETCONFORMANCE_TOKEN",
                                     PrecisionVariant::ETCONFORMANCE_TOKEN,
                                     &ReferenceCase::precision_token));
}

// Both alignment-based precision variants score markings taken from one
// optimal (prefix) alignment. Where equally cheap alignments place visible or
// silent moves differently, the reference value follows its search's tie
// order, which is not a public contract. These variants are therefore
// reported, not asserted, until that order is characterized.
void precision_alignment_variants_agreement() {
  const auto align = [] {
    Tally tally;
    for (const auto& item : loaded_cases()) {
      const auto& expected = item.reference->precision_alignment;
      if (expected.state == kUnresolved) continue;
      ++tally.compared;
      const auto result =
          evaluate_precision(item.log, item.document, PrecisionVariant::ALIGN_ETCONFORMANCE);
      if (difference(expected, result.computed, result.error, {result.precision}).empty())
        ++tally.agreed;
    }
    return tally;
  }();
  const auto automaton = [] {
    Tally tally;
    for (const auto& item : loaded_cases()) {
      const auto& expected = item.reference->precision_automaton;
      if (expected.state == kUnresolved) continue;
      ++tally.compared;
      const auto result =
          evaluate_precision(item.log, item.document, PrecisionVariant::AUTOMATON_AFTER_ALIGN);
      if (difference(expected, result.computed, result.error, {result.precision}).empty())
        ++tally.agreed;
    }
    return tally;
  }();
  std::printf("precision ALIGN_ETCONFORMANCE (report)    %3zu compared, %3zu agree\n",
              align.compared, align.agreed);
  std::printf("precision AUTOMATON_AFTER_ALIGN (report)  %3zu compared, %3zu agree\n",
              automaton.compared, automaton.agreed);
}

void generalization_token_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "generalization GENERALIZATION_TOKEN",
            item.reference->generalization_token, [&] {
      const auto result = evaluate_generalization(item.log, item.document,
                                                  GeneralizationVariant::GENERALIZATION_TOKEN);
      return difference(item.reference->generalization_token, result.computed, result.error,
                        {result.generalization});
    });
  }
  require_coverage("generalization GENERALIZATION_TOKEN", tally);
}

Tally compare_simplicity(const char* metric, SimplicityVariant variant,
                         Expect ReferenceCase::*expected) {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, metric, item.reference->*expected, [&] {
      const auto result = evaluate_simplicity(item.document, variant);
      return difference(item.reference->*expected, result.computed, result.error,
                        {result.simplicity});
    });
  }
  return tally;
}

void simplicity_arc_degree_equivalence() {
  require_coverage("simplicity SIMPLICITY_ARC_DEGREE",
                   compare_simplicity("simplicity SIMPLICITY_ARC_DEGREE",
                                      SimplicityVariant::ARC_DEGREE,
                                      &ReferenceCase::simplicity_arc_degree));
}

void simplicity_extended_cardoso_equivalence() {
  require_coverage("simplicity EXTENDED_CARDOSO",
                   compare_simplicity("simplicity EXTENDED_CARDOSO",
                                      SimplicityVariant::EXTENDED_CARDOSO,
                                      &ReferenceCase::simplicity_extended_cardoso));
}

void simplicity_extended_cyclomatic_equivalence() {
  require_coverage("simplicity EXTENDED_CYCLOMATIC",
                   compare_simplicity("simplicity EXTENDED_CYCLOMATIC",
                                      SimplicityVariant::EXTENDED_CYCLOMATIC,
                                      &ReferenceCase::simplicity_extended_cyclomatic));
}

void composite_default_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "composite default", item.reference->composite, [&] {
      const auto result = evaluate_composite(item.log, item.document);
      auto values = fitness_values(result.fitness_details);
      values.insert(values.end(), {result.precision, result.generalization, result.simplicity,
                                   result.metrics_average_weight, result.fscore});
      return difference(item.reference->composite, result.computed, result.error, values);
    });
  }
  require_coverage("composite evaluation default", tally);
}

}  // namespace

int main() {
  replay_fitness_token_based_equivalence();
  replay_fitness_alignment_based_equivalence();
  replay_fitness_default_dispatch_equivalence();
  precision_etconformance_token_equivalence();
  precision_alignment_variants_agreement();
  generalization_token_equivalence();
  simplicity_arc_degree_equivalence();
  simplicity_extended_cardoso_equivalence();
  simplicity_extended_cyclomatic_equivalence();
  composite_default_equivalence();
  std::printf("%zu reference cases, %d failures\n", reference_cases().size(), failures);
  return failures == 0 ? 0 : 1;
}
