#include "minera/powl_discovery.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "powl/semantics.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

using powl_detail::Sequence;
using powl_detail::State;
using powl_detail::UVCL;

void add_checked(std::uint64_t& dst, std::uint64_t value) {
  if (std::numeric_limits<std::uint64_t>::max() - dst < value)
    throw std::overflow_error("POWL counter overflow");
  dst += value;
}

void add_variant(UVCL& log, Sequence sequence, std::uint64_t count) {
  const auto [found, inserted] =
      log.index.try_emplace(sequence, log.entries.size());
  if (inserted) {
    log.entries.emplace_back(std::move(sequence), count);
    return;
  }
  add_checked(log.entries[found->second].second, count);
}

void validate_options(const PowlDiscoveryOptions& options);

std::string selected_activity(const Event& event,
                              const PowlDiscoveryOptions& options);

bool canonical_activity_key(std::string_view key) {
  return key.empty() || key == "concept:name" || key == "activity";
}

bool canonical_timestamp_key(std::string_view key) {
  return key.empty() || key == "time:timestamp" || key == "timestamp";
}

bool canonical_case_id_key(std::string_view key) {
  return key.empty() || key == "case:concept:name";
}

bool equivalent_first_class_key(std::string_view source,
                                std::string_view selector) {
  if (source == selector) return true;
  const auto same_pair = [&](std::string_view short_name,
                             std::string_view qualified_name) {
    return (source == short_name && selector == qualified_name) ||
           (source == qualified_name && selector == short_name);
  };
  return same_pair("activity", "concept:name") ||
         same_pair("resource", "org:resource") ||
         same_pair("timestamp", "time:timestamp") ||
         same_pair("lifecycle", "lifecycle:transition");
}

void set_projected_attribute(Event& event, std::string_view key,
                             const std::string& value) {
  for (auto& attribute : event.attributes) {
    if (attribute.key != key) continue;
    attribute.value = value;
    attribute.is_null = false;
    return;
  }
  EventAttribute attribute;
  attribute.key = std::string(key);
  attribute.value = value;
  event.attributes.push_back(std::move(attribute));
}

std::string selected_timestamp(const Event& event,
                               const PowlDiscoveryOptions& options) {
  if (canonical_timestamp_key(options.timestamp_key)) return event.timestamp;
  if (event.timestamp_source_key == options.timestamp_key) {
    if (event.timestamp_state == FirstClassAttributeState::kNull ||
        (event.timestamp_state == FirstClassAttributeState::kMissing &&
         event.timestamp.empty())) {
      throw std::invalid_argument(
          "powl_timestamp_key: event is missing a non-null attribute '" +
          options.timestamp_key + "'");
    }
    return event.timestamp;
  }
  if (const auto* value = event.attribute(options.timestamp_key))
    return *value;
  throw std::invalid_argument(
      "powl_timestamp_key: event is missing a non-null attribute '" +
      options.timestamp_key + "'");
}

std::string selected_case_id(const Event& event, const Trace& trace,
                             const PowlDiscoveryOptions& options) {
  if (options.case_id_key == "case_id") {
    if (event.case_id.empty())
      throw std::invalid_argument(
          "powl_case_id_key: event is missing a non-null attribute '" +
          options.case_id_key + "'");
    return event.case_id;
  }
  if (event.case_id_source_key == options.case_id_key) {
    if (event.case_id_state == FirstClassAttributeState::kNull ||
        (event.case_id_state == FirstClassAttributeState::kMissing &&
         event.case_id.empty())) {
      throw std::invalid_argument(
          "powl_case_id_key: event is missing a non-null attribute '" +
          options.case_id_key + "'");
    }
    if (event.case_id.empty())
      throw std::invalid_argument(
          "powl_case_id_key: selected case attribute '" +
          options.case_id_key + "' is empty");
    return event.case_id;
  }
  if (const auto* value = event.attribute(options.case_id_key)) {
    if (value->empty())
      throw std::invalid_argument(
          "powl_case_id_key: selected case attribute '" +
          options.case_id_key + "' is empty");
    return *value;
  }
  if (trace.case_id_source_key == options.case_id_key) {
    if (trace.case_id_state == FirstClassAttributeState::kNull ||
        (trace.case_id_state == FirstClassAttributeState::kMissing &&
         trace.case_id.empty())) {
      throw std::invalid_argument(
          "powl_case_id_key: trace is missing a non-null attribute '" +
          options.case_id_key + "'");
    }
    if (trace.case_id.empty())
      throw std::invalid_argument(
          "powl_case_id_key: selected case attribute '" +
          options.case_id_key + "' is empty");
    return trace.case_id;
  }
  if (const auto* value = trace.attribute(options.case_id_key)) {
    if (value->empty())
      throw std::invalid_argument(
          "powl_case_id_key: selected case attribute '" +
          options.case_id_key + "' is empty");
    return *value;
  }
  throw std::invalid_argument(
      "powl_case_id_key: event is missing a non-null attribute '" +
      options.case_id_key + "'");
}

struct PowlEventWithTime {
  Event event;
  std::optional<double> timestamp;
};

void project_powl_event(Event& event, const PowlDiscoveryOptions& options,
                        std::optional<double>* timestamp) {
  if (!canonical_activity_key(options.activity_key)) {
    const auto activity = selected_activity(event, options);
    event.activity = activity;
    set_projected_attribute(event, options.activity_key, activity);
  }
  if (!canonical_timestamp_key(options.timestamp_key)) {
    const auto value = selected_timestamp(event, options);
    const auto parsed = parse_timestamp(value);
    if (!parsed || !std::isfinite(*parsed))
      throw std::invalid_argument(
          "powl_timestamp_key: event has an invalid timestamp attribute '" +
          options.timestamp_key + "'");
    event.timestamp = value;
    event.timestamp_state = FirstClassAttributeState::kPresent;
    event.timestamp_source_key = options.timestamp_key;
    set_projected_attribute(event, options.timestamp_key, value);
    if (timestamp != nullptr) *timestamp = *parsed;
  }
}

EventLog normalize_powl_event_log_impl(const EventLog& event_log,
                                       const PowlDiscoveryOptions& options) {
  const bool custom_activity = !canonical_activity_key(options.activity_key);
  const bool custom_timestamp = !canonical_timestamp_key(options.timestamp_key);
  const bool custom_case_id = !canonical_case_id_key(options.case_id_key);
  if (!custom_activity && !custom_timestamp && !custom_case_id)
    return event_log;

  EventLog normalized;
  copy_event_log_context(event_log, normalized);

  if (!custom_case_id) {
    normalized.traces = event_log.traces;
    for (auto& trace : normalized.traces) {
      std::vector<PowlEventWithTime> projected;
      projected.reserve(trace.events.size());
      for (auto& event : trace.events) {
        PowlEventWithTime item;
        item.event = std::move(event);
        project_powl_event(item.event, options, &item.timestamp);
        projected.push_back(std::move(item));
      }
      if (custom_timestamp) {
        std::stable_sort(
            projected.begin(), projected.end(),
            [](const PowlEventWithTime& left, const PowlEventWithTime& right) {
              return *left.timestamp < *right.timestamp;
            });
      }
      trace.events.clear();
      trace.events.reserve(projected.size());
      for (auto& item : projected) trace.events.push_back(std::move(item.event));
    }
    return normalized;
  }

  struct Group {
    std::string case_id;
    Trace trace;
    std::vector<PowlEventWithTime> events;
    std::size_t first_seen = 0;
  };
  std::vector<Group> groups;
  std::unordered_map<std::string, std::size_t> group_index;
  std::size_t ordinal = 0;
  const auto get_group = [&](const std::string& case_id,
                             const Trace& source_trace) -> Group& {
    const auto found = group_index.find(case_id);
    if (found != group_index.end()) return groups[found->second];
    Group group;
    group.case_id = case_id;
    group.trace = source_trace;
    group.trace.case_id = case_id;
    group.trace.case_id_state = FirstClassAttributeState::kPresent;
    group.trace.case_id_source_key = options.case_id_key;
    group.trace.events.clear();
    group.first_seen = ordinal;
    groups.push_back(std::move(group));
    group_index.emplace(case_id, groups.size() - 1);
    return groups.back();
  };

  for (const auto& source_trace : event_log.traces) {
    if (source_trace.events.empty()) {
      const auto case_id = selected_case_id(
          Event{}, source_trace, options);
      (void)get_group(case_id, source_trace);
      ++ordinal;
      continue;
    }
    for (const auto& source_event : source_trace.events) {
      const auto case_id = selected_case_id(source_event, source_trace, options);
      auto& group = get_group(case_id, source_trace);
      PowlEventWithTime item;
      item.event = source_event;
      ++ordinal;
      project_powl_event(item.event, options, &item.timestamp);
      item.event.case_id = case_id;
      item.event.case_id_state = FirstClassAttributeState::kPresent;
      item.event.case_id_source_key = options.case_id_key;
      group.events.push_back(std::move(item));
    }
  }

  for (auto& group : groups) {
    if (!custom_timestamp) continue;
    std::stable_sort(
        group.events.begin(), group.events.end(),
        [](const PowlEventWithTime& left, const PowlEventWithTime& right) {
          return *left.timestamp < *right.timestamp;
        });
  }
  std::stable_sort(
      groups.begin(), groups.end(), [](const Group& left, const Group& right) {
        if (left.case_id != right.case_id) return left.case_id < right.case_id;
        return left.first_seen < right.first_seen;
      });

  normalized.traces.reserve(groups.size());
  for (auto& group : groups) {
    group.trace.events.clear();
    group.trace.events.reserve(group.events.size());
    for (auto& item : group.events) group.trace.events.push_back(std::move(item.event));
    normalized.traces.push_back(std::move(group.trace));
  }
  return normalized;
}

