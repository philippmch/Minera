#include "minera/chaotic_activities.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

struct EntropyModel {
  std::vector<std::uint64_t> counts;
  std::map<std::pair<std::size_t, std::size_t>, std::uint64_t> pairs;
};

std::optional<std::string> activity_value(const Event& event,
                                          std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  const auto* attribute = event.attribute(key);
  if (!attribute) return std::nullopt;
  return *attribute;
}

EntropyModel build_model(const std::vector<std::vector<std::size_t>>& traces,
                         std::size_t activity_count) {
  EntropyModel model;
  model.counts.assign(activity_count, 0);
  const std::size_t start = activity_count;
  const std::size_t end = activity_count + 1;
  for (const auto& trace : traces) {
    std::size_t previous = start;
    for (const auto activity : trace) {
      ++model.counts[activity];
      ++model.pairs[{previous, activity}];
      previous = activity;
    }
    ++model.pairs[{previous, end}];
  }
  return model;
}

double entropy(const std::vector<double>& values) {
  double result = 0.0;
  for (const double value : values)
    if (value > 0.0) result -= value * std::log2(value);
  return result;
}

double total_entropy(const EntropyModel& model, std::optional<double> alpha) {
  if (model.counts.empty()) return 0.0;
  std::vector<std::size_t> active;
  active.reserve(model.counts.size());
  for (std::size_t activity = 0; activity < model.counts.size(); ++activity)
    if (model.counts[activity] != 0) active.push_back(activity);
  if (active.empty()) return 0.0;
  const std::size_t category_count = active.size() + 1;
  const std::size_t start = model.counts.size();
  const std::size_t end = start + 1;
  double total = 0.0;
  for (const std::size_t activity : active) {
    const double denominator =
        static_cast<double>(model.counts[activity]) +
        (alpha ? *alpha * static_cast<double>(category_count) : 0.0);
    if (denominator <= 0.0) continue;
    std::vector<double> following;
    std::vector<double> preceding;
    following.reserve(category_count);
    preceding.reserve(category_count);
    for (const std::size_t target : active) {
      const auto count = model.pairs.find({activity, target});
      const auto previous = model.pairs.find({target, activity});
      following.push_back(
          (static_cast<double>(count == model.pairs.end() ? 0 : count->second) +
           (alpha ? *alpha : 0.0)) /
          denominator);
      preceding.push_back(
          (static_cast<double>(previous == model.pairs.end()
                                   ? 0
                                   : previous->second) +
           (alpha ? *alpha : 0.0)) /
          denominator);
    }
    const auto end_count = model.pairs.find({activity, end});
    const auto start_count = model.pairs.find({start, activity});
    following.push_back(
        (static_cast<double>(end_count == model.pairs.end() ? 0
                                                            : end_count->second) +
         (alpha ? *alpha : 0.0)) /
        denominator);
    preceding.push_back(
        (static_cast<double>(start_count == model.pairs.end()
                                 ? 0
                                 : start_count->second) +
         (alpha ? *alpha : 0.0)) /
        denominator);
    total += entropy(following) + entropy(preceding);
  }
  return total;
}

}  // namespace

