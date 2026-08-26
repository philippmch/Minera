#include "minera/edit_distance_alignment.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

using Variant = std::vector<std::string>;

struct TargetVariant {
  Variant activities;
  std::size_t first_index = 0;
};

bool variant_less(const Variant& a, const Variant& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

std::size_t levenshtein(const Variant& a, const Variant& b) {
  std::vector<std::size_t> previous(b.size() + 1), current(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) previous[j] = j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    current[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j) {
      const std::size_t substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
      current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
    }
    previous.swap(current);
  }
  return previous[b.size()];
}

// difflib.SequenceMatcher's matching blocks are the recursively selected
// longest *contiguous* matches, breaking ties by earliest source then target
// position. Its default autojunk mode suppresses popular target symbols when
// the target has at least 200 elements (the >1%+1 threshold). A sorted map is
// used instead of Python's hash table, making b2j iteration deterministic.
struct Block { std::size_t a = 0, b = 0, n = 0; };

Block find_longest(const Variant& a, std::size_t alo, std::size_t ahi,
                   const Variant& b, std::size_t blo, std::size_t bhi) {
  std::map<std::string, std::vector<std::size_t>> b2j;
  std::map<std::string, std::size_t> counts;
  for (std::size_t j = 0; j < b.size(); ++j) {
    b2j[b[j]].push_back(j);
    ++counts[b[j]];
  }
  if (b.size() >= 200) {
    const std::size_t popularity = b.size() / 100 + 1;
    for (auto it = b2j.begin(); it != b2j.end();) {
      if (counts[it->first] > popularity) it = b2j.erase(it);
      else ++it;
    }
  }
  std::size_t best_a = alo, best_b = blo, best_n = 0;
  std::map<std::size_t, std::size_t> previous;
  for (std::size_t i = alo; i < ahi; ++i) {
    std::map<std::size_t, std::size_t> current;
    const auto indexed = b2j.find(a[i]);
    if (indexed != b2j.end()) {
      for (const std::size_t j : indexed->second) {
        if (j < blo || j >= bhi) continue;
        const auto prior = previous.find(j - (j > 0 ? 1 : 0));
        const std::size_t n = (j > 0 && prior != previous.end() && prior->first + 1 == j)
                                  ? prior->second + 1 : 1;
        current[j] = n;
        if (n > best_n || (n == best_n && n != 0 &&
                           (i - n + 1 < best_a ||
                            (i - n + 1 == best_a && j - n + 1 < best_b)))) {
          best_a = i - n + 1; best_b = j - n + 1; best_n = n;
        }
      }
    }
    previous.swap(current);
  }
  // Extend a popular-element match around the non-popular core, as
  // SequenceMatcher does after its b2j scan.
  while (best_a > alo && best_b > blo && a[best_a - 1] == b[best_b - 1])
    --best_a, --best_b, ++best_n;
  while (best_a + best_n < ahi && best_b + best_n < bhi &&
         a[best_a + best_n] == b[best_b + best_n]) ++best_n;
  return {best_a, best_b, best_n};
}

void matching_blocks(const Variant& a, std::size_t alo, std::size_t ahi,
                     const Variant& b, std::size_t blo, std::size_t bhi,
                     std::vector<Block>& out) {
  struct Work { std::size_t alo, ahi, blo, bhi; bool emit; Block block; };
  std::vector<Work> stack{{alo, ahi, blo, bhi, false, {}}};
  while (!stack.empty()) {
    Work work = stack.back();
    stack.pop_back();
    if (work.emit) { out.push_back(work.block); continue; }
    const Block block = find_longest(a, work.alo, work.ahi, b, work.blo, work.bhi);
    if (block.n == 0) continue;
    stack.push_back({block.a + block.n, work.ahi, block.b + block.n, work.bhi, false, {}});
    stack.push_back({0, 0, 0, 0, true, block});
    stack.push_back({work.alo, block.a, work.blo, block.b, false, {}});
  }
  std::vector<Block> collapsed;
  for (const auto& block : out) {
    if (!collapsed.empty() && collapsed.back().a + collapsed.back().n == block.a &&
        collapsed.back().b + collapsed.back().n == block.b)
      collapsed.back().n += block.n;
    else collapsed.push_back(block);
  }
  out.swap(collapsed);
}

