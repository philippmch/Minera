#include "minera/polars_attribute_repetition_filter.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

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

void same_frame(const EventTable& actual, const EventTable& expected, const std::string& context) {
  if (actual.columns != expected.columns || actual.dtypes != expected.dtypes || actual.rows != expected.rows ||
      actual.attributes != expected.attributes || actual.metadata != expected.metadata ||
      actual.index != expected.index || actual.categorical_dtypes != expected.categorical_dtypes)
    throw std::runtime_error("Full Polars frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& left = actual.rows[row][column];
      const auto& right = expected.rows[row][column];
      if (!left || !right) continue;
      const auto* a = std::get_if<double>(&left->typed_value);
      const auto* b = std::get_if<double>(&right->typed_value);
      if (a && b && std::bit_cast<std::uint64_t>(*a) != std::bit_cast<std::uint64_t>(*b))
        throw std::runtime_error("Polars floating cell bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
                const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsAttributeRepetitionError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      std::cerr << "Wrong Polars diagnostic for " << context << ": " << error.type << '/' << error.phase << "\n";
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars diagnostic: " + context);
}

LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans;
    for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

#include "polars_attribute_repetition_public_vectors.inc"

void lazy_metadata_and_no_zero_match_cases() {
  EventTable source;
  source.columns = {"case:concept:name", "concept:name", "payload"};
  source.dtypes = {"String", "String", "Float64"};
  source.rows = {{EventAttribute{"case:concept:name", "zero"}, EventAttribute{"concept:name", "B"}, float_value("payload", -0.0)},
                 {EventAttribute{"case:concept:name", "twice"}, EventAttribute{"concept:name", "A"}, float_value("payload", 1.0)},
                 {EventAttribute{"case:concept:name", "twice"}, EventAttribute{"concept:name", "A"}, float_value("payload", 2.0)}};
  source.attributes = {{"source metadata", "retained"}};
  source.metadata.properties = {{"property", "whole native context"}};
  const auto before = source;
  std::size_t scans = 0;
  const auto lazy_source = counted_source(source, scans);
  PolarsAttributeRepetitionOptions options;
  options.minimum = TokenInteger(0);
  const auto filtered = filter_polars_attribute_value_repetition(lazy_source, EventAttribute{"", "A"}, options);
  assert(scans == 0 && filtered.attributes() == source.attributes && filtered.metadata() == source.metadata);
  auto expected = source; expected.rows.erase(expected.rows.begin());
  same_frame(filtered.materialize(), expected, "native lazy metadata");
  same_frame(filtered.materialize(), expected, "native restartable collection");
  assert(scans == 2);
  same_frame(source, before, "native input preservation");
  const auto missing = filter_polars_attribute_value_repetition(lazy_source, EventAttribute{"", "unseen"}, options);
  auto empty = source; empty.rows.clear();
  same_frame(missing.materialize(), empty, "zero matches remain excluded");
  options.attribute_key = TokenInteger(0);
  error_is([&] { (void)filter_polars_attribute_value_repetition(lazy_source, EventAttribute{"", "A"}, options); },
           "TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got 'int'.", "native immediate selector validation");
  assert(scans == 3);
}

}  // namespace

int main() {
  retained_polars_repetition_cases();
  lazy_metadata_and_no_zero_match_cases();
  std::cout << "Polars attribute repetition: 105 retained full frames/errors, apply/collect phases, actual literal columns, restartable lazy rows and native metadata passed\n";
}
