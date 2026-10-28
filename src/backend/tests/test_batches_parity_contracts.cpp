#include "batches/discovery_registry.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/json_export.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/log_conversion.hpp"

using namespace minera;
using namespace minera::batches_detail;

namespace {

EventAttribute cell(std::string key, std::string value) {
  return EventAttribute{std::move(key), std::move(value), AttributeKind::kString};
}

EventTable make_table(
    std::initializer_list<std::initializer_list<const char*>> values) {
  EventTable table;
  table.columns = {"case_ref", "phase", "team", "started_at", "finished_at",
                   "event_id"};
  for (const auto row_values : values) {
    auto it = row_values.begin();
    EventTable::Row row;
    row.emplace_back(cell("case_ref", *it++));
    row.emplace_back(cell("phase", *it++));
    row.emplace_back(cell("team", *it++));
    row.emplace_back(cell("started_at", *it++));
    row.emplace_back(cell("finished_at", *it++));
    row.emplace_back(cell("event_id", *it++));
    table.rows.push_back(std::move(row));
  }
  return table;
}

EventLog make_log(std::initializer_list<std::initializer_list<const char*>> values,
                  std::string trace_case = "source") {
  EventLog log;
  Trace trace;
  trace.case_id = std::move(trace_case);
  std::size_t index = 0;
  for (const auto row_values : values) {
    auto it = row_values.begin();
    Event event;
    event.case_id = trace.case_id;
    event.activity = *it++;
    event.resource = *it++;
    event.timestamp = *it++;
    event.attributes.emplace_back(cell("started_at", *it++));
    event.attributes.emplace_back(cell("case_ref", *it++));
    event.input_order = index++;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

BatchDiscoveryOptions table_options() {
  BatchDiscoveryOptions options;
  options.activity_attribute = "phase";
  options.resource_attribute = "team";
  options.case_id_attribute = "case_ref";
  options.timestamp_attribute = "finished_at";
  options.start_timestamp_attribute = "started_at";
  options.merge_distance_seconds = 0.0;
  return options;
}

const BatchGroup& only_group(const BatchDiscovery& discovery) {
  assert(discovery.groups.size() == 1);
  return discovery.groups.front();
}

void selector_components_cover_all_input_adapters() {
  const auto table = make_table({
      {"c1", "phase", "team", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "e1"},
      {"c2", "phase", "team", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "e2"},
  });
  auto options = table_options();
  options.event_id_attribute = "event_id";

  const auto pandas = BatchPandasDiscovery::apply(table, options);
  assert(pandas.variant == BatchDiscoveryVariant::kPandas);
  assert(only_group(pandas).event_count == 2);
  assert(only_group(pandas).batches.front().events.front().event_id == "e1");

  const auto polars = BatchPolarsDiscovery::apply(LazyEventTable::from_table(table),
                                                  options);
  assert(polars.variant == BatchDiscoveryVariant::kPolars);
  assert(only_group(polars).event_count == 2);

  const auto log = make_log({
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "c1"},
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "c2"},
  });
  auto log_options = options;
  log_options.event_id_attribute.clear();
  log_options.activity_attribute = "concept:name";
  log_options.resource_attribute = "org:resource";
  log_options.timestamp_attribute = "time:timestamp";
  log_options.case_id_attribute = "case_ref";
  const auto log_result = BatchLogDiscovery::apply(log, log_options);
  assert(log_result.variant == BatchDiscoveryVariant::kLog);
  assert(only_group(log_result).event_count == 2);

  assert(BatchDiscoverySelectorRegistry::parse("LOG") ==
         BatchDiscoverySelector::kLog);
  assert(BatchDiscoverySelectorRegistry::parse("pandas") ==
         BatchDiscoverySelector::kPandas);
  assert(BatchDiscoverySelectorRegistry::parse("PoLaRs") ==
         BatchDiscoverySelector::kPolars);
  assert(BatchDiscoverySelectorRegistry::all().size() == 3);
  const auto dispatched = BatchDiscoverySelectorRegistry::dispatch(
      EventDataView(table), BatchDiscoverySelector::PANDAS, options);
  assert(dispatched.variant == BatchDiscoveryVariant::kPandas);

  // The flat stream adapter must preserve the selector's input semantics.
  // In particular, PANDAS/POLARS need a rectangular conversion while LOG
  // must reconstruct trace boundaries before resolving event fields.
  const auto stream = event_log_to_event_stream(log);
  const auto stream_log = BatchDiscoverySelectorRegistry::dispatch(
      EventDataView(stream), BatchDiscoverySelector::LOG, log_options);
  assert(stream_log.variant == BatchDiscoveryVariant::kLog);
  assert(only_group(stream_log).event_count == 2);
  const auto stream_pandas = BatchDiscoverySelectorRegistry::dispatch(
      EventDataView(stream), BatchDiscoverySelector::PANDAS, log_options);
  assert(stream_pandas.variant == BatchDiscoveryVariant::kPandas);
  assert(only_group(stream_pandas).event_count == 2);
  const auto stream_polars = BatchDiscoverySelectorRegistry::dispatch(
      EventDataView(stream), BatchDiscoverySelector::POLARS, log_options);
  assert(stream_polars.variant == BatchDiscoveryVariant::kPolars);
  assert(only_group(stream_polars).event_count == 2);
}

void all_five_classifications_are_stable() {
  const auto table = make_table({
      {"s1", "sim", "r", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "s1"},
      {"s2", "sim", "r", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "s2"},
      {"b1", "start", "r", "1970-01-01T00:00:20Z",
       "1970-01-01T00:00:30Z", "b1"},
      {"b2", "start", "r", "1970-01-01T00:00:20Z",
       "1970-01-01T00:00:40Z", "b2"},
      {"e1", "end", "r", "1970-01-01T00:00:50Z",
       "1970-01-01T00:01:00Z", "e1"},
      {"e2", "end", "r", "1970-01-01T00:00:55Z",
       "1970-01-01T00:01:00Z", "e2"},
      {"q1", "seq", "r", "1970-01-01T00:01:10Z",
       "1970-01-01T00:01:20Z", "q1"},
      {"q2", "seq", "r", "1970-01-01T00:01:20Z",
       "1970-01-01T00:01:30Z", "q2"},
      {"c1", "con", "r", "1970-01-01T00:01:40Z",
       "1970-01-01T00:02:00Z", "c1"},
      {"c2", "con", "r", "1970-01-01T00:01:50Z",
       "1970-01-01T00:01:55Z", "c2"},
  });
  const auto result = BatchPandasDiscovery::apply(table, table_options());
  assert(result.groups.size() == 5);
  for (const auto& group : result.groups) {
    assert(group.batch_count == 1);
    assert(group.batches.size() == 1);
  }
  auto type_for = [&](const char* activity) {
    for (const auto& group : result.groups)
      if (group.activity == activity) return group.batches.front().type;
    return BatchType::kConcurrent;
  };
  assert(type_for("sim") == BatchType::kSimultaneous);
  assert(type_for("start") == BatchType::kBatchingOnStart);
  assert(type_for("end") == BatchType::kBatchingOnEnd);
  assert(type_for("seq") == BatchType::kSequential);
  assert(type_for("con") == BatchType::kConcurrent);
}

void merging_and_event_identity_match_contract() {
  const auto table = make_table({
      {"a", "merge", "r", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "m1"},
      {"b", "merge", "r", "1970-01-01T00:00:08Z",
       "1970-01-01T00:00:20Z", "m2"},
      {"c", "merge", "r", "1970-01-01T00:00:25Z",
       "1970-01-01T00:00:30Z", "m3"},
  });
  auto options = table_options();
  options.merge_distance_seconds = 5.0;
  options.event_id_attribute = "event_id";
  const auto merged = BatchPandasDiscovery::apply(table, options);
  assert(only_group(merged).batches.size() == 1);
  assert(only_group(merged).batches.front().events.size() == 3);
  options.merge_distance_seconds = 4.0;
  const auto split = BatchPandasDiscovery::apply(table, options);
  assert(only_group(split).batches.size() == 1);
  assert(only_group(split).batches.front().events.size() == 2);

  auto duplicate = make_table({
      {"same", "dup", "r", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "first"},
      {"same", "dup", "r", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "second"},
  });
  auto no_id = table_options();
  no_id.min_batch_size = 2;
  assert(BatchPandasDiscovery::apply(duplicate, no_id).groups.empty());
  no_id.event_id_attribute = "event_id";
  const auto with_id = BatchPandasDiscovery::apply(duplicate, no_id);
  assert(only_group(with_id).event_count == 2);
  assert(only_group(with_id).batches.front().events[0].event_id == "first");
  assert(only_group(with_id).batches.front().events[1].event_id == "second");
}

void custom_log_case_ids_are_resolved_per_event() {
  auto log = make_log({
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "case-a"},
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "case-b"},
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "case-c"},
  });
  log.traces.front().attributes.emplace_back(cell("case_ref", "trace-level"));
  BatchDiscoveryOptions options;
  options.case_id_attribute = "case_ref";
  options.start_timestamp_attribute = "started_at";
  options.merge_distance_seconds = 0.0;
  const auto result = BatchLogDiscovery::apply(log, options);
  assert(only_group(result).event_count == 3);
  assert(only_group(result).batches.front().events[0].case_id == "case-a");
  assert(only_group(result).batches.front().events[1].case_id == "case-b");
  assert(only_group(result).batches.front().events[2].case_id == "case-c");
}

