#include "minera/alpha_miner.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

// A sorted, duplicate-free set of activity ids. Kept sorted so set operations
// (union, subset, equality) are cheap and pairs deduplicate in an ordered set.
using ActivitySet = std::vector<std::size_t>;
using Place = std::pair<ActivitySet, ActivitySet>;

ActivitySet unite(const ActivitySet& a, const ActivitySet& b) {
  ActivitySet out;
  std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
  return out;
}

bool is_subset(const ActivitySet& sub, const ActivitySet& sup) {
  return std::includes(sup.begin(), sup.end(), sub.begin(), sub.end());
}

// The log with activity names interned to dense ids, so the full-log and
// reduced-log footprints of the Alpha+ variant share one id space.
struct IdLog {
  std::vector<std::string> names;
  std::vector<std::vector<std::size_t>> traces;
};

IdLog intern_log(const EventLog& log) {
  IdLog out;
  std::unordered_map<std::string, std::size_t> ids;
  for (const auto& trace : log.traces) {
    std::vector<std::size_t> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto [it, inserted] =
          ids.try_emplace(event.activity, out.names.size());
      if (inserted) out.names.push_back(event.activity);
      sequence.push_back(it->second);
    }
    out.traces.push_back(std::move(sequence));
  }
  return out;
}

// The log footprint: which activities directly follow which, plus the start and
// end activities. All higher-level relations derive from this matrix. With
// short_loops set, "a b a" patterns are also recorded so that a length-two loop
// — where both directions of succession appear — can be told apart from true
// concurrency when reading causality.
struct Footprint {
  std::vector<std::vector<bool>> follows;   // follows[a][b]: a directly precedes b
  std::vector<std::vector<bool>> two_loop;  // two_loop[a][b]: "a b a" occurs
  std::set<std::size_t> starts;
  std::set<std::size_t> ends;
  bool short_loops = false;

  [[nodiscard]] std::size_t size() const { return follows.size(); }
  [[nodiscard]] bool causal(std::size_t a, std::size_t b) const {
    if (!follows[a][b]) return false;
    if (!follows[b][a]) return true;
    // Succession in both directions: concurrency for the classic miner, but a
    // length-two loop — hence causal both ways — when "a b a" and "b a b" both
    // occur, which interleaved concurrent activities cannot produce.
    return short_loops && two_loop[a][b] && two_loop[b][a];
  }
  [[nodiscard]] bool choice(std::size_t a, std::size_t b) const {
    return !follows[a][b] && !follows[b][a];
  }
};

// Builds the footprint over the interned log. Activities marked in `exclude`
// (the Alpha+ length-one-loop removal) are read as absent from every trace.
Footprint build_footprint(const IdLog& log, const std::vector<bool>* exclude,
                          bool short_loops) {
  const std::size_t n = log.names.size();
  Footprint fp;
  fp.short_loops = short_loops;
  fp.follows.assign(n, std::vector<bool>(n, false));
  if (short_loops) fp.two_loop.assign(n, std::vector<bool>(n, false));

  std::vector<std::size_t> sequence;
  for (const auto& trace : log.traces) {
    sequence.clear();
    for (const auto id : trace) {
      if (!exclude || !(*exclude)[id]) sequence.push_back(id);
    }
    if (sequence.empty()) continue;
    fp.starts.insert(sequence.front());
    fp.ends.insert(sequence.back());
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      fp.follows[sequence[i - 1]][sequence[i]] = true;
      if (short_loops && i >= 2 && sequence[i - 2] == sequence[i]) {
        fp.two_loop[sequence[i]][sequence[i - 1]] = true;
      }
    }
  }
  return fp;
}

// A set is admissible on one side of a place only if its members are pairwise
// in the choice relation and none has a self-loop — both required for the place
// to be a valid Petri-net construct under the Alpha algorithm.
bool admissible(const ActivitySet& set, const Footprint& fp) {
  for (std::size_t i = 0; i < set.size(); ++i) {
    if (fp.follows[set[i]][set[i]]) return false;  // self-loop: a is not a # a
    for (std::size_t j = i + 1; j < set.size(); ++j) {
      if (!fp.choice(set[i], set[j])) return false;
    }
  }
  return true;
}

