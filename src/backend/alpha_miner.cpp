#include "minera/alpha_miner.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/alpha_relations.hpp"
#include "minera/frame_keys.hpp"

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
  std::vector<std::optional<std::string>> names;
  std::vector<std::vector<std::size_t>> traces;
};

// Canonical zero-key SipHash13 over the narrowest Unicode code-point width.
// This independently written policy is checked against public builtin hash
// observations. Runtime-randomized keys and identity hashes remain separate.
std::optional<std::uint64_t> alpha_string_hash(std::string_view text) {
  if (text.empty()) return 0;
  std::vector<std::uint32_t> points;
  std::uint32_t largest = 0;
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i++]);
    std::uint32_t point = lead;
    std::size_t extra = 0;
    std::uint32_t minimum = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
      point = lead & 0x1f; extra = 1; minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      point = lead & 0x0f; extra = 2; minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      point = lead & 7; extra = 3; minimum = 0x10000;
    } else if (lead >= 0x80) {
      return std::nullopt;
    }
    if (extra > text.size() - i) return std::nullopt;
    for (std::size_t j = 0; j < extra; ++j) {
      const auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) return std::nullopt;
      point = (point << 6) | (next & 0x3f);
    }
    if (point < minimum || point > 0x10ffff ||
        (point >= 0xd800 && point <= 0xdfff)) return std::nullopt;
    points.push_back(point);
    largest = std::max(largest, point);
  }
  const std::size_t width = largest <= 0xff ? 1 : largest <= 0xffff ? 2 : 4;
  std::vector<unsigned char> bytes;
  bytes.reserve(points.size() * width);
  for (const auto point : points)
    for (std::size_t j = 0; j < width; ++j)
      bytes.push_back(static_cast<unsigned char>(point >> (8 * j)));
  std::uint64_t a = 0x736f6d6570736575ULL, b = 0x646f72616e646f6dULL;
  std::uint64_t c = 0x6c7967656e657261ULL, d = 0x7465646279746573ULL;
  const auto round = [&] {
    a += b; b = std::rotl(b, 13) ^ a; a = std::rotl(a, 32);
    c += d; d = std::rotl(d, 16) ^ c;
    a += d; d = std::rotl(d, 21) ^ a;
    c += b; b = std::rotl(b, 17) ^ c; c = std::rotl(c, 32);
  };
  const auto consume = [&](std::uint64_t block) {
    d ^= block; round(); a ^= block;
  };
  const std::size_t full = bytes.size() / 8 * 8;
  for (std::size_t i = 0; i < full; i += 8) {
    std::uint64_t block = 0;
    for (std::size_t j = 0; j < 8; ++j)
      block |= static_cast<std::uint64_t>(bytes[i + j]) << (8 * j);
    consume(block);
  }
  std::uint64_t last = static_cast<std::uint64_t>(bytes.size() & 255) << 56;
  for (std::size_t i = full; i < bytes.size(); ++i)
    last |= static_cast<std::uint64_t>(bytes[i]) << (8 * (i - full));
  consume(last); c ^= 255;
  round(); round(); round();
  const auto value = a ^ b ^ c ^ d;
  return value == UINT64_MAX ? UINT64_MAX - 1 : value;
}

// Open addressing preserves the observed string-set iteration, including
// collisions and growth. Unsupported identity hashes keep the prior order.
ActivitySet alpha_string_set_order(
    const ActivitySet& values,
    const std::vector<std::optional<std::string>>& names) {
  struct Entry { std::size_t id; std::uint64_t hash; };
  std::vector<Entry> entries;
  entries.reserve(values.size());
  for (const auto id : values) {
    if (!names[id]) return values;
    const auto hash = alpha_string_hash(*names[id]);
    if (!hash) return values;
    entries.push_back({id, *hash});
  }
  const auto insert = [](Entry entry, auto& table) {
    const auto mask = table.size() - 1;
    std::size_t index = static_cast<std::size_t>(entry.hash & mask);
    auto perturb = entry.hash;
    for (;;) {
      const std::size_t probes = index + 9 < table.size() ? 9 : 0;
      for (std::size_t offset = 0; offset <= probes; ++offset) {
        auto& slot = table[index + offset];
        if (!slot) { slot = entry; return true; }
        if (slot->id == entry.id) return false;
      }
      perturb >>= 5;
      index = (index * 5 + 1 + static_cast<std::size_t>(perturb)) & mask;
    }
  };
  std::vector<std::optional<Entry>> table(8);
  std::size_t used = 0;
  for (const auto entry : entries) {
    if (!insert(entry, table)) continue;
    ++used;
    if (used * 5 >= (table.size() - 1) * 3) {
      std::size_t size = 8;
      while (size <= used * 4) size *= 2;
      auto old = std::move(table);
      table = std::vector<std::optional<Entry>>(size);
      for (const auto& slot : old)
        if (slot) insert(*slot, table);
    }
  }
  ActivitySet ordered;
  ordered.reserve(used);
  for (const auto& slot : table)
    if (slot) ordered.push_back(slot->id);
  return ordered;
}

