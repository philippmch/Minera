#include "minera/resource_profiles.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr std::size_t kHardResources = 100000;
constexpr std::size_t kHardRows = 1000000;

struct Bounds {
  std::optional<double> lo, hi;
};

Bounds bounds(const ResourceProfilesOptions& o) {
  Bounds b;
  if (!o.t1.empty()) b.lo = parse_timestamp(o.t1);
  if (!o.t2.empty()) b.hi = parse_timestamp(o.t2);
  if ((!o.t1.empty() && !b.lo) || (!o.t2.empty() && !b.hi))
    throw std::invalid_argument("resource_profiles timestamp is invalid");
  if (b.lo && !std::isfinite(*b.lo) || b.hi && !std::isfinite(*b.hi))
    throw std::invalid_argument("resource_profiles timestamp must be finite");
  if (b.lo && b.hi && !(*b.lo < *b.hi))
    throw std::invalid_argument("resource_profiles requires t1 < t2");
  return b;
}

bool in_slot(std::optional<double> t, const Bounds& b) {
  if (!t || !std::isfinite(*t)) return false;
  return (!b.lo || *t >= *b.lo) && (!b.hi || *t < *b.hi);
}

const char* metric_name(ResourceProfileMetric m) {
  switch (m) {
    case ResourceProfileMetric::kDistinctActivities: return "distinct_activities";
    case ResourceProfileMetric::kActivityFrequency: return "activity_frequency";
    case ResourceProfileMetric::kActivityCompletions: return "activity_completions";
    case ResourceProfileMetric::kCaseCompletions: return "case_completions";
    case ResourceProfileMetric::kFractionCaseCompletions: return "fraction_case_completions";
    case ResourceProfileMetric::kAverageWorkload: return "average_workload";
    case ResourceProfileMetric::kMultitasking: return "multitasking";
    case ResourceProfileMetric::kAverageDurationActivity: return "average_duration_activity";
    case ResourceProfileMetric::kAverageCaseDuration: return "average_case_duration";
    case ResourceProfileMetric::kInteractionTwoResources: return "interaction_two_resources";
    case ResourceProfileMetric::kSocialPosition: return "social_position";
  }
  return "unknown";
}

bool selected(const Event& e, const ResourceProfilesOptions& o) {
  return (o.resource.empty() || e.resource == o.resource) &&
         (o.activity.empty() || e.activity == o.activity);
}

struct Interval {
  std::string resource, activity;
  double start = 0, end = 0;
};