std::string selected_activity(const Event& event,
                              const PowlDiscoveryOptions& options) {
  if (options.activity_key == "concept:name" ||
      options.activity_key == "activity") {
    return event.activity;
  }
  bool first_class_match = false;
  const auto first_class_value = [&]() -> const std::string* {
    const auto select = [&](std::string_view source_key,
                            FirstClassAttributeState state,
                            const std::string& value) -> const std::string* {
      if (!equivalent_first_class_key(source_key, options.activity_key))
        return nullptr;
      first_class_match = true;
      return state == FirstClassAttributeState::kNull ||
                     (state == FirstClassAttributeState::kMissing &&
                      value.empty())
                 ? nullptr
                 : &value;
    };
    if (const auto* value = select(event.activity_source_key,
                                   event.activity_state, event.activity))
      return value;
    if (const auto* value = select(event.timestamp_source_key,
                                   event.timestamp_state, event.timestamp))
      return value;
    if (const auto* value = select(event.resource_source_key,
                                   event.resource_state, event.resource))
      return value;
    if (const auto* value = select(event.lifecycle_source_key,
                                   event.lifecycle_state, event.lifecycle))
      return value;
    if (const auto* value = select(event.case_id_source_key,
                                   event.case_id_state, event.case_id))
      return value;
    return nullptr;
  }();
  if (first_class_match) {
    if (first_class_value != nullptr) return *first_class_value;
    throw std::invalid_argument(
        "powl_activity_key: event is missing a non-null attribute '" +
        options.activity_key + "'");
  }
  if (const auto* value = event.attribute(options.activity_key))
    return *value;
  throw std::invalid_argument(
      "powl_activity_key: event is missing a non-null attribute '" +
      options.activity_key + "'");
}

UVCL make_uvcl(const EventLog& log, const PowlDiscoveryOptions& options) {
  UVCL out;
  for (const auto& trace : log.traces) {
    Sequence seq;
    seq.reserve(trace.events.size());
    for (const auto& event : trace.events)
      seq.push_back(selected_activity(event, options));
    add_variant(out, std::move(seq), 1);
  }
  return out;
}

std::vector<std::string> activities(const UVCL& log) {
  std::set<std::string> values;
  for (const auto& [seq, count] : log) {
    (void)count;
    values.insert(seq.begin(), seq.end());
  }
  return {values.begin(), values.end()};
}

UVCL project(const UVCL& log, const std::set<std::string>& keep) {
  UVCL out;
  for (const auto& [seq, count] : log) {
    Sequence p;
    for (const auto& a : seq) if (keep.count(a)) p.push_back(a);
    add_variant(out, std::move(p), count);
  }
  return out;
}

UVCL without_empty(const UVCL& log) {
  UVCL out;
  for (const auto& [seq, count] : log)
    if (!seq.empty()) add_variant(out, seq, count);
  return out;
}

struct Relations {
  std::vector<std::string> acts;
  std::map<std::pair<std::string, std::string>, std::uint64_t> dfg;
  std::map<std::pair<std::string, std::string>, std::uint64_t> efg;
  // Per-trace EFG support: each direction contributes at most once per trace
  // (the UVCL multiplicity is the number of traces with that exact sequence).
  std::map<std::pair<std::string, std::string>, std::uint64_t> trace_efg;
  std::uint64_t trace_count = 0;
  std::map<std::string, std::uint64_t> starts, ends;
  std::map<std::string, std::set<std::string>> predecessors, successors;
};

Relations compute_relations(const UVCL& log) {
  Relations r;
  r.acts = activities(log);
  for (const auto& a : r.acts) { r.predecessors[a]; r.successors[a]; }
  for (const auto& [seq, count] : log) {
    add_checked(r.trace_count, count);
    if (seq.empty()) continue;
    add_checked(r.starts[seq.front()], count);
    add_checked(r.ends[seq.back()], count);
    for (std::size_t i = 1; i < seq.size(); ++i) {
      add_checked(r.dfg[{seq[i - 1], seq[i]}], count);
    }
    std::set<std::pair<std::string, std::string>> seen_in_trace;
    for (std::size_t i = 0; i < seq.size(); ++i) {
      for (std::size_t j = i + 1; j < seq.size(); ++j) {
        add_checked(r.efg[{seq[i], seq[j]}], count);
        if (seen_in_trace.insert({seq[i], seq[j]}).second)
          add_checked(r.trace_efg[{seq[i], seq[j]}], count);
      }
    }
  }
  for (const auto& [p, count] : r.efg) {
    (void)count;
    r.successors[p.first].insert(p.second);
    r.predecessors[p.second].insert(p.first);
  }
  return r;
}

bool has(const std::map<std::pair<std::string, std::string>, std::uint64_t>& m,
         const std::string& a, const std::string& b) {
  return m.find({a, b}) != m.end();
}

PowlNode flower(const std::vector<std::string>& acts) {
  if (acts.empty()) return PowlNode::silent();
  std::vector<PowlNode> leaves;
  for (const auto& a : acts) leaves.push_back(PowlNode::transition(a));
  PowlNode redo_node = leaves.size() == 1 ? std::move(leaves.front())
                                         : PowlNode::xor_node(std::move(leaves));
  // The flower always executes one visible branch before allowing a redo.
  // This keeps the empty trace out of the fallback language.
  return PowlNode::loop(std::move(redo_node), PowlNode::silent());
}

// Discovery may produce nested partial orders while projecting a cut.  The
// model's final normalization flattens a nested order when its boundary is
// unambiguous, while retaining an order with multiple entry/exit choices as
// one node.  This mirrors the structural simplification used by the model
// object without changing its language.
PowlNode simplify_powl(PowlNode node);

PowlNode simplify_powl(PowlNode node) {
  if (node.kind() == PowlNodeKind::kTransition)
    return PowlNode::transition(node.label(), node.frequency());
  if (node.kind() == PowlNodeKind::kSilent) return PowlNode::silent();

  if (node.kind() == PowlNodeKind::kLoop) {
    return PowlNode::loop(simplify_powl(node.children()[0]),
                          simplify_powl(node.children()[1]));
  }

  if (node.kind() == PowlNodeKind::kXor) {
    std::vector<PowlNode> children;
    children.reserve(node.children().size());
    for (const auto& child : node.children()) {
      auto simplified = simplify_powl(child);
      if (simplified.kind() == PowlNodeKind::kXor) {
        for (auto& nested : simplified.children())
          children.push_back(simplify_powl(nested));
      } else {
        children.push_back(std::move(simplified));
      }
    }
    if (children.size() == 2) {
      const auto merge_optional_loop = [&](const PowlNode& optional,
                                           const PowlNode& loop) -> std::optional<PowlNode> {
        if (optional.kind() != PowlNodeKind::kSilent ||
            loop.kind() != PowlNodeKind::kLoop)
          return std::nullopt;
        if (loop.children()[0].kind() == PowlNodeKind::kSilent)
          return PowlNode::loop(loop.children()[0], loop.children()[1]);
        if (loop.children()[1].kind() == PowlNodeKind::kSilent)
          return PowlNode::loop(loop.children()[1], loop.children()[0]);
        return std::nullopt;
      };
      if (auto merged = merge_optional_loop(children[0], children[1]))
        return std::move(*merged);
      if (auto merged = merge_optional_loop(children[1], children[0]))
        return std::move(*merged);
    }
    return PowlNode::xor_node(std::move(children));
  }

  // Strict partial orders are normalized after all children have been
  // simplified.  A connected nested order can be spliced into its parent
  // only when it has one start and one end; disconnected orders can always be
  // flattened because no parent edge needs a boundary representative.
  std::vector<PowlNode> simplified_children;
  simplified_children.reserve(node.children().size());
  for (const auto& child : node.children())
    simplified_children.push_back(simplify_powl(child));

  struct Placement {
    std::size_t start = 0;
    std::size_t end = 0;
    bool flattened = false;
  };
  std::vector<std::pair<std::size_t, std::size_t>> order;
  std::vector<bool> flatten(node.children().size(), false);
  for (std::size_t i = 0; i < node.children().size(); ++i) {
    bool connected = false;
    for (std::size_t j = 0; j < node.children().size(); ++j)
      if (i != j && (node.has_edge(i, j) || node.has_edge(j, i))) {
        connected = true;
        break;
      }
    const auto& child = simplified_children[i];
    flatten[i] =
        child.kind() == PowlNodeKind::kStrictPartialOrder &&
        (!connected || (child.start_nodes().size() == 1 &&
                        child.end_nodes().size() == 1));
  }

  // The reference normalization emits retained children first and appends
  // the children of flattened nested orders afterward.  Keep that ordering so
  // serialized node indices remain deterministic across equivalent models.
  std::vector<PowlNode> output_children;
  output_children.reserve(simplified_children.size());
  std::vector<Placement> output_placements(node.children().size());
  for (std::size_t i = 0; i < node.children().size(); ++i) {
    const auto& child = simplified_children[i];
    if (!flatten[i]) {
      const auto index = output_children.size();
      output_children.push_back(child);
      output_placements[i] = {index, index, false};
    }
  }
  for (std::size_t i = 0; i < node.children().size(); ++i) {
    if (!flatten[i]) continue;
    const auto& child = simplified_children[i];
    const auto offset = output_children.size();
    for (const auto& nested : child.children()) output_children.push_back(nested);
    for (const auto [a, b] : child.order()) order.emplace_back(offset + a, offset + b);
    const auto starts = child.start_nodes();
    const auto ends = child.end_nodes();
    output_placements[i] = {
        starts.size() == 1 ? offset + starts.front() : offset,
        ends.size() == 1 ? offset + ends.front() : offset,
        true};
  }
  for (const auto [a, b] : node.order()) {
    const auto source = output_placements[a].flattened ? output_placements[a].end
                                                        : output_placements[a].start;
    const auto target = output_placements[b].flattened ? output_placements[b].start
                                                        : output_placements[b].start;
    if (source != target) order.emplace_back(source, target);
  }
  return PowlNode::strict_partial_order(std::move(output_children), order);
}

