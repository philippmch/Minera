#include "minera/network_analysis.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <variant>

#include "minera/timestamp.hpp"
#include "numpy_object_sort.hpp"
#include "python_scalar.hpp"
#include "reference_activity_selector.hpp"

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

// First-class fields count only when present: an explicitly present empty
// value is a value, a missing or null one is not (pandas drops NaN keys).
const std::string* present(FirstClassAttributeState state, const std::string& text) {
  if (state == FirstClassAttributeState::kPresent ||
      (state == FirstClassAttributeState::kMissing && !text.empty()))
    return &text;
  return nullptr;
}

// The literal cell a selector names: the event attribute, then (for "case:"
// keys) the trace attribute. nullptr when there is no such literal cell.
const EventAttribute* literal_cell(const Event& e, const Trace* trace, std::string_view selector) {
  for (const auto& attribute : e.attributes)
    if (attribute.key == selector) return &attribute;
  if (trace && selector.rfind("case:", 0) == 0) {
    const auto key = selector.substr(5);
    for (const auto& attribute : trace->attributes)
      if (attribute.key == key) return &attribute;
    for (const auto& attribute : trace->attributes)
      if (attribute.key == selector) return &attribute;
  }
  return nullptr;
}

// Whether a selector names the case column. A flat adapter's case column
// keeps its header name; an XES trace attribute (a trace with a selected
// case cell) is a dataframe column only under its "case:" prefix, so its
// bare key is not a column (the bare "concept:name" is the activity).
bool case_selector(const Event& e, const Trace* trace, std::string_view selector) {
  if (e.case_id_source_key.empty())
    return selector == "case:concept:name" || selector == "case:id" || selector == "case_id";
  if (selector == "case:" + e.case_id_source_key) return true;
  return selector == e.case_id_source_key && !(trace && trace->case_id_attribute);
}

