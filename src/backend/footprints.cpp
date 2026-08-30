#include "minera/footprints.hpp"

#include <algorithm>
#include <limits>

namespace minera {
namespace {

using PairSet = std::set<FootprintPair>;

PairSet configurations(const Footprint& fp) {
  PairSet out = fp.sequence;
  out.insert(fp.parallel.begin(), fp.parallel.end());
  out.insert(fp.atomic_interleaving.begin(), fp.atomic_interleaving.end());
  return out;
}

void derive_relations(Footprint& fp) {
  for (const auto& [pair, count] : fp.dfg) {
    (void)count;
    const auto reverse = FootprintPair{pair.second, pair.first};
    if (fp.dfg.find(reverse) != fp.dfg.end()) {
      fp.parallel.insert(pair);
    } else {
      fp.sequence.insert(pair);
    }
  }
}

Footprint trace_footprint(const Trace& trace) {
  Footprint fp;
  fp.min_trace_length = trace.events.size();
  fp.trace = std::vector<std::string>{};
  fp.trace->reserve(trace.events.size());
  for (const auto& event : trace.events) {
    fp.trace->push_back(event.activity);
    fp.activities.insert(event.activity);
  }
  if (!fp.trace->empty()) {
    fp.start_activities.insert(fp.trace->front());
    fp.end_activities.insert(fp.trace->back());
    for (std::size_t i = 1; i < fp.trace->size(); ++i) {
      ++fp.dfg[{(*fp.trace)[i - 1], (*fp.trace)[i]}];
    }
  }
  derive_relations(fp);
  return fp;
}

void set_fit(FootprintsExtensiveResult& result) {
  result.is_footprints_fit = result.footprints.empty() &&
                             result.start_activities.empty() &&
                             result.end_activities.empty() &&
                             result.activities_always_happening.empty() &&
                             result.min_length_fit;
  result.is_fit = result.is_footprints_fit;
}

FootprintsExtensiveResult extensive_one(const Footprint& log,
                                        const Footprint& model,
                                        bool check_aah) {
  FootprintsExtensiveResult result;
  const auto log_cfg = configurations(log);
  const auto model_cfg = configurations(model);
  for (const auto& pair : log_cfg) {
    if (model_cfg.find(pair) == model_cfg.end()) result.footprints.insert(pair);
  }
  if (model.start_activities_present) {
    for (const auto& activity : log.start_activities) {
      if (model.start_activities.find(activity) == model.start_activities.end()) {
        result.start_activities.insert(activity);
      }
    }
  }
  if (model.end_activities_present) {
    for (const auto& activity : log.end_activities) {
      if (model.end_activities.find(activity) == model.end_activities.end()) {
        result.end_activities.insert(activity);
      }
    }
  }
  if (check_aah && model.activities_always_happening) {
    for (const auto& activity : *model.activities_always_happening) {
      if (log.activities.find(activity) == log.activities.end()) {
        result.activities_always_happening.insert(activity);
      }
    }
  }
  if (log.min_trace_length && model.min_trace_length) {
    result.min_length_fit = *log.min_trace_length >= *model.min_trace_length;
  }
  set_fit(result);
  return result;
}

Footprint flatten(const std::vector<Footprint>& traces) {
  Footprint fp;
  for (const auto& trace : traces) {
    for (const auto& [pair, count] : trace.dfg) fp.dfg[pair] += count;
    fp.sequence.insert(trace.sequence.begin(), trace.sequence.end());
    fp.parallel.insert(trace.parallel.begin(), trace.parallel.end());
    fp.start_activities.insert(trace.start_activities.begin(), trace.start_activities.end());
    fp.end_activities.insert(trace.end_activities.begin(), trace.end_activities.end());
    fp.activities.insert(trace.activities.begin(), trace.activities.end());
  }
  return fp;
}

}  // namespace

Footprint discover_footprints(const EventLog& log) {
  Footprint fp;
  fp.min_trace_length = 0;
  bool first = true;
  for (const auto& trace : log.traces) {
    if (first || trace.events.size() < *fp.min_trace_length) {
      *fp.min_trace_length = trace.events.size();
      first = false;
    }
    if (trace.events.empty()) continue;
    fp.start_activities.insert(trace.events.front().activity);
    fp.end_activities.insert(trace.events.back().activity);
    for (const auto& event : trace.events) fp.activities.insert(event.activity);
    for (std::size_t i = 1; i < trace.events.size(); ++i) {
      ++fp.dfg[{trace.events[i - 1].activity, trace.events[i].activity}];
    }
  }
  derive_relations(fp);
  return fp;
}

std::vector<Footprint> discover_trace_footprints(const EventLog& log) {
  std::vector<Footprint> out;
  out.reserve(log.traces.size());
  for (const auto& trace : log.traces) out.push_back(trace_footprint(trace));
  return out;
}

Footprint discover_dfg_footprints(
    const std::map<FootprintPair, std::uint64_t>& dfg) {
  Footprint fp;
  fp.dfg_present = false;
  fp.min_trace_length_present = false;
  fp.min_trace_length.reset();
  // The DFG variant intentionally omits the dfg field. Build the
  // relations from the input map, then leave fp.dfg empty and mark it absent.
  Footprint relations;
  relations.dfg = dfg;
  derive_relations(relations);
  fp.sequence = std::move(relations.sequence);
  fp.parallel = std::move(relations.parallel);
  for (const auto& [pair, count] : dfg) {
    (void)count;
    fp.activities.insert(pair.first);
    fp.activities.insert(pair.second);
  }
  for (const auto& activity : fp.activities) {
    bool incoming = false, outgoing = false;
    for (const auto& [pair, count] : dfg) {
      (void)count;
      incoming = incoming || pair.second == activity;
      outgoing = outgoing || pair.first == activity;
    }
    if (!incoming) fp.start_activities.insert(activity);
    if (!outgoing) fp.end_activities.insert(activity);
  }
  return fp;
}

Footprint discover_dfg_footprints(const DirectlyFollowsGraph& graph) {
  // Start with the exact raw-DFG semantics. The Minera graph also carries
  // occurrence statistics, so enrich the result below with singleton
  // activities and observed boundaries that cannot be reconstructed from
  // edge keys alone. The map overload intentionally does not do this.
  std::map<FootprintPair, std::uint64_t> dfg;
  for (const auto& edge : graph.edges()) {
    if (edge.source < graph.activities().size() && edge.target < graph.activities().size()) {
      dfg[{graph.activities()[edge.source], graph.activities()[edge.target]}] = edge.count;
    }
  }
  auto fp = discover_dfg_footprints(dfg);
  for (const auto& activity : graph.activities()) {
    if (graph.activity_count(activity) == 0) continue;
    fp.activities.insert(activity);
    if (graph.start_activity_count(activity) > 0) {
      fp.start_activities.insert(activity);
    }
    if (graph.end_activity_count(activity) > 0) {
      fp.end_activities.insert(activity);
    }
  }
  return fp;
}

FootprintViolations check_footprints_log_model(
    const Footprint& log, const Footprint& model,
    FootprintsLogModelOptions options) {
  FootprintViolations violations;
  if (options.strict) {
    for (const auto& pair : log.sequence) {
      if (model.sequence.find(pair) == model.sequence.end()) violations.insert(pair);
    }
    for (const auto& pair : log.parallel) {
      if (model.parallel.find(pair) == model.parallel.end()) violations.insert(pair);
    }
    for (const auto& pair : log.atomic_interleaving) {
      if (model.atomic_interleaving.find(pair) == model.atomic_interleaving.end())
        violations.insert(pair);
    }
  } else {
    const auto log_cfg = configurations(log);
    const auto model_cfg = configurations(model);
    for (const auto& pair : log_cfg) {
      if (model_cfg.find(pair) == model_cfg.end()) violations.insert(pair);
    }
  }
  return violations;
}

std::vector<FootprintViolations> check_footprints_log_model(
    const std::vector<Footprint>& log, const Footprint& model,
    FootprintsLogModelOptions options) {
  std::vector<FootprintViolations> out;
  out.reserve(log.size());
  for (const auto& trace : log) out.push_back(check_footprints_log_model(trace, model, options));
  return out;
}

FootprintsExtensiveResult check_footprints_log_extensive(
    const Footprint& log, const Footprint& model) {
  return extensive_one(log, model, false);
}

std::vector<FootprintsExtensiveResult> check_footprints_trace_extensive(
    const std::vector<Footprint>& log, const Footprint& model,
    FootprintsTraceExtensiveOptions options) {
  std::vector<FootprintsExtensiveResult> out;
  out.reserve(log.size());
  for (const auto& trace : log) out.push_back(extensive_one(trace, model, options.enable_act_always_executed));
  return out;
}

double fp_precision(const Footprint& log, const Footprint& model) {
  const auto model_cfg = configurations(model);
  if (model_cfg.empty()) return 1.0;
  const auto log_cfg = configurations(log);
  std::size_t shared = 0;
  for (const auto& pair : log_cfg) if (model_cfg.find(pair) != model_cfg.end()) ++shared;
  return static_cast<double>(shared) / static_cast<double>(model_cfg.size());
}

double fp_fitness(const Footprint& log, const Footprint&, const FootprintsExtensiveResult& conformance) {
  if (log.dfg.empty()) return 1.0;
  const double sum_dfg = static_cast<double>([&] {
    std::uint64_t value = 0; for (const auto& [pair, count] : log.dfg) { (void)pair; value += count; } return value;
  }());
  std::uint64_t sum_dev = 0;
  for (const auto& pair : conformance.footprints) {
    const auto it = log.dfg.find(pair); if (it != log.dfg.end()) sum_dev += it->second;
  }
  const double nseq = static_cast<double>(log.sequence.size());
  const double npar = static_cast<double>(log.parallel.size());
  const double ninter =
      static_cast<double>(log.atomic_interleaving.size());
  const double nstart = static_cast<double>(log.start_activities.size());
  const double nend = static_cast<double>(log.end_activities.size());
  const double nrelations = nseq + npar + ninter;
  const double denominator = nrelations + nstart + nend;
  if (denominator == 0.0) return 1.0;
  return ((1.0 - static_cast<double>(sum_dev) / sum_dfg) * nrelations +
          nstart + nend - static_cast<double>(conformance.start_activities.size()) -
          static_cast<double>(conformance.end_activities.size())) / denominator;
}

FootprintsFitnessSummary fp_fitness(
    const std::vector<Footprint>& log, const Footprint& model,
    const std::vector<FootprintsExtensiveResult>& conformance) {
  FootprintsFitnessSummary result;
  if (!conformance.empty()) {
    std::size_t fit = 0;
    for (const auto& item : conformance) fit += item.is_footprints_fit ? 1U : 0U;
    result.perc_fit_traces = 100.0 * static_cast<double>(fit) /
                             static_cast<double>(conformance.size());
  }
  FootprintsExtensiveResult flattened_conf;
  for (const auto& item : conformance) {
    flattened_conf.footprints.insert(item.footprints.begin(), item.footprints.end());
    flattened_conf.start_activities.insert(item.start_activities.begin(), item.start_activities.end());
    flattened_conf.end_activities.insert(item.end_activities.begin(), item.end_activities.end());
  }
  result.log_fitness = fp_fitness(flatten(log), model, flattened_conf);
  return result;
}

}  // namespace minera
