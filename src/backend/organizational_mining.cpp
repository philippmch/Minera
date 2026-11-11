#include "minera/organizational_mining.hpp"

#include "frame_cells.hpp"
#include "numpy_object_sort.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

constexpr std::size_t kHardResources = 10000;
constexpr std::size_t kHardEdges = 1000000;
constexpr std::size_t kHardGap = 10000;

using Pair = std::pair<std::size_t, std::size_t>;

std::string selected_value(const Event& event, const std::string& key,
                           bool resource) {
  if (resource && (key.empty() || key == "org:resource" || key == "resource"))
    return event.resource;
  if (!resource && (key.empty() || key == "concept:name" || key == "activity"))
    return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

// Reads one event's value of a key: the classic selectors above, or on the
// DataFrame path the frame's typed cell (nullopt for a missing value).
using Selector = std::function<std::optional<std::string>(
    const Trace&, const Event&, const std::string&, bool)>;

bool handover_variant(OrganizationalSnaVariant variant) {
  return variant == OrganizationalSnaVariant::kHandover ||
         variant == OrganizationalSnaVariant::kHandoverPandas;
}
bool subcontracting_variant(OrganizationalSnaVariant variant) {
  return variant == OrganizationalSnaVariant::kSubcontracting ||
         variant == OrganizationalSnaVariant::kSubcontractingPandas;
}

bool case_id_variant(OrganizationalSnaVariant variant) {
  return variant == OrganizationalSnaVariant::kHandoverPandas ||
         variant == OrganizationalSnaVariant::kWorkingTogetherPandas ||
         variant == OrganizationalSnaVariant::kSubcontractingPandas;
}

std::vector<std::size_t> resource_ids(
    const Trace& trace, const std::unordered_map<std::string, std::size_t>& ids,
    const std::string& resource_attribute, const Selector& select) {
  std::vector<std::size_t> result;
  for (const auto& event : trace.events) {
    const auto resource = select(trace, event, resource_attribute, true);
    if (!resource) continue;
    const auto it = ids.find(*resource);
    if (it != ids.end()) result.push_back(it->second);
  }
  return result;
}

bool default_case_id_attribute(const std::string& key) {
  return key.empty() || key == "case_id" || key == "case:id" ||
         key == "case:concept:name";
}

std::optional<std::string> selected_case_id(const Trace& trace,
                                            const Event& event,
                                            const std::string& key) {
  if (default_case_id_attribute(key))
    return trace.case_id.empty() ? event.case_id : trace.case_id;
  if (const auto* value = trace.attribute(key)) return *value;
  if (const auto* value = event.attribute(key)) return *value;
  return std::nullopt;
}

std::vector<Trace> regroup_traces(const EventLog& log,
                                  const std::string& case_id_attribute,
                                  bool frame) {
  if (frame ? case_id_attribute == "case:concept:name"
            : default_case_id_attribute(case_id_attribute))
    return log.traces;
  std::vector<Trace> grouped;
  std::unordered_map<std::string, std::size_t> positions;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      std::optional<std::string> case_id;
      if (frame) {
        // groupby leaves out the rows whose key is NaN.
        const auto value = frame_value(log, trace, event, case_id_attribute, FrameKeyScope::kDataFrame);
        if (value.missing()) continue;
        case_id = value.text;
      } else {
        case_id = selected_case_id(trace, event, case_id_attribute);
      }
      if (!case_id)
        throw std::invalid_argument(
            "organizational SNA case selector is missing: " +
            case_id_attribute);
      auto [it, inserted] = positions.emplace(*case_id, grouped.size());
      if (inserted) grouped.push_back(Trace{*case_id, {}, {}});
      grouped[it->second].events.push_back(event);
    }
  }
  for (auto& trace : grouped)
    for (auto& event : trace.events) event.case_id = trace.case_id;
  return grouped;
}

