#pragma once

// pm4py's log skeleton keyed by objects equal as dict keys but printed apart
// (0.0 and -0.0, 1 and 1.0): each relation keys its pairs by the first tuple
// of objects its own pass meets (round HO, observed black-box), traces in
// order: directly_follows each event with the next, always_after each event
// with every later one, always_before each event with every earlier one from
// the trace's start, equivalence the first object of each class in the first
// trace where both hold equal counts; never_together and activ_freq name a
// class by its first object (the class's label). The conformance's
// deviations of those four relations name the model's pairs; a trace's
// never_together deviations are the intersection of the model's pairs whose
// source the trace holds with the trace's pairs a != b of its variant's
// first objects (a NaN or NaT class paired with itself), named from the
// trace when the model's set is not the smaller (CPython iterates the
// smaller operand).
//
// `labels` holds each trace's class labels (a trailing U+001F run keeps
// classes printed alike apart), `printed` str() of each event's own object,
// `missing` its NaN or NaT cells (may be empty), `order` the traces in the
// discovery's order (all in index order when empty); `conformance[i]` is
// trace i's result.

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "minera/log_skeleton.hpp"

namespace minera::detail {

inline void name_log_skeleton_pairs(LogSkeletonModel& model, std::vector<LogSkeletonConformance>& conformance,
                                    const std::vector<std::vector<std::string>>& labels,
                                    const std::vector<std::vector<std::string>>& printed,
                                    const std::vector<std::vector<bool>>& missing,
                                    const std::vector<std::size_t>& order) {
  const auto trailing = [](const std::string& label) {
    const auto end = label.find_last_not_of('\x1f');
    return label.substr(end == std::string::npos ? 0 : end + 1);
  };
  if (printed.size() != labels.size()) return;
  bool apart = false;
  for (std::size_t t = 0; t < labels.size() && !apart; ++t) {
    if (printed[t].size() != labels[t].size()) return;
    for (std::size_t e = 0; e < printed[t].size(); ++e) {
      const auto& label = labels[t][e];
      if (label.substr(0, label.size() - trailing(label).size()) != printed[t][e]) {
        apart = true;
        break;
      }
    }
  }
  if (!apart) return;
  using Pair = std::pair<std::string, std::string>;
  std::map<Pair, Pair> directly, after, before, equivalent;
  std::vector<std::size_t> traces = order;
  if (traces.empty())
    for (std::size_t t = 0; t < labels.size(); ++t) traces.push_back(t);
  for (const auto t : traces) {
    if (t >= labels.size()) continue;
    const auto& events = labels[t];
    const auto& shown = printed[t];
    const auto n = events.size();
    for (std::size_t i = 0; i + 1 < n; ++i) directly.try_emplace({events[i], events[i + 1]}, shown[i], shown[i + 1]);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j) {
        if (j > i) after.try_emplace({events[i], events[j]}, shown[i], shown[j]);
        if (j < i) before.try_emplace({events[i], events[j]}, shown[i], shown[j]);
      }
    std::vector<std::size_t> firsts;
    std::map<std::string, std::size_t> counts;
    for (std::size_t i = 0; i < n; ++i)
      if (counts[events[i]]++ == 0) firsts.push_back(i);
    for (const auto a : firsts)
      for (const auto b : firsts)
        if (a != b && counts[events[a]] == counts[events[b]])
          equivalent.try_emplace({events[a], events[b]}, shown[a], shown[b]);
  }
  // A renamed label never takes another class's: more U+001F keep it apart.
  std::map<std::string, std::string> taken;
  for (const auto& trace : labels)
    for (const auto& label : trace) taken.try_emplace(label, label);
  const auto rename = [&](const std::string& label, const std::string& shown) {
    auto out = shown + trailing(label);
    while (true) {
      const auto [at, added] = taken.try_emplace(out, label);
      if (added || at->second == label) return out;
      out += '\x1f';
    }
  };
  const auto apply = [&](std::vector<LogSkeletonPair>& pairs, const std::map<Pair, Pair>& met) {
    for (auto& pair : pairs)
      if (const auto found = met.find({pair.source, pair.target}); found != met.end()) {
        pair.source = rename(pair.source, found->second.first);
        pair.target = rename(pair.target, found->second.second);
      }
  };
  // never_together's operands, before the model's pairs are renamed.
  std::set<Pair> never_together;
  for (const auto& pair : model.never_together) never_together.insert({pair.source, pair.target});
  apply(model.directly_follows, directly);
  apply(model.always_after, after);
  apply(model.always_before, before);
  apply(model.equivalence, equivalent);
  for (auto* relation : {&model.directly_follows, &model.always_after, &model.always_before, &model.equivalence})
    std::sort(relation->begin(), relation->end());
  for (auto& result : conformance)
    for (auto& deviation : result.deviations) {
      const auto* met = deviation.constraint == "directly_follows" ? &directly
                        : deviation.constraint == "always_after"   ? &after
                        : deviation.constraint == "always_before"  ? &before
                        : deviation.constraint == "equivalence"    ? &equivalent
                                                                   : nullptr;
      if (!met) continue;
      if (const auto found = met->find({deviation.relation.source, deviation.relation.target}); found != met->end()) {
        deviation.relation.source = rename(deviation.relation.source, found->second.first);
        deviation.relation.target = rename(deviation.relation.target, found->second.second);
      }
    }
  // never_together: each trace's variant (its labels) first met in `traces`.
  std::map<std::vector<std::string>, std::size_t> variant_first;
  for (const auto t : traces)
    if (t < labels.size()) variant_first.try_emplace(labels[t], t);
  for (std::size_t i = 0; i < conformance.size() && i < labels.size(); ++i) {
    auto& deviations = conformance[i].deviations;
    if (std::none_of(deviations.begin(), deviations.end(),
                     [](const auto& deviation) { return deviation.constraint == "never_together"; }))
      continue;
    const auto found = variant_first.find(labels[i]);
    const auto first = found == variant_first.end() ? i : found->second;
    const auto& events = labels[first];
    std::map<std::string, std::string> first_shown;
    std::size_t unequal = 0;
    for (std::size_t e = 0; e < events.size(); ++e)
      if (first_shown.try_emplace(events[e], printed[first][e]).second && first < missing.size() &&
          e < missing[first].size() && missing[first][e])
        ++unequal;
    const auto k = first_shown.size();
    std::size_t model_pairs = 0;
    for (const auto& pair : never_together) model_pairs += first_shown.contains(pair.first);
    if (model_pairs < k * (k - 1) + unequal) continue;
    for (auto& deviation : deviations) {
      if (deviation.constraint != "never_together") continue;
      const auto source = first_shown.find(deviation.relation.source);
      const auto target = first_shown.find(deviation.relation.target);
      if (source == first_shown.end() || target == first_shown.end()) continue;
      deviation.relation.source = rename(deviation.relation.source, source->second);
      deviation.relation.target = rename(deviation.relation.target, target->second);
    }
  }
}

}  // namespace minera::detail
