#include "minera/resource_profiles.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

#include "frame_cells.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "python_scalar.hpp"
#include "reference_activity_selector.hpp"

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

// Presence-aware selectors: an explicitly present empty value is an ordinary
// resource or activity, while a missing or null one is no value at all. An
// omitted selector reads the projection; an explicit key reads the literal
// field of that name, or a projection read from that same field.
std::optional<std::string> selected_value(const Event& e, const std::string& key,
                                          const std::string& projection,
                                          FirstClassAttributeState state) {
  if (key.empty()) {
    if (state == FirstClassAttributeState::kPresent ||
        (state == FirstClassAttributeState::kMissing && !projection.empty()))
      return projection;
    return std::nullopt;
  }
  if (const auto* value = reference_selected_field(e, key)) return *value;
  return std::nullopt;
}

std::optional<std::string> resource_of(const Event& e, const ResourceProfilesOptions& o) {
  return selected_value(e, o.resource_attribute, e.resource, e.resource_state);
}

std::optional<std::string> activity_of(const Event& e, const ResourceProfilesOptions& o) {
  return selected_value(e, o.activity_attribute, e.activity, e.activity_state);
}

// An empty query value means every value unless the *_given flag selects the
// literal empty value.
bool resource_matches(const std::string& value, const ResourceProfilesOptions& o) {
  return (o.resource.empty() && !o.resource_given) || value == o.resource;
}
bool activity_matches(const std::string& value, const ResourceProfilesOptions& o) {
  return (o.activity.empty() && !o.activity_given) || value == o.activity;
}

std::string timestamp_of(const Event& e, const ResourceProfilesOptions& o) {
  if (o.timestamp_attribute.empty()) return e.timestamp;
  if (const auto* value = reference_selected_field(e, o.timestamp_attribute)) return *value;
  return {};
}

std::string case_id_of(const Trace& trace, const Event& e,
                       const ResourceProfilesOptions& o) {
  if (o.case_id_attribute.empty() || o.case_id_attribute == "case:concept:name" ||
      o.case_id_attribute == "case:id" || o.case_id_attribute == "case_id")
    return trace.case_id.empty() ? e.case_id : trace.case_id;
  if (const auto* value = trace.attribute(o.case_id_attribute)) return *value;
  if (const auto* value = e.attribute(o.case_id_attribute)) return *value;
  return {};
}

std::optional<double> explicit_start_of(const Event& e,
                                        const ResourceProfilesOptions& o) {
  if (o.start_timestamp_attribute.empty()) return std::nullopt;
  const auto* text = e.attribute(o.start_timestamp_attribute);
  if (!text) return std::nullopt;
  const auto start = parse_timestamp(*text);
  if (!start || !std::isfinite(*start)) return std::nullopt;
  return *start;
}

struct Interval {
  std::string resource, activity;
  double start = 0, end = 0;
};

// Activity intervals as the reference forms them: only events with no
// lifecycle or a (case-insensitive) "complete" transition take part, sorted by
// timestamp. Each one spans from its explicit start timestamp when one is
// configured, else from the previous kept event (or itself, for the first).
//
// With every_transition (average_duration_activity, average_workload and
// multitasking, which follow the reference DataFrame path that CSV/XES inputs
// take): every timestamped event
// takes part whatever its lifecycle, so an activity spans from the previous
// event of its case in timestamp order. PM4Py 2.7.22 pandas variant:
// B complete@0, D start@10, D complete@100 gives D 90 (the EventLog variant
// pairs lifecycles and gives 100).
std::vector<Interval> lifecycle_intervals(
    const Trace& trace, const ResourceProfilesOptions& o,
    bool every_transition = false) {
  const auto complete_or_unmarked = [](const std::string& lifecycle) {
    if (lifecycle.empty()) return true;
    std::string lower = lifecycle;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "complete";
  };
  struct Kept { double time; const Event* event; };
  std::vector<Kept> kept;
  for (const auto& e : trace.events) {
    const auto t = parse_timestamp(timestamp_of(e, o));
    if (!t || !std::isfinite(*t) || (!every_transition && !complete_or_unmarked(e.lifecycle))) continue;
    kept.push_back({*t, &e});
  }
  std::stable_sort(kept.begin(), kept.end(), [](const Kept& a, const Kept& b) { return a.time < b.time; });
  std::vector<Interval> out;
  for (std::size_t i = 0; i < kept.size(); ++i) {
    const auto& e = *kept[i].event;
    const auto resource = resource_of(e, o);
    if (!resource) continue;
    double start = i ? kept[i - 1].time : kept[i].time;
    if (const auto explicit_start = explicit_start_of(e, o)) start = *explicit_start;
    out.push_back({*resource, activity_of(e, o).value_or(std::string{}), start, kept[i].time});
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

namespace {
ResourceProfilesResult frame_resource_profiles(const EventLog& input, const ResourceProfilesOptions& o,
                                               const Bounds& b);
ResourceProfilesResult compute_resource_profiles(const EventLog& log, const ResourceProfilesOptions& o,
                                                 const Bounds& b, bool frame);
}  // namespace

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
      ((o.resource.empty() && !o.resource_given) || (o.resource2.empty() && !o.resource2_given)))
    throw std::invalid_argument("interaction_two_resources requires resource and resource2");
  const auto b = bounds(o);
  if (log.dataframe_columns) return frame_resource_profiles(log, o, b);
  return compute_resource_profiles(log, o, b, false);
}

namespace {
ResourceProfilesResult compute_resource_profiles(const EventLog& log, const ResourceProfilesOptions& o,
                                                 const Bounds& b, bool frame) {
  ResourceProfilesResult result; result.metric = o.metric;
  result.diagnostics.trace_count = log.trace_count(); result.diagnostics.event_count = log.event_count();
  std::set<std::string> names;
  for (const auto& tr : log.traces) for (const auto& e : tr.events)
    if (auto resource = resource_of(e, o)) names.insert(std::move(*resource));
  if (names.size() > o.max_resources) throw std::invalid_argument("resource_profiles resource bound exceeded");
  result.diagnostics.resource_count = names.size();

  using Key = std::pair<std::string, std::string>;
  std::map<Key, std::uint64_t> freq;
  std::map<std::string, std::uint64_t> completions;
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
    const auto case_id = case_id_of(tr, tr.events.empty() ? Event{} : tr.events.front(), o);
    // The case runs from its first to its last event in the supplied order,
    // as in the reference; an unsorted trace can have a negative duration.
    std::optional<double> first, last;
    std::set<std::string> tr_resources;
    for (const auto& e : tr.events) {
      const auto resource = resource_of(e, o);
      const auto activity = activity_of(e, o).value_or(std::string{});
      const auto t = parse_timestamp(timestamp_of(e, o));
      if (t && std::isfinite(*t)) { if (!first) first = *t; last = *t; }
      if (resource) tr_resources.insert(*resource);
      // On the frame a case is in the slot whatever its events' resources.
      if (frame && !resource && in_slot(t, b)) slot_cases.insert(case_id);
      if (resource && in_slot(t, b)) {
        slot_cases.insert(case_id);
        slot_cases_by_resource[*resource].insert(case_id);
        ++resource_events[*resource];
        if (resource_matches(*resource, o) && activity_matches(activity, o)) {
          ++freq[{*resource, activity}];
          // On the frame a NaN activity is no value (nunique skips it).
          if (!frame || e.activity_state != FirstClassAttributeState::kMissing) acts[*resource].insert(activity);
        }
        // The reference activity_completions(log, t1, t2, r) has no activity
        // argument: it counts every event the resource completes in the slot.
        if (resource_matches(*resource, o)) ++completions[*resource];
      }
    }
    if (first && last) {
      case_start[case_id] = *first; case_end[case_id] = *last;
      // A case counts toward a window when either endpoint lies in the closed
      // window or the case spans all of it.
      const auto inside = [&](double t) { return (!b.lo || t >= *b.lo) && (!b.hi || t <= *b.hi); };
      case_intersects[case_id] =
          inside(*first) || inside(*last) || ((!b.lo || *first <= *b.lo) && (!b.hi || *last >= *b.hi));
    }
    case_resources[case_id].insert(tr_resources.begin(), tr_resources.end());
    if (last && in_slot(last, b)) {
      all_completed_cases.insert(case_id);
      for (const auto& rr : tr_resources) completed_cases[rr].insert(case_id);
    }
    auto ints = lifecycle_intervals(tr, o, o.metric == ResourceProfileMetric::kAverageDurationActivity ||
                                               o.metric == ResourceProfileMetric::kAverageWorkload ||
                                               o.metric == ResourceProfileMetric::kMultitasking);
    intervals.insert(intervals.end(), ints.begin(), ints.end());
  }