void emit_edges(const std::map<Pair, double>& weights,
                OrganizationalSnaResult& result,
                std::size_t max_edges) {
  for (const auto& [pair, weight] : weights) {
    if (result.edges.size() >= max_edges) {
      result.diagnostics.output_truncated = true;
      break;
    }
    if (std::isinf(weight)) continue;
    result.edges.push_back({pair.first, pair.second, weight});
  }
}

// sorted() over a column's distinct values, as every SNA variant but the
// joint-activities pandas one sorts its resources: Python's TypeError of
// the first comparison it cannot make. pm4py sorts a set, whose order
// follows the hash seed (any pair of unorderable types can come first);
// Minera sorts the values in their first appearance. A NaN cell is a float;
// a datetime column's NaT orders with nothing and never raises; an object
// column's dates are datetime.datetime (round BC, observed black-box).
void require_sortable(const std::vector<python::Scalar>& values, bool object_column) {
  std::vector<python::Scalar> distinct;
  std::set<std::string> seen;
  for (const auto& value : values)
    if (seen.insert(python::identity(value)).second) distinct.push_back(value);
  try {
    python::sort(distinct, [](const python::Scalar& a, const python::Scalar& b) { return python::less(a, b); });
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(
        "TypeError: " + column_type_error(error.what(), object_column ? std::optional(DataFrameColumnType::kObject)
                                                                      : std::nullopt));
  }
}

// The handover, working-together and subcontracting pandas variants group
// the cases' resource sequences (one tuple a case, the cases in the case
// column's sorted order) and sort the distinct ones with numpy's argsort,
// which compares tuples as Python does (the first unequal items decide; a
// missing cell is NaN). When a comparison raises, pandas sorts them again
// position by position, each position an ordered Categorical, which fails
// when a position's values (NaN left out; text sorts apart from the rest)
// hold a date beside a number (round BC, observed black-box).
void require_sortable_variants(const std::vector<std::vector<python::Scalar>>& sequences) {
  std::vector<const std::vector<python::Scalar>*> distinct;
  std::set<std::vector<std::string>> seen;
  for (const auto& sequence : sequences) {
    std::vector<std::string> identity;
    for (const auto& value : sequence) identity.push_back(python::identity(value));
    if (seen.insert(std::move(identity)).second) distinct.push_back(&sequence);
  }
  const auto tuple_compare = [&](std::size_t x, std::size_t y) {
    const auto& a = *distinct[x];
    const auto& b = *distinct[y];
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
      if (!python::equal(a[i], b[i])) return python::compare(a[i], b[i]);
    return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
  };
  try {
    (void)detail::numpy_object_quick_argsort(distinct.size(), tuple_compare);
    return;
  } catch (const python::TypeError&) {
  }
  std::size_t longest = 0;
  for (const auto* sequence : distinct) longest = std::max(longest, sequence->size());
  for (std::size_t position = 0; position < longest; ++position) {
    std::vector<python::Scalar> values;
    std::set<std::string> present;
    for (const auto* sequence : distinct)
      if (position < sequence->size() && !python::is_missing((*sequence)[position]) &&
          present.insert(python::identity((*sequence)[position])).second)
        values.push_back((*sequence)[position]);
    try {
      (void)python::safe_sort_order(values);
    } catch (const python::TypeError&) {
      throw std::invalid_argument(
          "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
          "categories argument.");
    }
  }
}

}  // namespace

