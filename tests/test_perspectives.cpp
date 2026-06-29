// Tests for the time (performance) perspective, the organizational (resource)
// perspective, and PNML export. Kept in its own translation unit so the core
// test file stays focused on discovery and conformance.
#undef NDEBUG

#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/performance.hpp"
#include "minera/petri_net.hpp"
#include "minera/pnml.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path write_temp_log(const std::string& content) {
  auto path = std::filesystem::temp_directory_path() / "minera_persp_test.csv";
  std::ofstream output(path);
  output << content;
  return path;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

void timestamp_parses_iso8601_forms() {
  const auto epoch = minera::parse_timestamp("1970-01-01T00:00:00");
  assert(epoch && near(*epoch, 0.0));

  // One hour after the epoch.
  const auto hour = minera::parse_timestamp("1970-01-01T01:00:00");
  assert(hour && near(*hour, 3600.0));

  // A space separator and fractional seconds both parse.
  const auto frac = minera::parse_timestamp("1970-01-01 00:00:01.5");
  assert(frac && near(*frac, 1.5));

  // A bare date is midnight UTC; one full day is 86400 s.
  const auto day = minera::parse_timestamp("1970-01-02");
  assert(day && near(*day, 86400.0));

  // 'Z' is UTC; a +01:00 offset shifts the instant back an hour to UTC.
  const auto utc = minera::parse_timestamp("2020-06-01T12:00:00Z");
  const auto plus = minera::parse_timestamp("2020-06-01T13:00:00+01:00");
  assert(utc && plus && near(*utc, *plus));

  // Garbage and empty input parse to nothing.
  assert(!minera::parse_timestamp(""));
  assert(!minera::parse_timestamp("not-a-date"));
  assert(!minera::parse_timestamp("2020-13-01"));  // month out of range
}

void importer_orders_events_by_instant_not_string() {
  // The three events arrive in document order A, B, C, but their timestamps
  // carry different timezone offsets so that, by the actual instant, the true
  // order is C (07:30Z) -> A (08:00Z) -> B (09:00Z). A naive lexicographic sort
  // on the raw timestamp strings would keep A, B, C; ordering by the normalized
  // UTC instant must reorder them to C, A, B.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T08:00:00Z\n"
      "1,B,2026-01-01T10:00:00+01:00\n"   // 09:00Z
      "1,C,2026-01-01T09:30:00+02:00\n"); // 07:30Z
  const auto log = minera::import_csv_event_log(path);
  const auto& events = log.traces.front().events;
  assert(events.size() == 3);
  assert(events[0].activity == "C");
  assert(events[1].activity == "A");
  assert(events[2].activity == "B");
}

void importer_keeps_document_order_for_equal_instants() {
  // Same instant expressed two ways (UTC vs an offset) must not be reordered;
  // document order breaks the tie.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,first,2026-01-01T12:00:00Z\n"
      "1,second,2026-01-01T13:00:00+01:00\n");  // also 12:00Z
  const auto log = minera::import_csv_event_log(path);
  const auto& events = log.traces.front().events;
  assert(events.size() == 2);
  assert(events[0].activity == "first");
  assert(events[1].activity == "second");
}

void performance_measures_edge_waiting_and_case_duration() {
  // Two identical cases A->B->C, each step one minute apart, so every edge waits
  // exactly 60 s and each case spans 120 s.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:01:00\n"
      "1,C,2026-01-01T00:02:00\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,B,2026-01-01T00:01:00\n"
      "2,C,2026-01-01T00:02:00\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);

  assert(perf.timed_traces == 2);
  assert(near(perf.case_duration.mean, 120.0));
  assert(near(perf.case_duration.median, 120.0));
  assert(perf.case_duration.count == 2);

  // Edge timings line up with the graph's edges by index.
  assert(perf.edges.size() == graph.edges().size());
  for (const auto& edge : perf.edges) {
    assert(edge.stats.count == 2);
    assert(near(edge.stats.mean, 60.0));
    assert(near(edge.stats.min, 60.0));
    assert(near(edge.stats.max, 60.0));
  }
}

void performance_takes_the_median_of_varied_waits() {
  // Three A->B edges waiting 10, 20, and 60 seconds: median 20, mean 30.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:00:10\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,B,2026-01-01T00:00:20\n"
      "3,A,2026-01-01T00:00:00\n"
      "3,B,2026-01-01T00:01:00\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);

  assert(perf.edges.size() == 1);
  const auto& stats = perf.edges.front().stats;
  assert(stats.count == 3);
  assert(near(stats.median, 20.0));
  assert(near(stats.mean, 30.0));
  assert(near(stats.min, 10.0));
  assert(near(stats.max, 60.0));
}

void performance_ignores_untimed_logs() {
  // No timestamp column means no parseable instants, so every statistic is empty
  // rather than fabricated.
  const auto path = write_temp_log("case_id,activity\n1,A\n1,B\n");
  minera::CsvImportOptions options;
  options.timestamp_column = "missing";
  const auto log = minera::import_csv_event_log(path, options);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);

  assert(perf.timed_traces == 0);
  assert(perf.case_duration.count == 0);
  assert(perf.edges.size() == 1);
  assert(perf.edges.front().stats.count == 0);
}

