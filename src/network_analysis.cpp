#include "minera/network_analysis.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr double kWeekSeconds = 7.0 * 86400.0;

std::vector<BusinessHourSlot> normalized_slots(const NetworkAnalysisOptions& o) {
  auto slots = o.business_hour_slots;
  if (slots.empty()) {
    for (int day = 0; day < 5; ++day) {
      const double start = day * 86400.0 + 7.0 * 3600.0;
      slots.push_back({start, start + 10.0 * 3600.0});
    }
  }
  for (const auto& s : slots) {
    if (!std::isfinite(s.start_seconds) || !std::isfinite(s.end_seconds) ||
        s.start_seconds < 0.0 || s.end_seconds > kWeekSeconds ||
        s.end_seconds <= s.start_seconds)
      throw std::invalid_argument("network analysis: business-hour slots must satisfy 0 <= start < end <= 604800");
  }
  std::sort(slots.begin(), slots.end(), [](const auto& a, const auto& b) {
    return std::tie(a.start_seconds, a.end_seconds) < std::tie(b.start_seconds, b.end_seconds);
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& s : slots) {
    if (!merged.empty() && s.start_seconds <= merged.back().end_seconds)
      merged.back().end_seconds = std::max(merged.back().end_seconds, s.end_seconds);
    else merged.push_back(s);
  }
  return merged;
}

std::optional<std::string> value(const Event& e, std::string_view selector) {
  if (selector.empty()) return std::nullopt;
  if (selector == "concept:name") return e.activity;
  if (selector == "time:timestamp") return e.timestamp;
  if (selector == "org:resource") return e.resource;
  if (selector == "case:concept:name" || selector == "case:id" || selector == "case_id") return e.case_id;
  if (selector == "lifecycle:transition") return e.lifecycle;
  if (const auto* p = e.attribute(selector)) return *p;
  return std::nullopt;
}

std::optional<std::string> value(const Event& e, const Trace* trace,
                                 std::string_view selector) {
  if (const auto event_value = value(e, selector)) return event_value;
  if (trace && selector.rfind("case:", 0) == 0) {
    const auto key = selector.substr(5);
    if (const auto* attribute = trace->attribute(key)) return *attribute;
    if (const auto* attribute = trace->attribute(selector)) return *attribute;
  }
  return std::nullopt;
}

std::optional<double> instant(const Event& e, const Trace* trace,
                              std::string_view selector) {
  const auto v = value(e, trace, selector);
  if (!v) return std::nullopt;
  return parse_timestamp(*v);
}

std::optional<double> local_wall(std::string_view text) {
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
  const auto tz = text.substr(0, end).find_first_of("Zz+-", 10);
  if (tz != std::string_view::npos) end = tz;
  return parse_timestamp(text.substr(0, end));
}

double active(double left, double right, const std::vector<BusinessHourSlot>& slots) {
  double total = 0.0;
  for (const auto& s : slots) total += std::max(0.0, std::min(right, s.end_seconds) - std::max(left, s.start_seconds));
  return total;
}

double business_elapsed(double source, double target, const std::vector<BusinessHourSlot>& slots) {
  if (target <= source) return 0.0;
  constexpr double shift = 3.0 * 86400.0;
  const double a = source + shift, b = target + shift;
  const auto wa = static_cast<long long>(std::floor(a / kWeekSeconds));
  const auto wb = static_cast<long long>(std::floor(b / kWeekSeconds));
  const double oa = a - wa * kWeekSeconds, ob = b - wb * kWeekSeconds;
  double full = 0.0;
  for (const auto& s : slots) full += s.end_seconds - s.start_seconds;
  if (wa == wb) return active(oa, ob, slots);
  double total = active(oa, kWeekSeconds, slots) + active(0.0, ob, slots);
  if (wb - wa > 1) total += static_cast<double>(wb - wa - 1) * full;
  return total;
}

struct Ref {
  const Event* event;
  const Trace* trace;
  std::size_t index;
  std::string sort_text;
  std::optional<double> sort_time;
};

}  // namespace