PowlNode discover_rec(const UVCL&, State&);

PowlNode recurse(const UVCL& log, State& s) {
  return discover_rec(log, s);
}

std::optional<PowlNode> make_xor(const UVCL& log, State& s) {
  const auto r = compute_relations(log);
  if (r.acts.size() < 2) return std::nullopt;
  std::vector<std::vector<std::string>> comps;
  std::set<std::string> seen;
  for (const auto& root : r.acts) {
    if (seen.count(root)) continue;
    std::vector<std::string> comp{root}; seen.insert(root);
    for (std::size_t p = 0; p < comp.size(); ++p) {
      for (const auto& a : r.acts) {
        if (!seen.count(a) && (has(r.dfg, comp[p], a) || has(r.dfg, a, comp[p]))) {
          seen.insert(a); comp.push_back(a);
        }
      }
    }
    comps.push_back(std::move(comp));
  }
  if (comps.size() < 2) return std::nullopt;
  // Exclusive-choice projection assigns each trace to the component with the
  // largest activity overlap.  A trace belongs to one branch only; projecting
  // it independently onto every component would manufacture optional silent
  // branches and change the discovered language.
  std::vector<UVCL> projected(comps.size());
  for (const auto& [sequence, count] : log) {
    std::size_t selected = 0;
    std::size_t selected_count = 0;
    for (std::size_t index = 0; index < comps.size(); ++index) {
      std::size_t overlap = 0;
      for (const auto& activity : sequence)
        if (std::find(comps[index].begin(), comps[index].end(), activity) !=
            comps[index].end())
          ++overlap;
      // The source implementation resolves equal overlaps in favor of the
      // later component after sorting by (overlap, index) descending.
      if (overlap >= selected_count) {
        selected = index;
        selected_count = overlap;
      }
    }
    Sequence branch;
    for (const auto& activity : sequence)
      if (std::find(comps[selected].begin(), comps[selected].end(), activity) !=
          comps[selected].end())
        branch.push_back(activity);
    add_variant(projected[selected], std::move(branch), count);
  }
  std::vector<PowlNode> children;
  if (s.probe_only) return PowlNode::silent();
  children.reserve(projected.size());
  for (auto& child_log : projected)
    children.push_back(recurse(child_log, s));
  return PowlNode::xor_node(std::move(children));
}

std::optional<PowlNode> make_sequence(const UVCL& log, State& s) {
  const auto r = compute_relations(log);
  const auto n = r.acts.size();
  if (n < 2) return std::nullopt;

  // Build the transitive predecessor/successor relation of the directly
  // follows graph. Sequence groups are merged while a pair of groups has
  // either evidence in both directions or no evidence in either direction.
  // This preserves optional/skippable stages instead of forcing them into a
  // topological level solely because their direct graph is acyclic.
  std::map<std::string, std::set<std::string>> successors;
  std::map<std::string, std::set<std::string>> predecessors;
  for (const auto& activity : r.acts) {
    successors[activity];
    predecessors[activity];
  }
  for (const auto& [edge, count] : r.dfg) {
    (void)count;
    successors[edge.first].insert(edge.second);
    predecessors[edge.second].insert(edge.first);
  }
  for (const auto& source : r.acts) {
    std::vector<std::string> pending(successors[source].begin(),
                                     successors[source].end());
    std::set<std::string> visited;
    while (!pending.empty()) {
      const auto current = pending.back();
      pending.pop_back();
      if (!visited.insert(current).second) continue;
      for (const auto& next : successors[current]) pending.push_back(next);
    }
    visited.erase(source);
    successors[source] = std::move(visited);
  }
  for (const auto& target : r.acts) {
    std::vector<std::string> pending(predecessors[target].begin(),
                                     predecessors[target].end());
    std::set<std::string> visited;
    while (!pending.empty()) {
      const auto current = pending.back();
      pending.pop_back();
      if (!visited.insert(current).second) continue;
      for (const auto& previous : predecessors[current])
        pending.push_back(previous);
    }
    visited.erase(target);
    predecessors[target] = std::move(visited);
  }
  const auto reaches = [&](const std::string& source,
                           const std::string& target) {
    return successors.at(source).count(target) != 0;
  };

  std::vector<std::set<std::string>> groups;
  for (const auto& activity : r.acts) groups.push_back({activity});
  while (true) {
    bool merged = false;
    for (std::size_t left = 0; left < groups.size() && !merged; ++left) {
      for (std::size_t right = left + 1; right < groups.size(); ++right) {
        bool merge = false;
        for (const auto& a : groups[left]) {
          for (const auto& b : groups[right]) {
            const bool forward = reaches(a, b);
            const bool reverse = reaches(b, a);
            if ((forward && reverse) || (!forward && !reverse)) {
              merge = true;
              break;
            }
          }
          if (merge) break;
        }
        if (!merge) continue;
        groups[left].insert(groups[right].begin(), groups[right].end());
        groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(right));
        merged = true;
        break;
      }
    }
    if (!merged) break;
  }
  if (groups.size() < 2) return std::nullopt;
  std::sort(groups.begin(), groups.end(), [&](const auto& left, const auto& right) {
    const auto left_first = *left.begin();
    const auto right_first = *right.begin();
    const auto left_score = predecessors[left_first].size() +
                            (n - successors[left_first].size());
    const auto right_score = predecessors[right_first].size() +
                             (n - successors[right_first].size());
    if (left_score != right_score) return left_score < right_score;
    return left_first < right_first;
  });

  // Strict sequence repair merges groups that can be skipped. The direct DFG
  // is used for the repair while the transitive relation above determines the
  // initial sequence ordering.
  std::vector<std::ptrdiff_t> min_from(groups.size());
  std::vector<std::ptrdiff_t> max_to(groups.size());
  const auto group_has_start = [&](const std::set<std::string>& group) {
    for (const auto& activity : group)
      if (r.starts.count(activity) != 0) return true;
    return false;
  };
  const auto group_has_end = [&](const std::set<std::string>& group) {
    for (const auto& activity : group)
      if (r.ends.count(activity) != 0) return true;
    return false;
  };
  for (std::size_t index = 0; index < groups.size(); ++index) {
    min_from[index] = group_has_start(groups[index])
                          ? -1
                          : static_cast<std::ptrdiff_t>(groups.size());
    max_to[index] = group_has_end(groups[index])
                        ? static_cast<std::ptrdiff_t>(groups.size())
                        : -1;
  }
  std::map<std::string, std::size_t> group_of;
  for (std::size_t index = 0; index < groups.size(); ++index)
    for (const auto& activity : groups[index]) group_of[activity] = index;
  for (const auto& [edge, count] : r.dfg) {
    (void)count;
    min_from[group_of.at(edge.second)] = std::min(
        min_from[group_of.at(edge.second)],
        static_cast<std::ptrdiff_t>(group_of.at(edge.first)));
    max_to[group_of.at(edge.first)] = std::max(
        max_to[group_of.at(edge.first)],
        static_cast<std::ptrdiff_t>(group_of.at(edge.second)));
  }
  const auto skippable = [&](std::size_t pivot) {
    for (std::size_t before = 0; before < pivot; ++before)
      for (std::size_t after = pivot + 1; after < groups.size(); ++after)
        for (const auto& a : groups[before])
          for (const auto& b : groups[after])
            if (has(r.dfg, a, b)) return true;
    for (std::size_t after = pivot + 1; after < groups.size(); ++after)
      if (group_has_start(groups[after])) return true;
    for (std::size_t before = 0; before < pivot; ++before)
      if (group_has_end(groups[before])) return true;
    return false;
  };
  for (std::size_t pivot = 0; pivot < groups.size(); ++pivot) {
    if (!skippable(pivot)) continue;
    for (std::ptrdiff_t previous = static_cast<std::ptrdiff_t>(pivot) - 1;
         previous >= 0 && max_to[static_cast<std::size_t>(previous)] <=
                                static_cast<std::ptrdiff_t>(pivot);
         --previous) {
      groups[pivot].insert(groups[static_cast<std::size_t>(previous)].begin(),
                           groups[static_cast<std::size_t>(previous)].end());
      groups[static_cast<std::size_t>(previous)].clear();
    }
    for (std::size_t next = pivot + 1;
         next < groups.size() && min_from[next] >=
                                     static_cast<std::ptrdiff_t>(pivot);
         ++next) {
      groups[pivot].insert(groups[next].begin(), groups[next].end());
      groups[next].clear();
    }
  }
  groups.erase(std::remove_if(groups.begin(), groups.end(),
                              [](const auto& group) { return group.empty(); }),
               groups.end());
  if (groups.size() < 2) return std::nullopt;

  // Split each trace at the same least-cost boundaries as the strict sequence
  // projection: entering the current group reduces cost, while an event from
  // a not-yet-seen group increases it. Empty projected traces are retained.
  std::vector<UVCL> projected_logs(groups.size());
  for (const auto& [sequence, count] : log) {
    std::size_t split_point = 0;
    std::set<std::string> ignored;
    for (std::size_t index = 0; index < groups.size(); ++index) {
      std::ptrdiff_t cost = 0;
      std::ptrdiff_t least_cost = 0;
      std::size_t new_split_point = split_point;
      for (std::size_t position = split_point; position < sequence.size();
           ++position) {
        if (groups[index].count(sequence[position]) != 0) {
          --cost;
        } else if (ignored.count(sequence[position]) == 0) {
          ++cost;
        }
        if (cost < least_cost) {
          least_cost = cost;
          new_split_point = position + 1;
        }
      }
      Sequence child;
      for (std::size_t position = split_point; position < new_split_point;
           ++position)
        if (groups[index].count(sequence[position]) != 0)
          child.push_back(sequence[position]);
      add_variant(projected_logs[index], std::move(child), count);
      split_point = new_split_point;
      ignored.insert(groups[index].begin(), groups[index].end());
    }
  }
  std::vector<PowlNode> children;
  if (s.probe_only) return PowlNode::silent();
  children.reserve(projected_logs.size());
  for (auto& child_log : projected_logs)
    children.push_back(recurse(child_log, s));
  return PowlNode::sequence(std::move(children));
}

