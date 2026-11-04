#pragma once

// algo.anonymization.trace_variant_query of the PM4Py 2.7.22 reference.
//
// SaCoFa builds noisy prefix frequencies level by level. With the events in
// first-appearance order plus TRACE_END, level n extends every surviving
// prefix of level n - 1 by each of them (counts from the log). A prefix
// conforms when (level 1) its event starts some trace or (later levels) it
// has no behavioural-appropriateness violation. The exponential mechanism
// draws c from 0..m, m the non-conforming prefixes; c of those are added one
// by one with Python's random.sample(rest, 1). Each conforming and added
// prefix (in that order) gets int(laplace(0, 1 / epsilon)) added, clamped at
// zero. Before the last level, prefixes ending in TRACE_END are final and the
// others survive when their count is at least p; the last level is final
// whole. Every final prefix becomes count traces with case ids 0, 1, ...;
// the public apply returns a dataframe, so empty traces leave no rows. No
// traces at all raise ValueError (p too high); an empty input trace raises
// IndexError once k >= 1.
//
// The draws are the global generators of the reference: the Python random
// module and the legacy numpy generator, both seeded with the given seed.
//
// Behavioural appropriateness: follows(x, y) is A (S, N) when y occurs after
// the first x in every (some, no) trace containing x; precedes(x, y) the same
// for y before the last x. A prefix's violations, per distinct event x: each
// distinct event after its first x with follows N, each distinct event before
// its last x with precedes N, each y with precedes(x, y) = A missing before
// the last x and, for a prefix ending in TRACE_END, each y with
// follows(x, y) = A missing after the first x.
//
// The exponential mechanism over 0..n-1 scores element i by n - 1 - i; the
// weights are exp(exp(epsilon * score / 2) - max) (when the inner exp
// overflows, the overflowing elements share the probability evenly), and one
// numpy choice draws the element.
//
// LAPLACE raises AttributeError in the pinned release, whatever the input
// (it looks up importlib.util.find_loader, which does not exist).

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {

class TraceVariantQueryReferenceError : public std::runtime_error {
 public:
  TraceVariantQueryReferenceError(std::string type, const std::string& message)
      : std::runtime_error(message), type(std::move(type)) {}
  std::string type;  // the Python exception class
};

inline constexpr const char* kTraceVariantQueryTraceEnd = "TRACE_END";

struct BehavioralAppropriatenessRelations {
  std::vector<std::string> events;
  std::map<std::pair<std::string, std::string>, char> follows;   // 'A', 'S', 'N'
  std::map<std::pair<std::string, std::string>, char> precedes;  // 'A', 'S', 'N'
};

[[nodiscard]] BehavioralAppropriatenessRelations reference_ba_relations(
    const std::vector<std::vector<std::string>>& traces, const std::vector<std::string>& events);

// prefix may end with kTraceVariantQueryTraceEnd.
[[nodiscard]] std::int64_t reference_ba_violations(const BehavioralAppropriatenessRelations& relations,
                                           const std::vector<std::string>& prefix);

// exp_mech(np.arange(n), epsilon) right after np.random.seed(seed).
[[nodiscard]] std::int64_t reference_exp_mech(std::int64_t n, double epsilon, std::uint32_t seed);

struct TraceVariantQueryReferenceCase {
  std::int64_t case_id = 0;
  std::vector<std::string> events;
};

// util.generate_pm4py_log: keys are prefixes joined by ">>>", TRACE_END left
// out of the traces; empty traces are kept here.
[[nodiscard]] std::vector<TraceVariantQueryReferenceCase> reference_generate_variant_log(
    const std::vector<std::pair<std::string, std::int64_t>>& trace_frequencies);

struct TraceVariantQueryReferenceOptions {
  double epsilon = 1.0;
  std::int64_t k = 0;  // prefix length
  std::int64_t p = 1;  // pruning
  std::uint32_t seed = 0;
};

// Counters of one SaCoFa run: the candidate prefixes over all levels and the
// non-conforming prefixes the exponential mechanism added back.
struct TraceVariantQueryReferenceStats {
  std::uint64_t candidate_count = 0;
  std::uint64_t selected_violation_count = 0;
};

// algorithm.apply with SACOFA: the dataframe's cases (empty traces dropped;
// none when every trace is empty). |stats|, when given, is filled as the
// levels run, so it is complete even when the run raises ValueError.
[[nodiscard]] std::vector<TraceVariantQueryReferenceCase> reference_sacofa_trace_variant_query(
    const std::vector<std::vector<std::string>>& traces, const TraceVariantQueryReferenceOptions& options,
    TraceVariantQueryReferenceStats* stats = nullptr);

// algorithm.apply with LAPLACE: always raises.
[[noreturn]] void reference_laplace_trace_variant_query(const std::vector<std::vector<std::string>>& traces,
                                                        const TraceVariantQueryReferenceOptions& options);

}  // namespace minera
