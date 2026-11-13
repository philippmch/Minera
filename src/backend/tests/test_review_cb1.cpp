// Review CB1: the reference OCPN playout (pm4py's extensive variant) on
// OCEL logs with repeated event_object rows (review-harness round-ca
// ocel/gen2.py seeds 35 and 11, opened through /api/ocel/sqlite). Seed 35
// has 756,756 complete sequences of 15 silent steps under both IM and IMd;
// the search used to explore its dead ends again and again (647 s, pm4py
// 81 s) and now skips known dead states (a few seconds). Seed 11 pins the
// multiset of visible traces pm4py returns (its order follows Python set
// iteration and changes with PYTHONHASHSEED) and the max_traces cut, which
// keeps the first traces of the full list and stops the search there.
// Values are taken from pm4py 2.7.22 black-box on the same SQLite files.
#include "minera/ocel.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/ocpn_reference_playout.hpp"

#include <functional>
#include <iostream>
#include <map>
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

const char* const kSeed35 = R"json({"ocelVersion": "2.0", "objectTypes": [{"name": "t0", "attributes": [{"name": "v", "type": "float"}]}, {"name": "t1", "attributes": [{"name": "v", "type": "float"}]}, {"name": "t2", "attributes": [{"name": "v", "type": "float"}]}], "eventTypes": [{"name": "a", "attributes": [{"name": "x", "type": "float"}]}, {"name": "b", "attributes": [{"name": "x", "type": "float"}]}, {"name": "c", "attributes": [{"name": "x", "type": "float"}]}], "events": [{"id": "e0", "type": "b", "time": "2033-06-07 06:41:00+00:00", "attributes": [{"name": "x", "value": 0.0}], "relationships": [{"objectId": "o0", "qualifier": "r"}, {"objectId": "o0", "qualifier": "q"}]}, {"id": "e1", "type": "c", "time": "2033-06-07 06:41:00+00:00", "attributes": [{"name": "x", "value": 0.0}], "relationships": [{"objectId": "o2", "qualifier": "q"}, {"objectId": "o1", "qualifier": "r"}, {"objectId": "o0", "qualifier": "r"}]}, {"id": "e2", "type": "c", "time": "2033-06-07 06:41:59+00:00", "attributes": [{"name": "x", "value": 5.0}], "relationships": [{"objectId": "o2", "qualifier": "r"}, {"objectId": "o0", "qualifier": "q"}, {"objectId": "o1", "qualifier": "r"}]}, {"id": "e3", "type": "b", "time": "2033-06-08 06:41:59+00:00", "attributes": [{"name": "x", "value": 3.0}], "relationships": [{"objectId": "o2", "qualifier": "q"}, {"objectId": "o1", "qualifier": "q"}, {"objectId": "o0", "qualifier": "r"}, {"objectId": "o0", "qualifier": "r"}, {"objectId": "o1", "qualifier": "r"}]}, {"id": "e4", "type": "b", "time": "2033-06-08 06:42:00+00:00", "attributes": [{"name": "x", "value": 4.0}], "relationships": [{"objectId": "o0", "qualifier": "r"}, {"objectId": "o0", "qualifier": "r"}]}, {"id": "e5", "type": "c", "time": "2033-06-09 06:42:00+00:00", "attributes": [{"name": "x", "value": 4.0}], "relationships": [{"objectId": "o1", "qualifier": "r"}]}, {"id": "e6", "type": "c", "time": "2033-06-09 06:42:01+00:00", "attributes": [{"name": "x", "value": 2.0}], "relationships": [{"objectId": "o2", "qualifier": "q"}, {"objectId": "o0", "qualifier": "r"}, {"objectId": "o1", "qualifier": "q"}]}, {"id": "e7", "type": "a", "time": "2033-06-09 06:42:02+00:00", "attributes": [{"name": "x", "value": 3.0}], "relationships": [{"objectId": "o2", "qualifier": "q"}, {"objectId": "o2", "qualifier": "q"}, {"objectId": "o1", "qualifier": "r"}, {"objectId": "o0", "qualifier": "r"}]}, {"id": "e8", "type": "b", "time": "2033-06-09 06:43:01+00:00", "attributes": [{"name": "x", "value": 3.0}], "relationships": [{"objectId": "o0", "qualifier": "r"}, {"objectId": "o0", "qualifier": "q"}]}, {"id": "e9", "type": "c", "time": "2033-06-09 07:43:01+00:00", "attributes": [{"name": "x", "value": 5.0}], "relationships": [{"objectId": "o1", "qualifier": "r"}, {"objectId": "o0", "qualifier": "q"}, {"objectId": "o0", "qualifier": "r"}, {"objectId": "o2", "qualifier": "r"}]}, {"id": "e10", "type": "c", "time": "2033-06-09 07:43:01+00:00", "attributes": [{"name": "x", "value": 1.0}], "relationships": [{"objectId": "o2", "qualifier": "r"}, {"objectId": "o2", "qualifier": "q"}]}, {"id": "e11", "type": "c", "time": "2033-06-09 07:43:01+00:00", "attributes": [{"name": "x", "value": 0.0}], "relationships": [{"objectId": "o0", "qualifier": "r"}, {"objectId": "o2", "qualifier": "r"}]}], "objects": [{"id": "o2", "type": "t0", "attributes": [{"name": "v", "time": "1970-01-01T00:00:00Z", "value": 5.0}], "relationships": []}, {"id": "o1", "type": "t1", "attributes": [{"name": "v", "time": "1970-01-01T00:00:00Z", "value": 7.0}, {"name": "v", "value": 2.0, "time": "2033-06-07 06:41:00.700351+00:00"}], "relationships": [{"objectId": "o2", "qualifier": "s"}, {"objectId": "o2", "qualifier": "p"}]}, {"id": "o0", "type": "t2", "attributes": [{"name": "v", "time": "1970-01-01T00:00:00Z", "value": 4.0}, {"name": "v", "value": 8.0, "time": "2033-06-07 06:41:59+00:00"}, {"name": "v", "value": 1.0, "time": "2033-06-07 06:41:59.915080+00:00"}], "relationships": [{"objectId": "o2", "qualifier": "p"}]}], "minera:relationOrder": ["e1", "e0", "e1", "e0", "e1", "e2", "e2", "e2", "e3", "e3", "e3", "e3", "e3", "e4", "e4", "e5", "e6", "e6", "e6", "e7", "e7", "e7", "e7", "e8", "e8", "e9", "e10", "e11", "e9", "e9", "e11", "e9", "e10"], "minera:repeatedRelations": true, "minera:o2oOrder": ["o1", "o0", "o1"]})json";
const char* const kSeed11 = R"json({"ocelVersion":"2.0","objectTypes":[{"name":"t0","attributes":[{"name":"v","type":"float"}]},{"name":"t1","attributes":[{"name":"v","type":"float"}]}],"eventTypes":[{"name":"a","attributes":[{"name":"x","type":"float"}]},{"name":"b","attributes":[{"name":"x","type":"float"}]},{"name":"c","attributes":[{"name":"x","type":"float"}]},{"name":"d","attributes":[{"name":"x","type":"float"}]}],"events":[{"id":"e0","type":"a","time":"2033-06-08 06:41:00+00:00","attributes":[{"name":"x","value":5.0}],"relationships":[{"objectId":"o1","qualifier":"r"},{"objectId":"o4","qualifier":"q"},{"objectId":"o0","qualifier":"q"}]},{"id":"e1","type":"c","time":"2033-06-08 06:41:00+00:00","attributes":[{"name":"x","value":3.0}],"relationships":[{"objectId":"o0","qualifier":"r"},{"objectId":"o2","qualifier":"r"}]},{"id":"e2","type":"c","time":"2033-06-08 06:41:01+00:00","attributes":[{"name":"x","value":2.0}],"relationships":[{"objectId":"o1","qualifier":"q"}]},{"id":"e3","type":"a","time":"2033-06-08 07:41:01+00:00","attributes":[{"name":"x","value":0.0}],"relationships":[{"objectId":"o0","qualifier":"r"}]},{"id":"e4","type":"d","time":"2033-06-08 07:41:02+00:00","attributes":[{"name":"x","value":0.0}],"relationships":[{"objectId":"o4","qualifier":"q"}]},{"id":"e5","type":"a","time":"2033-06-09 07:41:02+00:00","attributes":[{"name":"x","value":0.0}],"relationships":[{"objectId":"o1","qualifier":"r"}]}],"objects":[{"id":"o1","type":"t0","attributes":[{"name":"v","time":"1970-01-01T00:00:00Z","value":6.0},{"name":"v","value":0.0,"time":"2033-06-07 06:41:00+00:00"},{"name":"v","value":9.0,"time":"2033-06-08 06:41:00+00:00"}],"relationships":[]},{"id":"o2","type":"t0","attributes":[{"name":"v","time":"1970-01-01T00:00:00Z","value":0.0}],"relationships":[]},{"id":"o4","type":"t0","attributes":[{"name":"v","time":"1970-01-01T00:00:00Z","value":7.0},{"name":"v","value":8.0,"time":"2033-06-07 07:41:00+00:00"}],"relationships":[]},{"id":"o0","type":"t1","attributes":[{"name":"v","time":"1970-01-01T00:00:00Z","value":1.0},{"name":"v","value":1.0,"time":"2033-06-07 06:41:59+00:00"},{"name":"v","value":9.0,"time":"2033-06-08 06:41:00+00:00"}],"relationships":[]}],"minera:relationOrder":["e0","e1","e0","e0","e1","e2","e3","e4","e5"],"minera:objectChangeOrder":[["o1","v"],["o0","v"],["o4","v"],["o1","v"],["o0","v"]]})json";

