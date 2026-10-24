#include "minera/log_timestamp_filter_workbench.hpp"
#include "../log_timestamp_original_call_parser.hpp"
#include <functional>
#include <iostream>

namespace log_timestamp_workbench_test {
using namespace minera;
namespace original = minera::log_timestamp_original_call_detail;
using Parts = std::vector<DiscoveryWorkbenchPart>;
inline void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
inline DiscoveryWorkbenchPart field(std::string name, std::string bytes) { return {std::move(name), "", std::move(bytes), false}; }
inline std::optional<std::string> text(const std::string& raw, std::string_view key) {
  const auto root = original::shared::Reader(raw).read(); const auto& node = original::at(root, key);
  if (node.kind == original::Node::kNull) return std::nullopt;
  require(node.kind == original::Node::kString, "expected string artifact " + std::string(key)); return node.text;
}
inline bool boolean(const std::string& raw, std::string_view key) {
  const auto root = original::shared::Reader(raw).read(); const auto& node = original::at(root, key);
  require(node.kind == original::Node::kBoolean, "expected Boolean flag " + std::string(key)); return node.boolean;
}
inline std::string descriptor(std::string args, std::string keywords = "[]", std::string selected = "null") {
  return "{\"schema\":\"minera-log-timestamp-original-call-v1\",\"args\":" + args + ",\"keyword_entries\":" + keywords + ",\"source_trace_index\":" + selected + '}';
}
inline std::string date(std::string value) { return "{\"type\":\"datetime\",\"iso\":" + original::q(value) + ",\"fold\":0,\"offset_seconds\":0}"; }
inline std::string bounds() { return date("2026-06-01T00:00:00Z") + ',' + date("2026-06-01T00:00:02Z"); }
inline EventLog source_log() {
  EventLog source; source.attributes = {{"opaque", "native context"}}; source.metadata.properties = {{"retained", "log properties"}};
  Trace trace; trace.case_id = "native case"; trace.case_id_state = FirstClassAttributeState::kPresent; trace.case_id_source_key = "concept:name";
  trace.case_id_attribute = EventAttribute{"concept:name", "native case"}; trace.attributes = {{"concept:name", "native case"}}; trace.properties = {{"retained", "trace properties"}};
  EventAttribute timestamp{"time:timestamp", "2026-06-01T00:00:01", AttributeKind::kDate}; timestamp.typed_value = NativeDate{"2026-06-01T00:00:01"};
  trace.attributes.push_back(timestamp);
  Event event; event.activity = "A"; event.activity_source_key = "concept:name"; event.activity_state = FirstClassAttributeState::kPresent; event.activity_attribute = EventAttribute{"concept:name", "A"};
  event.timestamp = timestamp.value; event.timestamp_source_key = timestamp.key; event.timestamp_state = FirstClassAttributeState::kPresent; event.timestamp_attribute = timestamp; event.input_order = 42;
  event.attributes = {{"concept:name", "A"}, timestamp, {"integer", "9007199254740993", AttributeKind::kInt}};
  trace.events.push_back(event); source.traces.push_back(trace);
  Trace empty; empty.case_id = "empty"; empty.properties = {{"empty", "retained"}}; source.traces.push_back(empty);
  return source;
}
inline Parts request(const EventLog& source, std::string operation, std::string call) {
  return {field("operation", std::move(operation)), {"source_file", "native.json", serialize_embedding_workbench_raw_log_json(source), true}, field("call_json", std::move(call))};
}
inline void envelope(const DiscoveryWorkbenchResponse& response) {
  const auto root = original::shared::Reader(response.json).read(); std::set<std::string> keys;
  for (const auto& [key, value] : root.fields) { (void)value; keys.insert(key); }
  require(keys == std::set<std::string>{"family","operation","http_status","phase","source_format","source","result","source_unchanged","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_log_json","source_after_log_json","filtered_log_json","filtered_xes","xes_error","value_json","error_type","error","native_returned","result_kind","call_projection_json","inspection_error"}, "not exact25 response envelope");
  require(text(response.json, "family") == "REFERENCE_LOG_TIMESTAMP", "wrong family");
}
inline void reopen(const DiscoveryWorkbenchResponse& response) {
  for (const auto key : {"source_log_json", "source_after_log_json", "filtered_log_json"}) if (const auto artifact = text(response.json, key)) {
    require(serialize_embedding_workbench_raw_log_json(parse_embedding_workbench_raw_log_json(*artifact)) == *artifact, "native log reopen changed " + std::string(key));
  }
  if (const auto artifact = text(response.json, "value_json")) require(serialize_embedding_workbench_table_json(parse_embedding_workbench_table_json(*artifact)) == *artifact, "native bool reopen changed");
  for (const auto key : {"controls_before_json", "controls_after_json"}) if (const auto artifact = text(response.json, key)) {
    const auto controls = original::shared::Reader(*artifact).read(); require(controls.fields.size() == 4, "controls fields lost");
    const auto& source = original::at(controls, "source_argument");
    for (const auto field_name : {"log_json", "trace_json"}) { const auto& log = original::at(source, field_name); if (log.kind != original::Node::kNull) (void)parse_embedding_workbench_raw_log_json(original::string(log)); }
    for (const auto& arg : original::at(controls, "arguments").items) (void)parse_embedding_workbench_table_json(original::string(original::at(arg, "value_json")));
    const auto& options = original::at(controls, "options"); if (options.kind != original::Node::kNull) for (const auto& [name, cell] : options.fields) { (void)name; if (cell.kind != original::Node::kNull) (void)parse_embedding_workbench_table_json(original::string(cell)); }
  }
}
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run) {
  const auto source = source_log(); const auto raw_source = serialize_embedding_workbench_raw_log_json(source);
  // NativeDate has the exact browser example's kDate index and NativeDate alternative/default metadata.
  require(raw_source.find("2026-06-01T00:00:01") != std::string::npos, "browser example timestamp missing");
  const auto invoke = [&](const std::string& operation, const std::string& call, const EventLog& input) {
    const auto response = run(request(input, operation, call)); envelope(response); reopen(response);
    require(text(response.json, "call_json") == call, "raw descriptor bytes changed");
    require(text(response.json, "source_log_json") == serialize_embedding_workbench_raw_log_json(input), "full source-before differs");
    return response;
  };
  for (const auto& operation : {"filter_traces_contained", "filter_traces_intersecting", "filter_traces_starting_in_timeframe", "filter_traces_completing_in_timeframe", "filter_on_trace_attribute", "filter_traces_attribute_in_timeframe", "apply_events"}) {
    const auto prefix = std::string(operation) == "filter_traces_attribute_in_timeframe" ? std::string(R"({"type":"str","value":"concept:name"},{"type":"str","value":"A"},)") : "";
    const auto call = std::string("\xef\xbb\xbf", 3) + "\r\n " + descriptor('[' + prefix + bounds() + ']') + "\r\n";
    const auto response = invoke(operation, call, source);
    require(response.status == 200 && text(response.json, "phase") == "complete" && boolean(response.json, "native_returned") && text(response.json, "result_kind") == "log", "native filter dispatch failed: " + response.json);
    const auto result = parse_embedding_workbench_raw_log_json(*text(response.json, "filtered_log_json")); require(result.traces.size() == 1 && result.traces[0].events.size() == 1, "native filtered result wrong");
    require(!text(response.json, "value_json") && !text(response.json, "inspection_error"), "log fabricated scalar or inspection error");
    const bool mutates = std::string(operation) == "apply_events";
    require(boolean(response.json, "source_unchanged") != mutates && boolean(response.json, "controls_unchanged") != mutates, "source/control mutation flag wrong");
    const auto controls = original::shared::Reader(*text(response.json, "controls_before_json")).read(); require(original::at(controls, "options").kind == original::Node::kObject, "explicit Options absent");
    // Independently constructed expected controls from actual native default object and source, not a descriptor mirror.
    const LogTimestampFilterOptions options;
    require(original::string(original::at(original::at(controls, "options"), "positive_json")) == original::cell_json(options.positive), "actual default native positive changed");
  }
  for (const auto& operation : {"is_contained", "is_intersecting", "trace_attr_is_contained", "has_attribute_in_timeframe"}) {
    const bool has_attribute = std::string(operation) == "has_attribute_in_timeframe";
    const auto prefix = has_attribute ? std::string(R"({"type":"str","value":"concept:name"},{"type":"str","value":"A"},)") : "";
    const auto args = '[' + prefix + bounds() + R"(,{"type":"str","value":"time:timestamp"}])";
    for (const auto selected : {"0", "1"}) {
      const auto response = invoke(operation, descriptor(args, "[]", selected), source);
      require(response.status == 200 && text(response.json, "result_kind") == "bool" && boolean(response.json, "native_returned"), "predicate dispatch failed: " + response.json);
      const auto table = parse_embedding_workbench_table_json(*text(response.json, "value_json"));
      require(std::get<bool>(table.rows[0][0]->typed_value) == (std::string(selected) == "0"), "native false/true predicate lost");
      require(!text(response.json, "filtered_log_json") && !text(response.json, "filtered_xes"), "predicate fabricated filtered log");
      const auto controls = original::shared::Reader(*text(response.json, "controls_before_json")).read(); require(original::at(controls, "options").kind == original::Node::kNull, "predicate fabricated Options");
      const auto& argument = original::at(controls, "source_argument");
      EventLog wrapper; wrapper.traces = {source.traces[std::string(selected) == "0" ? 0 : 1]};
      require(original::string(original::at(argument, "trace_json")) == serialize_embedding_workbench_raw_log_json(wrapper), "actual trace control wrapper differs");
    }
  }
  for (const auto& operation : {"apply", "apply_auto_filter"}) {
    const auto response = invoke(operation, descriptor("[]", R"([{"key":"parameters","value":{"type":"native_na"}}])"), source);
    require(response.status == 400 && text(response.json, "phase") == "native_call" && text(response.json, "error_type") == "Exception" && !boolean(response.json, "native_returned"), "source-only stub boundary lost");
    require(!text(response.json, "result_kind") && !text(response.json, "filtered_log_json") && !text(response.json, "value_json"), "stub error fabricated result");
    require(text(response.json, "call_projection_json")->find("implicit_default_not_inspected") != std::string::npos, "stub default inspection falsely claimed");
  }
  auto missing_timestamp = source;
  auto& event = missing_timestamp.traces[0].events[0]; event.attributes.erase(event.attributes.begin() + 1); event.timestamp.clear(); event.timestamp_attribute.reset(); event.timestamp_source_key.clear(); event.timestamp_state = FirstClassAttributeState::kMissing;
  const auto failure = invoke("apply_events", descriptor('[' + bounds() + ']'), missing_timestamp);
  require(failure.status == 400 && text(failure.json, "phase") == "native_call" && text(failure.json, "error_type") == "KeyError" && text(failure.json, "error") == "'time:timestamp'", "mutating error type/message lost");
  require(!boolean(failure.json, "native_returned") && !boolean(failure.json, "source_unchanged") && !boolean(failure.json, "controls_unchanged"), "mutating native error controls/source were hidden");
  const auto source_after = parse_embedding_workbench_raw_log_json(*text(failure.json, "source_after_log_json"));
  require(source_after.traces[0].events[0].attribute("case:concept:name") != nullptr, "partial mutation absent after error");
  require(!text(failure.json, "filtered_log_json") && !text(failure.json, "value_json"), "native error fabricated result");
  for (const auto& parameter : {R"({"type":"NoneType","value":null})", R"({"type":"native_na"})", R"({"type":"list","values":[]})"}) {
    const auto response = invoke("filter_traces_contained", descriptor('[' + bounds() + ']', "[{\"key\":\"parameters\",\"value\":" + std::string(parameter) + "}]"), source);
    require(response.status == 200 && text(response.json, "call_projection_json")->find("non_dictionary") != std::string::npos, "raw parameters became actual native parameters");
  }
  for (const auto& malformed : {std::string(""), descriptor("[]"), descriptor('[' + bounds() + ']', "[]", "0")}) {
    const auto response = run(request(source, "apply_events", malformed)); envelope(response);
    require(response.status == 400 && text(response.json, "error_type") == "control_error" && !boolean(response.json, "native_returned") && !text(response.json, "call_projection_json"), "invalid controls fabricated actual native call");
  }
  auto out_of_range = run(request(source, "is_contained", descriptor('[' + bounds() + R"(,{"type":"str","value":"time:timestamp"}])", "[]", "999")));
  require(out_of_range.status == 400 && !text(out_of_range.json, "controls_before_json") && !text(out_of_range.json, "call_projection_json"), "out-of-range predicate bound fabricated source");
  auto bytes = request(source, "filter_traces_contained", descriptor('[' + bounds() + ']')); bytes.back().file = true; bytes.back().filename = "call_json.txt";
  require(run(bytes).status == 200, "canonical byte File rejected");
  bytes.back().filename = "other.json"; require(run(bytes).status == 400, "noncanonical control File accepted");
  auto duplicated = request(source, "apply", descriptor("[]")); duplicated.push_back(duplicated.back()); require(run(duplicated).status == 400, "duplicate part accepted");
  auto missing = request(source, "apply", descriptor("[]")); missing.erase(missing.begin() + 1); require(run(missing).status == 400, "source-less call accepted");
  auto broken = request(source, "apply", descriptor("[]")); broken[1].bytes = "bad native log";
  const auto bad_import = run(broken); require(bad_import.status == 400 && text(bad_import.json, "phase") == "source_import" && !text(bad_import.json, "call_projection_json"), "source import boundary lost");
  auto invalid_utf8 = request(source, "apply", std::string("\xff", 1)); const auto utf8_failure = run(invalid_utf8);
  const auto root = original::shared::Reader(utf8_failure.json).read();
  require(utf8_failure.status == 400 && original::token(utf8_failure.json, original::at(root, "call_json")) == original::q(std::string("\xff", 1)), "invalid bytes lost NativeString diagnostic fallback");
  auto binary_source = source; binary_source.attributes[0].value = std::string("\xff\0", 2);
  const auto binary_result = invoke("filter_traces_contained", descriptor('[' + bounds() + ']'), binary_source);
  require(binary_result.status == 200, "genuine raw byte-string source rejected");
  const std::string browser_source = R"({"traces":[{"events":[{"attributes":[{"key":"time:timestamp","value":"2026-06-01T00:00:01","kind_index":1,"typed_value":{"index":4,"value":{"value":"2026-06-01T00:00:01","resolution_index":0,"timezone":null}}}]}]}]})";
  auto browser_request = request(source, "filter_traces_contained", descriptor('[' + bounds() + ']')); browser_request[1].bytes = browser_source;
  const auto browser_result = run(browser_request); envelope(browser_result); reopen(browser_result);
  require(browser_result.status == 200 && parse_embedding_workbench_raw_log_json(*text(browser_result.json, "filtered_log_json")).traces.size() == 1, "browser native date carrier shape rejected");
}
}
int main() {
  try {
    log_timestamp_workbench_test::workflows([](const auto& parts) { return minera::execute_log_timestamp_filter(parts); });
    log_timestamp_workbench_test::require(minera::execute_log_timestamp_filter({}, {}, "incomplete multipart").status == 400, "transport accepted");
    std::cout << "LOG timestamp full synthetic workflow controls/artifacts passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
  return 0;
}
