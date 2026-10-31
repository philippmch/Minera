#include "minera/oc_causal_net_reference_playout.hpp"

#include "portable_time.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_map>
#include <ctime>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

namespace minera {
namespace {

using Obligation = std::tuple<std::string, std::string, std::string, std::string>;  // from, to, type, object
using ObjectSet = std::vector<std::string>;                                        // sorted

// Every subset of `items` (sorted) with a size in [low, high].
std::vector<ObjectSet> subsets(const ObjectSet& items, std::size_t low, std::size_t high) {
  std::vector<ObjectSet> out;
  const auto n = items.size();
  if (n > 20) throw std::length_error("OC causal net playout binds too many objects");
  for (std::size_t size = low; size <= std::min(high, n); ++size) {
    std::vector<bool> pick(n, false);
    std::fill(pick.begin(), pick.begin() + static_cast<std::ptrdiff_t>(size), true);
    do {
      ObjectSet chosen;
      for (std::size_t i = 0; i < n; ++i)
        if (pick[i]) chosen.push_back(items[i]);
      out.push_back(std::move(chosen));
    } while (std::prev_permutation(pick.begin(), pick.end()));
  }
  return out;
}

bool intersects(const ObjectSet& a, const ObjectSet& b) {
  for (const auto& x : a)
    if (std::binary_search(b.begin(), b.end(), x)) return true;
  return false;
}

// Per marker of a group, a subset of its available objects within the
// marker's cardinality; markers with the same key and type take disjoint sets.
std::vector<std::vector<ObjectSet>> assignments(const OcCausalMarkerGroup& group,
                                                const std::vector<ObjectSet>& available,
                                                const std::vector<std::int64_t>& keys) {
  std::vector<std::vector<ObjectSet>> out;
  std::vector<ObjectSet> chosen;
  std::vector<std::vector<ObjectSet>> options;
  for (std::size_t i = 0; i < group.markers.size(); ++i)
    options.push_back(subsets(available[i], group.markers[i].min_cardinality, group.markers[i].max_cardinality));
  const auto recurse = [&](auto&& self, std::size_t index) -> void {
    if (index == group.markers.size()) {
      out.push_back(chosen);
      return;
    }
    for (const auto& option : options[index]) {
      bool clash = false;
      for (std::size_t j = 0; j < index && !clash; ++j)
        clash = keys[j] == keys[index] && group.markers[j].object_type == group.markers[index].object_type &&
                intersects(option, chosen[j]);
      if (clash) continue;
      chosen.push_back(option);
      self(self, index + 1);
      chosen.pop_back();
    }
  };
  recurse(recurse, 0);
  return out;
}

std::vector<OcCausalReferenceObligations> side(const OcCausalMarkerGroup& group, const std::vector<ObjectSet>& sets) {
  std::vector<OcCausalReferenceObligations> out;
  for (std::size_t i = 0; i < sets.size(); ++i)
    if (!sets[i].empty()) out.push_back({group.markers[i].related_activity, group.markers[i].object_type, sets[i]});
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    return std::tie(a.activity, a.object_type, a.objects) < std::tie(b.activity, b.object_type, b.objects);
  });
  return out;
}

bool same(const OcCausalReferenceBinding& a, const OcCausalReferenceBinding& b) {
  const auto equal = [](const auto& x, const auto& y) {
    if (x.has_value() != y.has_value()) return false;
    if (!x) return true;
    if (x->size() != y->size()) return false;
    for (std::size_t i = 0; i < x->size(); ++i)
      if (std::tie((*x)[i].activity, (*x)[i].object_type, (*x)[i].objects) !=
          std::tie((*y)[i].activity, (*y)[i].object_type, (*y)[i].objects))
        return false;
    return true;
  };
  return a.activity == b.activity && equal(a.consumed, b.consumed) && equal(a.produced, b.produced);
}

