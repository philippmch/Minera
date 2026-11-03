#undef NDEBUG
// Drives a real minera_server process over loopback HTTP and checks the
// parameter contract of the /api routes: strict values, repeated controls,
// unknown /api/mine fields, JSON 404/405 bodies, valid UTF-8 error JSON,
// defaults (miner, playout traces) and live-session release.
//
// Usage: minera_server_parameter_contract_tests <minera_server> <log.csv>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
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
  if (::bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0)
    std::abort();
  socklen_t length = sizeof address;
  ::getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length);
  const int port = ntohs(address.sin_port);
  ::close(socket_fd);
  return port;
}

// A body is valid JSON text only when it is UTF-8 without raw control bytes.
bool utf8_without_controls(const std::string& text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto byte = static_cast<unsigned char>(text[i]);
    if (byte < 0x20) return false;
    std::size_t length = byte < 0x80 ? 1 : (byte >> 5) == 6 ? 2 : (byte >> 4) == 14 ? 3
                       : (byte >> 3) == 30 ? 4 : 0;
    if (length == 0 || i + length > text.size()) return false;
    for (std::size_t k = 1; k < length; ++k)
      if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
    i += length;
  }
  return true;
}

bool contains(const httplib::Result& result, const std::string& needle) {
  return result && result->body.find(needle) != std::string::npos;
}

