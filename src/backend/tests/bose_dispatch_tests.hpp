#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/concept_drift.hpp"
#include "bose_log_apply_tests.hpp"

namespace minera_bose_dispatch_tests {

inline minera::EventAttribute date_cell(const std::string& key,
                                        const std::string& value) {
  minera::EventAttribute result{key, value, minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{
      value, minera::NativeDateResolution::kMicrosecond, std::string("UTC")};
  return result;
}

inline minera::EventTable input_table() {
  const std::array<std::string, 4> labels{{"A", "B", "C", "D"}};
  const std::array<std::string, 4> timestamps{{
      "1970-01-01T00:00:01Z", "1970-01-01T00:00:11Z",
      "1970-01-01T00:00:21Z", "1970-01-01T00:00:31Z"}};
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.dtypes = {"str", "str", "datetime64[us, UTC]"};
  for (std::size_t index = 0; index < labels.size(); ++index) {
    table.rows.push_back({
        minera::EventAttribute{"case:concept:name", "case-" + std::to_string(index)},
        minera::EventAttribute{"concept:name", labels[index]},
        date_cell("time:timestamp", timestamps[index])});
  }
  return table;
}

inline void assert_same_result(const minera::ConceptDriftTableResult& left,
                               const minera::ConceptDriftTableResult& right) {
  assert(left.change_timestamps == right.change_timestamps);
  assert(left.p_values == right.p_values);
  assert(left.returned_tables.size() == right.returned_tables.size());
  for (std::size_t index = 0; index < left.returned_tables.size(); ++index) {
    const auto& first = left.returned_tables[index];
    const auto& second = right.returned_tables[index];
    assert(first.columns == second.columns);
    assert(first.rows == second.rows);
    assert(first.attributes == second.attributes);
    assert(first.metadata == second.metadata);
    assert(first.dtypes == second.dtypes);
    assert(first.index == second.index);
    assert(first.categorical_dtypes == second.categorical_dtypes);
  }
}

inline bool rejects_invalid_variant(const auto& call) {
  try {
    call();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace minera_bose_dispatch_tests

inline void test_bose_dispatch() {
  using namespace minera_bose_dispatch_tests;

  auto log = minera_bose_log_apply_tests::input_log();
  minera::ConceptDriftTableOptions log_options;
  log_options.sub_log_size = 2;
  log_options.window_size = 1;
  log_options.num_permutations = 1;
  log_options.threshold = 1.1;
  log_options.max_no_change_points = 1;
  const auto log_direct = minera::apply_concept_drift_log(log, log_options);
  assert_same_result(minera::apply_concept_drift(log, log_options), log_direct);
  assert_same_result(minera::apply_concept_drift(
                         log, log_options, minera::ConceptDriftVariant::BOSE),
                     log_direct);

  std::mt19937 log_dispatch_rng(0), log_direct_rng(0);
  const auto log_dispatched_state =
      minera::apply_concept_drift(log, log_options, log_dispatch_rng);
  const auto log_direct_state =
      minera::apply_concept_drift_log(log, log_options, log_direct_rng);
  assert_same_result(log_dispatched_state, log_direct_state);
  assert(log_dispatch_rng() == log_direct_rng());

  auto seeded_log_options = log_options;
  seeded_log_options.seed = 42;
  assert_same_result(
      minera::apply_concept_drift(log, seeded_log_options,
                                  minera::ConceptDriftVariant::kBose),
      minera::apply_concept_drift_log(log, seeded_log_options));

  auto signed_log_options = log_options;
  signed_log_options.sub_log_size = -1;
  signed_log_options.window_size = -1;
  signed_log_options.num_permutations = -1;
  signed_log_options.max_no_change_points = -1;
  assert_same_result(minera::apply_concept_drift(log, signed_log_options),
                     minera::apply_concept_drift_log(log, signed_log_options));

  auto table = input_table();
  minera::ConceptDriftTableOptions table_options;
  table_options.sub_log_size = 1;
  table_options.window_size = 1;
  table_options.num_permutations = 1;
  table_options.threshold = 1.1;
  table_options.max_no_change_points = 1;
  const auto table_direct =
      minera::apply_concept_drift_table(table, table_options);
  assert_same_result(minera::apply_concept_drift(table, table_options),
                     table_direct);
  assert_same_result(minera::apply_concept_drift(
                         table, table_options, minera::ConceptDriftVariant::BOSE),
                     table_direct);

  std::mt19937 table_dispatch_rng(0), table_direct_rng(0);
  const auto table_dispatched_state =
      minera::apply_concept_drift(table, table_options, table_dispatch_rng);
  const auto table_direct_state =
      minera::apply_concept_drift_table(table, table_options, table_direct_rng);
  assert_same_result(table_dispatched_state, table_direct_state);
  assert(table_dispatch_rng() == table_direct_rng());

  auto seeded_table_options = table_options;
  seeded_table_options.seed = 42;
  assert_same_result(
      minera::apply_concept_drift(table, seeded_table_options,
                                  minera::ConceptDriftVariant::kBose),
      minera::apply_concept_drift_table(table, seeded_table_options));

  auto signed_table_options = table_options;
  signed_table_options.sub_log_size = -1;
  signed_table_options.window_size = -1;
  signed_table_options.num_permutations = -1;
  signed_table_options.max_no_change_points = -1;
  assert_same_result(
      minera::apply_concept_drift(table, signed_table_options),
      minera::apply_concept_drift_table(table, signed_table_options));

  const auto unsupported =
      static_cast<minera::ConceptDriftVariant>(std::uint8_t{99});
  std::mt19937 invalid_log_rng(0), invalid_table_rng(0);
  assert(rejects_invalid_variant([&] {
    (void)minera::apply_concept_drift(log, log_options, invalid_log_rng,
                                      unsupported);
  }));
  assert(invalid_log_rng() == 2357136044U);
  assert(rejects_invalid_variant([&] {
    (void)minera::apply_concept_drift(table, table_options,
                                      invalid_table_rng, unsupported);
  }));
  assert(invalid_table_rng() == 2357136044U);
  assert(rejects_invalid_variant([&] {
    (void)minera::apply_concept_drift(log, log_options, unsupported);
  }));
  assert(rejects_invalid_variant([&] {
    (void)minera::apply_concept_drift(table, table_options, unsupported);
  }));
}
