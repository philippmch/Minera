#include "minera/trace_variant_query.hpp"

#include "minera/secure_random.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

constexpr std::size_t kHardBound = 1000000;

// The reference algorithm keeps a terminator in its prefix tree. Keeping it
// as a separate bit avoids reserving an activity name that could collide with
// user data; it is removed before the public frequency map is returned.
struct Prefix {
  TraceVariantQueryPrefix activities;
  bool ended = false;

  bool operator<(const Prefix& other) const noexcept {
    if (activities != other.activities) return activities < other.activities;
    return ended < other.ended;
  }
};

struct PrefixState {
  Prefix prefix;
  std::uint64_t count = 0;
  std::size_t violations = 0;
};

enum class Relation { kNever, kSometimes, kAlways };
using RelationMatrix = std::map<std::pair<std::string, std::string>, Relation>;

std::uint64_t splitmix64(std::uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

std::uint64_t hash_prefix(std::uint64_t seed, const Prefix& prefix) {
  // FNV-1a followed by splitmix64 gives a stable stream independent of the
  // process' hash implementation.
  std::uint64_t h = 1469598103934665603ULL ^ seed;
  for (const auto& label : prefix.activities) {
    for (const unsigned char c : label) {
      h ^= c;
      h *= 1099511628211ULL;
    }
    h ^= 0xffU;
    h *= 1099511628211ULL;
  }
  h ^= prefix.ended ? 0xe1U : 0x1dU;
  return splitmix64(h);
}

double uniform01(std::uint64_t value) {
  // Keep the endpoints open, as required by inverse CDFs.
  // Converting all 64 bits to double first can round the top values to 2^64,
  // so use the 53 random bits that a double can represent exactly.
  constexpr double denominator = 9007199254740992.0;  // 2^53
  return (static_cast<double>(value >> 11) + 0.5) / denominator;
}

std::uint64_t sample_value(std::uint64_t seed, const Prefix& prefix,
                           std::uint64_t salt, bool secure_random) {
  return secure_random ? secure_random_u64()
                       : hash_prefix(seed ^ salt, prefix);
}

std::int64_t laplace_noise(std::uint64_t seed, const Prefix& prefix,
                           double epsilon, bool secure_random) {
  const auto sample = sample_value(seed, prefix, 0x6a09e667f3bcc909ULL,
                                   secure_random);
  const double u = uniform01(sample) - 0.5;
  const double magnitude = -std::log1p(-2.0 * std::abs(u)) / epsilon;
  const double signed_noise = std::copysign(magnitude, u);
  if (signed_noise >=
      static_cast<double>(std::numeric_limits<std::int64_t>::max()))
    return std::numeric_limits<std::int64_t>::max();
  if (signed_noise <=
      static_cast<double>(std::numeric_limits<std::int64_t>::min()))
    return std::numeric_limits<std::int64_t>::min();
  // The reference converts the sampled Laplace value to an integer by
  // truncating toward zero rather than rounding to the nearest integer.
  return static_cast<std::int64_t>(signed_noise);
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

void increment(std::map<Prefix, std::uint64_t>& counts, const Prefix& prefix) {
  auto& value = counts[prefix];
  if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

std::vector<std::string> first_seen_activities(const EventLog& log) {
  std::vector<std::string> result;
  std::set<std::string> seen;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      if (seen.insert(event.activity).second) result.push_back(event.activity);
    }
  }
  return result;
}

std::vector<std::string> trace_activities(const Trace& trace) {
  std::vector<std::string> activities;
  activities.reserve(trace.events.size());
  for (const auto& event : trace.events) activities.push_back(event.activity);
  return activities;
}

std::vector<std::string> unique_followers(const std::vector<std::string>& trace,
                                          std::size_t index) {
  std::vector<std::string> result;
  std::set<std::string> seen;
  for (std::size_t i = index + 1; i < trace.size(); ++i) {
    if (seen.insert(trace[i]).second) result.push_back(trace[i]);
  }
  return result;
}

std::vector<std::string> unique_predecessors(
    const std::vector<std::string>& trace, std::size_t index) {
  std::vector<std::string> result;
  std::set<std::string> seen;
  for (std::size_t i = 0; i < index; ++i) {
    if (seen.insert(trace[i]).second) result.push_back(trace[i]);
  }
  return result;
}

RelationMatrix build_follows(const EventLog& log,
                             const std::vector<std::string>& activities) {
  RelationMatrix result;
  for (const auto& source : activities) {
    std::map<std::string, bool> always;
    std::map<std::string, bool> never;
    for (const auto& target : activities) {
      always[target] = true;
      never[target] = true;
    }
    bool observed = false;
    for (const auto& trace : log.traces) {
      const auto values = trace_activities(trace);
      const auto it = std::find(values.begin(), values.end(), source);
      if (it == values.end()) continue;
      observed = true;
      const auto followers = unique_followers(
          values, static_cast<std::size_t>(std::distance(values.begin(), it)));
      const std::set<std::string> follower_set(followers.begin(), followers.end());
      for (const auto& target : activities) {
        if (!follower_set.contains(target)) always[target] = false;
        if (follower_set.contains(target)) never[target] = false;
      }
    }
    for (const auto& target : activities) {
      const auto key = std::make_pair(source, target);
      result[key] = !observed || !always[target]
                        ? (!observed || !never[target] ? Relation::kSometimes
                                                       : Relation::kNever)
                        : Relation::kAlways;
    }
  }
  return result;
}

RelationMatrix build_precedes(const EventLog& log,
                              const std::vector<std::string>& activities) {
  RelationMatrix result;
  for (const auto& source : activities) {
    std::map<std::string, bool> always;
    std::map<std::string, bool> never;
    for (const auto& target : activities) {
      always[target] = true;
      never[target] = true;
    }
    bool observed = false;
    for (const auto& trace : log.traces) {
      const auto values = trace_activities(trace);
      const auto it = std::find(values.rbegin(), values.rend(), source);
      if (it == values.rend()) continue;
      observed = true;
      const auto index = values.size() - 1 -
                         static_cast<std::size_t>(std::distance(values.rbegin(), it));
      const auto predecessors = unique_predecessors(values, index);
      const std::set<std::string> predecessor_set(predecessors.begin(),
                                                   predecessors.end());
      for (const auto& target : activities) {
        if (!predecessor_set.contains(target)) always[target] = false;
        if (predecessor_set.contains(target)) never[target] = false;
      }
    }
    for (const auto& target : activities) {
      const auto key = std::make_pair(source, target);
      result[key] = !observed || !always[target]
                        ? (!observed || !never[target] ? Relation::kSometimes
                                                       : Relation::kNever)
                        : Relation::kAlways;
    }
  }
  return result;
}

std::size_t behavioral_violations(
    const Prefix& prefix, const std::vector<std::string>& activities,
    const std::set<std::string>& first_events, const RelationMatrix& follows,
    const RelationMatrix& precedes) {
  if (prefix.activities.empty()) return 1;
  if (!prefix.ended && prefix.activities.size() <= 1) {
    return first_events.contains(prefix.activities.front()) ? 0 : 1;
  }

  std::vector<std::string> unique;
  std::set<std::string> seen;
  for (const auto& activity : prefix.activities) {
    if (seen.insert(activity).second) unique.push_back(activity);
  }
  std::size_t violations = 0;
  for (const auto& source : unique) {
    const auto first = std::find(prefix.activities.begin(), prefix.activities.end(), source);
    const auto last = std::find(prefix.activities.rbegin(), prefix.activities.rend(), source);
    const auto followers = unique_followers(
        prefix.activities,
        static_cast<std::size_t>(std::distance(prefix.activities.begin(), first)));
    const auto predecessors = unique_predecessors(
        prefix.activities,
        prefix.activities.size() - 1 -
            static_cast<std::size_t>(std::distance(prefix.activities.rbegin(), last)));
    const std::set<std::string> follower_set(followers.begin(), followers.end());
    const std::set<std::string> predecessor_set(predecessors.begin(),
                                                 predecessors.end());
    for (const auto& target : followers) {
      if (follows.at(std::make_pair(source, target)) == Relation::kNever)
        ++violations;
    }
    for (const auto& target : predecessors) {
      if (precedes.at(std::make_pair(source, target)) == Relation::kNever)
        ++violations;
    }
    for (const auto& target : activities) {
      if (prefix.ended &&
          follows.at(std::make_pair(source, target)) == Relation::kAlways &&
          !follower_set.contains(target))
        ++violations;
      if (precedes.at(std::make_pair(source, target)) == Relation::kAlways &&
          !predecessor_set.contains(target))
        ++violations;
    }
  }
  return violations;
}

long double mechanism_weight(long double exponent) {
  static const long double kDoubleMaxLog =
      std::log(static_cast<long double>(std::numeric_limits<double>::max()));
  if (exponent > kDoubleMaxLog)
    return static_cast<long double>(std::numeric_limits<double>::max());
  return std::exp(exponent);
}

std::size_t choose_universe(std::size_t violating_count, double epsilon,
                            std::uint64_t seed, std::size_t depth,
                            bool secure_random) {
  if (violating_count == 0) return 0;
  Prefix context;
  context.activities.push_back("universe:" + std::to_string(depth));
  const auto sample = sample_value(seed, context, 0x510e527fade682d1ULL,
                                   secure_random);
  const long double step = static_cast<long double>(epsilon) / 2.0L;
  const auto maximum =
      mechanism_weight(step * static_cast<long double>(violating_count));
  long double total = 0.0L;
  for (std::size_t i = 0; i <= violating_count; ++i) {
    const long double score = static_cast<long double>(violating_count - i);
    const long double exponent = step * score;
    // The published exponential-mechanism implementation normalizes a
    // second exponential after computing the score weights. Preserve that
    // behavior while avoiding overflow for large epsilon values.
    const long double raw = mechanism_weight(exponent);
    const long double normalized = raw - maximum;
    total += normalized < std::log(std::numeric_limits<long double>::min())
                 ? 0.0L
                 : std::exp(normalized);
  }
  if (!(total > 0.0L) || !std::isfinite(static_cast<double>(total))) return 0;
  const long double target = static_cast<long double>(uniform01(sample)) * total;
  long double cumulative = 0.0L;
  for (std::size_t i = 0; i <= violating_count; ++i) {
    const long double score = static_cast<long double>(violating_count - i);
    const long double exponent = step * score;
    const long double raw = mechanism_weight(exponent);
    const long double normalized = raw - maximum;
    if (normalized >= std::log(std::numeric_limits<long double>::min()))
      cumulative += std::exp(normalized);
    if (target < cumulative) return i;
  }
  return violating_count;
}

std::vector<std::size_t> select_violations(
    const std::vector<PrefixState>& violating, std::size_t budget,
    std::uint64_t seed, std::size_t depth, bool secure_random) {
  std::vector<std::size_t> remaining(violating.size());
  for (std::size_t i = 0; i < remaining.size(); ++i) remaining[i] = i;
  std::vector<std::size_t> selected;
  selected.reserve(std::min(budget, remaining.size()));
  for (std::size_t round = 0; round < budget && !remaining.empty(); ++round) {
    Prefix context;
    context.activities.push_back("select:" + std::to_string(depth));
    context.activities.push_back(std::to_string(round));
    const auto sample = sample_value(seed, context, 0x1f83d9abfb41bd6bULL,
                                     secure_random);
    const auto index = std::min<std::size_t>(
        remaining.size() - 1,
        static_cast<std::size_t>(uniform01(sample) * remaining.size()));
    selected.push_back(remaining[index]);
    remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(index));
  }
  return selected;
}