std::optional<PowlNode> make_concurrency(const UVCL& log, State& s) {
  const auto r = compute_relations(log);
  const auto n = r.acts.size();
  if (n < 2) return std::nullopt;

  // Start with one group per activity. Groups are merged whenever any pair
  // lacks the two-way DFG evidence required for concurrency. Repeating the
  // pass computes the connected components of that incompatibility relation,
  // while preserving the reference cut's pairwise merge semantics.
  std::vector<std::set<std::string>> groups;
  groups.reserve(n);
  for (const auto& activity : r.acts) groups.push_back({activity});
  bool merged = true;
  while (merged) {
    merged = false;
    for (std::size_t left = 0; left < groups.size() && !merged; ++left) {
      for (std::size_t right = left + 1; right < groups.size(); ++right) {
        bool incompatible = false;
        for (const auto& a : groups[left]) {
          for (const auto& b : groups[right]) {
            const bool parallel = has(r.dfg, a, b) && has(r.dfg, b, a);
            if (!parallel) {
              incompatible = true;
              break;
            }
          }
          if (incompatible) break;
        }
        if (!incompatible) continue;
        groups[left].insert(groups[right].begin(), groups[right].end());
        groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(right));
        merged = true;
        break;
      }
    }
  }
  std::sort(groups.begin(), groups.end(), [](const auto& left, const auto& right) {
    if (left.size() != right.size()) return left.size() < right.size();
    return *left.begin() < *right.begin();
  });
  if (groups.size() < 2) return std::nullopt;

  // A branch that cannot both start and finish is a skippable fragment. The
  // strict cut absorbs it into its adjacent branch, retaining a valid
  // concurrent partition instead of rejecting the cut outright.
  std::size_t index = 0;
  while (index < groups.size() && groups.size() > 1) {
    bool contains_start = false;
    bool contains_end = false;
    for (const auto& activity : groups[index]) {
      contains_start = contains_start || r.starts.count(activity) != 0;
      contains_end = contains_end || r.ends.count(activity) != 0;
    }
    if (contains_start && contains_end) {
      ++index;
      continue;
    }
    auto fragment = std::move(groups[index]);
    groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(index));
    if (index == 0)
      groups.front().insert(fragment.begin(), fragment.end());
    else
      groups[index - 1].insert(fragment.begin(), fragment.end());
  }
  if (groups.size() < 2) return std::nullopt;

  std::vector<PowlNode> children;
  if (s.probe_only) return PowlNode::silent();
  children.reserve(groups.size());
  for (const auto& group : groups) children.push_back(recurse(project(log, group), s));
  return PowlNode::concurrency(std::move(children));
}

std::optional<PowlNode> make_loop(const UVCL& log, State& s) {
  if (log.empty()) return std::nullopt;
  const auto relations = compute_relations(log);
  if (relations.dfg.empty()) return std::nullopt;

  std::set<std::string> starts;
  std::set<std::string> ends;
  for (const auto& [activity, count] : relations.starts) {
    (void)count;
    starts.insert(activity);
  }
  for (const auto& [activity, count] : relations.ends) {
    (void)count;
    ends.insert(activity);
  }
  std::set<std::string> boundary = starts;
  boundary.insert(ends.begin(), ends.end());

  // The do group starts with all boundary activities. Remove those nodes and
  // their incident edges before finding weakly connected redo components.
  std::vector<std::set<std::string>> groups;
  groups.push_back(boundary);
  std::set<std::string> remaining;
  for (const auto& activity : relations.acts)
    if (boundary.count(activity) == 0) remaining.insert(activity);
  while (!remaining.empty()) {
    const auto root = *remaining.begin();
    remaining.erase(remaining.begin());
    std::set<std::string> component{root};
    std::vector<std::string> pending{root};
    while (!pending.empty()) {
      const auto activity = pending.back();
      pending.pop_back();
      for (auto it = remaining.begin(); it != remaining.end();) {
        if (!has(relations.dfg, activity, *it) &&
            !has(relations.dfg, *it, activity)) {
          ++it;
          continue;
        }
        component.insert(*it);
        pending.push_back(*it);
        it = remaining.erase(it);
      }
    }
    groups.push_back(std::move(component));
  }

  const auto group_index = [&](const std::string& activity) {
    for (std::size_t index = 0; index < groups.size(); ++index)
      if (groups[index].count(activity) != 0) return index;
    return groups.size();
  };
  const auto merge_groups_into_do = [&](const std::string& left,
                                        const std::string& right) {
    const auto left_index = group_index(left);
    const auto right_index = group_index(right);
    if (left_index == groups.size() || right_index == groups.size() ||
        left_index == right_index)
      return;
    std::set<std::string> merged = groups[left_index];
    merged.insert(groups[right_index].begin(), groups[right_index].end());
    const auto high = std::max(left_index, right_index);
    const auto low = std::min(left_index, right_index);
    groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(high));
    groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(low));
    groups.insert(groups.begin(), std::move(merged));
  };

  // A non-terminal start and a non-initial end expose adjacent components that
  // must be absorbed into the do group before completeness is checked.
  for (const auto& start : starts) {
    if (ends.count(start) != 0) continue;
    for (const auto& [edge, count] : relations.dfg) {
      (void)count;
      if (edge.first == start)
        merge_groups_into_do(edge.first, edge.second);
    }
  }
  for (const auto& end : ends) {
    if (starts.count(end) != 0) continue;
    for (const auto& [edge, count] : relations.dfg) {
      (void)count;
      if (edge.second == end)
        merge_groups_into_do(edge.first, edge.second);
    }
  }

  // A redo component with a return edge to one possible start must return to
  // every possible start. Otherwise it is merged into do.
  for (std::size_t index = 1; index < groups.size();) {
    bool merge = false;
    for (const auto& activity : groups[index]) {
      bool reaches_start = false;
      for (const auto& [edge, count] : relations.dfg) {
        (void)count;
        if (edge.first == activity && starts.count(edge.second) != 0) {
          reaches_start = true;
          break;
        }
      }
      if (!reaches_start) continue;
      for (const auto& start : starts) {
        if (!has(relations.dfg, activity, start)) {
          merge = true;
          break;
        }
      }
      if (merge) break;
    }
    if (merge) {
      groups.front().insert(groups[index].begin(), groups[index].end());
      groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(index));
    } else {
      ++index;
    }
  }

  // Apply the dual completeness rule at the end boundary: a component reached
  // from one possible end must be reached from every possible end.
  for (std::size_t index = 1; index < groups.size();) {
    bool merge = false;
    for (const auto& activity : groups[index]) {
      bool reached_from_end = false;
      for (const auto& [edge, count] : relations.dfg) {
        (void)count;
        if (edge.second == activity && ends.count(edge.first) != 0) {
          reached_from_end = true;
          break;
        }
      }
      if (!reached_from_end) continue;
      for (const auto& end : ends) {
        if (!has(relations.dfg, end, activity)) {
          merge = true;
          break;
        }
      }
      if (merge) break;
    }
    if (merge) {
      groups.front().insert(groups[index].begin(), groups[index].end());
      groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(index));
    } else {
      ++index;
    }
  }

  std::vector<std::set<std::string>> nonempty_groups;
  for (auto& group : groups) {
    if (!group.empty()) nonempty_groups.push_back(std::move(group));
  }
  groups = std::move(nonempty_groups);
  if (groups.size() <= 1) return std::nullopt;

  // The loop cut has exactly one do group and one merged redo group. Project
  // by alternating maximal slices, retaining empty trailing do slices.
  const std::set<std::string> do_activities = groups.front();
  std::set<std::string> redo_activities;
  for (std::size_t index = 1; index < groups.size(); ++index) {
    redo_activities.insert(groups[index].begin(), groups[index].end());
  }
  if (redo_activities.empty()) return std::nullopt;

  UVCL do_log;
  UVCL redo_log;
  for (const auto& [sequence, count] : log) {
    Sequence do_slice;
    Sequence redo_slice;
    const auto flush_do = [&]() {
      if (do_slice.empty()) return;
      add_variant(do_log, do_slice, count);
      do_slice.clear();
    };
    const auto flush_redo = [&]() {
      if (redo_slice.empty()) return;
      add_variant(redo_log, redo_slice, count);
      redo_slice.clear();
    };
    for (const auto& activity : sequence) {
      if (do_activities.count(activity) != 0) {
        flush_redo();
        do_slice.push_back(activity);
      } else if (redo_activities.count(activity) != 0) {
        flush_do();
        redo_slice.push_back(activity);
      } else {
        flush_do();
        flush_redo();
      }
    }
    flush_redo();
    // The loop projection retains an empty trailing do slice.
    add_variant(do_log, std::move(do_slice), count);
  }
  if (s.probe_only) return PowlNode::silent();
  return PowlNode::loop(recurse(do_log, s), recurse(redo_log, s));
}