// The log footprint: which activities directly follow which, plus the start and
// end activities. All higher-level relations derive from this matrix. With
// short_loops set, "a b a" patterns are also recorded so that a length-two loop
// — where both directions of succession appear — can be told apart from true
// concurrency when reading causality.
struct Footprint {
  std::vector<std::vector<bool>> follows;   // follows[a][b]: a directly precedes b
  std::vector<std::size_t> follow_source_order;
  std::vector<std::vector<std::size_t>> follow_target_order;
  std::vector<std::vector<std::size_t>> causal_target_order;
  std::vector<std::vector<bool>> two_loop;  // two_loop[a][b]: "a b a" occurs
  std::vector<std::vector<bool>> classic_causal;
  std::set<std::size_t> starts;
  std::set<std::size_t> ends;
  bool short_loops = false;

  [[nodiscard]] std::size_t size() const { return follows.size(); }
  [[nodiscard]] bool causal(std::size_t a, std::size_t b) const {
    if (!short_loops) return classic_causal[a][b];
    if (!follows[a][b]) return false;
    if (!follows[b][a]) return true;
    // Succession in both directions: concurrency for the classic miner, but a
    // length-two loop — hence causal both ways — when "a b a" and "b a b" both
    // occur, which interleaved concurrent activities cannot produce.
    return short_loops && two_loop[a][b] && two_loop[b][a];
  }
  [[nodiscard]] bool choice(std::size_t a, std::size_t b) const {
    if (short_loops) {
      const bool a_outgoing = std::any_of(follows[a].begin(), follows[a].end(),
                                         [](bool value) { return value; });
      const bool b_outgoing = std::any_of(follows[b].begin(), follows[b].end(),
                                         [](bool value) { return value; });
      if (a_outgoing != b_outgoing) return false;
    }
    return !follows[a][b] && !follows[b][a];
  }
};

void set_classic_relations(Footprint& fp,
                           const std::vector<std::string>& names,
                           const DfgFrequency& graph) {
  const auto abstraction = make_alpha_classic_abstraction(graph);
  const auto n = names.size();
  fp.classic_causal.assign(n, std::vector<bool>(n));
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = 0; b < n; ++b) {
      fp.classic_causal[a][b] =
          abstraction.causal.contains({names[a], names[b]});
    }
  }
}

