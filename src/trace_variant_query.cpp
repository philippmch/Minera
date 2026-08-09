#include "minera/trace_variant_query.hpp"
#include "minera/secure_random.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>

namespace minera {
namespace {

constexpr std::size_t kHardBound = 1000000;

struct PrefixLess {
  bool operator()(const TraceVariantQueryPrefix& a,
                  const TraceVariantQueryPrefix& b) const noexcept {
    return a < b;
  }
};

std::uint64_t hash_prefix(std::uint64_t seed,
                          const TraceVariantQueryPrefix& prefix) {
  // FNV-1a followed by splitmix64 gives a stable stream independent of the
  // process' hash implementation.
  std::uint64_t h = 1469598103934665603ULL ^ seed;
  for (const auto& label : prefix) {
    for (const unsigned char c : label) {
      h ^= c;
      h *= 1099511628211ULL;
    }
    h ^= 0xffU;
    h *= 1099511628211ULL;
  }
  h += 0x9e3779b97f4a7c15ULL;
  h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
  h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
  return h ^ (h >> 31);
}

double uniform01(std::uint64_t value) {
  // Keep the endpoints open, as required by inverse CDFs.
  constexpr double denominator =
      static_cast<double>(std::numeric_limits<std::uint64_t>::max()) + 1.0;
  return (static_cast<double>(value) + 0.5) / denominator;
}

std::int64_t laplace_noise(std::uint64_t seed,
                           const TraceVariantQueryPrefix& prefix,
                           double epsilon, bool secure_random) {
  const auto sample = secure_random ? secure_random_u64()
                                    : hash_prefix(seed, prefix);
  const double u = uniform01(sample) - 0.5;
  const double magnitude = -std::log1p(-2.0 * std::abs(u)) / epsilon;
  const double signed_noise = std::copysign(magnitude, u);
  if (signed_noise >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
    return std::numeric_limits<std::int64_t>::max();
  if (signed_noise <= static_cast<double>(std::numeric_limits<std::int64_t>::min()))
    return std::numeric_limits<std::int64_t>::min();
  return static_cast<std::int64_t>(std::llround(signed_noise));
}

std::uint64_t add_noise(std::uint64_t count, std::int64_t noise) {
  if (noise < 0) {
    const auto magnitude = static_cast<std::uint64_t>(-(noise + 1)) + 1;
    return magnitude >= count ? 0 : count - magnitude;
  }
  const auto amount = static_cast<std::uint64_t>(noise);
  if (amount > std::numeric_limits<std::uint64_t>::max() - count)
    return std::numeric_limits<std::uint64_t>::max();
  return count + amount;
}

struct Candidate {
  TraceVariantQueryPrefix prefix;
  std::uint64_t count = 0;
  double score = 0.0;
};

}  // namespace

TraceVariantQueryResult anonymize_trace_variant_query(
    const EventLog& log, const TraceVariantQueryOptions& options) {
  if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0)
    throw std::invalid_argument("trace variant query: epsilon must be finite and > 0");
  if (options.prune_threshold < 1)
    throw std::invalid_argument("trace variant query: prune_threshold must be >= 1");
  if (options.max_output_traces > kHardBound ||
      options.max_output_variants > kHardBound ||
      options.max_prefix_length > kHardBound)
    throw std::invalid_argument("trace variant query: safety bound exceeded");

  TraceVariantQueryResult result;
  result.diagnostics.variant = options.variant;
  result.diagnostics.epsilon = options.epsilon;
  result.diagnostics.max_prefix_length = options.max_prefix_length;
  result.diagnostics.prune_threshold = options.prune_threshold;
  result.diagnostics.seed = options.seed;
  result.diagnostics.secure_random = options.secure_random;
  if (options.max_prefix_length == 0 || log.traces.empty()) {
    result.diagnostics.message = options.max_prefix_length == 0
                                     ? "empty prefix bound"
                                     : "empty event log";
    return result;
  }

