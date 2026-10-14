#undef NDEBUG
#include "minera/dataframe_format_json.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool rejected(const std::string& input) {
  try { (void)minera::format_dataframe_json(input); }
  catch (const std::invalid_argument&) { return true; }
  return false;
}

void date_metadata_round_trips_and_rejects_malformed_values() {
  const std::string valid = R"({"table":{"columns":["date"],"rows":[[{"kind":"date","value":"2024-01-01T00:00:00+01:00","typed_value":"2024-01-01T00:00:00+01:00","date_resolution":"ns","date_timezone":"Europe/Berlin"}],[{"kind":"date","value":"2024-01-01T00:00:00+00:00","typed_value":"2024-01-01T00:00:00+00:00","date_timezone":"UTC"}]]}})";
  const auto result = minera::format_dataframe_json(valid);
  assert(!result.success);
  assert(result.body.find("\"date_resolution\":\"ns\"") != std::string::npos);
  assert(result.body.find("\"date_timezone\":\"Europe/Berlin\"") != std::string::npos);
  assert(result.body.find("\"date_timezone\":\"UTC\"") != std::string::npos);

  for (const auto& invalid : std::vector<std::string>{
      R"({"table":{"columns":["x"],"rows":[[{"kind":"string","value":"x","date_resolution":"ns"}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"date","value":"2024-01-01","date_timezone":"UTC"}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"date","value":"2024-01-01","typed_value":"2024-01-01","date_resolution":"ms"}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"date","value":"2024-01-01","typed_value":"2024-01-01","date_timezone":""}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"date","value":"2024-01-01","null":true,"date_timezone":"UTC"}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"date","value":"2024-01-01","typed_value":"2024-01-01","date_timezone":1}]]}})"})
    assert(rejected(invalid));
}

void empty_and_space_selectors_use_json_input_columns() {
  const std::string request =
      R"({"table":{"columns":[""," ","when","case:concept:name","concept:name","time:timestamp"],"rows":[[{"value":"selected-case"},{"value":"A"},{"value":"2024-01-01"},{"value":"decoy-case"},{"value":"decoy-activity"},{"value":"2019-01-01"}]]},"options":{"case_id":"","activity_key":" ","timestamp_key":"when","start_timestamp_key":null}})";
  const auto result = minera::format_dataframe_json(request);
  assert(result.success);
  assert(result.body.find("selected-case") != std::string::npos);
  assert(result.body.find("decoy-case") == std::string::npos);
  assert(result.body.find("decoy-activity") == std::string::npos);
  assert(result.body.find("2024-01-01") != std::string::npos);
  assert(result.body.find("2019-01-01") == std::string::npos);
  assert(result.body.find("\"columns\":[\"\",\" ") !=
         std::string::npos);
}
}

int main() {
  date_metadata_round_trips_and_rejects_malformed_values();
  empty_and_space_selectors_use_json_input_columns();
  // The new lossless discovery table codec opts into integer storage tags.
  // The established formatter transport still rejects that additional field.
  const std::string tagged = R"({"columns":["x"],"rows":[[{"kind":"int","typed_value":1,"integer_storage":"int64"}]]})";
  const auto decoded_tagged = minera::parse_event_table_json(tagged);
  assert(std::get<std::int64_t>(decoded_tagged.rows[0][0]->typed_value) == 1);
  assert(rejected("{\"table\":" + tagged + '}'));
  const std::string valid = R"({"table":{"columns":["case:concept:name","concept:name","time:timestamp"],"dtypes":["uint64","str","str"],"rows":[[{"kind":"int","value":"18446744073709551615","typed_value":18446744073709551615},{"value":"A\uD83D\uDE00"},{"value":"2024-01-01"}]]}})";
  const auto result = minera::format_dataframe_json(valid);
  assert(result.success);
  assert(result.body.find("18446744073709551615") != std::string::npos);
  assert(result.body.find("2024-01-01T00:00:00+00:00") != std::string::npos);
  assert(result.body.find("A\xf0\x9f\x98\x80") != std::string::npos);
  assert(result.body.find("\"input_after\"") != std::string::npos);
  const auto missing = minera::format_dataframe_json(R"({"table":{"columns":[],"rows":[]}})");
  assert(!missing.success);
  assert(missing.body.find("case ID") != std::string::npos);
  assert(missing.body.find("\"input_after\"") != std::string::npos);
  for (const auto& invalid : std::vector<std::string>{
      "", "null", "[]", "{}", "{\"table\":null}", "{\"table\":{},\"table\":{}}",
      "{\"table\":{}} trailing", "{\"table\":{},}", "{\"table\":{\"rows\":[1,]}}",
      "{\"table\":{\"columns\":[\"\\uD800\"],\"rows\":[]}}",
      "{\"table\":{\"columns\":[\"\\uDC00\"],\"rows\":[]}}",
      "{\"table\":{\"columns\":[\"\xc0\x80\"],\"rows\":[]}}",
      "{\"table\":{\"columns\":[\"\xed\xa0\x80\"],\"rows\":[]}}",
      R"({"table":{"columns":["x"],"rows":[[]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"key":"y"}]]}})",
      R"({"table":{"columns":["x"],"rows":[],"dtypes":["str","int64"]}})",
      R"({"table":{"columns":[],"rows":[],"typo":true}})",
      R"({"table":{"columns":[],"rows":[]},"options":{"typo":true}})",
      R"({"table":{"columns":[],"rows":[]},"options":{"case_id":null}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"int","typed_value":18446744073709551616}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"int","typed_value":-9223372036854775809}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"int","typed_value":01}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"float","typed_value":1.}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"float","typed_value":1e309}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"float","typed_value":NaN}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"boolean","typed_value":1}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"int","value":"2","typed_value":1}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"float","value":"2","typed_value":1}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"boolean","value":"true","typed_value":false}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"date","value":"2020-01-01","typed_value":"2021-01-01"}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"first_class_state":"null"}]]}})",
      R"({"table":{"columns":["x"],"rows":[[{"kind":"int","null":true,"typed_value":1}]]}})"})
    assert(rejected(invalid));
  assert(rejected(std::string(66, '[') + "0" + std::string(66, ']')));
  assert(rejected(std::string(32u * 1024u * 1024u + 1, ' ')));
}