  switch (o.metric) {
    case ResourceProfileMetric::kDistinctActivities:
      for (const auto& [r,s] : acts) if (resource_matches(r, o)) add_row(result,{r,"","",static_cast<double>(s.size()),static_cast<std::uint64_t>(s.size())},o.max_rows); break;
    case ResourceProfileMetric::kActivityFrequency:
      for (const auto& [k,v] : freq) if (resource_matches(k.first, o) && activity_matches(k.second, o)) { const auto den = resource_events[k.first]; add_row(result,{k.first,k.second,"",den ? static_cast<double>(v)/den : 0.0,v},o.max_rows); } break;
    case ResourceProfileMetric::kActivityCompletions:
      for (const auto& [r,v] : completions) add_row(result,{r,"","",static_cast<double>(v),v},o.max_rows); break;
    case ResourceProfileMetric::kCaseCompletions:
      for (const auto& [r,s] : completed_cases) if (resource_matches(r, o)) add_row(result,{r,"","",static_cast<double>(s.size()),static_cast<std::uint64_t>(s.size())},o.max_rows); break;
    case ResourceProfileMetric::kFractionCaseCompletions: {
      const double den = static_cast<double>(all_completed_cases.size());
      for (const auto& [r,s] : completed_cases) if (resource_matches(r, o)) add_row(result,{r,"","",den == 0 ? 0.0 : static_cast<double>(s.size()) / den,static_cast<std::uint64_t>(s.size())},o.max_rows);
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
              intersects == case_intersects.end() || !intersects->second) {
            continue;
          }
          const double duration = end->second - start->second;
          for (const auto& resource : resources) {
            sums[resource] += duration;
            ++counts[resource];
          }
        }
        for (const auto& [resource, sum] : sums) {
          if (!resource_matches(resource, o)) continue;
          const auto count = counts[resource];
          add_row(result, {resource, "", "", count ? sum / count : 0.0, count}, o.max_rows);
        }
      }
      break;
    case ResourceProfileMetric::kAverageDurationActivity: {
      for (const auto& i : intervals) if (resource_matches(i.resource, o) && activity_matches(i.activity, o) && in_slot(i.end,b)) { activity_duration_sum[{i.resource,i.activity}] += i.end-i.start; ++activity_duration_n[{i.resource,i.activity}]; }
      for (const auto& [k,v] : activity_duration_sum) add_row(result,{k.first,k.second,"",v/activity_duration_n[k],activity_duration_n[k]},o.max_rows);
      break; }
    case ResourceProfileMetric::kAverageWorkload:
    case ResourceProfileMetric::kMultitasking: {
      std::map<std::string,std::vector<Interval>> by, all_by;
      for (const auto& i: intervals) {
        if (!resource_matches(i.resource, o)) continue;
        if (!activity_matches(i.activity, o)) continue;
        // The resource's intervals form an interval tree, which takes no
        // interval ending before it starts (observed black-box).
        if (i.start > i.end) throw std::invalid_argument("resource profiles: ValueError: Start must be less than end");
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
          // The reference sums each distinct (start, end, activity) interval
          // once, while its copies still count as overlapping it (observed
          // black-box in both the pandas and the EventLog variant).
          std::set<std::tuple<double, double, std::string>> summed;
          for (const auto& target : v) {
            const double d = std::max(0.0, target.end - target.start);
            if (d == 0.0) continue;
            if (!summed.emplace(target.start, target.end, target.activity).second) continue;
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
      if ((o.resource.empty() && !o.resource_given) || (o.resource2.empty() && !o.resource2_given)) break;
      std::uint64_t n = 0; for(const auto& [c,rs]:case_resources) { auto z=case_end.find(c); if(z!=case_end.end()&&in_slot(z->second,b)&&rs.count(o.resource)&&rs.count(o.resource2)) ++n; }
      add_row(result,{o.resource,"",o.resource2,static_cast<double>(n),n},o.max_rows); break; }
    case ResourceProfileMetric::kSocialPosition: {
      const double denominator = static_cast<double>(slot_cases.size());
      if (!o.resource.empty() || o.resource_given) {
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
  // A query for one resource with no observation: the reference averages
  // raise StatisticsError and every other metric returns zero.
  if (result.rows.empty() && (!o.resource.empty() || o.resource_given)) {
    if (o.metric == ResourceProfileMetric::kAverageDurationActivity ||
        o.metric == ResourceProfileMetric::kAverageCaseDuration)
      throw std::invalid_argument("resource profiles: mean requires at least one data point");
    if (o.metric != ResourceProfileMetric::kInteractionTwoResources &&
        o.metric != ResourceProfileMetric::kSocialPosition) {
      const bool by_activity = o.metric == ResourceProfileMetric::kActivityFrequency;
      add_row(result, {o.resource, by_activity ? o.activity : std::string{}, "", 0.0, 0}, o.max_rows);
    }
  }
  std::sort(result.rows.begin(), result.rows.end(), [](const auto&a,const auto&b){return std::tie(a.resource,a.activity,a.resource2)<std::tie(b.resource,b.activity,b.resource2);});
  result.diagnostics.computed = true; result.diagnostics.message = result.diagnostics.output_truncated ? "row safety bound applied" : (result.rows.empty()?"no matching data":"ok");
  return result;
}

// The keys a metric selects from the frame, in pm4py's `df[[...]]` order
// (observed black-box from its KeyError lists). average_case_duration reads
// its columns one at a time instead (see frame_resource_profiles).
std::vector<std::string> frame_selection(const ResourceProfilesOptions& o, const std::string& r,
                                         const std::string& a, const std::string& c,
                                         const std::string& t, const std::string& s) {
  const bool start = !o.start_timestamp_attribute.empty();
  switch (o.metric) {
    case ResourceProfileMetric::kDistinctActivities:
    case ResourceProfileMetric::kActivityFrequency: return {a, t, r};
    case ResourceProfileMetric::kActivityCompletions: return {t, r};
    case ResourceProfileMetric::kCaseCompletions:
    case ResourceProfileMetric::kFractionCaseCompletions:
    case ResourceProfileMetric::kSocialPosition: return {t, r, c};
    case ResourceProfileMetric::kInteractionTwoResources: return {t, r, c, a};
    case ResourceProfileMetric::kAverageDurationActivity:
      if (start) return {t, r, c, a, s};
      return {t, r, c, a};
    case ResourceProfileMetric::kAverageWorkload:
    case ResourceProfileMetric::kMultitasking:
      // With a start column the intervals need no case: pm4py then selects
      // no case column.
      if (start) return {t, r, a, s};
      return {t, r, c, a};
    case ResourceProfileMetric::kAverageCaseDuration: return {r, c, t};
  }
  return {};
}

// A frame column's cells as pm4py's timestamp arithmetic sees them: the
// Python type of its first present cell, or nullopt for a date column.
std::optional<std::string> non_date_type(const EventLog& frame, const std::string& key) {
  std::optional<std::string> first;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      const auto cell = frame_row_scalar(frame, trace, event, key);
      if (!cell) continue;
      if (cell->kind == python::Scalar::Kind::kDate) return std::nullopt;
      return python::type_name(*cell);
    }
  // An all-missing column is float NaN; an empty frame has no cell to fail.
  if (frame.event_count() == 0) return std::nullopt;
  return std::string("float");
}

std::string utc_text(const python::Scalar& date) {
  return format_timestamp_microseconds(static_cast<double>(date.date.seconds) +
                                       static_cast<double>(date.date.nanoseconds) / 1e9);
}

// average_case_duration on the frame: the cases (case:concept:name) with
// an event of the resource (an isin match, so a text argument matches text
// cells only), their rows grouped by the case column, each group running
// from its first to its last timestamp in frame order; the groups that meet
// the closed window [t1, t2] (an endpoint inside, or spanning it) average.
ResourceProfilesResult frame_average_case_duration(const EventLog& frame, const ResourceProfilesOptions& o,
                                                   const Bounds& b, const std::string& r,
                                                   const std::string& c, const std::string& t,
                                                   std::map<std::string, std::string>& resource_names) {
  struct Row {
    std::optional<std::string> resource, case_name, group;
    std::optional<python::Scalar> time;
  };
  std::vector<Row> rows;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      Row row;
      if (const auto cell = frame_row_scalar(frame, trace, event, r)) {
        row.resource = python::identity(*cell);
        resource_names.try_emplace(*row.resource, python::str(*cell));
      }
      if (const auto cell = frame_row_scalar(frame, trace, event, "case:concept:name"))
        row.case_name = python::identity(*cell);
      if (const auto cell = frame_row_scalar(frame, trace, event, c))
        row.group = python::identity(*cell);
      if (auto cell = frame_row_scalar(frame, trace, event, t);
          cell && cell->kind == python::Scalar::Kind::kDate)
        row.time = std::move(cell);
      rows.push_back(std::move(row));
    }
  const auto seconds = [](const python::Scalar& date) {
    return static_cast<double>(date.date.seconds) + static_cast<double>(date.date.nanoseconds) / 1e9;
  };
  const auto duration = [](const python::Scalar& from, const python::Scalar& to) {
    return static_cast<double>(to.date.seconds - from.date.seconds) +
           (static_cast<double>(to.date.nanoseconds) - static_cast<double>(from.date.nanoseconds)) / 1e9;
  };
  // Each case's rows and each resource's cases (first seen first), so one
  // resource costs the rows of its cases only.
  std::map<std::string, std::vector<std::size_t>> case_rows;
  std::map<std::string, std::vector<std::string>> resource_cases;
  std::set<std::pair<std::string, std::string>> resource_case_seen;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    if (!row.case_name) continue;
    case_rows[*row.case_name].push_back(i);
    if (row.resource && resource_case_seen.emplace(*row.resource, *row.case_name).second)
      resource_cases[*row.resource].push_back(*row.case_name);
  }
  const auto profile = [&](const std::string& key) -> std::optional<std::pair<double, std::uint64_t>> {
    const auto found = resource_cases.find(key);
    if (found == resource_cases.end()) return std::nullopt;
    // Each group's first and last row (frame order) among the cases' rows,
    // and its least and greatest timestamps.
    struct Span {
      std::size_t first, last, least, greatest;
    };
    std::map<std::string, Span> spans;
    for (const auto& case_name : found->second)
      for (const auto i : case_rows.at(case_name)) {
        const auto& row = rows[i];
        if (!row.group || !row.time) continue;
        auto [it, inserted] = spans.try_emplace(*row.group, Span{i, i, i, i});
        if (!inserted) {
          auto& span = it->second;
          span.first = std::min(span.first, i);
          span.last = std::max(span.last, i);
          if (seconds(*row.time) < seconds(*rows[span.least].time)) span.least = i;
          if (seconds(*row.time) > seconds(*rows[span.greatest].time)) span.greatest = i;
        }
      }
    std::vector<Span> ordered;
    for (const auto& [group, span] : spans) {
      (void)group;
      ordered.push_back(span);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Span& x, const Span& y) { return x.first < y.first; });
    double sum = 0.0;
    std::uint64_t count = 0;
    // pm4py's pandas variant keeps a group only when its least timestamp
    // lies before t2 and its greatest after t1, both strictly (observed
    // black-box: a case ending at t1 or starting at t2 is outside, so is a
    // one-instant case at either bound; round BC: by value, not by the
    // rows' order, in an unsorted XES case), unlike the closed window of
    // the EventLog one. The duration runs from the first row to the last.
    for (const auto& span : ordered) {
      const double least = seconds(*rows[span.least].time), greatest = seconds(*rows[span.greatest].time);
      if (!((!b.hi || least < *b.hi) && (!b.lo || greatest > *b.lo))) continue;
      sum += duration(*rows[span.first].time, *rows[span.last].time);
      ++count;
    }
    if (count == 0) return std::nullopt;
    return std::make_pair(sum / static_cast<double>(count), count);
  };
  ResourceProfilesResult result;
  result.metric = o.metric;
  std::set<std::string> keys;
  for (const auto& row : rows)
    if (row.resource) keys.insert(*row.resource);
  if (keys.size() > o.max_resources) throw std::invalid_argument("resource_profiles resource bound exceeded");
  result.diagnostics.resource_count = keys.size();
  result.diagnostics.computed = true;
  if (!o.resource.empty() || o.resource_given) {
    const auto value = profile(python::identity(python::text(o.resource)));
    if (!value) throw std::invalid_argument("resource profiles: mean requires at least one data point");
    add_row(result, {o.resource, "", "", value->first, value->second}, o.max_rows);
    return result;
  }
  for (const auto& key : keys)
    if (const auto value = profile(key)) add_row(result, {key, "", "", value->first, value->second}, o.max_rows);
  return result;
}

