#include "minera/alpha_relations.hpp"

#include <algorithm>
#include <iterator>

namespace minera {

AlphaClassicAbstraction make_alpha_classic_abstraction(
    const DfgFrequency& graph) {
  AlphaClassicAbstraction result;
  result.directly_follows = graph;
  for (const auto& [edge, frequency] : graph) {
    const auto reverse = graph.find({edge.second, edge.first});
    if (frequency > 0 && (reverse == graph.end() || reverse->second == 0))
      result.causal.insert(edge);
    if (reverse != graph.end())
      result.parallel.insert(edge);
  }
  return result;
}

AlphaEndpoints infer_alpha_endpoints(const DfgFrequency& graph) {
  AlphaEndpoints result;
  std::set<std::string> incoming;
  std::set<std::string> outgoing;
  for (const auto& [edge, frequency] : graph) {
    (void)frequency;
    outgoing.insert(edge.first);
    incoming.insert(edge.second);
  }
  std::set_difference(outgoing.begin(), outgoing.end(), incoming.begin(),
                      incoming.end(), std::inserter(result.starts,
                                                    result.starts.end()));
  std::set_difference(incoming.begin(), incoming.end(), outgoing.begin(),
                      outgoing.end(), std::inserter(result.ends,
                                                    result.ends.end()));
  return result;
}

AlphaEndpoints alpha_log_endpoints(const EventLog& log) {
  AlphaEndpoints result;
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    result.starts.insert(trace.events.front().activity);
    result.ends.insert(trace.events.back().activity);
  }
  return result;
}

}  // namespace minera
