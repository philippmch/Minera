#include "minera/local_diagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {
constexpr std::size_t kHardGroups = 10000;
constexpr std::size_t kHardResources = 100000;
constexpr std::size_t kHardRows = 1000000;

std::string value_for(const Event& e, const std::string& key, bool resource) {
  if (key.empty()) return resource ? e.resource : e.activity;
  if (resource && (key == "org:resource" || key == "resource")) return e.resource;
  if (!resource && (key == "concept:name" || key == "activity")) return e.activity;
  const auto* v = e.attribute(key);
  return v ? *v : std::string{};
}
bool nonblank(const std::string& s) {
  return std::any_of(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); });
}

void quote(std::ostringstream& out, const std::string& s) {
  out << '"';
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') out << '\\';
    if (c < 0x20) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<int>(c) << std::dec << std::setfill(' ');
    else out << c;
  }
  out << '"';
}

template <typename Map>
void emit_double_map(std::ostringstream& out, const Map& map) {
  out << '{'; bool first_group = true;
  for (const auto& [g, values] : map) {
    if (!first_group) out << ','; first_group = false; quote(out, g); out << ':' << '{';
    bool first = true;
    for (const auto& [k, v] : values) { if (!first) out << ','; first = false; quote(out, k); out << ':' << std::setprecision(17) << v; }
    out << '}';
  }
  out << '}';
}
}  // namespace

