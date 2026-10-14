#include "minera/alpha_miner.hpp"
#include "minera/directly_follows_graph.hpp"
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>

int main() {
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  const auto graph = minera::DirectlyFollowsGraph::from_frequencies(
      {"B", "A", "singleton", ""}, {{1, 0, maximum}, {2, 2, 0}},
      {0, maximum, 1, 0}, {maximum, 0, 1, 0});
  assert(graph.activities() == std::vector<std::string>({"B", "A", "singleton", ""}));
  assert(graph.edges().size() == 2 && graph.edges()[1].count == 0);
  assert(graph.edge_count("A", "B") == maximum);
  assert(graph.start_activity_count("A") == maximum);
  assert(graph.end_activity_count("B") == maximum);
  assert(graph.activity_count("A") == 0 && graph.variants().empty());
  const auto net = minera::discover_petri_net(graph);
  assert(net.activities().size() == 4);
  const auto rejects = [](auto operation) {
    try { operation(); } catch (const std::invalid_argument&) { return true; }
    return false;
  };
  assert(rejects([] { (void)minera::DirectlyFollowsGraph::from_frequencies({"A", "A"}, {}, {0, 0}, {0, 0}); }));
  assert(rejects([] { (void)minera::DirectlyFollowsGraph::from_frequencies({"A"}, {{0, 1, 1}}, {0}, {0}); }));
  assert(rejects([] { (void)minera::DirectlyFollowsGraph::from_frequencies({"A"}, {{0, 0, 1}, {0, 0, 0}}, {0}, {0}); }));
  assert(rejects([] { (void)minera::DirectlyFollowsGraph::from_frequencies({"A"}, {}, {}, {0}); }));
}
