// Tests for the time (performance) perspective, the organizational (resource)
// perspective, and PNML export. Kept in its own translation unit so the core
// test file stays focused on discovery and conformance.
#undef NDEBUG

#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
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
  assert(!minera::parse_timestamp("2021-02-29"));  // not a leap year
  assert(!minera::parse_timestamp("2020-02-30"));
  assert(!minera::parse_timestamp("2020-04-31"));
  assert(!minera::parse_timestamp("2020-01-01T24:00:00"));
  assert(!minera::parse_timestamp("2020-01-01T23:60:00"));
  assert(!minera::parse_timestamp("2020-01-01T23:59:60"));
  assert(!minera::parse_timestamp("2020-01-01T12:00:00+14:01"));
  assert(!minera::parse_timestamp("2020-01-01T12:00:00+15:00"));
  assert(minera::parse_timestamp("2020-02-29T23:59:59-14:00"));
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

void performance_measures_the_case_arrival_rate() {
  // Cases starting at 00:00, 00:01:40, and 00:05:50 (out of order in the
  // file): sorted arrival gaps are 100 s and 250 s — mean 175, median 175.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:05:50\n"
      "1,B,2026-01-01T00:06:00\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,B,2026-01-01T00:00:30\n"
      "3,A,2026-01-01T00:01:40\n"
      "3,B,2026-01-01T00:02:00\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);

  assert(perf.case_arrival.count == 2);
  assert(near(perf.case_arrival.mean, 175.0));
  assert(near(perf.case_arrival.median, 175.0));
  assert(near(perf.case_arrival.min, 100.0));
  assert(near(perf.case_arrival.max, 250.0));
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
  assert(perf.case_arrival.count == 0);
  assert(perf.edges.size() == 1);
  assert(perf.edges.front().stats.count == 0);
}

void service_times_pair_lifecycle_starts_and_completes() {
  // Case 1 works on A for 30 s; case 2 opens two A instances that finish in
  // arrival order (FIFO), 40 s each — and spells its transitions uppercase,
  // as BPI-style logs do. B never has both a start and a complete, so it
  // contributes no service time.
  const auto path = write_temp_log(
      "case_id,activity,timestamp,lifecycle\n"
      "1,A,2026-01-01T00:00:00,start\n"
      "1,A,2026-01-01T00:00:30,complete\n"
      "1,B,2026-01-01T00:01:00,complete\n"
      "2,A,2026-01-01T00:00:00,START\n"
      "2,A,2026-01-01T00:00:10,start\n"
      "2,A,2026-01-01T00:00:40,COMPLETE\n"
      "2,A,2026-01-01T00:00:50,complete\n"
      "2,B,2026-01-01T00:01:00,start\n");
  const auto log = minera::import_csv_event_log(path);
  assert(log.traces.front().events.front().lifecycle == "start");
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);

  // Only A ever pairs; the unmatched B events are atomic observations.
  assert(perf.activity_service.size() == 1);
  const auto& service = perf.activity_service.front();
  assert(service.activity == *graph.activity_id("A"));
  assert(service.stats.count == 3);
  assert(near(service.stats.min, 30.0));
  assert(near(service.stats.max, 40.0));
  assert(near(service.stats.median, 40.0));
  assert(near(service.stats.mean, 110.0 / 3.0));
}

void service_times_absent_for_atomic_logs() {
  // A log without lifecycle transitions yields no service times at all.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:01:00\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto perf = minera::compute_performance_metrics(log, graph);
  assert(perf.activity_service.empty());
}

