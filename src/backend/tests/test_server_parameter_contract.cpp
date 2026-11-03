#undef NDEBUG
// Drives a real minera_server process over loopback HTTP and checks the
// parameter contract of the /api routes: strict values, repeated controls,
// unknown /api/mine fields, JSON 404/405 bodies, valid UTF-8 error JSON,
// defaults (miner, playout traces), live-session release and the mining
// rollback window (cancel after the response was written restores the
// previous source until an ack, a request on the new source or the window's
// end releases it).
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

// Well-formed UTF-8 (JSON whitespace allowed), checked by Python's strict
// decoder rules: no overlong forms, surrogates or code points above U+10FFFF.
bool valid_utf8(const std::string& text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i]);
    if (lead < 0x80) { ++i; continue; }
    std::size_t length = lead >= 0xc2 && lead <= 0xdf ? 2 : lead >= 0xe0 && lead <= 0xef ? 3
                       : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
    if (length == 0 || i + length > text.size()) return false;
    for (std::size_t k = 1; k < length; ++k)
      if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
    const auto second = static_cast<unsigned char>(text[i + 1]);
    if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second > 0x9f) ||
        (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second > 0x8f))
      return false;
    i += length;
  }
  return true;
}

bool contains(const httplib::Result& result, const std::string& needle) {
  return result && result->body.find(needle) != std::string::npos;
}

