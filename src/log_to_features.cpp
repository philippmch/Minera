#include "minera/log_to_features.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string value_for(const Event& e, const std::string& key) {
  if (key.empty()) return {};
  if (key == "activity" || key == "concept:name") return e.activity;
  if (key == "time:timestamp" || key == "timestamp") return e.timestamp;
  if (key == "org:resource" || key == "resource") return e.resource;
  if (const auto* v = e.attribute(key)) return *v;
  return {};
}

const EventAttribute* attr_for(const Event& e, const std::string& key) {
  for (const auto& a : e.attributes)
    if (a.key == key) return &a;
  return nullptr;
}
const EventAttribute* attr_for(const Trace& t, const std::string& key) {
  for (const auto& a : t.attributes)
    if (a.key == key) return &a;
  return nullptr;
}

std::string trace_value_for(const Trace& trace, const std::string& key) {
  if (key == "case:concept:name" || key == "case_id" ||
      key == "concept:name")
    return trace.case_id;
  if (const auto* value = attr_for(trace, key)) return value->value;
  return {};
}

std::string event_attribute_value(const Event& event, const std::string& key) {
  return value_for(event, key);
}

std::string trace_attribute_value(const Trace& trace, const std::string& key) {
  return trace_value_for(trace, key);
}

std::string categorical_value(std::string value) {
  return value.empty() ? "UNDEFINED" : std::move(value);
}

std::vector<std::string> distinct_event_values(const EventLog& log,
                                               const std::string& key) {
  std::set<std::string> values;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events)
      values.insert(categorical_value(event_attribute_value(event, key)));
  return {values.begin(), values.end()};
}

std::vector<std::string> distinct_trace_values(const EventLog& log,
                                               const std::string& key) {
  std::set<std::string> values;
  for (const auto& trace : log.traces)
    values.insert(categorical_value(trace_attribute_value(trace, key)));
  return {values.begin(), values.end()};
}

bool within_cardinality(const std::vector<std::string>& values,
                        const LogToFeaturesOptions& options) {
  return values.size() >= options.min_num_different_string_values &&
         values.size() <= options.max_num_different_string_values;
}

bool is_numeric_kind(AttributeKind k) {
  return k == AttributeKind::kInt || k == AttributeKind::kFloat ||
         k == AttributeKind::kBoolean;
}

double parse_number(const std::string& s, double fallback, bool* ok = nullptr) {
  if (s.empty()) {
    if (ok) *ok = false;
    return fallback;
  }
  double x = 0.0;
  try {
    std::size_t n = 0;
    x = std::stod(s, &n);
    if (n != s.size() || !std::isfinite(x)) throw std::invalid_argument("bad");
  } catch (...) {
    if (ok) *ok = false;
    return fallback;
  }
  if (ok) *ok = true;
  return x;
}

double checked_number(const std::string& s, double fallback) {
  if (s == "true" || s == "TRUE" || s == "True") return 1.0;
  if (s == "false" || s == "FALSE" || s == "False") return 0.0;
  bool ok = false;
  const double value = parse_number(s, fallback, &ok);
  if (!ok) throw std::invalid_argument("log_to_features: invalid or non-finite numeric attribute");
  return value;
}

double numeric_attribute_value(const Event& event, const std::string& key,
                               double fallback) {
  if (const auto* attribute = attr_for(event, key))
    return checked_number(attribute->value, fallback);
  const auto value = event_attribute_value(event, key);
  return value.empty() ? fallback : checked_number(value, fallback);
}

double numeric_trace_value(const Trace& trace, const std::string& key,
                           double fallback) {
  if (const auto* attribute = attr_for(trace, key))
    return checked_number(attribute->value, fallback);
  const auto value = trace_attribute_value(trace, key);
  return value.empty() ? fallback : checked_number(value, fallback);
}