void variant_durations_summarize_per_variant() {
  // Two cases share the variant A->B->C (spanning 120 s and 60 s) and one case
  // is the variant A->C (spanning 30 s). Throughput must be summarized per
  // variant and reported in the graph's variant order.
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:01:00\n"
      "1,C,2026-01-01T00:02:00\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,B,2026-01-01T00:00:30\n"
      "2,C,2026-01-01T00:01:00\n"
      "3,A,2026-01-01T00:00:00\n"
      "3,C,2026-01-01T00:00:30\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto durations = minera::compute_variant_durations(log, graph);

  assert(durations.size() == graph.variants().size());
  bool saw_abc = false;
  bool saw_ac = false;
  for (std::size_t i = 0; i < graph.variants().size(); ++i) {
    const auto& stats = durations[i];
    if (graph.variants()[i].activities.size() == 3) {  // A->B->C
      saw_abc = true;
      assert(stats.count == 2);
      assert(near(stats.median, 90.0));  // (60 + 120) / 2
      assert(near(stats.mean, 90.0));
      assert(near(stats.min, 60.0));
      assert(near(stats.max, 120.0));
    } else {  // A->C
      saw_ac = true;
      assert(stats.count == 1);
      assert(near(stats.median, 30.0));
    }
  }
  assert(saw_abc && saw_ac);
}

void variant_durations_empty_without_timestamps() {
  // An untimed log yields a stats entry per variant, each with count 0 — no
  // fabricated throughput.
  const auto path = write_temp_log("case_id,activity\n1,A\n1,B\n2,A\n2,C\n");
  minera::CsvImportOptions options;
  options.timestamp_column = "missing";
  const auto log = minera::import_csv_event_log(path, options);
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto durations = minera::compute_variant_durations(log, graph);

  assert(durations.size() == graph.variants().size());
  for (const auto& stats : durations) {
    assert(stats.count == 0);
  }
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

void bpmn_export_carries_block_structure() {
  // "a" then a parallel block of "b" and "c" then "d": the inductive miner
  // discovers a parallel operator, which must become a parallel gateway pair
  // with the activities wired as tasks between start and end events.
  const auto tree = minera::discover_process_tree(
      log_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}));
  const auto bpmn = minera::to_bpmn(tree, "test");

  for (const char* key : {"<definitions", "<process", "<startEvent",
                          "<endEvent", "<task", "<parallelGateway",
                          "<sequenceFlow"}) {
    assert(bpmn.find(key) != std::string::npos);
  }
  // Activity names ride along as task labels.
  assert(bpmn.find("name=\"a\"") != std::string::npos);
  assert(bpmn.find("name=\"d\"") != std::string::npos);
}

void bpmn_export_uses_a_gateway_for_an_exclusive_choice() {
  // Two disjoint single-activity traces are an exclusive choice between "a" and
  // "b", which must surface as an exclusive gateway.
  const auto tree =
      minera::discover_process_tree(log_from_variants({{"a"}, {"b"}}));
  const auto bpmn = minera::to_bpmn(tree);
  assert(bpmn.find("<exclusiveGateway") != std::string::npos);
}

void bpmn_export_escapes_xml_metacharacters() {
  const auto tree =
      minera::discover_process_tree(log_from_variants({{"a & b", "c"}}));
  const auto bpmn = minera::to_bpmn(tree);
  assert(bpmn.find("a &amp; b") != std::string::npos);
  assert(bpmn.find("a & b") == std::string::npos);  // raw '&' never leaks
}

void bpmn_net_export_keeps_a_sequence_gateway_free() {
  // A plain a→b workflow net: start, the two tasks, end — the connector
  // places collapse into sequence flows and no gateway appears.
  const auto tree =
      minera::discover_process_tree(log_from_variants({{"a", "b"}}));
  const auto bpmn = minera::to_bpmn(minera::to_petri_net(tree), "test");
  for (const char* key : {"<definitions", "<startEvent", "<endEvent",
                          "name=\"a\"", "name=\"b\"", "<sequenceFlow"}) {
    assert(bpmn.find(key) != std::string::npos);
  }
  assert(bpmn.find("Gateway") == std::string::npos);
}

void bpmn_net_export_maps_places_and_transitions_to_gateways() {
  // The Alpha Miner's net for {a}|{b} branches at a place (a token picks one
  // consumer), which must become an exclusive gateway — and never a parallel
  // one, since no transition forks tokens here.
  const auto choice = minera::discover_petri_net(log_from_variants({{"a"}, {"b"}}));
  const auto choice_bpmn = minera::to_bpmn(choice);
  assert(choice_bpmn.find("<exclusiveGateway") != std::string::npos);
  assert(choice_bpmn.find("<parallelGateway") == std::string::npos);

  // A parallel block of b and c converts to a net whose silent AND-split
  // transition produces two places at once — a parallel gateway in BPMN.
  const auto tree = minera::discover_process_tree(
      log_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}));
  const auto parallel_bpmn = minera::to_bpmn(minera::to_petri_net(tree));
  assert(parallel_bpmn.find("<parallelGateway") != std::string::npos);
}