struct Search {
  const OcCausalNet* net = nullptr;
  std::size_t bound = 1;
  // Marker keys with zero replaced by a key of its own, per activity,
  // side (0 input, 1 output), group and marker.
  std::vector<std::array<std::vector<std::vector<std::int64_t>>, 2>> keys;
  std::map<Obligation, std::size_t> state;
  std::map<std::string, std::set<std::string>> start;  // remaining start obligations per type
  std::vector<std::size_t> fired;
  OcCausalReferenceSequence current;
  std::vector<OcCausalReferenceSequence> sequences;
  // Like the reference, a search state (obligations, remaining start
  // obligations and per-activity binding counts) is explored once: its
  // completing suffixes are shared and a dead state is not entered again.
  std::optional<std::chrono::steady_clock::time_point> deadline;
  double time_limit_seconds = 0;
  std::size_t steps = 0;

  std::string state_key() const {
    std::string key;
    for (const auto& [obligation, count] : state) {
      const auto& [from, to, type, object] = obligation;
      for (const auto* part : {&from, &to, &type, &object}) {
        key += *part;
        key += '\0';
      }
      key += std::to_string(count);
      key += '\1';
    }
    key += '\2';
    for (const auto& [type, objects] : start) {
      key += type;
      key += '\0';
      for (const auto& object : objects) {
        key += object;
        key += '\0';
      }
      key += '\1';
    }
    key += '\2';
    for (const auto count : fired) {
      key += std::to_string(count);
      key += ',';
    }
    return key;
  }

  ObjectSet obligated(const std::string& from, const std::string& to, const std::string& type) const {
    ObjectSet out;
    for (auto it = state.lower_bound({from, to, type, ""}); it != state.end(); ++it) {
      const auto& [f, t, y, object] = it->first;
      if (f != from || t != to || y != type) break;
      if (it->second) out.push_back(object);
    }
    return out;
  }

  // Every enabled binding of the activity, without repetitions.
  std::vector<OcCausalReferenceBinding> bindings(std::size_t index) const {
    const auto& activity = net->activities[index];
    // (consumed, objects per type)
    std::vector<std::pair<std::optional<std::vector<OcCausalReferenceObligations>>,
                          std::map<std::string, ObjectSet>>> inputs;
    if (activity.activity.rfind("START_", 0) == 0) {
      const auto type = activity.activity.substr(6);
      const auto found = start.find(type);
      if (found != start.end()) {
        const ObjectSet available(found->second.begin(), found->second.end());
        for (auto& chosen : subsets(available, 1, available.size()))
          inputs.push_back({std::nullopt, {{type, std::move(chosen)}}});
      }
    } else {
      for (std::size_t g = 0; g < activity.input_marker_groups.size(); ++g) {
        const auto& group = activity.input_marker_groups[g];
        std::vector<ObjectSet> available;
        for (const auto& marker : group.markers)
          available.push_back(obligated(marker.related_activity, activity.activity, marker.object_type));
        for (const auto& chosen : assignments(group, available, keys[index][0][g])) {
          std::map<std::string, ObjectSet> objects;
          bool any = false;
          for (std::size_t m = 0; m < chosen.size(); ++m) {
            auto& set = objects[group.markers[m].object_type];
            ObjectSet merged;
            std::set_union(set.begin(), set.end(), chosen[m].begin(), chosen[m].end(), std::back_inserter(merged));
            set = std::move(merged);
            any = any || !chosen[m].empty();
          }
          if (!any) continue;
          for (auto it = objects.begin(); it != objects.end();) it = it->second.empty() ? objects.erase(it) : std::next(it);
          inputs.push_back({side(group, chosen), std::move(objects)});
        }
      }
    }
    std::vector<OcCausalReferenceBinding> out;
    const auto add = [&](OcCausalReferenceBinding binding) {
      for (const auto& existing : out)
        if (same(existing, binding)) return;
      out.push_back(std::move(binding));
    };
    if (!inputs.empty() && activity.output_marker_groups.empty() && activity.activity.rfind("END_", 0) != 0)
      throw OcCausalReferenceError("KeyError", "'" + activity.activity + "'");
    for (const auto& [consumed, objects] : inputs) {
      if (activity.output_marker_groups.empty()) {
        add({activity.activity, consumed, std::nullopt});
        continue;
      }
      for (std::size_t g = 0; g < activity.output_marker_groups.size(); ++g) {
        const auto& group = activity.output_marker_groups[g];
        bool typed = true;
        for (const auto& [type, set] : objects) {
          bool has = false;
          for (const auto& marker : group.markers) has = has || marker.object_type == type;
          typed = typed && has;
        }
        if (!typed) continue;
        std::vector<ObjectSet> available;
        for (const auto& marker : group.markers) {
          const auto found = objects.find(marker.object_type);
          available.push_back(found == objects.end() ? ObjectSet{} : found->second);
        }
        for (const auto& chosen : assignments(group, available, keys[index][1][g])) {
          std::map<std::string, ObjectSet> cover;
          for (std::size_t m = 0; m < chosen.size(); ++m) {
            auto& set = cover[group.markers[m].object_type];
            ObjectSet merged;
            std::set_union(set.begin(), set.end(), chosen[m].begin(), chosen[m].end(), std::back_inserter(merged));
            set = std::move(merged);
          }
          for (auto it = cover.begin(); it != cover.end();) it = it->second.empty() ? cover.erase(it) : std::next(it);
          if (cover != objects) continue;
          add({activity.activity, consumed, side(group, chosen)});
        }
      }
    }
    return out;
  }