std::vector<std::vector<std::vector<std::size_t>>> enumerate_partitions(std::size_t n, std::size_t k) {
  using Partition = std::vector<std::vector<std::size_t>>;
  using Nodes = std::vector<std::size_t>;

  // Generate set partitions in the stable first-element order used by the
  // discovery cut. Each recursive step either keeps the first element as a
  // singleton block or inserts it into each existing block from left to
  // right. The block order is observable in serialized models.
  std::function<std::vector<Partition>(const Nodes&, std::size_t)> generate =
      [&](const Nodes& nodes, std::size_t groups) {
        std::vector<Partition> result;
        if (groups == 0 || nodes.empty() || groups > nodes.size()) return result;
        if (groups == 1) {
          result.push_back({nodes});
          return result;
        }
        if (nodes.size() == groups) {
          Partition singletons;
          singletons.reserve(groups);
          for (const auto node : nodes) singletons.push_back({node});
          result.push_back(std::move(singletons));
          return result;
        }

        const auto first = nodes.front();
        Nodes remainder(nodes.begin() + 1, nodes.end());
        for (auto partition : generate(remainder, groups - 1)) {
          partition.insert(partition.begin(), {first});
          result.push_back(std::move(partition));
        }
        for (auto partition : generate(remainder, groups)) {
          for (std::size_t block = 0; block < partition.size(); ++block) {
            auto with_first = partition;
            with_first[block].insert(with_first[block].begin(), first);
            result.push_back(std::move(with_first));
          }
        }
        return result;
      };

  Nodes nodes(n);
  std::iota(nodes.begin(), nodes.end(), 0);
  return generate(nodes, k);
}

struct BlockRelation {
  bool forward = false;
  bool reverse = false;
  bool valid = true;
};

BlockRelation block_relation(const Relations& r,
                             const std::vector<std::size_t>& x,
                             const std::vector<std::size_t>& y) {
  bool f_all = true, r_all = true, f_any = false, r_any = false;
  for (const auto i : x) for (const auto j : y) {
    const bool f = has(r.efg, r.acts[i], r.acts[j]);
    const bool b = has(r.efg, r.acts[j], r.acts[i]);
    f_all = f_all && f; r_all = r_all && b; f_any = f_any || f; r_any = r_any || b;
  }
  // A block pair is valid only when all cross evidence agrees.  Both-way
  // evidence is an unordered/concurrent pair; partial/conflicting evidence
  // cannot be represented by a strict PO edge.
  if (f_all && !r_any) return {true, false, true};
  if (!f_any && r_all) return {false, true, true};
  if (f_all && r_all) return {false, false, true};
  return {false, false, false};
}

bool valid_partition(const Relations& r, const std::vector<std::vector<std::size_t>>& blocks) {
  if (blocks.size() < 2) return false;
  std::vector<std::size_t> block_of(r.acts.size(), blocks.size());
  for (std::size_t b = 0; b < blocks.size(); ++b) for (const auto i : blocks[b]) {
    if (i >= r.acts.size() || block_of[i] != blocks.size()) return false;
    block_of[i] = b;
  }
  if (std::any_of(block_of.begin(), block_of.end(), [&](std::size_t b) { return b == blocks.size(); })) return false;
  std::vector<std::vector<std::size_t>> graph(blocks.size());
  for (std::size_t x = 0; x < blocks.size(); ++x) for (std::size_t y = x + 1; y < blocks.size(); ++y) {
    const auto rel = block_relation(r, blocks[x], blocks[y]);
    if (!rel.valid) return false;
    if (rel.forward) graph[x].push_back(y);
    if (rel.reverse) graph[y].push_back(x);
  }
  // The generated relation must itself be a strict partial order.  A DAG is
  // not sufficient: missing transitive edges would be silently added by the
  // model constructor and could accept a partition whose evidence does not
  // support the resulting order.
  for (std::size_t i = 0; i < graph.size(); ++i) {
    for (const auto j : graph[i]) {
      if (i == j) return false;
      for (const auto k : graph[j]) {
        if (i == k || std::find(graph[i].begin(), graph[i].end(), k) ==
                           graph[i].end())
          return false;
      }
    }
  }
  // Every order-minimal block must contain a DFG start and every
  // order-maximal block a DFG end (the partition validity criterion).
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    bool has_start = false, has_end = false;
    for (const auto a : blocks[i]) {
      has_start = has_start || r.starts.count(r.acts[a]) != 0;
      has_end = has_end || r.ends.count(r.acts[a]) != 0;
    }
    if (graph[i].empty() && !has_end) return false;
    bool incoming = false;
    for (const auto& edges : graph) if (std::find(edges.begin(), edges.end(), i) != edges.end()) { incoming = true; break; }
    if (!incoming && !has_start) return false;
  }
  std::vector<char> mark(blocks.size(), 0);
  std::function<bool(std::size_t)> acyclic = [&](std::size_t v) {
    if (mark[v] == 1) return false; if (mark[v] == 2) return true; mark[v] = 1;
    for (auto w : graph[v]) if (!acyclic(w)) return false; mark[v] = 2; return true;
  };
  for (std::size_t i = 0; i < blocks.size(); ++i) if (!acyclic(i)) return false;
  return true;
}

struct DynamicOrderResult {
  std::vector<std::vector<std::size_t>> groups;
  std::vector<std::pair<std::size_t, std::size_t>> order;
};

bool activity_group_less(const std::vector<std::size_t>& left,
                         const std::vector<std::size_t>& right) {
  return std::lexicographical_compare(left.begin(), left.end(), right.begin(),
                                      right.end());
}

