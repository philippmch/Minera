#include "minera/trace_variant_query.hpp"

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
    assert(!prefix.empty() && count >= 1);
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

  options.prune_threshold = 1000000;
  const auto pruned = minera::anonymize_trace_variant_query(log, options);
  assert(pruned.frequencies.empty());
}

}  // namespace

int main() {
  basics();
  edge_cases();
  sacofa_and_pruning();
}
