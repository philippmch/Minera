#include "minera/pripel.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "minera/timestamp.hpp"
#include "minera/secure_random.hpp"

namespace minera {
namespace {

constexpr std::size_t kHardBound = 1000000;
constexpr std::size_t kAssignmentCellBound = 2000000;
constexpr std::size_t kNoMatch = std::numeric_limits<std::size_t>::max();

std::uint64_t hash_text(std::uint64_t seed, std::string_view text) {
  std::uint64_t h = 1469598103934665603ULL ^ seed;
  for (unsigned char c : text) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  h += 0x9e3779b97f4a7c15ULL;
  h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
  h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
  return h ^ (h >> 31);
}

double unit(std::uint64_t x) {
  return (static_cast<double>(x) + .5) /
         (static_cast<double>(std::numeric_limits<std::uint64_t>::max()) + 1.0);
}

std::size_t levenshtein(const Trace& a, const Trace& b, std::size_t cutoff) {
  const auto n = a.events.size(), m = b.events.size();
  if ((n > m && n - m > cutoff) || (m > n && m - n > cutoff))
    return cutoff + 1;
  std::vector<std::size_t> prev(m + 1), cur(m + 1);
  for (std::size_t j = 0; j <= m; ++j) prev[j] = j;
  for (std::size_t i = 1; i <= n; ++i) {
    cur[0] = i;
    std::size_t row_min = cur[0];
    for (std::size_t j = 1; j <= m; ++j) {
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1,
                         prev[j - 1] + (a.events[i - 1].activity !=
                                         b.events[j - 1].activity)});
      row_min = std::min(row_min, cur[j]);
    }
    if (row_min > cutoff) return cutoff + 1;
    std::swap(prev, cur);
  }
  return prev[m];
}

std::vector<std::size_t> greedy_matches(const EventLog& queries,
                                        const EventLog& source,
                                        std::size_t query_count,
                                        std::size_t max_distance) {
  std::vector<std::size_t> assignment(query_count, kNoMatch);
  std::vector<bool> used(source.traces.size(), false);
  for (std::size_t qi = 0; qi < query_count; ++qi) {
    std::size_t best_source = kNoMatch;
    std::size_t best_distance = max_distance + 1;
    for (std::size_t si = 0; si < source.traces.size(); ++si) {
      if (used[si]) continue;
      const auto distance =
          levenshtein(queries.traces[qi], source.traces[si], max_distance);
      if (distance < best_distance ||
          (distance == best_distance && si < best_source)) {
        best_distance = distance;
        best_source = si;
      }
    }
    if (best_source != kNoMatch && best_distance <= max_distance) {
      assignment[qi] = best_source;
      used[best_source] = true;
    }
  }
  return assignment;
}