std::optional<DynamicOrderResult> dynamic_cluster_order(
    const UVCL& log, const Relations& relations, bool weighted,
    double order_frequency_ratio) {
  using Groups = std::vector<std::vector<std::size_t>>;

  std::map<std::string, std::size_t> activity_index;
  for (std::size_t i = 0; i < relations.acts.size(); ++i)
    activity_index.emplace(relations.acts[i], i);

  std::function<std::optional<DynamicOrderResult>(Groups)> generate =
      [&](Groups groups) -> std::optional<DynamicOrderResult> {
    std::sort(groups.begin(), groups.end(), activity_group_less);
    if (groups.size() < 2) return std::nullopt;
    const std::size_t n = groups.size();

    // The two dynamic variants differ only in how directional support is
    // measured: plain clustering uses presence, while frequency clustering
    // counts each direction once per trace and then applies multiplicity.
    std::vector<std::vector<bool>> any(
        n, std::vector<bool>(n, false));
    std::vector<std::vector<std::uint64_t>> frequencies(
        n, std::vector<std::uint64_t>(n, 0));
    std::vector<std::size_t> cluster_of(relations.acts.size(), n);
    for (std::size_t cluster = 0; cluster < n; ++cluster)
      for (const auto activity : groups[cluster]) cluster_of[activity] = cluster;

    if (weighted) {
      for (const auto& [sequence, count] : log) {
        std::set<std::pair<std::size_t, std::size_t>> seen;
        for (std::size_t i = 0; i < sequence.size(); ++i) {
          const auto source = activity_index.at(sequence[i]);
          const auto source_group = cluster_of[source];
          for (std::size_t j = i + 1; j < sequence.size(); ++j) {
            const auto target = activity_index.at(sequence[j]);
            const auto target_group = cluster_of[target];
            if (!seen.insert({source_group, target_group}).second) continue;
            add_checked(frequencies[source_group][target_group], count);
          }
        }
      }
      for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
          any[i][j] = frequencies[i][j] != 0;
    } else {
      for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
          for (const auto left : groups[i]) {
            for (const auto right : groups[j]) {
              any[i][j] = any[i][j] ||
                          has(relations.efg, relations.acts[left],
                              relations.acts[right]);
              any[j][i] = any[j][i] ||
                          has(relations.efg, relations.acts[right],
                              relations.acts[left]);
            }
          }
        }
      }
    }

    std::vector<std::vector<bool>> edges(
        n, std::vector<bool>(n, false));
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        if (weighted) {
          if (std::numeric_limits<std::uint64_t>::max() -
                  frequencies[i][j] < frequencies[j][i])
            throw std::overflow_error("POWL dynamic frequency overflow");
          const auto total = frequencies[i][j] + frequencies[j][i];
          if (total != 0) {
            const auto forward = static_cast<double>(frequencies[i][j]) /
                                 static_cast<double>(total);
            const auto reverse = static_cast<double>(frequencies[j][i]) /
                                 static_cast<double>(total);
            edges[i][j] = forward >= order_frequency_ratio;
            edges[j][i] = reverse >= order_frequency_ratio;
          }
        } else {
          if (any[i][j] && !any[j][i]) edges[i][j] = true;
          if (any[j][i] && !any[i][j]) edges[j][i] = true;
        }
      }
    }

    // Dynamic clustering removes both source groups and appends their
    // concatenation.  The next round re-sorts groups, but the concatenation
    // order remains part of the deterministic projected structure.
    const auto merge_by_activity = [](Groups& target,
                                      std::size_t left_activity,
                                      std::size_t right_activity) {
      std::size_t left = target.size();
      std::size_t right = target.size();
      for (std::size_t index = 0; index < target.size(); ++index) {
        if (std::find(target[index].begin(), target[index].end(),
                      left_activity) != target[index].end())
          left = index;
        if (std::find(target[index].begin(), target[index].end(),
                      right_activity) != target[index].end())
          right = index;
      }
      if (left == target.size() || right == target.size() || left == right)
        return false;
      std::vector<std::size_t> merged;
      merged.reserve(target[left].size() + target[right].size());
      merged.insert(merged.end(), target[left].begin(), target[left].end());
      merged.insert(merged.end(), target[right].begin(), target[right].end());
      if (left > right) std::swap(left, right);
      target.erase(target.begin() + static_cast<std::ptrdiff_t>(right));
      target.erase(target.begin() + static_cast<std::ptrdiff_t>(left));
      target.push_back(std::move(merged));
      return true;
    };

    // Close the relation transitively.  If the missing edge is contradicted
    // by reverse evidence, merge the two endpoint groups and rebuild from
    // the new partition; this is the key dynamic-clustering repair.
    for (;;) {
      bool added = false;
      for (std::size_t i = 0; i < n && !added; ++i) {
        for (std::size_t j = 0; j < n && !added; ++j) {
          if (i == j || !edges[i][j]) continue;
          for (std::size_t k = 0; k < n; ++k) {
            if (k == i || k == j || !edges[j][k] || edges[i][k]) continue;
            const bool reverse_evidence = weighted
                                              ? (frequencies[k][i] != 0 ||
                                                 frequencies[i][k] != 0)
                                              : any[k][i];
            if (reverse_evidence) {
              auto merged = groups;
              if (!merge_by_activity(merged, groups[i].front(),
                                     groups[k].front()))
                return std::nullopt;
              return generate(std::move(merged));
            }
            edges[i][k] = true;
            added = true;
            break;
          }
        }
      }
      if (!added) break;
    }

    // A two-cycle is not a strict partial order.  Merge the first conflicting
    // pair and regenerate the order on the coarser partition.
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        if (!edges[i][j] || !edges[j][i]) continue;
        auto merged = groups;
        if (!merge_by_activity(merged, groups[i].front(), groups[j].front()))
          return std::nullopt;
        return generate(std::move(merged));
      }
    }

    // Activities with no eventual-follows evidence in either direction are a
    // choice and are coarsened before the next order attempt.
    bool choice_merge = false;
    Groups choice_groups = groups;
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        if (edges[i][j] || edges[j][i] || any[i][j] || any[j][i]) continue;
        choice_merge =
            merge_by_activity(choice_groups, groups[i].front(),
                              groups[j].front()) || choice_merge;
      }
    }
    if (choice_merge) return generate(std::move(choice_groups));

    // Finally coarsen equal predecessor/successor neighborhoods.  The
    // two-node case intentionally keeps its observable unordered PO even
    // though both singleton neighborhoods are equal.
    std::vector<std::set<std::size_t>> predecessors(
        n), successors(n);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j) {
        if (edges[j][i]) predecessors[i].insert(j);
        if (edges[i][j]) successors[i].insert(j);
    }
    Groups equal_groups = groups;
    bool equal_merge = false;
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        if (predecessors[i] != predecessors[j] ||
            successors[i] != successors[j])
          continue;
        equal_merge =
            merge_by_activity(equal_groups, groups[i].front(),
                              groups[j].front()) || equal_merge;
      }
    }
    if (equal_merge && equal_groups.size() > 1)
      return generate(std::move(equal_groups));

    DynamicOrderResult result;
    result.groups = std::move(groups);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j)
        if (edges[i][j]) result.order.emplace_back(i, j);
    return result;
  };

  Groups initial;
  for (std::size_t i = 0; i < relations.acts.size(); ++i)
    initial.push_back({i});
  return generate(std::move(initial));
}

std::optional<PowlNode> make_po_impl(const UVCL& log, State& s) {
  const auto r = compute_relations(log);
  const auto n = r.acts.size();
  if (n < 2) return std::nullopt;
  if (s.opts.variant == PowlDiscoveryVariant::kBruteForce && s.opts.brute_force_activity_limit && n > s.opts.brute_force_activity_limit)
    throw std::runtime_error("POWL brute-force activity limit exceeded");
  std::vector<std::vector<std::size_t>> groups;
  std::vector<std::pair<std::size_t, std::size_t>> order;
  if (s.opts.variant == PowlDiscoveryVariant::kBruteForce) {
    for (std::size_t k = n; k >= 2 && groups.empty(); --k) {
      for (const auto& g : enumerate_partitions(n, k)) if (valid_partition(r, g)) { groups = g; break; }
      if (k == 2) break;
    }
  } else if (s.opts.variant == PowlDiscoveryVariant::kMaximal) {
    // Maximal groups are based only on the one-way relation, never on raw
    // bidirectional EFG neighbourhoods.
    struct Neighborhood {
      std::set<std::string> predecessors;
      std::set<std::string> successors;
      std::vector<std::size_t> members;
    };
    std::vector<Neighborhood> neighborhoods;
    for (std::size_t i = 0; i < n; ++i) {
      std::set<std::string> pre, suc;
      for (const auto& a : r.acts) if (a != r.acts[i]) {
        if (has(r.efg, a, r.acts[i]) && !has(r.efg, r.acts[i], a)) pre.insert(a);
        if (has(r.efg, r.acts[i], a) && !has(r.efg, a, r.acts[i])) suc.insert(a);
      }
      const auto found = std::find_if(
          neighborhoods.begin(), neighborhoods.end(),
          [&](const auto& neighborhood) {
            return neighborhood.predecessors == pre &&
                   neighborhood.successors == suc;
          });
      if (found == neighborhoods.end()) {
        neighborhoods.push_back({std::move(pre), std::move(suc), {i}});
      } else {
        found->members.push_back(i);
      }
    }
    for (auto& neighborhood : neighborhoods)
      groups.push_back(std::move(neighborhood.members));
    if (!valid_partition(r, groups)) groups.clear();
  } else {
    const bool weighted = s.opts.variant == PowlDiscoveryVariant::kDynamicClustering;
    const auto dynamic = dynamic_cluster_order(
        log, r, weighted, s.opts.order_frequency_ratio);
    if (dynamic) {
      groups = dynamic->groups;
      order = dynamic->order;
    }
  }
  if (groups.size() < 2) return std::nullopt;
  if (s.opts.variant != PowlDiscoveryVariant::kDynamicClustering && s.opts.variant != PowlDiscoveryVariant::kDynamicClusteringPlain) {
    for (std::size_t i = 0; i < groups.size(); ++i) for (std::size_t j = i + 1; j < groups.size(); ++j) {
      const auto rel = block_relation(r, groups[i], groups[j]);
      if (!rel.valid) return std::nullopt;
      if (rel.forward) order.emplace_back(i, j);
      if (rel.reverse) order.emplace_back(j, i);
    }
  }
  if (s.opts.variant != PowlDiscoveryVariant::kDynamicClustering && s.opts.variant != PowlDiscoveryVariant::kDynamicClusteringPlain && !valid_partition(r, groups)) return std::nullopt;
  std::vector<PowlNode> children;
  for (const auto& block : groups) { std::set<std::string> keep; for (auto i : block) keep.insert(r.acts[i]); children.push_back(recurse(project(log, keep), s)); }
  return PowlNode::strict_partial_order(std::move(children), order);
}

std::uint64_t weighted_cardinality(const UVCL& log) {
  std::uint64_t total = 0;
  for (const auto& [sequence, count] : log) {
    (void)sequence;
    add_checked(total, count);
  }
  return total;
}