  void apply(const OcCausalReferenceBinding& binding, bool forward) {
    const auto change = [&](const Obligation& key, bool add) {
      if (add) {
        ++state[key];
      } else if (--state[key] == 0) {
        state.erase(key);
      }
    };
    if (binding.consumed) {
      for (const auto& entry : *binding.consumed)
        for (const auto& object : entry.objects)
          change({entry.activity, binding.activity, entry.object_type, object}, !forward);
    }
    if (binding.produced) {
      for (const auto& entry : *binding.produced)
        for (const auto& object : entry.objects)
          change({binding.activity, entry.activity, entry.object_type, object}, forward);
    }
  }

  // Search graph: a node is a search state that completes at least one
  // sequence; its edges are the bindings leading to such states, in search
  // order. Returns the node, or -1 for a dead state.
  struct Node {
    bool complete = false;
    std::vector<std::pair<std::size_t, std::int64_t>> edges;  // binding, child
  };
  std::vector<Node> nodes;
  std::vector<OcCausalReferenceBinding> pool;
  std::unordered_map<std::string, std::int64_t> memo;

  std::int64_t explore() {
    bool empty = state.empty();
    for (const auto& [type, objects] : start) empty = empty && objects.empty();
    if (empty) {
      nodes.push_back({true, {}});
      return static_cast<std::int64_t>(nodes.size() - 1);
    }
    if (deadline && (++steps & 255) == 0 && std::chrono::steady_clock::now() > *deadline)
      throw std::runtime_error("OC causal net playout exceeded its " +
                               std::to_string(static_cast<long long>(time_limit_seconds)) +
                               "-second limit; lower max_bindings_per_activity or the objects");
    auto key = state_key();
    if (const auto found = memo.find(key); found != memo.end()) return found->second;
    // An activity other than START_<type> without input marker groups raises
    // as soon as the search looks for enabled bindings.
    for (const auto& activity : net->activities)
      if (activity.input_marker_groups.empty() && activity.activity.rfind("START_", 0) != 0)
        throw OcCausalReferenceError("KeyError", "'" + activity.activity + "'");
    Node node;
    for (std::size_t index = 0; index < net->activities.size(); ++index) {
      if (fired[index] >= bound) continue;
      const auto& name = net->activities[index].activity;
      for (auto& binding : bindings(index)) {
        // A START binding consumes the start obligations of its objects.
        ObjectSet started;
        if (!binding.consumed && name.rfind("START_", 0) == 0) {
          for (const auto& entry : *binding.produced) {
            ObjectSet merged;
            std::set_union(started.begin(), started.end(), entry.objects.begin(), entry.objects.end(),
                           std::back_inserter(merged));
            started = std::move(merged);
          }
          for (const auto& object : started) start[name.substr(6)].erase(object);
        }
        apply(binding, true);
        ++fired[index];
        const auto child = explore();
        --fired[index];
        apply(binding, false);
        for (const auto& object : started) start[name.substr(6)].insert(object);
        if (child >= 0) {
          pool.push_back(std::move(binding));
          node.edges.push_back({pool.size() - 1, child});
        }
      }
    }
    std::int64_t id = -1;
    if (!node.edges.empty()) {
      nodes.push_back(std::move(node));
      id = static_cast<std::int64_t>(nodes.size() - 1);
    }
    memo.emplace(std::move(key), id);
    return id;
  }