// The first-class projection a selector names on this event, with its
// presence: a projection answers only for the field it was read from (J1).
const std::string* projection(const Event& e, const Trace* trace, std::string_view selector) {
  using detail::projection_names;
  if (projection_names(e.activity_source_key, e.activity_attribute, "concept:name", selector))
    return present(e.activity_state, e.activity);
  if (projection_names(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", selector))
    return present(e.timestamp_state, e.timestamp);
  if (projection_names(e.resource_source_key, e.resource_attribute, "org:resource", selector))
    return present(e.resource_state, e.resource);
  if (projection_names(e.lifecycle_source_key, e.lifecycle_attribute, "lifecycle:transition", selector))
    return present(e.lifecycle_state, e.lifecycle);
  if (case_selector(e, trace, selector)) return present(e.case_id_state, e.case_id);
  return nullptr;
}

// Whether the event's dataframe row has the named column at all (a present
// null still is a column).
bool has_column(const Event& e, const Trace* trace, std::string_view selector) {
  if (literal_cell(e, trace, selector)) return true;
  using detail::projection_names;
  const auto owns = [&](const std::string& source, const std::optional<EventAttribute>& cell,
                        std::string_view canonical, FirstClassAttributeState state, const std::string& text) {
    return projection_names(source, cell, canonical, selector) &&
           (state != FirstClassAttributeState::kMissing || !text.empty());
  };
  if (owns(e.activity_source_key, e.activity_attribute, "concept:name", e.activity_state, e.activity)) return true;
  if (owns(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", e.timestamp_state, e.timestamp)) return true;
  if (owns(e.resource_source_key, e.resource_attribute, "org:resource", e.resource_state, e.resource)) return true;
  if (owns(e.lifecycle_source_key, e.lifecycle_attribute, "lifecycle:transition", e.lifecycle_state, e.lifecycle)) return true;
  if (case_selector(e, trace, selector) && (e.case_id_state != FirstClassAttributeState::kMissing || !e.case_id.empty() ||
                                     (trace && trace->has_case_id())))
    return true;
  return false;
}

// The typed cell an adapter kept for the projection a selector names (the
// XES value an int, float or boolean concept:name was read from), if any.
const EventAttribute* projection_cell(const Event& e, const Trace* trace, std::string_view selector) {
  using detail::projection_names;
  const auto kept = [](const std::optional<EventAttribute>& cell) { return cell ? &*cell : nullptr; };
  if (projection_names(e.activity_source_key, e.activity_attribute, "concept:name", selector)) return kept(e.activity_attribute);
  if (projection_names(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", selector)) return kept(e.timestamp_attribute);
  if (projection_names(e.resource_source_key, e.resource_attribute, "org:resource", selector)) return kept(e.resource_attribute);
  if (projection_names(e.lifecycle_source_key, e.lifecycle_attribute, "lifecycle:transition", selector)) return kept(e.lifecycle_attribute);
  if (case_selector(e, trace, selector) && trace) return kept(trace->case_id_attribute);
  return nullptr;
}

// The Python object the selector names on this event: a literal cell as it
// is typed, else the projection's typed cell, else its text (a timestamp
// projection is a Timestamp). An empty selector is a column name like any
// other. nullopt when the value is missing (None, NaN, pd.NA, absent).
std::optional<python::Scalar> typed_value(const Event& e, const Trace* trace, std::string_view selector) {
  if (const auto* cell = literal_cell(e, trace, selector)) {
    auto scalar = python::from_attribute(*cell);
    if (python::is_missing(scalar)) return std::nullopt;
    return scalar;
  }
  const auto* text = projection(e, trace, selector);
  if (!text) return std::nullopt;
  if (const auto* cell = projection_cell(e, trace, selector)) {
    auto scalar = python::from_attribute(*cell);
    if (python::is_missing(scalar)) return std::nullopt;
    if (scalar.kind != python::Scalar::Kind::kText) return scalar;
  }
  if (detail::projection_names(e.timestamp_source_key, e.timestamp_attribute, "time:timestamp", selector))
    if (auto date = python::date_literal(*text)) return date;
  return python::text(*text);
}

// The selected value as timestamp text: a Timestamp keeps the spelling it
// was read from.
std::optional<std::string> value(const Event& e, const Trace* trace, std::string_view selector) {
  const auto typed = typed_value(e, trace, selector);
  if (!typed) return std::nullopt;
  if (typed->kind == python::Scalar::Kind::kDate) return typed->text;
  return python::str(*typed);
}

// Sort keys compare like the pandas column they come from: numbers (bool,
// int, uint, float) by exact value, datetimes by exact instant, strings
// lexically (a date-looking string is still a string), and missing values
// (None, NaN, pd.NA, an absent column value) last. Numbers and strings in one
// column cannot be ordered (Python raises TypeError).
struct SortKey {
  enum class Kind { kMissing, kNumber, kDate, kText } kind = Kind::kMissing;
  python::Scalar value;
};

SortKey sort_key(const Event& e, const Trace* trace, std::string_view selector) {
  SortKey key;
  auto typed = typed_value(e, trace, selector);
  if (!typed) return key;
  if (python::is_number(*typed)) key.kind = SortKey::Kind::kNumber;
  else if (typed->kind == python::Scalar::Kind::kDate) key.kind = SortKey::Kind::kDate;
  else key.kind = SortKey::Kind::kText;
  key.value = std::move(*typed);
  return key;
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
  SortKey sort;
};

// The relation value pandas merges on: equal keys of any numeric type join
// (1, 1.0 and True), a number and its text do not, and null-like keys (None,
// NaN, pd.NA or an absent value) join each other, unlike in grouping.
std::string relation_key(const Event& e, const Trace* trace, std::string_view selector) {
  if (const auto v = typed_value(e, trace, selector)) return python::identity(*v);
  return std::string(1, '\0');
}

// df[[columns]] on the selected columns: every one must exist in some row.
void require_columns(const EventLog& log, const NetworkAnalysisOptions& o) {
  if (log.event_count() == 0) return;
  const std::vector<std::string> selected{o.out_relation_attribute, o.in_relation_attribute,
                                          o.source_node_attribute, o.target_node_attribute,
                                          o.edge_attribute, o.sorting_attribute, o.timestamp_attribute};
  std::vector<std::string> distinct, absent;
  for (const auto& name : selected) {
    if (std::find(distinct.begin(), distinct.end(), name) != distinct.end()) continue;
    distinct.push_back(name);
    bool found = false;
    for (const auto& tr : log.traces) {
      for (const auto& e : tr.events)
        if (has_column(e, &tr, name)) { found = true; break; }
      if (found) break;
    }
    if (!found) absent.push_back(name);
  }
  if (absent.empty()) return;
  const auto list = [](const std::vector<std::string>& names) {
    std::string out = "[";
    for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", '" : "'") + names[i] + "'";
    return out + "]";
  };
  // The reference lists the absent names in set order; native lists them in
  // selector order.
  if (absent.size() == distinct.size())
    throw std::invalid_argument("network analysis: KeyError: \"None of [Index(" + list(absent) +
                                ", dtype='str')] are in the [columns]\"");
  throw std::invalid_argument("network analysis: KeyError: \"" + list(absent) + " not in index\"");
}

}  // namespace

NetworkAnalysisResult discover_network_analysis(const EventLog& log, const NetworkAnalysisOptions& o) {
  if (o.variant != NetworkAnalysisVariant::kDataframe)
    throw std::invalid_argument("network_analysis_variant must be dataframe");
  if (o.edge_reference != NetworkAnalysisEdgeReference::kOut &&
      o.edge_reference != NetworkAnalysisEdgeReference::kIn)
    throw std::invalid_argument("network analysis edge_reference must be _out or _in");
  if (o.max_rows == 0 || o.max_rows > 1000000) throw std::invalid_argument("network analysis max_rows safety bound exceeded");
  if (o.max_values == 0 || o.max_values > 10000000) throw std::invalid_argument("network analysis max_values safety bound exceeded");
  require_columns(log, o);
  const auto slots = o.business_hours ? normalized_slots(o) : std::vector<BusinessHourSlot>{};
  NetworkAnalysisResult result;
  result.variant = o.variant;
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();
  std::vector<Ref> refs;
  refs.reserve(log.event_count());
  std::size_t input = 0;
  bool numbers = false, texts = false, dates = false;
  for (const auto& tr : log.traces) for (const auto& e : tr.events) {
    auto key = sort_key(e, &tr, o.sorting_attribute);
    numbers |= key.kind == SortKey::Kind::kNumber;
    texts |= key.kind == SortKey::Kind::kText;
    dates |= key.kind == SortKey::Kind::kDate;
    refs.push_back({&e, &tr, input++, std::move(key)});
  }
  if (static_cast<int>(numbers) + static_cast<int>(texts) + static_cast<int>(dates) > 1) {
    // sort_values argsorts the non-missing values; the first pair numpy's
    // quicksort cannot compare is the error.
    std::vector<const python::Scalar*> present_values;
    for (const auto& ref : refs)
      if (ref.sort.kind != SortKey::Kind::kMissing) present_values.push_back(&ref.sort.value);
    try {
      (void)detail::numpy_object_quick_argsort(present_values.size(), [&](std::size_t a, std::size_t b) {
        return python::compare(*present_values[a], *present_values[b]);
      });
    } catch (const python::TypeError& error) {
      throw std::invalid_argument(std::string("network analysis: ") + error.what());
    }
    throw std::invalid_argument("network analysis: '<' not supported between mixed sorting values");
  }
  std::stable_sort(refs.begin(), refs.end(), [](const Ref& a, const Ref& b) {
    const bool a_missing = a.sort.kind == SortKey::Kind::kMissing;
    const bool b_missing = b.sort.kind == SortKey::Kind::kMissing;
    if (a_missing || b_missing) return !a_missing && b_missing;
    return python::compare(a.sort.value, b.sort.value) < 0;
  });
  for (std::size_t i = 0; i < refs.size(); ++i) refs[i].index = i;
  std::vector<const Ref*> successors(refs.size(), nullptr);
  std::map<std::string, const Ref*> next_by_relation;
  for (std::size_t i = refs.size(); i-- > 0;) {
    const auto out = relation_key(*refs[i].event, refs[i].trace, o.out_relation_attribute);
    const auto target = next_by_relation.find(out);
    if (target != next_by_relation.end()) successors[i] = target->second;
    next_by_relation[relation_key(*refs[i].event, refs[i].trace, o.in_relation_attribute)] = &refs[i];
  }
  // Each link is a row of the joined frame; the edges are its groupby over
  // (source, target, edge), with pandas' typed key identity and labels.
  struct Link {
    std::optional<double> duration;
  };
  std::vector<std::vector<python::Scalar>> keys;
  std::vector<Link> links;
  for (std::size_t source_index = 0; source_index < refs.size(); ++source_index) {
    const auto& source = refs[source_index];
    const auto source_node = typed_value(*source.event, source.trace, o.source_node_attribute);
    if (!source_node) continue;
    const Ref* target = successors[source_index];
    if (!target) continue;
    const auto target_node = typed_value(*target->event, target->trace, o.target_node_attribute);
    const auto& edge_event = o.edge_reference == NetworkAnalysisEdgeReference::kOut ? source : *target;
    const auto edge_value = typed_value(*edge_event.event, edge_event.trace, o.edge_attribute);
    if (!target_node || !edge_value) continue;
    keys.push_back({*source_node, *target_node, *edge_value});
    Link link;
    ++result.diagnostics.link_count;
    if (o.include_performance) {
      const auto st = instant(*source.event, source.trace, o.timestamp_attribute);
      const auto tt = instant(*target->event, target->trace, o.timestamp_attribute);
      if (st && tt) {
        const double duration = o.business_hours
            ? business_elapsed(local_wall(value(*source.event, source.trace, o.timestamp_attribute).value_or("")).value_or(*st), local_wall(value(*target->event, target->trace, o.timestamp_attribute).value_or("")).value_or(*tt), slots)
            : (*tt - *st);
        if (std::isfinite(duration) && result.diagnostics.performance_value_count < o.max_values) {
          link.duration = duration;
          ++result.diagnostics.performance_value_count;
        } else if (result.diagnostics.performance_value_count >= o.max_values) result.diagnostics.output_truncated = true;
      }
    }
    links.push_back(link);
  }
  std::vector<python::Group> groups;
  try {
    groups = python::groupby(keys);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("network analysis: ") + error.what());
  }
  for (const auto& group : groups) {
    if (result.edges.size() >= o.max_rows) { result.diagnostics.output_truncated = true; break; }
    NetworkAnalysisEdge edge{python::str(group.labels[0]), python::str(group.labels[1]), python::str(group.labels[2]), 0, {}};
    for (const auto row : group.rows) {
      ++edge.count;
      if (links[row].duration) edge.performance.push_back(*links[row].duration);
    }
    result.edges.push_back(std::move(edge));
  }
  result.diagnostics.row_count = result.edges.size();
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated ? "output safety bound applied" : (result.edges.empty() ? "no links" : "ok");
  return result;
}

}  // namespace minera