// average_duration_activity's durations, observed black-box: the selected
// rows (a case, the resource, the activity, the timestamp in the window)
// subtract their start column from the timestamp column, and pandas'
// datetime subtraction rules follow the start column's dtype. An int column
// always fails; a float or bool column fails once a row is selected; a text
// column subtracts its present cells (one alone fails as the whole array, more
// fail cell by cell); an object column fails as the whole array for one row,
// else cell by cell at the first cell that is no date. Without a start
// column the start of a row is the previous timestamp of its case, an object
// column of dates when no case has two rows with a date among them. A subtraction that leaves no
// datetime result (no row, or object cells) then has no .dt accessor.
// `resource` is the identity of the resource's cells (pm4py's call is per
// resource; the all-resources request checks each one on its own rows).
void require_duration_rows(const EventLog& frame, const ResourceProfilesOptions& o, const Bounds& b,
                           const std::optional<std::string>& resource, const std::string& r,
                           const std::string& a, const std::string& c, const std::string& t,
                           const std::string& s, bool uses_start) {
  const auto fail = [](const std::string& message) {
    throw std::invalid_argument("resource profiles: " + message);
  };
  const auto no_accessor = [&] { fail("AttributeError: Can only use .dt accessor with datetimelike values"); };
  const auto date_column = [&](const std::string& key) {
    bool any = false;
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events)
        if (const auto cell = frame_row_scalar(frame, trace, event, key)) {
          if (cell->kind != python::Scalar::Kind::kDate) return false;
          any = true;
        }
    return any;
  };
  const auto wanted = [&](const std::string& value, bool given, bool dates) -> std::optional<std::string> {
    if (value.empty() && !given) return std::nullopt;
    if (dates)
      if (const auto parsed = parse_timestamp_exact(value)) {
        python::Scalar date;
        date.kind = python::Scalar::Kind::kDate;
        date.date = *parsed;
        date.date.timezone_aware = true;
        date.date.offset_minutes = 0;
        return python::identity(date);
      }
    return python::identity(python::text(value));
  };
  const auto activity = wanted(o.activity, o.activity_given, date_column(a));
  std::vector<std::optional<python::Scalar>> starts;
  // A case's rows sort with their missing timestamps last, so its second
  // row follows a dated one when any of its timestamps is a date; a row
  // without a case still counts below (no groupby drops it; round BB,
  // observed black-box).
  std::map<std::string, std::pair<std::size_t, bool>> case_rows;
  bool repeated_case = false;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      if (const auto case_cell = frame_row_scalar(frame, trace, event, c)) {
        auto& [count, dated] = case_rows[python::identity(*case_cell)];
        ++count;
        if (const auto time = frame_row_scalar(frame, trace, event, t); time && time->kind == python::Scalar::Kind::kDate)
          dated = true;
        repeated_case = repeated_case || (count > 1 && dated);
      }
      const auto resource_cell = frame_row_scalar(frame, trace, event, r);
      if (!resource_cell || (resource && python::identity(*resource_cell) != *resource)) continue;
      const auto activity_cell = frame_row_scalar(frame, trace, event, a);
      if (!activity_cell || (activity && python::identity(*activity_cell) != *activity)) continue;
      const auto time = frame_row_scalar(frame, trace, event, t);
      if (!time || time->kind != python::Scalar::Kind::kDate) continue;
      if (!in_slot(static_cast<double>(time->date.seconds) + static_cast<double>(time->date.nanoseconds) / 1e9, b))
        continue;
      starts.push_back(uses_start ? frame_row_scalar(frame, trace, event, s)
                                  : std::optional<python::Scalar>{});
    }
  if (!uses_start) {
    if (!repeated_case && starts.size() != 1) no_accessor();
    return;
  }
  auto dtype = frame_column_type(frame, s);
  if (!dtype || *dtype == DataFrameColumnType::kDate) return;
  // read_csv keeps a bool column with a missing cell as objects.
  if (*dtype == DataFrameColumnType::kBool)
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events)
        if (!frame_row_scalar(frame, trace, event, s)) dtype = DataFrameColumnType::kObject;
  const std::string integers =
      "TypeError: Addition/subtraction of integers and integer-arrays with ";
  const std::string integers_tail =
      " is no longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`";
  const auto unsupported = [&](const std::string& left, const std::string& right) {
    fail("TypeError: unsupported operand type(s) for -: '" + left + "' and '" + right + "'");
  };
  if (*dtype == DataFrameColumnType::kInt64) fail(integers + "DatetimeArray" + integers_tail);
  if (*dtype == DataFrameColumnType::kFloat64 || *dtype == DataFrameColumnType::kBool) {
    if (!starts.empty()) unsupported("Timestamp", *dtype == DataFrameColumnType::kBool ? "bool" : "float");
    no_accessor();
  }
  if (*dtype == DataFrameColumnType::kString) {
    const auto present = std::count_if(starts.begin(), starts.end(), [](const auto& cell) { return cell.has_value(); });
    if (present == 1) unsupported("DatetimeArray", "str");
    if (present > 1) unsupported("Timestamp", "str");
    no_accessor();
  }
  const auto failure = [&](const std::optional<python::Scalar>& cell, const std::string& array) {
    if (cell && cell->kind == python::Scalar::Kind::kDate) return;
    if (cell && cell->kind == python::Scalar::Kind::kInt) fail(integers + array + integers_tail);
    unsupported(array, cell ? python::type_name(*cell) : "float");
  };
  if (starts.size() == 1) {
    failure(starts.front(), "DatetimeArray");
    return;
  }
  for (const auto& cell : starts) failure(cell, "Timestamp");
  no_accessor();
}

void require_duration_column(const EventLog& frame, const ResourceProfilesOptions& o, const Bounds& b,
                             const std::string& r, const std::string& a, const std::string& c,
                             const std::string& t, const std::string& s, bool uses_start) {
  if (!o.resource.empty() || o.resource_given) {
    bool dates = false, other = false;
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events)
        if (const auto cell = frame_row_scalar(frame, trace, event, r))
          (cell->kind == python::Scalar::Kind::kDate ? dates : other) = true;
    std::optional<std::string> resource = python::identity(python::text(o.resource));
    if (dates && !other)
      if (const auto parsed = parse_timestamp_exact(o.resource)) {
        python::Scalar date;
        date.kind = python::Scalar::Kind::kDate;
        date.date = *parsed;
        date.date.timezone_aware = true;
        date.date.offset_minutes = 0;
        resource = python::identity(date);
      }
    return require_duration_rows(frame, o, b, resource, r, a, c, t, s, uses_start);
  }
  // Every resource of the column (seed 324 of round AY: two resources with
  // one row each pass alone, though their two rows together would not).
  std::set<std::string> resources;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events)
      if (const auto cell = frame_row_scalar(frame, trace, event, r))
        resources.insert(python::identity(*cell));
  for (const auto& resource : resources) require_duration_rows(frame, o, b, resource, r, a, c, t, s, uses_start);
}

