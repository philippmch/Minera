#include "minera/alpha_miner.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
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

// The log footprint: which activities directly follow which, plus the start and
// end activities. All higher-level relations derive from this matrix.
struct Footprint {
  std::vector<std::string> names;
  std::vector<std::vector<bool>> follows;  // follows[a][b]: a directly precedes b
  std::set<std::size_t> starts;
  std::set<std::size_t> ends;

  [[nodiscard]] std::size_t size() const { return names.size(); }
  [[nodiscard]] bool causal(std::size_t a, std::size_t b) const {
    return follows[a][b] && !follows[b][a];
  }
  [[nodiscard]] bool choice(std::size_t a, std::size_t b) const {
    return !follows[a][b] && !follows[b][a];
  }
};

Footprint build_footprint(const EventLog& log) {
  Footprint fp;
  std::unordered_map<std::string, std::size_t> ids;
  const auto intern = [&](const std::string& name) {
    const auto [it, inserted] = ids.try_emplace(name, fp.names.size());
    if (inserted) fp.names.push_back(name);
    return it->second;
  };

  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) intern(event.activity);
  }

  const std::size_t n = fp.names.size();
  fp.follows.assign(n, std::vector<bool>(n, false));
  for (const auto& trace : log.traces) {
    const auto& events = trace.events;
    if (events.empty()) continue;
    fp.starts.insert(ids[events.front().activity]);
    fp.ends.insert(ids[events.back().activity]);
    for (std::size_t i = 1; i < events.size(); ++i) {
      fp.follows[ids[events[i - 1].activity]][ids[events[i].activity]] = true;
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

PetriNet discover_petri_net(const EventLog& log) {
  const Footprint fp = build_footprint(log);
  const std::size_t n = fp.size();

  PetriNet net;
  net.set_activities(fp.names);
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

  for (const auto& [a, b] : maximal_places(fp)) {
    const std::size_t place = net.add_place();
    for (const auto from : a) outputs[from].push_back(place);
    for (const auto to : b) inputs[to].push_back(place);
  }

  for (std::size_t activity = 0; activity < n; ++activity) {
    net.add_transition(activity, std::move(inputs[activity]),
                       std::move(outputs[activity]));
  }
  return net;
}

}  // namespace minera
