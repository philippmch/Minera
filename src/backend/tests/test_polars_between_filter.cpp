#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/polars_between_filter.hpp"

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
    throw std::runtime_error("Full Polars between frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars between floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsBetweenError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars between diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars between type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars between diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

#include "polars_between_public_vectors.inc"
#include "polars_between_categorical_public_vectors.inc"

void native_lazy_sort_metadata_and_independent_result() {
  EventTable source;
  source.columns = {"case:concept:name", "concept:name", "time:timestamp", "payload"};
  source.dtypes = {"String", "String", "Datetime(time_unit='us', time_zone=None)", "Float64"};
  const std::vector<std::string> activities{"A", "A", "X", "B", "A", "B"};
  for (std::size_t i = 0; i < activities.size(); ++i) {
    EventAttribute id{"case:concept:name", "native"};
    id.first_class_role = FirstClassAttributeRole::kCaseId;
    id.first_class_state = FirstClassAttributeState::kMissing;
    id.first_class_source_key = "literal case provenance";
    id.container = AttributeContainerKind::kTuple;
    id.children = {{"native source descriptor", "keep on source"}};
    id.scalar_wrapper = true; id.scalar_children_list = true;
    source.rows.push_back({id, EventAttribute{"concept:name", activities[i]},
        date_value("time:timestamp", "2033-07-08T09:10:11.00000" + std::to_string(i)),
        float_value("payload", i % 2 ? -0.0 : 0.0)});
  }
  source.attributes = {{"native context", "whole frame"}};
  source.metadata.properties = {{"property", "retained independently"}};
  auto expected = source; expected.rows.erase(expected.rows.begin());
  for (std::size_t i = 0; i < expected.rows.size(); ++i) {
    auto& id = *expected.rows[i][0];
    id.value = i < 3 ? "native##@@1" : "native##@@2";
    id.container = AttributeContainerKind::kScalar; id.children.clear();
    id.scalar_wrapper = false; id.scalar_children_list = false;
    id.first_class_state = FirstClassAttributeState::kPresent;
  }
  std::reverse(source.rows.begin(), source.rows.end());
  const auto before = source;
  std::size_t scans = 0;
  const auto lazy = counted_source(source, scans);
  const auto filtered = filter_polars_between(lazy, EventAttribute{"", "A"}, EventAttribute{"", "B"});
  assert(scans == 0 && filtered.attributes() == source.attributes && filtered.metadata() == source.metadata);
  auto first = filtered.materialize();
  same_frame(first, expected, "native sorted latest-start segments and context");
  first.rows.clear(); first.attributes.clear();
  same_frame(filtered.materialize(), expected, "native restartable independent results");
  same_frame(source, before, "native source preservation");
  assert(scans == 2);
  PolarsBetweenOptions options; options.subcase_concat_str = null_value("");
  auto expected_null = expected;
  for (auto& row : expected_null.rows) {
    row[0]->value.clear(); row[0]->is_null = true;
    row[0]->first_class_state = FirstClassAttributeState::kNull;
  }
  same_frame(filter_polars_between(source, EventAttribute{"", "A"}, EventAttribute{"", "B"}, options),
             expected_null, "native null case state and canonical scalar payload");
  same_frame(source, before, "native descriptor/provenance source preservation");
}

void native_diagnostic_tokens_remain_literal() {
  EventTable source;
  source.columns = {"case%FRAME%", "activity%CASE%", "time:timestamp"};
  source.dtypes = {"String", "String", "Int64"};
  source.rows = {{EventAttribute{source.columns[0], "x"}, EventAttribute{source.columns[1], "%FRAME%"}, integer_value("time:timestamp", 1)},
                 {EventAttribute{source.columns[0], "x"}, EventAttribute{source.columns[1], "B"}, integer_value("time:timestamp", 2)}};
  const auto before = source;
  PolarsBetweenOptions options;
  options.case_id_key.value = source.columns[0]; options.activity_key.value = source.columns[1];
  options.subcase_concat_str = collection_value("", AttributeContainerKind::kList, {EventAttribute{"", "%ANCHOR1%"}});
  bool caught = false;
  std::size_t scans = 0;
  const auto filtered = filter_polars_between(counted_source(source, scans), EventAttribute{"", "%FRAME%"}, EventAttribute{"", "B"}, options);
  assert(scans == 0);
  try { (void)filtered.materialize(); }
  catch (const PolarsBetweenError& error) {
    caught = true;
    assert(error.type == "InvalidOperationError" && error.phase == "collect");
    assert(error.message.find("col(\"case%FRAME%\")") != std::string::npos);
    assert(error.message.find("col(\"activity%CASE%\").is_in([[\"%FRAME%\"]])") != std::string::npos);
    assert(error.message.find("str.concat_horizontal([[\"%ANCHOR1%\"],") != std::string::npos);
    assert(error.message.find("DF [\"case%FRAME%\", \"activity%CASE%\", \"time:timestamp\"]") != std::string::npos);
  }
  assert(caught && scans == 1);
  same_frame(source, before, "native diagnostic interpolation source preservation");
}

void native_enum_case_requires_dictionary() {
  EventTable source;
  source.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  source.dtypes = {"Enum(categories=['z', 'a'])", "String", "Int64"};
  source.rows = {{EventAttribute{source.columns[0], "a"}, EventAttribute{source.columns[1], "A"}, integer_value(source.columns[2], 1)},
                 {EventAttribute{source.columns[0], "a"}, EventAttribute{source.columns[1], "B"}, integer_value(source.columns[2], 2)},
                 {EventAttribute{source.columns[0], "z"}, EventAttribute{source.columns[1], "A"}, integer_value(source.columns[2], 3)},
                 {EventAttribute{source.columns[0], "z"}, EventAttribute{source.columns[1], "B"}, integer_value(source.columns[2], 4)}};
  const auto before = source;
  const auto rejected = [](auto action, const std::string& message) {
    bool caught = false;
    try { action(); }
    catch (const std::invalid_argument& error) {
      caught = true;
      assert(std::string(error.what()).find(message) != std::string::npos);
    }
    assert(caught);
  };
  const std::string missing_dictionary = "requires an Enum category dictionary";
  rejected([&] { (void)filter_polars_between(source, EventAttribute{"", "A"}, EventAttribute{"", "B"}); }, missing_dictionary);
  auto empty_missing_dictionary = source; empty_missing_dictionary.rows.clear();
  rejected([&] { (void)filter_polars_between(empty_missing_dictionary, EventAttribute{"", "A"}, EventAttribute{"", "B"}); }, missing_dictionary);
  std::size_t scans = 0;
  const auto lazy = filter_polars_between(counted_source(source, scans), EventAttribute{"", "A"}, EventAttribute{"", "B"});
  assert(scans == 0);
  rejected([&] { (void)lazy.materialize(); }, missing_dictionary);
  assert(scans == 1);
  same_frame(source, before, "missing Enum dictionary source preservation");

  EventTableCategoricalDtype dictionary;
  dictionary.categories_dtype = "String"; dictionary.ordered = true;
  dictionary.categories = {{"", "z"}, {"", "a"}};
  source.categorical_dtypes.resize(source.columns.size()); source.categorical_dtypes[0] = dictionary;
  auto expected = source;
  expected.rows = {source.rows[2], source.rows[3], source.rows[0], source.rows[1]};
  expected.dtypes[0] = "String"; expected.categorical_dtypes[0].reset();
  for (auto& row : expected.rows) row[0]->value += "##@@1";
  same_frame(filter_polars_between(source, EventAttribute{"", "A"}, EventAttribute{"", "B"}), expected,
             "provided Enum dictionary determines case ordering and clears rewritten descriptor");

  auto unknown_case = source; unknown_case.rows.resize(1);
  unknown_case.rows[0][0]->value = "outside-dictionary";
  const std::string unknown = "Enum cell is absent from its category dictionary";
  rejected([&] { (void)filter_polars_between(unknown_case, EventAttribute{"", "A"}, EventAttribute{"", "B"}); }, unknown);
  scans = 0;
  const auto bad_case = filter_polars_between(counted_source(unknown_case, scans), EventAttribute{"", "A"}, EventAttribute{"", "B"});
  assert(scans == 0);
  rejected([&] { (void)bad_case.materialize(); }, unknown); assert(scans == 1);

  auto unknown_time = unknown_case;
  unknown_time.dtypes = {"String", "String", "Enum(categories=['early', 'late'])"};
  unknown_time.categorical_dtypes[0].reset();
  dictionary.categories = {{"", "early"}, {"", "late"}};
  unknown_time.categorical_dtypes[2] = dictionary;
  unknown_time.rows[0][0]->value = "case";
  unknown_time.rows[0][2] = EventAttribute{"time:timestamp", "outside-time-dictionary"};
  rejected([&] { (void)filter_polars_between(unknown_time, EventAttribute{"", "A"}, EventAttribute{"", "B"}); }, unknown);
  scans = 0;
  const auto bad_time = filter_polars_between(counted_source(unknown_time, scans), EventAttribute{"", "A"}, EventAttribute{"", "B"});
  assert(scans == 0);
  rejected([&] { (void)bad_time.materialize(); }, unknown); assert(scans == 1);
}

}  // namespace

int main() {
  try {
    retained_polars_between_cases();
    retained_polars_between_categorical_cases();
    native_lazy_sort_metadata_and_independent_result();
    native_diagnostic_tokens_remain_literal();
    native_enum_case_requires_dictionary();
    std::cout << "Polars between: 343 retained outcomes, 163 full projected returned frames and 180 complete diagnostics; 10 complete category-label dictionaries, native lazy sorting/context passed\n";
  } catch (const std::exception& error) {
    std::cerr << '\n' << error.what() << '\n'; return 1;
  }
}
