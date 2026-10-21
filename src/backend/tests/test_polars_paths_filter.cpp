#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/polars_paths_filter.hpp"

namespace {
using namespace minera;

EventAttribute null_value(std::string key) {
  EventAttribute value{std::move(key), ""}; value.is_null = true; return value;
}
template <class Integer>
EventAttribute integer_value(std::string key, Integer number) {
  EventAttribute value{std::move(key), std::to_string(number), AttributeKind::kInt};
  value.typed_value = number; return value;
}
EventAttribute float_value(std::string key, double number) {
  EventAttribute value{std::move(key), std::to_string(number), AttributeKind::kFloat};
  value.typed_value = number; return value;
}
EventAttribute boolean_value(std::string key, bool boolean) {
  EventAttribute value{std::move(key), boolean ? "true" : "false", AttributeKind::kBoolean};
  value.typed_value = boolean; return value;
}
EventAttribute date_value(std::string key, std::string instant) {
  EventAttribute value{std::move(key), instant, AttributeKind::kDate};
  value.typed_value = NativeDate{instant}; return value;
}
EventAttribute collection_value(std::string key, AttributeContainerKind kind,
                                std::vector<EventAttribute> children) {
  EventAttribute value{std::move(key), ""}; value.container = kind;
  value.children = std::move(children); return value;
}

void same_frame(const EventTable& actual, const EventTable& expected, const std::string& context) {
  if (actual.columns != expected.columns || actual.dtypes != expected.dtypes || actual.rows != expected.rows ||
      actual.attributes != expected.attributes || actual.metadata != expected.metadata ||
      actual.index != expected.index || actual.categorical_dtypes != expected.categorical_dtypes)
    throw std::runtime_error("Full Polars paths frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars paths floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsPathsFilterError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars paths diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars paths type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars paths diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

#include "polars_paths_filter_public_vectors.inc"

void native_sorted_complete_context_and_inert_target() {
  EventTable source;
  source.columns = {"case:concept:name", "concept:name", "time:timestamp", "other", "payload"};
  source.dtypes = {"String", "String", "Datetime(time_unit='us', time_zone=None)", "String", "Float64"};
  source.rows = {{EventAttribute{source.columns[0], "c"}, EventAttribute{source.columns[1], "B"}, date_value(source.columns[2], "2038-01-01T00:00:01.999999"), EventAttribute{"other", "wrong"}, float_value("payload", -0.0)},
                 {EventAttribute{source.columns[0], "c"}, EventAttribute{source.columns[1], "A"}, date_value(source.columns[2], "2038-01-01T00:00:01"), EventAttribute{"other", "wrong"}, float_value("payload", 0.0)}};
  source.attributes = {{"context", "retained independently"}};
  source.metadata.properties = {{"property", "retained"}};
  source.rows[0][0]->first_class_role = FirstClassAttributeRole::kCaseId;
  source.rows[0][0]->first_class_state = FirstClassAttributeState::kPresent;
  source.rows[0][0]->first_class_source_key = "literal case provenance";
  const auto before = source;
  auto expected = source; std::reverse(expected.rows.begin(), expected.rows.end());
  const auto pair = collection_value("", AttributeContainerKind::kTuple, {{"", "A"}, {"", "B"}});
  const auto paths = collection_value("", AttributeContainerKind::kList, {pair});
  std::size_t scans = 0;
  const auto filtered = filter_polars_paths(counted_source(source, scans), paths);
  assert(scans == 0);
  same_frame(filtered.materialize(), expected, "sorted whole rows and exact native context"); assert(scans == 1);
  auto independent = filtered.materialize(); assert(scans == 2); independent.rows[0][0]->value = "changed";
  same_frame(source, before, "independent lazy source");
  PolarsPathsFilterOptions options; options.target_attribute_key = EventAttribute{"", "other"};
  auto empty = expected; empty.rows.clear();
  same_frame(filter_polars_paths(source, paths, options), empty, "plain source and shifted target attributes differ");
  same_frame(filter_polars_path_performance(source, pair, options), expected, "performance ignores target attribute");
  options.target_attribute_key = null_value("");
  same_frame(filter_polars_path_performance(source, pair, options), expected, "performance ignores invalid target selector");
  options.min_performance = integer_value("", 1);
  same_frame(filter_polars_path_performance(source, pair, options), empty, "fraction truncation precedes performance comparison");
  const auto comma_left = collection_value("", AttributeContainerKind::kTuple, {{"", "A,B"}, {"", "C"}});
  const auto comma_right = collection_value("", AttributeContainerKind::kTuple, {{"", "A"}, {"", "B,C"}});
  auto comma = source; comma.rows[0][1]->value = "C"; comma.rows[1][1]->value = "A,B";
  same_frame(filter_polars_paths(comma, collection_value("", AttributeContainerKind::kList, {comma_left})),
             filter_polars_paths(comma, collection_value("", AttributeContainerKind::kList, {comma_right})), "retained comma-encoding collision");
  same_frame(source, before, "all native controls preserve source/provenance");
}

void native_enum_dictionary_guard_without_sort_comparisons() {
  EventTable source;
  source.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  source.dtypes = {"Enum(categories=['case'])", "String", "Int64"};
  source.rows = {{EventAttribute{source.columns[0], "case"}, EventAttribute{source.columns[1], "A"}, integer_value(source.columns[2], 0)}};
  const auto path = collection_value("", AttributeContainerKind::kList, {collection_value("", AttributeContainerKind::kTuple, {{"", "A"}, {"", "B"}})});
  const auto rejected = [&](const EventTable& input, const std::string& expected) {
    std::size_t scans = 0; const auto filtered = filter_polars_paths(counted_source(input, scans), path); assert(scans == 0);
    bool caught = false;
    try { (void)filtered.materialize(); } catch (const std::invalid_argument& e) { caught = true; assert(std::string(e.what()).find(expected) != std::string::npos); }
    assert(caught && scans == 1);
  };
  rejected(source, "require an Enum dictionary");
  source.categorical_dtypes.resize(3); EventTableCategoricalDtype dictionary;
  dictionary.categories_dtype = "String"; dictionary.ordered = true; dictionary.categories = {{"", "different"}};
  source.categorical_dtypes[0] = dictionary;
  rejected(source, "Enum cell absent");
}
}  // namespace

int main() {
  try {
    retained_polars_paths_filter_cases();
    native_sorted_complete_context_and_inert_target();
    native_enum_dictionary_guard_without_sort_comparisons();
    std::cout << "Polars paths: 238 retained calls; 153 exact complete projected frames and 85 full diagnostic phases/messages; native sorted/lazy/context controls passed\n";
  } catch (const std::exception& error) { std::cerr << '\n' << error.what() << '\n'; return 1; }
}
