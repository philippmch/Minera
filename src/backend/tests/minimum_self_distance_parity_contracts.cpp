#include "minera/minimum_self_distance.hpp"

#include <cassert>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace minera;

EventLog make_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "outer";
  for (const auto [index, activity, case_id] :
       {std::tuple<std::size_t, const char*, const char*>{0, "A", "x"},
        {1, "B", "y"}, {2, "A", "z"}}) {
    Event event;
    event.case_id = case_id;
    event.activity = activity;
    event.input_order = index;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

EventAttribute cell(const char* key, const char* value) {
  return EventAttribute{key, value};
}

EventTable make_table() {
  EventTable table;
  table.columns = {"case_key", "activity_key"};
  table.rows = {{cell("case_key", "c1"), cell("activity_key", "A")},
               {cell("case_key", "c2"), cell("activity_key", "X")},
               {cell("case_key", "c1"), cell("activity_key", "B")},
               {cell("case_key", "c2"), cell("activity_key", "Y")},
               {cell("case_key", "c1"), cell("activity_key", "A")},
               {cell("case_key", "c2"), cell("activity_key", "X")}};
  return table;
}

EventStream make_stream(const EventTable& table) {
  EventStream stream;
  for (const auto& row : table.rows) {
    EventRecord record;
    record.fields = {row[0].value(), row[1].value()};
    stream.events.push_back(std::move(record));
  }
  return stream;
}

EventLog make_first_class_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "outer";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "case_col";
  for (const auto [index, activity] :
       {std::tuple<std::size_t, const char*>{0, "A"},
        {1, "B"}, {2, "A"}}) {
    Event event;
    event.case_id = "selected";
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = "case_col";
    event.activity = activity;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.activity_source_key = "activity_col";
    event.activity_attribute = EventAttribute{
        "activity_col", activity, AttributeKind::kString, false, {},
        AttributeContainerKind::kScalar, {}, FirstClassAttributeRole::kActivity,
        FirstClassAttributeState::kPresent, "activity_col"};
    const std::string projected = index == 1 ? "middle" : "outer";
    event.resource = projected;
    event.resource_state = FirstClassAttributeState::kPresent;
    event.resource_source_key = "resource_col";
    event.resource_attribute = EventAttribute{
        "resource_col", projected, AttributeKind::kString, false, {},
        AttributeContainerKind::kScalar, {}, FirstClassAttributeRole::kResource,
        FirstClassAttributeState::kPresent, "resource_col"};
    event.lifecycle = projected;
    event.lifecycle_state = FirstClassAttributeState::kPresent;
    event.lifecycle_source_key = "lifecycle_col";
    event.lifecycle_attribute = EventAttribute{
        "lifecycle_col", projected, AttributeKind::kString, false, {},
        AttributeContainerKind::kScalar, {},
        FirstClassAttributeRole::kLifecycle,
        FirstClassAttributeState::kPresent, "lifecycle_col"};
    event.input_order = index;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

MinimumSelfDistanceOptions custom_options() {
  MinimumSelfDistanceOptions options;
  options.activity_key = "activity_key";
  options.case_id_key = "case_key";
  return options;
}

void minimum_self_distance_log_boundary_contract() {
  const auto result = MinimumSelfDistanceLogDiscovery::apply(make_log());
  assert(result.size() == 1);
  assert(result.front().activity == "A");
  assert(result.front().distance == 1);
  assert((result.front().witnesses == std::vector<std::string>{"B"}));
}

void minimum_self_distance_flat_adapter_contract() {
  const auto table = make_table();
  const auto stream = make_stream(table);
  const auto options = custom_options();
  const auto eager = MinimumSelfDistancePandasDiscovery::apply(table, options);
  const auto streamed = MinimumSelfDistanceLogDiscovery::apply(stream, options);
  assert(eager.size() == 2);
  assert(eager[0].activity == "A" && eager[0].distance == 1);
  assert((eager[0].witnesses == std::vector<std::string>{"B"}));
  assert(streamed == eager);
  const auto generic = discover_minimum_self_distances(EventDataView(table),
                                                       options);
  assert(generic == eager);
}

void minimum_self_distance_polars_adapter_contract() {
  const auto table = make_table();
  const auto options = custom_options();
  const auto eager = MinimumSelfDistancePandasDiscovery::apply(table, options);
  const auto lazy = MinimumSelfDistancePolarsDiscovery::apply(
      LazyEventTable::from_table(table), options);
  assert(lazy.size() == 2);
  assert(lazy[0].activity == "A" && lazy[0].distance == 1);
  assert((lazy[0].witnesses == std::vector<std::string>{"B"}));
  assert(lazy == eager);
}

void minimum_self_distance_first_class_selector_contract() {
  const auto log = make_first_class_log();
  MinimumSelfDistanceOptions options;
  options.activity_key = "activity_col";
  options.case_id_key = "case_col";
  const auto direct = discover_minimum_self_distances(log, options);
  assert(direct.size() == 1);
  assert(direct.front().activity == "A" && direct.front().distance == 1);
  assert((direct.front().witnesses == std::vector<std::string>{"B"}));

  const auto pandas = MinimumSelfDistanceSelectorRegistry::dispatch(
      log, MinimumSelfDistanceSelector::PANDAS, options);
  const auto polars = MinimumSelfDistanceSelectorRegistry::dispatch(
      log, MinimumSelfDistanceSelector::POLARS, options);
  assert(pandas == direct);
  assert(polars == direct);

  for (const auto& selector : {std::string{"resource_col"},
                               std::string{"org:resource"},
                               std::string{"lifecycle_col"},
                               std::string{"lifecycle:transition"}}) {
    options.activity_key = selector;
    const auto direct = discover_minimum_self_distances(log, options);
    assert(direct.size() == 1);
    assert(direct.front().activity == "outer");
    assert(direct.front().distance == 1);
    assert((direct.front().witnesses ==
            std::vector<std::string>{"middle"}));

    // Imported logs retain the selected field's original source spelling in
    // first-class cell metadata. Both eager and lazy flat adapters must honor
    // either that spelling or the canonical alias for the same role.
    const auto pandas = MinimumSelfDistanceSelectorRegistry::dispatch(
        log, MinimumSelfDistanceSelector::PANDAS, options);
    const auto polars = MinimumSelfDistanceSelectorRegistry::dispatch(
        log, MinimumSelfDistanceSelector::POLARS, options);
    assert(pandas == direct);
    assert(polars == direct);
  }
}

EventTable mismatched_first_class_activity_table() {
  auto table = make_table();
  table.columns[1] = "activity";
  for (auto& row : table.rows) {
    row[1]->key = "activity";
    row[1]->first_class_role = FirstClassAttributeRole::kResource;
    row[1]->first_class_state = FirstClassAttributeState::kPresent;
    row[1]->first_class_source_key = "activity";
  }
  return table;
}

void minimum_self_distance_flat_role_mismatch_contract() {
  const auto table = mismatched_first_class_activity_table();
  auto options = custom_options();
  options.activity_key = "activity";

  bool pandas_rejected = false;
  try {
    (void)MinimumSelfDistancePandasDiscovery::apply(table, options);
  } catch (const std::invalid_argument&) {
    pandas_rejected = true;
  }
  assert(pandas_rejected);

  bool polars_rejected = false;
  try {
    (void)MinimumSelfDistancePolarsDiscovery::apply(
        LazyEventTable::from_table(table), options);
  } catch (const std::invalid_argument&) {
    polars_rejected = true;
  }
  assert(polars_rejected);
}

void minimum_self_distance_generic_dispatch_contract() {
  const auto table = make_table();
  const auto stream = make_stream(table);
  const auto options = custom_options();
  const auto pandas_expected =
      MinimumSelfDistancePandasDiscovery::apply(table, options);
  const auto pandas_actual = MinimumSelfDistanceSelectorRegistry::dispatch(
      EventDataView(stream), MinimumSelfDistanceSelector::PANDAS, options);
  assert(pandas_actual == pandas_expected);

  const auto polars_expected = MinimumSelfDistancePolarsDiscovery::apply(
      LazyEventTable::from_table(table), options);
  const auto polars_actual = MinimumSelfDistanceSelectorRegistry::dispatch(
      EventDataView(table), MinimumSelfDistanceSelector::POLARS, options);
  assert(polars_actual == polars_expected);

  const auto log_expected =
      MinimumSelfDistanceLogDiscovery::apply(stream, options);
  const auto log_actual = MinimumSelfDistanceSelectorRegistry::dispatch(
      EventDataView(stream), MinimumSelfDistanceSelector::LOG, options);
  assert(log_actual == log_expected);
}

void minimum_self_distance_witness_utility_contract() {
  const auto log = make_log();
  const std::map<std::string, std::uint64_t> requested{{"A", 1},
                                                        {"MISSING", 2},
                                                        {"ZERO", 0}};
  const auto witnesses = derive_msd_witnesses(log, requested);
  assert((witnesses.at("A") == MinimumSelfDistanceWitnessMap::mapped_type{"B"}));
  assert(!witnesses.contains("MISSING"));
  assert(!witnesses.contains("ZERO"));
  const auto discovered = discover_minimum_self_distances(log);
  const auto discovered_witnesses =
      derive_minimum_self_distance_witnesses(log, discovered);
  assert(discovered_witnesses.at("A") ==
         MinimumSelfDistanceWitnessMap::mapped_type{"B"});
  assert(MinimumSelfDistanceWitnessDerivation::derive(log, requested) ==
         witnesses);
  const auto class_witnesses =
      MinimumSelfDistanceWitnesses::derive(log, discovered);
  assert(class_witnesses.at("A") ==
         MinimumSelfDistanceWitnessMap::mapped_type{"B"});
}

Event nullable_event(MinimumSelfDistanceActivity label,
                     const std::string& key = "concept:name") {
  Event event;
  EventAttribute attribute;
  attribute.key = key;
  attribute.value = label.value_or("");
  attribute.is_null = !label.has_value();
  event.attributes.push_back(attribute);
  if (key == "concept:name") {
    event.activity = attribute.value;
    event.activity_state = attribute.is_null ? FirstClassAttributeState::kNull
                                             : FirstClassAttributeState::kPresent;
    event.activity_source_key = key;
    event.activity_attribute = attribute;
  } else {
    event.activity = "ignored";
    event.activity_state = FirstClassAttributeState::kPresent;
  }
  return event;
}

void minimum_self_distance_nullable_and_supplied_witness_contract() {
  for (const auto& key : {std::string{"concept:name"}, std::string{"phase"}}) {
    EventLog log;
    Trace first;
    first.events = {nullable_event(std::nullopt, key), nullable_event("", key),
                    nullable_event(std::nullopt, key)};
    Trace second;
    second.events = {nullable_event("A", key), nullable_event(std::nullopt, key),
                     nullable_event("A", key)};
    log.traces = {first, second};
    const auto before = log;
    MinimumSelfDistanceOptions options;
    options.activity_key = key;
    const auto distances = discover_minimum_self_distances(log, options);
    assert(distances.size() == 2);
    assert(distances[0].activity_is_null && distances[0].activity.empty());
    assert(distances[0].distance == 1 && !distances[0].witnesses_include_null);
    assert((distances[0].witnesses == std::vector<std::string>{""}));
    assert(!distances[1].activity_is_null && distances[1].activity == "A");
    assert(distances[1].distance == 1 && distances[1].witnesses.empty());
    assert(distances[1].witnesses_include_null);
    const MinimumSelfDistanceMap requested{{std::nullopt, 1}, {"", 1},
                                            {"A", 1}, {"unobserved", 1}};
    const auto witnesses = derive_msd_witnesses(log, requested, options);
    assert(witnesses.size() == 3 && witnesses.at("").empty());
    assert(witnesses.at(std::nullopt) ==
           MinimumSelfDistanceWitnessMap::mapped_type{""});
    assert(witnesses.at("A") ==
           MinimumSelfDistanceWitnessMap::mapped_type{std::nullopt});
    assert(!witnesses.contains("unobserved"));
    assert(derive_minimum_self_distance_witnesses(log, distances, options).size() == 2);
    const MinimumSelfDistanceMap omitted{{std::nullopt, 0}, {"", -1}, {"A", 0}};
    assert(derive_msd_witnesses(log, omitted, options).empty());
    assert(derive_msd_witnesses(log, MinimumSelfDistanceMap{}, options).empty());
    assert(log.traces[0].events[0].activity_state == before.traces[0].events[0].activity_state);
    assert(log.traces[0].events[0].attributes == before.traces[0].events[0].attributes);
    bool automatic_rejected = false;
    try {
      (void)derive_msd_witnesses(log, options);
    } catch (const std::logic_error& error) {
      automatic_rejected = std::string(error.what()) == "'dict' object has no attribute 'apply'";
    }
    assert(automatic_rejected);
  }

  EventLog log;
  Trace trace;
  trace.events = {nullable_event("A"), nullable_event("A"),
                  nullable_event("B"), nullable_event("C"), nullable_event("A")};
  log.traces.push_back(trace);
  assert(discover_minimum_self_distances(log).front().distance == 0);
  const auto witnesses = derive_msd_witnesses(log, MinimumSelfDistanceMap{{"A", 2}});
  assert((witnesses.at("A") == MinimumSelfDistanceWitnessMap::mapped_type{"B", "C"}));
  assert(derive_msd_witnesses(EventLog{}, MinimumSelfDistanceMap{{"A", 1}}).empty());
}

void minimum_self_distance_selector_contract() {
  const auto log = make_log();
  assert(MinimumSelfDistanceSelectorRegistry::all().size() == 3);
  for (const auto selector : {MinimumSelfDistanceSelector::LOG,
                              MinimumSelfDistanceSelector::PANDAS,
                              MinimumSelfDistanceSelector::POLARS}) {
    assert(MinimumSelfDistanceSelectorRegistry::supports(selector));
    const auto selected = MinimumSelfDistanceSelectorRegistry::dispatch(
        log, selector);
    assert(selected.size() == 1 && selected.front().distance == 1);
  }
  const auto parsed = MinimumSelfDistanceSelectorRegistry::parse("polars");
  assert(parsed && *parsed == MinimumSelfDistanceSelector::POLARS);
  assert(minimum_self_distance_variant_name(*parsed) == "POLARS");
  assert(parse_minimum_self_distance_variant("LOG") ==
         MinimumSelfDistanceSelector::LOG);
  const auto data = MinimumSelfDistanceSelectorRegistry::dispatch(
      EventDataView(make_table()), MinimumSelfDistanceSelector::PANDAS,
      custom_options());
  assert(data.size() == 2);
}

}  // namespace

int main() {
  minimum_self_distance_log_boundary_contract();
  minimum_self_distance_flat_adapter_contract();
  minimum_self_distance_polars_adapter_contract();
  minimum_self_distance_first_class_selector_contract();
  minimum_self_distance_flat_role_mismatch_contract();
  minimum_self_distance_generic_dispatch_contract();
  minimum_self_distance_witness_utility_contract();
  minimum_self_distance_nullable_and_supplied_witness_contract();
  minimum_self_distance_selector_contract();
  return 0;
}
