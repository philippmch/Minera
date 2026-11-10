#include "minera/local_diagnostics.hpp"

#include "frame_cells.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <numeric>
#include <optional>
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
// Python's repr() of a str, as a KeyError prints its key.
std::string python_repr(const std::string& text) {
  const bool double_quote = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const unsigned char c : text) {
    if (c == '\\') out += "\\\\";
    else if (c == static_cast<unsigned char>(quote)) { out += '\\'; out += quote; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c < 0x20 || c == 0x7f) {
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "\\x%02x", c);
      out += buffer;
    } else out += static_cast<char>(c);
  }
  return out + quote;
}

[[noreturn]] void zero_division() {
  throw std::invalid_argument("ZeroDivisionError: float division by zero");
}

// pm4py's local diagnostics on a DataFrame (CSV and XES uploads), which runs
// its pandas implementation (observed black-box, 3,000 random frames against
// a model of it): keys are frame columns (activity, then resource, then group
// raise KeyError), cells are typed values, and
// - C[r][a] counts the rows of resource r and activity a whose group cell is
//   no NaN (a pair seen only with NaN groups counts 0); rows without a
//   resource or an activity are left out;
// - group-attribute mode takes a group's members from the rows with that
//   group value and a resource, in first-seen order; explicit groups list
//   their members (repeats once), and a member the resource column does not
//   hold raises KeyError;
// - focus[g][a] = sum of C[r][a] over g's members / sum over all resources,
//   stake[g][a] = the same / g's total, both ZeroDivisionError on a zero
//   divisor;
// - coverage[g][r] is, with a group attribute, r's share of the rows tagged
//   with g (rows with an activity), and with explicit groups r's share of
//   the members' rows (activity or not);
// - contribution[g][r] = C[r] with its zero counts, KeyError for a member
//   with no row that has an activity.
LocalDiagnosticsResult frame_local_diagnostics(const EventLog& input,
                                               const LocalDiagnosticsOptions& o) {
  const auto frame = dataframe_frame_event_log(input);
  const auto scope = FrameKeyScope::kDataFrame;
  const auto keys = frame_keys(frame, scope);
  const std::string resource_key = o.resource_attribute.empty() ? "org:resource" : o.resource_attribute;
  const std::string activity_key = o.activity_attribute.empty() ? "concept:name" : o.activity_attribute;
  const std::string& group_key = o.group_attribute;
  // The activity and resource columns are read first, and their groupby
  // sorts before the group column is read.
  for (const auto* key : {&activity_key, &resource_key})
    if (!keys.contains(*key)) throw std::invalid_argument("KeyError: " + python_repr(*key));

  LocalDiagnosticsResult result;
  result.diagnostics.trace_count = input.trace_count();
  result.diagnostics.event_count = input.event_count();
  // Resources, activities and groups are separate dicts, keyed by Python
  // identity (1, 1.0 and True are one key) and named by str() of the first.
  struct Names {
    std::map<std::string, std::string> name;
    std::size_t size() const { return name.size(); }
    const std::string& add(const python::Scalar& value) {
      return name.try_emplace(python::identity(value), python::str(value)).first->first;
    }
    // KeyError's repr of the key: text quoted, numbers as printed.
    std::string repr(const std::string& id) const {
      return id.starts_with("S") ? python_repr(name.at(id)) : name.at(id);
    }
  };
  Names resources, activities, groups;
  struct Row {
    const std::string* resource = nullptr;
    const std::string* activity = nullptr;
    const std::string* group = nullptr;
  };
  std::vector<Row> rows;
  rows.reserve(input.event_count());
  // A listed name is a str: it matches only a text cell equal to it.
  std::map<std::string, std::string> text_resources;
  // The groupby keys: (resource, activity), and the group with a group
  // attribute; pandas sorts them, so values Python cannot order raise.
  std::vector<std::vector<python::Scalar>> pair_keys, group_keys;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      Row row;
      const auto r = frame_scalar(frame, trace, event, resource_key, scope);
      const auto a = frame_scalar(frame, trace, event, activity_key, scope);
      const auto g = frame_scalar(frame, trace, event, group_key, scope);
      if (r) {
        row.resource = &resources.add(*r);
        if (r->kind == python::Scalar::Kind::kText) text_resources.emplace(r->text, *row.resource);
      }
      if (a) row.activity = &activities.add(*a);
      if (g) row.group = &groups.add(*g);
      if (r && a) pair_keys.push_back({*r, *a});
      if (g && o.explicit_groups.empty()) group_keys.push_back({*g});
      rows.push_back(row);
    }
  try {
    (void)python::groupby(pair_keys);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("TypeError: ") + error.what());
  }
  if (!keys.contains(group_key)) throw std::invalid_argument("KeyError: " + python_repr(group_key));
  // A column named twice (the group as resource or activity, or with a group
  // attribute the resource as activity) is no single column to pandas there.
  if (group_key == resource_key || group_key == activity_key)
    throw std::invalid_argument("KeyError: " + python_repr(group_key));
  if (o.explicit_groups.empty() && resource_key == activity_key)
    throw std::invalid_argument("KeyError: " + python_repr(resource_key));
  try {
    (void)python::groupby(group_keys);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("TypeError: ") + error.what());
  }
  if (resources.size() > o.max_resources)
    throw std::invalid_argument("local_diagnostics resource bound exceeded");

  // C[r][a] with each resource's activities in first-seen order, and each
  // resource's rows (any activity).
  std::map<std::string, std::vector<std::pair<std::string, std::uint64_t>>> counts;
  std::map<std::string, std::uint64_t> resource_rows;
  std::vector<std::string> pair_resources;
  std::size_t skipped = 0;
  for (const auto& row : rows) {
    if (row.resource) ++resource_rows[*row.resource];
    if (!row.resource || !row.activity) {
      ++skipped;
      continue;
    }
    const auto [it, inserted] = counts.try_emplace(*row.resource);
    if (inserted) pair_resources.push_back(*row.resource);
    auto& pairs = it->second;
    auto pair = std::find_if(pairs.begin(), pairs.end(),
                             [&](const auto& p) { return p.first == *row.activity; });
    if (pair == pairs.end()) pair = pairs.emplace(pairs.end(), *row.activity, 0);
    if (row.group) ++pair->second;
  }
  std::vector<std::string> activity_order;
  std::map<std::string, std::uint64_t> activity_totals;
  for (const auto& r : pair_resources)
    for (const auto& [a, n] : counts[r]) {
      if (!activity_totals.contains(a)) activity_order.push_back(a);
      activity_totals[a] += n;
    }

  // The groups: name, group identity (group-attribute mode) and members.
  struct Group {
    std::string name;
    std::string id;
    std::vector<std::string> members;
  };
  const bool group_attribute_mode = o.explicit_groups.empty();
  std::vector<Group> members;
  const auto add_member = [](std::vector<std::string>& list, const std::string& r) {
    if (std::find(list.begin(), list.end(), r) == list.end()) list.push_back(r);
  };
  if (group_attribute_mode) {
    std::map<std::string, std::size_t> position;
    for (const auto& row : rows) {
      if (!row.group || !row.resource) continue;
      const auto [it, inserted] = position.try_emplace(*row.group, members.size());
      if (inserted) members.push_back({groups.name.at(*row.group), *row.group, {}});
      add_member(members[it->second].members, *row.resource);
    }
  } else {
    if (o.explicit_groups.size() > o.max_groups)
      throw std::invalid_argument("local_diagnostics group bound exceeded");
    for (const auto& [g, rs] : o.explicit_groups) {
      if (!nonblank(g)) throw std::invalid_argument("local_diagnostics group name is blank");
      Group group{g, {}, {}};
      for (const auto& r : rs) {
        const auto it = text_resources.find(r);
        if (it == text_resources.end()) throw std::invalid_argument("KeyError: " + python_repr(r));
        add_member(group.members, it->second);
      }
      if (group.members.size() > o.max_resources)
        throw std::invalid_argument("local_diagnostics resource bound exceeded");
      members.push_back(std::move(group));
    }
  }
  if (members.size() > o.max_groups) throw std::invalid_argument("local_diagnostics group bound exceeded");
  const auto count_of = [&](const std::string& r, const std::string& a) -> std::uint64_t {
    const auto it = counts.find(r);
    if (it == counts.end()) return 0;
    for (const auto& [activity, n] : it->second)
      if (activity == a) return n;
    return 0;
  };

  std::size_t leaves = 0;
  const auto add_leaf = [&]() {
    if (leaves >= o.max_rows) {
      result.diagnostics.output_truncated = true;
      return false;
    }
    ++leaves;
    return true;
  };
  for (const auto& group : members) {
    // Every group is a key of each dict, empty when nothing is counted (no
    // activity, or no member).
    (void)result.group_relative_focus[group.name];
    (void)result.group_relative_stake[group.name];
    (void)result.group_coverage[group.name];
    std::map<std::string, std::uint64_t> group_counts;
    std::uint64_t group_total = 0;
    for (const auto& a : activity_order) {
      std::uint64_t n = 0;
      for (const auto& r : group.members) n += count_of(r, a);
      group_counts[a] = n;
      group_total += n;
    }
    for (const auto& a : activity_order) {
      if (activity_totals[a] == 0) zero_division();
      if (add_leaf())
        result.group_relative_focus[group.name][activities.name.at(a)] =
            static_cast<double>(group_counts[a]) / static_cast<double>(activity_totals[a]);
    }
    for (const auto& a : activity_order) {
      if (group_total == 0) zero_division();
      if (add_leaf())
        result.group_relative_stake[group.name][activities.name.at(a)] =
            static_cast<double>(group_counts[a]) / static_cast<double>(group_total);
    }
    std::map<std::string, std::uint64_t> share;
    std::uint64_t share_total = 0;
    if (group_attribute_mode) {
      for (const auto& row : rows) {
        if (!row.resource || !row.activity || !row.group || *row.group != group.id) continue;
        ++share[*row.resource];
        ++share_total;
      }
    } else {
      for (const auto& r : group.members) {
        share[r] = resource_rows[r];
        share_total += resource_rows[r];
      }
    }
    for (const auto& r : group.members) {
      if (share_total == 0) zero_division();
      if (add_leaf())
        result.group_coverage[group.name][resources.name.at(r)] =
            static_cast<double>(share[r]) / static_cast<double>(share_total);
    }
  }
  for (const auto& group : members) {
    auto& by_resource = result.group_member_contribution[group.name];
    for (const auto& r : group.members) {
      const auto it = counts.find(r);
      if (it == counts.end()) throw std::invalid_argument("KeyError: " + resources.repr(r));
      auto& out = by_resource[resources.name.at(r)];
      for (const auto& [a, n] : it->second)
        if (add_leaf()) out[activities.name.at(a)] = n;
    }
  }
  result.diagnostics.group_count = members.size();
  result.diagnostics.resource_count = resources.size();
  result.diagnostics.skipped_events = skipped;
  result.diagnostics.row_count = leaves;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated
                                   ? "row safety bound applied"
                                   : (leaves ? "ok" : "no matching data");
  return result;
}

}  // namespace

LocalDiagnosticsResult discover_local_diagnostics(const EventLog& log,
                                                  const LocalDiagnosticsOptions& o) {
  if (o.max_groups == 0 || o.max_groups > kHardGroups || o.max_resources == 0 ||
      o.max_resources > kHardResources || o.max_rows == 0 || o.max_rows > kHardRows)
    throw std::invalid_argument("local_diagnostics safety bound exceeded");
  if (log.dataframe_columns) return frame_local_diagnostics(log, o);
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