// The first "source_id" string value of a body, or "" when there is none.
std::string source_id_of(const httplib::Result& result) {
  if (!result) return "";
  const std::string key = "\"source_id\":\"";
  const auto at = result->body.find(key);
  if (at == std::string::npos) return "";
  const auto end = result->body.find('"', at + key.size());
  return end == std::string::npos ? "" : result->body.substr(at + key.size(), end - at - key.size());
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

  // A short rollback window keeps the expiry check fast.
  constexpr int rollback_window_ms = 1500;
  ::setenv("MINERA_MINING_ROLLBACK_MS", std::to_string(rollback_window_ms).c_str(), 1);
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

    // Mining rollback: a written response keeps the previous source for a
    // cancel until it is confirmed or the window ends.
    {
      const auto mine_as = [&](const std::string& id) {return mine("/api/mine", {{"mining_request_id", id}});};
      const auto cancel = [&](const std::string& id) {
        return client.Post("/api/cancel-mine?request_id=" + id, "", "application/x-www-form-urlencoded");
      };
      const auto held_after = [&](const httplib::Result& canceled) {
        if (!canceled) return std::string("no response");
        const std::string key = "\"active_source_id\":\"";
        const auto at = canceled->body.find(key);
        if (at == std::string::npos) return canceled->body;
        return canceled->body.substr(at + key.size(), canceled->body.find('"', at + key.size()) - at - key.size());
      };
      const std::string a = "00000000-0000-4000-8000-00000000000a", b = "00000000-0000-4000-8000-00000000000b",
                        c = "00000000-0000-4000-8000-00000000000c", d = "00000000-0000-4000-8000-00000000000d",
                        e = "00000000-0000-4000-8000-00000000000e";
      result = mine_as(a);
      const auto source_a = source_id_of(result);
      check(result && result->status == 200 && !source_a.empty(), "rollback base mine: " + describe(result));
      result = client.Post("/api/mine-ack?request_id=" + a, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "\"acknowledged\":true"), "mine-ack: " + describe(result));
      result = client.Post("/api/mine-ack?request_id=" + a + "&x=1", "", "application/x-www-form-urlencoded");
      check(result && result->status == 400, "mine-ack takes only the request id: " + describe(result));

      // The client read the whole response but never confirmed it.
      result = mine_as(b);
      const auto source_b = source_id_of(result);
      check(result && result->status == 200 && !source_b.empty() && source_b != source_a, "rollback mine: " + describe(result));
      auto canceled = cancel(b);
      check(held_after(canceled) == source_a,
            "cancel after the response was written restores the previous source: " + describe(canceled));
      result = client.Get("/api/active-source?source_id=" + source_a);
      check(result && result->status == 200, "the restored source answers: " + describe(result));

      // An explicit ack releases the rollback.
      result = mine_as(c);
      const auto source_c = source_id_of(result);
      result = client.Post("/api/mine-ack?request_id=" + c, "", "application/x-www-form-urlencoded");
      check(result && contains(result, "\"acknowledged\":true"), "ack of a written response: " + describe(result));
      canceled = cancel(c);
      check(!source_c.empty() && held_after(canceled) == source_c, "cancel after an ack keeps the new source: " + describe(canceled));

      // A request bound to the new source id releases it too.
      result = mine_as(d);
      const auto source_d = source_id_of(result);
      result = client.Get("/api/active-source?source_id=" + source_d);
      check(result && result->status == 200, "the new source answers: " + describe(result));
      canceled = cancel(d);
      check(!source_d.empty() && held_after(canceled) == source_d,
            "cancel after a request on the new source keeps it: " + describe(canceled));

      // The window's end releases an unconfirmed rollback.
      result = mine_as(e);
      const auto source_e = source_id_of(result);
      std::this_thread::sleep_for(std::chrono::milliseconds(rollback_window_ms + 1000));
      canceled = cancel(e);
      check(!source_e.empty() && held_after(canceled) == source_e,
            "cancel after the rollback window keeps the new source: " + describe(canceled));
      result = client.Post("/api/mine-ack?request_id=" + e, "", "application/x-www-form-urlencoded");
      check(result && contains(result, "\"acknowledged\":false"), "a canceled request cannot be acknowledged: " + describe(result));
    }

    // The reader snapshot still answers after the lock change.
    {
      httplib::MultipartFormDataItems items = {{"log", log, "log.csv", "text/csv"}};
      result = client.Post("/api/stream-reader/create?session_id=r1&kind=csv_events", items);
      check(result && result->status == 200, "reader create: " + describe(result));
      result = client.Get("/api/stream-reader/snapshot?session_id=r1");
      check(result && result->status == 200 && contains(result, "STREAM_READER_SESSION"),
            "reader snapshot: " + describe(result));
    }

    // Logs whose text XES cannot carry (a C0 control, Latin-1 bytes): Petri
    // playout answers 400 JSON, and routes that embed XES keep their result
    // and report the refusal in the matching *_error field.
    for (const std::string activity : {std::string("A\x01x"), std::string("Pr\xfc" "fung")}) {
      const std::string bad_log =
          "case_id,activity,timestamp\n"
          "c1," + activity + ",2024-01-01T00:00:00Z\nc1,B,2024-01-01T00:01:00Z\n"
          "c2," + activity + ",2024-01-01T00:00:00Z\nc2,B,2024-01-01T00:01:00Z\n";
      const auto label = activity[1] == '\x01' ? std::string("control char") : std::string("Latin-1");
      httplib::MultipartFormDataItems items = {{"log", bad_log, "log.csv", "text/csv"}};
      result = client.Post("/api/mine", items);
      check(result && result->status == 200 && valid_utf8(result->body),
            label + " mine answers valid UTF-8 JSON: " + describe(result));
      const auto source = source_id_of(result);
      check(!source.empty(), label + " mine returns a source id: " + describe(result));
      const auto refused = "XES cannot represent";
      // Every JSON body is valid UTF-8: the Latin-1 byte reaches the result
      // JSON as U+FFFD (one per maximal ill-formed subpart).
      const bool latin1 = label == "Latin-1";
      const auto valid_body = [&](const httplib::Result& r) {
        return valid_utf8(r->body) &&
               (!latin1 || r->body.find("Pr\\ufffdfung") != std::string::npos);
      };

      result = client.Get("/api/playout?miner=inductive&traces=3");
      check(result && result->status == 400 && contains(result, "\"error\"") &&
                contains(result, refused) && utf8_without_controls(result->body),
            label + " Petri playout gives 400 JSON: " + describe(result));

      result = client.Post("/api/log-util?variant=split_train_test&include_source=true&source_id=" +
                               source, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "\"xes\":null") &&
                contains(result, std::string("\"xes_error\":\"") + refused) &&
                valid_body(result),
            label + " preparation keeps its result and reports xes_error: " + describe(result));

      result = client.Post("/api/label-splitting?include_source=true&source_id=" + source, "",
                           "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "LOG_TRANSFORMATION") &&
                contains(result, "\"transformed_xes\":null") &&
                contains(result, std::string("\"transformed_xes_error\":\"") + refused) &&
                valid_body(result),
            label + " transformation keeps its result and reports transformed_xes_error: " +
                describe(result));

      result = client.Post("/api/anonymize-trace-variants?include_xes=true&tvq_epsilon=100&tvq_k=1&tvq_seed=1&source_id=" +
                               source, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "ANONYMIZATION_RELEASE") &&
                contains(result, "\"released_xes\":null") &&
                contains(result, std::string("\"released_xes_error\":\"") + refused) &&
                valid_body(result),
            label + " anonymization keeps its release and reports released_xes_error: " +
                describe(result));

      result = client.Post("/api/monte-carlo?miner=inductive&include_workspace=true&source_id=" +
                               source, "", "application/x-www-form-urlencoded");
      check(result && result->status == 200 && contains(result, "SIMULATION_WORKSPACE") &&
                contains(result, "\"xes\":null") &&
                contains(result, std::string("\"xes_error\":\"") + refused) &&
                valid_body(result),
            label + " simulation workspace keeps its result and reports xes_error: " +
                describe(result));
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