// Builds the footprint over the interned log. Activities marked in `exclude`
// (the Alpha+ length-one-loop removal) are read as absent from every trace.
Footprint build_footprint(const IdLog& log, const std::vector<bool>* exclude,
                          bool short_loops) {
  const std::size_t n = log.names.size();
  Footprint fp;
  fp.short_loops = short_loops;
  fp.follows.assign(n, std::vector<bool>(n, false));
  fp.follow_target_order.resize(n);
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
      if (!fp.follows[sequence[i - 1]][sequence[i]]) {
        auto& targets = fp.follow_target_order[sequence[i - 1]];
        if (targets.empty())
          fp.follow_source_order.push_back(sequence[i - 1]);
        targets.push_back(sequence[i]);
      }
      fp.follows[sequence[i - 1]][sequence[i]] = true;
      if (short_loops && i >= 2 && sequence[i - 2] == sequence[i]) {
        fp.two_loop[sequence[i]][sequence[i - 1]] = true;
      }
    }
  }
  if (short_loops) {
    fp.causal_target_order.resize(n);
    for (const auto from : fp.follow_source_order) {
      const auto ordered = alpha_string_set_order(fp.follow_target_order[from],
                                                 log.names);
      ActivitySet accepted;
      for (const auto to : ordered)
        if (fp.causal(from, to)) accepted.push_back(to);
      fp.causal_target_order[from] = alpha_string_set_order(accepted, log.names);
    }
  } else {
    fp.classic_causal.assign(n, std::vector<bool>(n));
    for (std::size_t a = 0; a < n; ++a) {
      for (std::size_t b = 0; b < n; ++b)
        fp.classic_causal[a][b] = fp.follows[a][b] && !fp.follows[b][a];
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

const std::string& selected_activity(const Event& event,
                                     std::string_view selector) {
  const auto selected = selector.empty() ? std::string_view("concept:name")
                                         : selector;
  const bool canonical_selector = selected == "concept:name" ||
                                  selected == "activity";
  if (canonical_selector) {
    // The canonical projection remains authoritative even when an adapter did
    // not retain a source spelling.  A stale retained cell must not turn a
    // null or genuinely missing first-class value into an activity label.
    if (event.activity_state == FirstClassAttributeState::kNull ||
        (event.activity_state == FirstClassAttributeState::kMissing &&
         event.activity.empty())) {
      throw std::invalid_argument(
          "Alpha: event is missing activity attribute '" +
          std::string(selected) + "'");
    }
    return event.activity;
  }
  // A custom first-class projection is authoritative for its source key.
  // Do this before consulting the retained attribute copy: adapters may keep
  // that copy for round-tripping even after the projected cell changes.
  if (!event.activity_source_key.empty() &&
      event.activity_source_key == selected) {
    if (event.activity_state == FirstClassAttributeState::kNull ||
        (event.activity_state == FirstClassAttributeState::kMissing &&
         event.activity.empty())) {
      throw std::invalid_argument(
          "Alpha: event is missing activity attribute '" +
          std::string(selected) + "'");
    }
    return event.activity;
  }
  if (event.activity_attribute && event.activity_attribute->key == selected) {
    if (!event.activity_attribute->is_null) return event.activity_attribute->value;
    throw std::invalid_argument(
        "Alpha: event is missing activity attribute '" +
        std::string(selected) + "'");
  }
  if (const auto* value = dfg_event_field_value(event, selected)) return *value;
  throw std::invalid_argument(
      "Alpha: event is missing activity attribute '" +
      std::string(selected) + "'");
}

struct SelectedAlphaActivity {
  FirstClassAttributeState state;
  std::string label;
};

SelectedAlphaActivity nullable_selected_activity(const Event& event,
                                                 std::string_view selector) {
  if (selector == "concept:name" || selector == "activity" ||
      (!event.activity_source_key.empty() &&
       event.activity_source_key == selector)) {
    const auto state = event.activity_state == FirstClassAttributeState::kMissing &&
                               !event.activity.empty()
                           ? FirstClassAttributeState::kPresent
                           : event.activity_state;
    return {state, event.activity};
  }
  if (event.activity_attribute && event.activity_attribute->key == selector) {
    return {event.activity_attribute->is_null ? FirstClassAttributeState::kNull
                                              : FirstClassAttributeState::kPresent,
            event.activity_attribute->value};
  }
  if (const auto* value = dfg_event_field_value(event, selector))
    return {FirstClassAttributeState::kPresent, *value};
  for (const auto& attribute : event.attributes) {
    if (attribute.key == selector && attribute.is_null)
      return {FirstClassAttributeState::kNull, {}};
  }
  return {FirstClassAttributeState::kMissing, {}};
}

IdLog intern_alpha_plus_log(const EventLog& log, std::string_view selector) {
  IdLog out;
  std::unordered_map<std::optional<std::string>, std::size_t> ids;
  for (const auto& trace : log.traces) {
    std::vector<std::size_t> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto value = nullable_selected_activity(event, selector);
      if (value.state == FirstClassAttributeState::kMissing)
        throw std::invalid_argument(
            "Alpha: event is missing activity attribute '" +
            std::string(selector) + "'");
      const auto label = value.state == FirstClassAttributeState::kNull
                             ? std::nullopt
                             : std::optional<std::string>(value.label);
      const auto [it, inserted] = ids.try_emplace(label, out.names.size());
      if (inserted) out.names.push_back(label);
      sequence.push_back(it->second);
    }
    out.traces.push_back(std::move(sequence));
  }
  return out;
}

void check_alpha_row_limit(const EventLog& log, const AlphaMinerOptions& options) {
  if (!options.max_rows_to_materialize) return;
  std::size_t rows = 0;
  for (const auto& trace : log.traces) {
    if (trace.events.size() > *options.max_rows_to_materialize - rows)
      throw std::length_error("Alpha: event row materialization limit exceeded");
    rows += trace.events.size();
  }
}

std::vector<Place> maximal_places(const Footprint& fp);

PetriNet build_classic_net(const std::vector<std::string>& names,
                           const Footprint& fp, bool remove_unconnected,
                           const std::vector<std::optional<std::size_t>>*
                               transition_labels = nullptr) {
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
    if (remove_unconnected && inputs[activity].empty() &&
        outputs[activity].empty()) {
      continue;
    }
    net.add_transition(transition_labels ? transition_labels->at(activity)
                                        : std::optional<std::size_t>(activity),
                       std::move(inputs[activity]),
                       std::move(outputs[activity]));
  }
  return net;
}

