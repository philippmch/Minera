#undef NDEBUG
// Drives a real minera_server process over loopback HTTP and checks the
// on-demand traces contract:
//  - /api/mine without include_traces (and with include_traces=true) keeps
//    the "traces" array inline, as before;
//  - include_traces=false leaves it out, puts a "trace_summary" in its place
//    and changes nothing else in the analysis, on the classic and on the
//    native (active-source) mining path;
//  - GET /api/traces?source_id= serves the inline "traces" bytes exactly,
//    gzipped or not, and answers 409 for a source that is not the active one
//    (or when nothing was mined) and 400 for a malformed request.
//
// Usage: minera_server_traces_route_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"

extern char** environ;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    ++g_failures;
    std::cerr << "FAIL: " << what << '\n';
  }
}

std::string describe(const httplib::Result& result) {
  if (!result) return "no response";
  return std::to_string(result->status) + " " + result->body.substr(0, 300);
}

int free_port() {
  const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (::bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) std::abort();
  socklen_t length = sizeof address;
  ::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length);
  const int port = ntohs(address.sin_port);
  ::close(socket_fd);
  return port;
}

// The first "source_id" string value of a body, or "" when there is none.
std::string source_id_of(const std::string& body) {
  const std::string key = "\"source_id\":\"";
  const auto at = body.find(key);
  if (at == std::string::npos) return "";
  const auto end = body.find('"', at + key.size());
  return end == std::string::npos ? "" : body.substr(at + key.size(), end - at - key.size());
}

// The analysis writes its top-level keys one per line ("  \"key\": value,\n").
const std::string kTracesLine = "  \"traces\": ";
const std::string kSummaryLine = "  \"trace_summary\": ";
const std::string kNextLine = ",\n  \"activities\": ";

// The inline "traces" value of an analysis, from "[" to "]".
std::string inline_traces(const std::string& body) {
  const auto at = body.find(kTracesLine);
  if (at == std::string::npos) return "";
  const auto begin = at + kTracesLine.size();
  const auto end = body.find(kNextLine, begin);
  return end == std::string::npos ? "" : body.substr(begin, end - begin);
}

// The "traces" value of a GET /api/traces body (its last member).
std::string served_traces(const std::string& body) {
  const std::string key = ",\"traces\":";
  const auto at = body.find(key);
  if (at == std::string::npos || body.empty() || body.back() != '}') return "";
  return body.substr(at + key.size(), body.size() - 1 - at - key.size());
}

// The analysis with its traces line (or summary line) cut out.
std::string without_line(const std::string& body, const std::string& line) {
  const auto at = body.find(line);
  if (at == std::string::npos) return body;
  const auto end = body.find(kNextLine, at);
  if (end == std::string::npos) return body;
  return body.substr(0, at) + body.substr(end + 2);  // keeps "  \"activities\""
}