OrganizationalSnaResult discover_organizational_sna(
    const EventLog& input, const OrganizationalSnaOptions& options) {
  if (!std::isfinite(options.beta) || options.beta < 0.0 || options.beta > 1.0)
    throw std::invalid_argument("organizational SNA beta must be finite and in [0,1]");
  if (options.n == 0 || options.n > kHardGap)
    throw std::invalid_argument("organizational SNA n must be in [1,10000]");
  if (options.max_resources == 0 || options.max_resources > kHardResources)
    throw std::invalid_argument("organizational SNA max_resources safety bound exceeded");
  if (options.max_edges == 0 || options.max_edges > kHardEdges)
    throw std::invalid_argument("organizational SNA max_edges safety bound exceeded");
  if (!default_case_id_attribute(options.case_id_attribute) &&
      !case_id_variant(options.variant))
    throw std::invalid_argument(
        "organizational SNA case selector requires a handover, working-together, or subcontracting pandas variant");

  // A log read on the DataFrame path (CSV and XES uploads) is read as
  // pm4py reads its frame: keys are column names (no org:resource alias), a
  // key the frame lacks raises KeyError, cells are typed, and sorting the
  // resources (every variant but the joint-activities pandas one, which
  // drops NaN first) raises TypeError when text sits beside numbers or NaN
  // (observed black-box). A NaN cell is left out: pm4py keys each NaN cell
  // as its own resource (float identity), which no JSON name can tell apart.
  const bool frame = input.dataframe_columns != nullptr;
  std::optional<EventLog> frame_storage;
  if (frame) frame_storage = dataframe_frame_event_log(input);
  const EventLog& log = frame ? *frame_storage : input;
  const bool pandas = options.variant == OrganizationalSnaVariant::kHandoverPandas ||
                      options.variant == OrganizationalSnaVariant::kWorkingTogetherPandas ||
                      options.variant == OrganizationalSnaVariant::kSubcontractingPandas ||
                      options.variant == OrganizationalSnaVariant::kJointActivitiesPandas;
  const auto scope = pandas ? FrameKeyScope::kDataFrame : FrameKeyScope::kEventLog;
  const bool joint = options.variant == OrganizationalSnaVariant::kJointActivities ||
                     options.variant == OrganizationalSnaVariant::kJointActivitiesPandas;
  const bool drop_missing = options.variant == OrganizationalSnaVariant::kJointActivitiesPandas;
  // A datetime column (an XES attribute holding only dates) keeps its
  // missing cells as pd.NaT, one object: pm4py counts them as one resource
  // (or activity) NaT in every variant but the joint-activities pandas one,
  // which drops them (round BC, observed black-box).
  const auto nat_cells = [&](const std::string& key) {
    return frame && !drop_missing && frame_column_type(log, key) == DataFrameColumnType::kDate;
  };
  const bool nat_resource = nat_cells(options.resource_attribute);
  const bool nat_activity = joint && nat_cells(options.activity_attribute);
  // Resources and activities are separate dicts to pm4py.
  FrameNames frame_names, frame_activity_names;
  const Selector select = [&](const Trace& trace, const Event& event, const std::string& key,
                              bool resource) -> std::optional<std::string> {
    if (frame) {
      const auto value = frame_value(log, trace, event, key, scope);
      if (value.missing()) {
        if (resource ? nat_resource : nat_activity) return std::string("NaT");
        return std::nullopt;
      }
      return (resource ? frame_names : frame_activity_names).name(value);
    }
    auto value = selected_value(event, key, resource);
    if (value.empty()) return std::nullopt;
    return value;
  };
  if (frame) {
    const auto keys = frame_keys(log, scope);
    if (case_id_variant(options.variant)) {
      // These variants group the frame by the case column first, then
      // select the resource column of the groups (pandas' 'Column not
      // found'; round BC, observed black-box).
      require_frame_key(keys, options.case_id_attribute);
      if (!keys.contains(options.resource_attribute))
        throw std::invalid_argument("KeyError: 'Column not found: " + options.resource_attribute + "'");
    } else {
      require_frame_key(keys, options.resource_attribute);
      if (joint) require_frame_key(keys, options.activity_attribute);
    }
    const auto cell = [&](const Trace& trace, const Event& event, const std::string& key) {
      return frame_scalar(log, trace, event, key, scope);
    };
    // A missing cell sorts as NaN, except a datetime column's NaT, which
    // orders with nothing.
    const auto sorted_cell = [&](const std::optional<python::Scalar>& value, const std::string& key)
        -> std::optional<python::Scalar> {
      if (value) return value;
      if (frame_column_type(log, key) == DataFrameColumnType::kDate) return std::nullopt;
      return python::real(std::numeric_limits<double>::quiet_NaN());
    };
    if (case_id_variant(options.variant) &&
        frame_column_type(log, options.resource_attribute) != DataFrameColumnType::kDate) {
      // A datetime column's Timestamps and NaT always compare.
      std::vector<std::vector<python::Scalar>> case_keys;
      std::vector<python::Scalar> resources;
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events)
          if (const auto case_cell = frame_row_scalar(log, trace, event, options.case_id_attribute)) {
            case_keys.push_back({*case_cell});
            resources.push_back(cell(trace, event, options.resource_attribute)
                                    .value_or(python::real(std::numeric_limits<double>::quiet_NaN())));
          }
      std::vector<std::vector<python::Scalar>> sequences;
      try {
        for (const auto& group : python::groupby(case_keys)) {
          auto& sequence = sequences.emplace_back();
          for (const auto row : group.rows) sequence.push_back(resources[row]);
        }
      } catch (const python::TypeError&) {
        sequences.clear();  // the case column's own sort error is not modelled here
      }
      require_sortable_variants(sequences);
    }
    std::vector<python::Scalar> resource_values, activity_values;
    // The case-grouped pandas variants sort the resources of the rows their
    // groupby keeps: a row without a case is left out (round BD, observed
    // black-box).
    const bool grouped = case_id_variant(options.variant);
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events) {
        if (grouped && !frame_row_scalar(log, trace, event, options.case_id_attribute)) continue;
        const auto resource = cell(trace, event, options.resource_attribute);
        if (joint) {
          const auto activity = cell(trace, event, options.activity_attribute);
          if (drop_missing && (!resource || !activity)) continue;
          if (const auto value = sorted_cell(activity, options.activity_attribute)) activity_values.push_back(*value);
        }
        if (drop_missing && !resource) continue;
        if (const auto value = sorted_cell(resource, options.resource_attribute)) resource_values.push_back(*value);
      }
    const auto object_column = [&](const std::string& key) {
      return frame_column_type(log, key) == DataFrameColumnType::kObject;
    };
    require_sortable(resource_values, object_column(options.resource_attribute));
    if (joint) require_sortable(activity_values, object_column(options.activity_attribute));
  }

  OrganizationalSnaResult result;
  result.variant = options.variant;
  result.directed = handover_variant(options.variant) ||
                    subcontracting_variant(options.variant);
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();
  std::set<std::string> names;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events)
      if (const auto resource = select(trace, event, options.resource_attribute, true))
        names.insert(*resource);
  if (names.size() > options.max_resources)
    throw std::invalid_argument("organizational SNA resource bound exceeded");
  result.resources.assign(names.begin(), names.end());
  result.diagnostics.resource_count = result.resources.size();
  std::unordered_map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < result.resources.size(); ++i)
    ids.emplace(result.resources[i], i);

  std::map<Pair, double> weights;
  const auto traces = case_id_variant(options.variant)
                          ? regroup_traces(log, options.case_id_attribute, frame)
                          : log.traces;
  switch (options.variant) {
    case OrganizationalSnaVariant::kHandover:
    case OrganizationalSnaVariant::kHandoverPandas: {
      std::map<std::vector<std::optional<std::string>>, std::size_t> variants;
      for (const auto& trace : traces) {
        std::vector<std::optional<std::string>> sequence;
        for (const auto& event : trace.events)
          sequence.push_back(select(trace, event, options.resource_attribute, true));
        ++variants[std::move(sequence)];
      }
      double dividend = 0.0;
      for (const auto& [sequence, occurrence] : variants) {
        for (std::size_t i = 0; i + 1 < sequence.size(); ++i) {
          for (std::size_t j = i + 1; j < sequence.size(); ++j) {
            const double weight = options.beta == 0.0
                                      ? 1.0
                                      : std::pow(options.beta,
                                                 static_cast<double>(j - i - 1));
            dividend += weight * occurrence;
            if (sequence[i] && sequence[j]) {
              const auto source = ids.find(*sequence[i]);
              const auto target = ids.find(*sequence[j]);
              if (source != ids.end() && target != ids.end())
                weights[{source->second, target->second}] += weight * occurrence;
            }
            if (options.beta == 0.0) break;
          }
        }
      }
      if (dividend > 0.0)
        for (auto& [pair, weight] : weights) { (void)pair; weight /= dividend; }
      break;
    }
    case OrganizationalSnaVariant::kWorkingTogether:
    case OrganizationalSnaVariant::kWorkingTogetherPandas: {
      if (!traces.empty()) {
        // The log variant divides by len(EventLog) (cases); the pandas variant
        // divides by len(DataFrame), i.e. the number of event rows.
        const double denominator =
            options.variant == OrganizationalSnaVariant::kWorkingTogetherPandas
                ? static_cast<double>(log.event_count())
                : static_cast<double>(traces.size());
        const double normalizer = 1.0 / denominator;
        for (const auto& trace : traces) {
          auto events = resource_ids(trace, ids, options.resource_attribute, select);
          std::sort(events.begin(), events.end());
          events.erase(std::unique(events.begin(), events.end()), events.end());
          for (std::size_t i = 0; i < events.size(); ++i) {
            for (std::size_t j = i + 1; j < events.size(); ++j) {
              weights[{events[i], events[j]}] += normalizer;
              weights[{events[j], events[i]}] += normalizer;
            }
          }
        }
      }
      break;
    }
    case OrganizationalSnaVariant::kSubcontracting:
    case OrganizationalSnaVariant::kSubcontractingPandas: {
      // The reference iterates resource variants (first appearance for the
      // log variant; descending (count, variant) for the pandas variant) and
      // only records the intermediaries of a source resource the first time
      // that source closes an n-window anywhere in the log: later windows of
      // an already-seen source add nothing. The divisor is the case count.
      std::vector<std::pair<std::vector<std::optional<std::string>>, std::size_t>> variants;
      std::map<std::vector<std::optional<std::string>>, std::size_t> positions;
      for (const auto& trace : traces) {
        std::vector<std::optional<std::string>> sequence;
        for (const auto& event : trace.events)
          sequence.push_back(select(trace, event, options.resource_attribute, true));
        const auto [it, inserted] = positions.emplace(sequence, variants.size());
        if (inserted) variants.push_back({std::move(sequence), 0});
        ++variants[it->second].second;
      }
      if (options.variant == OrganizationalSnaVariant::kSubcontractingPandas) {
        // A frame's resources compare as their typed values (numbers by value).
        const auto element_less = [&](const std::optional<std::string>& left,
                                      const std::optional<std::string>& right) {
          if (!left || !right) return !left && right.has_value();
          return frame ? frame_names.less(*left, *right) : *left < *right;
        };
        std::sort(variants.begin(), variants.end(),
                  [&](const auto& left, const auto& right) {
                    if (left.second != right.second) return left.second > right.second;
                    return std::lexicographical_compare(right.first.begin(), right.first.end(),
                                                        left.first.begin(), left.first.end(),
                                                        element_less);
                  });
      }
      std::set<std::size_t> seen_sources;
      for (const auto& [events, occurrence] : variants) {
        for (std::size_t i = 0; i + options.n < events.size(); ++i) {
          if (!events[i] || events[i] != events[i + options.n]) continue;
          const auto source = ids.find(*events[i]);
          if (source == ids.end()) continue;
          if (!seen_sources.insert(source->second).second) continue;
          for (std::size_t j = i + 1; j < i + options.n; ++j) {
            if (!events[j]) continue;
            const auto intermediary = ids.find(*events[j]);
            if (intermediary != ids.end())
              weights[{source->second, intermediary->second}] +=
                  static_cast<double>(occurrence);
          }
        }
      }
      if (!traces.empty())
        for (auto& [pair, weight] : weights) { (void)pair; weight /= traces.size(); }
      break;
    }
    case OrganizationalSnaVariant::kJointActivities:
    case OrganizationalSnaVariant::kJointActivitiesPandas: {
      std::set<std::string> activity_names;
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events)
          if (const auto activity = select(trace, event, options.activity_attribute, false))
            activity_names.insert(*activity);
      std::vector<std::string> activities(activity_names.begin(), activity_names.end());
      std::unordered_map<std::string, std::size_t> activity_ids;
      for (std::size_t i = 0; i < activities.size(); ++i)
        activity_ids.emplace(activities[i], i);
      std::vector<std::vector<double>> vectors(result.resources.size(),
                                                std::vector<double>(activities.size()));
      for (const auto& trace : log.traces)
        for (const auto& event : trace.events) {
          const auto resource = select(trace, event, options.resource_attribute, true);
          const auto activity = select(trace, event, options.activity_attribute, false);
          if (!resource || !activity) continue;
          const auto ri = ids.find(*resource);
          const auto ai = activity_ids.find(*activity);
          if (ri != ids.end() && ai != activity_ids.end()) vectors[ri->second][ai->second] += 1.0;
        }
      // pearsonr rejects vectors shorter than two activities (scipy's
      // message; round BC, observed black-box).
      if (vectors.size() > 1 && activities.size() < 2)
        throw std::invalid_argument("ValueError: `x` and `y` must have length at least 2.");
      for (std::size_t a = 0; a < vectors.size(); ++a) {
        for (std::size_t b = a + 1; b < vectors.size(); ++b) {
          const auto& x = vectors[a];
          const auto& y = vectors[b];

          const double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
          const double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / y.size();
          double numerator = 0.0, xx = 0.0, yy = 0.0;
          for (std::size_t i = 0; i < x.size(); ++i) {
            const double dx = x[i] - mean_x, dy = y[i] - mean_y;
            numerator += dx * dy;
            xx += dx * dx;
            yy += dy * dy;
          }
          const double denominator = std::sqrt(xx * yy);
          // scipy.stats.pearsonr returns NaN for a constant input vector;
          // the NaN entry is kept, as the reference keeps it.
          const double correlation = denominator == 0.0
                                         ? std::numeric_limits<double>::quiet_NaN()
                                         : numerator / denominator;
          weights[{a, b}] = correlation;
          weights[{b, a}] = correlation;
        }
      }
      break;
    }
  }
  if (options.metric_normalization) {
    // np.max over the absolute weights is NaN when any weight is NaN, and
    // the reference then skips normalization (NaN > 0 is false).
    double maximum = 0.0;
    for (const auto& [pair, weight] : weights) {
      (void)pair;
      if (std::isnan(weight)) { maximum = 0.0; break; }
      maximum = std::max(maximum, std::abs(weight));
    }
    if (maximum > 0.0)
      for (auto& [pair, weight] : weights) { (void)pair; weight /= maximum; }
  }
  // Every reference SNA variant takes np.max over the connection values and
  // raises ValueError ("zero-size array to reduction operation maximum which
  // has no identity") when there is no connection. The empty network stays
  // available, reported as not computed, as other reference exceptions are.
  if (weights.empty()) {
    result.diagnostics.computed = false;
    result.diagnostics.message =
        "the reference raises ValueError: zero-size array to reduction operation "
        "maximum which has no identity (no connections)";
    return result;
  }
  emit_edges(weights, result, options.max_edges);
  result.diagnostics.edge_count = result.edges.size();
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated
                                   ? "edge safety bound applied"
                                   : (result.resources.empty() ? "no resources" : "ok");
  return result;
}

}  // namespace minera