void custom_log_case_ids_round_trip_to_json() {
  auto log = make_log({
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "case-a"},
      {"phase", "team", "1970-01-01T00:00:10Z",
       "1970-01-01T00:00:00Z", "case-b"},
  });
  // Deliberately make the trace-level value disagree with every event.  JSON
  // validation must use each event's configured selector, just as discovery.
  log.traces.front().attributes.emplace_back(cell("case_ref", "trace-level"));

  BatchDiscoveryOptions options;
  options.activity_attribute = "activity";
  options.resource_attribute = "resource";
  options.timestamp_attribute = "timestamp";
  options.start_timestamp_attribute = "started_at";
  options.case_id_attribute = "case_ref";
  const auto discovery = BatchLogDiscovery::apply(log, options);
  const auto graph = discover_directly_follows_graph(log);
  const auto json = analysis_json(
      "batch-event-case", log, graph, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      &discovery);
  assert(json.find("\"case_id\": \"case-a\"") != std::string::npos);
  assert(json.find("\"case_id\": \"case-b\"") != std::string::npos);
}

void imported_first_class_activity_and_resource_selectors_round_trip_to_json() {
  std::istringstream input(
      "case_ref,phase,team,started_at,finished_at\n"
      "c1,Alice,blue,1970-01-01T00:00:00Z,1970-01-01T00:00:10Z\n"
      "c2,Alice,blue,1970-01-01T00:00:00Z,1970-01-01T00:00:10Z\n");
  CsvImportOptions import_options;
  import_options.case_id_column = "case_ref";
  import_options.activity_column = "phase";
  import_options.resource_column = "team";
  import_options.timestamp_column = "finished_at";
  import_options.require_timestamp_column = true;
  const auto log = import_csv_event_log(input, import_options);
  assert(log.traces.size() == 2);
  assert(log.traces.front().events.front().activity_source_key == "phase");
  assert(log.traces.front().events.front().resource_source_key == "team");

  BatchDiscoveryOptions options;
  options.activity_attribute = "phase";
  options.resource_attribute = "team";
  options.timestamp_attribute = "finished_at";
  options.start_timestamp_attribute = "started_at";
  options.case_id_attribute = "case_ref";
  options.merge_distance_seconds = 0.0;
  const auto discovery = BatchLogDiscovery::apply(log, options);
  assert(discovery.groups.size() == 1);
  assert(only_group(discovery).activity == "Alice");
  assert(only_group(discovery).resource == "blue");

  const auto graph = discover_directly_follows_graph(log);
  const auto json = analysis_json(
      "batch-first-class-source-keys", log, graph,
      nullptr,  // tree
      nullptr,  // net
      nullptr,  // heuristics
      nullptr,  // conformance
      nullptr,  // performance
      nullptr,  // resources
      nullptr,  // variant durations
      nullptr,  // eventually follows
      nullptr,  // decisions
      nullptr,  // Petri decoration
      nullptr,  // performance spectrum
      nullptr,  // inductive net
      nullptr,  // inductive decoration
      nullptr,  // heuristic Petri net
      nullptr,  // heuristic decoration
      nullptr,  // temporal profile
      nullptr,  // minimum self distances
      nullptr,  // transition system
      nullptr,  // POWL
      nullptr,  // POWL Petri net
      nullptr,  // POWL decoration
      nullptr,  // log skeleton
      nullptr,  // log skeleton conformance
      nullptr,  // Declare
      nullptr,  // Declare conformance
      nullptr,  // footprints
      nullptr,  // DFG variants
      nullptr,  // correlation mining
      &discovery);
  assert(json.find("\"activity_label\": \"Alice\"") != std::string::npos);
  assert(json.find("\"resource\": \"blue\"") != std::string::npos);
  assert(json.find("\"activity_attribute\": \"phase\"") !=
         std::string::npos);
  assert(json.find("\"resource_attribute\": \"team\"") !=
         std::string::npos);

  auto mismatched = discovery;
  mismatched.groups.front().resource = "red";
  bool threw = false;
  try {
    (void)analysis_json(
        "batch-first-class-source-keys", log, graph,
        nullptr,  // tree
        nullptr,  // net
        nullptr,  // heuristics
        nullptr,  // conformance
        nullptr,  // performance
        nullptr,  // resources
        nullptr,  // variant durations
        nullptr,  // eventually follows
        nullptr,  // decisions
        nullptr,  // Petri decoration
        nullptr,  // performance spectrum
        nullptr,  // inductive net
        nullptr,  // inductive decoration
        nullptr,  // heuristic Petri net
        nullptr,  // heuristic decoration
        nullptr,  // temporal profile
        nullptr,  // minimum self distances
        nullptr,  // transition system
        nullptr,  // POWL
        nullptr,  // POWL Petri net
        nullptr,  // POWL decoration
        nullptr,  // log skeleton
        nullptr,  // log skeleton conformance
        nullptr,  // Declare
        nullptr,  // Declare conformance
        nullptr,  // footprints
        nullptr,  // DFG variants
        nullptr,  // correlation mining
        &mismatched);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void imported_custom_case_selectors_reach_tabular_adapters() {
  std::istringstream input(
      "case_ref,phase,team,started_at,finished_at\n"
      "case-a,Alice,blue,1970-01-01T00:00:00Z,1970-01-01T00:00:10Z\n"
      "case-b,Alice,blue,1970-01-01T00:00:00Z,1970-01-01T00:00:10Z\n");
  CsvImportOptions import_options;
  import_options.case_id_column = "case_ref";
  import_options.activity_column = "phase";
  import_options.resource_column = "team";
  import_options.timestamp_column = "finished_at";
  import_options.require_timestamp_column = true;
  const auto log = import_csv_event_log(input, import_options);

  BatchDiscoveryOptions options;
  options.activity_attribute = "phase";
  options.resource_attribute = "team";
  options.timestamp_attribute = "finished_at";
  options.start_timestamp_attribute = "started_at";
  options.case_id_attribute = "case_ref";
  options.merge_distance_seconds = 0.0;

  const auto pandas = BatchDiscoverySelectorRegistry::dispatch(
      EventDataView(log), BatchDiscoverySelector::PANDAS, options);
  assert(pandas.variant == BatchDiscoveryVariant::kPandas);
  assert(pandas.groups.size() == 1);
  assert(only_group(pandas).activity == "Alice");
  assert(only_group(pandas).resource == "blue");
  assert(only_group(pandas).event_count == 2);
  assert(only_group(pandas).batches.front().events[0].case_id == "case-a");
  assert(only_group(pandas).batches.front().events[1].case_id == "case-b");

  const auto polars = BatchDiscoverySelectorRegistry::dispatch(
      EventDataView(log), BatchDiscoverySelector::POLARS, options);
  assert(polars.variant == BatchDiscoveryVariant::kPolars);
  assert(polars.groups.size() == 1);
  assert(only_group(polars).activity == "Alice");
  assert(only_group(polars).resource == "blue");
  assert(only_group(polars).event_count == 2);
  assert(only_group(polars).batches.front().events[0].case_id == "case-a");
  assert(only_group(polars).batches.front().events[1].case_id == "case-b");
}

void required_columns_are_reported() {
  auto table = make_table({
      {"c1", "phase", "team", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "e1"},
      {"c2", "phase", "team", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "e2"},
  });
  table.columns[1] = "wrong";
  auto options = table_options();
  bool threw = false;
  try {
    (void)BatchPandasDiscovery::apply(table, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  table = make_table({
      {"c1", "phase", "team", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "e1"},
      {"c2", "phase", "team", "1970-01-01T00:00:00Z",
       "1970-01-01T00:00:10Z", "e2"},
  });
  table.rows.front().pop_back();
  threw = false;
  try {
    (void)BatchPandasDiscovery::apply(table, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

std::string at_seconds(int seconds) {
  char text[32];
  std::snprintf(text, sizeof(text), "2021-03-01T%02d:%02d:%02dZ",
                seconds / 3600, seconds / 60 % 60, seconds % 60);
  return text;
}

EventTable timed_table(const std::vector<int>& seconds) {
  EventTable table;
  table.columns = {"case_ref", "phase", "team", "started_at", "finished_at",
                   "event_id"};
  for (std::size_t index = 0; index < seconds.size(); ++index) {
    const auto stamp = at_seconds(seconds[index]);
    EventTable::Row row;
    row.emplace_back(cell("case_ref", "c"));
    row.emplace_back(cell("phase", "p"));
    row.emplace_back(cell("team", "t"));
    row.emplace_back(cell("started_at", stamp));
    row.emplace_back(cell("finished_at", stamp));
    row.emplace_back(cell("event_id", std::to_string(index)));
    table.rows.push_back(std::move(row));
  }
  return table;
}

std::vector<std::vector<double>> batch_offsets(const BatchDiscovery& discovery) {
  std::vector<std::vector<double>> result;
  for (const auto& batch : only_group(discovery).batches) {
    std::vector<double> offsets;
    for (const auto& event : batch.events)
      offsets.push_back(event.start - only_group(discovery).batches.front().events.front().start);
    result.push_back(offsets);
  }
  return result;
}

void reference_interval_merging_contract() {
  // The reference heapifies one interval per event in input order and merges
  // near intervals by heap-pushing each union back, so input order matters and
  // intervals farther apart than the distance can join.
  auto options = table_options();
  options.merge_distance_seconds = 60.0;
  options.min_batch_size = 1;
  const auto unsorted = BatchPandasDiscovery::apply(timed_table({0, 6000, 3000}), options);
  assert((batch_offsets(unsorted) ==
          std::vector<std::vector<double>>{{0.0}, {3000.0, 6000.0}}));
  const auto sorted = BatchPandasDiscovery::apply(timed_table({0, 3000, 6000}), options);
  assert(only_group(sorted).batches.size() == 3);

  options.merge_distance_seconds = 300.0;
  const auto far = BatchPandasDiscovery::apply(timed_table({0, 10, 1000, 5000}), options);
  assert((batch_offsets(far) ==
          std::vector<std::vector<double>>{{0.0, 10.0}, {1000.0, 5000.0}}));

  // A negative distance and a minimum of zero are accepted; a repeated event
  // then stays in two single-event batches.
  options.merge_distance_seconds = -10.0;
  options.min_batch_size = 0;
  assert(only_group(BatchPandasDiscovery::apply(timed_table({0, 0}), options))
             .batches.size() == 2);

  // Polars keeps a null resource as its own group.
  auto table = timed_table({0, 0});
  table.rows[0][2].reset();
  table.rows[1][2].reset();
  table.rows[1][0] = cell("case_ref", "d");
  options.min_batch_size = 2;
  options.merge_distance_seconds = 0.0;
  const auto polars = BatchPolarsDiscovery::apply(LazyEventTable::from_table(table), options);
  assert(only_group(polars).resource_is_null);
  assert(only_group(polars).batches.front().events.size() == 2);
  // pandas drops rows with a missing resource instead.
  assert(BatchPandasDiscovery::apply(table, options).groups.empty());

  // pandas reads a microsecond datetime column as its units over 1e9, drops
  // rows with a missing activity or resource and keeps a missing case as nan.
  auto micro = timed_table({0, 30, 6000});
  micro.dtypes = {"str", "str", "str", "datetime64[us, UTC]",
                  "datetime64[us, UTC]", "str"};
  micro.rows[1][0].reset();
  micro.rows[2][2].reset();
  options.merge_distance_seconds = 0.05;
  options.min_batch_size = 2;
  const auto scaled = BatchPandasDiscovery::apply(micro, options);
  const auto& scaled_batch = only_group(scaled).batches.at(0);
  assert(scaled_batch.events.size() == 2);
  assert(std::abs(scaled_batch.complete - scaled_batch.start - 0.03) < 1e-6);
  assert(scaled_batch.start < 2e6);
  assert(scaled_batch.events[1].case_id == "nan");
}

}  // namespace

int main() {
  selector_components_cover_all_input_adapters();
  all_five_classifications_are_stable();
  merging_and_event_identity_match_contract();
  custom_log_case_ids_are_resolved_per_event();
  custom_log_case_ids_round_trip_to_json();
  imported_first_class_activity_and_resource_selectors_round_trip_to_json();
  imported_custom_case_selectors_reach_tabular_adapters();
  required_columns_are_reported();
  reference_interval_merging_contract();
}
