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
std::optional<std::string> present(FirstClassAttributeState state, const std::string& text) {
  if (state == FirstClassAttributeState::kPresent ||
      (state == FirstClassAttributeState::kMissing && !text.empty()))
    return text;
  return std::nullopt;
}

// A present scalar that pandas treats as missing: None, pd.NA or NaN.
bool missing_scalar(const EventAttribute& attribute) {
  if (attribute.is_null || std::holds_alternative<NativeNA>(attribute.typed_value)) return true;
  if (const auto* d = std::get_if<double>(&attribute.typed_value)) return std::isnan(*d);
  return false;
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

std::optional<std::string> value(const Event& e, std::string_view selector) {
  if (selector.empty()) return std::nullopt;
  for (const auto& attribute : e.attributes)
    if (attribute.key == selector)
      return missing_scalar(attribute) ? std::nullopt : std::optional<std::string>(attribute.value);
  if (selector == "concept:name") return present(e.activity_state, e.activity);
  if (selector == "time:timestamp") return present(e.timestamp_state, e.timestamp);
  if (selector == "org:resource") return present(e.resource_state, e.resource);
  if (selector == "case:concept:name" || selector == "case:id" || selector == "case_id")
    return present(e.case_id_state, e.case_id);
  if (selector == "lifecycle:transition") return present(e.lifecycle_state, e.lifecycle);
  return std::nullopt;
}

std::optional<std::string> value(const Event& e, const Trace* trace,
                                 std::string_view selector) {
  if (const auto event_value = value(e, selector)) return event_value;
  for (const auto& attribute : e.attributes)
    if (attribute.key == selector) return std::nullopt;  // a present missing scalar
  if (trace && selector.rfind("case:", 0) == 0) {
    if (const auto* attribute = literal_cell(e, trace, selector))
      return missing_scalar(*attribute) ? std::nullopt : std::optional<std::string>(attribute->value);
  }
  return std::nullopt;
}

// Sort keys compare like the pandas column they come from: numbers (bool,
// int, uint, float) by exact value, datetimes by exact instant, strings
// lexically (a date-looking string is still a string), and missing values
// (None, NaN, pd.NA, an absent column value) last. Numbers and strings in one
// column cannot be ordered (Python raises TypeError).
struct SortKey {
  enum class Kind { kMissing, kNumber, kDate, kText } kind = Kind::kMissing;
  std::variant<std::int64_t, std::uint64_t, double> number;
  ParsedExactTimestamp date;
  std::string text;
};

SortKey number_key(std::variant<std::int64_t, std::uint64_t, double> n) {
  SortKey key;
  key.kind = SortKey::Kind::kNumber;
  key.number = n;
  return key;
}

SortKey sort_key(const Event& e, const Trace* trace, std::string_view selector) {
  SortKey key;
  const EventAttribute* cell = literal_cell(e, trace, selector);
  if (cell) {
    if (missing_scalar(*cell)) return key;
    if (const auto* i = std::get_if<std::int64_t>(&cell->typed_value)) return number_key(*i);
    if (const auto* u = std::get_if<std::uint64_t>(&cell->typed_value)) return number_key(*u);
    if (const auto* d = std::get_if<double>(&cell->typed_value)) return number_key(*d);
    if (const auto* b = std::get_if<bool>(&cell->typed_value)) return number_key(std::int64_t(*b ? 1 : 0));
    if (const auto* date = std::get_if<NativeDate>(&cell->typed_value)) {
      if (const auto exact = parse_timestamp_exact(date->value)) {
        key.kind = SortKey::Kind::kDate;
        key.date = *exact;
        return key;
      }
    }
    switch (cell->kind) {
      case AttributeKind::kInt: {
        std::int64_t i = 0;
        std::uint64_t u = 0;
        const auto& text = cell->value;
        if (std::from_chars(text.data(), text.data() + text.size(), i).ec == std::errc{} &&
            std::to_string(i) == text) return number_key(i);
        if (std::from_chars(text.data(), text.data() + text.size(), u).ec == std::errc{} &&
            std::to_string(u) == text) return number_key(u);
        break;
      }
      case AttributeKind::kFloat:
        try {
          std::size_t used = 0;
          const double d = std::stod(cell->value, &used);
          if (used == cell->value.size()) return std::isnan(d) ? key : number_key(d);
        } catch (const std::exception&) {}
        break;
      case AttributeKind::kBoolean:
        if (cell->value == "true" || cell->value == "True") return number_key(std::int64_t(1));
        if (cell->value == "false" || cell->value == "False") return number_key(std::int64_t(0));
        break;
      case AttributeKind::kDate:
        if (const auto exact = parse_timestamp_exact(cell->value)) {
          key.kind = SortKey::Kind::kDate;
          key.date = *exact;
          return key;
        }
        break;
      default: break;
    }
    key.kind = SortKey::Kind::kText;
    key.text = cell->value;
    return key;
  }
  if (selector == "time:timestamp") {
    const auto text = present(e.timestamp_state, e.timestamp);
    if (!text) return key;
    if (const auto exact = parse_timestamp_exact(*text)) {
      key.kind = SortKey::Kind::kDate;
      key.date = *exact;
      return key;
    }
    key.kind = SortKey::Kind::kText;
    key.text = *text;
    return key;
  }
  if (const auto text = value(e, trace, selector)) {
    key.kind = SortKey::Kind::kText;
    key.text = *text;
  }
  return key;
}

// Exact three-way comparison of two numbers of any supported kind.
int compare_numbers(const std::variant<std::int64_t, std::uint64_t, double>& a,
                    const std::variant<std::int64_t, std::uint64_t, double>& b) {
  struct Parts {
    bool is_double = false;
    double d = 0;
    bool negative = false;
    std::uint64_t magnitude = 0;
  };
  const auto parts = [](const std::variant<std::int64_t, std::uint64_t, double>& v) {
    Parts p;
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
      p.negative = *i < 0;
      p.magnitude = p.negative ? std::uint64_t(0) - static_cast<std::uint64_t>(*i)
                               : static_cast<std::uint64_t>(*i);
    } else if (const auto* u = std::get_if<std::uint64_t>(&v)) {
      p.magnitude = *u;
    } else {
      p.is_double = true;
      p.d = std::get<double>(v);
    }
    return p;
  };
  const auto pa = parts(a), pb = parts(b);
  const auto integers = [](const Parts& x, const Parts& y) {
    if (x.magnitude == 0 && y.magnitude == 0) return 0;
    if (x.negative != y.negative) return x.negative ? -1 : 1;
    if (x.magnitude == y.magnitude) return 0;
    return ((x.magnitude < y.magnitude) != x.negative) ? -1 : 1;
  };
  if (!pa.is_double && !pb.is_double) return integers(pa, pb);
  if (pa.is_double && pb.is_double) return pa.d < pb.d ? -1 : (pa.d > pb.d ? 1 : 0);
  const bool swapped = pa.is_double;
  const auto& integer = swapped ? pb : pa;
  const double d = swapped ? pa.d : pb.d;
  int result = 0;
  constexpr double kTwo64 = 18446744073709551616.0;
  if (std::fabs(d) >= kTwo64) {
    result = d > 0 ? -1 : 1;
  } else {
    // Compare with the integer part first; a fractional rest breaks ties.
    const double whole = std::trunc(d);
    Parts w;
    w.negative = whole < 0;
    w.magnitude = static_cast<std::uint64_t>(std::fabs(whole));
    result = integers(integer, w);
    if (result == 0 && whole != d) result = d > whole ? -1 : 1;
  }
  return swapped ? -result : result;
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

// The relation value pandas merges on: null-like keys (None, NaN, pd.NA or
// an absent value) join each other, unlike in grouping.
std::string relation_key(const Event& e, const Trace* trace, std::string_view selector) {
  if (const auto v = value(e, trace, selector)) return std::string(1, '\x01') + *v;
  return std::string(1, '\0');
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
  if (static_cast<int>(numbers) + static_cast<int>(texts) + static_cast<int>(dates) > 1)
    throw std::invalid_argument("network analysis: '<' not supported between mixed sorting values");
  std::stable_sort(refs.begin(), refs.end(), [](const Ref& a, const Ref& b) {
    const bool a_missing = a.sort.kind == SortKey::Kind::kMissing;
    const bool b_missing = b.sort.kind == SortKey::Kind::kMissing;
    if (a_missing || b_missing) return !a_missing && b_missing;
    switch (a.sort.kind) {
      case SortKey::Kind::kNumber: return compare_numbers(a.sort.number, b.sort.number) < 0;
      case SortKey::Kind::kDate:
        return std::pair(a.sort.date.seconds, a.sort.date.nanoseconds) <
               std::pair(b.sort.date.seconds, b.sort.date.nanoseconds);
      default: return a.sort.text < b.sort.text;
    }
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
  using Key = std::tuple<std::string, std::string, std::string>;
  std::map<Key, NetworkAnalysisEdge> grouped;
  for (std::size_t source_index = 0; source_index < refs.size(); ++source_index) {
    const auto& source = refs[source_index];
    const auto source_node = value(*source.event, source.trace, o.source_node_attribute);
    if (!source_node) continue;
    const Ref* target = successors[source_index];
    if (!target) continue;
    const auto target_node = value(*target->event, target->trace, o.target_node_attribute);
    const auto& edge_event = o.edge_reference == NetworkAnalysisEdgeReference::kOut ? source : *target;
    const auto edge_value = value(*edge_event.event, edge_event.trace, o.edge_attribute);
    if (!target_node || !edge_value) continue;
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
