#include "minera/trace_variant_query_reference.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "numpy_legacy_random.hpp"
#include "python_random.hpp"

namespace minera {
namespace {

constexpr const char* kSeparator = ">>>";

// numpy's pairwise summation of a contiguous float64 array (np.sum).
double pairwise_sum(const double* values, std::size_t n) {
  if (n < 8) {
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += values[i];
    return sum;
  }
  if (n <= 128) {
    double r[8];
    for (std::size_t j = 0; j < 8; ++j) r[j] = values[j];
    std::size_t i = 8;
    for (; i < n - n % 8; i += 8)
      for (std::size_t j = 0; j < 8; ++j) r[j] += values[i + j];
    double sum = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; ++i) sum += values[i];
    return sum;
  }
  std::size_t half = n / 2;
  half -= half % 8;
  return pairwise_sum(values, half) + pairwise_sum(values + half, n - half);
}

long exp_mech(long n, double epsilon, NumpyLegacyRandom& rng) {
  const auto size = static_cast<std::size_t>(n);
  std::vector<double> weights(size), p(size);
  bool overflow = false;
  for (std::size_t i = 0; i < size; ++i) {
    weights[i] = std::exp(epsilon * static_cast<double>(size - 1 - i) / 2.0);
    overflow = overflow || std::isinf(weights[i]);
  }
  if (overflow) {
    const auto count = static_cast<double>(std::count_if(weights.begin(), weights.end(), [](double w) { return std::isinf(w); }));
    for (std::size_t i = 0; i < size; ++i) p[i] = std::isinf(weights[i]) ? 1.0 / count : 0.0;
  } else {
    const double top = *std::max_element(weights.begin(), weights.end());
    for (std::size_t i = 0; i < size; ++i) p[i] = std::exp(weights[i] - top);
    const double total = pairwise_sum(p.data(), size);
    for (auto& value : p) value /= total;
  }
  // np.random.choice(universe, p=p): cumulative sums normalised by the last,
  // one uniform, the first index whose sum exceeds it.
  std::vector<double> cdf(size);
  double running = 0.0;
  for (std::size_t i = 0; i < size; ++i) cdf[i] = running += p[i];
  for (auto& value : cdf) value /= cdf.back();
  const double uniform = rng.random_sample();
  return static_cast<long>(std::upper_bound(cdf.begin(), cdf.end(), uniform) - cdf.begin());
}

std::vector<std::string> split_key(const std::string& key) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const auto at = key.find(kSeparator, start);
    out.push_back(key.substr(start, at == std::string::npos ? std::string::npos : at - start));
    if (at == std::string::npos) return out;
    start = at + 3;
  }
}

std::vector<TraceVariantQueryReferenceCase> nonempty_cases(std::vector<TraceVariantQueryReferenceCase> cases) {
  if (cases.empty())
    throw TraceVariantQueryReferenceError(
        "ValueError",
        "The pruning parameter p is probably too high. The result of the trace variant query is empty. Of a noisy "
        "trace variant, at least p traces must appear. Otherwise, the trace variant and its traces won't be part of "
        "the result of the trace variant query.");
  std::erase_if(cases, [](const TraceVariantQueryReferenceCase& c) { return c.events.empty(); });
  return cases;
}

}  // namespace

BehavioralAppropriatenessRelations reference_ba_relations(const std::vector<std::vector<std::string>>& traces,
                                                          const std::vector<std::string>& events) {
  BehavioralAppropriatenessRelations out;
  out.events = events;
  for (const auto& x : events) {
    for (const auto& y : events) {
      long containing = 0, after = 0, before = 0;
      for (const auto& trace : traces) {
        const auto first = std::find(trace.begin(), trace.end(), x);
        if (first == trace.end()) continue;
        ++containing;
        if (std::find(first + 1, trace.end(), y) != trace.end()) ++after;
        const auto last = std::find(trace.rbegin(), trace.rend(), x).base() - 1;
        if (std::find(trace.begin(), last, y) != last) ++before;
      }
      const auto letter = [&](long hits) { return containing && hits == containing ? 'A' : hits ? 'S' : 'N'; };
      out.follows[{x, y}] = letter(after);
      out.precedes[{x, y}] = letter(before);
    }
  }
  return out;
}

long reference_ba_violations(const BehavioralAppropriatenessRelations& relations, const std::vector<std::string>& prefix) {
  std::vector<std::string> parts = prefix;
  const bool ended = !parts.empty() && parts.back() == kTraceVariantQueryTraceEnd;
  if (ended) parts.pop_back();
  long violations = 0;
  std::set<std::string> seen;
  for (const auto& x : parts) {
    if (!seen.insert(x).second) continue;
    const auto first = std::find(parts.begin(), parts.end(), x);
    const auto last = std::find(parts.rbegin(), parts.rend(), x).base() - 1;
    const std::set<std::string> followers(first + 1, parts.end()), predecessors(parts.begin(), last);
    for (const auto& y : followers)
      if (relations.follows.at({x, y}) == 'N') ++violations;
    for (const auto& y : predecessors)
      if (relations.precedes.at({x, y}) == 'N') ++violations;
    for (const auto& y : relations.events) {
      if (ended && relations.follows.at({x, y}) == 'A' && !followers.contains(y)) ++violations;
      if (relations.precedes.at({x, y}) == 'A' && !predecessors.contains(y)) ++violations;
    }
  }
  return violations;
}

