#include "minera/trace_variant_query.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <stdexcept>

namespace {

minera::EventLog sample() {
  minera::EventLog log;
  log.traces.push_back({"a", {{"a", "A", "", "", 0, {}, ""},
                               {"a", "B", "", "", 1, {}, ""}}});
  log.traces.push_back({"b", {{"b", "A", "", "", 0, {}, ""},
                               {"b", "C", "", "", 1, {}, ""}}});
  return log;
}

void basics() {
  const auto log = sample();
  minera::TraceVariantQueryOptions options;
  options.variant = minera::TraceVariantQueryVariant::kLaplace;
  options.epsilon = 2.0;
  options.seed = 9;
  options.max_prefix_length = 2;
  options.prune_threshold = 1;
  options.max_output_traces = 50;
  const auto first = minera::anonymize_trace_variant_query(log, options);
  const auto second = minera::anonymize_trace_variant_query(log, options);
  assert(first.frequencies == second.frequencies);
  assert(first.log.trace_count() <= 50);
  for (const auto& [prefix, count] : first.frequencies) {
    (void)prefix;
    assert(count >= 1);
  }
}

void edge_cases() {
  minera::TraceVariantQueryOptions options;
  options.max_prefix_length = 0;
  auto result = minera::anonymize_trace_variant_query(sample(), options);
  assert(result.frequencies.empty() && result.log.traces.empty());
  options.max_prefix_length = 2;
  options.epsilon = 0.0;
  bool threw = false;
  try { (void)minera::anonymize_trace_variant_query(sample(), options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void sacofa_and_pruning() {
  auto log = sample();
  // Make the observed behavioural prefixes comfortably survive noise.
  for (int i = 0; i < 20; ++i) log.traces.push_back(log.traces.front());
  minera::TraceVariantQueryOptions options;
  options.variant = minera::TraceVariantQueryVariant::kSacofa;
  options.epsilon = 100.0;
  options.max_prefix_length = 2;
  options.seed = 3;
  const auto result = minera::anonymize_trace_variant_query(log, options);
  assert(result.frequencies.contains({"A", "B"}));
  assert(!result.frequencies.contains({"B", "A"}));
  // The reference-port diagnostics: four candidates per level (A, B, C and
  // the end marker after the single surviving prefix), none added back.
  assert(result.diagnostics.candidate_count == 8);
  assert(result.diagnostics.selected_violation_count == 0);
  assert(result.diagnostics.seed == 3);

  options.prune_threshold = 1000000;
  const auto pruned = minera::anonymize_trace_variant_query(log, options);
  assert(pruned.frequencies.empty());

  options.prune_threshold = 1;
  options.secure_random = true;
  const auto secure = minera::anonymize_trace_variant_query(log, options);
  assert(secure.diagnostics.secure_random);
  assert(secure.log.trace_count() <= options.max_output_traces);
  // The OS-drawn key is never reported, in diagnostics or JSON.
  assert(secure.diagnostics.seed == 0);
  assert(secure.diagnostics.candidate_count == 8);
  options.seed = 0xffffffffULL;
  for (int i = 0; i < 4; ++i) {
    const auto again = minera::anonymize_trace_variant_query(log, options);
    assert(again.diagnostics.seed == 0);
  }
  options.variant = minera::TraceVariantQueryVariant::kLaplace;
  options.seed = 5;
  const auto secure_laplace = minera::anonymize_trace_variant_query(log, options);
  assert(secure_laplace.diagnostics.seed == 0);
}

minera::EventLog two_complete_variants() {
  minera::EventLog log;
  log.traces.push_back({"ab", {{"ab", "A", "", "", 0, {}, ""},
                                {"ab", "B", "", "", 1, {}, ""}}});
  log.traces.push_back({"ac", {{"ac", "A", "", "", 0, {}, ""},
                                {"ac", "C", "", "", 1, {}, ""}}});
  return log;
}

void laplace_completion_and_depth_pruning() {
  const auto log = two_complete_variants();
  minera::TraceVariantQueryOptions complete;
  complete.variant = minera::TraceVariantQueryVariant::kLaplace;
  complete.epsilon = 1e9;
  complete.max_prefix_length = 3;  // two events plus the completion marker
  complete.seed = 17;
  const auto released =
      minera::anonymize_trace_variant_query(log, complete);
  assert(released.frequencies.size() == 2);
  assert(released.frequencies.contains({"A", "B"}));
  assert(released.frequencies.contains({"A", "C"}));
  assert(!released.frequencies.contains({"A"}));

  complete.max_prefix_length = 1;
  const auto incomplete =
      minera::anonymize_trace_variant_query(log, complete);
  assert(incomplete.frequencies.empty());

  // A parent removed at depth one cannot be resurrected by a later Cartesian
  // expansion, even when its child is an observed complete variant.
  complete.prune_threshold = 3;
  const auto pruned = minera::anonymize_trace_variant_query(log, complete);
  assert(pruned.frequencies.empty());
}

void sacofa_requires_complete_behaviorally_appropriate_variants() {
  minera::EventLog log;
  for (int i = 0; i < 3; ++i) {
    const std::string id = "abc" + std::to_string(i);
    log.traces.push_back({id, {{id, "A", "", "", 0, {}, ""},
                               {id, "B", "", "", 1, {}, ""},
                               {id, "C", "", "", 2, {}, ""}}});
  }
  minera::TraceVariantQueryOptions options;
  options.variant = minera::TraceVariantQueryVariant::kSacofa;
  options.epsilon = 1e9;
  options.max_prefix_length = 3;
  options.seed = 29;
  const auto result =
      minera::anonymize_trace_variant_query(log, options);
  assert(result.frequencies.contains({"A", "B", "C"}));
  // The observed prefix A,B is not a complete variant: B always has C after
  // it, so SaCoFa must not release A,B as an ended trace.
  assert(!result.frequencies.contains({"A", "B"}));
}

void sacofa_checks_completed_single_activity_prefixes() {
  minera::EventLog log;
  for (int i = 0; i < 3; ++i) {
    const std::string id = "ab" + std::to_string(i);
    log.traces.push_back({id, {{id, "A", "", "", 0, {}, ""},
                               {id, "B", "", "", 1, {}, ""}}});
  }
  minera::TraceVariantQueryOptions options;
  options.variant = minera::TraceVariantQueryVariant::kSacofa;
  options.epsilon = 1e9;
  options.max_prefix_length = 2;
  options.seed = 31;
  const auto result =
      minera::anonymize_trace_variant_query(log, options);
  assert(result.frequencies.contains({"A", "B"}));
  // A always has B after it in this log, so the ended one-event candidate A
  // violates the completion relation and must not be released.
  assert(!result.frequencies.contains({"A"}));
}

void empty_trace_variants_and_open_noise_boundary() {
  minera::EventLog only_empty;
  only_empty.traces.push_back({"empty", {}});
  minera::TraceVariantQueryOptions options;
  options.variant = minera::TraceVariantQueryVariant::kLaplace;
  options.epsilon = 1e9;
  options.max_prefix_length = 1;
  options.seed = 10419623598414093158ULL;
  const auto pure =
      minera::anonymize_trace_variant_query(only_empty, options);
  assert(pure.frequencies.contains({}));
  assert(pure.frequencies.at({}) == 1);
  assert(pure.log.trace_count() == 1);
  assert(pure.log.traces.front().events.empty());

  minera::EventLog mixed = only_empty;
  mixed.traces.push_back({"a", {{"a", "A", "", "", 0, {}, ""}}});
  options.max_prefix_length = 2;
  const auto released =
      minera::anonymize_trace_variant_query(mixed, options);
  assert(released.frequencies.contains({}));
  assert(released.frequencies.at({}) == 1);
  assert(released.frequencies.contains({"A"}));
  assert(released.frequencies.at({"A"}) == 1);

  // This seed makes the deterministic 64-bit sample equal UINT64_MAX. The
  // inverse-CDF conversion must still stay strictly below one, avoiding an
  // infinite Laplace draw and an int64-sized released frequency.
  assert(released.frequencies.at({}) < 100);
}

void empty_completion_and_exponential_selection_contract() {
  minera::EventLog only_empty;
  only_empty.traces.push_back({"empty", {}});
  minera::TraceVariantQueryOptions empty_options;
  empty_options.variant = minera::TraceVariantQueryVariant::kSacofa;
  empty_options.epsilon = 1.0;
  empty_options.max_prefix_length = 1;
  empty_options.seed = 5;
  const auto empty_result =
      minera::anonymize_trace_variant_query(only_empty, empty_options);
  // The completion marker is a behavioral violation (it is not a first
  // event), so with this seed the exponential choice leaves it unperturbed.
  assert(empty_result.frequencies.contains({}));
  assert(empty_result.frequencies.at({}) == 1);
  assert(empty_result.diagnostics.selected_violation_count == 0);

  minera::EventLog log;
  log.traces.push_back({"ab", {{"ab", "A", "", "", 0, {}, ""},
                               {"ab", "B", "", "", 1, {}, ""}}});
  log.traces.push_back({"ac", {{"ac", "A", "", "", 0, {}, ""},
                               {"ac", "C", "", "", 1, {}, ""}}});
  minera::TraceVariantQueryOptions selection_options;
  selection_options.variant = minera::TraceVariantQueryVariant::kSacofa;
  selection_options.epsilon = 1.0;
  selection_options.max_prefix_length = 1;
  selection_options.seed = 1;
  const auto selected = minera::anonymize_trace_variant_query(
      log, selection_options);
  // Logs without empty traces run the reference SaCoFa, which is
  // deterministic for a fixed seed and does not report violation counts.
  const auto again = minera::anonymize_trace_variant_query(log, selection_options);
  assert(selected.frequencies == again.frequencies);
  assert(selected.diagnostics.selected_violation_count == 0);
}

}  // namespace

int main() {
  basics();
  edge_cases();
  sacofa_and_pruning();
  laplace_completion_and_depth_pruning();
  sacofa_requires_complete_behaviorally_appropriate_variants();
  sacofa_checks_completed_single_activity_prefixes();
  empty_trace_variants_and_open_noise_boundary();
  empty_completion_and_exponential_selection_contract();
  auto malformed = minera::anonymize_trace_variant_query(
      sample(), minera::TraceVariantQueryOptions{});
  ++malformed.diagnostics.retained_variant_count;
  bool threw = false;
  try { (void)minera::trace_variant_query_json(malformed); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