Footprint footprint_from_graph(const DirectlyFollowsGraph& graph) {
  Footprint fp;
  const auto& names = graph.activities();
  const std::size_t n = names.size();
  fp.follows.assign(n, std::vector<bool>(n, false));
  DfgFrequency frequency;
  for (const auto& edge : graph.edges()) {
    if (edge.source >= n || edge.target >= n)
      throw std::invalid_argument("Alpha: graph edge activity id is out of range");
    fp.follows[edge.source][edge.target] = true;
    frequency[{names[edge.source], names[edge.target]}] = edge.count;
  }
  for (std::size_t activity = 0; activity < n; ++activity) {
    if (graph.start_activity_count(names[activity]) != 0)
      fp.starts.insert(activity);
    if (graph.end_activity_count(names[activity]) != 0)
      fp.ends.insert(activity);
  }
  set_classic_relations(fp, names, frequency);
  return fp;
}

// Alpha+ expands the original causal seeds against the candidates accumulated
// so far. A containment on either side permits growing both sides; it does not
// require every newly introduced cross edge to be causal. Newly grown pairs
// do not become outer seeds. Sources retain first-seen order; string targets
// use the separately constructed follows and causal set iteration.
std::vector<Place> maximal_alpha_plus_places(const Footprint& fp) {
  std::vector<Place> candidates;
  std::set<Place> seen;
  for (const auto from : fp.follow_source_order) {
    if (fp.follows[from][from]) continue;
    for (const auto to : fp.causal_target_order[from]) {
      if (fp.follows[to][to] || !fp.causal(from, to)) continue;
      Place seed{{from}, {to}};
      seen.insert(seed);
      candidates.push_back(std::move(seed));
    }
  }
  const auto seed_count = candidates.size();
  for (std::size_t i = 0; i < seed_count; ++i) {
    const auto seed = candidates[i];
    const auto end = candidates.size();
    for (std::size_t j = i + 1; j < end; ++j) {
      const auto& other = candidates[j];
      if (!is_subset(seed.first, other.first) &&
          !is_subset(seed.second, other.second)) continue;
      Place merged{unite(seed.first, other.first),
                   unite(seed.second, other.second)};
      if (admissible(merged.first, fp) && admissible(merged.second, fp) &&
          seen.insert(merged).second)
        candidates.push_back(std::move(merged));
    }
  }
  const std::vector<Place> all(seen.begin(), seen.end());
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

// Classic closure retains the complete cross-causality condition and grows
// either side with the other fixed until no new admissible pair appears.
std::vector<Place> maximal_places(const Footprint& fp) {
  if (fp.short_loops) return maximal_alpha_plus_places(fp);
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

// pm4py's alpha miner keyed by a typed column of an upload's frame names
// each transition str() of the cell (round BS): the frame-keyed log under
// concept:name, or nullopt for the log's own activities.
std::optional<EventLog> alpha_frame_keyed_log(const EventLog& log,
                                              const AlphaMinerOptions& options) {
  if (auto keyed = frame_keyed_activities(log, options.activity_attribute); keyed && keyed->non_text)
    return std::move(keyed->log);
  return std::nullopt;
}

AlphaMinerOptions concept_name_options(AlphaMinerOptions options) {
  options.activity_attribute = "concept:name";
  return options;
}

}  // namespace

EventLog normalize_alpha_event_log(const EventLog& event_log,
                                   const AlphaMinerOptions& options) {
  if (const auto keyed = alpha_frame_keyed_log(event_log, options))
    return normalize_alpha_event_log(*keyed, concept_name_options(options));
  check_alpha_row_limit(event_log, options);
  EventLog normalized = event_log;
  const std::string selector = options.activity_attribute.empty()
                                   ? "concept:name"
                                   : options.activity_attribute;
  for (auto& trace : normalized.traces) {
    for (auto& event : trace.events) {
      event.activity = selected_activity(event, selector);
      event.activity_state = FirstClassAttributeState::kPresent;
    }
  }
  return normalized;
}

PetriNet discover_alpha_classic(const EventLog& log,
                                const AlphaMinerOptions& options) {
  check_alpha_row_limit(log, options);
  const std::string selector = options.activity_attribute.empty()
                                   ? "concept:name"
                                   : options.activity_attribute;
  // Null is an activity value with its own dense identity; missing fields
  // remain absent positions, so they cannot create artificial adjacent edges.
  std::vector<std::optional<std::string>> labels;
  std::unordered_map<std::optional<std::string>, std::size_t> ids;
  std::vector<std::vector<std::optional<std::size_t>>> traces;
  for (const auto& trace : log.traces) {
    std::vector<std::optional<std::size_t>> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto value = nullable_selected_activity(event, selector);
      if (value.state == FirstClassAttributeState::kMissing) {
        sequence.push_back(std::nullopt);
        continue;
      }
      const auto label = value.state == FirstClassAttributeState::kNull
                             ? std::nullopt
                             : std::optional<std::string>(value.label);
      const auto [it, inserted] = ids.try_emplace(label, labels.size());
      if (inserted) labels.push_back(label);
      sequence.push_back(it->second);
    }
    traces.push_back(std::move(sequence));
  }
  const auto n = labels.size();
  Footprint footprint;
  footprint.follows.assign(n, std::vector<bool>(n));
  for (const auto& trace : traces) {
    if (trace.empty()) continue;
    if (trace.front()) footprint.starts.insert(*trace.front());
    if (trace.back()) footprint.ends.insert(*trace.back());
    for (std::size_t i = 1; i < trace.size(); ++i) {
      if (!trace[i - 1] || !trace[i])
        throw std::invalid_argument(
            "Alpha: event is missing activity attribute '" + selector + "'");
      footprint.follows[*trace[i - 1]][*trace[i]] = true;
    }
  }
  footprint.classic_causal.assign(n, std::vector<bool>(n));
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = 0; b < n; ++b)
      footprint.classic_causal[a][b] =
          footprint.follows[a][b] && !footprint.follows[b][a];
  }
  std::vector<std::string> activities;
  std::vector<std::optional<std::size_t>> transition_labels;
  for (const auto& label : labels) {
    if (label) {
      transition_labels.push_back(activities.size());
      activities.push_back(*label);
    } else {
      transition_labels.push_back(std::nullopt);
    }
  }
  return build_classic_net(activities, footprint, false, &transition_labels);
}