std::vector<Interval> lifecycle_intervals(const Trace& trace) {
  std::vector<Interval> out;
  struct Pending { std::string resource, activity; double start; };
  std::map<std::pair<std::string, std::string>, std::vector<double>> pending;
  std::vector<const Event*> ordered;
  ordered.reserve(trace.events.size());
  for (const auto& e : trace.events) ordered.push_back(&e);
  std::stable_sort(ordered.begin(), ordered.end(), [](const Event* a, const Event* b) {
    const auto ta = parse_timestamp(a->timestamp), tb = parse_timestamp(b->timestamp);
    if (ta && tb && *ta != *tb) return *ta < *tb;
    return a->input_order < b->input_order;
  });
  bool saw_lifecycle = false;
  for (const auto* ep : ordered) {
    const auto& e = *ep;
    const auto t = parse_timestamp(e.timestamp);
    if (!t || !std::isfinite(*t) || e.resource.empty()) continue;
    const bool is_start = e.lifecycle == "start" || e.lifecycle == "START";
    const bool is_complete = e.lifecycle == "complete" || e.lifecycle == "COMPLETE";
    saw_lifecycle = saw_lifecycle || is_start || is_complete;
    if (is_start) {
      pending[{e.resource, e.activity}].push_back(*t);
    } else if (is_complete) {
      auto& q = pending[{e.resource, e.activity}];
      if (!q.empty()) {
        const double s = q.front(); q.erase(q.begin());
        if (*t >= s) out.push_back({e.resource, e.activity, s, *t});
      }
    }
  }
  if (!saw_lifecycle) {
    std::vector<std::pair<double, const Event*>> es;
    for (const auto& e : trace.events) {
      auto t = parse_timestamp(e.timestamp);
      if (t && std::isfinite(*t) && !e.resource.empty()) es.push_back({*t, &e});
    }
    std::stable_sort(es.begin(), es.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (std::size_t i = 0; i < es.size(); ++i) {
      const double s = i ? es[i - 1].first : es[i].first;
      if (es[i].first >= s) out.push_back({es[i].second->resource, es[i].second->activity, s, es[i].first});
    }
  } else {
    // Mixed logs retain unmarked atomic events; infer their interval from the
    // preceding completion (or a zero-length interval for the first event).
    std::vector<std::pair<double, const Event*>> es;
    for (const auto& e : trace.events) {
      auto t = parse_timestamp(e.timestamp);
      if (t && std::isfinite(*t) && !e.resource.empty() && e.lifecycle.empty()) es.push_back({*t, &e});
    }
    std::stable_sort(es.begin(), es.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (std::size_t i = 0; i < es.size(); ++i) {
      const double s = i ? es[i - 1].first : es[i].first;
      out.push_back({es[i].second->resource, es[i].second->activity, s, es[i].first});
    }
  }
  return out;
}

void add_row(ResourceProfilesResult& r, ResourceProfileRow row, std::size_t max_rows) {
  if (r.rows.size() >= max_rows) { r.diagnostics.output_truncated = true; return; }
  if (std::isfinite(row.value)) r.rows.push_back(std::move(row));
}

}  // namespace

const char* resource_profile_metric_name(ResourceProfileMetric metric) {
  return metric_name(metric);
}

ResourceProfileMetric resource_profile_metric_from_string(std::string_view name) {
  const std::pair<std::string_view, ResourceProfileMetric> values[] = {
      {"distinct_activities", ResourceProfileMetric::kDistinctActivities},
      {"activity_frequency", ResourceProfileMetric::kActivityFrequency},
      {"activity_completions", ResourceProfileMetric::kActivityCompletions},
      {"case_completions", ResourceProfileMetric::kCaseCompletions},
      {"fraction_case_completions", ResourceProfileMetric::kFractionCaseCompletions},
      {"average_workload", ResourceProfileMetric::kAverageWorkload},
      {"multitasking", ResourceProfileMetric::kMultitasking},
      {"average_duration_activity", ResourceProfileMetric::kAverageDurationActivity},
      {"average_case_duration", ResourceProfileMetric::kAverageCaseDuration},
      {"interaction_two_resources", ResourceProfileMetric::kInteractionTwoResources},
      {"social_position", ResourceProfileMetric::kSocialPosition},
  };
  for (const auto& [key, value] : values) if (name == key) return value;
  throw std::invalid_argument("unknown resource profile metric");
}

ResourceProfilesResult discover_resource_profiles(const EventLog& log,
                                                  const ResourceProfilesOptions& o) {
  if (o.max_resources == 0 || o.max_resources > kHardResources)
    throw std::invalid_argument("resource_profiles max_resources safety bound exceeded");
  if (o.max_rows == 0 || o.max_rows > kHardRows)
    throw std::invalid_argument("resource_profiles max_rows safety bound exceeded");
  if (o.metric == ResourceProfileMetric::kAverageWorkload && o.t2.empty())
    throw std::invalid_argument("average_workload requires t2");
  if (o.metric == ResourceProfileMetric::kMultitasking && (o.t1.empty() || o.t2.empty()))
    throw std::invalid_argument("multitasking requires t1 and t2");
  if (o.metric == ResourceProfileMetric::kInteractionTwoResources &&
      (o.resource.empty() || o.resource2.empty()))
    throw std::invalid_argument("interaction_two_resources requires resource and resource2");
  const auto b = bounds(o);
  ResourceProfilesResult result; result.metric = o.metric;
  result.diagnostics.trace_count = log.trace_count(); result.diagnostics.event_count = log.event_count();
  std::set<std::string> names;
  for (const auto& tr : log.traces) for (const auto& e : tr.events)
    if (!e.resource.empty()) names.insert(e.resource);
  if (names.size() > o.max_resources) throw std::invalid_argument("resource_profiles resource bound exceeded");
  result.diagnostics.resource_count = names.size();

  using Key = std::pair<std::string, std::string>;
  std::map<Key, std::uint64_t> freq, completions;
  std::map<std::string, std::uint64_t> resource_events;
  std::map<std::string, std::set<std::string>> acts;
  std::map<std::string, std::set<std::string>> completed_cases;
  std::set<std::string> all_completed_cases;
  std::map<std::string, std::set<std::string>> slot_cases_by_resource;
  std::set<std::string> slot_cases;
  std::map<Key, double> activity_duration_sum; std::map<Key, std::uint64_t> activity_duration_n;
  std::vector<Interval> intervals;
  std::map<std::string, double> case_start, case_end;
  std::map<std::string, std::set<std::string>> case_resources;
  std::map<std::string, bool> case_intersects;

  for (const auto& tr : log.traces) {
    std::optional<double> first, last;
    std::set<std::string> tr_resources;
    for (const auto& e : tr.events) {
      const auto t = parse_timestamp(e.timestamp);
      if (t && std::isfinite(*t)) { if (!first || *t < *first) first = *t; if (!last || *t > *last) last = *t; }
      if (!e.resource.empty()) tr_resources.insert(e.resource);
      if (!e.resource.empty() && in_slot(t, b)) {
        slot_cases.insert(tr.case_id);
        slot_cases_by_resource[e.resource].insert(tr.case_id);
        ++resource_events[e.resource];
        if (selected(e, o)) {
          ++freq[{e.resource, e.activity}]; acts[e.resource].insert(e.activity);
          ++completions[{e.resource, e.activity}];
        }
      }
      if (in_slot(t, b)) case_intersects[tr.case_id] = true;
    }
    if (first && last) { case_start[tr.case_id] = *first; case_end[tr.case_id] = *last; }
    case_resources[tr.case_id].insert(tr_resources.begin(), tr_resources.end());
    if (last && in_slot(last, b)) {
      all_completed_cases.insert(tr.case_id);
      for (const auto& rr : tr_resources) completed_cases[rr].insert(tr.case_id);
    }
    auto ints = lifecycle_intervals(tr); intervals.insert(intervals.end(), ints.begin(), ints.end());
  }

  switch (o.metric) {
    case ResourceProfileMetric::kDistinctActivities:
      for (const auto& [r,s] : acts) if (o.resource.empty() || o.resource == r) add_row(result,{r,"","",static_cast<double>(s.size()),static_cast<std::uint64_t>(s.size())},o.max_rows); break;
    case ResourceProfileMetric::kActivityFrequency:
      for (const auto& [k,v] : freq) if ((o.resource.empty() || o.resource == k.first) && (o.activity.empty() || o.activity == k.second)) { const auto den = resource_events[k.first]; add_row(result,{k.first,k.second,"",den ? static_cast<double>(v)/den : 0.0,v},o.max_rows); } break;
    case ResourceProfileMetric::kActivityCompletions:
      for (const auto& [k,v] : completions) if ((o.resource.empty() || o.resource == k.first) && (o.activity.empty() || o.activity == k.second)) add_row(result,{k.first,k.second,"",static_cast<double>(v),v},o.max_rows); break;
    case ResourceProfileMetric::kCaseCompletions:
      for (const auto& [r,s] : completed_cases) if (o.resource.empty() || o.resource == r) add_row(result,{r,"","",static_cast<double>(s.size()),static_cast<std::uint64_t>(s.size())},o.max_rows); break;
    case ResourceProfileMetric::kFractionCaseCompletions: {
      const double den = static_cast<double>(all_completed_cases.size());
      for (const auto& [r,s] : completed_cases) if (o.resource.empty() || o.resource == r) add_row(result,{r,"","",den == 0 ? 0.0 : static_cast<double>(s.size()) / den,static_cast<std::uint64_t>(s.size())},o.max_rows);
      break; }
    case ResourceProfileMetric::kAverageCaseDuration:
      {
        std::map<std::string, double> sums;
        std::map<std::string, std::uint64_t> counts;
        for (const auto& [case_id, resources] : case_resources) {
          const auto start = case_start.find(case_id);
          const auto end = case_end.find(case_id);
          const auto intersects = case_intersects.find(case_id);
          if (start == case_start.end() || end == case_end.end() ||
              intersects == case_intersects.end() || !intersects->second ||
              end->second < start->second) {
            continue;
          }
          const double duration = end->second - start->second;
          for (const auto& resource : resources) {
            sums[resource] += duration;
            ++counts[resource];
          }
        }
        for (const auto& [resource, sum] : sums) {
          if (!o.resource.empty() && o.resource != resource) continue;
          const auto count = counts[resource];
          add_row(result, {resource, "", "", count ? sum / count : 0.0, count}, o.max_rows);
        }
      }
      break;
    case ResourceProfileMetric::kAverageDurationActivity: {
      for (const auto& i : intervals) if ((o.resource.empty() || o.resource == i.resource) && (o.activity.empty() || o.activity == i.activity) && in_slot(i.end,b)) { activity_duration_sum[{i.resource,i.activity}] += i.end-i.start; ++activity_duration_n[{i.resource,i.activity}]; }
      for (const auto& [k,v] : activity_duration_sum) add_row(result,{k.first,k.second,"",v/activity_duration_n[k],activity_duration_n[k]},o.max_rows);
      break; }
    case ResourceProfileMetric::kAverageWorkload:
    case ResourceProfileMetric::kMultitasking: {
      std::map<std::string,std::vector<Interval>> by, all_by;
      for (const auto& i: intervals) {
        if (!o.resource.empty() && o.resource != i.resource) continue;
        if (!o.activity.empty() && o.activity != i.activity) continue;
        all_by[i.resource].push_back(i);
        if (o.metric == ResourceProfileMetric::kAverageWorkload) {
          if (!b.hi || !(i.start < *b.hi && i.end >= *b.hi)) continue;
        } else if (!(b.lo && b.hi && i.start >= *b.lo && i.end <= *b.hi)) continue;
        by[i.resource].push_back(i);
      }
      for (auto& [r,v] : by) {
        double value = 0.0;
        if (o.metric == ResourceProfileMetric::kAverageWorkload) {
          double weighted = 0.0, total = 0.0;
          const auto& all = all_by[r];
          for (const auto& i : v) { const double d = std::max(0.0, i.end - i.start); if (d == 0) continue; int n = 0; for (const auto& j : all) if (j.start < i.end + 1e-6 && j.end + 1e-6 > i.start) ++n; weighted += d * n; total += d; }
          value = total ? weighted / total : 0.0;
        } else {
          const auto& all = all_by[r];
          double denominator = 0.0, numerator = 0.0;
          for (const auto& target : v) {
            const double d = std::max(0.0, target.end - target.start);
            if (d == 0.0) continue;
            int n = 0;
            for (const auto& other : all) {
              // A tiny endpoint tolerance mirrors intervalized event
              // semantics: touching lifecycle intervals count as concurrent.
              if (other.start < target.end + 1e-6 &&
                  other.end + 1e-6 > target.start) ++n;
            }
            denominator += d;
            if (n > 1) numerator += d;
          }
          value = denominator ? numerator / denominator : 0.0;
        }
        add_row(result,{r,"","",value,0},o.max_rows);
      }
      break;
    }
    case ResourceProfileMetric::kInteractionTwoResources: {
      if (o.resource.empty() || o.resource2.empty()) break;
      std::uint64_t n = 0; for(const auto& [c,rs]:case_resources) { auto z=case_end.find(c); if(z!=case_end.end()&&in_slot(z->second,b)&&rs.count(o.resource)&&rs.count(o.resource2)) ++n; }
      add_row(result,{o.resource,"",o.resource2,static_cast<double>(n),n},o.max_rows); break; }
    case ResourceProfileMetric::kSocialPosition: {
      const double denominator = static_cast<double>(slot_cases.size());
      if (!o.resource.empty()) {
        const auto it = slot_cases_by_resource.find(o.resource);
        const std::uint64_t count = it == slot_cases_by_resource.end() ? 0 : it->second.size();
        add_row(result, {o.resource, "", "", denominator ? static_cast<double>(count) / denominator : 0.0, count}, o.max_rows);
      } else {
        for (const auto& [resource, cases] : slot_cases_by_resource) {
          const auto count = static_cast<std::uint64_t>(cases.size());
          add_row(result, {resource, "", "", denominator ? static_cast<double>(count) / denominator : 0.0, count}, o.max_rows);
        }
      }
      break;
    }
  }
  std::sort(result.rows.begin(), result.rows.end(), [](const auto&a,const auto&b){return std::tie(a.resource,a.activity,a.resource2)<std::tie(b.resource,b.activity,b.resource2);});
  result.diagnostics.computed = true; result.diagnostics.message = result.diagnostics.output_truncated ? "row safety bound applied" : (result.rows.empty()?"no matching data":"ok");
  return result;
}

std::string resource_profiles_json(const ResourceProfilesResult& result) {
  std::ostringstream out; out << "{\"family\":\"RESOURCE_PROFILES\",\"metric\":\"" << resource_profile_metric_name(result.metric) << "\",\"rows\":[";
  const auto quote = [&out](const std::string& s) { out << '"'; for (unsigned char c : s) { if (c == '\\' || c == '"') out << '\\'; if (c < 0x20) { out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << std::dec << std::setfill(' '); } else out << c; } out << '"'; };
  for(std::size_t i=0;i<result.rows.size();++i){const auto&r=result.rows[i]; if(i)out<<','; out<<"{\"resource\":"; quote(r.resource); out<<",\"activity\":"; quote(r.activity); out<<",\"resource2\":"; quote(r.resource2); out<<",\"value\":"<<std::setprecision(17)<<r.value<<",\"count\":"<<r.count<<"}";} out<<"],\"diagnostics\":{\"computed\":"<<(result.diagnostics.computed?"true":"false")<<",\"output_truncated\":"<<(result.diagnostics.output_truncated?"true":"false")<<",\"trace_count\":"<<result.diagnostics.trace_count<<",\"event_count\":"<<result.diagnostics.event_count<<",\"resource_count\":"<<result.diagnostics.resource_count<<",\"message\":"; quote(result.diagnostics.message); out << "}}"; return out.str();
}

}  // namespace minera