// interaction_two_resources on the frame, observed black-box: the selection
// [t, r, c, a] holds the resource column once (one column named four times
// masks instead of realigning) and needs case:concept:name; the rows of the
// cases (case:concept:name) with an event of the first resource, then of
// those with the second (isin: text matches text cells only), group by the
// case column (one column; the groupby sorts its keys); each group's last
// present timestamp is compared with t1 in group order, a timestamp column
// named twice then cannot realign, and the groups whose last timestamp lies
// in the window count.
std::uint64_t frame_interaction_count(const EventLog& frame, const ResourceProfilesOptions& o, const Bounds& b,
                                      const std::vector<std::string>& selection, const std::string& r,
                                      const std::string& c, const std::string& t) {
  const auto fail = [](const std::string& message) {
    throw std::invalid_argument("resource profiles: " + message);
  };
  const auto named = [&](const std::string& key) {
    return static_cast<std::size_t>(std::count(selection.begin(), selection.end(), key));
  };
  if (named(r) > 1 && named(r) < selection.size()) fail("ValueError: cannot reindex on an axis with duplicate labels");
  if (named("case:concept:name") == 0) fail("KeyError: \"None of ['case:concept:name'] are in the columns\"");
  struct Row {
    const Trace* trace;
    const Event* event;
    std::string case_key;
  };
  std::vector<Row> rows;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      const auto cell = frame_row_scalar(frame, trace, event, "case:concept:name");
      rows.push_back({&trace, &event, cell ? python::identity(*cell) : std::string("\x01missing")});
    }
  const auto keep_cases_of = [&](const std::string& resource) {
    const auto wanted = python::identity(python::text(resource));
    std::set<std::string> cases;
    for (const auto& row : rows)
      if (const auto cell = frame_row_scalar(frame, *row.trace, *row.event, r);
          cell && python::identity(*cell) == wanted)
        cases.insert(row.case_key);
    std::erase_if(rows, [&](const Row& row) { return !cases.contains(row.case_key); });
  };
  keep_cases_of(o.resource);
  keep_cases_of(o.resource2);
  if (named(c) > 1) fail("ValueError: Grouper for '" + c + "' not 1-dimensional");
  std::vector<std::vector<python::Scalar>> keys;
  std::vector<std::size_t> keyed;
  for (std::size_t i = 0; i < rows.size(); ++i)
    if (auto cell = frame_row_scalar(frame, *rows[i].trace, *rows[i].event, c)) {
      keys.push_back({std::move(*cell)});
      keyed.push_back(i);
    }
  std::vector<python::Group> groups;
  try {
    groups = python::groupby(keys);
  } catch (const python::TypeError& error) {
    fail(std::string("TypeError: ") + error.what());
  }
  std::vector<std::optional<python::Scalar>> lasts;
  for (const auto& group : groups) {
    std::optional<python::Scalar> last;
    for (const auto member : group.rows)
      if (auto cell = frame_row_scalar(frame, *rows[keyed[member]].trace, *rows[keyed[member]].event, t);
          cell && !python::is_missing(*cell))
        last = std::move(*cell);
    // A float64 column's group of NaN cells is NaN, which numpy compares.
    if (!last && frame_column_type(frame, t) == DataFrameColumnType::kFloat64) last = python::real(NAN);
    if (last && last->kind != python::Scalar::Kind::kDate)
      fail("TypeError: '>=' not supported between instances of '" + python::type_name(*last) +
           "' and 'datetime.datetime'");
    lasts.push_back(std::move(last));
  }
  if (named(t) > 1) fail("ValueError: cannot reindex on an axis with duplicate labels");
  std::uint64_t count = 0;
  for (const auto& last : lasts)
    if (last && in_slot(static_cast<double>(last->date.seconds) + static_cast<double>(last->date.nanoseconds) / 1e9, b))
      ++count;
  return count;
}

