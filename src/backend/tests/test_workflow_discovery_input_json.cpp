#undef NDEBUG

#include "minera/dataframe_format_json.hpp"
#include "minera/workflow_discovery_input_json.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace minera;

template <class Parse>
void rejects(Parse parse, const std::vector<std::string>& cases) {
  for (const auto& value : cases) {
    bool threw = false;
    try { (void)parse(value); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}

void table_codec_preserves_all_metadata_and_integer_storage() {
  const std::string json = R"({"columns":[" null ","missing","number","date","category"],"dtypes":["object","object","uint64","datetime64[ns, UTC]","category"],"rows":[[{"null":true},null,{"kind":"int","typed_value":18446744073709551615,"integer_storage":"uint64"},{"kind":"date","typed_value":"2024-01-01T00:00:00+00:00","date_resolution":"ns","date_timezone":"UTC"},{"kind":"int","typed_value":1,"integer_storage":"int64"}]],"categorical_dtypes":[null,null,null,null,{"categories_dtype":"int64","ordered":true,"categories":[{"kind":"int","typed_value":1,"integer_storage":"int64"}]}],"index":{"class":"MultiIndex","dtype":"object","names":[null," idx "],"labels":[{"kind":"tuple","items":[{"kind":"scalar","value":{"kind":"int","typed_value":1,"integer_storage":"int64"}},{"kind":"scalar","value":{"value":"a\u0000b"}}]}]},"attributes":[{"key":"wrapped","value":"root","scalar_wrapper":true,"scalar_children_list":true,"children":[{"key":"same","kind":"int","typed_value":1,"integer_storage":"int64"},{"key":"same","kind":"int","typed_value":2,"integer_storage":"uint64"}]}],"metadata":{"extensions":[{"name":" n ","prefix":"p","uri":"u"}],"classifiers":[{"name":" c ","keys":["x","x"]}],"trace_globals":[{"key":"t","kind":"int","typed_value":3,"integer_storage":"int64"}],"event_globals":[{"key":"e","kind":"int","typed_value":4,"integer_storage":"int64"}],"properties":[{"key":"p","kind":"int","typed_value":5,"integer_storage":"int64"}],"globals":[{"scope":"trace","attributes":[{"key":"g","kind":"int","typed_value":6,"integer_storage":"int64"}]}]}})";
  const auto input = parse_event_table_json(json);
  assert(input.columns.size() == 5 && input.rows.size() == 1);
  assert(input.rows[0][0]->is_null && !input.rows[0][1]);
  assert(std::get<std::uint64_t>(input.rows[0][2]->typed_value) ==
         std::numeric_limits<std::uint64_t>::max());
  assert(std::get<std::int64_t>(input.rows[0][4]->typed_value) == 1);
  const auto before = serialize_event_table_input_json(input);
  const auto output = parse_event_table_json(before);
  assert(output.columns == input.columns && output.rows == input.rows);
  assert(output.dtypes == input.dtypes && output.index == input.index &&
         output.categorical_dtypes == input.categorical_dtypes);
  assert(output.attributes == input.attributes);
  assert(std::get<std::int64_t>(output.attributes[0].children[0].typed_value) == 1);
  assert(std::get<std::uint64_t>(output.attributes[0].children[1].typed_value) == 2);
  assert(output.metadata.trace_globals == input.metadata.trace_globals &&
         output.metadata.event_globals == input.metadata.event_globals &&
         output.metadata.properties == input.metadata.properties);
  assert(std::get<std::int64_t>(output.metadata.globals[0].attributes[0].typed_value) == 6);
  assert(output.metadata.extensions[0].name == " n " &&
         output.metadata.classifiers[0].keys.size() == 2);
  const auto& date = std::get<NativeDate>(output.rows[0][3]->typed_value);
  assert(date.resolution == NativeDateResolution::kNanosecond && date.timezone == "UTC");
  assert(serialize_event_table_input_json(output) == before);
  assert(input.columns == output.columns);  // no formatter columns were appended
}

void strict_table_codec_rejects_invalid_schema_and_storage() {
  rejects(parse_event_table_json, {
      "{}", "null", "[]", R"({"columns":["x","x"],"rows":[]})",
      R"({"columns":["x"],"rows":[[]]})",
      R"({"columns":["x"],"rows":[[{"key":"y"}]]})",
      R"({"columns":[],"rows":[],"unknown":0})",
      R"({"columns":["x"],"rows":[[{"kind":"int","typed_value":9223372036854775808,"integer_storage":"int64"}]]})",
      R"({"columns":["x"],"rows":[[{"kind":"int","typed_value":-1,"integer_storage":"uint64"}]]})",
      R"({"columns":["x"],"rows":[[{"kind":"int","typed_value":1,"integer_storage":"other"}]]})",
      R"({"columns":["x"],"rows":[[{"kind":"string","integer_storage":"int64"}]]})",
      R"({"columns":["x"],"rows":[[{"kind":"int","integer_storage":"int64"}]]})",
      R"({"columns":["x"],"rows":[[{"kind":"int","null":true,"typed_value":null,"integer_storage":"int64"}]]})",
      R"({"columns":[],"rows":[],"index":{"class":"Index","dtype":"int64","names":[null,null],"labels":[]}})",
      R"({"columns":[],"rows":[],"columns":[]})"});
  EventTable invalid;
  invalid.columns = {"x", "x"};
  bool threw = false;
  try { (void)serialize_event_table_input_json(invalid); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void table_codec_preserves_nonfinite_and_distinct_missing_markers() {
  const std::string json = R"({"columns":["missing","null","empty","na","nan","infinity","zero"],"rows":[[null,{"null":true},{"value":""},{"kind":"na","value":"<NA>","typed_value":"NA"},{"kind":"float","value":"nan","typed_value":"NaN"},{"kind":"float","value":"inf","typed_value":"INF"},{"kind":"float","value":"-0","typed_value":-0.0}]]})";
  const auto input = parse_event_table_json(json);
  const auto output = parse_event_table_json(serialize_event_table_input_json(input));
  assert(!output.rows[0][0] && output.rows[0][1]->is_null);
  assert(output.rows[0][2]->value.empty() && !output.rows[0][2]->is_null);
  assert(std::holds_alternative<NativeNA>(output.rows[0][3]->typed_value));
  assert(std::isnan(std::get<double>(output.rows[0][4]->typed_value)));
  assert(std::isinf(std::get<double>(output.rows[0][5]->typed_value)));
  assert(std::signbit(std::get<double>(output.rows[0][6]->typed_value)));
}

void alpha_graph_forms_preserve_presence_labels_and_registry_order() {
  const auto inferred = parse_alpha_dfg_input_json(
      R"({"kind":"sparse","dfg":[{"source":"A","target":"B","count":0}]})");
  assert(!inferred.start_activities && !inferred.end_activities);
  const auto explicit_empty = parse_alpha_dfg_input_json(
      R"({"kind":"sparse","dfg":[],"start_activities":{},"end_activities":{}})");
  assert(explicit_empty.start_activities && explicit_empty.start_activities->empty());
  assert(explicit_empty.end_activities && explicit_empty.end_activities->empty());
  const auto clean = parse_alpha_dfg_input_json(
      R"({"kind":"clean","dfg":[],"start_activities":{"singleton":0},"end_activities":{"singleton":18446744073709551615}})");
  assert(clean.kind == AlphaDfgDiscoveryInput::Kind::kClean);
  assert(clean.start_activities->at("singleton") == 0);
  assert(clean.end_activities->at("singleton") == std::numeric_limits<std::uint64_t>::max());
  const std::string graph_json = R"({"kind":"graph","activities":[" z,>;:\t ","a\u0000b","\uD83D\uDE00"],"dfg":[{"source":"a\u0000b","target":" z,>;:\t ","count":18446744073709551615}],"start_activities":{"a\u0000b":0},"end_activities":{"\uD83D\uDE00":2}})";
  const auto graph = parse_alpha_dfg_input_json(graph_json);
  assert(graph.activities[0] == " z,>;:\t " &&
         graph.activities[1] == std::string("a\0b", 3));
  assert(graph.activities[2] == "\xF0\x9F\x98\x80");
  for (const auto* input : {&inferred, &explicit_empty, &clean, &graph}) {
    const auto decoded = parse_alpha_dfg_input_json(serialize_alpha_dfg_input_json(*input));
    assert(decoded.kind == input->kind && decoded.dfg == input->dfg &&
           decoded.start_activities == input->start_activities &&
           decoded.end_activities == input->end_activities && decoded.activities == input->activities);
  }
}

void alpha_graph_forms_reject_invalid_or_ambiguous_input() {
  rejects(parse_alpha_dfg_input_json, {
      "{}", "[]", R"({"kind":"other","dfg":[]})",
      R"({"kind":"sparse","dfg":[],"start_activities":{}})",
      R"({"kind":"clean","dfg":[]})",
      R"({"kind":"sparse","dfg":[],"activities":[]})",
      R"({"kind":"graph","dfg":[],"start_activities":{},"end_activities":{}})",
      R"({"kind":"graph","activities":["A","\u0041"],"dfg":[],"start_activities":{},"end_activities":{}})",
      R"({"kind":"graph","activities":["A"],"dfg":[{"source":"A","target":"B","count":1}],"start_activities":{},"end_activities":{}})",
      R"({"kind":"graph","activities":[],"dfg":[],"start_activities":{"A":0},"end_activities":{}})",
      R"({"kind":"sparse","dfg":[{"source":"A","target":"B","count":1},{"source":"\u0041","target":"B","count":2}]})",
      R"({"kind":"sparse","dfg":[{"source":"A","target":"B","count":18446744073709551616}]})",
      R"({"kind":"sparse","dfg":[{"source":"A","target":"B","count":-1}]})",
      R"({"kind":"sparse","dfg":[{"source":"A","target":"B","count":1.0}]})",
      R"({"kind":"sparse","dfg":[{"source":"A","target":"B","count":"1"}]})",
      R"({"kind":"sparse","dfg":[{"source":1,"target":"B","count":1}]})",
      R"({"kind":"sparse","dfg":[],"start_activities":{"A":1,"\u0041":2},"end_activities":{}})"});
}

void heuristic_input_preserves_every_native_field() {
  const std::string json = R"({"dfg":[{"source":"B","target":"A","count":18446744073709551615},{"source":"A","target":"B","count":0}],"activities":["B","A","A"," isolated "],"activity_counts":{"A":18446744073709551615,"B":0},"start_counts":{},"end_counts":{"B":1},"window_two_dfg":[{"source":"A","target":"A","count":2}],"frequency_triples":[{"source":"A","via":"B","target":"A","count":3}],"dfg_window_2_supplied":false,"freq_triples_supplied":true,"start_activities":["B","B"],"end_activities":{"A":0},"dfg_order":[{"source":"B","target":"A"},{"source":"A","target":"B"}],"performance_dfg":[{"source":"A","target":"B","samples":[-0.0,1.25,"NaN","INF","-INF"]}],"performance_values":[{"source":"A","target":"B","value":{"type":"scalar","value":-0.0}},{"source":"B","target":"A","value":{"type":"samples","value":[]}},{"source":"S","target":"T","value":{"type":"summary","value":{"mean":1,"median":2,"minimum":-3,"maximum":4,"stdev":"NaN","sum":"INF"}}},{"source":" X,>:;\t ","target":"y\u0000z","value":{"type":"structured","value":{"type":"object","value":{"n":{"type":"null","value":null},"b":{"type":"boolean","value":true},"i":{"type":"int64","value":1},"u":{"type":"uint64","value":18446744073709551615},"s":{"type":"string","value":" \u4e2d "},"a":{"type":"array","value":[{"type":"int64","value":-9223372036854775808},{"type":"float","value":"-INF"},{"type":"float","value":-0.0}]}}}}}]})";
  const auto input = parse_heuristic_dfg_input_json(json);
  assert(input.activities->size() == 4 && input.activities->at(2) == "A");
  assert(input.activity_counts->at("A") == std::numeric_limits<std::uint64_t>::max());
  assert(input.start_counts && input.start_counts->empty());
  assert(input.end_counts->at("B") == 1);
  assert(!input.dfg_window_2_supplied && input.freq_triples_supplied);
  assert(std::get<std::vector<std::string>>(*input.start_activities).size() == 2);
  assert((std::get<std::map<std::string, std::uint64_t>>(*input.end_activities).at("A") == 0));
  const auto encoded = serialize_heuristic_dfg_input_json(input);
  const auto output = parse_heuristic_dfg_input_json(encoded);
  assert(output.dfg == input.dfg && output.activities == input.activities &&
         output.activity_counts == input.activity_counts && output.start_counts == input.start_counts &&
         output.end_counts == input.end_counts && output.window_two_dfg == input.window_two_dfg &&
         output.frequency_triples == input.frequency_triples && output.dfg_order == input.dfg_order);
  assert(output.start_activities == input.start_activities && output.end_activities == input.end_activities);
  assert(!output.dfg_window_2_supplied && output.freq_triples_supplied);
  const auto& samples = output.performance_dfg->at({"A", "B"});
  assert(samples.size() == 5 && std::signbit(samples[0]) && std::isnan(samples[2]) &&
         std::isinf(samples[3]) && std::signbit(samples[4]));
  assert(std::signbit(std::get<double>(output.performance_values->at({"A", "B"}))));
  assert(std::get<std::vector<double>>(output.performance_values->at({"B", "A"})).empty());
  const auto& summary = std::get<HeuristicPerformanceSummary>(output.performance_values->at({"S", "T"}));
  assert(summary.minimum == -3 && std::isnan(summary.stdev) && std::isinf(summary.sum));
  const auto& root = std::get<HeuristicStructuredValue::Object>(
      std::get<HeuristicStructuredValue>(output.performance_values->at(
          {" X,>:;\t ", std::string("y\0z", 3)})).value);
  assert(std::get<std::int64_t>(root.at("i").value) == 1);
  assert(std::get<std::uint64_t>(root.at("u").value) == std::numeric_limits<std::uint64_t>::max());
  assert(std::get<std::string>(root.at("s").value) == " \xE4\xB8\xAD ");
  const auto& nested = std::get<HeuristicStructuredValue::Array>(root.at("a").value);
  assert(std::get<std::int64_t>(nested[0].value) == std::numeric_limits<std::int64_t>::min());
  assert(std::signbit(std::get<double>(nested[2].value)));
  assert(serialize_heuristic_dfg_input_json(output) == encoded);
}

void heuristic_input_preserves_omitted_and_explicit_empty_fields() {
  const auto omitted = parse_heuristic_dfg_input_json(R"({"dfg":[]})");
  assert(!omitted.activities && !omitted.activity_counts && !omitted.start_counts && !omitted.end_counts);
  assert(!omitted.performance_dfg && !omitted.performance_values &&
         !omitted.start_activities && !omitted.end_activities && !omitted.dfg_order);
  const auto round_trip = parse_heuristic_dfg_input_json(serialize_heuristic_dfg_input_json(omitted));
  assert(!round_trip.dfg_window_2_supplied && !round_trip.freq_triples_supplied);
  const auto empty = parse_heuristic_dfg_input_json(R"({"dfg":[],"activities":[],"activity_counts":{},"start_counts":{},"end_counts":{},"window_two_dfg":[],"frequency_triples":[],"performance_dfg":[],"performance_values":[],"start_activities":[],"end_activities":{},"dfg_order":[]})");
  assert(empty.activities && empty.activities->empty() && empty.activity_counts && empty.activity_counts->empty());
  assert(empty.start_counts && empty.end_counts && empty.performance_dfg && empty.performance_values);
  assert(empty.start_activities && empty.end_activities && empty.dfg_order);
  assert(empty.dfg_window_2_supplied && empty.freq_triples_supplied);
  assert(std::holds_alternative<std::vector<std::string>>(*empty.start_activities));
  assert((std::holds_alternative<std::map<std::string, std::uint64_t>>(*empty.end_activities)));
}

void heuristic_input_rejects_malformed_fields_and_duplicates() {
  rejects(parse_heuristic_dfg_input_json, {
      "{}", "[]", R"({"dfg":[],"unknown":1})", R"({"dfg":[],"activities":null})",
      R"({"dfg":[],"start_counts":{"A":true}})", R"({"dfg":[],"dfg_window_2_supplied":0})",
      R"({"dfg":[],"window_two_dfg":[{"source":"A","target":"B","count":1},{"source":"A","target":"B","count":2}]})",
      R"({"dfg":[],"frequency_triples":[{"source":"A","via":"B","target":"A","count":1},{"source":"A","via":"B","target":"A","count":2}]})",
      R"({"dfg":[],"dfg_order":[{"source":"A","target":"B"}]})",
      R"({"dfg":[{"source":"A","target":"B","count":1}],"dfg_order":[]})",
      R"({"dfg":[],"performance_dfg":[{"source":"A","target":"B","samples":["1"]}]})",
      R"({"dfg":[],"performance_values":[{"source":"A","target":"B","value":{"type":"other","value":0}}]})",
      R"({"dfg":[],"performance_values":[{"source":"A","target":"B","value":{"type":"scalar","value":1e309}}]})",
      R"({"dfg":[],"performance_values":[{"source":"A","target":"B","value":{"type":"structured","value":{"type":"int64","value":9223372036854775808}}}]})",
      R"({"dfg":[],"performance_values":[{"source":"A","target":"B","value":{"type":"structured","value":{"type":"null","value":false}}}]})",
      R"({"dfg":[],"performance_values":[{"source":"A","target":"B","value":{"type":"structured","value":{"type":"object","value":{"A":{"type":"null","value":null},"\u0041":{"type":"null","value":null}}}}}]})",
      R"({"dfg":[],"performance_dfg":[{"source":"A","target":"B","samples":[]},{"source":"\u0041","target":"B","samples":[]}]})",
      R"({"dfg":[],"performance_values":[{"source":"A","target":"B","value":{"type":"samples","value":[]}},{"source":"\u0041","target":"B","value":{"type":"scalar","value":0}}]})"});
}

void selector_strings_are_exact_and_strict() {
  const std::string json = R"({"empty":""," whitespace ":" \t\n ","\u4e2d":"a,b:;>\\\"","nul":"a\u0000b"})";
  const auto selectors = parse_workflow_selector_strings_json(json);
  assert(selectors.at("empty").empty());
  assert(selectors.at(" whitespace ") == " \t\n ");
  assert(selectors.at("\xE4\xB8\xAD") == "a,b:;>\\\"");
  assert(selectors.at("nul") == std::string("a\0b", 3));
  assert(parse_workflow_selector_strings_json("{}").empty());
  rejects(parse_workflow_selector_strings_json, {"[]", "null", "true",
      R"({"x":null})", R"({"x":0})", R"({"x":[]})", R"({"x":{}})",
      R"({"A":"x","\u0041":"y"})", "{} trailing", "{\"x\":\"\xC0\xAF\"}"});
}

void standalone_structured_codec_retains_native_types_and_tags() {
  const std::string json = R"({"type":"array","value":[{"type":"int64","value":1},{"type":"uint64","value":1},{"type":"float","value":"NaN"},{"type":"float","value":"INF"},{"type":"float","value":-0.0},{"type":"string","value":"a\u0000b"}]})";
  const auto parsed = parse_workflow_structured_value_json(json);
  const auto encoded = serialize_workflow_structured_value_json(parsed);
  const auto result = parse_workflow_structured_value_json(encoded);
  const auto& array = std::get<HeuristicStructuredValue::Array>(result.value);
  assert(std::holds_alternative<std::int64_t>(array[0].value));
  assert(std::holds_alternative<std::uint64_t>(array[1].value));
  assert(std::isnan(std::get<double>(array[2].value)));
  assert(std::isinf(std::get<double>(array[3].value)));
  assert(std::signbit(std::get<double>(array[4].value)));
  assert(std::get<std::string>(array[5].value) == std::string("a\0b", 3));
  assert(serialize_workflow_structured_value_json(result) == encoded);
}

void new_codecs_escape_lone_surrogates_without_loss() {
  const std::string high = "\xED\xA0\x80", low = "\xED\xBF\xBF";
  const std::string table_json = R"({"columns":["\ud800"],"rows":[[{"value":"\udfff\u0041"}]],"attributes":[{"key":"\ud800","value":"\udfff"}]})";
  const auto table = parse_event_table_json(table_json);
  assert(table.columns[0] == high && table.rows[0][0]->value == low + "A");
  const auto encoded_table = serialize_event_table_input_json(table);
  assert(encoded_table.find("\\ud800") != std::string::npos &&
         encoded_table.find("\\udfff") != std::string::npos);
  assert(encoded_table.find(high) == std::string::npos && encoded_table.find(low) == std::string::npos);
  const auto table_again = parse_event_table_json(encoded_table);
  assert(table_again.rows == table.rows && table_again.attributes == table.attributes);
  const std::string dfg_json = R"({"kind":"sparse","dfg":[{"source":"\ud800","target":"\udfff","count":1}]})";
  const auto graph = parse_alpha_dfg_input_json(dfg_json);
  assert(graph.dfg.at({high, low}) == 1);
  const auto encoded_graph = serialize_alpha_dfg_input_json(graph);
  assert(encoded_graph.find(high) == std::string::npos && encoded_graph.find("\\ud800") != std::string::npos);
  const std::string selectors_json = R"({"\ud800":"\udfff"})";
  const auto selectors = parse_workflow_selector_strings_json(selectors_json);
  assert(selectors.at(high) == low);
  const std::string structured_json = R"({"type":"object","value":{"\ud800":{"type":"string","value":"\ud800\ud800"}}})";
  const auto structured = parse_workflow_structured_value_json(structured_json);
  const auto& object = std::get<HeuristicStructuredValue::Object>(structured.value);
  assert(std::get<std::string>(object.at(high).value) == high + high);
  const auto encoded_structured = serialize_workflow_structured_value_json(structured);
  assert(encoded_structured.find(high) == std::string::npos && encoded_structured.find("\\ud800") != std::string::npos);
  const auto encoded_performance = serialize_workflow_performance_value_json(
      HeuristicPerformanceValue{structured});
  assert(encoded_performance.find(high) == std::string::npos &&
         encoded_performance.find("\\ud800") != std::string::npos);
  rejects(parse_event_table_json, {"{\"columns\":[\"" + high + "\"],\"rows\":[]}"});
  rejects(parse_workflow_selector_strings_json, {"{\"x\":\"" + low + "\"}"});
}

void complete_native_outputs_do_not_inherit_input_transport_caps() {
  HeuristicStructuredValue deep{std::int64_t{7}};
  EventAttribute attribute{"leaf", "7", AttributeKind::kInt};
  attribute.typed_value = std::int64_t{7};
  for (int i = 0; i < 90; ++i) {
    HeuristicStructuredValue::Array array;
    array.push_back(std::move(deep));
    deep = HeuristicStructuredValue{std::move(array)};
    EventAttribute parent{"node", "", AttributeKind::kString};
    parent.children.push_back(std::move(attribute));
    parent.scalar_wrapper = true;
    attribute = std::move(parent);
  }
  const auto encoded = serialize_workflow_structured_value_json(deep);
  std::size_t arrays = 0;
  for (std::size_t position = 0; (position = encoded.find("\"type\":\"array\"", position)) != std::string::npos; ++position)
    ++arrays;
  assert(arrays == 90 && encoded.find("\"type\":\"int64\",\"value\":7") != std::string::npos);
  const auto decorated = serialize_workflow_performance_value_json(HeuristicPerformanceValue{deep});
  assert(decorated.find(encoded) != std::string::npos);
  HeuristicClassicDfgInput graph;
  graph.performance_values.emplace();
  graph.performance_values->emplace(std::pair<std::string, std::string>{"A", "B"}, deep);
  const auto graph_output = serialize_heuristic_dfg_input_json(graph);
  assert(graph_output.find(encoded) != std::string::npos);
  EventTable table;
  table.metadata.properties.push_back(std::move(attribute));
  const auto table_output = serialize_event_table_input_json(table);
  assert(table_output.find("\"integer_storage\":\"int64\"") != std::string::npos);
  assert(table_output.find("\"key\":\"leaf\"") != std::string::npos);
  rejects(parse_workflow_structured_value_json, {encoded});
  rejects(parse_heuristic_dfg_input_json, {graph_output});
  rejects(parse_event_table_json, {table_output});

  bool size_rejected = false;
  try { (void)parse_workflow_selector_strings_json(std::string(32u * 1024u * 1024u + 1, ' ')); }
  catch (const std::invalid_argument& error) {
    size_rejected = std::string(error.what()).find("32 MiB") != std::string::npos;
  }
  assert(size_rejected);
  std::string many_values = "[";
  many_values.reserve(2000003);
  for (int i = 0; i < 1000000; ++i) {
    if (i) many_values += ',';
    many_values += '0';
  }
  many_values += ']';
  bool nodes_rejected = false;
  try { (void)parse_workflow_structured_value_json(many_values); }
  catch (const std::invalid_argument& error) {
    nodes_rejected = std::string(error.what()).find("one million") != std::string::npos;
  }
  assert(nodes_rejected);
}
}  // namespace

int main() {
  table_codec_preserves_all_metadata_and_integer_storage();
  strict_table_codec_rejects_invalid_schema_and_storage();
  table_codec_preserves_nonfinite_and_distinct_missing_markers();
  alpha_graph_forms_preserve_presence_labels_and_registry_order();
  alpha_graph_forms_reject_invalid_or_ambiguous_input();
  heuristic_input_preserves_every_native_field();
  heuristic_input_preserves_omitted_and_explicit_empty_fields();
  heuristic_input_rejects_malformed_fields_and_duplicates();
  selector_strings_are_exact_and_strict();
  standalone_structured_codec_retains_native_types_and_tags();
  new_codecs_escape_lone_surrogates_without_loss();
  complete_native_outputs_do_not_inherit_input_transport_caps();
}