std::map<Prefix, std::uint64_t> build_known_counts(const EventLog& log) {
  std::map<Prefix, std::uint64_t> known;
  for (const auto& trace : log.traces) {
    const auto activities = trace_activities(trace);
    Prefix prefix;
    for (const auto& activity : activities) {
      prefix.activities.push_back(activity);
      increment(known, prefix);
    }
    prefix.ended = true;
    increment(known, prefix);
  }
  return known;
}

void bump_candidate_count(TraceVariantQueryDiagnostics& diagnostics,
                          std::size_t count) {
  if (count > kHardBound - diagnostics.candidate_count)
    throw std::invalid_argument(
        "trace variant query: candidate safety bound exceeded");
  diagnostics.candidate_count += count;
}

struct FinalState {
  Prefix prefix;
  std::uint64_t count = 0;
  double score = 0.0;
};

void add_final(std::map<Prefix, FinalState>& final, const PrefixState& state,
               double score) {
  if (state.count == 0 ||
      (state.prefix.activities.empty() && !state.prefix.ended))
    return;
  final[state.prefix] = FinalState{state.prefix, state.count, score};
}

void cap_final_states(std::map<Prefix, FinalState>& final,
                      const TraceVariantQueryOptions& options,
                      TraceVariantQueryDiagnostics& diagnostics) {
  if (final.size() <= options.max_output_variants) return;
  diagnostics.output_truncated = true;
  if (options.max_output_variants == 0) {
    final.clear();
    return;
  }
  struct Ranked {
    double key = 0.0;
    Prefix prefix;
  };
  std::vector<Ranked> ranked;
  ranked.reserve(final.size());
  for (const auto& [prefix, state] : final) {
    const auto sample = options.secure_random
                            ? secure_random_u64()
                            : hash_prefix(options.seed + 0x517cc1b727220a95ULL,
                                          prefix);
    const double weight = std::exp(std::min(40.0,
                                             options.epsilon *
                                                 std::max(0.0, state.score)));
    ranked.push_back({-std::log(uniform01(sample)) /
                          std::max(weight, 1e-300),
                      prefix});
  }
  std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
    if (a.key != b.key) return a.key < b.key;
    return a.prefix < b.prefix;
  });
  std::map<Prefix, FinalState> selected;
  for (std::size_t i = 0; i < options.max_output_variants && i < ranked.size();
       ++i) {
    selected.emplace(ranked[i].prefix, final.at(ranked[i].prefix));
  }
  final = std::move(selected);
}

}  // namespace

