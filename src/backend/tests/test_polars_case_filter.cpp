#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/polars_case_filter.hpp"

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
    throw std::runtime_error("Full Polars case filter frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars case filter floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsCaseFilterError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars case filter diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars case filter type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars case filter diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

bool same_frame_value(const EventTable& actual, const EventTable& expected) {
  try { same_frame(actual, expected, "admitted complete frame"); return true; }
  catch (const std::runtime_error&) { return false; }
}

#include "polars_case_filter_public_vectors.inc"

void native_lazy_complete_context_and_rank() {
  EventTable source;
  source.columns = {"case:concept:name", "time:timestamp", "payload"};
  source.dtypes = {"String", "Datetime(time_unit='us', time_zone=None)", "Float64"};
  source.rows = {{EventAttribute{source.columns[0], "rare"}, date_value(source.columns[1], "2037-01-01T00:00:01.1"), float_value(source.columns[2], -0.0)},
                 {EventAttribute{source.columns[0], "frequent"}, date_value(source.columns[1], "2037-01-01T00:00:02.9"), float_value(source.columns[2], 0.0)},
                 {EventAttribute{source.columns[0], "frequent"}, date_value(source.columns[1], "2037-01-01T00:00:01.1"), float_value(source.columns[2], -0.0)}};
  source.attributes = {{"context", "independent"}};
  source.metadata.extensions.push_back({"example", "ex", "https://example.test/"});
  source.rows[1][0]->first_class_role = FirstClassAttributeRole::kCaseId;
  source.rows[1][0]->first_class_state = FirstClassAttributeState::kPresent;
  source.rows[1][0]->first_class_source_key = "original case";
  const auto before = source;
  auto expected = source; expected.rows = {source.rows[1], source.rows[2]};
  std::size_t scans = 0;
  const auto counted = counted_source(source, scans);
  const auto filtered = filter_polars_ncases(counted, integer_value("", 1));
  assert(scans == 0);
  same_frame(filtered.materialize(), expected, "frequency rank precedes encounter order"); assert(scans == 1);
  auto independent = filtered.materialize(); assert(scans == 2);
  independent.rows[0][0]->value = "changed";
  same_frame(source, before, "independent lazy source/context");
  same_frame(filter_polars_case_size(source, integer_value("", 2)), expected, "complete size rows and context");
  same_frame(filter_polars_case_performance(source, integer_value("", 1), integer_value("", 1)), expected,
             "integer-second range before inclusive performance bounds");
  PolarsCaseFilterOptions options; options.business_hours = boolean_value("", true);
  error_is([&] { (void)filter_polars_case_performance(counted, integer_value("", 0), integer_value("", 5), options); },
           "NotImplementedError", "apply", "Business hours calculation is not yet implemented for Polars filtering", "truthy business hours before scan");
  assert(scans == 2);
  error_is([&] { (void)apply_polars_case_filter(counted); }, "NotImplementedError", "apply", "apply method not available for case filter", "apply before scan");
  error_is([&] { (void)auto_filter_polars_cases(counted); }, "Exception", "apply", "apply_auto_filter method not available for case filter", "auto before scan");
  assert(scans == 2);
}

}  // namespace

int main() {
  try {
    retained_polars_case_filter_cases();
    native_lazy_complete_context_and_rank();
    std::cout << "Polars case filter: 225 retained calls; 134 exact projected frames, 79 complete diagnostics, 12 admitted count-tie projections; native lazy/context passed\n";
  } catch (const std::exception& error) { std::cerr << '\n' << error.what() << '\n'; return 1; }
}
