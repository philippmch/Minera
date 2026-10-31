#include "minera/role_discovery.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace minera {
namespace {
std::string selected_value(const Event& event, const std::string& key,
                           bool resource) {
  if (resource && (key.empty() || key == "org:resource" || key == "resource"))
    return event.resource;
  if (!resource && (key.empty() || key == "concept:name" || key == "activity"))
    return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

struct WorkingRole {
  std::set<std::string> activities;
  std::map<std::string, std::size_t> resources;
};

std::string activity_key(const WorkingRole& role) {
  std::string key;
  for (const auto& activity : role.activities) {
    if (!key.empty()) key.push_back('\x1f');
    key += activity;
  }
  return key;
}

std::size_t resource_cardinality(const WorkingRole& role) {
  return role.resources.size();
}

// The reference implementation uses lexical ordering while selecting the
// next merge, and a reverse size/cardinality/name ordering for the public
// result. Keeping these two orders separate is important for reproducible
// ties after a merge.
bool lexical_role_order(const WorkingRole& left, const WorkingRole& right) {
  return activity_key(left) < activity_key(right);
}

bool final_role_order(const WorkingRole& left, const WorkingRole& right) {
  if (left.activities.size() != right.activities.size())
    return left.activities.size() > right.activities.size();
  if (resource_cardinality(left) != resource_cardinality(right))
    return resource_cardinality(left) > resource_cardinality(right);
  return activity_key(left) > activity_key(right);
}

double similarity(const WorkingRole& a, const WorkingRole& b) {
  std::set<std::string> names;
  for (const auto& [n, c] : a.resources) { (void)c; names.insert(n); }
  for (const auto& [n, c] : b.resources) { (void)c; names.insert(n); }
  double total_a = 0.0, total_b = 0.0;
  for (const auto& [n, c] : a.resources) { (void)n; total_a += c; }
  for (const auto& [n, c] : b.resources) { (void)n; total_b += c; }
  if (total_a == 0.0 || total_b == 0.0) return 0.0;
  double numerator = 0.0, denominator = 0.0;
  for (const auto& n : names) {
    const auto ia = a.resources.find(n), ib = b.resources.find(n);
    const double xa = ia == a.resources.end() ? 0.0 : ia->second / total_a;
    const double xb = ib == b.resources.end() ? 0.0 : ib->second / total_b;
    numerator += std::min(xa, xb);
    denominator += std::max(xa, xb);
  }
  return denominator == 0.0 ? 0.0 : numerator / denominator;
}
}

RoleDiscoveryResult discover_roles(const EventLog& log,
                                   const RoleDiscoveryOptions& options) {
  if (options.variant != RoleDiscoveryVariant::kLog &&
      options.variant != RoleDiscoveryVariant::kPandas)
    throw std::invalid_argument("roles_variant must be log or pandas");
  if (!std::isfinite(options.threshold) || options.threshold < 0.0 || options.threshold > 1.0)
    throw std::invalid_argument("roles_threshold must be finite and in [0,1]");
  if (options.max_resources == 0 || options.max_resources > 10000)
    throw std::invalid_argument("roles_max_resources safety bound exceeded");
  if (options.max_activities == 0 || options.max_activities > 100000)
    throw std::invalid_argument("roles_max_activities safety bound exceeded");

  std::map<std::string, std::map<std::string, std::size_t>> activity_resources;
  std::set<std::string> resources;
  for (const auto& trace : log.traces) for (const auto& event : trace.events) {
    const auto activity = selected_value(event, options.activity_attribute, false);
    const auto resource = selected_value(event, options.resource_attribute, true);
    if (activity.empty() || resource.empty()) continue;
    ++activity_resources[activity][resource];
    resources.insert(resource);
  }
  if (resources.size() > options.max_resources)
    throw std::invalid_argument("roles_max_resources safety bound exceeded");
  if (activity_resources.size() > options.max_activities)
    throw std::invalid_argument("roles_max_activities safety bound exceeded");

  std::vector<WorkingRole> roles;
  for (const auto& [activity, counts] : activity_resources)
    roles.push_back({{activity}, counts});
  std::sort(roles.begin(), roles.end(), final_role_order);
  while (roles.size() > 1) {
    double best = -1.0;
    std::size_t bi = roles.size(), bj = roles.size();
    std::string best_left, best_right;
    for (std::size_t i = 0; i < roles.size(); ++i)
      for (std::size_t j = i + 1; j < roles.size(); ++j) {
        const double score = similarity(roles[i], roles[j]);
        if (!(score > options.threshold)) continue;
        const auto candidate_left = activity_key(roles[i]);
        const auto candidate_right = activity_key(roles[j]);
        if (bi == roles.size() || score > best ||
            (score == best && (candidate_left < best_left ||
                               (candidate_left == best_left &&
                                candidate_right < best_right)))) {
          best = score; bi = i; bj = j;
          best_left = candidate_left;
          best_right = candidate_right;
        }
      }
    if (bi == roles.size()) break;
    roles[bi].activities.insert(roles[bj].activities.begin(), roles[bj].activities.end());
    // The reference merges with Counter(set_res1 + set_res2): the merged role
    // carries the summed event counts of both source roles, which then feed
    // the normalized similarities of later merges.
    for (const auto& [name, count] : roles[bj].resources)
      roles[bi].resources[name] += count;
    roles.erase(roles.begin() + static_cast<std::ptrdiff_t>(bj));
    std::sort(roles.begin(), roles.end(), lexical_role_order);
  }

  RoleDiscoveryResult result;
  result.variant = options.variant;
  std::sort(roles.begin(), roles.end(), final_role_order);
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();
  result.diagnostics.activity_count = activity_resources.size();
  result.diagnostics.resource_count = resources.size();
  for (const auto& role : roles) {
    DiscoveredRole out;
    out.activities.assign(role.activities.begin(), role.activities.end());
    for (const auto& [name, count] : role.resources) out.resources.push_back({name, count});
    result.roles.push_back(std::move(out));
  }
  result.diagnostics.role_count = result.roles.size();
  result.diagnostics.computed = true;
  result.diagnostics.message = result.roles.empty() ? "no resource activity pairs" : "ok";
  return result;
}
}  // namespace minera