void bpmn_net_export_escapes_xml_metacharacters() {
  const auto tree =
      minera::discover_process_tree(log_from_variants({{"a & b", "c"}}));
  const auto bpmn = minera::to_bpmn(minera::to_petri_net(tree));
  assert(bpmn.find("a &amp; b") != std::string::npos);
  assert(bpmn.find("a & b") == std::string::npos);  // raw '&' never leaks
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
                          "\"case_duration\"", "\"case_arrival\"",
                          "\"median\"", "\"resources\"",
                          "\"total_handovers\"", "\"handovers\"",
                          "\"event_count\"", "\"rework_cases\""}) {
    assert(json.find(key) != std::string::npos);
  }
  // The per-event resource shows up in the trace dump.
  assert(json.find("\"resource\": \"alice\"") != std::string::npos);

  // A lifecycle log additionally carries per-event transitions and the
  // service-time block inside performance.
  const auto lifecycle_path = write_temp_log(
      "case_id,activity,timestamp,lifecycle\n"
      "1,A,2026-01-01T00:00:00,start\n"
      "1,A,2026-01-01T00:00:30,complete\n");
  const auto lifecycle_log = minera::import_csv_event_log(lifecycle_path);
  const auto lifecycle_graph =
      minera::discover_directly_follows_graph(lifecycle_log);
  const auto lifecycle_perf =
      minera::compute_performance_metrics(lifecycle_log, lifecycle_graph);
  const auto lifecycle_json =
      minera::analysis_json("test", lifecycle_log, lifecycle_graph, nullptr,
                            nullptr, nullptr, nullptr, &lifecycle_perf);
  assert(lifecycle_json.find("\"lifecycle\": \"start\"") != std::string::npos);
  assert(lifecycle_json.find("\"activities\": [{\"activity\": 0, \"stats\": "
                             "{\"count\": 1, \"total\": 30") !=
         std::string::npos);

  // Both sections are omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"performance\"") == std::string::npos);
  assert(without.find("\"resources\"") == std::string::npos);
  // Per-variant durations are likewise opt-in: absent unless passed.
  assert(without.find("\"duration\"") == std::string::npos);

  // When supplied, each variant carries a "duration" object.
  const auto variant_durations = minera::compute_variant_durations(log, graph);
  const auto with_durations =
      minera::analysis_json("test", log, graph, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, &variant_durations);
  assert(with_durations.find("\"duration\"") != std::string::npos);
}

}  // namespace

int main() {
  timestamp_parses_iso8601_forms();
  importer_orders_events_by_instant_not_string();
  importer_keeps_document_order_for_equal_instants();
  performance_measures_edge_waiting_and_case_duration();
  performance_takes_the_median_of_varied_waits();
  performance_measures_the_case_arrival_rate();
  performance_ignores_untimed_logs();
  service_times_pair_lifecycle_starts_and_completes();
  service_times_absent_for_atomic_logs();
  variant_durations_summarize_per_variant();
  variant_durations_empty_without_timestamps();
  resource_network_counts_handovers();
  resource_network_breaks_the_chain_on_missing_resource();
  xes_importer_reads_the_resource_attribute();
  pnml_export_carries_the_net_structure();
  pnml_export_flags_silent_transitions();
  pnml_export_escapes_xml_metacharacters();
  bpmn_export_carries_block_structure();
  bpmn_export_uses_a_gateway_for_an_exclusive_choice();
  bpmn_export_escapes_xml_metacharacters();
  bpmn_net_export_keeps_a_sequence_gateway_free();
  bpmn_net_export_maps_places_and_transitions_to_gateways();
  bpmn_net_export_escapes_xml_metacharacters();
  json_export_carries_performance_and_resource_contracts();
  return 0;
}
