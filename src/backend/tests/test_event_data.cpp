#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

#include "minera/event_data.hpp"
#include "minera/log_conversion.hpp"

namespace {

minera::EventAttribute cell(std::string key, std::string value,
                            minera::AttributeKind kind =
                                minera::AttributeKind::kString,
                            bool is_null = false) {
  return {std::move(key), std::move(value), kind, is_null};
}

minera::EventTable sample_table() {
  minera::EventTable table;
  table.columns = {"case:id", "concept:name", "note", "nullable"};
  table.attributes.push_back(cell("source", "table"));
  table.metadata.extensions.push_back({"demo", "d", "urn:demo"});
  table.metadata.classifiers.push_back({"activity", {"concept:name"}});
  table.metadata.event_globals.push_back(cell("nullable", "", {}, true));
  table.rows = {
      {cell("case:id", "c2", minera::AttributeKind::kId),
       cell("concept:name", "A"), cell("note", ""),
       cell("nullable", "", {}, true)},
      {cell("case:id", "c1", minera::AttributeKind::kId),
       cell("concept:name", "B"), std::nullopt, std::nullopt},
      {cell("case:id", "c2", minera::AttributeKind::kId),
       cell("concept:name", "C"), cell("note", "present"), std::nullopt},
  };
  return table;
}

void table_validation_and_null_contract() {
  auto table = sample_table();
  table.validate();
  const auto stream = minera::event_table_to_event_stream(table);
  assert(stream.events.size() == 3);
  assert(stream.events[0].fields.size() == 4);
  assert(stream.events[0].field("nullable")->is_null);
  assert(stream.events[0].field("note")->value.empty());
  assert(stream.events[1].field("note") == nullptr);

  minera::LogConversionOptions remove_nulls;
  remove_nulls.remove_null_fields = true;
  const auto filtered =
      minera::event_table_to_event_stream(table, remove_nulls);
  assert(filtered.events[0].field("nullable") == nullptr);
  assert(filtered.events[0].field("note") != nullptr);

  auto duplicate = table;
  duplicate.columns[1] = duplicate.columns[0];
  bool threw = false;
  try {
    duplicate.validate();
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  auto short_row = table;
  short_row.rows[0].pop_back();
  threw = false;
  try {
    short_row.validate();
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void table_prefixes_infer_only_known_extensions() {
  minera::EventTable table;
  table.columns = {"concept:name", "time:timestamp", "org:resource",
                   "lifecycle:transition", "artifactlifecycle:transition",
                   "micro:trace", "semantic:concept", "swcomm:message",
                   "swevent:type", "swtelemetry:value", "case:id",
                   "custom:value"};
  table.rows.push_back(
      {cell("concept:name", "A"), cell("time:timestamp", "2026-01-01T00:00:00Z",
                                       minera::AttributeKind::kDate),
       cell("org:resource", "alice"), cell("lifecycle:transition", "complete"),
       cell("artifactlifecycle:transition", "complete"),
       cell("micro:trace", "x"), cell("semantic:concept", "y"),
       cell("swcomm:message", "z"), cell("swevent:type", "event"),
       cell("swtelemetry:value", "1"),
       cell("case:id", "c1", minera::AttributeKind::kId),
       cell("custom:value", "x")});
  const auto stream = minera::event_table_to_event_stream(table);
  assert(stream.metadata.extensions.size() == 10);
  assert(stream.metadata.extensions[0].prefix == "concept");
  assert(stream.metadata.extensions[1].prefix == "time");
  assert(stream.metadata.extensions[2].prefix == "org");
  assert(stream.metadata.extensions[3].prefix == "lifecycle");
  assert(stream.metadata.extensions[4].prefix == "artifactlifecycle");
  assert(stream.metadata.extensions[5].prefix == "micro");
  assert(stream.metadata.extensions[6].prefix == "semantic");
  assert(stream.metadata.extensions[7].prefix == "swcomm");
  assert(stream.metadata.extensions[8].prefix == "swevent");
  assert(stream.metadata.extensions[9].prefix == "swtelemetry");

  table.metadata.extensions.push_back({"declared", "org", "urn:declared"});
  const auto authoritative = minera::event_table_to_event_stream(table);
  assert(authoritative.metadata.extensions.size() == 10);
  const auto org = std::find_if(
      authoritative.metadata.extensions.begin(),
      authoritative.metadata.extensions.end(),
      [](const auto& extension) { return extension.prefix == "org"; });
  assert(org != authoritative.metadata.extensions.end());
  assert(org->name == "declared");
  assert(org->uri == "urn:declared");
}

void lazy_scan_is_restartable_and_bounded() {
  auto table = sample_table();
  const auto lazy = minera::LazyEventTable::from_table(table);
  assert(lazy.valid());
  assert(lazy.row_count() == 3);
  std::size_t first = 0;
  lazy.scan([&](const auto&) { ++first; });
  std::size_t second = 0;
  lazy.scan([&](const auto&) { ++second; });
  assert(first == 3 && second == 3);
  assert(lazy.materialize() .rows == table.rows);

  bool threw = false;
  try {
    (void)lazy.materialize(2);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);

  minera::LazyEventTable malformed(
      table.columns,
      [](const auto& consume) {
        minera::EventTable::Row row;
        consume(row);
      });
  threw = false;
  try {
    (void)malformed.materialize();
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void invalid_default_lazy_table_is_rejected() {
  const minera::LazyEventTable invalid;
  assert(!invalid.valid());

  bool threw = false;
  try {
    invalid.scan([](const auto&) {});
  } catch (const std::invalid_argument& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "lazy event table: scanner is empty");
  }
  assert(threw);

  threw = false;
  try {
    (void)invalid.materialize();
  } catch (const std::invalid_argument& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "lazy event table: scanner is empty");
  }
  assert(threw);
}

void lazy_inputs_apply_null_cleanup_across_generic_paths() {
  const auto table = sample_table();
  const auto lazy = minera::LazyEventTable::from_table(table);
  const minera::EventDataView view(lazy);
  minera::LogConversionOptions options;
  options.remove_null_fields = true;

  const auto converted_table = minera::event_data_to_table(view, options);
  assert(converted_table.rows.size() == table.rows.size());
  assert(converted_table.columns == table.columns);
  assert(converted_table.attributes == table.attributes);
  assert(converted_table.metadata == table.metadata);
  for (const auto& row : converted_table.rows) {
    for (const auto& cell : row)
      assert(!cell || !cell->is_null);
  }

  const minera::EventDataView eager_view(table);
  const auto direct_stream =
      minera::event_table_to_event_stream(table, options);
  const auto eager_cleaned = minera::event_data_to_table(eager_view, options);
  assert(eager_cleaned.columns == table.columns);
  assert(eager_cleaned.attributes == table.attributes);
  assert(eager_cleaned.metadata == table.metadata);
  const auto eager_stream =
      minera::event_data_to_event_stream(eager_view, options);
  assert(eager_stream.attributes == table.attributes);
  assert(eager_stream.metadata == direct_stream.metadata);

  const auto converted_stream =
      minera::event_data_to_event_stream(view, options);
  assert(converted_stream.attributes == table.attributes);
  assert(converted_stream.metadata == direct_stream.metadata);
  assert(converted_stream.metadata.extensions.size() == 2);
  assert(converted_stream.metadata.extensions[0].prefix == "d");
  assert(converted_stream.metadata.extensions[1].prefix == "concept");
  for (const auto& event : converted_stream.events)
    assert(event.field("nullable") == nullptr);

  // Returning a lazy form keeps the source restartable while filtering null
  // cells when its rows are consumed.
  const auto filtered_lazy = minera::event_data_to_lazy_table(view, options);
  assert(filtered_lazy.valid());
  assert(filtered_lazy.row_count() == table.rows.size());
  const auto filtered_materialized = filtered_lazy.materialize();
  assert(filtered_materialized.columns == table.columns);
  assert(filtered_materialized.attributes == table.attributes);
  assert(filtered_materialized.metadata == table.metadata);
  for (const auto& row : filtered_materialized.rows) {
    for (const auto& cell : row)
      assert(!cell || !cell->is_null);
  }

  options.max_rows_to_materialize = 2;
  const auto capped_lazy = minera::event_data_to_lazy_table(view, options);
  bool threw = false;
  try {
    (void)capped_lazy.materialize();
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)minera::event_data_to_table(view, options);
  } catch (const std::length_error& error) {
    threw = true;
    assert(std::string(error.what()) ==
           "lazy event table: row materialization cap exceeded");
  }
  assert(threw);
}

void tagged_views_and_compatibility_aliases() {
  auto table = sample_table();
  minera::LogConversionTable old_name = table;
  minera::ConvertedEventStream stream =
      minera::event_table_to_event_stream(old_name);
  minera::EventDataView table_view(table);
  assert(table_view.kind() == minera::EventDataKind::kEventTable);
  const auto round_trip = minera::event_data_to_table(table_view);
  assert(round_trip.columns == table.columns);
  assert(round_trip.rows == table.rows);

  minera::EventDataView stream_view(stream);
  assert(minera::event_data_to_event_stream(stream_view).events.size() == 3);
  const auto lazy = minera::event_data_to_lazy_table(table_view);
  minera::EventDataView lazy_view(lazy);
  assert(minera::event_data_to_table(lazy_view).rows == table.rows);
}

}  // namespace

int main() {
  table_validation_and_null_contract();
  table_prefixes_infer_only_known_extensions();
  lazy_scan_is_restartable_and_bounded();
  invalid_default_lazy_table_is_rejected();
  lazy_inputs_apply_null_cleanup_across_generic_paths();
  tagged_views_and_compatibility_aliases();
}
