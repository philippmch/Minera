#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/polars_timestamp_filter.hpp"

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
    throw std::runtime_error("Full Polars timestamp frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars timestamp floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsTimestampFilterError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars timestamp diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars timestamp type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars timestamp diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

#include "polars_timestamp_filter_public_vectors.inc"

void native_context_strict_boundaries_and_lazy_isolation() {
  EventTable source;
  source.columns={"case:concept:name","time:timestamp","concept:name","payload"};
  source.dtypes={"String","Datetime(time_unit='us', time_zone='UTC')","String","Float64"};
  source.attributes={{"context","independent"}};source.metadata.properties={{"property","retained"}};
  source.rows={{EventAttribute{source.columns[0],"exact"},date_value(source.columns[1],"2039-06-01T08:00:05+00:00"),EventAttribute{source.columns[2],"B"},float_value("payload",-0.0)},
               {EventAttribute{source.columns[0],"cover"},date_value(source.columns[1],"2039-06-01T07:59:59+00:00"),EventAttribute{source.columns[2],"A"},float_value("payload",0.0)},
               {EventAttribute{source.columns[0],"exact"},date_value(source.columns[1],"2039-06-01T08:00:00+00:00"),EventAttribute{source.columns[2],"A"},float_value("payload",0.0)},
               {EventAttribute{source.columns[0],"cover"},date_value(source.columns[1],"2039-06-01T08:00:06+00:00"),EventAttribute{source.columns[2],"B"},float_value("payload",-0.0)}};
  source.rows[0][0]->first_class_role=FirstClassAttributeRole::kCaseId;source.rows[0][0]->first_class_state=FirstClassAttributeState::kPresent;source.rows[0][0]->first_class_source_key="literal provenance";
  const auto before=source;const EventAttribute low{"","2039-06-01 08:00:00"},high{"","2039-06-01 08:00:05"};
  auto contained=source;contained.rows={source.rows[0],source.rows[2]};auto covering=source;covering.rows={source.rows[1],source.rows[3]};
  same_frame(filter_polars_timestamp_contained(source,low,high),contained,"inclusive containment and source order");
  same_frame(filter_polars_timestamp_intersecting(source,low,high),covering,"strict intersection excludes exact equal span");
  same_frame(filter_polars_timestamp_attribute(source,{"","concept:name"},{"","A"},low,high),contained,"whole cases from matching events");
  std::size_t scans=0;const auto filtered=filter_polars_timestamp_events(counted_source(source,scans),low,high);assert(scans==0);
  same_frame(filtered.materialize(),contained,"lazy complete context");assert(scans==1);auto independent=filtered.materialize();assert(scans==2);independent.rows[0][0]->value="changed";
  PolarsTimestampFilterOptions options;options.case_id_key=null_value("");same_frame(filter_polars_timestamp_events(source,low,high,options),contained,"event case role is inert");
  same_frame(source,before,"native context/provenance/source preservation");
  // Error interpolation treats literal user text as bytes, including template-looking text.
  options.timestamp_key={"","%FRAME%"};
  error_is([&]{(void)filter_polars_timestamp_events(source,low,high,options);},"ColumnNotFoundError","apply",
      "unable to find column \"%FRAME%\"; valid columns: [\"case:concept:name\", \"time:timestamp\", \"concept:name\", \"payload\"]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING THIS_NODE <---\nDF [\"case:concept:name\", \"time:timestamp\", \"concept:name\", \"payload\"]; PROJECT */4 COLUMNS","literal diagnostic text");
}
}  // namespace

int main() {
  try {
    retained_polars_timestamp_filter_cases();
    native_context_strict_boundaries_and_lazy_isolation();
    std::cout << "Polars timestamp: 732 retained calls; 229 exact complete projected frames and 503 full diagnostic phases/messages; native context/lazy/boundary controls passed\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