bool all_causal(const ActivitySet& a, const ActivitySet& b, const Footprint& fp) {
  for (const auto from : a) {
    for (const auto to : b) {
      if (!fp.causal(from, to)) return false;
    }
  }
  return true;
}

std::vector<Place> maximal_places(const Footprint& fp);

PetriNet build_classic_net(const std::vector<std::string>& names,
                           const Footprint& fp) {
  PetriNet net;
  net.set_activities(names);
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  const std::size_t n = fp.size();
  std::vector<std::vector<std::size_t>> inputs(n);
  std::vector<std::vector<std::size_t>> outputs(n);
  for (const auto start : fp.starts) inputs[start].push_back(source);
  for (const auto end : fp.ends) outputs[end].push_back(sink);

  for (const auto& pair : maximal_places(fp)) {
    const std::size_t place = net.add_place();
    for (const auto from : pair.first) outputs[from].push_back(place);
    for (const auto to : pair.second) inputs[to].push_back(place);
  }

  for (std::size_t activity = 0; activity < n; ++activity) {
    net.add_transition(activity, std::move(inputs[activity]),
                       std::move(outputs[activity]));
  }
  return net;
}

Footprint footprint_from_graph(const DirectlyFollowsGraph& graph) {
  Footprint fp;
  const auto& names = graph.activities();
  const std::size_t n = names.size();
  fp.follows.assign(n, std::vector<bool>(n, false));
  for (const auto& edge : graph.edges()) {
    if (edge.source >= n || edge.target >= n)
      throw std::invalid_argument("Alpha: graph edge activity id is out of range");
    fp.follows[edge.source][edge.target] = true;
  }
  for (std::size_t activity = 0; activity < n; ++activity) {
    if (graph.start_activity_count(names[activity]) != 0)
      fp.starts.insert(activity);
    if (graph.end_activity_count(names[activity]) != 0)
      fp.ends.insert(activity);
  }
  return fp;
}

// Enumerates the maximal place candidates (A, B). Seeds with singleton causal
// pairs, then grows either side by union — keeping the other side fixed — until
// no new admissible pair appears. Growing one side at a time reaches every
// maximal pair, after which non-maximal pairs are dropped.
std::vector<Place> maximal_places(const Footprint& fp) {
  std::set<Place> candidates;
  const std::size_t n = fp.size();
  for (std::size_t a = 0; a < n; ++a) {
    if (fp.follows[a][a]) continue;
    for (std::size_t b = 0; b < n; ++b) {
      if (fp.follows[b][b]) continue;
      if (fp.causal(a, b)) candidates.insert({{a}, {b}});
    }
  }

  for (bool changed = true; changed;) {
    changed = false;
    const std::vector<Place> snapshot(candidates.begin(), candidates.end());
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
      for (std::size_t j = i + 1; j < snapshot.size(); ++j) {
        const auto& [a1, b1] = snapshot[i];
        const auto& [a2, b2] = snapshot[j];
        if (b1 == b2) {
          auto a = unite(a1, a2);
          if (a != a1 && admissible(a, fp) && all_causal(a, b1, fp)) {
            changed |= candidates.insert({std::move(a), b1}).second;
          }
        }
        if (a1 == a2) {
          auto b = unite(b1, b2);
          if (b != b1 && admissible(b, fp) && all_causal(a1, b, fp)) {
            changed |= candidates.insert({a1, std::move(b)}).second;
          }
        }
      }
    }
  }

  const std::vector<Place> all(candidates.begin(), candidates.end());
  std::vector<Place> maximal;
  for (const auto& place : all) {
    const bool dominated = std::any_of(
        all.begin(), all.end(), [&](const Place& other) {
          return other != place && is_subset(place.first, other.first) &&
                 is_subset(place.second, other.second);
        });
    if (!dominated) maximal.push_back(place);
  }
  return maximal;
}

}  // namespace