ChaoticActivitiesResult discover_chaotic_activities(
    const EventLog& log, const ChaoticActivitiesOptions& options) {
  if (options.max_activities == 0 || options.max_events == 0)
    throw std::invalid_argument("chaotic activities: safety bounds must be positive");
  if (options.alpha &&
      (!std::isfinite(*options.alpha) || *options.alpha < 0.0))
    throw std::invalid_argument("chaotic activities: alpha must be finite and non-negative");

  std::map<std::string, std::size_t> ids;
  std::vector<std::vector<std::string>> labels;
  std::size_t event_count = 0;
  for (const auto& trace : log.traces) {
    std::vector<std::string> projected;
    projected.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      if (++event_count > options.max_events)
        throw std::invalid_argument("chaotic activities: event safety bound exceeded");
      const auto value = activity_value(event, options.activity_key);
      if (!value)
        throw std::invalid_argument("chaotic activities: event is missing activity attribute");
      projected.push_back(*value);
      ids.try_emplace(*value, ids.size());
      if (ids.size() > options.max_activities)
        throw std::invalid_argument("chaotic activities: activity safety bound exceeded");
    }
    labels.push_back(std::move(projected));
  }

  ChaoticActivitiesResult result;
  result.diagnostics.activity_count = ids.size();
  result.diagnostics.event_count = event_count;
  if (ids.empty()) {
    result.diagnostics.message = "empty event log";
    return result;
  }

  // Re-index lexicographically so output and entropy category order are stable.
  std::map<std::string, std::size_t> sorted_ids;
  std::vector<std::string> names;
  names.reserve(ids.size());
  for (const auto& [name, unused] : ids) {
    (void)unused;
    sorted_ids.emplace(name, names.size());
    names.push_back(name);
  }
  std::vector<std::vector<std::size_t>> traces;
  traces.reserve(labels.size());
  for (const auto& trace : labels) {
    auto& target = traces.emplace_back();
    target.reserve(trace.size());
    for (const auto& label : trace) target.push_back(sorted_ids.at(label));
  }

  const auto smoothing = options.alpha.value_or(1.0 / names.size());
  result.diagnostics.alpha = smoothing;
  const auto model = build_model(traces, names.size());
  const double baseline = total_entropy(model, std::nullopt);

  std::vector<std::uint64_t> frequencies(names.size(), 0);
  for (const auto& trace : traces)
    for (const auto activity : trace) ++frequencies[activity];

  result.activities.reserve(names.size());
  for (std::size_t activity = 0; activity < names.size(); ++activity) {
    const double denominator = static_cast<double>(frequencies[activity]);
    const std::size_t category_count = names.size() + 1;
    const std::size_t start = names.size();
    const std::size_t end = start + 1;
    std::vector<double> following;
    std::vector<double> preceding;
    following.reserve(category_count);
    preceding.reserve(category_count);
    for (std::size_t target = 0; target < names.size(); ++target) {
      const auto count = model.pairs.find({activity, target});
      const auto previous = model.pairs.find({target, activity});
      following.push_back(static_cast<double>(
          count == model.pairs.end() ? 0 : count->second) /
                          denominator);
      preceding.push_back(static_cast<double>(
          previous == model.pairs.end() ? 0 : previous->second) /
                          denominator);
    }
    const auto end_count = model.pairs.find({activity, end});
    const auto start_count = model.pairs.find({start, activity});
    following.push_back(static_cast<double>(
        end_count == model.pairs.end() ? 0 : end_count->second) /
                        denominator);
    preceding.push_back(static_cast<double>(
        start_count == model.pairs.end() ? 0 : start_count->second) /
                        denominator);
    const double raw = entropy(following) + entropy(preceding);

    std::vector<std::vector<std::size_t>> without;
    without.reserve(traces.size());
    for (const auto& trace : traces) {
      std::vector<std::size_t> filtered;
      filtered.reserve(trace.size());
      for (const auto value : trace)
        if (value != activity) filtered.push_back(value);
      if (!filtered.empty()) without.push_back(std::move(filtered));
    }
    const double gain = baseline -
                        total_entropy(build_model(without, names.size()),
                                      std::nullopt);
    const double smooth = [&] {
      std::vector<double> f;
      std::vector<double> p;
      f.reserve(category_count);
      p.reserve(category_count);
      const double smooth_denominator =
          denominator + smoothing * static_cast<double>(category_count);
      for (std::size_t target = 0; target < names.size(); ++target) {
        const auto count = model.pairs.find({activity, target});
        const auto previous = model.pairs.find({target, activity});
        f.push_back((static_cast<double>(count == model.pairs.end()
                                              ? 0
                                              : count->second) +
                     smoothing) /
                    smooth_denominator);
        p.push_back((static_cast<double>(previous == model.pairs.end()
                                              ? 0
                                              : previous->second) +
                     smoothing) /
                    smooth_denominator);
      }
      const auto ec = model.pairs.find({activity, end});
      const auto sc = model.pairs.find({start, activity});
      f.push_back((static_cast<double>(ec == model.pairs.end() ? 0
                                                              : ec->second) +
                   smoothing) /
                  smooth_denominator);
      p.push_back((static_cast<double>(sc == model.pairs.end() ? 0
                                                              : sc->second) +
                   smoothing) /
                  smooth_denominator);
      return entropy(f) + entropy(p);
    }();
    result.activities.push_back(
        {names[activity], frequencies[activity], raw, smooth, gain,
         (smooth + gain) / 2.0});
  }

  std::sort(result.activities.begin(), result.activities.end(),
            [](const auto& left, const auto& right) {
              if (left.chaotic_score != right.chaotic_score)
                return left.chaotic_score > right.chaotic_score;
              if (left.frequency != right.frequency)
                return left.frequency > right.frequency;
              return left.activity < right.activity;
            });
  result.diagnostics.message = "ok";
  return result;
}

}  // namespace minera
