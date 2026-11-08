#include "minera/model_decoration.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "minera/conformance.hpp"
#include "minera/petri_reference_arcs.hpp"
#include "minera/timestamp.hpp"
#include "performance_arithmetic.hpp"

namespace minera {

namespace {

bool standard_activity_attribute(std::string_view key) {
  return key.empty() || key == "concept:name" || key == "activity";
}

const std::string& selected_activity(const Event& event,
                                     std::string_view activity_attribute) {
  if (standard_activity_attribute(activity_attribute)) return event.activity;
  if (const auto* value = event.attribute(activity_attribute)) return *value;
  throw std::invalid_argument(
      "model decoration: event is missing activity attribute '" +
      std::string(activity_attribute) + "'");
}

bool standard_timestamp_attribute(std::string_view key) {
  return key.empty() || key == "time:timestamp" || key == "timestamp";
}

std::string_view selected_timestamp(const Event& event,
                                    std::string_view timestamp_attribute) {
  if (standard_timestamp_attribute(timestamp_attribute)) return event.timestamp;
  if (const auto* value = event.attribute(timestamp_attribute)) return *value;
  // A missing custom timestamp makes this event untimed. Do not substitute the
  // canonical event timestamp: callers may deliberately select a sparse field.
  return {};
}

// An event's exact instant; unparseable text is untimed. Any two instants of
// the calendar range (year 1 to 9999) have an exact gap (detail::exact_gap).
std::optional<ParsedExactTimestamp> exact_instant(std::string_view text) {
  return parse_timestamp_exact(text);
}

// The wait from `start` to `end` in timedelta seconds, nullopt when negative.
// Beyond exact_gap's bound the wait is a difference of epoch doubles.
std::optional<double> wait_seconds(const ParsedExactTimestamp& start,
                                   const ParsedExactTimestamp& end) {
  if (end.seconds < start.seconds ||
      (end.seconds == start.seconds && end.nanoseconds < start.nanoseconds))
    return std::nullopt;
  if (const auto gap = detail::exact_gap(start, end))
    return detail::eventlog_gap_seconds(*gap);
  return static_cast<double>(end.seconds) - static_cast<double>(start.seconds) +
         (static_cast<double>(end.nanoseconds) - static_cast<double>(start.nanoseconds)) * 1e-9;
}

// Wait samples a place keeps one by one; beyond this, further samples are
// kept as runs of one value, so a huge arc weight costs one entry.
constexpr std::uint64_t kExpandedWaitSamples = std::uint64_t{1} << 24;

// summarize_durations over `singles` plus each run's value repeated its count
// times, without materializing the runs: the count saturates, the total adds
// value * count per run, and the median is found by walking both sorted.
DurationStats summarize_wait_runs(std::vector<double> singles,
                                  std::vector<std::pair<double, std::uint64_t>> runs) {
  std::sort(singles.begin(), singles.end());
  std::sort(runs.begin(), runs.end());
  DurationStats stats;
  std::uint64_t count = singles.size();
  for (const auto& run : runs)
    count = run.second > std::numeric_limits<std::uint64_t>::max() - count
                ? std::numeric_limits<std::uint64_t>::max()
                : count + run.second;
  if (count == 0) return stats;
  stats.count = count;
  const auto low = (count - 1) / 2, high = count / 2;  // the median's ranks
  double total = 0.0, at_low = 0.0, at_high = 0.0;
  bool first = true;
  std::uint64_t seen = 0;
  std::size_t i = 0, j = 0;
  while (i < singles.size() || j < runs.size()) {
    const bool single = j == runs.size() || (i < singles.size() && singles[i] <= runs[j].first);
    const double value = single ? singles[i] : runs[j].first;
    const std::uint64_t n = single ? 1 : runs[j].second;
    if (n == 0) { ++j; continue; }
    if (first) { stats.min = value; first = false; }
    stats.max = value;
    total += single ? value : value * static_cast<double>(n);
    if (seen <= low && low - seen < n) at_low = value;
    if (seen <= high && high - seen < n) at_high = value;
    seen = n > std::numeric_limits<std::uint64_t>::max() - seen
               ? std::numeric_limits<std::uint64_t>::max()
               : seen + n;
    if (single) ++i; else ++j;
  }
  stats.total = total;
  stats.mean = total / static_cast<double>(count);
  stats.median = low == high ? at_low : (at_low + at_high) / 2.0;
  return stats;
}

using FiringSequence = std::vector<std::vector<std::size_t>>;

// Nets with inhibitor or reset arcs keep the exhaustive silent-routing
// replay (other nets use it only for variants token replay rejects). Its marking-graph search is bounded, so a variant can come back
// empty because the search stopped, not because it deviates: those are
// counted as capped. Once kCappedBeforeStop variants have capped without one
// replaying, the rest are not attempted (the same structure would cap them
// too) and are counted as capped as well.
constexpr std::size_t kFallbackChunk = 16;
constexpr std::size_t kSilentRoutingBound = 4096;  // conformance.cpp kSilentSearchCap
constexpr std::uint64_t kCappedBeforeStop = 16;

std::vector<std::optional<FiringSequence>> extended_arc_firing_sequences(
    const PetriNet& net, const std::vector<std::vector<std::string>>& variants,
    const std::function<bool()>& cancelled, std::uint64_t& capped,
    std::uint64_t& skipped) {
  std::vector<std::optional<FiringSequence>> result(variants.size());
  std::uint64_t replayed = 0;
  for (std::size_t begin = 0; begin < variants.size(); begin += kFallbackChunk) {
    if (replayed == 0 && capped >= kCappedBeforeStop) {
      skipped = variants.size() - begin;
      break;
    }
    const auto end = std::min(variants.size(), begin + kFallbackChunk);
    const std::vector<std::vector<std::string>> chunk(
        variants.begin() + static_cast<std::ptrdiff_t>(begin),
        variants.begin() + static_cast<std::ptrdiff_t>(end));
    std::vector<bool> chunk_capped;
    auto sequences = replay_firing_sequences(net, chunk, cancelled, &chunk_capped);
    for (std::size_t i = 0; i < chunk.size(); ++i) {
      if (sequences[i]) {
        ++replayed;
        result[begin + i] = std::move(sequences[i]);
      } else if (chunk_capped[i]) {
        ++capped;
      }
    }
  }
  return result;
}

}  // namespace

ModelDecoration decorate_model(const EventLog& log, const PetriNet& net,
                               std::string_view activity_attribute,
                               std::string_view timestamp_attribute,
                               const std::function<bool()>& cancelled) {
  ModelDecoration result;
  result.transition_counts.assign(net.transitions().size(), 0);
  result.place_counts.assign(net.place_count(), 0);
  result.trace_count = log.traces.size();

  // Replay each distinct variant once; every trace of the variant fires the
  // same transitions, only its instants differ.
  std::vector<std::vector<std::string>> variants;
  std::unordered_map<std::string, std::size_t> variant_ids;
  std::vector<std::size_t> variant_of(log.traces.size());
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    std::string key;
    for (const auto& event : log.traces[i].events) {
      key.push_back('\x1f');
      key += selected_activity(event, activity_attribute);
    }
    const auto [it, inserted] =
        variant_ids.emplace(std::move(key), variants.size());
    if (inserted) {
      std::vector<std::string> activities;
      activities.reserve(log.traces[i].events.size());
      for (const auto& event : log.traces[i].events) {
        activities.push_back(selected_activity(event, activity_attribute));
      }
      variants.push_back(std::move(activities));
    }
    variant_of[i] = it->second;
  }
  std::uint64_t capped = 0, skipped = 0;
  std::vector<std::optional<FiringSequence>> firing;
  if (net.has_extended_arcs()) {
    firing = extended_arc_firing_sequences(net, variants, cancelled, capped, skipped);
  } else {
    firing = token_replay_firing_sequences(net, variants, cancelled);
    // Token replay fires silent transitions greedily, so it can reject a
    // variant that does have a clean path through the net. Those variants
    // still get the bounded marking-graph search the decoration used before;
    // a search that stops at its bound only leaves the variant undecorated,
    // as the replay already judged it deviating.
    std::vector<std::size_t> rejected;
    for (std::size_t v = 0; v < firing.size(); ++v)
      if (!firing[v]) rejected.push_back(v);
    if (!rejected.empty()) {
      std::vector<std::vector<std::string>> retry;
      retry.reserve(rejected.size());
      for (const auto v : rejected) retry.push_back(variants[v]);
      std::uint64_t retry_capped = 0, retry_skipped = 0;
      auto searched =
          extended_arc_firing_sequences(net, retry, cancelled, retry_capped, retry_skipped);
      for (std::size_t i = 0; i < rejected.size(); ++i)
        if (searched[i]) firing[rejected[i]] = std::move(searched[i]);
    }
  }
  result.capped_variants = capped + skipped;

