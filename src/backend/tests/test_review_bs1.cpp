// Review BS1: typed columns as activity keys on a CSV upload's frame, the
// LLM abstractions on nanosecond cells, and the stochastic map's CTMC
// reachability when the fitted map has no CTMC. pm4py labels the alpha
// transitions of a date column with str() of the UTC Timestamp; heuristics,
// Inductive and POWL raise TypeError on keys that are no str. The LLM
// durations and the date quantiles read the exact nanoseconds (the
// quantiles through numpy's linear interpolation on int64 units). Values are
// taken from pm4py 2.7.22 black-box on the same input (review-harness
// round-bs, bs1/mk.py).
#include "minera/alpha_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/llm_abstractions.hpp"
#include "minera/petri_net.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/stochastic_map.hpp"
#include "../tools/pnml_model_options.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

void check_error(const std::string& label, const std::function<void()>& run, const std::string& message) {
  try {
    run();
  } catch (const std::exception& error) {
    if (std::string(error.what()).find(message) != std::string::npos) return;
    ++failures;
    std::cerr << label << ":\n  error    " << error.what() << "\n  expected " << message << "\n";
    return;
  }
  ++failures;
  std::cerr << label << ": no error, expected " << message << "\n";
}

const std::string kCsv =
    "case,act,ts,d,n,t\n"
    "c1,a,2020-01-01 00:00:00.000000600,2021-03-01,1,x\n"
    "c1,b,2020-01-01 00:00:05.123456789,2021-03-02,2,y\n"
    "c2,a,2020-01-01 00:00:01.000001400,2021-03-01,1,x\n"
    "c2,b,2020-01-01 00:00:07.999999999,2021-03-03,3,\n"
    "c3,a,2020-01-01 00:00:02.5,2021-03-01,1,x\n"
    "c3,b,2020-01-01 00:00:09.000000001,2021-03-02,2,y\n";

EventLog import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options).log;
}

std::string join(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  std::string out;
  for (const auto& value : values) out += (out.empty() ? "" : "|") + value;
  return out;
}

std::string labels(const PetriNet& net, const std::vector<std::string>& activities) {
  std::vector<std::string> out;
  for (const auto& transition : net.transitions())
    if (transition.activity) out.push_back(activities.at(*transition.activity));
  return join(out);
}

void typed_activity_keys(const EventLog& log) {
  AlphaMinerOptions alpha;
  alpha.activity_attribute = "d";
  const auto net = discover_petri_net(log, alpha);
  check("alpha keyed by a date column", labels(net, net.activities()),
        "2021-03-01 00:00:00+00:00|2021-03-02 00:00:00+00:00|2021-03-03 00:00:00+00:00");

  HeuristicMinerOptions heuristics;
  heuristics.activity_attribute = "d";
  check_error("heuristics keyed by a date column", [&] { (void)discover_heuristics_net(log, heuristics); },
              "TypeError: 'Timestamp' object is not subscriptable");
  heuristics.activity_attribute = "case";
  const auto by_case = discover_heuristics_net(log, heuristics);
  std::vector<std::string> case_labels;
  for (const auto index : by_case.active_activities) case_labels.push_back(by_case.activities.at(index));
  check("heuristics keyed by the case column", join(case_labels), "c1|c2|c3");

  InductiveMinerOptions inductive;
  inductive.activity_key = "n";
  check_error("Inductive keyed by an integer column", [&] { (void)prepare_inductive_event_log(log, inductive); },
              "TypeError: can only concatenate str (not \"int\") to str");
  inductive.activity_key = "d";
  check_error("Inductive keyed by a date column", [&] { (void)prepare_inductive_event_log(log, inductive); },
              "TypeError: can only concatenate str (not \"Timestamp\") to str");

  PowlDiscoveryOptions powl;
  powl.activity_key = "t";
  check_error("POWL keyed by text with a blank", [&] { (void)discover_powl(log, powl); },
              "TypeError: '<' not supported between instances of 'float' and 'str'");
}

