#include "minera/source_acquisition.hpp"
#include "minera/xes_export.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace minera;
#undef assert
#define assert(condition) do{if(!(condition)){std::cerr<<"Assertion failed at "<<__LINE__<<": "<<#condition<<'\n';throw std::runtime_error(#condition);}}while(false)
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
void normalized_sources_preserve_original_bytes_and_native_fields() {
  SourceAcquisitionInput input;input.connector="camunda_workflow";input.source_bytes="process_id,case_id,activity,timestamp,resource\np,c1,\"A,東京\",2024-01-01T00:00:00Z,r\n";const auto workflow=acquire_source(input);assert(workflow.log.event_count()==1&&workflow.original_bytes==input.source_bytes);const auto* process=workflow.log.traces[0].attribute("case:process_id");assert(process&&*process=="p");assert(source_acquisition_json(workflow).find("\"native_log\":{")!=std::string::npos);
  input.connector="sap_o2c";input.source_bytes="case_id,activity,timestamp,resource,document_type\nc,Create,2024-01-01T00:00:00Z,r,東京\n";const auto order=acquire_source(input);assert(order.log.traces[0].events[0].activity=="Create"&&order.original_bytes==input.source_bytes);input.connector="sap_accounting";const auto accounting=acquire_source(input);const auto* document=accounting.log.traces[0].events[0].attribute("document_type");assert(document&&*document=="東京"&&accounting.native_result_json.find("\"variant\":\"accounting\"")!=std::string::npos);
}
void mailbox_and_calendar_native_event_expansion_is_preserved() {
  SourceAcquisitionInput input;input.connector="outlook_mail_extractor";input.source_bytes="conversation_id,direction,item_class,created_at,sender,recipients,topic,subject\nc,Sent,43,2024-01-01T00:00:00Z,A,B,T,Hello\n";assert(acquire_source(input).log.event_count()==1);
  input.connector="outlook_calendar";input.parameters["communication_current_time"]="2024-02-01T00:00:00Z";input.source_bytes="conversation_id,subject,created_at,last_changed_at,start_at,end_at,cancelled,reminder_at\nc,Meet,2024-01-01T00:00:00Z,2024-01-02T00:00:00Z,2024-01-03T09:00:00Z,2024-01-03T10:00:00Z,true,2024-01-03T08:45:00Z\n";assert(acquire_source(input).log.event_count()==6);input.parameters["communication_include_reminders"]="false";assert(acquire_source(input).log.event_count()==5);
}
void repository_transport_is_injected_and_tokens_are_not_serialized() {
  SourceAcquisitionInput input;input.connector="github_repo";input.parameters={{"repository_owner","acme"},{"repository_name","engine"},{"repository_api_base_url","https://api.example.test"}};input.auth_token="synthetic-secret";input.repository_transport=[](std::string_view url,std::string_view token){assert(token=="synthetic-secret");if(url.ends_with("page=1"))return RepositoryHistoryResponse{200,R"([{"timeline_url":"https://api.example.test/repos/acme/engine/issues/1/timeline","created_at":"2024-01-01T00:00:00Z","user":{"login":"a"},"author_association":"NONE","title":"T"}])"};return RepositoryHistoryResponse{200,"[]"};};const auto result=acquire_source(input);assert(result.log.trace_count()==1&&result.log.event_count()==1);assert(source_acquisition_json(result).find(input.auth_token)==std::string::npos);
}
void invalid_sources_fail_explicitly() {
  SourceAcquisitionInput input;input.connector="unknown";input.source_bytes="x";rejects([&]{(void)acquire_source(input);});input.connector="chrome_history";input.source_bytes="not sqlite";rejects([&]{(void)acquire_source(input);});input.connector="windows_events";input.source_bytes="<Events><Event><System><Provider Name=\"P\"/><EventID>1</EventID></System></Event></Events>";rejects([&]{(void)acquire_source(input);});input.server_path="also-a-path";rejects([&]{(void)acquire_source(input);});
}

std::string all_xes_options(const std::string& variant, const std::string& compress,
                            const std::string& name = "\"\"") {
  return "{\"variant\":" + variant + ",\"compress\":" + compress + ",\"name\":" + name +
      ",\"infer_metadata\":true,\"preserve_nulls\":false}";
}
void exact_xes_options_preserve_complete_native_domain() {
  for (const auto& compression : {std::string("null"), std::string("false"), std::string("true")}) {
    const auto options = parse_source_acquisition_xes_options_json(
        all_xes_options("1", compression, "{\"native_bytes\":\"hex-v1\",\"data\":\"6100ffeda080\"}"));
    assert(options.variant == XesExportVariant::kElementTree);
    assert(options.compress == (compression == "null" ? std::optional<bool>{} : std::optional<bool>{compression == "true"}));
    assert(options.name == std::string("a\0\xff\xed\xa0\x80", 6));
    assert(options.infer_metadata && !options.preserve_nulls);
    const auto serialized = source_acquisition_xes_options_json(options);
    const auto again = parse_source_acquisition_xes_options_json(serialized);
    assert(again.variant == options.variant && again.compress == options.compress && again.name == options.name);
    assert(again.infer_metadata == options.infer_metadata && again.preserve_nulls == options.preserve_nulls);
  }
  const auto unknown = parse_source_acquisition_xes_options_json(all_xes_options("-2147483648", "null"));
  assert(static_cast<int>(unknown.variant) == std::numeric_limits<int>::min());
  assert(source_acquisition_xes_options_json(unknown).find("\"name\":null,\"index\":-2147483648") != std::string::npos);
  rejects([&] { (void)serialize_xes(EventLog{}, unknown); }); // Native validation, not transport validation.
  const auto named = parse_source_acquisition_xes_options_json(all_xes_options("{\"name\":\"descriptive only\",\"index\":0}", "false"));
  assert(named.variant == XesExportVariant::kLineByLine);
  const auto escaped = parse_source_acquisition_xes_options_json(all_xes_options("0", "null", "\"\\ud800\""));
  const auto raw = parse_source_acquisition_xes_options_json(all_xes_options("0", "null", std::string("\"\xed\xa0\x80\"", 5)));
  assert(escaped.name == std::string("\xed\xa0\x80", 3) && raw.name == escaped.name);
  const std::string long_name(100001, 'x');
  assert(parse_source_acquisition_xes_options_json(all_xes_options("0", "false", "\"" + long_name + "\"")).name == long_name);
}
void exact_xes_options_reject_structural_narrowing_and_ambiguity() {
  for (const auto& invalid : {
      "null", "[]", "{}", "true",
      "{\"variant\":0,\"compress\":null,\"name\":\"\",\"infer_metadata\":false}",
      "{\"variant\":0,\"compress\":null,\"name\":\"\",\"infer_metadata\":false,\"preserve_nulls\":false,\"extra\":0}"})
    rejects([&] { (void)parse_source_acquisition_xes_options_json(invalid); });
  for (const auto& variant : {"null", "true", "\"0\"", "0.0", "2147483648", "-2147483649", "{\"index\":0}",
      "{\"name\":false,\"index\":0}", "{\"name\":null,\"index\":0,\"extra\":0}"})
    rejects([&] { (void)parse_source_acquisition_xes_options_json(all_xes_options(variant, "null")); });
  for (const auto& compression : {"0", "\"true\"", "{}"})
    rejects([&] { (void)parse_source_acquisition_xes_options_json(all_xes_options("0", compression)); });
  for (const auto& name : {"null", "false", "{\"native_bytes\":\"hex-v1\",\"data\":\"f\"}",
      "{\"native_bytes\":\"hex-v1\",\"data\":\"gg\"}", "{\"native_bytes\":\"other\",\"data\":\"ff\"}"})
    rejects([&] { (void)parse_source_acquisition_xes_options_json(all_xes_options("0", "null", name)); });
  auto duplicate = all_xes_options("0", "null");
  duplicate.insert(1, "\"na\\u006de\":\"duplicate\",");
  rejects([&] { (void)parse_source_acquisition_xes_options_json(duplicate); });
  rejects([&] { (void)parse_source_acquisition_xes_options_json(all_xes_options("0", "null", std::string("\"\xff\"", 3))); });
}
EventLog exact_manual_log(const std::string& label) {
  EventLog log;
  EventAttribute attribute{std::string("key\0\xff", 5), label};
  attribute.value = std::string("stored\0", 7);
  attribute.typed_value = std::numeric_limits<std::uint64_t>::max();
  attribute.scalar_wrapper = true;
  attribute.scalar_children_list = true;
  attribute.children = {{"duplicate", "one"}, {"duplicate", "two"}};
  attribute.first_class_role = static_cast<FirstClassAttributeRole>(17);
  attribute.first_class_state = static_cast<FirstClassAttributeState>(23);
  attribute.first_class_source_key = std::string("source\xed\xa0\x80", 9);
  log.attributes = {attribute};
  log.metadata.extensions = {{label, "p", "u"}};
  log.metadata.classifiers = {{label, {"a", "a"}}};
  log.metadata.trace_globals = {attribute};
  log.metadata.event_globals = {{"event", label}};
  log.metadata.properties = {{"property", label}};
  log.metadata.globals = {{"event", {attribute}}, {"event", {}}};
  log.metadata.compatibility_snapshot_valid = true;
  log.metadata.compatibility_globals_snapshot = {{"latent", {{"x", label}}}};
  log.metadata.compatibility_trace_globals_snapshot = {{"latent-trace", label}};
  log.metadata.compatibility_event_globals_snapshot = {{"latent-event", label}};
  Trace trace;
  trace.case_id = label;
  trace.case_id_state = static_cast<FirstClassAttributeState>(-4);
  trace.case_id_attribute = attribute;
  Event event;
  event.case_id = label;
  event.activity = label;
  event.input_order = std::numeric_limits<std::size_t>::max();
  event.activity_attribute = attribute;
  event.attributes = {attribute, attribute};
  trace.events = {event};
  log.traces = {trace};
  return log;
}
void exact_result_retains_all_optional_capture_constructor_state() {
  SourceAcquisitionResult result;
  const std::string bytes("a\0\xff\xed\xa0\x80", 6);
  result.connector = bytes;
  result.log = exact_manual_log("primary");
  result.native_result_json = std::string("not-json\0\xff", 10);
  result.original_bytes = std::string{}; // Engaged empty is different from absent.
  result.windows_capture = WindowsEventCapture{static_cast<WindowsEventSourceKind>(-19), bytes, bytes, true,
      std::numeric_limits<std::size_t>::max(), true, bytes};
  DatabaseHistoryCapture database;
  database.connector = bytes; database.driver = bytes; database.prefix = bytes;
  database.dialect = bytes; database.query = bytes; database.reference_query = bytes;
  database.table.columns = {bytes, "", bytes};
  database.table.column_types = {"one type for three columns"};
  database.table.rows = {{std::nullopt, std::string{}, bytes}, {}, {bytes}};
  database.table.limited = true;
  database.table.query_timeout_applied = false;
  database.log = exact_manual_log("database independent");
  database.raw_table_json = bytes; database.formatted_table_json = "invalid JSON";
  result.database_capture = database;
  OutlookSourceCapture outlook;
  outlook.log = exact_manual_log("outlook independent");
  outlook.connector = bytes; outlook.email_user = bytes;
  outlook.calendar_id = std::numeric_limits<unsigned>::max();
  outlook.total_items = std::numeric_limits<std::size_t>::max();
  outlook.examined_items = 7; outlook.skipped_items = 5; outlook.limited = true;
  outlook.item_table_json = bytes; outlook.formatted_table_json = "null";
  result.outlook_capture = outlook;
  const auto before_primary = exact_source_snapshot(EventDataView{result.log});
  const auto before_database = exact_source_snapshot(EventDataView{result.database_capture->log});
  const auto before_outlook = exact_source_snapshot(EventDataView{result.outlook_capture->log});
  const auto text = source_acquisition_result_exact_json(result);
  const auto safe_bytes = serialize_embedding_workbench_string_json(bytes);
  assert(text.find("\"connector\":" + safe_bytes) != std::string::npos);
  assert(text.find("\"native_result_json\":" + serialize_embedding_workbench_string_json(result.native_result_json)) != std::string::npos);
  assert(text.find("\"original_bytes\":\"\"") != std::string::npos);
  assert(text.find("\"kind\":{\"name\":null,\"index\":-19}") != std::string::npos);
  assert(text.find("\"rows\":[[null,\"\"," + safe_bytes + "],[],[" + safe_bytes + "]]") != std::string::npos);
  assert(text.find("\"query_timeout_applied\":false") != std::string::npos);
  assert(text.find("\"calendar_id\":" + std::to_string(std::numeric_limits<unsigned>::max())) != std::string::npos);
  assert(text.find("\"events\":" + std::to_string(std::numeric_limits<std::size_t>::max())) != std::string::npos);
  assert(text.find("\"log\":" + before_primary) != std::string::npos);
  assert(text.find("\"log\":" + before_database) != std::string::npos);
  assert(text.find("\"log\":" + before_outlook) != std::string::npos);
  const auto artifacts = source_acquisition_artifacts_json(result);
  // Invalid legacy enum/state is an optional projection error, while each raw
  // capture and the complete primary/capture snapshots remain available.
  assert(artifacts.find("\"source_acquisition_json\":{\"bytes\":null,\"error\":") != std::string::npos);
  assert(artifacts.find("\"windows_xml\":{\"bytes\":" + safe_bytes + ",\"error\":null}") != std::string::npos);
  assert(artifacts.find("\"database_raw_table_json\":{\"bytes\":" + safe_bytes + ",\"error\":null}") != std::string::npos);
  assert(exact_source_snapshot(EventDataView{result.log}) == before_primary);
  assert(exact_source_snapshot(EventDataView{result.database_capture->log}) == before_database);
  assert(exact_source_snapshot(EventDataView{result.outlook_capture->log}) == before_outlook);
  result.original_bytes.reset();
  assert(source_acquisition_result_exact_json(result).find("\"original_bytes\":null") != std::string::npos);
  SourceAcquisitionResult empty;
  empty.windows_capture = WindowsEventCapture{};
  empty.database_capture.emplace();
  empty.outlook_capture.emplace();
  const auto engaged = source_acquisition_result_exact_json(empty);
  assert(engaged.find("\"windows_capture\":{") != std::string::npos);
  assert(engaged.find("\"database_capture\":{") != std::string::npos);
  assert(engaged.find("\"outlook_capture\":{") != std::string::npos);
  empty.windows_capture.reset(); empty.database_capture.reset(); empty.outlook_capture.reset();
  assert(source_acquisition_result_exact_json(empty).find("\"windows_capture\":null,\"database_capture\":null,\"outlook_capture\":null") != std::string::npos);
}
void exact_xes_path_compression_uses_native_suffix_and_retains_options() {
  const auto base = std::filesystem::temp_directory_path() / ("minera-exact-acquisition-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto cleanup = [&] { std::error_code error; std::filesystem::remove(base, error); std::filesystem::remove(base.string() + ".gz", error); };
  try {
    XesExportOptions options;
    options.name = "native name";
    const auto before = source_acquisition_xes_options_json(options);
    const auto path = export_xes_event_log(base.string() + ".gz", EventLog{}, options);
    std::ifstream file(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    assert(bytes.size() > 2 && static_cast<unsigned char>(bytes[0]) == 0x1f && static_cast<unsigned char>(bytes[1]) == 0x8b);
    assert(!options.compress && source_acquisition_xes_options_json(options) == before);
    options.compress = false;
    const auto plain_path = export_xes_event_log(base, EventLog{}, options);
    std::ifstream plain(plain_path, std::ios::binary);
    const std::string xml((std::istreambuf_iterator<char>(plain)), std::istreambuf_iterator<char>());
    assert(xml == serialize_xes(EventLog{}, options));
    cleanup();
  } catch (...) { cleanup(); throw; }
}
int main() {
  try {
    normalized_sources_preserve_original_bytes_and_native_fields();
    mailbox_and_calendar_native_event_expansion_is_preserved();
    repository_transport_is_injected_and_tokens_are_not_serialized();
    invalid_sources_fail_explicitly();
    exact_xes_options_preserve_complete_native_domain();
    exact_xes_options_reject_structural_narrowing_and_ambiguity();
    exact_result_retains_all_optional_capture_constructor_state();
    exact_xes_path_compression_uses_native_suffix_and_retains_options();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