void resource_network_counts_handovers() {
  const auto path = write_temp_log(
      "case_id,activity,resource\n"
      "1,A,alice\n"
      "1,B,bob\n"
      "1,C,alice\n"
      "2,A,alice\n"
      "2,B,bob\n");
  const auto log = minera::import_csv_event_log(path);
  assert(log.traces.front().events.front().resource == "alice");

  const auto network = minera::discover_resource_network(log);
  assert(network.resources.size() == 2);

  const auto index_of = [&](const std::string& name) {
    for (std::size_t i = 0; i < network.resources.size(); ++i) {
      if (network.resources[i] == name) return i;
    }
    assert(false);
    return std::size_t{0};
  };
  const auto alice = index_of("alice");
  const auto bob = index_of("bob");
  assert(network.event_counts[alice] == 3);  // A,C of case 1 and A of case 2
  assert(network.event_counts[bob] == 2);

  const auto count = [&](std::size_t s, std::size_t t) -> std::uint64_t {
    for (const auto& h : network.handovers) {
      if (h.source == s && h.target == t) return h.count;
    }
    return 0;
  };
  assert(count(alice, bob) == 2);  // A->B in both cases
  assert(count(bob, alice) == 1);  // B->C only in case 1
  assert(network.total_handovers == 3);
}

void resource_network_breaks_the_chain_on_missing_resource() {
  // A gap in the resource attribute must not bridge a handover across the
  // unknown worker.
  const auto path = write_temp_log(
      "case_id,activity,resource\n"
      "1,A,alice\n"
      "1,B,\n"
      "1,C,bob\n");
  const auto log = minera::import_csv_event_log(path);
  const auto network = minera::discover_resource_network(log);
  assert(network.resources.size() == 2);  // alice and bob, not the blank
  assert(network.total_handovers == 0);   // alice->? and ?->bob both dropped
}

void xes_importer_reads_the_resource_attribute() {
  std::istringstream input(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <string key=\"org:resource\" value=\"clerk\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");
  const auto log = minera::import_xes_event_log(input);
  assert(log.traces.front().events.front().resource == "clerk");
}

minera::EventLog log_from_variants(
    const std::vector<std::vector<std::string>>& variants) {
  minera::EventLog log;
  std::size_t case_index = 0;
  for (const auto& variant : variants) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& activity : variant) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void pnml_export_carries_the_net_structure() {
  const auto tree = minera::discover_process_tree(log_from_variants({{"a", "b", "c"}}));
  const auto net = minera::to_petri_net(tree);
  const auto pnml = minera::to_pnml(net, "inductive net");

  // Structural keys ProM/PM4Py read.
  for (const char* key : {"<pnml", "<net", "<place", "<transition", "<arc",
                          "initialMarking", "finalmarkings"}) {
    assert(pnml.find(key) != std::string::npos);
  }
  // Every place and transition is emitted exactly once.
  std::size_t places = 0;
  for (std::size_t pos = pnml.find("<place id="); pos != std::string::npos;
       pos = pnml.find("<place id=", pos + 1)) {
    ++places;
  }
  assert(places == net.place_count());
  // Activity names appear as transition labels.
  assert(pnml.find("<text>a</text>") != std::string::npos);
}

void pnml_export_flags_silent_transitions() {
  // A parallel block introduces silent AND-split/join transitions, which must be
  // marked invisible for downstream tools.
  const auto tree = minera::discover_process_tree(
      log_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}));
  const auto net = minera::to_petri_net(tree);
  const auto pnml = minera::to_pnml(net);
  assert(pnml.find("$invisible$") != std::string::npos);
}

void pnml_export_escapes_xml_metacharacters() {
  const auto tree = minera::discover_process_tree(
      log_from_variants({{"a & b", "c"}}));
  const auto net = minera::to_petri_net(tree);
  const auto pnml = minera::to_pnml(net);
  assert(pnml.find("a &amp; b") != std::string::npos);
  assert(pnml.find("a & b") == std::string::npos);  // raw '&' never leaks
}

void json_export_carries_performance_and_resource_contracts() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp,resource\n"
      "1,A,2026-01-01T00:00:00,alice\n"
      "1,B,2026-01-01T00:01:00,bob\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);
  const auto resources = minera::discover_resource_network(log);

  const auto json = minera::analysis_json("test", log, graph, nullptr, nullptr,
                                          nullptr, nullptr, &perf, &resources);
  for (const char* key : {"\"performance\"", "\"timed_traces\"",
                          "\"case_duration\"", "\"median\"", "\"resources\"",
                          "\"total_handovers\"", "\"handovers\"",
                          "\"event_count\""}) {
    assert(json.find(key) != std::string::npos);
  }
  // The per-event resource shows up in the trace dump.
  assert(json.find("\"resource\": \"alice\"") != std::string::npos);

  // Both sections are omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"performance\"") == std::string::npos);
  assert(without.find("\"resources\"") == std::string::npos);
}

}  // namespace

int main() {
  timestamp_parses_iso8601_forms();
  importer_orders_events_by_instant_not_string();
  importer_keeps_document_order_for_equal_instants();
  performance_measures_edge_waiting_and_case_duration();
  performance_takes_the_median_of_varied_waits();
  performance_ignores_untimed_logs();
  resource_network_counts_handovers();
  resource_network_breaks_the_chain_on_missing_resource();
  xes_importer_reads_the_resource_attribute();
  pnml_export_carries_the_net_structure();
  pnml_export_flags_silent_transitions();
  pnml_export_escapes_xml_metacharacters();
  json_export_carries_performance_and_resource_contracts();
  return 0;
}