namespace {

// Picks one index into a list of tied incident transitions; the result is
// ignored when the list is empty.
using AlphaPlusCleanupChooser =
    std::function<std::size_t(const std::vector<std::size_t>&)>;

AlphaPlusDiscoveryResult alpha_plus_with_cleanup_choices(
    const EventLog& log, const AlphaMinerOptions& options,
    const AlphaPlusCleanupChooser& choose_index,
    std::vector<std::optional<std::string>>* chosen_names) {
  check_alpha_row_limit(log, options);
  const std::string selector = options.activity_attribute.empty()
                                   ? "concept:name"
                                   : options.activity_attribute;
  IdLog ids = intern_alpha_plus_log(log, selector);
  std::vector<std::string> visible_names;
  std::vector<std::optional<std::size_t>> visible_ids;
  for (const auto& label : ids.names) {
    if (label) {
      visible_ids.push_back(visible_names.size());
      visible_names.push_back(*label);
    } else {
      visible_ids.push_back(std::nullopt);
    }
  }
  if (ids.traces.empty())
    throw std::invalid_argument("Alpha+: a trace-bearing log is required");
  // Boundary labels participate in ordinary label identity and loop removal.
  const auto boundary_id = [&](const std::string& name) {
    const auto found = std::find(ids.names.begin(), ids.names.end(), name);
    if (found != ids.names.end())
      return static_cast<std::size_t>(found - ids.names.begin());
    ids.names.push_back(name);
    return ids.names.size() - 1;
  };
  const auto start_label = boundary_id("artificial_start");
  const auto end_label = boundary_id("artificial_end");
  IdLog boundary_log = ids;
  for (auto& sequence : boundary_log.traces) {
    sequence.insert(sequence.begin(), start_label);
    sequence.push_back(end_label);
  }
  const std::size_t n = ids.names.size();
  const Footprint full = build_footprint(boundary_log, nullptr, true);
  std::vector<bool> loops(n, false);
  for (std::size_t a = 0; a < n; ++a) loops[a] = full.follows[a][a];
  // Reserved boundary labels can themselves be loops; a trace whose every
  // label is a loop then has no reduced endpoints and fails publicly.
  for (const auto& sequence : boundary_log.traces)
    if (std::all_of(sequence.begin(), sequence.end(),
                    [&](std::size_t label) { return loops[label]; }))
      throw std::invalid_argument("Alpha+: boundary reduction leaves an empty trace");
  const auto reduced = build_footprint(boundary_log, &loops, true);
  if (reduced.starts.empty() || reduced.ends.empty())
    throw std::invalid_argument("Alpha+: boundary reduction leaves no endpoints");
  const auto& boundary_exclude = loops;
  const std::vector<Place> boundary_pairs = maximal_places(
      build_footprint(boundary_log, &boundary_exclude, true));

  // Retain the last immediate non-loop predecessor of each loop label while
  // accumulating all immediate non-loop successors, in original trace order.
  std::vector<ActivitySet> predecessors(n);
  std::vector<ActivitySet> successors(n);
  const auto insert_sorted = [](ActivitySet& values, std::size_t value) {
    const auto at = std::lower_bound(values.begin(), values.end(), value);
    if (at == values.end() || *at != value) values.insert(at, value);
  };
  for (const auto& trace : boundary_log.traces) {
    for (std::size_t i = 1; i < trace.size(); ++i) {
      const auto before = trace[i - 1];
      const auto after = trace[i];
      const bool before_is_loop = before < n && loops[before];
      const bool after_is_loop = after < n && loops[after];
      if (!before_is_loop && after_is_loop)
        predecessors[after] = {before};
      if (before_is_loop && !after_is_loop)
        insert_sorted(successors[before], after);
    }
  }

  // Every maximal pair containing both context differences contributes its
  // own fresh loop place. Empty differences participate in the subset tests.
  std::vector<std::vector<std::size_t>> loop_pairs(n);
  for (std::size_t t = 0; t < n; ++t) {
    if (!loops[t]) continue;
    if (predecessors[t].empty() || successors[t].empty()) continue;
    ActivitySet before_only;
    ActivitySet after_only;
    std::set_difference(predecessors[t].begin(), predecessors[t].end(),
                        successors[t].begin(), successors[t].end(),
                        std::back_inserter(before_only));
    std::set_difference(successors[t].begin(), successors[t].end(),
                        predecessors[t].begin(), predecessors[t].end(),
                        std::back_inserter(after_only));
    for (std::size_t pair_index = 0; pair_index < boundary_pairs.size();
         ++pair_index) {
      const auto& candidate = boundary_pairs[pair_index];
      if (!is_subset(before_only, candidate.first) ||
          !is_subset(after_only, candidate.second)) {
        continue;
      }
      loop_pairs[t].push_back(pair_index);
    }
  }

  std::vector<std::vector<std::size_t>> transition_inputs(n);
  std::vector<std::vector<std::size_t>> transition_outputs(n);
  for (std::size_t pair_index = 0; pair_index < boundary_pairs.size();
       ++pair_index) {
    for (const auto from : boundary_pairs[pair_index].first)
      transition_outputs[from].push_back(pair_index);
    for (const auto to : boundary_pairs[pair_index].second)
      transition_inputs[to].push_back(pair_index);
  }
  // Assemble the complete graph before cleanup: reserved names can occur
  // inside traces, and reduced traces can have several distinct endpoints.
  std::vector<AlphaPlaceName> names{{"start", {}, {}}, {"end", {}, {}}};
  const auto pair_name = [&](std::size_t index) {
    AlphaPlaceName name;
    for (const auto id : boundary_pairs[index].first)
      name.before.push_back(boundary_log.names[id]);
    for (const auto id : boundary_pairs[index].second)
      name.after.push_back(boundary_log.names[id]);
    std::sort(name.before.begin(), name.before.end());
    std::sort(name.after.begin(), name.after.end());
    return name;
  };
  for (std::size_t i = 0; i < boundary_pairs.size(); ++i)
    names.push_back(pair_name(i));
  auto inputs = transition_inputs;
  auto outputs = transition_outputs;
  for (std::size_t t = 0; t < n; ++t) {
    for (auto& p : inputs[t]) p += 2;
    for (auto& p : outputs[t]) p += 2;
    if (reduced.starts.contains(t)) inputs[t].push_back(0);
    if (reduced.ends.contains(t)) outputs[t].push_back(1);
    if (loops[t]) {
      for (const auto pair : loop_pairs[t]) {
        inputs[t].push_back(names.size());
        outputs[t].push_back(names.size());
        names.push_back(pair_name(pair));
      }
    }
  }
  std::vector<bool> live_places(names.size(), true), live_transitions(n, true);
  const auto erase_place = [&](std::size_t p) {
    live_places[p] = false;
    for (auto& values : inputs) std::erase(values, p);
    for (auto& values : outputs) std::erase(values, p);
  };
  const auto erase_transition = [&](std::size_t t) {
    live_transitions[t] = false;
    inputs[t].clear();
    outputs[t].clear();
  };
  const auto incident = [&](std::size_t p, const auto& arcs) {
    std::vector<std::size_t> result;
    for (std::size_t t = 0; t < n; ++t)
      if (live_transitions[t] && std::find(arcs[t].begin(), arcs[t].end(), p) != arcs[t].end())
        result.push_back(t);
    return result;
  };
  // The public implementation selects an identity-ordered incident arc;
  // allocation identity is not portable, so callers pick the tied candidate.
  const auto choose = [&](const std::vector<std::size_t>& candidates) {
    // The chooser also observes empty stages, which always fail.
    const auto index = choose_index(candidates);
    if (candidates.empty())
      throw std::invalid_argument("Alpha+: boundary cleanup has no incident transition");
    const auto chosen = candidates.at(index);
    if (chosen_names) chosen_names->push_back(ids.names[chosen]);
    return chosen;
  };
  std::size_t initial = 0;
  const auto first = choose(incident(0, inputs));
  if (outputs[first].size() == 1) {
    const auto next = outputs[first].front();
    if (incident(next, outputs).size() == 1) {
      initial = next;
      erase_transition(first);
      erase_place(0);
    }
  }
  const auto last = choose(incident(1, outputs));
  if (inputs[last].size() == 1) {
    const auto previous = inputs[last].front();
    const auto producers = incident(previous, outputs);
    const bool exclusive = std::all_of(producers.begin(), producers.end(),
        [&](std::size_t t) { return outputs[t].size() == 1; });
    if (exclusive && incident(previous, inputs).size() == 1) {
      erase_transition(last);
      erase_place(previous);
      for (const auto t : producers)
        if (live_transitions[t]) outputs[t].push_back(1);
    }
  }
  AlphaPlusDiscoveryResult result;
  auto& net = result.net;
  net.set_activities(visible_names);
  std::vector<std::size_t> place_ids(names.size());
  for (std::size_t p = 0; p < names.size(); ++p) {
    if (!live_places[p]) continue;
    place_ids[p] = net.add_place();
    result.place_names.push_back(names[p]);
  }
  PetriTransitionNameList transition_names;
  for (std::size_t t = 0; t < n; ++t) {
    if (!live_transitions[t]) continue;
    if (options.remove_unconnected && inputs[t].empty() && outputs[t].empty()) continue;
    for (auto& p : inputs[t]) p = place_ids[p];
    for (auto& p : outputs[t]) p = place_ids[p];
    const bool hidden = !loops[t] && (t == start_label || t == end_label);
    net.add_transition(hidden ? std::nullopt : visible_ids.at(t),
                       std::move(inputs[t]), std::move(outputs[t]));
    transition_names.push_back(ids.names[t]);
  }
  net.set_transition_names(std::move(transition_names));
  result.initial_marking.resize(net.place_count());
  result.final_marking.resize(net.place_count());
  if (live_places[initial]) {
    net.set_source(place_ids[initial]);
    result.initial_marking[place_ids[initial]] = 1;
  } else result.detached_initial_marking.push_back({names[initial], 1});
  if (live_places[1]) {
    net.set_sink(place_ids[1]);
    result.final_marking[place_ids[1]] = 1;
  } else result.detached_final_marking.push_back({names[1], 1});

  PetriDiscoveryMarkings markings;
  const auto marking_place = [&](std::size_t original) {
    if (live_places[original])
      return PetriMarkingPlace{PetriMarkingPlaceKind::kRoot, place_ids[original]};
    // Original removed IDs form a separate namespace. In particular, a live
    // loop place with an equal structural name does not receive this token.
    if (std::none_of(markings.detached_places.begin(), markings.detached_places.end(),
                     [&](const auto& place) { return place.id == original; })) {
      const auto members = [](const std::vector<std::optional<std::string>>& values) {
        HeuristicStructuredValue::Array array;
        for (const auto& value : values)
          array.push_back(value ? HeuristicStructuredValue{*value}
                                : HeuristicStructuredValue{nullptr});
        return HeuristicStructuredValue{std::move(array)};
      };
      markings.detached_places.push_back({original, HeuristicStructuredValue{
          HeuristicStructuredValue::Object{
              {"boundary", HeuristicStructuredValue{names[original].boundary}},
              {"before", members(names[original].before)},
              {"after", members(names[original].after)}}}});
    }
    return PetriMarkingPlace{PetriMarkingPlaceKind::kDetached, original};
  };
  markings.initial.push_back({marking_place(initial), 1});
  markings.final.push_back({marking_place(1), 1});
  net.set_discovery_markings(std::move(markings));

  return result;
}

}  // namespace