// pm4py's resource_profiles functions on a DataFrame (CSV and XES uploads):
// keys are the frame's columns (no org:resource alias, a missing column
// raises KeyError in the metric's selection order), cells are typed (the
// resource and activity arguments are text, so they match text cells only;
// rows for every resource are named by str() of the typed value), dates are
// UTC, and cases are the groups of the case column. The metrics themselves
// are the native ones over that view.
ResourceProfilesResult frame_resource_profiles(const EventLog& input, const ResourceProfilesOptions& o,
                                               const Bounds& b) {
  const auto frame = dataframe_frame_event_log(input);
  const auto keys = frame_keys(frame, FrameKeyScope::kDataFrame);
  const auto or_default = [](const std::string& key, const char* fallback) {
    return key.empty() ? std::string(fallback) : key;
  };
  const auto r = or_default(o.resource_attribute, "org:resource");
  const auto a = or_default(o.activity_attribute, "concept:name");
  const auto c = or_default(o.case_id_attribute, "case:concept:name");
  const auto t = or_default(o.timestamp_attribute, "time:timestamp");
  const bool start = !o.start_timestamp_attribute.empty();
  const auto s = start ? o.start_timestamp_attribute : t;
  const auto fail = [](const std::string& message) {
    throw std::invalid_argument("resource profiles: " + message);
  };
  const auto selection = frame_selection(o, r, a, c, t, s);
  const auto metric = o.metric;
  if (metric == ResourceProfileMetric::kAverageCaseDuration) {
    if (!keys.contains(r)) fail("KeyError: '" + r + "'");
    if (!keys.contains(c)) fail("KeyError: '" + c + "'");
    if (!keys.contains(t)) fail("KeyError: 'Column not found: " + t + "'");
  } else {
    try {
      require_frame_columns(keys, selection);
    } catch (const std::invalid_argument& error) {
      fail(error.what());
    }
  }
  const bool workload = metric == ResourceProfileMetric::kAverageWorkload ||
                        metric == ResourceProfileMetric::kMultitasking;
  // Most metrics keep the rows of the resource before they compare times,
  // so a timestamp column of no dates fails only when the resource occurs
  // (interaction_two_resources: in a case with the second resource).
  const auto text_key = [](const std::string& value) { return python::identity(python::text(value)); };
  const auto date_column = [&](const std::string& key) {
    bool any = false;
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events)
        if (const auto cell = frame_row_scalar(frame, trace, event, key)) {
          if (cell->kind != python::Scalar::Kind::kDate) return false;
          any = true;
        }
    return any;
  };
  // `column == value` on a datetime column compares the argument as a
  // Timestamp (naive text as UTC); any other column compares it as text.
  const auto query_key = [&](const std::string& value, bool dates) {
    if (dates)
      if (const auto parsed = parse_timestamp_exact(value)) {
        python::Scalar date;
        date.kind = python::Scalar::Kind::kDate;
        date.date = *parsed;
        date.date.timezone_aware = true;
        date.date.offset_minutes = 0;
        return python::identity(date);
      }
    return text_key(value);
  };
  // social_position, interaction_two_resources and average_case_duration
  // match the resource with isin, which compares text as text.
  const bool resource_dates = metric != ResourceProfileMetric::kSocialPosition &&
                              metric != ResourceProfileMetric::kInteractionTwoResources &&
                              metric != ResourceProfileMetric::kAverageCaseDuration && date_column(r);
  const bool all_resources = o.resource.empty() && !o.resource_given;
  // The rows a metric compares times on: most metrics keep the rows of the
  // resource first (interaction_two_resources: the cases with both
  // resources), the case metrics compare every row. A missing cell never
  // fails a comparison.
  const auto wanted = query_key(o.resource, resource_dates);
  const bool resource_first = metric != ResourceProfileMetric::kCaseCompletions &&
                              metric != ResourceProfileMetric::kFractionCaseCompletions &&
                              metric != ResourceProfileMetric::kSocialPosition;
  const auto activity_wanted = o.activity.empty() && !o.activity_given
                                   ? std::optional<std::string>{}
                                   : std::optional<std::string>{query_key(o.activity, date_column(a))};
  // The frame's rows, in the order the timestamp comparisons meet them:
  // workload and average_duration_activity without a start column first
  // sort the frame by the case and timestamp columns (see below).
  std::vector<std::pair<const Trace*, const Event*>> compared_rows;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) compared_rows.emplace_back(&trace, &event);
  // A float64 column compares NaN cells too (numpy, not the masked object
  // comparison), so a compared row of it always fails.
  // The case metrics compare the rows of a case only: the groupby of the
  // case column drops a row without one (round BB, observed black-box).
  const bool case_metric = metric == ResourceProfileMetric::kCaseCompletions ||
                           metric == ResourceProfileMetric::kFractionCaseCompletions;
  const auto offending_type = [&](const std::string& key) -> std::optional<std::string> {
    const bool floats = frame_column_type(frame, key) == DataFrameColumnType::kFloat64;
    for (const auto& [trace, event] : compared_rows) {
      if (case_metric && !frame_row_scalar(frame, *trace, *event, c)) continue;
      if (resource_first) {
        const auto cell = frame_row_scalar(frame, *trace, *event, r);
        if (!cell) continue;
        if (!all_resources && python::identity(*cell) != wanted) continue;
        // average_duration_activity keeps the rows of the activity too.
        if (metric == ResourceProfileMetric::kAverageDurationActivity && activity_wanted) {
          const auto activity = frame_row_scalar(frame, *trace, *event, a);
          if (!activity || python::identity(*activity) != *activity_wanted) continue;
        }
      }
      const auto value = frame_row_scalar(frame, *trace, *event, key);
      if (!value && floats) return std::string("float");
      if (value && value->kind != python::Scalar::Kind::kDate) return python::type_name(*value);
    }
    return std::nullopt;
  };
  // groupby over the case column: its sort's TypeError names an XES object
  // column's dates datetime.datetime (round BC, observed black-box).
  const auto case_groupby = [&](const std::vector<std::vector<python::Scalar>>& keys) -> std::vector<python::Group> {
    try {
      return python::groupby(keys);
    } catch (const python::TypeError& error) {
      fail("TypeError: " + column_type_error(error.what(), frame_column_type(frame, c)));
    }
    return {};
  };
  // A missing cell as pm4py's timestamp conversion meets it: None in the
  // case and activity columns of a CSV frame (format_dataframe makes them
  // the "string" dtype, whose missing value is NA), NaN elsewhere (round
  // BB, observed black-box).
  const auto missing_name = [&](const std::string& key) {
    const bool na_string = frame.dataframe_columns && frame.dataframe_columns->csv_text &&
                           (key == "case:concept:name" || key == "concept:name");
    return std::string(na_string ? "NoneType" : "float");
  };
  // The workload metrics convert each row of the resource to an interval,
  // its start and then its end with .timestamp(), before they compare
  // anything with the window: the first cell that is no date fails, with
  // its own type (a missing date is NaT). Without a start column the start
  // is the previous row's timestamp in the sorted frame when that row is of
  // the same case (a missing one, or the case's first row, takes the row's
  // own timestamp); shifting makes an int64 column floats (round BB,
  // observed black-box). The shift is per case, and pandas types the
  // shifted column by the cells it holds: on an object column (XES) whose
  // shifted cells are all ints and floats (no bool, text or date), it is
  // float64, so a previous int cell becomes a float; a case's first row
  // keeps its own cell's type (round BD, observed black-box).
  const auto same_case = [&](std::size_t i) {
    if (i == 0) return false;
    const auto case_cell = frame_row_scalar(frame, *compared_rows[i].first, *compared_rows[i].second, c);
    const auto previous_case =
        frame_row_scalar(frame, *compared_rows[i - 1].first, *compared_rows[i - 1].second, c);
    return case_cell && previous_case && python::identity(*case_cell) == python::identity(*previous_case);
  };
  const auto workload_conversion_error = [&]() -> std::optional<std::string> {
    const auto t_dtype = frame_column_type(frame, t);
    bool shifted_numbers = t_dtype == DataFrameColumnType::kObject;
    if (!start && shifted_numbers)
      for (std::size_t i = 1; i < compared_rows.size() && shifted_numbers; ++i)
        if (same_case(i))
          if (const auto previous = frame_row_scalar(frame, *compared_rows[i - 1].first, *compared_rows[i - 1].second, t))
            shifted_numbers = python::is_number(*previous) && previous->kind != python::Scalar::Kind::kBool;
    const auto convert = [&](const std::optional<python::Scalar>& cell, const std::string& key,
                             bool shifted, bool previous = false) -> std::optional<std::string> {
      if (!cell) {
        if (frame_column_type(frame, key) == DataFrameColumnType::kDate)
          return std::string("ValueError: NaTType does not support timestamp");
        return "AttributeError: '" + missing_name(key) + "' object has no attribute 'timestamp'";
      }
      if (cell->kind == python::Scalar::Kind::kDate) return std::nullopt;
      const bool floated = shifted && (t_dtype == DataFrameColumnType::kInt64 ||
                                       (previous && shifted_numbers && cell->kind == python::Scalar::Kind::kInt));
      const auto type = floated ? std::string("float") : python::type_name(*cell);
      return "AttributeError: '" + type + "' object has no attribute 'timestamp'";
    };
    const auto of_resource = [&](std::size_t i) {
      const auto cell = frame_row_scalar(frame, *compared_rows[i].first, *compared_rows[i].second, r);
      return cell && (all_resources || python::identity(*cell) == wanted);
    };
    if (start) {
      // With a start column the rows keep the frame's order.
      for (std::size_t i = 0; i < compared_rows.size(); ++i) {
        if (!of_resource(i)) continue;
        const auto& [trace, event] = compared_rows[i];
        if (const auto error = convert(frame_row_scalar(frame, *trace, *event, s), s, false)) return error;
        if (const auto error = convert(frame_row_scalar(frame, *trace, *event, t), t, false)) return error;
      }
      return std::nullopt;
    }
    // Without one the frame is sorted again, by the start and then the
    // timestamp column, each an ordered Categorical (numbers before text,
    // missing cells last), before the intervals are converted (round BD,
    // observed black-box with cells that log their conversion).
    std::vector<std::optional<python::Scalar>> begins(compared_rows.size()), ends(compared_rows.size());
    std::vector<bool> previous_cells(compared_rows.size(), false);
    for (std::size_t i = 0; i < compared_rows.size(); ++i) {
      ends[i] = frame_row_scalar(frame, *compared_rows[i].first, *compared_rows[i].second, t);
      begins[i] = ends[i];
      if (same_case(i))
        if (auto previous = frame_row_scalar(frame, *compared_rows[i - 1].first, *compared_rows[i - 1].second, t)) {
          begins[i] = std::move(previous);
          previous_cells[i] = true;
        }
    }
    const auto value_codes = [&](const std::vector<std::optional<python::Scalar>>& values) {
      std::vector<std::vector<python::Scalar>> keys;
      std::vector<std::size_t> keyed;
      for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] && !python::is_missing(*values[i])) {
          keys.push_back({*values[i]});
          keyed.push_back(i);
        }
      std::vector<python::Group> groups;
      try {
        groups = python::groupby(keys);
      } catch (const python::TypeError&) {
        fail("TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
             "categories argument.");
      }
      std::vector<std::size_t> code(values.size(), groups.size());
      for (std::size_t g = 0; g < groups.size(); ++g)
        for (const auto member : groups[g].rows) code[keyed[member]] = g;
      return code;
    };
    const auto begin_codes = value_codes(begins);
    const auto end_codes = value_codes(ends);
    std::vector<std::size_t> order(compared_rows.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
      return std::tie(begin_codes[x], end_codes[x]) < std::tie(begin_codes[y], end_codes[y]);
    });
    for (const auto i : order) {
      if (!of_resource(i)) continue;
      if (const auto error = convert(begins[i], t, true, previous_cells[i])) return error;
      if (const auto error = convert(ends[i], t, false)) return error;
    }
    return std::nullopt;
  };
  // A column named twice in the selection: pandas cannot realign the
  // selected frame (workload with its start column on the timestamp column
  // is the one combination that runs). Most metrics fail so before they
  // compare a timestamp; social_position and interaction_two_resources
  // compare the timestamps first, and social_position with every selected
  // column the same one masks instead of realigning.
  const bool social = metric == ResourceProfileMetric::kSocialPosition;
  const bool interaction = metric == ResourceProfileMetric::kInteractionTwoResources;
  std::optional<std::uint64_t> interaction_count;
  if (interaction && (!o.resource.empty() || o.resource_given) && (!o.resource2.empty() || o.resource2_given))
    interaction_count = frame_interaction_count(frame, o, b, selection, r, c, t);
  const auto named_in_selection = [&](const std::string& key) { return std::count(selection.begin(), selection.end(), key); };
  const bool one_column =
      std::all_of(selection.begin(), selection.end(), [&](const auto& key) { return key == selection.front(); });
  const bool timed = workload || metric == ResourceProfileMetric::kAverageDurationActivity;
  const auto reindex = [&] { fail("ValueError: cannot reindex on an axis with duplicate labels"); };
  // The workload metrics and average_duration_activity (observed
  // black-box): without a start column the frame is first sorted by the
  // case and timestamp columns, which must be unique; the masks that keep
  // the resource's (then the activity's) rows cannot realign a frame with
  // that column named twice, unless every selected column is the one, and
  // with a start column the timestamp and start cells are read row by row
  // (average_duration_activity masks on its timestamp column too, after it
  // compared it).
  const auto require_timed_selection = [&](bool masks) {
    if (!start && !masks) {
      if (named_in_selection(c) > 1) fail("ValueError: The column label '" + c + "' is not unique.");
      if (named_in_selection(t) > 1) fail("ValueError: The column label '" + t + "' is not unique.");
      return;
    }
    if (one_column) return;
    if (named_in_selection(r) > 1) reindex();
    if (metric == ResourceProfileMetric::kAverageDurationActivity && named_in_selection(a) > 1) reindex();
  };
  // activity_frequency with the activity and resource on the timestamp
  // column selects that one column three times: its masks keep every row (a
  // non-matching cell is NaN), as activity_completions does with the
  // resource on the timestamp column, so the frame's length is both the
  // numerator and the denominator (round BA).
  const bool masked_frequency = metric == ResourceProfileMetric::kActivityFrequency && one_column;
  // distinct_activities masks so too; nunique skips the NaN cells, so each
  // copy of the column counts the resource's one value when a row of it
  // lies in the window (pm4py returns that count three times, as a Series).
  const bool masked_distinct = metric == ResourceProfileMetric::kDistinctActivities && one_column;
  const auto require_unique_selection = [&] {
    if (timed) return require_timed_selection(false);
    // The case metrics group by the case column before they realign or
    // compare anything: the case column on the timestamp column is two.
    if ((metric == ResourceProfileMetric::kCaseCompletions ||
         metric == ResourceProfileMetric::kFractionCaseCompletions) &&
        c == t)
      fail("ValueError: Grouper for '" + c + "' not 1-dimensional");
    if (metric == ResourceProfileMetric::kSocialPosition && one_column) return;
    if (masked_frequency || masked_distinct) return;
    if (metric == ResourceProfileMetric::kAverageCaseDuration || metric == ResourceProfileMetric::kActivityCompletions)
      return;
    for (const auto& key : selection)
      if (named_in_selection(key) > 1) reindex();
  };
  // distinct_activities and activity_frequency keep the rows of the
  // resource (named once) before they compare times, so a timestamp column
  // named twice fails that comparison first; only a comparison that passes
  // (no row of the resource holds a non-date) reaches the realignment.
  if ((metric == ResourceProfileMetric::kDistinctActivities ||
       metric == ResourceProfileMetric::kActivityFrequency) &&
      std::count(selection.begin(), selection.end(), r) == 1 &&
      std::count(selection.begin(), selection.end(), t) > 1)
    if (const auto t_type = offending_type(t))
      fail("TypeError: '>=' not supported between instances of '" + *t_type + "' and 'datetime.datetime'");
  if (!social && !interaction) require_unique_selection();
  if (!start && (workload || metric == ResourceProfileMetric::kAverageDurationActivity)) {
    // The start of each row is the previous event of its case: the frame is
    // first sorted by the case and timestamp columns, each made an ordered
    // Categorical (whose categories need values pandas can sort; missing
    // cells last), so a sort that compares a Timestamp with a number fails.
    const auto codes = [&](const std::string& key) {
      std::vector<std::vector<python::Scalar>> keys;
      std::vector<std::size_t> keyed;
      for (std::size_t i = 0; i < compared_rows.size(); ++i)
        if (auto cell = frame_row_scalar(frame, *compared_rows[i].first, *compared_rows[i].second, key);
            cell && !python::is_missing(*cell)) {
          keys.push_back({std::move(*cell)});
          keyed.push_back(i);
        }
      std::vector<python::Group> groups;
      try {
        groups = python::groupby(keys);
      } catch (const python::TypeError&) {
        fail("TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
             "categories argument.");
      }
      std::vector<std::size_t> code(compared_rows.size(), groups.size());
      for (std::size_t g = 0; g < groups.size(); ++g)
        for (const auto member : groups[g].rows) code[keyed[member]] = g;
      return code;
    };
    const auto case_codes = codes(c);
    const auto time_codes = codes(t);
    std::vector<std::size_t> order(compared_rows.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
      return std::tie(case_codes[x], time_codes[x]) < std::tie(case_codes[y], time_codes[y]);
    });
    std::vector<std::pair<const Trace*, const Event*>> sorted;
    for (const auto i : order) sorted.push_back(compared_rows[i]);
    compared_rows = std::move(sorted);
  }
  if (timed && (!start || one_column)) {
    if (!start) require_timed_selection(true);
    // Every selected column the one: the masks keep every row, the
    // others' cells missing.
    else if (metric == ResourceProfileMetric::kAverageDurationActivity)
      fail("AttributeError: 'DataFrame' object has no attribute 'dt'");
    else if (!frame.traces.empty() && !frame.traces.front().events.empty()) {
      const auto& trace = frame.traces.front();
      const auto cell = frame_row_scalar(frame, trace, trace.events.front(), r);
      const bool kept = cell && (all_resources || python::identity(*cell) == wanted);
      if (frame_column_type(frame, r) == DataFrameColumnType::kDate) {
        if (!kept) fail("ValueError: NaTType does not support timestamp");
      } else {
        fail("AttributeError: '" + (kept ? python::type_name(*cell) : missing_name(r)) +
             "' object has no attribute 'timestamp'");
      }
    }
  }
  if (metric == ResourceProfileMetric::kAverageCaseDuration) {
    // The rows of the resource's cases (case:concept:name) compare their
    // timestamps first; the groupby of the case column then needs one
    // column (the case column on the timestamp column is two), and the
    // spans use the .dt accessor, which needs a datetime column.
    std::set<std::string> cases;
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events) {
        const auto cell = frame_row_scalar(frame, trace, event, r);
        const auto case_name = frame_row_scalar(frame, trace, event, "case:concept:name");
        if (cell && case_name && (all_resources || python::identity(*cell) == text_key(o.resource)))
          cases.insert(python::identity(*case_name));
      }
    // Their rows group by the case column (which sorts its values; a row
    // without one is dropped), and each group's least and greatest
    // timestamps are compared with the window: the least first, the groups
    // in the groupby's order. An object column's group of dates beside text
    // or numbers has no minimum (round BC, observed black-box).
    std::vector<std::vector<python::Scalar>> keys;
    std::vector<std::pair<const Trace*, const Event*>> keyed;
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events) {
        const auto case_name = frame_row_scalar(frame, trace, event, "case:concept:name");
        if (!case_name || !cases.contains(python::identity(*case_name))) continue;
        if (auto cell = frame_row_scalar(frame, trace, event, c)) {
          keys.push_back({std::move(*cell)});
          keyed.emplace_back(&trace, &event);
        }
      }
    const auto groups = case_groupby(keys);
    std::vector<std::optional<python::Scalar>> minima;
    for (const auto& group : groups) {
      std::optional<python::Scalar> least;
      std::set<int> classes;
      for (const auto member : group.rows) {
        auto value = frame_row_scalar(frame, *keyed[member].first, *keyed[member].second, t);
        if (!value) continue;
        classes.insert(python::is_number(*value) ? 0 : value->kind == python::Scalar::Kind::kText ? 1 : 2);
        if (classes.size() > 1) fail("TypeError: agg function failed [how->min,dtype->object]");
        if (!least || python::less(*value, *least)) least = std::move(value);
      }
      minima.push_back(std::move(least));
    }
    // A float64 column compares its NaN minima too (numpy).
    if (frame_column_type(frame, t) == DataFrameColumnType::kFloat64 && !groups.empty())
      fail("TypeError: '<' not supported between instances of 'float' and 'datetime.datetime'");
    for (const auto& least : minima)
      if (least && least->kind != python::Scalar::Kind::kDate)
        fail("TypeError: '<' not supported between instances of '" + python::type_name(*least) +
             "' and 'datetime.datetime'");
    if (c == t) fail("ValueError: Grouper for '" + t + "' not 1-dimensional");
    // The cases' frame then reads the start column: any column but the
    // timestamp one is a KeyError there (round BA: after the comparison and
    // the groupby, observed black-box).
    if (start && s != t) fail("KeyError: '" + s + "'");
    // The cases' last less first timestamps: numpy refuses to subtract a
    // bool column (even with no case kept); other columns of no dates leave
    // no .dt accessor (round BB, observed black-box).
    if (frame_column_type(frame, t) == DataFrameColumnType::kBool)
      fail("TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` "
           "operator, or the logical_xor function instead.");
    // Durations of an object column (dates in it, round BC) leave no .dt
    // accessor either.
    if (frame_column_type(frame, t) != DataFrameColumnType::kDate)
      fail("AttributeError: Can only use .dt accessor with datetimelike values");
  } else if ((metric == ResourceProfileMetric::kActivityCompletions && r == t) || masked_frequency ||
             masked_distinct) {
    // The resource on the timestamp column: the masks keep every row (a
    // non-matching cell is NaN), and the frame's timestamp comparison then
    // raises for a typed (numeric or bool) column of no dates, or for an
    // object or text column at a matching cell that is no date.
    const auto dtype = frame_column_type(frame, t);
    if (dtype && *dtype != DataFrameColumnType::kDate && *dtype != DataFrameColumnType::kObject &&
        *dtype != DataFrameColumnType::kString) {
      // A masked int64 column is float64 unless every row matches, a
      // masked bool column object (its comparison skips the NaN cells, so
      // it fails only at a matching cell); numpy compares every cell of a
      // number column (round BB, observed black-box).
      bool any_kept = false, all_kept = true;
      for (const auto& [trace, event] : compared_rows) {
        const auto cell = frame_row_scalar(frame, *trace, *event, r);
        const bool kept = cell && (all_resources || python::identity(*cell) == wanted);
        any_kept = any_kept || kept;
        all_kept = all_kept && kept;
      }
      std::optional<std::string> t_type;
      if (*dtype == DataFrameColumnType::kBool) {
        if (any_kept) t_type = "bool";
      } else if (*dtype == DataFrameColumnType::kInt64) {
        if (!compared_rows.empty()) t_type = all_kept ? "int" : "float";
      } else {
        t_type = non_date_type(frame, t);
      }
      if (t_type)
        fail("TypeError: '>=' not supported between instances of '" + *t_type + "' and 'datetime.datetime'");
    } else if (const auto t_type = offending_type(t)) {
      fail("TypeError: '>=' not supported between instances of '" + *t_type + "' and 'datetime.datetime'");
    }
  } else if (interaction) {
  } else if (case_metric) {
    // groupby(case)[timestamp].last(): each case's last present cell is
    // compared with the window, the cases in the groupby's order (it sorts
    // the case values first); an earlier cell that is no date never fails
    // (round BC, observed black-box on XES object columns).
    std::vector<std::vector<python::Scalar>> keys;
    std::vector<std::size_t> keyed;
    for (std::size_t i = 0; i < compared_rows.size(); ++i)
      if (auto cell = frame_row_scalar(frame, *compared_rows[i].first, *compared_rows[i].second, c)) {
        keys.push_back({std::move(*cell)});
        keyed.push_back(i);
      }
    const auto groups = case_groupby(keys);
    const bool floats = frame_column_type(frame, t) == DataFrameColumnType::kFloat64;
    if (floats && !groups.empty()) fail("TypeError: '>=' not supported between instances of 'float' and 'datetime.datetime'");
    for (const auto& group : groups) {
      std::optional<python::Scalar> last;
      for (const auto member : group.rows)
        if (auto cell = frame_row_scalar(frame, *compared_rows[keyed[member]].first,
                                         *compared_rows[keyed[member]].second, t))
          last = std::move(cell);
      if (last && last->kind != python::Scalar::Kind::kDate)
        fail("TypeError: '>=' not supported between instances of '" + python::type_name(*last) +
             "' and 'datetime.datetime'");
    }
  } else if (workload) {
    if (const auto error = workload_conversion_error()) fail(*error);
  } else if (const auto t_type = offending_type(t)) {
    fail("TypeError: '>=' not supported between instances of '" + *t_type + "' and 'datetime.datetime'");
  } else if (metric == ResourceProfileMetric::kAverageDurationActivity && start && named_in_selection(t) > 1) {
    reindex();
  } else if (metric == ResourceProfileMetric::kAverageDurationActivity && start && named_in_selection(s) > 1) {
    // The start column named twice (as the case column) is a frame the
    // timestamp column is subtracted from: a datetime column realigns it
    // when rows of the resource and activity remain, and fails on NaT when
    // none do; any other column gives a frame with no .dt accessor.
    const auto t_dtype = frame_column_type(frame, t);
    if (t_dtype != DataFrameColumnType::kDate) {
      // A date start column is subtracted from the masked timestamp column
      // (round BB, observed black-box): a masked int or float column is
      // float64, a bool or object one object, a text column's missing
      // cells NaN (NA in format_dataframe's case and activity columns).
      if (frame_column_type(frame, s) == DataFrameColumnType::kDate) {
        if (t_dtype == DataFrameColumnType::kInt64 || t_dtype == DataFrameColumnType::kFloat64)
          fail("TypeError: cannot subtract DatetimeArray from ndarray[float64]");
        if (t_dtype == DataFrameColumnType::kString)
          fail("TypeError: unsupported operand type(s) for -: 'DatetimeArray' and '" +
               std::string(missing_name(t) == "NoneType" ? "NAType" : "float") + "'");
        fail("TypeError: cannot subtract DatetimeArray from ndarray[object]");
      }
      fail("AttributeError: 'DataFrame' object has no attribute 'dt'");
    }
    const bool kept = std::any_of(compared_rows.begin(), compared_rows.end(), [&](const auto& row) {
      const auto cell = frame_row_scalar(frame, *row.first, *row.second, r);
      if (!cell || (!all_resources && python::identity(*cell) != wanted)) return false;
      if (!activity_wanted) return true;
      const auto activity = frame_row_scalar(frame, *row.first, *row.second, a);
      return activity && python::identity(*activity) == *activity_wanted;
    });
    if (kept) reindex();
    // A date start column leaves a frame of no .dt accessor, any other the
    // subtraction of NaT from the column's own dtype (round BB, observed
    // black-box).
    const auto s_dtype = frame_column_type(frame, s);
    if (s_dtype == DataFrameColumnType::kDate) fail("AttributeError: 'DataFrame' object has no attribute 'dt'");
    const char* s_name = s_dtype == DataFrameColumnType::kInt64     ? "int64"
                         : s_dtype == DataFrameColumnType::kFloat64 ? "float64"
                         : s_dtype == DataFrameColumnType::kBool    ? "bool"
                                                                    : "object";
    fail(std::string("TypeError: Cannot subtract NaT from ndarray with dtype ") + s_name);
  } else if (metric == ResourceProfileMetric::kAverageDurationActivity &&
             frame_column_type(frame, t) != DataFrameColumnType::kDate) {
    // No compared row fails; a date start column cannot be subtracted from
    // the column, any other durations then use the .dt accessor. Two
    // object columns (XES, round BC, observed black-box) subtract cell by
    // cell over the rows of the resource and activity, where Python's first
    // unsupported operands raise.
    if (start && s != t && frame_column_type(frame, t) == DataFrameColumnType::kObject &&
        frame_column_type(frame, s) == DataFrameColumnType::kObject) {
      const auto python_type = [](const python::Scalar& value) {
        return value.kind == python::Scalar::Kind::kDate ? std::string("datetime.datetime") : python::type_name(value);
      };
      for (const auto& [trace, event] : compared_rows) {
        const auto cell = frame_row_scalar(frame, *trace, *event, r);
        if (!cell || (!all_resources && python::identity(*cell) != wanted)) continue;
        if (activity_wanted) {
          const auto activity = frame_row_scalar(frame, *trace, *event, a);
          if (!activity || python::identity(*activity) != *activity_wanted) continue;
        }
        const auto end = frame_row_scalar(frame, *trace, *event, t);
        const auto begin = frame_row_scalar(frame, *trace, *event, s);
        if (!end || !begin) continue;
        const bool dates = end->kind == python::Scalar::Kind::kDate && begin->kind == python::Scalar::Kind::kDate;
        if (!dates && !(python::is_number(*end) && python::is_number(*begin)))
          fail("TypeError: unsupported operand type(s) for -: '" + python_type(*end) + "' and '" + python_type(*begin) + "'");
      }
    }
    if (start && s != t && frame_column_type(frame, s) == DataFrameColumnType::kDate) {
      auto dtype = frame_column_type(frame, t);
      if (dtype == DataFrameColumnType::kBool)
        for (const auto& trace : frame.traces)
          for (const auto& event : trace.events)
            if (!frame_row_scalar(frame, trace, event, t)) dtype = DataFrameColumnType::kObject;
      const char* name = dtype == DataFrameColumnType::kInt64     ? "int64"
                         : dtype == DataFrameColumnType::kFloat64 ? "float64"
                         : dtype == DataFrameColumnType::kBool    ? "bool"
                                                                  : "object";
      fail(std::string("TypeError: cannot subtract DatetimeArray from ndarray[") + name + "]");
    }
    fail("AttributeError: Can only use .dt accessor with datetimelike values");
  }
  if (social) {
    require_unique_selection();
    // Both read case:concept:name from the selection.
    if (std::find(selection.begin(), selection.end(), "case:concept:name") == selection.end())
      fail("KeyError: \"None of ['case:concept:name'] are in the columns\"");
  }
  const bool uses_start = start && (workload || metric == ResourceProfileMetric::kAverageDurationActivity);
  if (metric == ResourceProfileMetric::kAverageDurationActivity) require_duration_column(frame, o, b, r, a, c, t, s, uses_start);
  // The metrics that read cases (workload and multitasking only without a
  // start column).
  const bool uses_case = metric == ResourceProfileMetric::kCaseCompletions ||
                         metric == ResourceProfileMetric::kFractionCaseCompletions ||
                         metric == ResourceProfileMetric::kSocialPosition ||
                         metric == ResourceProfileMetric::kInteractionTwoResources ||
                         metric == ResourceProfileMetric::kAverageDurationActivity ||
                         metric == ResourceProfileMetric::kAverageCaseDuration || (workload && !start);
  // The metrics that group by the case column sort its values: values of
  // two classes (text beside numbers, dates beside numbers) raise
  // TypeError. average_case_duration groups the cases of the resource only.
  if (uses_case && metric != ResourceProfileMetric::kInteractionTwoResources &&
      metric != ResourceProfileMetric::kSocialPosition) {
    std::set<std::string> resource_cases;
    if (metric == ResourceProfileMetric::kAverageCaseDuration)
      for (const auto& trace : frame.traces)
        for (const auto& event : trace.events) {
          const auto cell = frame_row_scalar(frame, trace, event, r);
          const auto case_name = frame_row_scalar(frame, trace, event, "case:concept:name");
          if (cell && case_name && (all_resources || python::identity(*cell) == text_key(o.resource)))
            resource_cases.insert(python::identity(*case_name));
        }
    std::vector<std::vector<python::Scalar>> case_keys;
    for (const auto& trace : frame.traces)
      for (const auto& event : trace.events) {
        if (metric == ResourceProfileMetric::kAverageCaseDuration) {
          const auto case_name = frame_row_scalar(frame, trace, event, "case:concept:name");
          if (!case_name || !resource_cases.contains(python::identity(*case_name))) continue;
        }
        if (auto cell = frame_row_scalar(frame, trace, event, c))
          case_keys.push_back({std::move(*cell)});
      }
    try {
      (void)python::groupby(case_keys);
    } catch (const python::TypeError& error) {
      fail(std::string("TypeError: ") + error.what());
    }
  }

  // The view: one trace per case group (pandas groupby drops a NaN case),
  // events in frame order; resources, activities and case ids keyed by
  // their Python identity.
  std::map<std::string, std::string> resource_names, activity_names;
  EventLog view;
  std::map<std::string, std::size_t> case_index;
  const std::string start_key = "\x01start";
  const bool intervals = workload || metric == ResourceProfileMetric::kAverageDurationActivity;
  std::size_t row_number = 0;
  for (const auto& trace : frame.traces) {
    if (!uses_case) view.traces.emplace_back();
    for (const auto& event : trace.events) {
      ++row_number;
      // average_duration_activity with a start column: a row whose start or
      // end is NaT has a NaN duration, which the mean skips (round BB,
      // observed black-box); the native intervals would take the previous
      // event's time.
      if (uses_start && metric == ResourceProfileMetric::kAverageDurationActivity) {
        const auto begin = frame_row_scalar(frame, trace, event, s);
        const auto end = frame_row_scalar(frame, trace, event, t);
        if (!begin || begin->kind != python::Scalar::Kind::kDate || !end ||
            end->kind != python::Scalar::Kind::kDate)
          continue;
      }
      std::string case_key;
      if (uses_case) {
        const auto cell = frame_row_scalar(frame, trace, event, c);
        if (!cell) {
          // The interval metrics keep a row with no case, its interval
          // starting at its own time; the other metrics' groupby drops it.
          if (!intervals) continue;
          case_key = "\x01row" + std::to_string(row_number);
        } else {
          case_key = python::identity(*cell);
        }
        const auto [it, inserted] = case_index.try_emplace(case_key, view.traces.size());
        if (inserted) {
          view.traces.emplace_back();
          view.traces.back().case_id = case_key;
        }
      }
      auto& target = uses_case ? view.traces[case_index[case_key]] : view.traces.back();
      Event out;
      if (const auto cell = frame_row_scalar(frame, trace, event, r)) {
        out.resource = python::identity(*cell);
        out.resource_state = FirstClassAttributeState::kPresent;
        resource_names.try_emplace(out.resource, python::str(*cell));
      }
      if (const auto cell = frame_row_scalar(frame, trace, event, a)) {
        out.activity = python::identity(*cell);
        out.activity_state = FirstClassAttributeState::kPresent;
        activity_names.try_emplace(out.activity, python::str(*cell));
      }
      if (const auto cell = frame_row_scalar(frame, trace, event, t);
          cell && cell->kind == python::Scalar::Kind::kDate)
        out.timestamp = utc_text(*cell);
      if (uses_start)
        if (const auto cell = frame_row_scalar(frame, trace, event, s);
            cell && cell->kind == python::Scalar::Kind::kDate)
          out.attributes.push_back(EventAttribute{start_key, utc_text(*cell)});
      target.events.push_back(std::move(out));
    }
  }
  ResourceProfilesOptions native = o;
  native.resource_attribute.clear();
  native.activity_attribute.clear();
  native.timestamp_attribute.clear();
  native.case_id_attribute.clear();
  native.start_timestamp_attribute = uses_start ? start_key : std::string{};
  if (!o.resource.empty() || o.resource_given) native.resource = query_key(o.resource, resource_dates);
  if (!o.activity.empty() || o.activity_given) native.activity = query_key(o.activity, date_column(a));
  if (!o.resource2.empty() || o.resource2_given) native.resource2 = query_key(o.resource2, resource_dates);
  ResourceProfilesResult result;
  if (metric == ResourceProfileMetric::kAverageCaseDuration) {
    result = frame_average_case_duration(frame, o, b, r, c, t, resource_names);
  } else {
    result = compute_resource_profiles(view, native, b, true);
  }
  if (interaction_count)
    for (auto& row : result.rows) {
      row.value = static_cast<double>(*interaction_count);
      row.count = *interaction_count;
    }
  // activity_completions with the resource on the timestamp column selects
  // that column twice; its masks then keep every row (len of the frame).
  // activity_frequency on one column three times: len / len.
  if ((metric == ResourceProfileMetric::kActivityCompletions && r == t) || masked_frequency) {
    const auto rows = static_cast<std::uint64_t>(frame.event_count());
    const double value = masked_frequency ? 1.0 : static_cast<double>(rows);
    const bool activity_given = !o.activity.empty() || o.activity_given;
    const auto activity_of = [&](const std::string& resource) {
      if (!masked_frequency) return std::string{};
      return activity_given ? native.activity : resource;
    };
    result.rows.clear();
    if (!all_resources) {
      add_row(result, {native.resource, activity_of(native.resource), "", value, rows}, o.max_rows);
    } else {
      for (const auto& [key, name] : resource_names) {
        (void)name;
        add_row(result, {key, activity_of(key), "", value, rows}, o.max_rows);
      }
    }
  }
  result.diagnostics.trace_count = input.trace_count();
  result.diagnostics.event_count = input.event_count();
  const auto rename = [](const std::map<std::string, std::string>& names, const std::string& key,
                         const std::string& query_key, const std::string& query) {
    if (key == query_key) return query;
    const auto it = names.find(key);
    return it == names.end() ? key : it->second;
  };
  std::vector<ResourceProfileRow> rows;
  std::set<std::tuple<std::string, std::string, std::string>> named;
  for (auto& row : result.rows) {
    if (metric == ResourceProfileMetric::kActivityFrequency && row.activity.empty()) continue;
    // average_case_duration names its one-resource row already.
    if (metric != ResourceProfileMetric::kAverageCaseDuration || all_resources)
      row.resource = rename(resource_names, row.resource, native.resource, o.resource);
    row.resource2 = rename(resource_names, row.resource2, native.resource2, o.resource2);
    if (!row.activity.empty()) row.activity = rename(activity_names, row.activity, native.activity, o.activity);
    // Two values with one str() (the int 7 and the text "7" in one object
    // column) keep the first row.
    if (named.emplace(row.resource, row.activity, row.resource2).second) rows.push_back(std::move(row));
  }
  std::sort(rows.begin(), rows.end(), [](const auto& x, const auto& y) {
    return std::tie(x.resource, x.activity, x.resource2) < std::tie(y.resource, y.activity, y.resource2);
  });
  result.rows = std::move(rows);
  result.diagnostics.message = result.diagnostics.output_truncated ? "row safety bound applied"
                               : (result.rows.empty() ? "no matching data" : "ok");
  return result;
}
}  // namespace

