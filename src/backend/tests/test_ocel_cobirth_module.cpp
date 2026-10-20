#include "minera/ocel_transform.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace minera;

namespace {
using Edge = std::pair<std::string, std::string>;

std::string bytes(std::string_view hex) {
  const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  assert(hex.size() % 2 == 0);
  std::string value;
  for (std::size_t i = 0; i < hex.size(); i += 2)
    value.push_back(static_cast<char>(16 * digit(hex[i]) + digit(hex[i + 1])));
  return value;
}

struct CapturedCase {
  const char* name;
  unsigned mask;
  std::string change;
  std::vector<std::size_t> relation_order;
  std::vector<Edge> expected;
  std::optional<std::string> missing_event;
};

const std::vector<CapturedCase>& cases() {
  // Actual direct public set returns / KeyError event identities, never
  // expectations computed by the native graph formula.
  // Capture: build/ocel-cobirth-focused-public-actual-1/records.jsonl
  // SHA-256: d29bcb4bb3f1c23d2050c7cff35bcc305a21526be78f1e6bbb8a3bbfd2e72a9e
  // Custom OCEL column bindings are normalized into typed native ID fields.
  static const std::vector<CapturedCase> values = {
      {"incidence-0", 0, "", {}, {}, bytes("6530")},
      {"incidence-1", 1, "", {}, {}, bytes("6531")},
      {"incidence-2", 2, "", {}, {}, bytes("6531")},
      {"incidence-3", 3, "", {}, {}, bytes("6531")},
      {"incidence-4", 4, "", {}, {}, bytes("6531")},
      {"incidence-5", 5, "", {}, {}, bytes("6531")},
      {"incidence-6", 6, "", {}, {}, bytes("6531")},
      {"incidence-7", 7, "", {}, {}, bytes("6531")},
      {"incidence-8", 8, "", {}, {}, bytes("6530")},
      {"incidence-9", 9, "", {}, {}, std::nullopt},
      {"incidence-10", 10, "", {}, {}, std::nullopt},
      {"incidence-11", 11, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-12", 12, "", {}, {}, std::nullopt},
      {"incidence-13", 13, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-14", 14, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-15", 15, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-16", 16, "", {}, {}, bytes("6530")},
      {"incidence-17", 17, "", {}, {}, std::nullopt},
      {"incidence-18", 18, "", {}, {}, std::nullopt},
      {"incidence-19", 19, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-20", 20, "", {}, {}, std::nullopt},
      {"incidence-21", 21, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-22", 22, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-23", 23, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-24", 24, "", {}, {}, bytes("6530")},
      {"incidence-25", 25, "", {}, {}, std::nullopt},
      {"incidence-26", 26, "", {}, {}, std::nullopt},
      {"incidence-27", 27, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-28", 28, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-29", 29, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-30", 30, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-31", 31, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-32", 32, "", {}, {}, bytes("6530")},
      {"incidence-33", 33, "", {}, {}, std::nullopt},
      {"incidence-34", 34, "", {}, {}, std::nullopt},
      {"incidence-35", 35, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-36", 36, "", {}, {}, std::nullopt},
      {"incidence-37", 37, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-38", 38, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-39", 39, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-40", 40, "", {}, {}, bytes("6530")},
      {"incidence-41", 41, "", {}, {}, std::nullopt},
      {"incidence-42", 42, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-43", 43, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-44", 44, "", {}, {}, std::nullopt},
      {"incidence-45", 45, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-46", 46, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-47", 47, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-48", 48, "", {}, {}, bytes("6530")},
      {"incidence-49", 49, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-50", 50, "", {}, {}, std::nullopt},
      {"incidence-51", 51, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-52", 52, "", {}, {}, std::nullopt},
      {"incidence-53", 53, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-54", 54, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-55", 55, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-56", 56, "", {}, {}, bytes("6530")},
      {"incidence-57", 57, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-58", 58, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-59", 59, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-60", 60, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"incidence-61", 61, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"incidence-62", 62, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"incidence-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"parameters-none-0", 0, "", {}, {}, bytes("6530")},
      {"parameters-empty-0", 0, "", {}, {}, bytes("6530")},
      {"parameters-irrelevant-0", 0, "", {}, {}, bytes("6530")},
      {"parameters-none-7", 7, "", {}, {}, bytes("6531")},
      {"parameters-empty-7", 7, "", {}, {}, bytes("6531")},
      {"parameters-irrelevant-7", 7, "", {}, {}, bytes("6531")},
      {"parameters-none-51", 51, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"parameters-empty-51", 51, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"parameters-irrelevant-51", 51, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"parameters-none-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"parameters-empty-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"parameters-irrelevant-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"duplicate-identical", 63, "duplicate-identical", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"duplicate-qualified", 63, "duplicate-qualified", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"relation-reversed", 63, "relation-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"events-reversed", 63, "events-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"objects-reversed", 63, "objects-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"timestamps-reversed", 63, "timestamps-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"unicode-ids", 63, "unicode-ids", {}, {{bytes("20ceb120"), bytes("f09f9880")}, {bytes("20ceb120"), bytes("71756f746564220a")}, {bytes("71756f746564220a"), bytes("f09f9880")}}, std::nullopt},
      {"empty-object-id", 63, "empty-object-id", {}, {{bytes(""), bytes("6f31")}, {bytes(""), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"empty-event-id", 63, "empty-event-id", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"unused-records", 63, "unused-records", {}, {}, bytes("756e72656c61746564")},
      {"objects-only", 63, "objects-only", {}, {}, std::nullopt},
      {"events-only", 63, "events-only", {}, {}, bytes("6530")},
      {"empty", 63, "empty", {}, {}, std::nullopt},
      {"custom-columns", 21, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt},
      {"order-relations-reversed", 51, "relation-reversed", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"order-events-reversed", 51, "events-reversed", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"order-timestamps-reversed", 51, "timestamps-reversed", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"order-relations-interleaved", 51, "", {0, 2, 1, 3}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"order-relation-leading-later-event", 51, "", {2, 0, 3, 1}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
      {"order-events-and-relations-reversed", 51, "events-and-relations-reversed", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt},
      {"order-tied-timestamps", 51, "tied-timestamps", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt},
  };
  return values;
}

Ocel model(const CapturedCase& spec) {
  std::vector<std::string> objects = {"o0", "o1", "o2"};
  std::vector<std::string> events = {"e0", "e1"};
  if (spec.change == "unicode-ids") {
    objects = {bytes("20ceb120"), bytes("f09f9880"), "quoted\"\n"};
    events = {" event,0 ", "event->1"};
  }
  if (spec.change == "empty-object-id") objects.front().clear();
  if (spec.change == "empty-event-id") events.front().clear();
  Ocel value;
  for (const auto& id : objects) value.objects.push_back({id, "T", {}});
  for (std::size_t i = 0; i < events.size(); ++i)
    value.events.push_back({events[i], "A", "2026-01-01T00:00:0" +
        std::to_string(i) + "Z", {}});
  for (unsigned e = 0; e < 2; ++e)
    for (unsigned o = 0; o < 3; ++o)
      if (spec.mask & (1u << (3 * e + o)))
        value.relations.push_back({events[e], objects[o], ""});
  if (spec.change.starts_with("duplicate")) {
    auto duplicate = value.relations.front();
    if (spec.change == "duplicate-qualified") duplicate.qualifier = "other role";
    value.relations.push_back(std::move(duplicate));
  }
  if (!spec.relation_order.empty()) {
    const auto original = value.relations;
    value.relations.clear();
    for (const auto index : spec.relation_order)
      value.relations.push_back(original.at(index));
  }
  if (spec.change == "relation-reversed")
    std::reverse(value.relations.begin(), value.relations.end());
  if (spec.change == "events-and-relations-reversed") {
    std::reverse(value.events.begin(), value.events.end());
    std::reverse(value.relations.begin(), value.relations.end());
  }
  if (spec.change == "tied-timestamps")
    for (auto& event : value.events) event.timestamp = "2026-01-01T00:00:00Z";
  if (spec.change == "events-reversed")
    std::reverse(value.events.begin(), value.events.end());
  if (spec.change == "objects-reversed")
    std::reverse(value.objects.begin(), value.objects.end());
  if (spec.change == "timestamps-reversed")
    for (std::size_t i = 0; i < value.events.size(); ++i)
      value.events[i].timestamp = "2026-01-01T00:00:0" + std::to_string(1-i) + "Z";
  if (spec.change == "unused-records") {
    value.objects.push_back({"isolated", "unused", {}});
    value.events.push_back({"unrelated", "unused", "2026-01-01T00:00:09Z", {}});
  }
  if (spec.change == "objects-only" || spec.change == "events-only" ||
      spec.change == "empty") {
    value.relations.clear();
    if (spec.change != "events-only") value.events.clear();
    if (spec.change != "objects-only") value.objects.clear();
  }
  return value;
}

void check(const Ocel& source, std::vector<Edge> expected,
           OcelEnrichmentGraph graph = OcelEnrichmentGraph::kCobirth,
           const std::optional<std::string>& missing_event = std::nullopt) {
  const auto before = serialize_oc_playout_workbench_ocel_json(source);
  try {
    const auto result = discover_ocel_object_graph(source, graph);
    if (missing_event)
      throw std::runtime_error("expected missing-event diagnostic for " + *missing_event);
    std::sort(expected.begin(), expected.end());
    assert(result.graph == OcelEnrichmentGraph::kCobirth);
    if (result.edges != expected) throw std::runtime_error("captured edge set differs");
    std::vector<std::string> ids;
    for (const auto& object : source.objects) ids.push_back(object.id);
    std::sort(ids.begin(), ids.end());
    assert(result.object_ids == ids);
  } catch (const std::out_of_range& error) {
    if (!missing_event) throw;
    assert(error.what() == "OCEL cobirth event has no object relations: " + *missing_event);
  }
  assert(serialize_oc_playout_workbench_ocel_json(source) == before);
}

unsigned retained_public_cobirth_cases() {
  unsigned failures = 0;
  for (const auto& spec : cases()) {
    try { check(model(spec), spec.expected, OcelEnrichmentGraph::kCobirth, spec.missing_event); }
    catch (const std::exception& error) {
      ++failures;
      std::cerr << spec.name << ": " << error.what() << '\n';
    }
  }
  std::cout << "Retained direct public cobirth cases: "
            << cases().size() - failures << '/' << cases().size() << " passed\n";
  return failures;
}

void metadata_and_literal_identity_invariance() {
  auto source = model(cases().at(63));
  const auto expected = cases().at(63).expected;
  const std::string odd = bytes("2000ffeda08022");
  for (auto& event : source.events) {
    event.activity = odd;
    event.timestamp = "invalid timestamp is irrelevant to first event-table encounter";
    event.attributes = {{odd, nullptr}, {odd, true}, {odd, std::int64_t{-7}},
                        {odd, -0.0}, {odd, odd}};
  }
  for (auto& object : source.objects) {
    object.type = odd;
    object.attributes = {{odd, std::numeric_limits<double>::infinity()}};
  }
  for (auto& relation : source.relations) relation.qualifier = odd;
  source.o2o = {{"o0", "o2", odd}};
  source.e2e = {{"e0", "e1", odd}};
  source.object_changes = {{"o0", odd, odd, odd, odd}};
  source.globals[odd] = odd;
  source.event_type_attributes[odd][odd] = odd;
  source.object_type_attributes[odd][odd] = odd;
  source.table_columns.events = std::vector<std::string>{odd};
  source.table_columns.objects = std::vector<std::string>{};
  source.table_columns.relations = std::vector<std::string>{odd, odd};
  check(source, expected);
  // All ordinary string bytes remain identity data, including embedded NULs.
  source.objects[0].id = odd;
  for (auto& relation : source.relations)
    if (relation.object_id == "o0") relation.object_id = odd;
  std::vector<Edge> renamed;
  for (auto edge : expected) {
    if (edge.first == "o0") edge.first = odd;
    if (edge.second == "o0") edge.second = odd;
    if (edge.second < edge.first) std::swap(edge.first, edge.second);
    renamed.push_back(std::move(edge));
  }
  check(source, renamed);
}

void preserved_validation_and_other_selectors() {
  const auto rejects = [](const Ocel& source, OcelEnrichmentGraph graph) {
    const auto before = serialize_oc_playout_workbench_ocel_json(source);
    bool threw = false;
    try { (void)discover_ocel_object_graph(source, graph); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    assert(serialize_oc_playout_workbench_ocel_json(source) == before);
  };
  auto source = model(cases().at(63));
  source.objects.push_back(source.objects.front());
  rejects(source, OcelEnrichmentGraph::kCobirth);
  source = model(cases().at(63));
  source.events.push_back(source.events.front());
  rejects(source, OcelEnrichmentGraph::kCobirth);
  source = model(cases().at(63));
  source.relations.front().object_id = "absent";
  rejects(source, OcelEnrichmentGraph::kCobirth);
  source = model(cases().at(63));
  source.relations.front().event_id = "absent";
  rejects(source, OcelEnrichmentGraph::kCobirth);
  source = model(cases().at(63));
  rejects(source, static_cast<OcelEnrichmentGraph>(-1));

  for (const auto& spec : cases()) {
    if (spec.change != "empty-object-id" && spec.change != "empty-event-id") continue;
    source = model(spec);
    // All five direct relations now retain empty literal identities.
    for (auto graph : {OcelEnrichmentGraph::kInteraction, OcelEnrichmentGraph::kCobirth,
                       OcelEnrichmentGraph::kCodeath, OcelEnrichmentGraph::kDescendants,
                       OcelEnrichmentGraph::kInheritance}) {
      const auto result = discover_ocel_object_graph(source, graph);
      assert(result.graph == graph);
      if (graph == OcelEnrichmentGraph::kDescendants || graph == OcelEnrichmentGraph::kInheritance)
        assert(result.edges.empty());
      else
        assert(result.edges.size() == 3);
    }
    bool threw = false;
    try { (void)enrich_ocel(source); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const auto failures = retained_public_cobirth_cases();
  if (argc == 2 && std::string_view(argv[1]) == "--captured-only")
    return failures ? 1 : 0;
  if (failures) return 1;
  metadata_and_literal_identity_invariance();
  preserved_validation_and_other_selectors();
  std::cout << "Cobirth metadata/identity invariance and preserved validation passed\n";
}