  // Wait samples per place, reduced to stats at the end: one by one up to
  // kExpandedWaitSamples, then as (value, count) runs.
  std::vector<std::vector<double>> waits(net.place_count());
  std::vector<std::vector<std::pair<double, std::uint64_t>>> wait_runs(net.place_count());

  // Token instants resting in each place, oldest first. nullopt marks a token
  // produced by an untimed event; it moves like any other but yields no wait.
  std::vector<std::deque<std::pair<std::optional<ParsedExactTimestamp>, std::uint64_t>>> tokens(
      net.place_count());

  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    const auto& sequence = firing[variant_of[i]];
    if (!sequence) continue;  // deviating trace: not a real path through the net
    ++result.decorated_traces;
    const auto& trace = log.traces[i];

    for (auto& queue : tokens) queue.clear();

    // The initial token carries the case's first instant, so its wait in the
    // source measures delay before the process starts moving, not the span
    // back to some epoch.
    std::optional<ParsedExactTimestamp> first_instant;
    for (const auto& event : trace.events) {
      first_instant = exact_instant(
          selected_timestamp(event, timestamp_attribute));
      if (first_instant) break;
    }
    std::optional<ParsedExactTimestamp> last_instant;
    for (auto it = trace.events.rbegin(); it != trace.events.rend(); ++it) {
      last_instant = exact_instant(
          selected_timestamp(*it, timestamp_attribute));
      if (last_instant) break;
    }
    tokens[net.source()].emplace_back(first_instant, 1);

