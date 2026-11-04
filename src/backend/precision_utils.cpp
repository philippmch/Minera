#include "minera/precision_utils.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <system_error>
#include <tuple>

namespace minera {
namespace {

using Activity = std::optional<std::string>;  // nullopt is None

// trace[key] of every event; KeyError when an event lacks the key.
Activity event_activity(const Event& event, const std::string& key) {
  if (key == "concept:name") {
    if (!event.has_activity()) throw PrecisionUtilsError("KeyError", "'" + key + "'");
    if (event.activity_state == FirstClassAttributeState::kNull) return std::nullopt;
    return event.activity;
  }
  for (const auto& attribute : event.attributes) {
    if (attribute.key != key) continue;
    if (attribute.is_null) return std::nullopt;
    return attribute.value;
  }
  throw PrecisionUtilsError("KeyError", "'" + key + "'");
}

// A groupby key: integer ids compare as exact int64 values (pandas keeps
// ids past 2^53 apart), float ids as doubles, and an integer equals a float
// of the same value. A long double key was exact only on x86; on ARM and
// MSVC it is a double and merged large integer ids.
struct CaseKey {
  bool numeric = false;
  bool integral = false;
  std::int64_t integer = 0;
  double real = 0.0;
  std::string text;
};

CaseKey numeric_case_key(const std::string& value) {
  CaseKey key;
  key.numeric = true;
  std::int64_t integer = 0;
  const auto* end = value.data() + value.size();
  if (const auto parsed = std::from_chars(value.data(), end, integer);
      parsed.ec == std::errc{} && parsed.ptr == end) {
    key.integral = true;
    key.integer = integer;
  } else {
    key.real = std::strtod(value.c_str(), nullptr);
  }
  return key;
}

// -1, 0 or 1 for integer `lhs` against double `rhs`, exactly; NaN is last.
int compare_integer_real(std::int64_t lhs, double rhs) {
  if (std::isnan(rhs)) return -1;
  if (rhs >= 0x1p63) return -1;
  if (rhs < -0x1p63) return 1;
  const double whole = std::trunc(rhs);
  const auto truncated = static_cast<std::int64_t>(whole);
  if (lhs != truncated) return lhs < truncated ? -1 : 1;
  const double fraction = rhs - whole;
  return fraction > 0 ? -1 : fraction < 0 ? 1 : 0;
}

int compare_reals(double lhs, double rhs) {
  const bool left_nan = std::isnan(lhs), right_nan = std::isnan(rhs);
  if (left_nan || right_nan) return left_nan == right_nan ? 0 : left_nan ? 1 : -1;
  return lhs < rhs ? -1 : rhs < lhs ? 1 : 0;
}

struct CaseKeyLess {
  bool operator()(const CaseKey& lhs, const CaseKey& rhs) const {
    if (lhs.numeric != rhs.numeric) return lhs.numeric < rhs.numeric;
    if (!lhs.numeric) return lhs.text < rhs.text;
    int order = 0;
    if (lhs.integral && rhs.integral)
      order = lhs.integer < rhs.integer ? -1 : rhs.integer < lhs.integer ? 1 : 0;
    else if (lhs.integral)
      order = compare_integer_real(lhs.integer, rhs.real);
    else if (rhs.integral)
      order = -compare_integer_real(rhs.integer, lhs.real);
    else
      order = compare_reals(lhs.real, rhs.real);
    return order < 0;
  }
};

PrecisionLogPrefixes prefixes_of(const std::vector<std::vector<Activity>>& traces) {
  PrecisionLogPrefixes result;
  std::map<std::string, std::size_t> position;
  for (const auto& trace : traces) {
    std::string prefix;
    for (std::size_t i = 1; i < trace.size(); ++i) {
      if (!trace[i - 1])
        throw PrecisionUtilsError("TypeError",
                                  "sequence item " + std::to_string(i - 1) + ": expected str instance, NoneType found");
      prefix += (i > 1 ? "," : "") + *trace[i - 1];
      auto [found, inserted] = position.emplace(prefix, result.prefixes.size());
      if (inserted) {
        result.prefixes.push_back({prefix, {}});
        result.counts.push_back({prefix, 0});
      }
      auto& next = result.prefixes[found->second].second;
      if (std::find(next.begin(), next.end(), trace[i]) == next.end()) {
        next.push_back(trace[i]);
        std::sort(next.begin(), next.end());
      }
      ++result.counts[found->second].second;
    }
  }
  return result;
}

std::size_t column(const EventTable& table, const std::string& name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) throw PrecisionUtilsError("KeyError", "'" + name + "'");
  return static_cast<std::size_t>(found - table.columns.begin());
}

bool is_null(const std::optional<EventAttribute>& cell) {
  if (!cell || cell->is_null || cell->kind == AttributeKind::kNA) return true;
  const auto* real = std::get_if<double>(&cell->typed_value);
  return real && std::isnan(*real);
}

}  // namespace