NetworkAnalysisResult discover_network_analysis(const EventLog& log, const NetworkAnalysisOptions& o) {
  if (o.edge_reference != NetworkAnalysisEdgeReference::kOut &&
      o.edge_reference != NetworkAnalysisEdgeReference::kIn)
    throw std::invalid_argument("network analysis edge_reference must be _out or _in");
  if (o.max_rows == 0 || o.max_rows > 1000000) throw std::invalid_argument("network analysis max_rows safety bound exceeded");
  if (o.max_values == 0 || o.max_values > 10000000) throw std::invalid_argument("network analysis max_values safety bound exceeded");
  const auto slots = o.business_hours ? normalized_slots(o) : std::vector<BusinessHourSlot>{};
  NetworkAnalysisResult result;
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();
  std::vector<Ref> refs;
  refs.reserve(log.event_count());
  std::size_t input = 0;
  for (const auto& tr : log.traces) for (const auto& e : tr.events) {
    auto sv = value(e, &tr, o.sorting_attribute);
    if (!sv || sv->empty()) continue;
    refs.push_back({&e, &tr, input++, sv.value_or(""), instant(e, &tr, o.sorting_attribute)});
  }
  std::stable_sort(refs.begin(), refs.end(), [](const Ref& a, const Ref& b) {
    if (a.sort_time && b.sort_time && *a.sort_time != *b.sort_time) return *a.sort_time < *b.sort_time;
    if (a.sort_time.has_value() != b.sort_time.has_value()) return a.sort_time.has_value();
    if (a.sort_text != b.sort_text) return a.sort_text < b.sort_text;
    return a.index < b.index;
  });
  for (std::size_t i = 0; i < refs.size(); ++i) refs[i].index = i;
  std::vector<const Ref*> successors(refs.size(), nullptr);
  std::map<std::string, const Ref*> next_by_relation;
  for (std::size_t i = refs.size(); i-- > 0;) {
    if (const auto relation = value(*refs[i].event, refs[i].trace, o.out_relation_attribute);
        relation && !relation->empty()) {
      const auto target = next_by_relation.find(*relation);
      if (target != next_by_relation.end()) successors[i] = target->second;
    }
    if (const auto relation = value(*refs[i].event, refs[i].trace, o.in_relation_attribute);
        relation && !relation->empty()) {
      next_by_relation[*relation] = &refs[i];
    }
  }
  using Key = std::tuple<std::string, std::string, std::string>;
  std::map<Key, NetworkAnalysisEdge> grouped;
  for (std::size_t source_index = 0; source_index < refs.size(); ++source_index) {
    const auto& source = refs[source_index];
    const auto relation = value(*source.event, source.trace, o.out_relation_attribute);
    if (!relation || relation->empty()) continue;
    const auto source_node = value(*source.event, source.trace, o.source_node_attribute);
    if (!source_node || source_node->empty()) continue;
    const Ref* target = successors[source_index];
    if (!target) continue;
    const auto target_node = value(*target->event, target->trace, o.target_node_attribute);
    const auto& edge_event = o.edge_reference == NetworkAnalysisEdgeReference::kOut ? source : *target;
    const auto edge_value = value(*edge_event.event, edge_event.trace, o.edge_attribute);
    if (!target_node || target_node->empty() || !edge_value || edge_value->empty()) continue;
    const Key key{*source_node, *target_node, *edge_value};
    auto [it, inserted] = grouped.emplace(key, NetworkAnalysisEdge{*source_node, *target_node, *edge_value, 0, {}});
    (void)inserted;
    ++it->second.count;
    ++result.diagnostics.link_count;
    if (o.include_performance) {
      const auto st = instant(*source.event, source.trace, o.timestamp_attribute);
      const auto tt = instant(*target->event, target->trace, o.timestamp_attribute);
      if (st && tt) {
        const double duration = o.business_hours
            ? business_elapsed(local_wall(value(*source.event, source.trace, o.timestamp_attribute).value_or("")).value_or(*st), local_wall(value(*target->event, target->trace, o.timestamp_attribute).value_or("")).value_or(*tt), slots)
            : (*tt - *st);
        if (std::isfinite(duration) && result.diagnostics.performance_value_count < o.max_values) {
          it->second.performance.push_back(duration);
          ++result.diagnostics.performance_value_count;
        } else if (result.diagnostics.performance_value_count >= o.max_values) result.diagnostics.output_truncated = true;
      }
    }
  }
  for (auto& [key, edge] : grouped) {
    (void)key;
    if (result.edges.size() >= o.max_rows) { result.diagnostics.output_truncated = true; break; }
    result.edges.push_back(std::move(edge));
  }
  result.diagnostics.row_count = result.edges.size();
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated ? "output safety bound applied" : (result.edges.empty() ? "no links" : "ok");
  return result;
}

}  // namespace minera