std::vector<EditDistanceMove> make_moves(const Variant& source,
                                         const Variant& target) {
  if (source == target) {
    std::vector<EditDistanceMove> exact;
    for (const auto& activity : source) {
      EditDistanceMove move;
      move.type = EditDistanceMoveType::kSynchronous;
      move.log_activity = move.model_activity = move.activity = activity;
      move.log = move.model = activity;
      exact.push_back(std::move(move));
    }
    return exact;
  }
  std::vector<Block> blocks;
  matching_blocks(source, 0, source.size(), target, 0, target.size(), blocks);
  std::vector<EditDistanceMove> moves;
  std::size_t i = 0, j = 0;
  auto log_move = [&](const std::string& activity) {
    EditDistanceMove move;
    move.type = EditDistanceMoveType::kLog;
    move.log = activity;
    move.log_activity = activity;
    move.activity = activity;
    moves.push_back(std::move(move));
  };
  auto model_move = [&](const std::string& activity) {
    EditDistanceMove move;
    move.type = EditDistanceMoveType::kModel;
    move.model = activity;
    move.model_activity = activity;
    move.activity = activity;
    moves.push_back(std::move(move));
  };
  for (const auto& block : blocks) {
    while (i < block.a) log_move(source[i++]);
    while (j < block.b) model_move(target[j++]);
    for (std::size_t k = 0; k < block.n; ++k) {
      EditDistanceMove move;
      move.type = EditDistanceMoveType::kSynchronous;
      move.log = source[i];
      move.model = target[j];
      move.log_activity = source[i];
      move.model_activity = target[j];
      move.activity = source[i];
      moves.push_back(std::move(move));
      ++i; ++j;
    }
  }
  while (i < source.size()) log_move(source[i++]);
  while (j < target.size()) model_move(target[j++]);
  return moves;
}

Variant activities_of(const Trace& trace) {
  Variant result;
  result.reserve(trace.events.size());
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

std::string join_variant(const Variant& v) {
  std::string result;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) result += ',';
    result += v[i];
  }
  return result;
}

void set_attr(std::vector<EventAttribute>& attrs, std::string key,
              std::string value, AttributeKind kind = AttributeKind::kString) {
  auto first = attrs.end();
  for (auto it = attrs.begin(); it != attrs.end();) {
    if (it->key != key) {
      ++it;
    } else if (first == attrs.end()) {
      first = it++;
    } else {
      it = attrs.erase(it);
    }
  }
  if (first != attrs.end()) {
    first->value = std::move(value);
    first->kind = kind;
  } else {
    attrs.push_back({std::move(key), std::move(value), kind});
  }
}

void erase_attr(std::vector<EventAttribute>& attrs, std::string_view key) {
  attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
                             [&](const EventAttribute& attr) {
                               return attr.key == key;
                             }),
              attrs.end());
}

std::uint64_t checked_cost(std::size_t deviations, std::uint64_t unit) {
  if (deviations > std::numeric_limits<std::uint64_t>::max() / unit)
    throw std::invalid_argument("edit-distance cost overflows uint64_t");
  return static_cast<std::uint64_t>(deviations) * unit;
}

}  // namespace

std::vector<EditDistanceAlignment> align_edit_distance(
    const EventLog& source, const EventLog& target, EditDistanceOptions options) {
  if (options.deviation_cost == 0) {
    throw std::invalid_argument("edit-distance deviation_cost must be positive");
  }
  if (source.traces.empty()) return {};
  if (target.traces.empty()) {
    throw std::invalid_argument("edit-distance target log must not be empty");
  }

  std::vector<TargetVariant> targets;
  std::map<Variant, std::size_t> target_seen;
  for (std::size_t i = 0; i < target.traces.size(); ++i) {
    Variant v = activities_of(target.traces[i]);
    if (target_seen.emplace(v, targets.size()).second) targets.push_back({std::move(v), i});
  }
  std::size_t shortest = std::numeric_limits<std::size_t>::max();
  for (const auto& t : targets) shortest = std::min(shortest, t.activities.size());
  std::map<Variant, EditDistanceAlignment> cache;
  std::vector<EditDistanceAlignment> result;
  result.reserve(source.traces.size());
  for (const auto& trace : source.traces) {
    Variant source_variant = activities_of(trace);
    auto cached = cache.find(source_variant);
    if (cached == cache.end()) {
      std::size_t selected = 0;
      std::size_t selected_distance = options.perform_anti_alignment ? 0 : std::numeric_limits<std::size_t>::max();
      for (std::size_t ti = 0; ti < targets.size(); ++ti) {
        const std::size_t distance = levenshtein(source_variant, targets[ti].activities);
        bool better = options.perform_anti_alignment ? distance > selected_distance
                                                       : distance < selected_distance;
        if (!better && distance == selected_distance &&
            (variant_less(targets[ti].activities, targets[selected].activities) ||
             (targets[ti].activities == targets[selected].activities &&
              targets[ti].first_index < targets[selected].first_index))) better = true;
        if (better) { selected = ti; selected_distance = distance; }
      }
      const auto moves = make_moves(source_variant, targets[selected].activities);
      std::uint64_t deviations = 0;
      for (const auto& move : moves)
        if (move.type != EditDistanceMoveType::kSynchronous) ++deviations;
      EditDistanceAlignment alignment;
      alignment.alignment = moves;
      alignment.moves = moves;
      alignment.source_variant = source_variant;
      alignment.target_variant = targets[selected].activities;
      alignment.target_index = targets[selected].first_index;
      alignment.cost = checked_cost(deviations, options.deviation_cost);
      if (source_variant.size() > std::numeric_limits<std::size_t>::max() - shortest)
        throw std::invalid_argument("edit-distance denominator overflows size_t");
      alignment.denominator = checked_cost(source_variant.size() + shortest,
                                           options.deviation_cost);
      // The complete worst-case denominator `bwc` (the shortest
      // target plus every source event as a deviation), not merely the target
      // component of that sum.
      alignment.best_worst_cost = alignment.denominator;
      alignment.bwc = alignment.denominator;
      alignment.fitness = alignment.cost == 0 ? 1.0
          : (alignment.denominator == 0 ? 0.0 : 1.0 - static_cast<double>(deviations) /
                                                static_cast<double>(source_variant.size() + shortest));
      cached = cache.emplace(source_variant, std::move(alignment)).first;
    }
    result.push_back(cached->second);
  }
  return result;
}