void append_unique(std::vector<std::string>& out,
                   const std::vector<std::string>& in) {
  out.insert(out.end(), in.begin(), in.end());
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

void validate_names(const std::vector<std::string>& names, const char* label) {
  std::set<std::string> seen;
  for (const auto& n : names) {
    if (n.empty()) throw std::invalid_argument(std::string("log_to_features: ") + label + " contains empty name");
    if (!seen.insert(n).second) throw std::invalid_argument(std::string("log_to_features: duplicate ") + label);
  }
}

std::vector<std::string> categories_for_events(const EventLog& log,
                                                const std::string& key) {
  std::set<std::string> values;
  for (const auto& t : log.traces)
    for (const auto& e : t.events) values.insert(value_for(e, key));
  return {values.begin(), values.end()};
}

std::vector<std::string> categories_for_attr(const EventLog& log,
                                             const std::string& key,
                                             bool case_level) {
  std::set<std::string> values;
  for (const auto& t : log.traces) {
    if (case_level) {
      if (const auto* a = attr_for(t, key)) values.insert(a->value);
    } else {
      for (const auto& e : t.events)
        if (const auto* a = attr_for(e, key)) values.insert(a->value);
    }
  }
  return {values.begin(), values.end()};
}

std::vector<std::string> merged(const std::vector<std::string>& a,
                                const std::vector<std::string>& b) {
  std::vector<std::string> x = a;
  append_unique(x, b);
  return x;
}

struct TimedEvent {
  const Event* event = nullptr;
  double finish = 0.0;
  double start = 0.0;
  std::size_t trace = 0;
  std::size_t index = 0;
};

double instant(const Event& e, const std::string& key, bool required) {
  const auto text = value_for(e, key);
  if (text.empty()) {
    if (required) throw std::invalid_argument("log_to_features: missing timestamp");
    return std::numeric_limits<double>::quiet_NaN();
  }
  const auto parsed = parse_timestamp(text);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("log_to_features: invalid or non-finite timestamp");
  return *parsed;
}

}  // namespace