TraceVariantQueryResult anonymize_trace_variant_query(
    const EventLog& log, const TraceVariantQueryOptions& options) {
  if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0)
    throw std::invalid_argument(
        "trace variant query: epsilon must be finite and > 0");
  if (options.prune_threshold < 1)
    throw std::invalid_argument(
        "trace variant query: prune_threshold must be >= 1");
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

  const auto activities = first_seen_activities(log);
  const auto known = build_known_counts(log);
  std::map<Prefix, std::uint64_t> active;
  active.emplace(Prefix{}, 0);

  std::set<std::string> first_events;
  for (const auto& trace : log.traces) {
    if (!trace.events.empty()) first_events.insert(trace.events.front().activity);
  }
  const auto follows = build_follows(log, activities);
  const auto precedes = build_precedes(log, activities);
  std::map<Prefix, FinalState> final;

  for (std::size_t depth = 1; depth <= options.max_prefix_length; ++depth) {
    if (activities.size() >= kHardBound)
      throw std::invalid_argument(
          "trace variant query: activity alphabet safety bound exceeded");
    const std::size_t branch_count = activities.size() + 1;
    if (active.size() >
        (kHardBound - result.diagnostics.candidate_count) / branch_count)
      throw std::invalid_argument(
          "trace variant query: candidate safety bound exceeded");
    std::map<Prefix, std::uint64_t> raw;
    for (const auto& [parent, ignored] : active) {
      (void)ignored;
      for (const auto& activity : activities) {
        Prefix child = parent;
        child.activities.push_back(activity);
        child.ended = false;
        const auto it = known.find(child);
        if (!raw.emplace(std::move(child),
                         it == known.end() ? 0 : it->second)
                 .second)
          continue;
      }
      Prefix ended = parent;
      ended.ended = true;
      const auto it = known.find(ended);
      raw.emplace(std::move(ended), it == known.end() ? 0 : it->second);
    }
    bump_candidate_count(result.diagnostics, raw.size());
    if (raw.empty()) break;

    std::vector<PrefixState> output;
    output.reserve(raw.size());
    if (options.variant == TraceVariantQueryVariant::kLaplace) {
      for (const auto& [prefix, count] : raw) {
        PrefixState state{
            prefix,
            add_noise(count, laplace_noise(options.seed, prefix,
                                            options.epsilon,
                                            options.secure_random)),
            0};
        if (state.count >= options.prune_threshold)
          output.push_back(std::move(state));
      }
    } else {
      std::vector<PrefixState> conforming;
      std::vector<PrefixState> violating;
      conforming.reserve(raw.size());
      violating.reserve(raw.size());
      for (const auto& [prefix, count] : raw) {
        const auto violations = behavioral_violations(
            prefix, activities, first_events, follows, precedes);
        PrefixState state{prefix, count, violations};
        if (violations == 0)
          conforming.push_back(std::move(state));
        else
          violating.push_back(std::move(state));
      }
      const auto budget = choose_universe(violating.size(), options.epsilon,
                                          options.seed, depth,
                                          options.secure_random);
      const auto selected = select_violations(
          violating, budget, options.seed, depth, options.secure_random);
      if (selected.size() >
          kHardBound - result.diagnostics.selected_violation_count)
        throw std::invalid_argument(
            "trace variant query: selected-violation safety bound exceeded");
      result.diagnostics.selected_violation_count += selected.size();
      output.reserve(raw.size());
      for (auto& state : conforming) {
        state.count = add_noise(
            state.count,
            laplace_noise(options.seed, state.prefix, options.epsilon,
                          options.secure_random));
        output.push_back(std::move(state));
      }
      std::set<std::size_t> selected_set(selected.begin(), selected.end());
      for (std::size_t i = 0; i < violating.size(); ++i) {
        auto state = violating[i];
        if (selected_set.contains(i)) {
          state.count = add_noise(
              state.count,
              laplace_noise(options.seed, state.prefix, options.epsilon,
                            options.secure_random));
        }
        // Non-selected violating candidates retain their known frequency.
        // The exponential mechanism controls which candidates are perturbed,
        // not which candidates exist in the bounded prefix dictionary.
        output.push_back(std::move(state));
      }
    }

    std::map<Prefix, std::uint64_t> next_active;
    for (const auto& state : output) {
      if (state.prefix.ended) {
        add_final(final, state,
                  static_cast<double>(state.count) + state.violations);
      } else if (depth < options.max_prefix_length) {
        if (state.count >= options.prune_threshold)
          next_active.emplace(state.prefix, state.count);
      } else if (options.variant == TraceVariantQueryVariant::kSacofa) {
        // The final depth is deliberately not pruned by the reference
        // semantics; it may release a short prefix even when it is below p.
        add_final(final, state,
                  static_cast<double>(state.count) + state.violations);
      }
    }
    active = std::move(next_active);
    if (active.empty()) break;
  }

  cap_final_states(final, options, result.diagnostics);
  for (const auto& [prefix, state] : final) {
    if (state.count == 0) continue;
    result.frequencies[prefix.activities] = state.count;
  }
  result.diagnostics.retained_variant_count = result.frequencies.size();

  std::uint64_t remaining = options.max_output_traces;
  std::uint64_t case_number = 1;
  for (const auto& [prefix, frequency] : result.frequencies) {
    if (remaining == 0) {
      result.diagnostics.output_truncated = true;
      break;
    }
    const std::uint64_t emit = std::min(frequency, remaining);
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
    remaining -= emit;
    result.diagnostics.released_trace_count += emit;
  }
  result.diagnostics.message = result.diagnostics.output_truncated
                                   ? "output safety bound applied"
                                   : "ok";
  return result;
}

}  // namespace minera