std::size_t count(const std::string& text, const std::string& needle) {
  std::size_t total = 0;
  for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1))
    ++total;
  return total;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <minera_server> <log.csv>\n";
    return 2;
  }
  std::ifstream log_file(argv[2], std::ios::binary);
  std::stringstream log_stream;
  log_stream << log_file.rdbuf();
  const std::string log = log_stream.str();
  if (log.empty()) {
    std::cerr << "cannot read " << argv[2] << '\n';
    return 2;
  }

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
    if (auto result = client.Get("/api/version"); result && result->status == 200)
      ready = true;
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  check(ready, "server answers /api/version");

  if (ready) {
    const auto mine = [&](const std::string& path,
                          std::vector<std::pair<std::string, std::string>> fields) {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      for (auto& [name, value] : fields) items.push_back({name, value, "", ""});
      return client.Post(path, items);
    };

    // Item 8: unknown route and wrong method carry JSON bodies.
    auto result = client.Get("/api/nonexistent");
    check(result && result->status == 404 &&
              contains(result, "\"error\"") &&
              contains(result, "unknown API route: /api/nonexistent"),
          "unknown /api route gives a JSON 404: " + describe(result));
    result = client.Post("/api/version", "", "text/plain");
    check(result && result->status == 405 &&
              result->get_header_value("Allow") == "GET" &&
              contains(result, "\"error\""),
          "wrong method gives a JSON 405 with Allow: " + describe(result));

    // A plain mine still succeeds.
    result = mine("/api/mine", {});
    check(result && result->status == 200, "plain mine: " + describe(result));

    // Item 6: multipart /api/mine rejects query parameters and unknown fields.
    result = mine("/api/mine?dependency_threshold=abc", {});
    check(result && result->status == 400 && contains(result, "query-string parameter"),
          "multipart mine rejects query parameters: " + describe(result));
    result = mine("/api/mine", {{"dependency_treshold", "0.99"}});
    check(result && result->status == 400 &&
              contains(result, "unknown or inapplicable /api/mine form field: dependency_treshold"),
          "multipart mine rejects a misspelled field: " + describe(result));
    result = mine("/api/mine", {{"dependency_threshold", "0.9"}});
    check(result && result->status == 200,
          "multipart mine accepts a known field: " + describe(result));

    // Items 2 and 5: strict values and repeated controls.
    for (const auto* bad : {"1junk", "abc", "-1", "", " 1", "0x5"}) {
      result = mine("/api/mine", {{"top_variants", bad}});
      check(result && result->status == 400,
            std::string("top_variants='") + bad + "' is rejected: " + describe(result));
    }
    result = mine("/api/mine", {{"top_variants", "1"}, {"top_variants", "3"}});
    check(result && result->status == 400 && contains(result, "more than once"),
          "a repeated mine field is rejected: " + describe(result));
    result = mine("/api/mine", {{"rework_activity", "A"}, {"rework_min_occurrences", "-5"}});
    check(result && result->status == 400,
          "negative rework_min_occurrences is rejected: " + describe(result));
    result = mine("/api/mine", {{"delimiter", ";x"}});
    check(result && result->status == 400,
          "a multi-character delimiter is rejected: " + describe(result));

    // Item 1: error bodies stay valid UTF-8 JSON whatever the value holds.
    result = mine("/api/mine", {{"path_predecessor", "A"}, {"path_successor", "B"},
                                {"path_min_performance", "1\nx\xff"}});
    check(result && result->status == 400 && utf8_without_controls(result->body) &&
              contains(result, "\\n") && contains(result, "\\ufffd"),
          "error JSON escapes controls and invalid bytes: " + describe(result));

    result = mine("/api/mine", {});
    check(result && result->status == 200, "re-mine: " + describe(result));
    result = client.Get("/api/dfg?noise_threshold=0.1&noise_threshold=abc");
    check(result && result->status == 400 && contains(result, "more than once"),
          "a repeated query parameter is rejected: " + describe(result));

    // Plausible item: model export and alignment default to inductive.
    result = client.Get("/api/pnml");
    check(result && result->status == 200 && contains(result, "<pnml"),
          "/api/pnml defaults to the inductive net: " + describe(result));
    result = client.Post("/api/align", "", "application/x-www-form-urlencoded");
    check(result && result->status == 200,
          "/api/align defaults to the inductive net: " + describe(result));
    result = client.Get("/api/pnml?miner=");
    check(result && result->status == 400,
          "an explicit empty miner is still rejected: " + describe(result));

    // Imported-model binding: each check-model answer carries a fresh id, and
    // a run that echoes a superseded id is refused instead of answering for
    // the newer model.
    {
      const auto pnml = client.Get("/api/pnml?miner=inductive");
      check(pnml && pnml->status == 200, "inductive PNML for import: " + describe(pnml));
      const auto import_model = [&]() -> std::string {
        httplib::MultipartFormDataItems items = {
            {"model", pnml ? pnml->body : std::string{}, "model.pnml", "application/xml"}};
        const auto checked = client.Post("/api/check-model", items);
        const std::string key = "\"imported_model_id\":";
        const auto at = checked ? checked->body.find(key) : std::string::npos;
        check(checked && checked->status == 200 && at == 1,
              "check-model reports its imported model id: " + describe(checked));
        if (at == std::string::npos) return {};
        const auto begin = at + key.size();
        return checked->body.substr(begin, checked->body.find(',', begin) - begin);
      };
      const auto first = import_model(), second = import_model();
      check(!first.empty() && !second.empty() && first != second,
            "each import gets a fresh id: " + first + " vs " + second);
      result = client.Post("/api/align?miner=imported", {{"X-Minera-Imported-Model", second}},
                           "", "application/x-www-form-urlencoded");
      check(result && result->status == 200,
            "a run bound to the current imported model runs: " + describe(result));
      result = client.Post("/api/align?miner=imported", {{"X-Minera-Imported-Model", first}},
                           "", "application/x-www-form-urlencoded");
      check(result && result->status == 409 && contains(result, "check the model again"),
            "a run bound to a replaced imported model is refused: " + describe(result));
      result = client.Get("/api/playout?miner=imported&traces=5",
                          {{"X-Minera-Imported-Model", first}});
      check(result && result->status == 409,
            "a stale bound playout is refused: " + describe(result));
      result = client.Post("/api/align?miner=imported", "", "application/x-www-form-urlencoded");
      check(result && result->status == 200,
            "an unbound run keeps working: " + describe(result));
    }

    // Item 4: basic Petri playout generates pm4py's 1000 traces by default.
    result = client.Get("/api/playout?miner=inductive");
    check(result && result->status == 200 && count(result->body, "<trace>") == 1000,
          "default playout has 1000 traces: " +
              (result ? std::to_string(count(result->body, "<trace>")) : describe(result)));
    // An omitted trace count never fails the 1,000,000-event budget: with a
    // long per-trace cap it shrinks to budget / cap (declared), while an
    // explicit count is still checked as given.
    result = client.Get("/api/playout?miner=inductive&max_steps=5000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 200,
          "long max_steps scales the default trace count: " +
              (result && result->status == 200 ? std::to_string(count(result->body, "<trace>"))
                                               : describe(result)));
    result = client.Get("/api/playout?miner=inductive&max_steps=10000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 100,
          "the longest max_steps still plays out: " +
              (result && result->status == 200 ? std::to_string(count(result->body, "<trace>"))
                                               : describe(result)));
    result = client.Get("/api/playout?miner=inductive&max_steps=1000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 1000,
          "max_steps 1000 keeps 1000 default traces: " + describe(result));
    result = client.Get("/api/playout?miner=inductive&max_steps=5000&traces=1000");
    check(result && result->status == 400 && contains(result, "event budget"),
          "an explicit over-budget trace count is rejected: " + describe(result));
    result = client.Get("/api/playout?miner=stochastic&stochastic_max_steps=5000");
    check(result && result->status == 200 && count(result->body, "<trace>") == 200,
          "stochastic playout scales its default trace count: " +
              (result && result->status == 200 ? std::to_string(count(result->body, "<trace>"))
                                               : describe(result)));
    result = client.Get("/api/playout?miner=stochastic&stochastic_max_steps=5000&stochastic_traces=1000");
    check(result && result->status == 400 && contains(result, "event budget"),
          "an explicit over-budget stochastic count is rejected: " + describe(result));
    result = client.Get("/api/playout?miner=dfg&dfg_playout_variant=performance&dfg_playout_max_steps=5000");
    check(result && result->status == 200,
          "DFG performance playout scales its default trace count: " + describe(result));
    result = client.Get("/api/playout?miner=dfg&dfg_playout_variant=performance&dfg_playout_max_steps=5000&dfg_playout_traces=1000");
    check(result && result->status == 400 && contains(result, "event budget"),
          "an explicit over-budget DFG count is rejected: " + describe(result));
    // Item 9: an empty number is not zero.
    result = client.Get("/api/playout?miner=inductive&start=");
    check(result && result->status == 400, "empty start is rejected: " + describe(result));
    result = client.Get("/api/playout?miner=inductive&case_interval=");
    check(result && result->status == 400,
          "empty case_interval is rejected: " + describe(result));

    // Item 7 and the release fix on live sessions.
    result = client.Post("/api/live-stream/create?method=dfg&session_id=c1&streaming_include_diagnostics=maybe",
                         "", "application/x-www-form-urlencoded");
    check(result && result->status == 400,
          "a non-boolean streaming_include_diagnostics is rejected: " + describe(result));
    result = client.Post("/api/live-stream/create?method=dfg&session_id=c1&source_name=x.pnml",
                         "", "application/x-www-form-urlencoded");
    check(result && result->status == 400 && contains(result, "source_name"),
          "source_name without a model is rejected: " + describe(result));
    result = client.Post("/api/live-stream/create?method=dfg&session_id=c1&streaming_include_diagnostics=true",
                         "", "application/x-www-form-urlencoded");
    check(result && result->status == 200, "live create: " + describe(result));
    {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      result = client.Post("/api/live-stream/append?session_id=c1&expected_revision=0&batch_id=b1&format=csv",
                           items);
      check(result && result->status == 200 && contains(result, "\"state\":\"INACTIVE\""),
            "queued append on an unstarted session: " + describe(result));
    }
    result = client.Post("/api/live-stream/release?session_id=c1&expected_revision=1", "",
                         "application/x-www-form-urlencoded");
    check(result && result->status == 200 && contains(result, "\"released\":true"),
          "an unstarted session with queued events can be released: " + describe(result));
    result = client.Get("/api/live-stream/snapshot?session_id=c1");
    check(result && result->status == 404, "the released session is gone: " + describe(result));

    // The reader snapshot still answers after the lock change.
    {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      result = client.Post("/api/stream-reader/create?session_id=r1&kind=csv_events", items);
      check(result && result->status == 200, "reader create: " + describe(result));
      result = client.Get("/api/stream-reader/snapshot?session_id=r1");
      check(result && result->status == 200 && contains(result, "STREAM_READER_SESSION"),
            "reader snapshot: " + describe(result));
    }
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures != 0) {
    std::cerr << g_failures << " server contract check(s) failed\n";
    return 1;
  }
  std::cout << "server parameter contract checks passed\n";
  return 0;
}