void llm_nanoseconds(const EventLog& log) {
  LlmAbstractionOptions dfg;
  dfg.secondary_performance_aggregation = "stdev";
  check("LLM DFG durations", llm_log_dfg_description(log, dfg),
        "If I have a process with flow:\n\na -> b ( frequency = 3  performance = 6.208  stdev = 0.972 )\n\n\n");
  LlmAbstractionOptions variants;
  variants.primary_performance_aggregation = "max";
  check("LLM variant durations", llm_log_variants_description(log, variants),
        "If I have a process with the following process variants:\n\n a -> b ( frequency = 3  performance = 7.000 )\n\n\n");
  check("LLM stream keeps the nanoseconds", llm_event_stream_description(log),
        "\n"
        "\n"
        "If I have the following stream of events:\n"
        "\n"
        "{'case': 'c1', 'act': 'a', 'ts': Timestamp('2020-01-01 00:00:00.000000600+0000', tz='UTC'), 'd': Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 'n': 1, 't': 'x', 'case:concept:name': 'c1', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-01 00:00:00.000000600+0000', tz='UTC'), '@@index': 0, '@@case_index': 0}\n"
        "{'case': 'c2', 'act': 'a', 'ts': Timestamp('2020-01-01 00:00:01.000001400+0000', tz='UTC'), 'd': Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 'n': 1, 't': 'x', 'case:concept:name': 'c2', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-01 00:00:01.000001400+0000', tz='UTC'), '@@index': 2, '@@case_index': 1}\n"
        "{'case': 'c3', 'act': 'a', 'ts': Timestamp('2020-01-01 00:00:02.500000+0000', tz='UTC'), 'd': Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 'n': 1, 't': 'x', 'case:concept:name': 'c3', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-01 00:00:02.500000+0000', tz='UTC'), '@@index': 4, '@@case_index': 2}\n"
        "{'case': 'c1', 'act': 'b', 'ts': Timestamp('2020-01-01 00:00:05.123456789+0000', tz='UTC'), 'd': Timestamp('2021-03-02 00:00:00+0000', tz='UTC'), 'n': 2, 't': 'y', 'case:concept:name': 'c1', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 00:00:05.123456789+0000', tz='UTC'), '@@index': 1, '@@case_index': 0}\n"
        "{'case': 'c2', 'act': 'b', 'ts': Timestamp('2020-01-01 00:00:07.999999999+0000', tz='UTC'), 'd': Timestamp('2021-03-03 00:00:00+0000', tz='UTC'), 'n': 3, 'case:concept:name': 'c2', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 00:00:07.999999999+0000', tz='UTC'), '@@index': 3, '@@case_index': 1}\n"
        "{'case': 'c3', 'act': 'b', 'ts': Timestamp('2020-01-01 00:00:09.000000001+0000', tz='UTC'), 'd': Timestamp('2021-03-02 00:00:00+0000', tz='UTC'), 'n': 2, 't': 'y', 'case:concept:name': 'c3', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 00:00:09.000000001+0000', tz='UTC'), '@@index': 5, '@@case_index': 2}\n"
        "\n");
  check("LLM date quantiles",
        llm_log_columns_description(log),
        "concept:name  empty: 0 values: (b; freq. 3) (a; freq. 3)\n"
        "d  empty: 0  quantiles: {0.0: Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 0.25: "
        "Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 0.5: Timestamp('2021-03-01 12:00:00+0000', tz='UTC'), "
        "0.75: Timestamp('2021-03-02 00:00:00+0000', tz='UTC'), 1.0: Timestamp('2021-03-03 00:00:00+0000', "
        "tz='UTC')}\n"
        "time:timestamp  empty: 0  quantiles: {0.0: Timestamp('2020-01-01 00:00:00.000000512+0000', tz='UTC'), "
        "0.25: Timestamp('2020-01-01 00:00:01.375000832+0000', tz='UTC'), 0.5: "
        "Timestamp('2020-01-01 00:00:03.811728384+0000', tz='UTC'), 0.75: "
        "Timestamp('2020-01-01 00:00:07.280864256+0000', tz='UTC'), 1.0: Timestamp('2020-01-01 00:00:09+0000', "
        "tz='UTC')}\n"
        "ts  empty: 0  quantiles: {0.0: Timestamp('2020-01-01 00:00:00.000000512+0000', tz='UTC'), 0.25: "
        "Timestamp('2020-01-01 00:00:01.375000832+0000', tz='UTC'), 0.5: "
        "Timestamp('2020-01-01 00:00:03.811728384+0000', tz='UTC'), 0.75: "
        "Timestamp('2020-01-01 00:00:07.280864256+0000', tz='UTC'), 1.0: Timestamp('2020-01-01 00:00:09+0000', "
        "tz='UTC')}");
}

// A fitted normal with a negative mean (durations of rows out of time order)
// has no CTMC rate: the route returns the map with the reachability marked
// unavailable instead of failing.
void stochastic_map_without_ctmc() {
  PetriNet net;
  const auto start = net.add_place();
  const auto end = net.add_place();
  (void)net.add_transition(0, {start}, {end});
  StochasticMap map;
  StochasticMapTransition record;
  record.transition = 0;
  record.distribution = StochasticMapDistribution::kNormal;
  record.parameter = -2.5;
  record.secondary_parameter = 1.0;
  map.transitions.push_back(record);
  const auto result = minera::tools::stochastic_map_reachability(net, map, {}, {1, 0}, 1000);
  check("reachability without a CTMC is exhausted", result.exhausted ? "true" : "false", "true");
  check("reachability without a CTMC explains why", result.diagnostic.substr(0, 18), "CTMC unavailable: ");
  check_error("a wrong-size marking still fails",
              [&] { (void)minera::tools::stochastic_map_reachability(net, map, {}, {1}, 1000); },
              "initial marking must match the place count");
}

}  // namespace

int main() {
  const auto log = import();
  typed_activity_keys(log);
  llm_nanoseconds(log);
  stochastic_map_without_ctmc();
  if (failures) {
    std::cerr << failures << " review BS1 check(s) failed\n";
    return 1;
  }
  std::cout << "review BS1 checks passed\n";
  return 0;
}
