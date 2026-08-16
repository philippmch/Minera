#include "minera/organizational_mining.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace minera {
namespace {

constexpr std::size_t kHardResources = 10000;
constexpr std::size_t kHardEdges = 1000000;
constexpr std::size_t kHardGap = 10000;

using Pair = std::pair<std::size_t, std::size_t>;

std::string selected_value(const Event& event, const std::string& key,
                           bool resource) {
  if (resource && (key.empty() || key == "org:resource" || key == "resource"))
    return event.resource;
  if (!resource && (key.empty() || key == "concept:name" || key == "activity"))
    return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

bool handover_variant(OrganizationalSnaVariant variant) {
  return variant == OrganizationalSnaVariant::kHandover ||
         variant == OrganizationalSnaVariant::kHandoverPandas;
}
bool subcontracting_variant(OrganizationalSnaVariant variant) {
  return variant == OrganizationalSnaVariant::kSubcontracting ||
         variant == OrganizationalSnaVariant::kSubcontractingPandas;
}

std::vector<std::size_t> resource_ids(
    const Trace& trace, const std::unordered_map<std::string, std::size_t>& ids,
    const std::string& resource_attribute) {
  std::vector<std::size_t> result;
  for (const auto& event : trace.events) {
    const auto resource = selected_value(event, resource_attribute, true);
    if (resource.empty()) continue;
    const auto it = ids.find(resource);
    if (it != ids.end()) result.push_back(it->second);
  }
  return result;
}

void emit_edges(const std::map<Pair, double>& weights,
                OrganizationalSnaResult& result,
                std::size_t max_edges) {
  for (const auto& [pair, weight] : weights) {
    if (result.edges.size() >= max_edges) {
      result.diagnostics.output_truncated = true;
      break;
    }
    if (!std::isfinite(weight)) continue;
    result.edges.push_back({pair.first, pair.second, weight});
  }
}

}  // namespace

OrganizationalSnaResult discover_organizational_sna(
    const EventLog& log, const OrganizationalSnaOptions& options) {
  if (!std::isfinite(options.beta) || options.beta < 0.0 || options.beta > 1.0)
    throw std::invalid_argument("organizational SNA beta must be finite and in [0,1]");
  if (options.n == 0 || options.n > kHardGap)
    throw std::invalid_argument("organizational SNA n must be in [1,10000]");
  if (options.max_resources == 0 || options.max_resources > kHardResources)
    throw std::invalid_argument("organizational SNA max_resources safety bound exceeded");
  if (options.max_edges == 0 || options.max_edges > kHardEdges)
    throw std::invalid_argument("organizational SNA max_edges safety bound exceeded");

  OrganizationalSnaResult result;
  result.variant = options.variant;
  result.directed = handover_variant(options.variant) ||
                    subcontracting_variant(options.variant);
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();
  std::set<std::string> names;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events)
      if (const auto resource = selected_value(event, options.resource_attribute, true);
          !resource.empty()) names.insert(resource);
  if (names.size() > options.max_resources)
    throw std::invalid_argument("organizational SNA resource bound exceeded");
  result.resources.assign(names.begin(), names.end());
  result.diagnostics.resource_count = result.resources.size();
  std::unordered_map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < result.resources.size(); ++i)
    ids.emplace(result.resources[i], i);

  std::map<Pair, double> weights;
  switch (options.variant) {
    case OrganizationalSnaVariant::kHandover:
    case OrganizationalSnaVariant::kHandoverPandas: {
      std::map<std::vector<std::string>, std::size_t> variants;
      for (const auto& trace : log.traces) {
        std::vector<std::string> sequence;
        for (const auto& event : trace.events)
          sequence.push_back(selected_value(event, options.resource_attribute, true));
        ++variants[std::move(sequence)];
      }
      double dividend = 0.0;
      for (const auto& [sequence, occurrence] : variants) {
        for (std::size_t i = 0; i + 1 < sequence.size(); ++i) {
          for (std::size_t j = i + 1; j < sequence.size(); ++j) {
            const double weight = options.beta == 0.0
                                      ? 1.0
                                      : std::pow(options.beta,
                                                 static_cast<double>(j - i - 1));
            dividend += weight * occurrence;
            if (!sequence[i].empty() && !sequence[j].empty()) {
              const auto source = ids.find(sequence[i]);
              const auto target = ids.find(sequence[j]);
              if (source != ids.end() && target != ids.end())
                weights[{source->second, target->second}] += weight * occurrence;
            }
            if (options.beta == 0.0) break;
          }
        }
      }
      if (dividend > 0.0)
        for (auto& [pair, weight] : weights) { (void)pair; weight /= dividend; }
      break;
    }
    case OrganizationalSnaVariant::kWorkingTogether:
    case OrganizationalSnaVariant::kWorkingTogetherPandas: {
      if (log.trace_count() != 0) {
        const double normalizer = 1.0 / static_cast<double>(log.trace_count());
        for (const auto& trace : log.traces) {
          auto events = resource_ids(trace, ids, options.resource_attribute);
          std::sort(events.begin(), events.end());
          events.erase(std::unique(events.begin(), events.end()), events.end());
          for (std::size_t i = 0; i < events.size(); ++i) {
            for (std::size_t j = i + 1; j < events.size(); ++j) {
              weights[{events[i], events[j]}] += normalizer;
              weights[{events[j], events[i]}] += normalizer;
            }
          }
        }
      }
      break;
    }
    case OrganizationalSnaVariant::kSubcontracting:
    case OrganizationalSnaVariant::kSubcontractingPandas: {
      for (const auto& trace : log.traces) {
        std::vector<std::string> events;
        for (const auto& event : trace.events)
          events.push_back(selected_value(event, options.resource_attribute, true));
        for (std::size_t i = 0; i + options.n < events.size(); ++i) {
          if (events[i].empty() || events[i] != events[i + options.n]) continue;
          const auto source = ids.find(events[i]);
          if (source == ids.end()) continue;
          for (std::size_t j = i + 1; j < i + options.n; ++j) {
            const auto intermediary = ids.find(events[j]);
            if (intermediary != ids.end())
              weights[{source->second, intermediary->second}] += 1.0;
          }
        }
      }
      if (log.trace_count() != 0)
        for (auto& [pair, weight] : weights) { (void)pair; weight /= log.trace_count(); }
      break;
    }
    case OrganizationalSnaVariant::kJointActivities:
    case OrganizationalSnaVariant::kJointActivitiesPandas: {
      std::set<std::string> activity_names;
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events)
          if (const auto activity = selected_value(event, options.activity_attribute, false);
              !activity.empty()) activity_names.insert(activity);
      std::vector<std::string> activities(activity_names.begin(), activity_names.end());
      std::unordered_map<std::string, std::size_t> activity_ids;
      for (std::size_t i = 0; i < activities.size(); ++i)
        activity_ids.emplace(activities[i], i);
      std::vector<std::vector<double>> vectors(result.resources.size(),
                                                std::vector<double>(activities.size()));
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events) {
          const auto resource = selected_value(event, options.resource_attribute, true);
          const auto activity = selected_value(event, options.activity_attribute, false);
          if (resource.empty() || activity.empty()) continue;
          const auto ri = ids.find(resource);
          const auto ai = activity_ids.find(activity);
          if (ri != ids.end() && ai != activity_ids.end()) vectors[ri->second][ai->second] += 1.0;
        }
      for (std::size_t a = 0; a < vectors.size(); ++a) {
        for (std::size_t b = a + 1; b < vectors.size(); ++b) {
          const auto& x = vectors[a];
          const auto& y = vectors[b];
          if (x.empty()) { weights[{a, b}] = 0.0; continue; }
          const double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
          const double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / y.size();
          double numerator = 0.0, xx = 0.0, yy = 0.0;
          for (std::size_t i = 0; i < x.size(); ++i) {
            const double dx = x[i] - mean_x, dy = y[i] - mean_y;
            numerator += dx * dy;
            xx += dx * dx;
            yy += dy * dy;
          }
          const double denominator = std::sqrt(xx * yy);
          const double correlation = denominator == 0.0 ? 0.0 : numerator / denominator;
          weights[{a, b}] = correlation;
          weights[{b, a}] = correlation;
        }
      }
      break;
    }
  }
  if (options.metric_normalization) {
    double maximum = 0.0;
    for (const auto& [pair, weight] : weights) {
      (void)pair;
      maximum = std::max(maximum, std::abs(weight));
    }
    if (maximum > 0.0)
      for (auto& [pair, weight] : weights) { (void)pair; weight /= maximum; }
  }
  emit_edges(weights, result, options.max_edges);
  result.diagnostics.edge_count = result.edges.size();
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated
                                   ? "edge safety bound applied"
                                   : (result.resources.empty() ? "no resources" : "ok");
  return result;
}

}  // namespace minera