PrecisionLogPrefixes precision_log_prefixes(const EventLog& log, const std::string& activity_key) {
  std::vector<std::vector<Activity>> traces;
  for (const auto& trace : log.traces) {
    auto& activities = traces.emplace_back();
    for (const auto& event : trace.events) activities.push_back(event_activity(event, activity_key));
  }
  return prefixes_of(traces);
}

PrecisionLogPrefixes precision_log_prefixes(const EventTable& table, const std::string& activity_key,
                                            const std::string& case_id_key) {
  const auto case_column = column(table, case_id_key);
  const auto activity_column = column(table, activity_key);
  // Numeric case ids order by value, others as text; null cases are dropped.
  std::map<CaseKey, std::vector<Activity>, CaseKeyLess> cases;
  for (const auto& row : table.rows) {
    const auto& cell = row[case_column];
    if (is_null(cell)) continue;
    const bool numeric = cell->kind == AttributeKind::kInt || cell->kind == AttributeKind::kFloat;
    const CaseKey key = numeric ? numeric_case_key(cell->value) : CaseKey{false, false, 0, 0.0, cell->value};
    const auto& activity = row[activity_column];
    cases[key].push_back(is_null(activity) ? Activity{} : Activity{activity->value});
  }
  std::vector<std::vector<Activity>> traces;
  for (auto& [key, activities] : cases) traces.push_back(std::move(activities));
  return prefixes_of(traces);
}

EventLog precision_fake_log(const std::vector<std::string>& prefixes, const std::string& activity_key) {
  EventLog log;
  for (const auto& prefix : prefixes) {
    auto& trace = log.traces.emplace_back();
    std::size_t from = 0;
    while (true) {
      const auto to = prefix.find(',', from);
      const auto part = prefix.substr(from, to == std::string::npos ? std::string::npos : to - from);
      Event event;
      if (activity_key == "concept:name") {
        event.activity = part;
        event.activity_state = FirstClassAttributeState::kPresent;
      } else {
        EventAttribute attribute;
        attribute.key = activity_key;
        attribute.value = part;
        event.attributes.push_back(attribute);
      }
      trace.events.push_back(std::move(event));
      if (to == std::string::npos) break;
      from = to + 1;
    }
  }
  return log;
}

std::optional<std::vector<PrecisionMarking>> precision_sync_search(const PrecisionSyncNet& net,
                                                                   const PrecisionMarking& initial,
                                                                   const PrecisionMarking& stop,
                                                                   const std::string& skip) {
  using Dense = std::vector<std::size_t>;
  const auto dense = [&](const PrecisionMarking& marking) {
    Dense out(net.places, 0);
    for (const auto& [place, tokens] : marking) out.at(place) += tokens;
    return out;
  };
  const auto sparse = [](const Dense& marking) {
    PrecisionMarking out;
    for (std::size_t place = 0; place < marking.size(); ++place)
      if (marking[place]) out.push_back({place, marking[place]});
    return out;
  };
  // Synchronous moves and invisible model moves only.
  std::vector<const PrecisionSyncTransition*> moves;
  for (const auto& transition : net.transitions) {
    if (transition.model_label == skip) continue;
    if (transition.trace_label == skip && transition.model_label) continue;
    moves.push_back(&transition);
  }
  const auto target = dense(stop);
  using Entry = std::tuple<double, std::size_t, Dense>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
  std::size_t pushed = 0;
  queue.push({0.0, pushed++, dense(initial)});
  std::set<Dense> closed, found;
  std::optional<double> best;
  while (!queue.empty()) {
    auto [cost, order, marking] = queue.top();
    queue.pop();
    if (best && cost > *best) break;
    if (!closed.insert(marking).second) continue;
    bool covers = true;
    for (std::size_t place = 0; place < marking.size() && covers; ++place) covers = marking[place] >= target[place];
    if (covers) {
      found.insert(marking);
      best = cost;
      continue;
    }
    for (const auto* move : moves) {
      bool enabled = true;
      for (const auto& [place, weight] : move->in) enabled = enabled && marking[place] >= weight;
      if (!enabled) continue;
      auto next = marking;
      for (const auto& [place, weight] : move->in) next[place] -= weight;
      for (const auto& [place, weight] : move->out) next[place] += weight;
      queue.push({cost + move->cost, pushed++, std::move(next)});
    }
  }
  if (found.empty()) return std::nullopt;
  std::vector<PrecisionMarking> result;
  for (const auto& marking : found) result.push_back(sparse(marking));
  std::sort(result.begin(), result.end());
  return result;
}

}  // namespace minera
