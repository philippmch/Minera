#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/performance_spectrum.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/log_conversion.hpp"

namespace {

using minera::Event;
using minera::EventAttribute;
using minera::EventLog;
using minera::EventTable;
using minera::LazyEventTable;

EventAttribute cell(const char* key, const std::string& value) {
  return EventAttribute{key, value};
}

EventLog repeated_log(const std::vector<std::string>& activities) {
  EventLog log;
  for (std::size_t index = 0; index < 11; ++index) {
    minera::Trace trace;
    trace.case_id = std::to_string(index);
    for (std::size_t event_index = 0; event_index < activities.size();
         ++event_index) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activities[event_index];
      event.timestamp = std::string("1970-01-") +
                        (index < 9 ? "0" : "") + std::to_string(index + 1) +
                        "T00:00:00Z";
      event.input_order = event_index;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

EventTable repeated_table(const std::vector<std::string>& activities) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  for (std::size_t index = 0; index < 11; ++index) {
    for (std::size_t event_index = 0; event_index < activities.size();
         ++event_index) {
      table.rows.push_back(
          {cell("case:concept:name", std::to_string(index)),
           cell("concept:name", activities[event_index]),
           cell("time:timestamp",
                std::string("1970-01-") + (index < 9 ? "0" : "") +
                    std::to_string(index + 1) + "T00:00:00Z")});
    }
  }
  return table;
}

EventLog first_class_log() {
  EventLog log;
  minera::Trace trace;
  trace.case_id = "outer";
  trace.case_id_state = minera::FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "case_col";
  for (const auto [index, activity, timestamp] :
       {std::tuple<std::size_t, const char*, const char*>{
            0, "start", "1970-01-01T00:00:00Z"},
        {1, "finish", "1970-01-01T00:01:00Z"}}) {
    Event event;
    event.case_id = "selected";
    event.case_id_state = minera::FirstClassAttributeState::kPresent;
    event.case_id_source_key = "case_col";
    event.activity = activity;
    event.activity_state = minera::FirstClassAttributeState::kPresent;
    event.activity_source_key = "phase_col";
    event.activity_attribute = EventAttribute{
        "phase_col", activity, minera::AttributeKind::kString, false, {},
        minera::AttributeContainerKind::kScalar, {},
        minera::FirstClassAttributeRole::kActivity,
        minera::FirstClassAttributeState::kPresent, "phase_col"};
    event.timestamp = timestamp;
    event.timestamp_state = minera::FirstClassAttributeState::kPresent;
    event.timestamp_source_key = "occurred_col";
    event.timestamp_attribute = EventAttribute{
        "occurred_col", timestamp, minera::AttributeKind::kDate, false, {},
        minera::AttributeContainerKind::kScalar, {},
        minera::FirstClassAttributeRole::kTimestamp,
        minera::FirstClassAttributeState::kPresent, "occurred_col"};
    event.input_order = index;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

std::vector<std::string> observation_case_ids(
    const minera::PerformanceSpectrum& spectrum) {
  std::vector<std::string> result;
  for (const auto& observation : spectrum.observations) {
    result.push_back(observation.case_id);
  }
  return result;
}

std::vector<double> first_instants(const minera::PerformanceSpectrum& spectrum) {
  std::vector<double> result;
  for (const auto& observation : spectrum.observations) {
    assert(!observation.instants.empty());
    result.push_back(observation.instants.front());
  }
  return result;
}

void assert_close(double actual, double expected) {
  assert(std::abs(actual - expected) < 1e-9);
}

void test_canonical_connected_sampling() {
  const auto log = repeated_log({"a", "b"});
  const auto table = repeated_table({"a", "b"});
  const auto lazy = LazyEventTable::from_table(table);

  minera::PerformanceSpectrumOptions options;
  options.sample_size = 4;

  const auto log_result =
      minera::discover_performance_spectrum_log(log, {"a", "b"}, options);
  assert(log_result.variant == minera::PerformanceSpectrumVariant::kLog);
  assert((observation_case_ids(log_result) ==
          std::vector<std::string>{"0", "1", "3", "6", "9", "10"}));
  assert_close(log_result.observations.front().instants.front(), 0.0);

  const auto dataframe_result = minera::discover_performance_spectrum_dataframe(
      table, {"a", "b"}, options);
  assert(dataframe_result.variant ==
         minera::PerformanceSpectrumVariant::kDataframe);
  assert((first_instants(dataframe_result) ==
          std::vector<double>{0.0, 86.4, 518.4, 864.0}));

  const auto lazy_result = minera::discover_performance_spectrum_lazyframe(
      lazy, {"a", "b"}, options);
  assert(lazy_result.variant == minera::PerformanceSpectrumVariant::kLazyframe);
  assert((observation_case_ids(lazy_result) ==
          std::vector<std::string>{"0", "1", "2", "3"}));
  assert_close(lazy_result.observations.front().instants.front(), 0.0);
}

void test_disconnected_components_and_zero_sampling() {
  const auto log = repeated_log({"a", "b"});
  const auto table = repeated_table({"a", "b"});
  const auto lazy = LazyEventTable::from_table(table);
  minera::PerformanceSpectrumOptions options;
  options.sample_size = 4;

  const auto log_result = minera::discover_performance_spectrum_log_disconnected(
      log, {"a", "b", "c"}, options);
  assert(log_result.observations.size() == 6);
  for (const auto& observation : log_result.observations) {
    assert(observation.activities == std::vector<std::string>({"a", "b"}));
  }

  const auto dataframe_result =
      minera::discover_performance_spectrum_dataframe_disconnected(
          table, {"a", "b", "c"}, options);
  assert(dataframe_result.observations.size() == 6);
  assert(dataframe_result.observations.front().activities ==
         std::vector<std::string>({"a", "b"}));

  const auto lazy_result =
      minera::discover_performance_spectrum_lazyframe_disconnected(
          lazy, {"a", "b", "c"}, options);
  assert(lazy_result.observations.size() == 6);
  assert(lazy_result.observations.front().activities ==
         std::vector<std::string>({"a", "b"}));

  options.sample_size = 0;
  assert(minera::discover_performance_spectrum_log(log, {"a", "b"}, options)
             .observations.size() == 2);
  assert(minera::discover_performance_spectrum_dataframe(
             table, {"a", "b"}, options)
             .observations.empty());
  assert(minera::discover_performance_spectrum_log_disconnected(
             log, {"a", "b", "c"}, options)
             .observations.size() == 2);
  assert(minera::discover_performance_spectrum_dataframe_disconnected(
             table, {"a", "b", "c"}, options)
             .observations.size() == 2);
  assert(minera::discover_performance_spectrum_lazyframe(lazy, {"a", "b"},
                                                         options)
             .observations.empty());
  assert(minera::discover_performance_spectrum_lazyframe_disconnected(
             lazy, {"a", "b", "c"}, options)
             .observations.size() == 2);
}

void test_longest_disconnected_subpatterns() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  const std::vector<std::string> activities{"a", "b", "a", "b", "c"};
  for (std::size_t index = 0; index < activities.size(); ++index) {
    table.rows.push_back(
        {cell("case:concept:name", "case"), cell("concept:name", activities[index]),
         cell("time:timestamp", "1970-01-01T00:00:0" + std::to_string(index) +
                                  "Z")});
  }
  const auto result = minera::discover_performance_spectrum_dataframe_disconnected(
      table, {"a", "b", "c"});
  assert(result.observations.size() == 3);
  assert(result.observations[0].activities ==
         std::vector<std::string>({"a", "b"}));
  assert(result.observations[1].activities ==
         std::vector<std::string>({"b", "c"}));
  assert(result.observations[2].activities ==
         std::vector<std::string>({"a", "b", "c"}));
  assert(result.observations[2].pattern_offset == 0);
}

void test_custom_selectors_and_views() {
  EventTable table;
  table.columns = {"case_ref", "phase", "occurred_at"};
  table.rows = {{cell("case_ref", "selected"), cell("phase", "start"),
                cell("occurred_at", "1970-01-01T00:00:00Z")},
               {cell("case_ref", "selected"), cell("phase", "finish"),
                cell("occurred_at", "1970-01-01T00:01:00Z")}};
  minera::PerformanceSpectrumOptions options;
  options.activity_attribute = "phase";
  options.timestamp_attribute = "occurred_at";
  options.case_id_attribute = "case_ref";
  const auto direct = minera::discover_performance_spectrum_dataframe(
      table, {"start", "finish"}, options);
  assert(direct.observations.size() == 1);
  assert(direct.observations.front().case_id == "selected");
  assert(direct.activity_attribute == "phase");
  assert_close(direct.observations.front().instants[1], 0.06);

  const minera::EventDataView view(table);
  const auto via_view = minera::compute_performance_spectrum(
      view, {"start", "finish"}, options);
  assert(via_view.observations.size() == 1);
  assert(via_view.observations.front().case_id == "selected");
}

void test_first_class_log_selectors() {
  auto options = minera::PerformanceSpectrumOptions{};
  options.activity_attribute = "phase_col";
  options.timestamp_attribute = "occurred_col";
  options.case_id_attribute = "case_col";
  const auto result = minera::discover_performance_spectrum_log(
      first_class_log(), {"start", "finish"}, options);
  assert(result.observations.size() == 1);
  assert(result.observations.front().case_id == "selected");
  assert(result.observations.front().activities ==
         std::vector<std::string>({"start", "finish"}));
  assert_close(result.observations.front().instants[1], 60.0);
}

void test_lazy_converted_first_class_schema() {
  std::istringstream input(
      "case_ref,phase,occurred_at\n"
      "selected,start,1970-01-01T00:00:00Z\n"
      "selected,finish,1970-01-01T00:01:00Z\n");
  minera::CsvImportOptions import_options;
  import_options.case_id_column = "case_ref";
  import_options.activity_column = "phase";
  import_options.timestamp_column = "occurred_at";
  import_options.require_timestamp_column = true;
  const auto log = minera::import_csv_event_log(input, import_options);
  const auto lazy = minera::event_log_to_lazy_table(log);

  const auto result = minera::discover_performance_spectrum_lazyframe(
      lazy, {"start", "finish"});
  assert(result.observations.size() == 1);
  assert(result.observations.front().case_id == "selected");
  assert_close(result.observations.front().instants[1], 60.0);
}

void test_columnar_string_projection() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {
      {cell("case:concept:name", "c"), cell("concept:name", "aa"),
       cell("time:timestamp", "1970-01-01T00:00:00Z")},
      {cell("case:concept:name", "c"), cell("concept:name", "a"),
       cell("time:timestamp", "1970-01-01T00:00:01Z")},
  };
  const auto result = minera::discover_performance_spectrum_dataframe(
      table, {"a", "aa"});
  assert(result.observations.size() == 1);
  assert(result.observations.front().activities ==
         std::vector<std::string>({"a", "aa"}));
  assert_close(result.observations.front().instants[0], 0.0);
}

void performance_spectrum_log_variant_contract() {
  const auto result = minera::discover_performance_spectrum_log(
      repeated_log({"a", "b"}), {"a", "b"});
  assert(result.variant == minera::PerformanceSpectrumVariant::kLog);
  assert(result.observations.size() == 11);
}

void performance_spectrum_dataframe_variant_contract() {
  const auto result = minera::discover_performance_spectrum_dataframe(
      repeated_table({"a", "b"}), {"a", "b"});
  assert(result.variant == minera::PerformanceSpectrumVariant::kDataframe);
  assert(result.observations.size() == 11);
}

void performance_spectrum_lazyframe_variant_contract() {
  const auto table = repeated_table({"a", "b"});
  const auto result = minera::discover_performance_spectrum_lazyframe(
      LazyEventTable::from_table(table), {"a", "b"});
  assert(result.variant == minera::PerformanceSpectrumVariant::kLazyframe);
  assert(result.observations.size() == 11);
}

void performance_spectrum_log_disconnected_variant_contract() {
  const auto result = minera::discover_performance_spectrum_log_disconnected(
      repeated_log({"a", "b"}), {"a", "b", "c"});
  assert(result.variant ==
         minera::PerformanceSpectrumVariant::kLogDisconnected);
  assert(result.observations.size() == 11);
}

void performance_spectrum_dataframe_disconnected_variant_contract() {
  const auto result =
      minera::discover_performance_spectrum_dataframe_disconnected(
          repeated_table({"a", "b"}), {"a", "b", "c"});
  assert(result.variant ==
         minera::PerformanceSpectrumVariant::kDataframeDisconnected);
  assert(result.observations.size() == 11);
}

void performance_spectrum_lazyframe_disconnected_variant_contract() {
  const auto table = repeated_table({"a", "b"});
  const auto result =
      minera::discover_performance_spectrum_lazyframe_disconnected(
          LazyEventTable::from_table(table), {"a", "b", "c"});
  assert(result.variant ==
         minera::PerformanceSpectrumVariant::kLazyframeDisconnected);
  assert(result.observations.size() == 11);
}

}  // namespace

int main() {
  test_canonical_connected_sampling();
  test_disconnected_components_and_zero_sampling();
  test_longest_disconnected_subpatterns();
  test_custom_selectors_and_views();
  test_first_class_log_selectors();
  test_lazy_converted_first_class_schema();
  test_columnar_string_projection();
  performance_spectrum_log_variant_contract();
  performance_spectrum_dataframe_variant_contract();
  performance_spectrum_lazyframe_variant_contract();
  performance_spectrum_log_disconnected_variant_contract();
  performance_spectrum_dataframe_disconnected_variant_contract();
  performance_spectrum_lazyframe_disconnected_variant_contract();
}