OcpnGraph mined(const char* json, bool imd) {
  OcpnDiscoveryOptions options;
  if (imd) options.variant = OcpnInductiveVariant::kDfg;
  return discover_ocpn(parse_ocel_json(json), options);
}

std::string visible(const OcpnReferenceTrace& trace) {
  std::string out;
  for (const auto& step : trace) {
    if (!step.activity) continue;
    if (!out.empty()) out += ' ';
    out += *step.activity + ':';
    for (std::size_t i = 0; i < step.objects.size(); ++i) out += (i ? "+" : "") + step.objects[i];
  }
  return out;
}

// Visible projections and step counts of every complete sequence.
std::string summary(const OcpnGraph& graph) {
  std::map<std::string, std::size_t> traces;
  std::map<std::size_t, std::size_t> lengths;
  std::size_t total = 0;
  reference_ocpn_extensive_visit(graph, {}, [&](const OcpnReferenceTrace& trace) {
    ++traces[visible(trace)];
    ++lengths[trace.size()];
    ++total;
    return true;
  });
  std::string out = std::to_string(total);
  for (const auto& [k, n] : traces) out += " [" + k + "]=" + std::to_string(n);
  for (const auto& [k, n] : lengths) out += " len" + std::to_string(k) + "=" + std::to_string(n);
  return out;
}

