#include "httplib.h"
#include "../tools/reference_log_filter_route.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

namespace {
using namespace minera;
constexpr auto route = "/api/reference-log-filter";
const std::string csv = "case_id,activity\r\nc1,A\r\nc1,B\r\nc1,A\r\nc2,B\r\nc3,\"A,B\"\r\n";
std::string last_http_envelope;
void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

// This test owns exactly this server/socket/thread. It never accesses an app
// process or a fixed service port. Exceptions unwind through stop + join.
class OwnedServer {
 public:
  OwnedServer() {
    server_.Post(route, handle_reference_log_filter);
    server_.set_read_timeout(3, 0);
    server_.set_write_timeout(3, 0);
    port_ = server_.bind_to_any_port("127.0.0.1");
    require(port_ > 0, "could not bind the test's ephemeral loopback socket");
    worker_ = std::thread([this] {
      listen_ok_ = server_.listen_after_bind();
      finished_ = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server_.is_running() && !finished_ && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server_.is_running()) {
      stop();
      throw std::runtime_error("the owned HTTP test server did not become ready");
    }
  }
  OwnedServer(const OwnedServer&) = delete;
  OwnedServer& operator=(const OwnedServer&) = delete;
  ~OwnedServer() { stop(); }
  int port() const { return port_; }
  void stop() {
    server_.stop();
    if (worker_.joinable()) worker_.join();
  }
  bool listened_successfully() const { return finished_ && listen_ok_; }
 private:
  httplib::Server server_;
  std::thread worker_;
  std::atomic<bool> finished_{false};
  std::atomic<bool> listen_ok_{false};
  int port_ = -1;
};

std::string string_token(std::string_view token) {
  const auto values = parse_literal_string_vector_json("[" + std::string(token) + "]");
  require(values.size() == 1, "expected one JSON string token");
  return values.front();
}

// Split only the top-level envelope. String contents remain JSON tokens; the
// public literal-string parser decodes artifacts after envelope validation.
std::map<std::string, std::string> fields(std::string_view raw) {
  std::map<std::string, std::string> result;
  std::size_t at = 0;
  const auto space = [&] {
    while (at < raw.size() && (raw[at] == ' ' || raw[at] == '\t' || raw[at] == '\r' || raw[at] == '\n')) ++at;
  };
  const auto take = [&](char expected) {
    space();
    require(at < raw.size() && raw[at++] == expected, "malformed HTTP JSON envelope delimiter");
  };
  const auto quoted = [&] {
    const auto begin = at;
    require(at < raw.size() && raw[at++] == '"', "expected JSON object key");
    bool escaped = false;
    while (at < raw.size()) {
      const char c = raw[at++];
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') return std::string(raw.substr(begin, at - begin));
    }
    throw std::runtime_error("unterminated JSON envelope key");
  };
  take('{');
  while (true) {
    space();
    if (at < raw.size() && raw[at] == '}') { ++at; break; }
    const auto key = string_token(quoted());
    take(':'); space();
    const auto begin = at;
    int depth = 0; bool in_string = false, escaped = false;
    while (at < raw.size()) {
      const char c = raw[at];
      if (in_string) {
        if (escaped) escaped = false;
        else if (c == '\\') escaped = true;
        else if (c == '"') in_string = false;
      } else if (c == '"') in_string = true;
      else if (c == '{' || c == '[') ++depth;
      else if (c == '}' || c == ']') {
        if (depth == 0) break;
        --depth;
      } else if (c == ',' && depth == 0) break;
      ++at;
    }
    require(at > begin && !in_string && depth == 0, "malformed HTTP JSON envelope value");
    require(result.emplace(key, raw.substr(begin, at - begin)).second, "duplicate HTTP envelope key");
    require(at < raw.size(), "incomplete HTTP envelope");
    if (raw[at] == '}') { ++at; break; }
    take(',');
  }
  space(); require(at == raw.size(), "trailing HTTP envelope content");
  return result;
}
struct Envelope {
  int status;
  std::map<std::string, std::string> value;
};
Envelope received(const httplib::Result& response) {
  require(static_cast<bool>(response), "owned loopback HTTP request failed");
  last_http_envelope = response->body;
  require(response->get_header_value("Content-Type").starts_with("application/json"), "route did not return JSON content type");
  auto value = fields(response->body);
  const std::vector<std::string> names = {"family", "operation", "http_status", "phase", "source_format", "source", "result", "source_unchanged", "activity_key", "filtered_log_json", "filtered_xes", "xes_error", "error"};
  require(value.size() == names.size(), "route envelope has an unexpected field count");
  for (const auto& name : names) require(value.contains(name), "missing route envelope field: " + name);
  require(value.at("family") == "\"REFERENCE_LOG_FILTER\"", "wrong response family");
  require(value.at("http_status") == std::to_string(response->status), "HTTP/envelope status disagreement");
  return {response->status, std::move(value)};
}
httplib::MultipartFormData field(std::string name, std::string value) {
  return {std::move(name), std::move(value), "", ""};
}
httplib::MultipartFormData blob(std::string name, std::string value) {
  const auto filename = name + ".txt";
  return {std::move(name), std::move(value), filename, "application/octet-stream"};
}
httplib::MultipartFormDataItems request(std::string operation,
    const httplib::MultipartFormDataItems& controls = {},
    std::string bytes = csv, std::string filename = "source.csv") {
  httplib::MultipartFormDataItems parts = {field("operation", std::move(operation)),
      {"source_file", std::move(bytes), std::move(filename), "application/octet-stream"}};
  parts.insert(parts.end(), controls.begin(), controls.end());
  return parts;
}
std::string count_json(const EventLog& log) {
  return "{\"traces\":" + std::to_string(log.trace_count()) + ",\"events\":" + std::to_string(log.event_count()) + "}";
}
void success(const Envelope& response, std::string_view operation, const EventLog& source,
    const EventLog& expected, std::string_view format) {
  require(response.status == 200, "expected successful HTTP " + std::string(operation));
  require(response.value.at("operation") == serialize_embedding_workbench_string_json(operation), "wrong successful operation");
  require(response.value.at("phase") == "\"complete\"", "HTTP workflow did not complete");
  require(response.value.at("source_format") == serialize_embedding_workbench_string_json(format), "wrong imported source format");
  require(response.value.at("source") == count_json(source), "source HTTP counts differ");
  require(response.value.at("result") == count_json(expected), "filtered HTTP counts differ");
  require(response.value.at("source_unchanged") == "true", "HTTP filtering changed the source");
  require(response.value.at("error") == "null", "unexpected primary HTTP error");
  require(response.value.at("xes_error") == "null", "unexpected HTTP XES export failure");
  const auto json = string_token(response.value.at("filtered_log_json"));
  require(json == serialize_embedding_workbench_raw_log_json(expected), "full native artifact differs from the expected complete traces/events/context");
  require(serialize_embedding_workbench_raw_log_json(parse_embedding_workbench_raw_log_json(json)) == json,
      "actual native JSON artifact cannot be reopened losslessly");
  const auto xes = string_token(response.value.at("filtered_xes"));
  require(xes.find("<log") != std::string::npos, "route returned no actual XES log bytes");
  std::istringstream input(xes); const auto reopened = import_xes_event_log(input);
  require(reopened.trace_count() == expected.trace_count() && reopened.event_count() == expected.event_count(), "actual XES artifact cannot be reopened with the selected counts");
}
void failure(const Envelope& response, std::string_view phase, std::string_view message) {
  require(response.status == 400, "expected HTTP 400");
  require(response.value.at("phase") == serialize_embedding_workbench_string_json(phase), "wrong error phase");
  require(response.value.at("error") != "null", "missing error text");
  require(string_token(response.value.at("error")).find(message) != std::string::npos, "HTTP error text differs");
  require(response.value.at("filtered_log_json") == "null" && response.value.at("filtered_xes") == "null", "primary error exposed successful output artifacts");
}

void csv_and_xes_http_workflow(httplib::Client& client) {
  std::istringstream input(csv); const auto source = import_csv_event_log(input);
  auto selected = source; selected.traces = {source.traces.at(0)};
  const auto rework = received(client.Post(route, request("rework", {field("activity", "A")})));
  success(rework, "rework", source, selected, "csv");
  require(rework.value.at("activity_key") == "\"activity\"", "omitted CSV activity key did not use the actual CSV column");
  for (const auto& operation : {"start", "end"})
    success(received(client.Post(route, request(operation, {field("admitted_json", "[\"A\"]")}))), operation, source, selected, "csv");
  success(received(client.Post(route, request("traces", {blob("patterns_json", "\r\n[[\"A\",\"...\",\"A\"]]\r\n")}))), "traces", source, selected, "csv");
  auto prefix = selected; prefix.traces.front().events.clear();
  success(received(client.Post(route, request("prefix", {field("activity", "A")}))), "prefix", source, prefix, "csv");
  success(received(client.Post(route, request("prefix", {field("activity", "A"), field("strict", "false"), field("occurrence", "last")}))), "prefix", source, selected, "csv");
  auto suffix = selected; suffix.traces.front().events.erase(suffix.traces.front().events.begin());
  success(received(client.Post(route, request("suffix", {field("activity", "A")}))), "suffix", source, suffix, "csv");

  // Reuse the returned XES bytes as the next actual source File upload.
  const auto xes = string_token(rework.value.at("filtered_xes"));
  std::istringstream reopened_input(xes); const auto reopened = import_xes_event_log(reopened_input);
  const std::vector<std::string> labels = {"A", "B", "A"};
  for (std::size_t i = 0; i < labels.size(); ++i) {
    const auto& event = reopened.traces.at(0).events.at(i);
    require(event.activity_source_key == "activity" && event.activity == labels[i],
        "returned XES did not retain the original CSV activity labels/key: " + serialize_embedding_workbench_raw_log_json(reopened));
  }
  failure(received(client.Post(route, request("start", {field("admitted_json", "[\"A\"]")}, xes, "filtered.xes"))),
      "filter", "concept:name");
  success(received(client.Post(route, request("start", {field("admitted_json", "[\"A\"]"), field("activity_key", "activity")}, xes, "filtered.xes"))), "start", reopened, reopened, "xes");
}

void typed_native_projection_http_workflow(httplib::Client& client) {
  EventLog source;
  Trace trace; trace.case_id = "typed-case";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.attributes = {{"concept:name", trace.case_id}};
  Event event; event.activity = "A";
  event.activity_state = FirstClassAttributeState::kPresent;
  event.activity_source_key = "concept:name";
  EventAttribute tuple{"tuple-value", ""};
  tuple.container = AttributeContainerKind::kTuple;
  EventAttribute number{"1", "7", AttributeKind::kInt}; number.typed_value = std::int64_t{7};
  tuple.children = {{"0", "left"}, number};
  EventAttribute set{"set-value", ""};
  set.container = AttributeContainerKind::kSet; set.children = {{"0", "tag"}};
  event.attributes = {{"concept:name", "A"}, make_bytes_attribute("byte-value", "AB"), tuple, set};
  trace.events = {event}; source.traces = {trace};
  const auto response = received(client.Post(route, request("traces", {field("patterns_json", "[]")},
      serialize_embedding_workbench_raw_log_json(source), "typed-native.json")));
  success(response, "traces", source, source, "native_log");
  const auto retained = parse_embedding_workbench_raw_log_json(string_token(response.value.at("filtered_log_json")));
  require(retained.traces.at(0).events.at(0).attributes == event.attributes,
      "full native JSON lost the original byte/tuple/set storage");
  std::istringstream xes_input(string_token(response.value.at("filtered_xes")));
  const auto projected = import_xes_event_log(xes_input);
  const auto& projected_event = projected.traces.at(0).events.at(0);
  for (const auto& [key, text] : std::vector<std::pair<std::string, std::string>>{
      {"byte-value", "b'AB'"}, {"tuple-value", "('left', 7)"}, {"set-value", "{'tag'}"}}) {
    const EventAttribute* found = nullptr;
    for (const auto& attribute : projected_event.attributes) if (attribute.key == key) found = &attribute;
    require(found && found->kind == AttributeKind::kString && found->container == AttributeContainerKind::kScalar
        && found->children.empty() && found->value == text,
        "XES projection did not expose the expected scalar text for " + key);
  }
}

void literal_blob_http_workflow(httplib::Client& client) {
  EventLog source; source.attributes = {{"log-context", "original"}};
  source.metadata.properties = {{"source-property", "kept"}};
  Trace trace; trace.case_id = "literal-case";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.attributes = {{"concept:name", trace.case_id}, {"trace-context", "kept"}};
  trace.properties = {{"trace-property", "original"}};
  const std::string key = " work\r\nkey ", label = " A\r\nB ";
  Event event; event.attributes = {{key, label}, {"event-context", "kept"}};
  event.input_order = 7;
  trace.events = {event, event}; source.traces = {trace};
  const auto bytes = serialize_embedding_workbench_raw_log_json(source);
  success(received(client.Post(route, request("rework", {blob("activity_key", key), blob("activity", label), field("min_occurrences", "2")}, bytes, "literal.json"))), "rework", source, source, "native_log");

  // An explicitly present empty label remains an active target over HTTP.
  auto empty = source;
  for (auto& item : empty.traces.front().events) {
    item.attributes = {{"concept:name", ""}};
    item.activity_state = FirstClassAttributeState::kPresent;
    item.activity_source_key = "concept:name";
  }
  success(received(client.Post(route, request("rework", {field("activity", "")}, serialize_embedding_workbench_raw_log_json(empty), "empty.json"))), "rework", empty, empty, "native_log");
}

void file_presence_and_error_http_workflow(httplib::Client& client) {
  auto parts = request("rework", {field("activity", "A")});
  parts[1].filename.clear(); // same bytes, but no Content-Disposition filename
  auto response = received(client.Post(route, parts));
  failure(response, "control", "source_file requires an uploaded log File");
  require(response.value.at("source") == "null" && response.value.at("source_unchanged") == "null", "non-File input entered source import");

  // The public client serializer omits empty filenames, so write a real
  // filename="" multipart header to test the server's presence distinction.
  const std::string boundary = "minera-owned-reference-filter-boundary";
  std::string body;
  for (const auto& [name, bytes, file] : std::vector<std::tuple<std::string, std::string, bool>>{
      {"operation", "rework", false}, {"source_file", csv, true}, {"source_format", "csv", false}, {"activity", "A", false}}) {
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"";
    if (file) body += "; filename=\"\"";
    body += "\r\n\r\n" + bytes + "\r\n";
  }
  body += "--" + boundary + "--\r\n";
  std::istringstream source_input(csv); const auto source = import_csv_event_log(source_input);
  auto expected = source; expected.traces = {source.traces.at(0)};
  success(received(client.Post(route, body, "multipart/form-data; boundary=" + boundary)), "rework", source, expected, "csv");

  failure(received(client.Post(route, "plain source text", "text/plain")), "control", "multipart log File");
  failure(received(client.Post(route, request("unknown"))), "control", "supported log filter");
  failure(received(client.Post(route, request("rework"))), "control", "requires an activity");
  failure(received(client.Post(route, request("prefix", {field("activity", "A"), field("positive", "true")}))), "control", "inapplicable");
  failure(received(client.Post(std::string(route) + "?operation=end", request("start", {field("admitted_json", "[]")}))), "control", "duplicate log filter control: operation");
  failure(received(client.Post(route, request("rework", {field("activity", "A"), field("positive", "yes")}))), "filter", "positive must be true or false");
  failure(received(client.Post(route, request("prefix", {field("activity", "A"), field("occurrence", "middle")}))), "filter", "first or last");
  response = received(client.Post(route, request("traces", {field("patterns_json", "[[42]]")})));
  failure(response, "filter", "Literal controls JSON: expected an object");
  require(response.value.at("error") == "\"Literal controls JSON: expected an object\"",
      "malformed pattern did not preserve the native literal-parser diagnostic");
  response = received(client.Post(route, request("start", {field("admitted_json", "[\"A\"]"), field("activity_key", "missing")})));
  failure(response, "filter", "missing");
  require(response.value.at("source_unchanged") == "true", "failed native filter did not preserve source state");
  require(response.value.at("source") == count_json(source), "failed filter lost source counts");
}
} // namespace

int main() {
  try {
    OwnedServer server;
    httplib::Client client("127.0.0.1", server.port());
    client.set_connection_timeout(3, 0);
    client.set_read_timeout(3, 0);
    client.set_write_timeout(3, 0);
    csv_and_xes_http_workflow(client);
    typed_native_projection_http_workflow(client);
    literal_blob_http_workflow(client);
    file_presence_and_error_http_workflow(client);
    server.stop();
    require(server.listened_successfully(), "owned HTTP server listen loop failed");
    std::cout << "reference-log-filter actual ephemeral HTTP/multipart workflows passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "reference-log-filter HTTP acceptance failed: " << error.what()
              << "\nActual latest HTTP envelope:\n" << last_http_envelope << '\n';
    return 1;
  }
}