EventLog project_log_on_alignments(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments) {
  if (alignments.size() != source.traces.size()) {
    throw std::invalid_argument(
        "edit-distance projection requires one alignment per source trace");
  }
  EventLog projected;
  projected.attributes = source.attributes;
  erase_attr(projected.attributes, "@@aligned_variant");
  erase_attr(projected.attributes, "@@aligned_variants");
  bool uniform_target = true;
  for (std::size_t i = 1; i < alignments.size(); ++i) {
    if (alignments[i].target_variant != alignments.front().target_variant) {
      uniform_target = false;
      break;
    }
  }
  if (uniform_target && !alignments.empty()) {
    set_attr(projected.attributes, "@@aligned_variant",
             join_variant(alignments.front().target_variant));
  } else if (!alignments.empty()) {
    // A single log-level target marker would be ambiguous for heterogeneous
    // projections. Keep the marker explicit while recording the selected
    // variant on each trace below.
    set_attr(projected.attributes, "@@aligned_variants", "multiple");
  }
  for (std::size_t ti = 0; ti < source.traces.size(); ++ti) {
    const auto& original = source.traces[ti];
    const auto& alignment = alignments[ti];
    Trace projected_trace;
    projected_trace.case_id = original.case_id;
    projected_trace.attributes = original.attributes;
    set_attr(projected_trace.attributes, "@@original_trace",
             join_variant(alignment.source_variant));
    set_attr(projected_trace.attributes, "@@aligned_variant",
             join_variant(alignment.target_variant));
    std::string current_timestamp;
    if (!original.events.empty()) current_timestamp = original.events.front().timestamp;
    std::size_t source_pos = 0;
    for (const auto& move : alignment.alignment) {
      if (move.type == EditDistanceMoveType::kLog) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        ++source_pos;
        continue;
      }
      Event event;
      if (move.type == EditDistanceMoveType::kSynchronous) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        event = original.events[source_pos++];
        current_timestamp = event.timestamp;
      } else {
        event.case_id = original.case_id;
        event.activity = move.model_activity;
        event.timestamp = current_timestamp;
      }
      event.activity = move.model_activity;
      event.input_order = projected_trace.events.size();
      set_attr(event.attributes, "@@is_conforming",
               move.type == EditDistanceMoveType::kSynchronous ? "true" : "false",
               AttributeKind::kBoolean);
      projected_trace.events.push_back(std::move(event));
    }
    projected.traces.push_back(std::move(projected_trace));
  }
  return projected;
}

EventLog project_log_on_variant(const EventLog& source,
                                const std::vector<std::string>& variant,
                                EditDistanceOptions options) {
  EventLog target;
  Trace target_trace;
  target_trace.events.reserve(variant.size());
  for (const auto& activity : variant) {
    target_trace.events.push_back(Event{"", activity});
  }
  target.traces.push_back(std::move(target_trace));
  auto projected = project_log_on_alignments(
      source, align_edit_distance(source, target, options));
  // Preserve the historical marker even when the source log has no traces.
  set_attr(projected.attributes, "@@aligned_variant", join_variant(variant));
  erase_attr(projected.attributes, "@@aligned_variants");
  return projected;
}

}  // namespace minera