// Hungarian assignment over a rectangular cost matrix. Dummy columns let a
// query remain unmatched when every source trace exceeds the distance bound.
std::vector<std::size_t> assignment_matches(const EventLog& queries,
                                             const EventLog& source,
                                             std::size_t query_count,
                                             std::size_t max_distance) {
  if (query_count == 0 || source.traces.empty())
    return std::vector<std::size_t>(query_count, kNoMatch);

  const auto source_count = source.traces.size();
  const auto columns = source_count + query_count;
  if (source_count > kAssignmentCellBound ||
      query_count > kAssignmentCellBound ||
      columns > kAssignmentCellBound ||
      query_count > kAssignmentCellBound / columns) {
    return greedy_matches(queries, source, query_count, max_distance);
  }

  const auto unmatched_cost = static_cast<std::int64_t>(max_distance) + 1;
  const auto rejected_cost = unmatched_cost + 1;
  std::vector<std::vector<std::int64_t>> costs(
      query_count, std::vector<std::int64_t>(columns, unmatched_cost));
  for (std::size_t qi = 0; qi < query_count; ++qi) {
    for (std::size_t si = 0; si < source_count; ++si) {
      const auto distance =
          levenshtein(queries.traces[qi], source.traces[si], max_distance);
      costs[qi][si] = distance <= max_distance
                          ? static_cast<std::int64_t>(distance)
                          : rejected_cost;
    }
  }

  // This implementation uses one-indexed arrays, matching the standard
  // shortest-augmenting-path formulation for rectangular assignment.
  const auto rows = query_count;
  const auto cols = columns;
  std::vector<std::int64_t> u(rows + 1), v(cols + 1);
  std::vector<std::size_t> p(cols + 1), way(cols + 1);
  constexpr std::int64_t kInfinity =
      std::numeric_limits<std::int64_t>::max() / 4;
  for (std::size_t i = 1; i <= rows; ++i) {
    p[0] = i;
    std::size_t column = 0;
    std::vector<std::int64_t> min_value(cols + 1, kInfinity);
    std::vector<bool> used(cols + 1, false);
    do {
      used[column] = true;
      const auto row = p[column];
      std::int64_t delta = kInfinity;
      std::size_t next_column = 0;
      for (std::size_t j = 1; j <= cols; ++j) {
        if (used[j]) continue;
        const auto current = costs[row - 1][j - 1] - u[row] - v[j];
        if (current < min_value[j]) {
          min_value[j] = current;
          way[j] = column;
        }
        if (min_value[j] < delta ||
            (min_value[j] == delta && j < next_column)) {
          delta = min_value[j];
          next_column = j;
        }
      }
      for (std::size_t j = 0; j <= cols; ++j) {
        if (used[j]) {
          u[p[j]] += delta;
          v[j] -= delta;
        } else {
          min_value[j] -= delta;
        }
      }
      column = next_column;
    } while (p[column] != 0);
    do {
      const auto previous = way[column];
      p[column] = p[previous];
      column = previous;
    } while (column != 0);
  }

  std::vector<std::size_t> assignment(query_count, kNoMatch);
  for (std::size_t j = 1; j <= cols; ++j) {
    if (p[j] == 0 || p[j] > query_count || j > source_count) continue;
    const auto qi = p[j] - 1;
    const auto distance = costs[qi][j - 1];
    if (distance <= static_cast<std::int64_t>(max_distance))
      assignment[qi] = j - 1;
  }
  return assignment;
}

bool blocked(std::string_view key, const std::set<std::string>& list) {
  if (list.contains(std::string(key))) return true;
  // Identity-bearing names are never released, even when callers forgot to
  // include them in the explicit blocklist.
  std::string lower(key);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower == "case_id" || lower == "caseid" || lower == "variant" ||
         lower == "trace_variant" || lower == "tracevariant" ||
         lower == "id" || lower.find("case") != std::string::npos ||
         lower.find("variant") != std::string::npos;
}

std::string json_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out.push_back(c);
  }
  return out;
}

struct NumericBounds { double lo = 0.0, hi = 0.0; bool valid = false; };

}  // namespace

std::string PripelDiagnostics::to_json() const {
  std::ostringstream out;
  out << "{\"epsilon\":" << std::setprecision(17) << epsilon
      << ",\"seed\":" << seed << ",\"query_trace_count\":"
      << query_trace_count << ",\"released_trace_count\":"
      << released_trace_count << ",\"exact_matches\":" << exact_matches
      << ",\"nearest_matches\":" << nearest_matches
      << ",\"synthesized_traces\":" << synthesized_traces
      << ",\"synthesized_events\":" << synthesized_events
      << ",\"secure_random\":" << (secure_random ? "true" : "false")
      << ",\"output_truncated\":" << (output_truncated ? "true" : "false")
      << ",\"message\":\"" << json_escape(message) << "\"}";
  return out.str();
}