std::string resource_profiles_json(const ResourceProfilesResult& result) {
  std::set<std::tuple<std::string, std::string, std::string>> keys;
  std::set<std::string> resources;
  for (const auto& row : result.rows) {
    if (!std::isfinite(row.value) ||
        !keys.emplace(row.resource, row.activity, row.resource2).second)
      throw std::invalid_argument("resource profile row is invalid");
    // Query-specific zero rows can name resources absent from the source.
    // Their identities do not change the native source resource_count.
    const bool queried_zero = row.count == 0 && row.value == 0.0;
    if (!queried_zero) {
      resources.insert(row.resource);
      if (result.metric == ResourceProfileMetric::kInteractionTwoResources) resources.insert(row.resource2);
    }
  }
  if (result.diagnostics.resource_count < resources.size())
    throw std::invalid_argument("resource profile diagnostics are inconsistent");
  std::ostringstream out; out << "{\"family\":\"RESOURCE_PROFILES\",\"metric\":\"" << resource_profile_metric_name(result.metric) << "\",\"rows\":[";
  const auto quote = [&out](const std::string& s) { out << '"'; for (unsigned char c : s) { if (c == '\\' || c == '"') out << '\\'; if (c < 0x20) { out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << std::dec << std::setfill(' '); } else out << c; } out << '"'; };
  for(std::size_t i=0;i<result.rows.size();++i){const auto&r=result.rows[i]; if(i)out<<','; out<<"{\"resource\":"; quote(r.resource); out<<",\"activity\":"; quote(r.activity); out<<",\"resource2\":"; quote(r.resource2); out<<",\"value\":"<<std::setprecision(17)<<r.value<<",\"count\":"<<r.count<<"}";} out<<"],\"diagnostics\":{\"computed\":"<<(result.diagnostics.computed?"true":"false")<<",\"output_truncated\":"<<(result.diagnostics.output_truncated?"true":"false")<<",\"trace_count\":"<<result.diagnostics.trace_count<<",\"event_count\":"<<result.diagnostics.event_count<<",\"resource_count\":"<<result.diagnostics.resource_count<<",\"message\":"; quote(result.diagnostics.message); out << "}}"; return out.str();
}

}  // namespace minera