    // One firing list per consumed event — fired at that event's instant, or
    // untimed (yielding no wait samples) when the event has none — then one
    // for the routing to the sink, at the case's last instant.
    for (std::size_t step = 0; step < sequence->size(); ++step) {
      const auto now = step < trace.events.size()
                           ? exact_instant(selected_timestamp(
                                 trace.events[step], timestamp_attribute))
                           : last_instant;
      for (const auto transition_id : (*sequence)[step]) {
        ++result.transition_counts[transition_id];
        const auto& transition = net.transitions()[transition_id];
        // Each ordinary arc moves its own weight in tokens, as the replay
        // fired it (repeated unit endpoints fold into one arc of that weight).
        // Tokens are kept as runs of equal instants, so a large weight costs
        // one entry, not one per token.
        for (const auto& arc : petri_reference_arcs(transition, true)) {
          if (arc.negative) continue;
          const auto place = arc.place;
          auto remaining = arc.weight;
          result.place_counts[place] = remaining > TokenInteger(std::numeric_limits<std::uint64_t>::max() - result.place_counts[place])
              ? std::numeric_limits<std::uint64_t>::max()
              : result.place_counts[place] + static_cast<std::uint64_t>(remaining);
          // A clean replay never fires without its tokens; the guard is
          // belt-and-braces against an inconsistent sequence.
          while (remaining > 0 && !tokens[place].empty()) {
            auto& [produced, count] = tokens[place].front();
            const std::uint64_t taken = remaining < TokenInteger(count)
                                            ? static_cast<std::uint64_t>(remaining)
                                            : count;
            const auto measured = produced && now ? wait_seconds(*produced, *now)
                                                  : std::optional<double>{};
            if (measured) {
              // One wait sample per token; samples that would not fit one by
              // one are kept as a run instead. The wait is the exact gap
              // (timedelta seconds), not a difference of epoch doubles.
              const double wait = *measured;
              if (wait_runs[place].empty() && taken <= kExpandedWaitSamples - waits[place].size())
                waits[place].insert(waits[place].end(), taken, wait);
              else
                wait_runs[place].emplace_back(wait, taken);
            }
            remaining = remaining - TokenInteger(taken);
            count -= taken;
            if (count == 0) tokens[place].pop_front();
          }
        }
        for (const auto& arc : petri_reference_arcs(transition, false)) {
          if (arc.negative || arc.weight == 0) continue;
          const auto count = arc.weight > TokenInteger(std::numeric_limits<std::uint64_t>::max())
                                 ? std::numeric_limits<std::uint64_t>::max()
                                 : static_cast<std::uint64_t>(arc.weight);
          tokens[arc.place].emplace_back(now, count);
        }
      }
    }
  }

  if (result.capped_variants != 0) {
    result.message =
        std::to_string(capped) + " of " + std::to_string(variants.size()) +
        " variants were not decorated: on this net with inhibitor or reset arcs "
        "their silent routing exceeded the replay's search bound of " +
        std::to_string(kSilentRoutingBound) + " markings";
    if (skipped != 0)
      result.message += "; the other " + std::to_string(skipped) +
                        " were skipped after every attempted variant exceeded it";
  } else if (result.decorated_traces == 0 && result.trace_count != 0) {
    result.message = "no trace replays cleanly on this model, so it carries no decoration";
  }

  result.place_waits.reserve(net.place_count());
  for (std::size_t place = 0; place < waits.size(); ++place) {
    result.place_waits.push_back(
        wait_runs[place].empty()
            ? summarize_durations(std::move(waits[place]))
            : summarize_wait_runs(std::move(waits[place]), std::move(wait_runs[place])));
  }
  return result;
}

}  // namespace minera