PetriNet discover_petri_net(const EventLog& log,
                            const AlphaMinerOptions& options) {
  const IdLog ids = intern_log(log);
  const std::size_t n = ids.names.size();

  // Alpha+ first sets aside the length-one-loop activities (those that directly
  // follow themselves): they can never sit on a valid place, so the place
  // search runs on the log read without them, and each is re-attached to the
  // place between its neighbours afterwards.
  const Footprint full = build_footprint(ids, nullptr, options.short_loops);
  if (!options.short_loops) return build_classic_net(ids.names, full);
  std::vector<bool> loops(n, false);
  bool any_loop = false;
  if (options.short_loops) {
    for (std::size_t a = 0; a < n; ++a) {
      loops[a] = full.follows[a][a];
      any_loop |= loops[a];
    }
  }
  const Footprint reduced =
      any_loop ? build_footprint(ids, &loops, true) : Footprint{};
  const Footprint& fp = any_loop ? reduced : full;

  PetriNet net;
  net.set_activities(ids.names);
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  // Accumulate each activity's input and output places, then emit one
  // transition per activity. The source feeds every start activity; every end
  // activity drains into the sink.
  std::vector<std::vector<std::size_t>> inputs(n);
  std::vector<std::vector<std::size_t>> outputs(n);
  for (const auto start : fp.starts) inputs[start].push_back(source);
  for (const auto end : fp.ends) outputs[end].push_back(sink);

  std::vector<std::pair<Place, std::size_t>> placed;
  for (auto& pair : maximal_places(fp)) {
    const std::size_t place = net.add_place();
    for (const auto from : pair.first) outputs[from].push_back(place);
    for (const auto to : pair.second) inputs[to].push_back(place);
    placed.emplace_back(std::move(pair), place);
  }

  // Re-attach each length-one loop t as a self loop on the place between its
  // neighbours A = {a | a > t} and B = {b | t > b} (other loops excluded): t
  // borrows the token and puts it back, leaving the surrounding flow intact.
  // On a well-formed log (A, B) is exactly one of the discovered places; on a
  // noisy one, fall back to the largest place nested inside the pair.
  for (std::size_t t = 0; any_loop && t < n; ++t) {
    if (!loops[t]) continue;
    ActivitySet a_side;
    ActivitySet b_side;
    for (std::size_t other = 0; other < n; ++other) {
      if (loops[other]) continue;
      if (full.follows[other][t]) a_side.push_back(other);
      if (full.follows[t][other]) b_side.push_back(other);
    }
    const std::pair<Place, std::size_t>* best = nullptr;
    for (const auto& candidate : placed) {
      if (candidate.first.first == a_side && candidate.first.second == b_side) {
        best = &candidate;
        break;
      }
      if (!is_subset(candidate.first.first, a_side) ||
          !is_subset(candidate.first.second, b_side)) {
        continue;
      }
      const auto span = [](const Place& place) {
        return place.first.size() + place.second.size();
      };
      if (!best || span(candidate.first) > span(best->first)) best = &candidate;
    }
    if (best) {
      inputs[t].push_back(best->second);
      outputs[t].push_back(best->second);
    }
  }

  for (std::size_t activity = 0; activity < n; ++activity) {
    net.add_transition(activity, std::move(inputs[activity]),
                       std::move(outputs[activity]));
  }
  return net;
}

PetriNet discover_petri_net(const DirectlyFollowsGraph& graph,
                            const AlphaMinerOptions& options) {
  if (options.short_loops) {
    throw std::invalid_argument(
        "Alpha: Alpha+ requires event-log trace context; direct graph input supports classic only");
  }
  return build_classic_net(graph.activities(), footprint_from_graph(graph));
}

}  // namespace minera