  std::map<TraceVariantQueryPrefix, std::uint64_t, PrefixLess> counts;
  std::set<std::string> alphabet_set;
  std::set<std::string> first_events;
  std::set<std::pair<std::string, std::string>> all_follows;
  std::set<std::pair<std::string, std::string>> all_precedes;
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    first_events.insert(trace.events.front().activity);
    for (const auto& event : trace.events) alphabet_set.insert(event.activity);
    // SaCoFa's behavioural checks are universal over the observed ordering:
    // an activity may follow (or precede) another whenever that relation was
    // seen at any later (or earlier) position in a trace.
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      for (std::size_t j = i + 1; j < trace.events.size(); ++j) {
        all_follows.emplace(trace.events[i].activity,
                            trace.events[j].activity);
        all_precedes.emplace(trace.events[j].activity,
                             trace.events[i].activity);
      }
    }
    const std::size_t limit =
        std::min(options.max_prefix_length, trace.events.size());
    TraceVariantQueryPrefix prefix;
    prefix.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
      prefix.push_back(trace.events[i].activity);
      auto& count = counts[prefix];
      if (count != std::numeric_limits<std::uint64_t>::max()) ++count;
      if (counts.size() > kHardBound)
        throw std::invalid_argument(
            "trace variant query: candidate safety bound exceeded");
    }
  }

  // The Laplace query is defined over a bounded candidate universe, not only
  // prefixes that happened to occur. Add alphabet expansions when the Cartesian
  // universe is small enough to remain within the hard safety bound.
  if (options.variant == TraceVariantQueryVariant::kLaplace &&
      !alphabet_set.empty()) {
    const std::vector<std::string> alphabet(alphabet_set.begin(), alphabet_set.end());
    std::size_t universe = 0;
    std::size_t power = 1;
    bool bounded = true;
    for (std::size_t length = 1; length <= options.max_prefix_length; ++length) {
      if (power > kHardBound / alphabet.size()) { bounded = false; break; }
      power *= alphabet.size();
      if (universe > kHardBound - power) { bounded = false; break; }
      universe += power;
    }
    if (bounded) {
      TraceVariantQueryPrefix prefix;
      std::function<void(std::size_t)> expand = [&](std::size_t length) {
        if (length != 0) counts.try_emplace(prefix, 0);
        if (length == options.max_prefix_length) return;
        for (const auto& activity : alphabet) {
          prefix.push_back(activity);
          expand(length + 1);
          prefix.pop_back();
        }
      };
      expand(0);
    }
  }

  result.diagnostics.candidate_count = counts.size();
  std::vector<Candidate> candidates;
  candidates.reserve(counts.size());
  for (const auto& [prefix, count] : counts) {
    bool appropriate = !prefix.empty() && first_events.contains(prefix.front());
    double score = static_cast<double>(count) + (appropriate ? 1.0 : 0.0);
    for (std::size_t i = 1; i < prefix.size(); ++i) {
      const auto edge = std::make_pair(prefix[i - 1], prefix[i]);
      const bool valid = all_follows.contains(edge) &&
                         all_precedes.contains(std::make_pair(prefix[i], prefix[i - 1]));
      appropriate = appropriate && valid;
      if (valid) score += 1.0;
    }
    if (options.variant == TraceVariantQueryVariant::kSacofa && !appropriate)
      continue;
    candidates.push_back({prefix, count, score});
  }

  // The exponential-mechanism step is only needed when the candidate set is
  // larger than the configured bound. Deterministic mode uses a stable weighted
  // key; secure mode draws each ranking sample from the OS entropy provider.
  if (candidates.size() > options.max_output_variants) {
    if (options.max_output_variants == 0) {
      candidates.clear();
      result.diagnostics.output_truncated = true;
    } else {
      struct Ranked { double key; std::size_t index; };
      std::vector<Ranked> ranked;
      ranked.reserve(candidates.size());
      for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto sample = options.secure_random
                                ? secure_random_u64()
                                : hash_prefix(options.seed + 0x517cc1b727220a95ULL,
                                              candidates[i].prefix);
        const double u = uniform01(sample);
        const double weight = std::exp(std::min(40.0, options.epsilon * candidates[i].score));
        ranked.push_back({-std::log(u) / std::max(weight, 1e-300), i});
      }
      std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        if (a.key != b.key) return a.key < b.key;
        return a.index < b.index;
      });
      std::vector<Candidate> selected;
      selected.reserve(options.max_output_variants);
      for (std::size_t i = 0; i < options.max_output_variants; ++i)
        selected.push_back(std::move(candidates[ranked[i].index]));
      std::sort(selected.begin(), selected.end(), [](const Candidate& a, const Candidate& b) {
        return a.prefix < b.prefix;
      });
      candidates = std::move(selected);
      result.diagnostics.output_truncated = true;
    }
  }

  for (const auto& candidate : candidates) {
    const auto released = add_noise(candidate.count,
                                    laplace_noise(options.seed, candidate.prefix,
                                                  options.epsilon,
                                                  options.secure_random));
    if (released < options.prune_threshold) continue;
    result.frequencies.emplace(candidate.prefix, released);
  }
  result.diagnostics.retained_variant_count = result.frequencies.size();

  std::size_t remaining = options.max_output_traces;
  std::uint64_t case_number = 1;
  for (const auto& [prefix, frequency] : result.frequencies) {
    if (remaining == 0) {
      result.diagnostics.output_truncated = true;
      break;
    }
    const std::uint64_t emit = std::min<std::uint64_t>(frequency, remaining);
    if (emit < frequency) result.diagnostics.output_truncated = true;
    for (std::uint64_t n = 0; n < emit; ++n) {
      Trace trace;
      trace.case_id = "tvq-" + std::to_string(case_number++);
      trace.events.reserve(prefix.size());
      for (std::size_t i = 0; i < prefix.size(); ++i) {
        trace.events.push_back(Event{trace.case_id, prefix[i],
                                     "1970-01-01T00:00:00Z", "", i, {}, ""});
      }
      result.log.traces.push_back(std::move(trace));
    }
    remaining -= static_cast<std::size_t>(emit);
    result.diagnostics.released_trace_count += emit;
  }
  result.diagnostics.message = result.diagnostics.output_truncated
                                   ? "output safety bound applied"
                                   : "ok";
  return result;
}

}  // namespace minera