AlphaPlusDiscoveryResult discover_alpha_plus_result(const EventLog& log,
                             const AlphaMinerOptions& options) {
  if (const auto keyed = alpha_frame_keyed_log(log, options))
    return discover_alpha_plus_result(*keyed, concept_name_options(options));
  // Deterministic member of the public support: the first tied transition
  // in first-seen label order, with the boundary labels last.
  return alpha_plus_with_cleanup_choices(
      log, options, [](const auto&) { return std::size_t{0}; }, nullptr);
}

std::vector<AlphaPlusCleanupAlternative> enumerate_alpha_plus_cleanup_alternatives(
    const EventLog& log, const AlphaMinerOptions& options) {
  if (const auto keyed = alpha_frame_keyed_log(log, options))
    return enumerate_alpha_plus_cleanup_alternatives(*keyed, concept_name_options(options));
  // Odometer over every reachable sequence of tied choices. Each run replays
  // the fixed prefix and takes the first candidate at later stages.
  std::vector<AlphaPlusCleanupAlternative> alternatives;
  std::vector<std::size_t> path;
  while (true) {
    std::vector<std::size_t> sizes;
    const auto scripted = [&](const std::vector<std::size_t>& candidates) {
      const auto stage = sizes.size();
      sizes.push_back(candidates.size());
      return stage < path.size() ? path[stage] : std::size_t{0};
    };
    AlphaPlusCleanupAlternative alternative;
    try {
      alternative.result = alpha_plus_with_cleanup_choices(
          log, options, scripted, &alternative.selected_transitions);
    } catch (const std::invalid_argument& error) {
      // Failures before any tied choice are not cleanup alternatives.
      if (sizes.empty()) throw;
      alternative.error = error.what();
    }
    alternatives.push_back(std::move(alternative));
    path.resize(sizes.size(), 0);
    // A stage with no candidates records size zero and has no successor.
    std::size_t stage = path.size();
    while (stage > 0 && path[stage - 1] + 1 >= sizes[stage - 1]) --stage;
    if (stage == 0) break;
    path.resize(stage);
    ++path[stage - 1];
  }
  return alternatives;
}