std::optional<PowlNode> activity_once_fall_through(const UVCL& log,
                                                   State& s) {
  const auto acts = activities(log);
  for (const auto& candidate : acts) {
    bool exactly_once = true;
    for (const auto& [sequence, count] : log) {
      (void)count;
      if (std::count(sequence.begin(), sequence.end(), candidate) != 1) {
        exactly_once = false;
        break;
      }
    }
    if (!exactly_once) continue;
    std::set<std::string> rest(acts.begin(), acts.end());
    rest.erase(candidate);
    if (rest.empty()) continue;
    std::vector<PowlNode> children;
    children.push_back(recurse(project(log, {candidate}), s));
    children.push_back(recurse(project(log, rest), s));
    return PowlNode::concurrency(std::move(children));
  }
  return std::nullopt;
}

bool remainder_has_normal_cut(const UVCL& log, const State& state) {
  if (activities(log).size() < 2) return false;
  State probe = state;
  probe.probe_only = true;
  if (make_xor(log, probe)) return true;
  if (make_sequence(log, probe)) return true;
  if (make_concurrency(log, probe)) return true;
  if (make_loop(log, probe)) return true;
  return false;
}

std::optional<PowlNode> activity_concurrent_fall_through(const UVCL& log,
                                                         State& s) {
  const auto acts = activities(log);
  for (const auto& candidate : acts) {
    std::set<std::string> rest(acts.begin(), acts.end());
    rest.erase(candidate);
    if (rest.empty()) continue;
    const auto remainder = project(log, rest);
    if (!remainder_has_normal_cut(remainder, s)) continue;
    std::vector<PowlNode> children;
    children.push_back(recurse(project(log, {candidate}), s));
    children.push_back(recurse(remainder, s));
    return PowlNode::concurrency(std::move(children));
  }
  return std::nullopt;
}

UVCL split_tau_loop(const UVCL& log, bool strict) {
  const auto relations = compute_relations(log);
  std::set<std::string> starts;
  std::set<std::string> ends;
  for (const auto& [activity, count] : relations.starts) {
    (void)count;
    starts.insert(activity);
  }
  for (const auto& [activity, count] : relations.ends) {
    (void)count;
    ends.insert(activity);
  }

  UVCL projected;
  for (const auto& [sequence, count] : log) {
    if (sequence.empty()) {
      add_variant(projected, sequence, count);
      continue;
    }
    std::size_t begin = 0;
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      if (starts.count(sequence[i]) == 0) continue;
      if (strict && ends.count(sequence[i - 1]) == 0) continue;
      Sequence segment(sequence.begin() + static_cast<std::ptrdiff_t>(begin),
                       sequence.begin() + static_cast<std::ptrdiff_t>(i));
      add_variant(projected, segment, count);
      begin = i;
    }
    Sequence segment(sequence.begin() + static_cast<std::ptrdiff_t>(begin),
                     sequence.end());
    add_variant(projected, std::move(segment), count);
  }
  return projected;
}

std::optional<PowlNode> tau_loop_fall_through(const UVCL& log, State& s,
                                              bool strict) {
  const auto projected = split_tau_loop(log, strict);
  if (weighted_cardinality(projected) <= weighted_cardinality(log))
    return std::nullopt;
  return PowlNode::loop(recurse(projected, s), PowlNode::silent());
}

std::optional<UVCL> filtered_log(const UVCL& log,
                                 const PowlDiscoveryOptions& options) {
  // Dynamic filtering is independent of the decreasing-factor threshold.
  // A zero threshold disables only the decreasing-factor policy; the dynamic
  // policy still removes the least frequent variants as in the reference.
  if ((options.filtering_type == PowlFilteringType::kDecreasingFactor &&
       options.filtering_threshold == 0.0) ||
      log.size() < 2)
    return std::nullopt;
  UVCL filtered;
  if (options.filtering_type == PowlFilteringType::kDynamic) {
    std::uint64_t minimum = std::numeric_limits<std::uint64_t>::max();
    for (const auto& [sequence, count] : log) {
      (void)sequence;
      minimum = std::min(minimum, count);
    }
    for (const auto& [sequence, count] : log)
      if (count != minimum) add_variant(filtered, sequence, count);
  } else {
    std::vector<std::pair<Sequence, std::uint64_t>> ranked(log.begin(),
                                                           log.end());
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [](const auto& left, const auto& right) {
          return left.second > right.second;
        });
    if (!ranked.empty()) {
      add_variant(filtered, ranked.front().first, ranked.front().second);
      auto previous_retained = ranked.front().second;
      for (std::size_t i = 1; i < ranked.size(); ++i) {
        if (static_cast<long double>(ranked[i].second) >
            static_cast<long double>(previous_retained) *
                options.filtering_threshold) {
          add_variant(filtered, ranked[i].first, ranked[i].second);
          previous_retained = ranked[i].second;
        } else {
          break;
        }
      }
    }
  }
  if (filtered.empty() || filtered == log) return std::nullopt;
  return filtered;
}

}  // namespace

namespace powl_detail {

namespace {

class VariantOverride {
 public:
  VariantOverride(State& state, PowlDiscoveryVariant variant)
      : state_(state), original_(state.opts.variant) {
    state_.opts.variant = variant;
  }

  VariantOverride(const VariantOverride&) = delete;
  VariantOverride& operator=(const VariantOverride&) = delete;

  ~VariantOverride() { state_.opts.variant = original_; }

 private:
  State& state_;
  PowlDiscoveryVariant original_;
};

}  // namespace

bool EmptyLogBaseCase::detects(const VariantLog& log) {
  return std::all_of(log.begin(), log.end(),
                     [](const auto& entry) { return entry.first.empty(); });
}

BaseCaseResult BaseCase::inspect(const VariantLog& log) {
  BaseCaseResult result;
  for (const auto& [sequence, count] : log) {
    (sequence.empty() ? result.empty_weight : result.nonempty_weight) += count;
  }
  return result;
}

PowlNode BaseCaseFactory::empty() { return PowlNode::silent(); }

PowlNode BaseCaseFactory::single(const std::string& activity, bool repeated) {
  if (!repeated) return PowlNode::transition(activity);
  return PowlNode::loop(PowlNode::transition(activity), PowlNode::silent());
}

std::optional<PowlNode> SingleActivityBaseCase::resolve(
    const VariantLog& log) {
  if (log.size() != 1) return std::nullopt;
  const auto& sequence = log.begin()->first;
  if (sequence.size() > 1) return std::nullopt;
  if (sequence.empty()) return std::nullopt;
  return BaseCaseFactory::single(sequence.front(), false);
}

std::optional<PowlNode> ExclusiveChoiceCut::detect(const VariantLog& log,
                                                    State& state) {
  return make_xor(log, state);
}

std::optional<PowlNode> SequenceCut::detect(const VariantLog& log,
                                            State& state) {
  return make_sequence(log, state);
}

std::optional<PowlNode> ConcurrencyCut::detect(const VariantLog& log,
                                               State& state) {
  return make_concurrency(log, state);
}

std::optional<PowlNode> LoopCut::detect(const VariantLog& log, State& state) {
  return make_loop(log, state);
}

std::optional<PowlNode> BruteForcePartialOrderCut::detect(
    const VariantLog& log, State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kBruteForce);
  return make_po_impl(log, state);
}

std::optional<PowlNode> BruteForceCutFactory::find(const VariantLog& log,
                                                   State& state) {
  return BruteForcePartialOrderCut::detect(log, state);
}

std::optional<PowlNode> DynamicClusteringFrequencyPartialOrderCut::detect(
    const VariantLog& log, State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kDynamicClustering);
  return make_po_impl(log, state);
}

std::optional<PowlNode> DynamicClusteringFrequencyCutFactory::find(
    const VariantLog& log, State& state) {
  return DynamicClusteringFrequencyPartialOrderCut::detect(log, state);
}

std::optional<PowlNode> DynamicClusteringPartialOrderCut::detect(
    const VariantLog& log, State& state) {
  const VariantOverride override(state,
                                 PowlDiscoveryVariant::kDynamicClusteringPlain);
  return make_po_impl(log, state);
}

std::optional<PowlNode> DynamicClusteringCutFactory::find(
    const VariantLog& log, State& state) {
  return DynamicClusteringPartialOrderCut::detect(log, state);
}

std::optional<PowlNode> MaximalPartialOrderCut::detect(const VariantLog& log,
                                                       State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kMaximal);
  return make_po_impl(log, state);
}

std::optional<PowlNode> MaximalCutFactory::find(const VariantLog& log,
                                                State& state) {
  return MaximalPartialOrderCut::detect(log, state);
}

std::optional<PowlNode> CutFactory::find(const VariantLog& log, State& state) {
  std::optional<PowlNode> cut;
  switch (state.opts.variant) {
    case PowlDiscoveryVariant::kTree:
    case PowlDiscoveryVariant::kBruteForce:
    case PowlDiscoveryVariant::kMaximal:
      cut = ExclusiveChoiceCut::detect(log, state);
      if (!cut) cut = SequenceCut::detect(log, state);
      if (!cut) cut = ConcurrencyCut::detect(log, state);
      if (!cut) cut = LoopCut::detect(log, state);
      break;
    case PowlDiscoveryVariant::kDynamicClustering:
    case PowlDiscoveryVariant::kDynamicClusteringPlain:
      cut = ExclusiveChoiceCut::detect(log, state);
      if (!cut) cut = LoopCut::detect(log, state);
      break;
  }
  if (cut) return cut;

  switch (state.opts.variant) {
    case PowlDiscoveryVariant::kBruteForce:
      return BruteForceCutFactory::find(log, state);
    case PowlDiscoveryVariant::kMaximal:
      return MaximalCutFactory::find(log, state);
    case PowlDiscoveryVariant::kDynamicClustering:
      return DynamicClusteringFrequencyCutFactory::find(log, state);
    case PowlDiscoveryVariant::kDynamicClusteringPlain:
      return DynamicClusteringCutFactory::find(log, state);
    case PowlDiscoveryVariant::kTree:
      return std::nullopt;
  }
  return std::nullopt;
}