LogToFeaturesResult log_to_features(const EventLog& log,
                                    const LogToFeaturesOptions& options) {
  if (options.activity_key.empty() || options.timestamp_key.empty() ||
      options.resource_key.empty())
    throw std::invalid_argument("log_to_features: keys must not be empty");
  if (!std::isfinite(options.default_not_present))
    throw std::invalid_argument("log_to_features: default_not_present must be finite");
  if (!std::isfinite(options.window_seconds) || options.window_seconds <= 0.0)
    throw std::invalid_argument("log_to_features: window_seconds must be finite and positive");
  if (!std::isfinite(options.epsilon) || options.epsilon < 0.0)
    throw std::invalid_argument("log_to_features: epsilon must be finite and non-negative");
  if (options.min_num_different_string_values > options.max_num_different_string_values)
    throw std::invalid_argument("log_to_features: string cardinality bounds are inverted");
  validate_names(options.event_string_attributes, "event_string_attributes");
  validate_names(options.event_numeric_attributes, "event_numeric_attributes");
  validate_names(options.case_string_attributes, "case_string_attributes");
  validate_names(options.case_numeric_attributes, "case_numeric_attributes");
  validate_names(options.event_attributes, "event_attributes");
  validate_names(options.case_attributes, "case_attributes");

  LogToFeaturesResult out;
  out.variant = options.variant;
  if (!options.feature_names.empty()) out.feature_names = options.feature_names;

  std::vector<std::string> event_strings = merged(options.event_string_attributes, {});
  std::vector<std::string> event_numbers = merged(options.event_numeric_attributes, {});
  std::vector<std::string> case_strings = merged(options.case_string_attributes, {});
  std::vector<std::string> case_numbers = merged(options.case_numeric_attributes, {});
  // Generic lists are deterministic and infer their representation from data.
  for (const auto& n : options.event_attributes) {
    bool numeric = false;
    for (const auto& t : log.traces) for (const auto& e : t.events)
      if (const auto* a = attr_for(e, n)) { numeric = is_numeric_kind(a->kind); break; }
    (numeric ? event_numbers : event_strings).push_back(n);
  }
  for (const auto& n : options.case_attributes) {
    bool numeric = false;
    for (const auto& t : log.traces)
      if (const auto* a = attr_for(t, n)) { numeric = is_numeric_kind(a->kind); break; }
    (numeric ? case_numbers : case_strings).push_back(n);
  }
  append_unique(event_strings, {}); append_unique(event_numbers, {});
  append_unique(case_strings, {}); append_unique(case_numbers, {});

  if (options.variant == LogToFeaturesVariant::EVENT_BASED) {
    std::vector<std::string> names;
    const auto activities = categories_for_events(log, options.activity_key);
    for (const auto& x : activities) names.push_back("activity:" + x);
    for (const auto& n : event_strings) {
      const auto values = distinct_event_values(log, n);
      if (!within_cardinality(values, options)) continue;
      for (const auto& x : values) names.push_back("event:" + n + "=" + x);
    }
    for (const auto& n : case_strings) {
      const auto values = distinct_trace_values(log, n);
      if (!within_cardinality(values, options)) continue;
      for (const auto& x : values) names.push_back("case:" + n + "=" + x);
    }
    for (const auto& n : event_numbers) names.push_back("event:" + n);
    for (const auto& n : case_numbers) names.push_back("case:" + n);
    names.push_back("timestamp");
    if (options.feature_names.empty()) out.feature_names = names;
    else validate_names(out.feature_names, "feature_names");
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < out.feature_names.size(); ++i) index[out.feature_names[i]] = i;
    for (const auto& t : log.traces) {
      std::vector<std::vector<double>> rows;
      for (const auto& e : t.events) {
        std::vector<double> row(out.feature_names.size(), options.default_not_present);
        auto set = [&](const std::string& n, double v) { if (auto i = index.find(n); i != index.end()) row[i->second] = v; };
        set("activity:" + value_for(e, options.activity_key), 1.0);
        for (const auto& n : event_strings) {
          const auto values = distinct_event_values(log, n);
          if (within_cardinality(values, options))
            set("event:" + n + "=" + event_attribute_value(e, n), 1.0);
        }
        for (const auto& n : case_strings) {
          const auto values = distinct_trace_values(log, n);
          if (within_cardinality(values, options))
            set("case:" + n + "=" + trace_attribute_value(t, n), 1.0);
        }
        for (const auto& n : event_numbers) if (const auto* a = attr_for(e, n)) set("event:" + n, checked_number(a->value, options.default_not_present));
        for (const auto& n : case_numbers) if (const auto* a = attr_for(t, n)) set("case:" + n, checked_number(a->value, options.default_not_present));
        if (const auto p = parse_timestamp(value_for(e, options.timestamp_key)); p && std::isfinite(*p)) set("timestamp", *p);
        else if (!value_for(e, options.timestamp_key).empty()) throw std::invalid_argument("log_to_features: invalid timestamp");
        rows.push_back(std::move(row));
      }
      out.event_rows.push_back(std::move(rows));
    }
    return out;
  }

  if (options.variant == LogToFeaturesVariant::TRACE_BASED) {
    const auto activities = categories_for_events(log, options.activity_key);
    std::vector<std::string> names;
    auto add_name = [&](std::string name) {
      if (std::find(names.begin(), names.end(), name) == names.end())
        names.push_back(std::move(name));
    };
    for (const auto& a : activities) {
      add_name("activity_count:" + a);
      add_name("first_activity:" + a);
      add_name("last_activity:" + a);
      add_name("activity_index:" + a);
      if (options.enable_activity_def_representation)
        add_name("activity:" + a);
    }
    names.insert(names.end(), {"duration", "wip"});
    for (const auto& n : event_numbers) names.push_back("event:" + n);
    for (const auto& n : case_numbers) names.push_back("case:" + n);
    for (const auto& n : event_strings) {
      const auto values = distinct_event_values(log, n);
      if (!within_cardinality(values, options)) continue;
      for (const auto& value : values) add_name("event:" + n + "@" + value);
    }
    for (const auto& n : case_strings) {
      const auto values = distinct_trace_values(log, n);
      if (!within_cardinality(values, options)) continue;
      for (const auto& value : values) add_name("trace:" + n + "@" + value);
    }
    std::vector<std::string> successor_attributes = options.event_successor_attributes;
    if (successor_attributes.empty() && options.enable_succ_def_representation)
      successor_attributes.push_back(options.activity_key);
    append_unique(successor_attributes, {});
    if (options.enable_succ_def_representation) {
      for (const auto& key : successor_attributes) {
        std::set<std::pair<std::string, std::string>> pairs;
        for (const auto& trace : log.traces)
          for (std::size_t i = 1; i < trace.events.size(); ++i)
            pairs.emplace(categorical_value(event_attribute_value(trace.events[i - 1], key)),
                          categorical_value(event_attribute_value(trace.events[i], key)));
        for (const auto& pair : pairs)
          add_name("succession:" + key + "@" + pair.first + "#" + pair.second);
      }
    }
    const bool all_extra = options.enable_all_extra_features;
    const bool case_duration = all_extra || options.enable_case_duration;
    const bool case_id = all_extra || options.add_case_identifier_column;
    const bool first_times = all_extra || options.enable_times_from_first_occurrence;
    const bool last_times = all_extra || options.enable_times_from_last_occurrence;
    const bool direct_paths = all_extra || options.enable_direct_paths_times_last_occ;
    const bool indirect_paths = all_extra || options.enable_indirect_paths_times_last_occ;
    const bool wip = all_extra || options.enable_work_in_progress;
    const bool workload = all_extra || options.enable_resource_workload;
    const bool indexes = all_extra || options.enable_first_last_activity_index;
    const bool max_concurrent = all_extra || options.enable_max_concurrent_events;
    const bool max_per_activity = all_extra || options.enable_max_concurrent_events_per_activity;
    if (case_duration) add_name("@@caseDuration");
    if (case_id) add_name("@@case_id_column");
    if (first_times) for (const auto& a : activities) {
      add_name("startToFirstOcc@@" + a); add_name("firstOccToEnd@@" + a);
    }
    if (last_times) for (const auto& a : activities) {
      add_name("startToLastOcc@@" + a); add_name("lastOccToEnd@@" + a);
    }
    if (direct_paths || indirect_paths) {
      for (const auto& from : activities) for (const auto& to : activities) if (from != to) {
        if (direct_paths) add_name("directPathPerformanceLastOcc@@" + from + "##" + to);
        if (indirect_paths) add_name("indirectPathPerformanceLastOcc@@" + from + "##" + to);
      }
    }
    if (wip) add_name("@@work_in_progress");
    if (indexes) for (const auto& a : activities) {
      add_name("firstIndexAct@@" + a); add_name("lastIndexAct@@" + a);
    }
    if (max_concurrent) add_name("@@max_concurrent_activities_general");
    if (max_per_activity) for (const auto& a : activities)
      add_name("@@max_concurrent_activities_like_" + a);
    std::set<std::string> resources;
    for (const auto& t : log.traces) for (const auto& e : t.events) resources.insert(value_for(e, options.resource_key));
    for (const auto& r : resources) if (!r.empty()) {
      add_name("resource_workload:" + r);
      if (workload) add_name("resource_workload@@" + r);
    }
    if (options.feature_names.empty()) out.feature_names = names; else { validate_names(options.feature_names, "feature_names"); }
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < out.feature_names.size(); ++i) index[out.feature_names[i]] = i;
    for (const auto& t : log.traces) {
      std::vector<double> row(out.feature_names.size(), options.default_not_present);
      auto set = [&](const std::string& n, double v) { if (auto i = index.find(n); i != index.end()) row[i->second] = v; };
      std::map<std::string, int> count; std::map<std::string, std::size_t> first, last;
      std::set<std::string> seen; double lo = 0, hi = 0; bool have = false;
      double case_start = std::numeric_limits<double>::infinity();
      double case_end = -std::numeric_limits<double>::infinity();
      std::map<std::string, double> first_time, last_time;
      std::map<std::string, double> direct_path_values, indirect_path_values;
      std::vector<TimedEvent> timed;
      std::map<std::string, int> resource_count;
      for (std::size_t i = 0; i < t.events.size(); ++i) {
        const auto& e = t.events[i]; const auto a = value_for(e, options.activity_key); ++count[a]; if (!seen.count(a)) first[a] = i; last[a] = i; seen.insert(a);
        const auto text = value_for(e, options.timestamp_key); if (!text.empty()) { auto p = parse_timestamp(text); if (!p || !std::isfinite(*p)) throw std::invalid_argument("log_to_features: invalid timestamp"); if (!have) lo = hi = *p; else { lo = std::min(lo,*p); hi = std::max(hi,*p); } have = true; }
        const auto start_text = options.start_timestamp_key.empty() ? text : value_for(e, options.start_timestamp_key);
        const auto start = start_text.empty() ? std::numeric_limits<double>::quiet_NaN() : instant(e, options.start_timestamp_key.empty() ? options.timestamp_key : options.start_timestamp_key, false);
        const auto finish = text.empty() ? std::numeric_limits<double>::quiet_NaN() : instant(e, options.timestamp_key, false);
        if (std::isfinite(start) && std::isfinite(finish)) {
          timed.push_back({&e, finish, start, 0, i});
          case_start = std::min(case_start, start);
          case_end = std::max(case_end, finish);
        }
        if (std::isfinite(finish)) {
          if (!first_time.contains(a)) first_time[a] = finish;
          last_time[a] = finish;
        }
        const auto r = value_for(e, options.resource_key);
        if (!r.empty()) {
          ++resource_count[r];
          const auto it = index.find("resource_workload:" + r);
          if (it != index.end()) set(it->first, row[it->second] + 1.0);
        }
      }
      for (const auto& [a,c] : count) { set("activity_count:"+a,c); set("first_activity:"+a, first[a] == 0 ? 1.0 : 0.0); set("last_activity:"+a, !t.events.empty() && value_for(t.events.back(), options.activity_key)==a ? 1.0 : 0.0); set("activity_index:"+a, static_cast<double>(first[a])); set("activity:"+a, 1.0); if (indexes) { set("firstIndexAct@@"+a, static_cast<double>(first[a])); set("lastIndexAct@@"+a, static_cast<double>(last[a])); } }
      for (const auto& n : event_strings) { const auto values = distinct_event_values(log, n); if (!within_cardinality(values, options)) continue; set("event:"+n+"@"+categorical_value(event_attribute_value(t.events.empty() ? Event{} : t.events.front(), n)), 1.0); for (const auto& e : t.events) set("event:"+n+"@"+categorical_value(event_attribute_value(e,n)), 1.0); }
      for (const auto& n : case_strings) { const auto values = distinct_trace_values(log, n); if (within_cardinality(values, options)) set("trace:"+n+"@"+categorical_value(trace_attribute_value(t,n)), 1.0); }
      for (const auto& n : successor_attributes) if (options.enable_succ_def_representation) for (std::size_t i=1;i<t.events.size();++i) set("succession:"+n+"@"+categorical_value(event_attribute_value(t.events[i-1],n))+"#"+categorical_value(event_attribute_value(t.events[i],n)), 1.0);
      if (have) {
        set("duration", hi-lo);
        if (case_duration && std::isfinite(case_start) && std::isfinite(case_end))
          set("@@caseDuration", case_end-case_start);
      }
      set("wip", static_cast<double>(t.events.size()));
      for (const auto& n : event_numbers) { double value = options.default_not_present; bool found = false; for (const auto& e:t.events) if (attr_for(e,n) || !event_attribute_value(e,n).empty()) { value = numeric_attribute_value(e,n,options.default_not_present); found = true; } if (found) set("event:"+n,value); }
      for (const auto& n : case_numbers) if (attr_for(t,n) || !trace_attribute_value(t,n).empty()) set("case:"+n,numeric_trace_value(t,n,options.default_not_present));
      for (const auto& [r, c] : resource_count) if (workload) set("resource_workload@@" + r, static_cast<double>(c));
      for (const auto& [a, first_value] : first_time) {
        if (first_times && std::isfinite(case_start) && std::isfinite(case_end)) { set("startToFirstOcc@@"+a, first_value-case_start); set("firstOccToEnd@@"+a, case_end-first_value); }
        if (last_times && std::isfinite(case_start) && std::isfinite(case_end)) { const auto last_value = last_time[a]; set("startToLastOcc@@"+a, last_value-case_start); set("lastOccToEnd@@"+a, case_end-last_value); }
      }
      for (std::size_t i=1;i<t.events.size();++i) {
        const auto from=value_for(t.events[i-1],options.activity_key), to=value_for(t.events[i],options.activity_key);
        const auto lhs = instant(t.events[i-1], options.timestamp_key, false), rhs = instant(t.events[i], options.timestamp_key, false);
        if (std::isfinite(lhs) && std::isfinite(rhs)) set("directPathPerformanceLastOcc@@"+from+"##"+to, std::max(0.0,rhs-lhs));
      }
      if (indirect_paths) for (std::size_t i=0;i<t.events.size();++i) for (std::size_t j=i+2;j<t.events.size();++j) {
        const auto lhs=instant(t.events[i],options.timestamp_key,false), rhs=instant(t.events[j],options.timestamp_key,false);
        if (std::isfinite(lhs)&&std::isfinite(rhs)) set("indirectPathPerformanceLastOcc@@"+value_for(t.events[i],options.activity_key)+"##"+value_for(t.events[j],options.activity_key), std::max(0.0,rhs-lhs));
      }
      if (wip || max_concurrent || max_per_activity) {
        int general = 0; std::map<std::string,int> per_activity;
        for (const auto& event : timed) {
          int active = 0; int same_activity = 0;
          for (const auto& other : timed) if (other.start <= event.finish + options.epsilon && other.finish + options.epsilon >= event.start) {
            ++active;
            if (value_for(*other.event, options.activity_key) == value_for(*event.event, options.activity_key)) ++same_activity;
          }
          general = std::max(general, active);
          per_activity[value_for(*event.event, options.activity_key)] = std::max(per_activity[value_for(*event.event, options.activity_key)], same_activity);
        }
        if (wip) set("@@work_in_progress", static_cast<double>(general));
        if (max_concurrent) set("@@max_concurrent_activities_general", static_cast<double>(general));
        if (max_per_activity) for (const auto& [a,c] : per_activity) set("@@max_concurrent_activities_like_"+a, static_cast<double>(c));
      }
      if (case_id) out.case_ids.push_back(t.case_id);
      out.trace_rows.push_back(std::move(row));
    }
    return out;
  }

  if (options.variant != LogToFeaturesVariant::TEMPORAL) throw std::invalid_argument("log_to_features: unknown variant");
  std::vector<TimedEvent> events; double origin = std::numeric_limits<double>::infinity();
  for (std::size_t ti=0; ti<log.traces.size(); ++ti) for (std::size_t i=0;i<log.traces[ti].events.size();++i) { const auto& e=log.traces[ti].events[i]; double f=instant(e,options.timestamp_key,true); double s=f; if (!options.start_timestamp_key.empty()) s=instant(e,options.start_timestamp_key,true); if (s>f) throw std::invalid_argument("log_to_features: start timestamp after finish"); origin=std::min(origin,s); events.push_back({&e,f,s,ti,i}); }
  out.feature_names = options.feature_names;
  if (out.feature_names.empty()) out.feature_names = {"arrivals", "finishes", "service_seconds", "waiting_seconds", "sojourn_seconds", "rework", "cases", "resource_workload"}; else validate_names(out.feature_names,"feature_names");
  if (events.empty()) return out;
  std::map<long long,std::vector<const TimedEvent*>> buckets;
  for (const auto& e:events) buckets[static_cast<long long>(std::floor((e.start-origin)/options.window_seconds))].push_back(&e);
  std::unordered_map<std::string,std::size_t> index; for (std::size_t i=0;i<out.feature_names.size();++i) index[out.feature_names[i]]=i;
  for (const auto& [b,es]:buckets) { TemporalFeatureRow row; row.timestamp=origin + static_cast<double>(b)*options.window_seconds; row.values.assign(out.feature_names.size(), options.default_not_present); auto add=[&](const std::string& n,double v){auto i=index.find(n); if(i!=index.end()) row.values[i->second]=v;}; double service=0, wait=0, soj=0; std::set<std::size_t> cases; std::set<std::string> resources; std::map<std::size_t,double> last;
    for(const auto* e:es){ add("arrivals", (index.count("arrivals")?row.values[index["arrivals"]]:0)+1); add("finishes", (index.count("finishes")?row.values[index["finishes"]]:0)+1); service += e->finish-e->start; soj += e->finish-e->start; cases.insert(e->trace); const auto r=value_for(*e->event,options.resource_key); if(!r.empty()) resources.insert(r); auto it=last.find(e->trace); if(it!=last.end()) wait += std::max(0.0,e->start-it->second); last[e->trace]=e->finish; }
    add("service_seconds",service); add("waiting_seconds",wait); add("sojourn_seconds",soj); add("cases",static_cast<double>(cases.size())); add("resource_workload",static_cast<double>(es.size())); double rework=0; for(const auto* e:es){ int prior=0; for(const auto* q:es) if(q->trace==e->trace && q->index<e->index && q->event->activity==e->event->activity) ++prior; if(prior) rework+=1; } add("rework",rework); out.temporal_rows.push_back(std::move(row)); }
  return out;
}

}  // namespace minera