long reference_exp_mech(long n, double epsilon, std::uint32_t seed) {
  NumpyLegacyRandom rng(seed);
  return exp_mech(n, epsilon, rng);
}

std::vector<TraceVariantQueryReferenceCase> reference_generate_variant_log(
    const std::vector<std::pair<std::string, long>>& trace_frequencies) {
  // A Python dict: a repeated key keeps its first place and its last count.
  std::vector<std::pair<std::string, long>> frequencies;
  for (const auto& [key, count] : trace_frequencies) {
    const auto found = std::find_if(frequencies.begin(), frequencies.end(), [&](const auto& e) { return e.first == key; });
    if (found == frequencies.end()) {
      frequencies.emplace_back(key, count);
    } else {
      found->second = count;
    }
  }
  std::vector<TraceVariantQueryReferenceCase> out;
  for (const auto& [key, count] : frequencies) {
    std::vector<std::string> events;
    for (auto& piece : split_key(key))
      if (piece != kTraceVariantQueryTraceEnd) events.push_back(std::move(piece));
    for (long i = 0; i < count; ++i) out.push_back({static_cast<long>(out.size()), events});
  }
  return out;
}

std::vector<TraceVariantQueryReferenceCase> reference_sacofa_trace_variant_query(
    const std::vector<std::vector<std::string>>& traces, const TraceVariantQueryReferenceOptions& options) {
  if (options.k >= 1 && std::any_of(traces.begin(), traces.end(), [](const auto& t) { return t.empty(); }))
    throw TraceVariantQueryReferenceError("IndexError", "list index out of range");
  std::vector<std::string> events;
  for (const auto& trace : traces)
    for (const auto& event : trace)
      if (std::find(events.begin(), events.end(), event) == events.end()) events.push_back(event);
  const auto relations = reference_ba_relations(traces, events);
  std::map<std::vector<std::string>, long> prefixes, ended_traces;
  std::set<std::string> first_events;
  for (const auto& trace : traces) {
    for (std::size_t i = 1; i <= trace.size(); ++i) ++prefixes[std::vector<std::string>(trace.begin(), trace.begin() + i)];
    ++ended_traces[trace];
    if (!trace.empty()) first_events.insert(trace.front());
  }

  struct Entry {
    std::vector<std::string> sequence;
    bool ended = false;
    long count = 0;
  };
  NumpyLegacyRandom numpy_rng(options.seed);
  PythonRandom python_rng(options.seed);
  std::vector<Entry> final_entries;
  std::vector<std::vector<std::string>> current{{}};
  for (long n = 1; n <= options.k; ++n) {
    std::vector<Entry> level;
    for (const auto& prefix : current) {
      for (const auto& event : events) {
        auto sequence = prefix;
        sequence.push_back(event);
        const auto found = prefixes.find(sequence);
        level.push_back({std::move(sequence), false, found == prefixes.end() ? 0 : found->second});
      }
      const auto found = ended_traces.find(prefix);
      level.push_back({prefix, true, found == ended_traces.end() ? 0 : found->second});
    }
    std::vector<std::size_t> conforming, rest;
    for (std::size_t i = 0; i < level.size(); ++i) {
      bool conforms = false;
      if (n == 1) {
        conforms = !level[i].ended && first_events.contains(level[i].sequence.front());
      } else {
        auto prefix = level[i].sequence;
        if (level[i].ended) prefix.emplace_back(kTraceVariantQueryTraceEnd);
        conforms = reference_ba_violations(relations, prefix) == 0;
      }
      (conforms ? conforming : rest).push_back(i);
    }
    const long added = exp_mech(static_cast<long>(rest.size()) + 1, options.epsilon, numpy_rng);
    for (long i = 0; i < added; ++i) {
      const auto at = python_rng.randbelow(static_cast<std::uint32_t>(rest.size()));
      conforming.push_back(rest[at]);
      rest.erase(rest.begin() + at);
    }
    for (const auto i : conforming) {
      const double noise = numpy_rng.laplace(0.0, 1.0 / options.epsilon);
      level[i].count = std::max(0L, level[i].count + static_cast<long>(std::trunc(noise)));
    }
    if (n < options.k) {
      current.clear();
      for (auto& entry : level) {
        if (entry.ended) {
          final_entries.push_back(std::move(entry));
        } else if (entry.count >= options.p) {
          current.push_back(std::move(entry.sequence));
        }
      }
    } else {
      for (auto& entry : level) final_entries.push_back(std::move(entry));
    }
  }
  std::vector<TraceVariantQueryReferenceCase> cases;
  for (const auto& entry : final_entries)
    for (long i = 0; i < entry.count; ++i) cases.push_back({static_cast<long>(cases.size()), entry.sequence});
  return nonempty_cases(std::move(cases));
}

void reference_laplace_trace_variant_query(const std::vector<std::vector<std::string>>&,
                                           const TraceVariantQueryReferenceOptions&) {
  throw TraceVariantQueryReferenceError("AttributeError", "module 'importlib.util' has no attribute 'find_loader'");
}

}  // namespace minera