  // Every complete sequence, in search order.
  void run() {
    const auto root = explore();
    if (root < 0) return;
    const auto walk = [&](auto&& self, std::int64_t id) -> void {
      const auto& node = nodes[static_cast<std::size_t>(id)];
      if (node.complete) sequences.push_back(current);
      for (const auto& [binding, child] : node.edges) {
        current.push_back(pool[binding]);
        self(self, child);
        current.pop_back();
      }
    };
    walk(walk, root);
  }
};

std::string timestamp(long long seconds) {
  const std::time_t value = static_cast<std::time_t>(seconds);
  std::tm parts{};
  utc_fields(value, parts);
  char buffer[32];
  std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &parts);
  return buffer;
}

}  // namespace

std::vector<OcCausalReferenceSequence> reference_oc_causal_net_sequences(
    const OcCausalNet& net, const std::map<std::string, std::vector<std::string>>& objects,
    const OcCausalReferencePlayoutOptions& options) {
  Search search;
  search.net = &net;
  search.bound = options.max_bindings_per_activity;
  std::int64_t own = std::numeric_limits<std::int64_t>::min();
  for (const auto& activity : net.activities) {
    auto& keys = search.keys.emplace_back();
    for (int s = 0; s < 2; ++s)
      for (const auto& group : s == 0 ? activity.input_marker_groups : activity.output_marker_groups) {
        auto& row = keys[s].emplace_back();
        for (const auto& marker : group.markers) row.push_back(marker.marker_key == 0 ? own++ : marker.marker_key);
      }
  }
  for (const auto& [type, ids] : objects) {
    bool has_start = false;
    for (const auto& activity : net.activities) has_start = has_start || activity.activity == "START_" + type;
    if (!has_start) throw OcCausalReferenceError("KeyError", "'START_" + type + "'");
    search.start[type] = std::set<std::string>(ids.begin(), ids.end());
  }
  search.fired.assign(net.activities.size(), 0);
  if (options.time_limit_seconds > 0) {
    search.time_limit_seconds = options.time_limit_seconds;
    search.deadline = std::chrono::steady_clock::now() +
                      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(options.time_limit_seconds));
  }
  search.run();
  return std::move(search.sequences);
}

Ocel reference_oc_causal_net_playout(const OcCausalNet& net,
                                     const std::map<std::string, std::vector<std::string>>& objects,
                                     const OcCausalReferencePlayoutOptions& options) {
  const auto sequences = reference_oc_causal_net_sequences(net, objects, options);
  Ocel log;
  std::set<std::string> seen;
  long long clock = 10000001;
  for (std::size_t index = 0; index < sequences.size(); ++index) {
    for (const auto& binding : sequences[index]) {
      if (binding.activity.rfind("START_", 0) == 0 || binding.activity.rfind("END_", 0) == 0) continue;
      // One relation per consumed obligation: an object consumed from two
      // activities is related twice.
      std::vector<std::pair<std::string, std::string>> bound;  // object, type
      for (const auto& entry : *binding.consumed)
        for (const auto& object : entry.objects) bound.emplace_back(object, entry.object_type);
      const auto id = "event_" + std::to_string(log.events.size());
      log.events.push_back({id, binding.activity, timestamp(clock++), {}});
      for (const auto& [object, type] : bound) {
        const auto name = options.objects_unique_per_sequence ? object + "_" + std::to_string(index) : object;
        log.relations.push_back({id, name, ""});
        if (seen.insert(name).second) log.objects.push_back({name, type, {}});
      }
    }
  }
  return log;
}

}  // namespace minera