LocalDiagnosticsResult discover_local_diagnostics(const EventLog& log,
                                                  const LocalDiagnosticsOptions& o) {
  if (o.max_groups == 0 || o.max_groups > kHardGroups || o.max_resources == 0 ||
      o.max_resources > kHardResources || o.max_rows == 0 || o.max_rows > kHardRows)
    throw std::invalid_argument("local_diagnostics safety bound exceeded");
  LocalDiagnosticsResult result;
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();

  std::map<std::string, std::set<std::string>> members;
  std::map<std::string, std::map<std::string, std::uint64_t>> tagged;
  const bool group_attribute_mode = o.explicit_groups.empty();
  std::set<std::string> all_resources;
  if (!o.explicit_groups.empty()) {
    if (o.explicit_groups.size() > o.max_groups) throw std::invalid_argument("local_diagnostics group bound exceeded");
    for (const auto& [g, rs] : o.explicit_groups) {
      if (!nonblank(g)) throw std::invalid_argument("local_diagnostics group name is blank");
      auto& set = members[g];
      for (const auto& r : rs) if (nonblank(r)) set.insert(r);
      if (set.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");
      all_resources.insert(set.begin(), set.end());
    }
    if (all_resources.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");
  } else {
    // Group-attribute mode, as the reference apply_from_group_attribute: a
    // group is the set of resources seen with that group value, and every
    // event of a member then counts for each of its groups, whatever group
    // value (if any) that particular event carries.
    // Coverage alone stays on the tagged events: the share of a group's
    // tagged events that each member performed.
    for (const auto& tr : log.traces) for (const auto& e : tr.events) {
      const std::string resource = value_for(e, o.resource_attribute, true);
      if (!nonblank(resource)) continue;
      const auto* gv = e.attribute(o.group_attribute);
      if (!gv || !nonblank(*gv)) continue;
      auto& set = members[*gv];
      set.insert(resource);
      all_resources.insert(resource);
      if (nonblank(value_for(e, o.activity_attribute, false))) ++tagged[*gv][resource];
      if (members.size() > o.max_groups) throw std::invalid_argument("local_diagnostics group bound exceeded");
      if (set.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");
    }
  }

  // Raw counts by group, resource and activity: every event of a member
  // resource counts for each group that contains it.
  using Triple = std::map<std::string, std::map<std::string, std::map<std::string, std::uint64_t>>>;
  Triple counts;
  std::map<std::string, std::uint64_t> activity_totals;
  std::size_t skipped = 0;
  for (const auto& tr : log.traces) for (const auto& e : tr.events) {
    std::string activity = value_for(e, o.activity_attribute, false);
    std::string resource = value_for(e, o.resource_attribute, true);
    if (!nonblank(activity) || !nonblank(resource)) { ++skipped; continue; }
    all_resources.insert(resource);
    ++activity_totals[activity];
    if (all_resources.size() > o.max_resources)
      throw std::invalid_argument("local_diagnostics resource bound exceeded");
    std::vector<std::string> groups;
    for (const auto& [g, rs] : members) if (rs.contains(resource)) groups.push_back(g);
    if (groups.empty()) { ++skipped; continue; }
    for (const auto& g : groups) {
      ++counts[g][resource][activity];
    }
  }
  if (all_resources.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");

  std::map<std::string, std::uint64_t> group_totals;
  std::map<std::string, std::map<std::string, std::uint64_t>> group_activity_totals;
  for (const auto& [g, rs] : counts) for (const auto& [r, acts] : rs) for (const auto& [a, n] : acts) group_totals[g] += n;
  for (const auto& [g, rs] : counts) for (const auto& [r, acts] : rs) for (const auto& [a, n] : acts) group_activity_totals[g][a] += n;
  result.diagnostics.group_count = members.size();
  result.diagnostics.resource_count = all_resources.size();
  result.diagnostics.skipped_events = skipped;
  std::size_t rows = 0;
  const auto add_metric_row = [&]() {
    if (rows >= o.max_rows) {
      result.diagnostics.output_truncated = true;
      return false;
    }
    ++rows;
    return true;
  };
  for (const auto& [g, rs] : members) {
    for (const auto& [a, total] : activity_totals) {
      const auto group_count = group_activity_totals[g][a];
      if (add_metric_row()) {
        result.group_relative_focus[g][a] = total
            ? static_cast<double>(group_count) / total : 0.0;
      }
      if (add_metric_row()) {
        result.group_relative_stake[g][a] = group_totals[g]
            ? static_cast<double>(group_count) / group_totals[g] : 0.0;
      }
    }
    std::uint64_t coverage_total = group_totals[g];
    if (group_attribute_mode) {
      coverage_total = 0;
      for (const auto& [r, n] : tagged[g]) { (void)r; coverage_total += n; }
    }
    for (const auto& r : rs) {
      std::uint64_t resource_total = 0;
      if (group_attribute_mode) {
        if (const auto it = tagged[g].find(r); it != tagged[g].end()) resource_total = it->second;
      } else if (const auto group_it = counts.find(g); group_it != counts.end()) {
        if (const auto resource_it = group_it->second.find(r); resource_it != group_it->second.end()) {
          for (const auto& [a, n] : resource_it->second) resource_total += n;
        }
      }
      if (add_metric_row()) {
        result.group_coverage[g][r] = coverage_total
            ? static_cast<double>(resource_total) / coverage_total : 0.0;
      }
    }
  }
  for (const auto& [g, rs] : counts) {
    for (const auto& [r, acts] : rs) {
      for (const auto& [a, n] : acts) {
        if (add_metric_row()) result.group_member_contribution[g][r][a] = n;
      }
    }
  }
  result.diagnostics.row_count = rows;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated ? "row safety bound applied" : (rows ? "ok" : "no matching data");
  return result;
}

LocalDiagnosticsResult apply_from_group_attribute(const EventLog& log,
                                                  const LocalDiagnosticsOptions& options) {
  auto mode = options;
  mode.explicit_groups.clear();
  return discover_local_diagnostics(log, mode);
}

LocalDiagnosticsResult apply_from_clustering_or_roles(
    const EventLog& log,
    const std::map<std::string, std::vector<std::string>>& groups,
    LocalDiagnosticsOptions options) {
  options.explicit_groups = groups;
  return discover_local_diagnostics(log, options);
}

std::string local_diagnostics_json(const LocalDiagnosticsResult& r) {
  const auto finite_ratios = [](const auto& values) {
    for (const auto& [outer, inner] : values) {
      (void)outer;
      for (const auto& [key, value] : inner) {
        (void)key;
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) return false;
      }
    }
    return true;
  };
  if (!finite_ratios(r.group_relative_focus) ||
      !finite_ratios(r.group_relative_stake) ||
      !finite_ratios(r.group_coverage))
    throw std::invalid_argument("local diagnostics ratio is invalid");
  std::set<std::string> groups;
  std::set<std::string> resources;
  std::size_t rows = 0;
  for (const auto& [group, values] : r.group_relative_focus) {
    rows += values.size(); groups.insert(group);
  }
  for (const auto& [group, values] : r.group_relative_stake) {
    rows += values.size(); groups.insert(group);
  }
  for (const auto& [group, values] : r.group_coverage) {
    rows += values.size();
    groups.insert(group);
    for (const auto& [resource, value] : values) {
      (void)value; resources.insert(resource);
    }
  }
  for (const auto& [group, by_resource] : r.group_member_contribution) {
    groups.insert(group);
    for (const auto& [resource, activities] : by_resource) {
      rows += activities.size(); resources.insert(resource);
    }
  }
  // Membership totals include groups with no scalar output, and truncation
  // may omit whole groups. Serialized leaves still exactly match row_count.
  if (r.diagnostics.group_count < groups.size() ||
      r.diagnostics.resource_count < resources.size() ||
      r.diagnostics.row_count != rows ||
      r.diagnostics.skipped_events > r.diagnostics.event_count)
    throw std::invalid_argument("local diagnostics counts are inconsistent");
  std::ostringstream out;
  out << "{\"family\":\"LOCAL_DIAGNOSTICS\",\"group_relative_focus\":";
  emit_double_map(out, r.group_relative_focus);
  out << ",\"group_relative_stake\":"; emit_double_map(out, r.group_relative_stake);
  out << ",\"group_coverage\":"; emit_double_map(out, r.group_coverage);
  out << ",\"group_member_contribution\":{";
  bool fg = true;
  for (const auto& [g, rs] : r.group_member_contribution) {
    if (!fg) out << ','; fg = false; quote(out, g); out << ":{"; bool fr = true;
    for (const auto& [res, acts] : rs) { if (!fr) out << ','; fr = false; quote(out, res); out << ":{"; bool fa = true; for (const auto& [a,n] : acts) { if (!fa) out << ','; fa = false; quote(out,a); out << ':' << n; } out << '}'; }
    out << '}';
  }
  out << "},\"diagnostics\":{";
  out << "\"computed\":" << (r.diagnostics.computed ? "true" : "false")
      << ",\"output_truncated\":" << (r.diagnostics.output_truncated ? "true" : "false")
      << ",\"trace_count\":" << r.diagnostics.trace_count << ",\"event_count\":" << r.diagnostics.event_count
      << ",\"group_count\":" << r.diagnostics.group_count << ",\"resource_count\":" << r.diagnostics.resource_count
      << ",\"row_count\":" << r.diagnostics.row_count << ",\"skipped_events\":" << r.diagnostics.skipped_events << ",\"message\":";
  quote(out, r.diagnostics.message); out << "}}";
  return out.str();
}
}  // namespace minera
