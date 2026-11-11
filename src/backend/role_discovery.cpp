#include "minera/role_discovery.hpp"

#include "frame_cells.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

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
  // The reference's originator_importance key order: a Counter built from the
  // activity's (resource) pairs, then Counter(left) + Counter(right) on each
  // merge, so the left role's keys come first and the right role's new keys
  // follow in its own order.
  std::vector<std::string> resource_order;
  // The EventLog variant on a frame: a resource's name in this role, the
  // str() of the first value of its identity among the role's events (its
  // Counter key; 1 and True are one key), the left role's on a merge.
  std::map<std::string, std::string> display;
};

// The role's sorted activity list, which the reference compares as a Python
// list (element by element, a prefix first; an empty name is an element).
const std::set<std::string>& activity_key(const WorkingRole& role) {
  return role.activities;
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
  // The sums run over a's resources in its key order, then b's others in
  // its order, as the reference does: the order decides the last bit, and
  // with it which of two equally similar pairs merges first.
  std::vector<std::string> names = a.resource_order;
  for (const auto& n : b.resource_order)
    if (!a.resources.contains(n)) names.push_back(n);
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

RoleDiscoveryResult discover_roles(const EventLog& input,
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

  // A log read on the DataFrame path (CSV and XES uploads) is read as pm4py
  // reads its frame: keys are column names (no org:resource alias), a key the
  // frame lacks raises KeyError, and cells are typed. The pandas variant's
  // groupby drops rows with a NaN activity or resource; both variants join
  // the activity names, so an activity that is no text raises TypeError
  // (observed black-box). The log variant keys each NaN resource cell as its
  // own resource (float identity), which no JSON name can tell apart: it is
  // left out here.
  const bool frame = input.dataframe_columns != nullptr;
  std::optional<EventLog> frame_storage;
  if (frame) frame_storage = dataframe_frame_event_log(input);
  const EventLog& log = frame ? *frame_storage : input;
  const auto scope = options.variant == RoleDiscoveryVariant::kPandas
                         ? FrameKeyScope::kDataFrame : FrameKeyScope::kEventLog;
  // On a two-row frame where the activity or resource key is no column,
  // the pandas variant's groupby reads the two key names as the rows' labels
  // (frame_cells.hpp): each label's first character is a resource, its
  // second an activity, counted by the rows carrying that label; a later
  // label of the same pair overwrites (observed black-box).
  const auto keys = frame ? frame_keys(log, scope) : std::set<std::string>{};
  const bool pair_labels =
      frame && scope == FrameKeyScope::kDataFrame &&
      pandas_row_label_keys(log.event_count(), keys.contains(options.activity_attribute) &&
                                                   keys.contains(options.resource_attribute));
  // The pandas variant's resources in its groupby's order: the rank of each
  // resource's Python identity.
  std::map<std::string, std::size_t> resource_rank;
  if (frame && !pair_labels) {
    // The pandas variant's groupby [resource, activity] names the first
    // missing key in that order (round BB, observed black-box).
    if (scope == FrameKeyScope::kDataFrame) {
      require_frame_key(keys, options.resource_attribute);
      require_frame_key(keys, options.activity_attribute);
    } else {
      require_frame_key(keys, options.activity_attribute);
      require_frame_key(keys, options.resource_attribute);
    }
    if (scope == FrameKeyScope::kDataFrame) {
      // groupby([resource, activity]) sorts each level over every row, also
      // the rows the other level's null drops: text beside numbers takes
      // pandas' mixed order (numbers first), values Python cannot order
      // raise, and an XES object column's dates are datetime.datetime. The
      // roles' sort keys then join each role's activity names in the
      // groups' order: the first that is no text raises (round BB, observed
      // black-box).
      std::vector<std::vector<python::Scalar>> pair_keys;
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events)
          pair_keys.push_back({frame_row_scalar(log, trace, event, options.resource_attribute).value_or(python::none()),
                               frame_row_scalar(log, trace, event, options.activity_attribute).value_or(python::none())});
      std::vector<python::Group> groups;
      try {
        groups = python::groupby(pair_keys);
      } catch (const python::TypeError& error) {
        std::string message = error.what();
        for (std::size_t at; (at = message.find("'Timestamp'")) != std::string::npos;)
          message.replace(at, 11, "'datetime.datetime'");
        throw std::invalid_argument("TypeError: " + message);
      }
      const bool object_activity = frame_column_type(log, options.activity_attribute) == DataFrameColumnType::kObject;
      for (const auto& group : groups) {
        const auto& activity = group.labels[1];
        if (activity.kind != python::Scalar::Kind::kText)
          throw std::invalid_argument(
              "TypeError: sequence item 0: expected str instance, " +
              (activity.kind == python::Scalar::Kind::kDate && object_activity ? std::string("datetime.datetime")
                                                                               : python::type_name(activity)) +
              " found");
        resource_rank.try_emplace(python::identity(group.labels[0]), resource_rank.size());
      }
    } else {
      // An XES object column's dates stay datetime.datetime in the EventLog.
      const bool object_activity = frame_column_type(log, options.activity_attribute) == DataFrameColumnType::kObject;
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events) {
          const auto activity = frame_value(log, trace, event, options.activity_attribute, scope);
          if (activity.value_class != FrameValueClass::kText)
            throw std::invalid_argument(
                std::string("TypeError: sequence item 0: expected str instance, ") +
                (activity.value_class == FrameValueClass::kDate
                     ? std::string(object_activity ? "datetime.datetime" : "Timestamp")
                 : activity.missing()
                     ? std::string(frame_column_type(log, options.activity_attribute) == DataFrameColumnType::kDate
                                       ? "NaTType"
                                       : "float")
                                      : python::type_name(frame_number_scalar(activity))) +
                " found");
        }
    }
  }
  // Resources and activities are separate dicts to pm4py.
  FrameNames frame_names, frame_activity_names;
  const auto select = [&](const Trace& trace, const Event& event, const std::string& key,
                          bool resource) -> std::optional<std::string> {
    if (frame) {
      const auto value = frame_value(log, trace, event, key, scope);
      if (value.missing()) return std::nullopt;
      return (resource ? frame_names : frame_activity_names).name(value);
    }
    auto value = selected_value(event, key, resource);
    if (value.empty()) return std::nullopt;
    return value;
  };

  std::map<std::string, std::map<std::string, std::size_t>> activity_resources;
  // Per activity, its resources in first-appearance order (the EventLog
  // variant's Counter order); the pandas variant groups by name instead.
  std::map<std::string, std::vector<std::string>> first_seen;
  std::set<std::string> resources;
  std::map<std::string, std::size_t> rank_of;
  std::map<std::string, std::map<std::string, std::string>> first_text;
  const bool log_on_frame = frame && options.variant == RoleDiscoveryVariant::kLog;
  const bool nat_resources =
      log_on_frame && !pair_labels &&
      frame_column_type(log, options.resource_attribute) == DataFrameColumnType::kDate;
  if (pair_labels) {
    std::map<std::string, std::size_t> labels;
    ++labels[options.resource_attribute];
    ++labels[options.activity_attribute];
    for (const auto& [label, rows] : labels) {
      const auto [resource, activity] = label_characters(label);
      activity_resources[activity][resource] = rows;
      resources.insert(resource);
    }
  } else for (const auto& trace : log.traces) for (const auto& event : trace.events) {
    const auto activity_value = select(trace, event, options.activity_attribute, false);
    auto resource_value = select(trace, event, options.resource_attribute, true);
    // The EventLog keeps a datetime column's missing cell as pd.NaT, one
    // object, so the Counter keys them all as one resource (a NaN cell is a
    // float of its own each time: left out; round BB, observed black-box).
    if (!resource_value && nat_resources) resource_value = std::string("NaT");
    if (!activity_value || !resource_value) continue;
    const auto& activity = *activity_value;
    const auto& resource = *resource_value;
    if (!resource_rank.empty())
      if (const auto cell = frame_row_scalar(log, trace, event, options.resource_attribute))
        if (const auto rank = resource_rank.find(python::identity(*cell)); rank != resource_rank.end())
          rank_of.try_emplace(resource, rank->second);
    if (activity_resources[activity][resource]++ == 0) {
      first_seen[activity].push_back(resource);
      if (log_on_frame) {
        const auto text = frame_value(log, trace, event, options.resource_attribute, scope).text;
        first_text[activity][resource] = text.empty() && resource == "NaT" ? resource : text;
      }
    }
    resources.insert(resource);
  }
  if (resources.size() > options.max_resources)
    throw std::invalid_argument("roles_max_resources safety bound exceeded");
  if (activity_resources.size() > options.max_activities)
    throw std::invalid_argument("roles_max_activities safety bound exceeded");

  std::vector<WorkingRole> roles;
  for (const auto& [activity, counts] : activity_resources) {
    WorkingRole role{{activity}, counts, {}, {}};
    if (options.variant == RoleDiscoveryVariant::kLog) {
      role.resource_order = first_seen[activity];
      if (log_on_frame) role.display = first_text[activity];
    } else {
      for (const auto& [name, count] : counts) {
        (void)count;
        role.resource_order.push_back(name);
      }
      // On the DataFrame path the groupby orders a role's resources by their
      // typed value (numbers by value, text by code point, numbers before
      // text in a mixed column).
      if (!rank_of.empty())
        std::stable_sort(role.resource_order.begin(), role.resource_order.end(),
                         [&](const std::string& left, const std::string& right) {
                           const auto rank = [&](const std::string& name) {
                             const auto it = rank_of.find(name);
                             return it == rank_of.end() ? rank_of.size() : it->second;
                           };
                           return rank(left) < rank(right);
                         });
      else if (frame)
        std::stable_sort(role.resource_order.begin(), role.resource_order.end(),
                         [&](const std::string& left, const std::string& right) {
                           return frame_names.less(left, right);
                         });
    }
    roles.push_back(std::move(role));
  }
  std::sort(roles.begin(), roles.end(), final_role_order);
  while (roles.size() > 1) {
    double best = -1.0;
    std::size_t bi = roles.size(), bj = roles.size();
    std::set<std::string> best_left, best_right;
    for (std::size_t i = 0; i < roles.size(); ++i)
      for (std::size_t j = i + 1; j < roles.size(); ++j) {
        const double score = similarity(roles[i], roles[j]);
        if (!(score > options.threshold)) continue;
        const auto& candidate_left = activity_key(roles[i]);
        const auto& candidate_right = activity_key(roles[j]);
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
    for (const auto& name : roles[bj].resource_order)
      if (!roles[bi].resources.contains(name)) {
        roles[bi].resource_order.push_back(name);
        if (const auto it = roles[bj].display.find(name); it != roles[bj].display.end())
          roles[bi].display.emplace(name, it->second);
      }
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
    for (const auto& name : role.resource_order) {
      const auto shown = role.display.find(name);
      out.resources.push_back({shown == role.display.end() ? name : shown->second, role.resources.at(name)});
    }
    result.roles.push_back(std::move(out));
  }
  result.diagnostics.role_count = result.roles.size();
  if (log_on_frame) {
    // One resource may show two names in two roles (1 here, True there).
    std::set<std::string> shown;
    for (const auto& role : result.roles)
      for (const auto& resource : role.resources) shown.insert(resource.name);
    result.diagnostics.resource_count = shown.size();
  }
  result.diagnostics.computed = true;
  result.diagnostics.message = result.roles.empty() ? "no resource activity pairs" : "ok";
  return result;
}
}  // namespace minera
