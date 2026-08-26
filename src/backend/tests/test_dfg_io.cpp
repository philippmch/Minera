#include "minera/dfg_io.hpp"
#include "minera/directly_follows_graph.hpp"

#include <cassert>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using minera::DfgCleanAnalysis;

bool throws(const std::string& text) {
  try {
    (void)minera::parse_dfg(text);
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

void round_trip_is_deterministic() {
  DfgCleanAnalysis source;
  source.graph[{"B", "C"}] = 4;
  source.graph[{"A", "B"}] = 7;
  source.start_activities["A"] = 2;
  source.end_activities["C"] = 2;
  const auto text = minera::to_dfg(source);
  assert(text == "3\nA\nB\nC\n1\n0x2\n1\n2x2\n0>1x7\n1>2x4\n");
  const auto reread = minera::parse_dfg(text);
  assert(reread.graph == source.graph);
  assert(reread.start_activities == source.start_activities);
  assert(reread.end_activities == source.end_activities);
  assert(minera::serialize_dfg(reread) == text);
  std::istringstream input(text);
  assert(minera::read_dfg(input).graph == source.graph);
}

void preserves_utf8_labels() {
  const auto dfg = minera::parse_dfg(
      "2\nAuftrag ✓\n送信\n1\n0x3\n1\n1x3\n0>1x3\n");
  assert(dfg.start_activities.at("Auftrag ✓") == 3);
  assert(dfg.end_activities.at("送信") == 3);
  assert(dfg.graph.at(std::pair<std::string, std::string>{"Auftrag ✓", "送信"}) == 3);
  assert(minera::to_dfg(dfg).find("Auftrag ✓") != std::string::npos);
}

void projects_discovered_graph_without_zero_registry_entries() {
  minera::EventLog log;
  log.traces = {
      {"one", {minera::Event{"one", "A", "", "", 0, {}, {}},
                minera::Event{"one", "B", "", "", 1, {}, {}}}, {}},
      {"two", {minera::Event{"two", "A", "", "", 0, {}, {}}}, {}},
  };
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto projected = minera::to_dfg_analysis(graph);
  assert(projected.graph.at({"A", "B"}) == 1);
  assert(projected.start_activities.at("A") == 2);
  assert(projected.end_activities.at("A") == 1);
  assert(projected.end_activities.at("B") == 1);
  assert(minera::to_dfg(projected).find("C") == std::string::npos);
}

void rejects_malformed_and_duplicate_rows() {
  assert(throws("1\nA\n1\n0x1\n"));  // missing end count
  assert(throws("2\nA\nA\n0\n0\n"));  // duplicate label
  assert(throws("1\nA\n2\n0x1\n0x1\n0\n"));  // duplicate start id
  assert(throws("1\nA\n0\n0\n0>1x1\n"));  // dangling edge id
  assert(throws("1\nA\n0\n0\n0>0x0\n"));  // zero frequency
  assert(throws("1\nA\n0\n0\n0>0x1 extra\n"));  // variant/malformed row
  assert(throws("1\nA\n0\n0\n 0>0x1\n"));  // non-canonical row whitespace
}

void imports_and_exports_files() {
  DfgCleanAnalysis source;
  source.graph[{"α", "β"}] = 1;
  source.start_activities["α"] = 1;
  source.end_activities["β"] = 1;
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_dfg_io_roundtrip.dfg";
  minera::export_dfg(source, path);
  const auto loaded = minera::import_dfg(path);
  std::filesystem::remove(path);
  assert(loaded.graph == source.graph);
  assert(loaded.start_activities == source.start_activities);
  assert(loaded.end_activities == source.end_activities);
}

}  // namespace

int main() {
  round_trip_is_deterministic();
  preserves_utf8_labels();
  projects_discovered_graph_without_zero_registry_entries();
  rejects_malformed_and_duplicate_rows();
  imports_and_exports_files();
}