void playout_dead_ends() {
  check("seed 35 im", summary(mined(kSeed35, false)), "756756 []=756756 len15=756756");
  check("seed 35 imd", summary(mined(kSeed35, true)), "756756 []=756756 len15=756756");
  check("seed 35 exists", reference_ocpn_trace_exists(mined(kSeed35, false)) ? "true" : "false", "true");
  check("seed 11 im", summary(mined(kSeed11, false)),
        "1184 [a:t1_0]=592 [a:t1_0 d:t0_0]=548 [d:t0_0 a:t1_0]=44 len8=224 len10=960");
  check("seed 11 imd", summary(mined(kSeed11, true)), "70 []=35 [d:t0_0]=35 len7=70");
}

std::string sequences(const OcpnPlayoutResult& result) {
  std::string out = std::to_string(result.sequences.size()) + (result.trace_exists ? " exists" : " none");
  for (const auto& sequence : result.sequences) {
    out += " [";
    for (std::size_t i = 0; i < sequence.size(); ++i) {
      out += (i ? " " : "") + sequence[i].activity;
      for (const auto& [type, id] : sequence[i].objects) out += ":" + id;
    }
    out += "]";
  }
  return out + " events=" + std::to_string(result.log.events.size());
}

void playout_max_traces() {
  const auto graph = mined(kSeed11, false);
  OcpnPlayoutOptions options;
  options.event_binding = OcpnEventBinding::kReference;
  options.max_bindings_per_activity = 1;
  options.return_sequences = true;
  options.max_traces = 0;
  const auto all = ocpn_extensive_playout_result(graph, options);
  check("seed 11 all", std::to_string(all.sequences.size()), "1184");
  options.max_traces = 3;
  auto first = all;
  first.sequences.resize(3);
  std::size_t events = 0;
  for (const auto& sequence : first.sequences) events += sequence.size();
  const auto cut = ocpn_extensive_playout_result(graph, options);
  check("seed 11 max_traces=3", sequences(cut),
        sequences(first).substr(0, sequences(first).rfind(" events=")) + " events=" + std::to_string(events));
}

}  // namespace

int main() {
  playout_dead_ends();
  playout_max_traces();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review CB1 tests passed\n";
  return 0;
}