PetriNet discover_alpha_plus(const EventLog& log,
                             const AlphaMinerOptions& options) {
  auto result = discover_alpha_plus_result(log, options);
  return std::move(result.net);
}

PetriNet discover_petri_net(const EventLog& log,
                            const AlphaMinerOptions& options) {
  if (const auto keyed = alpha_frame_keyed_log(log, options))
    return discover_petri_net(*keyed, concept_name_options(options));
  return options.short_loops ? discover_alpha_plus(log, options)
                             : discover_alpha_classic(log, options);
}

PetriNet discover_petri_net(const DirectlyFollowsGraph& graph,
                            const AlphaMinerOptions& options) {
  if (options.short_loops) {
    throw std::invalid_argument(
        "Alpha: Alpha+ requires event-log trace context; direct graph input supports classic only");
  }
  return build_classic_net(graph.activities(), footprint_from_graph(graph),
                           false);
}

PetriNet discover_petri_net(
    const DfgFrequency& graph,
    const std::map<std::string, std::uint64_t>& starts,
    const std::map<std::string, std::uint64_t>& ends,
    const AlphaMinerOptions& options) {
  if (options.short_loops)
    throw std::invalid_argument(
        "Alpha: Alpha+ requires event-log trace context; direct graph input supports classic only");
  std::set<std::string> labels;
  for (const auto& [edge, frequency] : graph) {
    (void)frequency;
    labels.insert(edge.first);
    labels.insert(edge.second);
  }
  for (const auto& [label, frequency] : starts) {
    (void)frequency;
    labels.insert(label);
  }
  for (const auto& [label, frequency] : ends) {
    (void)frequency;
    labels.insert(label);
  }
  const std::vector<std::string> names(labels.begin(), labels.end());
  Footprint footprint;
  footprint.follows.assign(names.size(), std::vector<bool>(names.size()));
  for (std::size_t a = 0; a < names.size(); ++a) {
    if (starts.contains(names[a])) footprint.starts.insert(a);
    if (ends.contains(names[a])) footprint.ends.insert(a);
    for (std::size_t b = 0; b < names.size(); ++b)
      footprint.follows[a][b] = graph.contains({names[a], names[b]});
  }
  set_classic_relations(footprint, names, graph);
  return build_classic_net(names, footprint, false);
}

PetriNet discover_petri_net(const DfgFrequency& graph,
                            const AlphaMinerOptions& options) {
  const auto endpoints = infer_alpha_endpoints(graph);
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  for (const auto& activity : endpoints.starts) starts.emplace(activity, 1);
  for (const auto& activity : endpoints.ends) ends.emplace(activity, 1);
  return discover_petri_net(graph, starts, ends, options);
}

PetriNet discover_petri_net(const DfgCleanAnalysis& graph,
                            const AlphaMinerOptions& options) {
  return discover_petri_net(graph.graph, graph.start_activities,
                            graph.end_activities, options);
}

}  // namespace minera