PripelResult anonymize_pripel(const EventLog& source,
                              const EventLog& released_query,
                              const PripelOptions& options) {
  if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0)
    throw std::invalid_argument("pripel: epsilon must be finite and > 0");
  if (options.max_output_traces > kHardBound || options.max_distance > kHardBound)
    throw std::invalid_argument("pripel: safety bound exceeded");
  if (released_query.traces.empty())
    throw std::invalid_argument("pripel: released query must not be empty");
  for (const auto& trace : source.traces)
    if (trace.events.size() > kHardBound)
      throw std::invalid_argument("pripel: source trace safety bound exceeded");
  for (const auto& trace : released_query.traces)
    if (trace.events.size() > kHardBound)
      throw std::invalid_argument("pripel: query trace safety bound exceeded");

  PripelResult result;
  result.diagnostics.epsilon = options.epsilon;
  result.diagnostics.seed = options.seed;
  result.diagnostics.secure_random = options.secure_random;
  result.diagnostics.query_trace_count = released_query.traces.size();

  std::set<std::string> deny(options.blocklist.begin(), options.blocklist.end());
  const auto query_count = std::min(released_query.traces.size(),
                                    options.max_output_traces);
  result.diagnostics.output_truncated =
      query_count < released_query.traces.size();
  const auto matches = assignment_matches(released_query, source, query_count,
                                          options.max_distance);

  std::map<std::string, std::set<std::string>> categorical;
  std::map<std::string, NumericBounds> numeric;
  double time_lo = 0.0, time_hi = 0.0;
  bool have_time = false;
  for (const auto& trace : source.traces) {
    for (const auto& event : trace.events) {
      if (!event.resource.empty() && !blocked("resource", deny))
        categorical["resource"].insert(event.resource);
      if (auto ts = parse_timestamp(event.timestamp)) {
        if (!have_time) time_lo = time_hi = *ts;
        else { time_lo = std::min(time_lo, *ts); time_hi = std::max(time_hi, *ts); }
        have_time = true;
      }
      for (const auto& attr : event.attributes) {
        if (attr.is_null || blocked(attr.key, deny)) continue;
        if (attr.kind == AttributeKind::kInt || attr.kind == AttributeKind::kFloat) {
          try {
            const double value = std::stod(attr.value);
            auto& b = numeric[attr.key];
            if (!b.valid) b = {value, value, true};
            else { b.lo = std::min(b.lo, value); b.hi = std::max(b.hi, value); }
          } catch (...) { categorical[attr.key].insert(attr.value); }
        } else categorical[attr.key].insert(attr.value);
      }
    }
  }

  auto random_value = [&](std::uint64_t salt, std::string_view text) {
    return options.secure_random ? secure_random_u64()
                                 : hash_text(options.seed + salt, text);
  };

  auto randomized = [&](std::string value, std::string_view key,
                        std::uint64_t salt, bool binary = false) {
    auto it = categorical.find(std::string(key));
    if (it == categorical.end() || it->second.size() < 2) return value;
    const auto& domain = it->second;
    // Categorical utility has unit sensitivity; the exponential mechanism
    // therefore uses epsilon/2 in the keep-vs-replace probability.
    const double utility_exponent =
        binary ? options.epsilon : options.epsilon / 2.0;
    const double keep = utility_exponent > 40.0
                            ? 1.0
                            : std::exp(utility_exponent) /
                                  (std::exp(utility_exponent) +
                                   static_cast<double>(domain.size() - 1));
    if (unit(random_value(salt, value)) < keep) return value;
    const std::size_t index = static_cast<std::size_t>(
        random_value(salt + 17, value) % (domain.size() - 1));
    std::size_t i = 0;
    for (const auto& candidate : domain) {
      if (candidate == value) continue;
      if (i++ == index) return candidate;
    }
    return value;
  };

  auto next_numeric = [&](const std::string& key, const std::string& text,
                          std::uint64_t salt) {
    auto it = numeric.find(key);
    if (it == numeric.end() || !it->second.valid) return text;
    try {
      const auto b = it->second;
      double v = std::stod(text);
      const double span = b.hi - b.lo;
      if (span > 0.0) {
        const double noise = (unit(random_value(salt, text)) - .5) *
                             (span / options.epsilon);
        v = std::clamp(v + noise, b.lo, b.hi);
      }
      std::ostringstream s;
      s << std::setprecision(12) << v;
      return s.str();
    } catch (...) { return text; }
  };

  std::size_t emitted = 0;
  for (std::size_t qi = 0; qi < query_count; ++qi) {
    if (emitted >= options.max_output_traces) { result.diagnostics.output_truncated = true; break; }
    const auto& query = released_query.traces[qi];
    const auto source_index = matches[qi];
    if (source_index != kNoMatch) {
      const auto distance =
          levenshtein(query, source.traces[source_index], options.max_distance);
      if (distance == 0)
        ++result.diagnostics.exact_matches;
      else
        ++result.diagnostics.nearest_matches;
    }

    Trace out_trace;
    out_trace.case_id = "pripel-" + std::to_string(emitted + 1);
    const Trace* source_trace = source_index == kNoMatch
                                    ? nullptr : &source.traces[source_index];
    std::vector<bool> used(source_trace ? source_trace->events.size() : 0, false);
    out_trace.events.reserve(query.events.size());
    double previous_time = -std::numeric_limits<double>::infinity();
    for (std::size_t ei = 0; ei < query.events.size(); ++ei) {
      const auto& qe = query.events[ei];
      const Event* matched = nullptr;
      if (source_trace) {
        if (ei < source_trace->events.size() && !used[ei] &&
            source_trace->events[ei].activity == qe.activity) {
          matched = &source_trace->events[ei]; used[ei] = true;
        } else {
          for (std::size_t si = 0; si < source_trace->events.size(); ++si)
            if (!used[si] && source_trace->events[si].activity == qe.activity) {
              matched = &source_trace->events[si]; used[si] = true; break;
            }
        }
      }
      Event out;
      out.case_id = out_trace.case_id;
      out.activity = qe.activity;
      out.input_order = ei;
      if (matched) {
        out.timestamp = matched->timestamp;
        out.resource = matched->resource;
        out.attributes = matched->attributes;
        out.lifecycle = matched->lifecycle;
      } else {
        ++result.diagnostics.synthesized_events;
        out.timestamp = (ei == 0 ? "1970-01-01T00:00:00.000Z" : out_trace.events.back().timestamp);
        out.resource.clear();
      }
      if (blocked("resource", deny)) out.resource.clear();
      else if (!out.resource.empty())
        out.resource = randomized(out.resource, "resource", (emitted + 1) * 1009 + ei + 31);
      std::vector<EventAttribute> attrs;
      for (auto attr : out.attributes) {
        if (blocked(attr.key, deny)) continue;
        if (attr.is_null) {
          attrs.push_back(std::move(attr));
          continue;
        }
        if (attr.kind == AttributeKind::kInt || attr.kind == AttributeKind::kFloat)
          attr.value = next_numeric(attr.key, attr.value, (emitted + 1) * 1009 + ei + 101);
        else attr.value = randomized(
            std::move(attr.value), attr.key, (emitted + 1) * 1009 + ei + 101,
            attr.kind == AttributeKind::kBoolean);
        attrs.push_back(std::move(attr));
      }
      out.attributes = std::move(attrs);
      if (auto ts = parse_timestamp(out.timestamp)) {
        double t = *ts;
        if (have_time && time_hi > time_lo)
          t = std::clamp(t + (unit(random_value(333,
                                              std::to_string((emitted + 1) * 1009 + ei))) - .5) *
                                 ((time_hi - time_lo) / options.epsilon), time_lo, time_hi);
        if (t < previous_time) t = previous_time;
        previous_time = t;
        out.timestamp = format_timestamp(t);
      }
      out_trace.events.push_back(std::move(out));
    }
    if (!source_trace) ++result.diagnostics.synthesized_traces;
    result.log.traces.push_back(std::move(out_trace));
    ++emitted;
  }
  result.diagnostics.released_trace_count = emitted;
  result.diagnostics.message = result.diagnostics.output_truncated ?
      "output safety bound applied" : "ok";
  return result;
}

}  // namespace minera