// The analysis part of a classic mining body: everything before the bound
// tail (",\"source_id\":..."), which names the session.
std::string analysis_part(const std::string& body) {
  const auto at = body.find(",\"source_id\":\"");
  return at == std::string::npos ? body : body.substr(0, at);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <minera_server>\n";
    return 2;
  }
  // Two cases with lifecycle transitions, a resource and a non-ASCII label;
  // c3 has no timestamps at all.
  const std::string log =
      "case_id,activity,timestamp,resource,lifecycle\n"
      "c1,Register,2024-01-01T08:00:00Z,Ann,start\n"
      "c1,Register,2024-01-01T08:05:00Z,Ann,complete\n"
      "c1,Prüfen,2024-01-01T09:00:00Z,Bo,complete\n"
      "c2,Register,2024-01-02T08:00:00Z,Ann,start\n"
      "c2,Register,2024-01-02T08:10:00Z,Bo,complete\n"
      "c2,Prüfen,2024-01-02T10:00:00Z,Bo,complete\n"
      "c2,Archive,2024-01-02T11:00:00Z,Cy,complete\n";

  const int port = free_port();
  const std::string port_text = std::to_string(port);
  std::vector<std::string> args = {argv[1], "--port", port_text, "--frontend",
                                   "/nonexistent-minera-frontend"};
  std::vector<char*> raw;
  for (auto& arg : args) raw.push_back(arg.data());
  raw.push_back(nullptr);
  pid_t server = 0;
  if (::posix_spawn(&server, argv[1], nullptr, nullptr, raw.data(), environ) != 0) {
    std::cerr << "cannot start " << argv[1] << '\n';
    return 2;
  }

  httplib::Client client("127.0.0.1", port);
  client.set_read_timeout(60, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200) ready = true;
    else std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  check(ready, "server answers /api/version");

  if (ready) {
    const auto mine = [&](std::vector<std::pair<std::string, std::string>> fields) {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      for (auto& [name, value] : fields) items.push_back({name, value, "", ""});
      return client.Post("/api/mine", items);
    };
    const auto traces = [&](const std::string& query, bool gzip = false) {
      httplib::Headers headers;
      if (gzip) headers.emplace("Accept-Encoding", "gzip");
      return client.Get("/api/traces" + query, headers);
    };

    // Nothing mined yet: the route has no source to serve.
    auto r = traces("?source_id=1");
    check(r && r->status == 409 && r->body.find("mine a log first") != std::string::npos,
          "traces before any mine answer 409: " + describe(r));

    // The default keeps the traces inline (scripts and tests rely on it).
    const auto full = mine({});
    check(full && full->status == 200, "default mine: " + describe(full));
    const std::string full_body = full ? full->body : "";
    const auto full_traces = inline_traces(full_body);
    check(!full_traces.empty() && full_traces.front() == '[' && full_traces.back() == ']',
          "the default mine carries the traces inline");
    check(full_body.find(kSummaryLine) == std::string::npos, "the default mine has no trace summary");
    const auto full_id = source_id_of(full_body);
    check(!full_id.empty(), "the default mine names its source");

    // include_traces=true is the same as the default.
    const auto explicit_full = mine({{"include_traces", "true"}});
    check(explicit_full && explicit_full->status == 200 &&
              analysis_part(explicit_full->body) == analysis_part(full_body),
          "include_traces=true answers the default analysis: " + describe(explicit_full));

    // The source just mined serves the same bytes on demand, plain or gzipped.
    const auto explicit_id = explicit_full ? source_id_of(explicit_full->body) : "";
#if defined(CPPHTTPLIB_ZLIB_SUPPORT)
    const std::vector<bool> encodings = {false, true};
#else
    const std::vector<bool> encodings = {false};  // this client cannot inflate
#endif
    for (const bool gzip : encodings) {
      r = traces("?source_id=" + explicit_id, gzip);
      check(r && r->status == 200, std::string("traces of the active source") + (gzip ? " (gzip)" : "") + ": " + describe(r));
      if (r && r->status == 200) {
        check(r->get_header_value("Content-Type").starts_with("application/json"), "traces are JSON");
        check(r->body.starts_with("{\"family\":\"ANALYSIS_TRACES\",\"source_id\":\"" + explicit_id +
                                  "\",\"trace_count\":2,\"event_count\":7,\"traces\":"),
              "traces name their family, source and counts: " + r->body.substr(0, 160));
        check(served_traces(r->body) == full_traces,
              std::string("GET /api/traces is byte-identical to the inline traces") + (gzip ? " (gzip)" : ""));
      }
    }
    // The client decompresses transparently; ask for the raw gzip bytes too.
    {
      httplib::Client raw_client("127.0.0.1", port);
      raw_client.set_decompress(false);
      auto gz = raw_client.Get("/api/traces?source_id=" + explicit_id, {{"Accept-Encoding", "gzip"}});
      check(gz && gz->status == 200 && gz->get_header_value("Content-Encoding") == "gzip" &&
                gz->body.size() > 2 && static_cast<unsigned char>(gz->body[0]) == 0x1f &&
                static_cast<unsigned char>(gz->body[1]) == 0x8b,
            "a client that accepts gzip gets gzipped traces: " + describe(gz));
    }

    // include_traces=false: the summary takes the traces' place, and nothing
    // else in the analysis changes.
    const auto lazy = mine({{"include_traces", "false"}});
    check(lazy && lazy->status == 200, "include_traces=false mine: " + describe(lazy));
    const std::string lazy_body = lazy ? lazy->body : "";
    check(lazy_body.find(kTracesLine) == std::string::npos, "include_traces=false leaves the traces out");
    check(lazy_body.find("  \"trace_summary\": {\"traces\": \"on_demand\", \"trace_count\": 2, \"event_count\": 7, "
                         "\"max_trace_length\": 4, \"timestamped_trace_count\": 2, \"has_timestamps\": true, "
                         "\"has_lifecycles\": true, \"lifecycles\": [\"start\", \"complete\"]},\n") != std::string::npos,
          "include_traces=false writes the trace summary: " + lazy_body.substr(0, 400));
    check(analysis_part(without_line(lazy_body, kSummaryLine)) ==
              analysis_part(without_line(full_body, kTracesLine)),
          "the summary-only analysis equals the complete one without its traces");
    check(lazy_body.size() < full_body.size(), "the summary-only body is smaller");
    const auto lazy_id = source_id_of(lazy_body);
    r = traces("?source_id=" + lazy_id);
    check(r && r->status == 200 && served_traces(r->body) == full_traces,
          "the summary-only source serves the inline traces byte for byte: " + describe(r));

    // Another source than the active one: 409, like the other session routes.
    r = traces("?source_id=" + full_id);
    check(r && r->status == 409 && r->body.find("source log changed") != std::string::npos,
          "traces of a replaced source answer 409: " + describe(r));
    r = traces("?source_id=not-a-source");
    check(r && r->status == 409, "traces of an unknown source answer 409: " + describe(r));
    // Malformed requests: 400.
    r = traces("");
    check(r && r->status == 400 && r->body.find("source_id is required") != std::string::npos,
          "traces without a source answer 400: " + describe(r));
    r = traces("?source_id=" + lazy_id + "&limit=1");
    check(r && r->status == 400 && r->body.find("unknown traces parameter: limit") != std::string::npos,
          "traces reject unknown parameters: " + describe(r));
    r = mine({{"include_traces", "no"}});
    check(r && r->status == 400 && r->body.find("include_traces expects true or false") != std::string::npos,
          "mine rejects an invalid include_traces: " + describe(r));
    // The rejected mine left the active source alone.
    r = traces("?source_id=" + lazy_id);
    check(r && r->status == 200, "a rejected mine keeps the active source's traces: " + describe(r));

    // The native (active-source) mining path nests the analysis; the same
    // switch applies to it.
    const auto native_full = mine({{"mining_input_shape", "stream"}});
    check(native_full && native_full->status == 200 &&
              native_full->body.find("\"family\":\"ACTIVE_SOURCE_MINING\"") != std::string::npos,
          "native mine: " + describe(native_full));
    const auto native_traces = native_full ? inline_traces(native_full->body) : "";
    check(!native_traces.empty(), "the native mine carries the traces inline by default");
    const auto native_lazy = mine({{"mining_input_shape", "stream"}, {"include_traces", "false"}});
    check(native_lazy && native_lazy->status == 200, "native include_traces=false mine: " + describe(native_lazy));
    if (native_lazy && native_lazy->status == 200) {
      check(native_lazy->body.find(kTracesLine) == std::string::npos &&
                native_lazy->body.find(kSummaryLine) != std::string::npos,
            "the native summary-only analysis has a trace summary instead of traces");
      r = traces("?source_id=" + source_id_of(native_lazy->body));
      check(r && r->status == 200 && served_traces(r->body) == native_traces,
            "the native source serves its inline traces byte for byte: " + describe(r));
    }
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " traces route check(s) failed\n";
    return 1;
  }
  std::cout << "traces route checks passed\n";
  return 0;
}