EmptyTraceSplitResult EmptyTraceFallThrough::split(const VariantLog& log) {
  const auto counts = BaseCase::inspect(log);
  EmptyTraceSplitResult result;
  result.empty_weight = counts.empty_weight;
  result.nonempty_weight = counts.nonempty_weight;
  if (!counts.all_empty()) result.nonempty = without_empty(log);
  return result;
}

std::optional<PowlNode> ActivityOncePerTraceFallThrough::apply(
    const VariantLog& log, State& state) {
  return activity_once_fall_through(log, state);
}

std::optional<PowlNode> ActivityConcurrentFallThrough::apply(
    const VariantLog& log, State& state) {
  return activity_concurrent_fall_through(log, state);
}

PowlNode FlowerFallThrough::build(
    const std::vector<std::string>& activity_names) {
  return flower(activity_names);
}

VariantLog StrictTauLoopFallThrough::project(const VariantLog& log) {
  return split_tau_loop(log, true);
}

VariantLog TauLoopFallThrough::project(const VariantLog& log) {
  return split_tau_loop(log, false);
}

FallThrough FallThroughFactory::apply(const VariantLog& log, State& state) {
  if (auto node = ActivityOncePerTraceFallThrough::apply(log, state))
    return FallThrough{std::move(node)};
  if (auto node = ActivityConcurrentFallThrough::apply(log, state))
    return FallThrough{std::move(node)};
  const auto strict = StrictTauLoopFallThrough::project(log);
  if (weighted_cardinality(strict) > weighted_cardinality(log)) {
    return FallThrough{PowlNode::loop(recurse(strict, state),
                                      PowlNode::silent())};
  }
  const auto relaxed = TauLoopFallThrough::project(log);
  if (weighted_cardinality(relaxed) > weighted_cardinality(log)) {
    return FallThrough{PowlNode::loop(recurse(relaxed, state),
                                      PowlNode::silent())};
  }
  return {};
}

std::optional<VariantLog> PowlFiltering::apply(
    const VariantLog& log, const PowlDiscoveryOptions& options) {
  return filtered_log(log, options);
}

PowlNode PowlTreeVariant::discover(const VariantLog& log, State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kTree);
  return discover_rec(log, state);
}

PowlNode PowlBruteForceVariant::discover(const VariantLog& log, State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kBruteForce);
  return discover_rec(log, state);
}

PowlNode PowlMaximalVariant::discover(const VariantLog& log, State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kMaximal);
  return discover_rec(log, state);
}

PowlNode PowlDynamicClusteringVariant::discover(const VariantLog& log,
                                                State& state) {
  const VariantOverride override(state,
                                 PowlDiscoveryVariant::kDynamicClusteringPlain);
  return discover_rec(log, state);
}

PowlNode PowlDynamicClusteringFrequencyVariant::discover(
    const VariantLog& log, State& state) {
  const VariantOverride override(state, PowlDiscoveryVariant::kDynamicClustering);
  return discover_rec(log, state);
}

const std::vector<PowlVariantDescriptor>& PowlSelectorRegistry::all() {
  static const std::vector<PowlVariantDescriptor> descriptors = {
      {PowlDiscoveryVariant::kTree, "TREE"},
      {PowlDiscoveryVariant::kBruteForce, "BRUTE_FORCE"},
      {PowlDiscoveryVariant::kMaximal, "MAXIMAL"},
      {PowlDiscoveryVariant::kDynamicClustering, "DYNAMIC_CLUSTERING"},
  };
  return descriptors;
}

bool PowlSelectorRegistry::supports(PowlDiscoveryVariant variant) noexcept {
  switch (variant) {
    case PowlDiscoveryVariant::kTree:
    case PowlDiscoveryVariant::kBruteForce:
    case PowlDiscoveryVariant::kMaximal:
    case PowlDiscoveryVariant::kDynamicClustering:
    case PowlDiscoveryVariant::kDynamicClusteringPlain:
      return true;
  }
  return false;
}

PowlNode PowlSelectorRegistry::discover(const VariantLog& log, State& state) {
  switch (state.opts.variant) {
    case PowlDiscoveryVariant::kTree:
      return PowlTreeVariant::discover(log, state);
    case PowlDiscoveryVariant::kBruteForce:
      return PowlBruteForceVariant::discover(log, state);
    case PowlDiscoveryVariant::kMaximal:
      return PowlMaximalVariant::discover(log, state);
    case PowlDiscoveryVariant::kDynamicClustering:
      return PowlDynamicClusteringFrequencyVariant::discover(log, state);
    case PowlDiscoveryVariant::kDynamicClusteringPlain:
      return PowlDynamicClusteringVariant::discover(log, state);
  }
  return PowlNode::silent();
}

}  // namespace powl_detail

namespace {

PowlNode discover_rec(const UVCL& raw, State& s) {
  if (++s.depth > 512) throw std::runtime_error("POWL recursion did not terminate");
  auto finish = [&](PowlNode n) { --s.depth; return n; };
  if (raw.empty()) return finish(powl_detail::BaseCaseFactory::empty());
  UVCL log = raw;
  if (powl_detail::EmptyLogBaseCase::detects(log))
    return finish(powl_detail::BaseCaseFactory::empty());
  const auto acts0 = activities(log);
  if (acts0.empty()) return finish(powl_detail::BaseCaseFactory::empty());
  const auto empty_split = powl_detail::EmptyTraceFallThrough::split(log);
  if (empty_split.has_empty() && !empty_split.nonempty.empty()) {
    return finish(PowlNode::xor_node(
        {PowlNode::silent(), recurse(empty_split.nonempty, s)}));
  }
  if (const auto base = powl_detail::SingleActivityBaseCase::resolve(log))
    return finish(std::move(*base));

  auto cut = powl_detail::CutFactory::find(log, s);
  if (cut) return finish(std::move(*cut));

  if (const auto filtered = powl_detail::PowlFiltering::apply(log, s.opts))
    return finish(recurse(*filtered, s));

  if (auto fall_through = powl_detail::FallThroughFactory::apply(log, s);
      fall_through.handled())
    return finish(std::move(*fall_through.node));
  return finish(powl_detail::FlowerFallThrough::build(activities(log)));
}

void validate_options(const PowlDiscoveryOptions& o) {
  if (o.filtering_type != PowlFilteringType::kDecreasingFactor && o.filtering_type != PowlFilteringType::kDynamic)
    throw std::invalid_argument("invalid POWL filtering type");
  if (o.variant != PowlDiscoveryVariant::kTree && o.variant != PowlDiscoveryVariant::kBruteForce && o.variant != PowlDiscoveryVariant::kMaximal && o.variant != PowlDiscoveryVariant::kDynamicClustering && o.variant != PowlDiscoveryVariant::kDynamicClusteringPlain)
    throw std::invalid_argument("invalid POWL discovery variant");
  if (!std::isfinite(o.filtering_threshold) || o.filtering_threshold < 0.0 || o.filtering_threshold >= 1.0)
    throw std::invalid_argument("POWL filtering threshold must satisfy 0 <= t < 1");
  if (!std::isfinite(o.order_frequency_ratio) || o.order_frequency_ratio <= 0.5 || o.order_frequency_ratio > 1.0)
    throw std::invalid_argument("POWL order frequency ratio must satisfy 0.5 < ratio <= 1");
  const auto nonempty = [](const std::string& value) {
    return value.find_first_not_of(" \t\r\n") != std::string::npos;
  };
  if (!nonempty(o.activity_key))
    throw std::invalid_argument("powl_activity_key must not be empty");
  if (!nonempty(o.timestamp_key))
    throw std::invalid_argument("powl_timestamp_key must not be empty");
  if (!nonempty(o.case_id_key))
    throw std::invalid_argument("powl_case_id_key must not be empty");
}

}  // namespace

PowlNode discover_powl(const EventLog& event_log, const PowlDiscoveryOptions& options) {
  validate_options(options);
  State state{options, 0};
  const auto normalized = normalize_powl_event_log_impl(event_log, options);
  const auto log = make_uvcl(normalized, options);
  if (options.variant == PowlDiscoveryVariant::kBruteForce &&
      options.brute_force_activity_limit != 0 &&
      activities(log).size() > options.brute_force_activity_limit) {
    throw std::runtime_error("POWL brute-force activity limit exceeded");
  }
  return simplify_powl(powl_detail::PowlSelectorRegistry::discover(log, state));
}

EventLog normalize_powl_event_log(const EventLog& event_log,
                                  const PowlDiscoveryOptions& options) {
  validate_options(options);
  return normalize_powl_event_log_impl(event_log, options);
}

}  // namespace minera
