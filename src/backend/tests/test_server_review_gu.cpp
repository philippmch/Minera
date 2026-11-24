#undef NDEBUG
// Review GU: pm4py's frame DFG over an upload's start_timestamp column.
//
// - The process map (/api/mine edges, start and end counts,
//   performance.edges) and the DFG performance variant sort each case's rows
//   by (start, timestamp), NaT last and ties stable; a pair's duration is the
//   later start minus the earlier timestamp, signed, a NaT start NaN (skipped
//   by the aggregates, null when an edge has no other). A start column of no
//   dates leaves the edges untimed with pm4py's TypeError in
//   performance.edges_error; the performance variant fails with it.
// - Passed time reads the column as its start (signed, NaN pairs counted in
//   the frequency); cycle time on a CSV upload's frame, whose attrs name it.
// - /api/dfg-dataframe keeps an edge whose flows are all NaN (a NaT start);
//   business hours fail converting a NaN flow.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gu: tc/mkgu.py, ct/*.py).
//
// Usage: minera_server_review_gu_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "httplib.h"

extern char** environ;

namespace {

int g_failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++g_failures;
  std::cerr << "FAIL: " << label << "\n  seen     " << seen << "\n  expected " << expected << '\n';
}

int free_port() {
  if (const char* pinned = std::getenv("MINERA_TEST_SERVER_PORT"); pinned && *pinned)
    return std::atoi(pinned);
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

// A small JSON reader: enough for the routes' results.
struct Json {
  enum class Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = Kind::kNull;
  std::string text;  // a string's value, a number's or boolean's literal
  std::vector<Json> items;
  std::vector<std::pair<std::string, Json>> members;
  const Json& operator[](const std::string& key) const {
    static const Json null;
    for (const auto& [name, value] : members)
      if (name == key) return value;
    return null;
  }
};

struct JsonReader {
  const std::string& s;
  std::size_t i = 0;
  void space() {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
  }
  std::string string() {
    std::string out;
    for (++i; i < s.size() && s[i] != '"'; ++i) {
      if (s[i] != '\\') {
        out += s[i];
        continue;
      }
      const char c = s[++i];
      if (c == 'n') out += '\n';
      else if (c == 't') out += '\t';
      else if (c == 'r') out += '\r';
      else if (c == 'b') out += '\b';
      else if (c == 'f') out += '\f';
      else if (c == 'u') {
        const auto code = std::stoul(s.substr(i + 1, 4), nullptr, 16);
        i += 4;
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) {
          out += static_cast<char>(0xc0 | (code >> 6));
          out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
          out += static_cast<char>(0xe0 | (code >> 12));
          out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
          out += static_cast<char>(0x80 | (code & 0x3f));
        }
      } else out += c;
    }
    ++i;
    return out;
  }
  Json value() {
    space();
    Json out;
    if (i >= s.size()) return out;
    const char c = s[i];
    if (c == '{') {
      out.kind = Json::Kind::kObject;
      ++i;
      space();
      if (s[i] == '}') { ++i; return out; }
      while (true) {
        space();
        auto key = string();
        space();
        ++i;  // ':'
        auto item = value();
        out.members.emplace_back(std::move(key), std::move(item));
        space();
        if (s[i++] == '}') return out;
      }
    }
    if (c == '[') {
      out.kind = Json::Kind::kArray;
      ++i;
      space();
      if (s[i] == ']') { ++i; return out; }
      while (true) {
        out.items.push_back(value());
        space();
        if (s[i++] == ']') return out;
      }
    }
    if (c == '"') {
      out.kind = Json::Kind::kString;
      out.text = string();
      return out;
    }
    const auto end = s.find_first_of(",]} \n\r\t", i);
    out.text = s.substr(i, end - i);
    i = end;
    out.kind = out.text == "null" ? Json::Kind::kNull
               : out.text == "true" || out.text == "false" ? Json::Kind::kBool : Json::Kind::kNumber;
    return out;
  }
};

std::string sorted_join(std::vector<std::string> parts, const std::string& by) {
  std::sort(parts.begin(), parts.end());
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string plain_join(const std::vector<std::string>& parts, const std::string& by) {
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string number(const Json& value, const char* format) {
  if (value.kind == Json::Kind::kNull) return "None";
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, format, std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

std::string name_of(const Json& value) { return value.kind == Json::Kind::kNull ? "None" : value.text; }

std::string error_text(const Json& error) {
  if (error.kind == Json::Kind::kObject) {
    const auto& type = error["type"].text;
    return (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + error["message"].text;
  }
  return error.text;
}

using Fields = std::vector<std::pair<std::string, std::string>>;

std::string g6(const Json& value) { return number(value, "%.6g"); }

// One case's outcome in mkgu.py's canonical form. Fields prefixed "p:" go to
// the second request (passed time, cycle time, dataframe DFG), the rest to
// /api/mine.
std::string pcanon(const std::vector<std::string>& edges, const std::vector<std::string>& starts,
                   const std::vector<std::string>& ends, const std::vector<std::string>& timings,
                   const Json& timing_error) {
  std::string out = "E " + sorted_join(edges, ",") + " | S " + sorted_join(starts, ",") + " | X " +
                    sorted_join(ends, ",");
  if (timing_error.kind == Json::Kind::kString && !timing_error.text.empty())
    return out + " | Perr " + timing_error.text;
  return out + " | P " + sorted_join(timings, ",");
}

std::string entries(const Json& list) {
  std::vector<std::string> out;
  for (const auto& e : list.items) out.push_back(e["activity"].text + "=" + g6(e["performance"]) + "/" + e["frequency"].text);
  return sorted_join(out, ",");
}

std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""}};
  httplib::Params second;
  for (const auto& [key, value] : fields) {
    if (key.starts_with("p:")) second.emplace(key.substr(2), value);
    else items.push_back({key, value, "", ""});
  }
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  if (body["error"].kind != Json::Kind::kNull) return "error: " + error_text(body["error"]);
  std::vector<std::string> names;
  for (const auto& a : body["activities"].items) names.push_back(a["name"].text);
  const auto named = [&](const Json& id) { return names[std::stoul(id.text)]; };
  if (kind == "pm") {
    std::vector<std::string> edges, starts, ends, timings;
    for (const auto& e : body["edges"].items) edges.push_back(named(e["source"]) + ">" + named(e["target"]) + "=" + e["count"].text);
    for (const auto& a : body["activities"].items) {
      if (a["start_count"].text != "0") starts.push_back(a["name"].text + "=" + a["start_count"].text);
      if (a["end_count"].text != "0") ends.push_back(a["name"].text + "=" + a["end_count"].text);
    }
    const auto& performance = body["performance"];
    for (const auto& e : performance["edges"].items)
      timings.push_back(named(e["source"]) + ">" + named(e["target"]) + "=" + g6(e["stats"]["mean"]) + "/" +
                        g6(e["stats"]["max"]));
    return pcanon(edges, starts, ends, timings, performance["edges_error"]);
  }
  if (kind == "pv") {
    std::vector<std::string> timings;
    for (const auto& e : body["dfg_variants"]["statistical"].items[0]["performance_dfg"].items)
      timings.push_back(named(e["source"]) + ">" + named(e["target"]) + "=" + g6(e["summary"]["mean"]) + "/" +
                        g6(e["summary"]["sum"]));
    return "V " + sorted_join(timings, ",");
  }
  const std::string path = kind == "pt" ? "/api/passed-time" : kind == "cy" ? "/api/cycle-time" : "/api/dfg-dataframe";
  const auto answer = second.empty() ? client.Post(path, "", "application/x-www-form-urlencoded") : client.Post(path, second);
  if (!answer) return "no response";
  JsonReader next{answer->body};
  const auto out = next.value();
  if (out["error"].kind != Json::Kind::kNull) return "error: " + error_text(out["error"]);
  if (kind == "pt")
    return "T pre:" + entries(out["pre"]) + " post:" + entries(out["post"]) + " avg " + g6(out["pre_avg_perf"]) + "/" +
           g6(out["post_avg_perf"]);
  if (kind == "cy") return "Y " + g6(out["cycle_time"]);
  std::vector<std::string> frequency, performance;
  for (const auto& e : out["frequency"].items) frequency.push_back(e["source"].text + ">" + e["target"].text + "=" + e["count"].text);
  for (const auto& e : out["performance"].items) performance.push_back(e["source"].text + ">" + e["target"].text + "=" + g6(e["value"]));
  return "F " + sorted_join(frequency, ",") + " | P " + sorted_join(performance, ",");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <minera_server>\n";
    return 2;
  }
  const int port = free_port();
  const std::string port_text = std::to_string(port);
  std::vector<std::string> args = {argv[1], "--port", port_text, "--frontend", "/nonexistent-minera-frontend"};
  std::vector<char*> raw;
  for (auto& arg : args) raw.push_back(arg.data());
  raw.push_back(nullptr);
  pid_t server = 0;
  if (::posix_spawn(&server, argv[1], nullptr, nullptr, raw.data(), environ) != 0) {
    std::cerr << "cannot start " << argv[1] << '\n';
    return 2;
  }
  httplib::Client client("127.0.0.1", port);
  client.set_read_timeout(120, 0);
  bool ready = false;
  for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
    if (auto result = client.Get("/api/version"); result && result->status == 200) ready = true;
    else std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (!ready) {
    std::cerr << "FAIL: server answers /api/version\n";
    ::kill(server, SIGTERM);
    return 1;
  }

  struct Case {
    std::string label, name, content, kind;
    Fields fields;
    std::string expected;
  };
  const std::string f0 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:17:00,2021-03-01 08:47:00\nc0,d,2021-03-01 08:18:00,2021-03-01 08:18:00\nc0,b,2021-03-01 09:18:00,2021-03-01 09:18:00\nc0,b,2021-03-01 09:18:00,2021-02-28 21:38:00\nc0,b,2021-03-01 09:48:00,2021-03-01 09:43:00\nc1,a,2021-03-01 08:06:00,2021-03-01 08:06:00\nc1,d,2021-03-01 18:06:00,2021-03-01 17:36:00\nc2,b,2021-03-01 08:35:00,2021-02-28 20:55:00\nc3,c,2021-03-01 09:18:00,2021-03-01 09:18:00\nc3,b,2021-03-01 09:19:00,2021-02-28 21:39:00\nc3,c,2021-03-01 09:24:00,2021-03-01 09:25:00\nc3,a,2021-03-01 09:24:00,2021-03-01 08:54:00\n";
  const std::string f1 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:51:00,2021-03-01 08:51:00\nc0,a,2021-03-01 08:56:00,2021-02-28 21:16:00\nc0,b,2021-03-01 09:26:00,2021-03-01 09:56:00\nc0,a,2021-03-01 09:27:00,2021-03-01 09:22:00\nc0,a,2021-03-01 09:57:00,2021-03-01 09:52:00\nc1,c,2021-03-01 08:10:00,2021-03-01 08:40:00\nc1,a,2021-03-01 09:10:00,2021-03-01 09:10:00\nc1,d,2021-03-01 09:15:00,2021-03-01 08:45:00\nc2,c,2021-03-01 18:16:00,2021-03-01 16:46:00\nc2,c,2021-03-01 19:16:00,2021-03-01 07:36:00\nc2,d,2021-03-01 19:17:00,2021-03-01 19:17:00\nc3,c,2021-03-01 18:27:00,2021-03-01 18:27:00\nc3,c,2021-03-01 18:28:00,2021-03-01 18:28:00\nc3,c,2021-03-01 18:33:00,2021-03-01 17:03:00\nc4,a,2021-03-01 08:55:00,2021-02-28 21:15:00\n";
  const std::string f2 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 09:14:00,2021-02-28 21:34:00\nc1,b,2021-03-01 08:12:00,2021-03-01 07:42:00\n";
  const std::string f3 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,c,2021-03-01 19:55:00,2021-03-01 19:25:00\nc1,d,2021-03-01 19:56:00,2021-03-01 19:56:00\nc0,d,2021-03-01 19:06:00,2021-03-01 19:07:00\nc0,a,2021-03-01 19:06:00,2021-03-01 19:01:00\nc0,a,2021-03-01 18:36:00,2021-03-01 18:36:00\nc2,a,2021-03-01 08:44:00,2021-03-01 08:14:00\nc1,b,2021-03-01 19:25:00,\nc2,a,2021-03-01 08:49:00,2021-03-01 08:44:00\nc1,a,2021-03-01 18:55:00,2021-03-01 07:15:00\nc1,c,2021-03-01 08:55:00,2021-03-01 08:25:00\nc0,c,2021-03-02 05:06:00,2021-03-02 04:36:00\nc2,c,2021-03-01 09:49:00,2021-03-01 09:19:00\nc1,b,2021-03-01 08:55:00,2021-03-01 08:55:00\n";
  const std::string f4 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 09:36:00,2021-03-01 08:06:00\nc2,c,2021-03-01 08:19:00,2021-03-01 07:49:00\nc2,b,2021-03-01 08:25:00,2021-03-01 08:25:00\nc1,d,2021-03-01 08:26:00,2021-03-01 08:21:00\nc2,d,2021-03-01 08:24:00,2021-03-01 06:54:00\nc2,b,2021-03-01 09:25:00,2021-02-28 21:45:00\nc0,d,2021-03-01 20:06:00,2021-03-01 20:36:00\nc0,b,2021-03-01 19:36:00,2021-03-01 19:31:00\nc0,a,2021-03-01 20:06:00,2021-03-01 08:26:00\nc2,b,2021-03-01 09:30:00,2021-03-01 08:00:00\n";
  const std::string f5 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:57:00,2021-03-01 08:52:00\nc0,c,2021-03-01 09:27:00,2021-02-28 21:47:00\nc0,c,2021-03-01 09:27:00,2021-03-01 09:27:00\nc0,c,2021-03-01 09:32:00,2021-03-01 09:32:00\nc1,c,2021-03-01 09:15:00,2021-03-01 09:16:00\nc1,a,2021-03-01 19:15:00,\nc2,b,2021-03-01 08:53:00,2021-02-28 21:13:00\nc2,b,2021-03-01 09:53:00,2021-03-01 10:23:00\nc2,c,2021-03-01 09:53:00,2021-03-01 09:54:00\nc2,b,2021-03-01 10:23:00,2021-03-01 09:53:00\nc2,c,2021-03-01 10:53:00,\nc2,d,2021-03-01 10:54:00,2021-03-01 10:24:00\nc3,b,2021-03-01 08:33:00,2021-03-01 07:03:00\nc3,a,2021-03-01 08:33:00,2021-03-01 08:34:00\n";
  const std::string f6 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc3,b,2021-03-01 08:24:00,2021-03-01 07:54:00\nc2,b,2021-03-01 18:00:00,2021-03-01 06:20:00\nc4,c,2021-03-01 09:04:00,2021-03-01 08:34:00\nc1,a,2021-03-01 18:25:00,2021-03-01 18:26:00\nc0,b,2021-03-01 08:56:00,\n";
  const std::string f7 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 18:19:00,2021-03-01 18:49:00\nc0,b,2021-03-01 18:20:00,2021-03-01 18:20:00\nc0,b,2021-03-01 19:20:00,2021-03-01 19:21:00\nc1,b,2021-03-01 09:00:00,2021-02-28 21:20:00\n";
  const std::string f8 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 09:19:00,2021-03-01 08:49:00\nc0,b,2021-03-01 09:19:00,2021-03-01 09:19:00\nc1,b,2021-03-01 18:00:00,2021-03-01 16:30:00\nc1,a,2021-03-01 19:00:00,2021-03-01 19:00:00\nc1,c,2021-03-01 19:05:00,2021-03-01 18:35:00\nc1,c,2021-03-01 19:05:00,2021-03-01 19:05:00\nc1,a,2021-03-02 05:05:00,2021-03-02 04:35:00\nc2,d,2021-03-01 09:26:00,2021-03-01 09:26:00\nc2,c,2021-03-01 19:26:00,2021-03-01 19:21:00\nc2,a,2021-03-02 05:26:00,2021-03-02 05:27:00\nc2,d,2021-03-02 15:26:00,2021-03-02 03:46:00\nc2,d,2021-03-02 15:26:00,2021-03-02 15:21:00\nc2,b,2021-03-02 15:31:00,2021-03-02 15:01:00\n";
  const std::string f9 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:05:00,2021-02-28 20:25:00\nc0,b,2021-03-01 08:06:00,2021-02-28 20:26:00\n";
  const std::string f10 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 09:35:00,2021-03-01 09:30:00\n";
  const std::string f11 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,d,2021-03-01 09:31:00,\nc0,b,2021-03-01 08:52:00,2021-03-01 08:22:00\nc0,b,2021-03-01 08:53:00,2021-03-01 08:48:00\n";
  const std::string f12 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:45:00,2021-03-01 08:40:00\nc1,d,2021-03-01 08:56:00,2021-02-28 21:16:00\nc1,b,2021-03-01 08:56:00,2021-03-01 08:56:00\nc1,c,2021-03-01 09:56:00,\nc1,c,2021-03-01 10:01:00,2021-03-01 10:31:00\nc1,c,2021-03-01 10:02:00,2021-03-01 09:32:00\nc1,d,2021-03-01 10:03:00,\nc2,a,2021-03-01 08:55:00,2021-03-01 08:55:00\nc2,d,2021-03-01 08:55:00,\nc2,d,2021-03-01 08:55:00,\nc3,b,2021-03-01 08:19:00,2021-03-01 08:19:00\nc3,b,2021-03-01 18:19:00,2021-03-01 18:19:00\nc3,c,2021-03-02 04:19:00,2021-03-01 16:39:00\n";
  const std::string f13 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 18:47:00,2021-03-01 18:47:00\nc0,c,2021-03-01 19:17:00,2021-03-01 19:17:00\nc0,d,2021-03-01 19:17:00,2021-03-01 19:17:00\nc1,a,2021-03-01 09:14:00,2021-03-01 09:14:00\nc1,b,2021-03-01 19:14:00,2021-03-01 19:44:00\nc1,a,2021-03-01 19:19:00,2021-03-01 19:19:00\nc1,d,2021-03-01 19:20:00,2021-03-01 19:15:00\nc1,b,2021-03-01 19:21:00,2021-03-01 17:51:00\n";
  const std::string f14 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc4,a,2021-03-01 18:26:00,2021-03-01 18:21:00\nc1,a,2021-03-01 09:35:00,2021-03-01 08:05:00\nc2,a,2021-03-01 19:51:00,2021-03-01 19:51:00\nc4,d,2021-03-02 04:56:00,2021-03-01 17:16:00\nc2,d,2021-03-01 19:21:00,2021-03-01 18:51:00\nc2,d,2021-03-01 18:21:00,\nc4,b,2021-03-02 05:31:00,2021-03-02 05:31:00\nc4,a,2021-03-01 18:56:00,2021-03-01 18:56:00\nc1,c,2021-03-01 10:35:00,2021-02-28 22:55:00\nc3,b,2021-03-01 10:20:00,2021-02-28 22:40:00\nc0,a,2021-03-01 09:17:00,2021-03-01 09:17:00\nc4,b,2021-03-02 05:01:00,2021-03-02 04:56:00\nc3,d,2021-03-01 09:15:00,2021-03-01 07:45:00\nc3,d,2021-03-01 09:15:00,2021-03-01 08:45:00\nc4,a,2021-03-02 05:31:00,2021-03-02 05:31:00\nc3,b,2021-03-01 10:15:00,2021-03-01 08:45:00\n";
  const std::string f15 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:13:00,2021-03-01 08:43:00\nc0,d,2021-03-01 08:13:00,2021-03-01 08:08:00\nc0,c,2021-03-01 08:13:00,2021-03-01 06:43:00\nc0,c,2021-03-01 18:13:00,2021-03-01 06:33:00\nc1,c,2021-03-01 09:42:00,2021-03-01 09:12:00\nc1,d,2021-03-01 10:12:00,2021-02-28 22:32:00\nc1,a,2021-03-01 10:42:00,2021-02-28 23:02:00\nc1,d,2021-03-01 11:42:00,2021-03-01 11:37:00\nc1,b,2021-03-01 11:47:00,2021-03-01 11:47:00\nc2,a,2021-03-01 08:39:00,2021-03-01 08:39:00\nc2,d,2021-03-01 08:40:00,2021-02-28 21:00:00\nc3,c,2021-03-01 08:53:00,2021-03-01 08:23:00\nc3,c,2021-03-01 08:54:00,2021-03-01 08:54:00\n";
  const std::string f16 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc3,b,2021-03-01 09:19:00,2021-03-01 09:14:00\nc2,b,2021-03-02 05:18:00,2021-03-02 05:13:00\nc2,a,2021-03-01 18:13:00,2021-03-01 17:43:00\nc3,a,2021-03-01 09:18:00,2021-03-01 09:18:00\nc3,d,2021-03-01 09:19:00,2021-02-28 21:39:00\nc3,d,2021-03-01 09:19:00,2021-03-01 08:49:00\nc0,d,2021-03-01 08:39:00,2021-03-01 08:34:00\nc1,d,2021-03-01 18:59:00,2021-03-01 18:29:00\nc2,c,2021-03-01 19:18:00,2021-03-01 19:18:00\nc1,a,2021-03-01 19:00:00,2021-03-01 07:20:00\nc2,c,2021-03-01 18:18:00,2021-03-01 06:38:00\nc3,d,2021-03-01 08:48:00,2021-03-01 08:48:00\n";
  const std::string f17 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 18:55:00,2021-03-01 07:15:00\nc0,d,2021-03-02 04:55:00,2021-03-02 04:50:00\nc1,c,2021-03-01 08:53:00,2021-03-01 08:23:00\nc1,b,2021-03-01 09:53:00,2021-03-01 09:53:00\nc1,b,2021-03-01 10:53:00,2021-02-28 23:13:00\nc2,c,2021-03-01 18:15:00,2021-03-01 17:45:00\nc2,d,2021-03-01 18:15:00,2021-03-01 06:35:00\nc2,d,2021-03-01 19:15:00,2021-03-01 17:45:00\nc2,a,2021-03-02 05:15:00,2021-03-02 03:45:00\nc3,a,2021-03-01 08:15:00,\nc3,c,2021-03-01 09:15:00,2021-03-01 09:15:00\nc3,b,2021-03-01 09:15:00,2021-02-28 21:35:00\nc3,b,2021-03-01 09:15:00,2021-03-01 09:10:00\nc3,b,2021-03-01 09:20:00,\nc4,d,2021-03-01 09:29:00,\nc4,a,2021-03-01 09:59:00,2021-03-01 10:29:00\nc4,a,2021-03-01 19:59:00,2021-03-01 19:29:00\nc4,d,2021-03-01 20:29:00,2021-03-01 20:24:00\nc4,c,2021-03-01 21:29:00,2021-03-01 19:59:00\n";
  const std::string f18 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc3,d,2021-03-01 18:17:00,2021-03-01 17:47:00\nc3,b,2021-03-01 18:47:00,2021-03-01 18:47:00\nc1,a,2021-03-01 08:24:00,2021-03-01 07:54:00\nc0,b,2021-03-01 08:34:00,2021-03-01 07:04:00\nc3,d,2021-03-01 18:12:00,2021-03-01 18:13:00\nc3,d,2021-03-01 18:12:00,2021-03-01 18:07:00\nc4,b,2021-03-01 19:06:00,2021-03-01 17:36:00\nc2,a,2021-03-01 10:04:00,2021-03-01 10:05:00\nc4,a,2021-03-01 09:00:00,2021-03-01 09:00:00\nc2,a,2021-03-01 10:04:00,2021-03-01 09:59:00\nc4,b,2021-03-01 09:06:00,2021-03-01 09:06:00\nc1,a,2021-03-01 08:24:00,2021-03-01 07:54:00\nc3,b,2021-03-01 18:17:00,2021-03-01 18:17:00\nc2,b,2021-03-01 09:59:00,2021-03-01 09:59:00\nc3,c,2021-03-01 18:17:00,2021-03-01 18:12:00\nc2,a,2021-03-01 10:34:00,2021-03-01 09:04:00\nc4,a,2021-03-01 19:07:00,2021-03-01 19:07:00\nc4,d,2021-03-01 09:05:00,2021-03-01 09:05:00\n";
  const std::string f19 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc2,d,2021-03-01 10:27:00,2021-03-01 10:22:00\nc0,d,2021-03-01 21:58:00,2021-03-01 10:18:00\nc2,b,2021-03-01 08:52:00,2021-03-01 08:22:00\nc0,b,2021-03-01 22:04:00,2021-03-01 22:04:00\nc0,d,2021-03-01 21:59:00,2021-03-01 22:00:00\nc0,c,2021-03-01 19:58:00,2021-03-01 18:28:00\nc2,c,2021-03-01 09:57:00,2021-03-01 09:27:00\nc2,a,2021-03-01 20:27:00,2021-03-01 18:57:00\nc2,a,2021-03-01 09:52:00,2021-02-28 22:12:00\nc1,a,2021-03-01 08:20:00,2021-03-01 08:20:00\nc2,c,2021-03-01 08:52:00,2021-03-01 08:52:00\nc0,c,2021-03-01 09:58:00,2021-03-01 09:28:00\nc0,b,2021-03-01 20:58:00,2021-03-01 21:28:00\nc3,b,2021-03-01 10:53:00,2021-03-01 09:23:00\nc3,d,2021-03-01 09:53:00,2021-03-01 09:53:00\n";
  const std::string f20 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 09:02:00,2021-03-01 09:02:00\nc2,d,2021-03-01 21:10:00,2021-03-01 20:40:00\nc1,d,2021-03-01 11:34:00,2021-02-28 23:54:00\nc2,a,2021-03-01 10:10:00,2021-02-28 22:30:00\nc2,a,2021-03-01 11:10:00,2021-03-01 09:40:00\nc1,c,2021-03-01 10:59:00,2021-03-01 10:29:00\nc2,a,2021-03-01 09:10:00,2021-03-01 09:10:00\nc2,d,2021-03-01 08:35:00,2021-03-01 08:05:00\nc1,a,2021-03-01 08:59:00,2021-03-01 07:29:00\nc1,b,2021-03-01 09:59:00,2021-03-01 09:54:00\nc1,d,2021-03-01 11:04:00,2021-02-28 23:24:00\nc2,a,2021-03-01 08:40:00,2021-03-01 08:35:00\n";
  const std::string f21 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 09:02:00,2021-03-01 08:32:00\nc0,a,2021-03-01 09:02:00,2021-03-01 08:32:00\nc0,c,2021-03-01 09:07:00,2021-03-01 09:07:00\nc0,a,2021-03-01 09:07:00,2021-03-01 08:37:00\nc1,a,2021-03-01 09:48:00,2021-03-01 09:43:00\nc1,d,2021-03-01 19:48:00,2021-03-01 08:08:00\nc1,b,2021-03-01 20:18:00,2021-03-01 18:48:00\nc1,a,2021-03-01 21:18:00,2021-03-01 21:18:00\nc1,a,2021-03-01 22:18:00,2021-03-01 21:48:00\nc1,b,2021-03-01 22:23:00,2021-03-01 20:53:00\nc2,d,2021-03-01 08:38:00,2021-03-01 08:38:00\nc2,b,2021-03-01 08:43:00,2021-03-01 08:43:00\nc2,d,2021-03-01 08:43:00,2021-03-01 09:13:00\nc2,a,2021-03-01 08:48:00,2021-03-01 07:18:00\nc2,b,2021-03-01 08:49:00,2021-03-01 08:49:00\nc3,d,2021-03-01 09:15:00,2021-02-28 21:35:00\nc4,a,2021-03-01 08:09:00,2021-03-01 06:39:00\nc4,c,2021-03-01 08:39:00,2021-03-01 08:34:00\nc4,b,2021-03-01 08:40:00,2021-03-01 08:10:00\n";
  const std::string f22 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:25:00,2021-03-01 08:25:00\nc0,b,2021-03-01 08:30:00,2021-03-01 08:25:00\nc0,c,2021-03-01 09:00:00,\nc0,a,2021-03-01 10:00:00,\nc0,c,2021-03-01 10:00:00,2021-03-01 10:30:00\nc0,a,2021-03-01 10:05:00,2021-03-01 10:05:00\nc1,d,2021-03-01 08:32:00,2021-03-01 08:02:00\nc1,a,2021-03-01 08:37:00,2021-03-01 08:32:00\nc1,c,2021-03-01 08:38:00,2021-03-01 09:08:00\nc1,b,2021-03-01 09:08:00,\nc1,d,2021-03-01 09:13:00,2021-03-01 09:13:00\nc1,c,2021-03-01 09:13:00,2021-03-01 09:08:00\n";
  const std::string f23 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:55:00,2021-03-01 08:50:00\nc0,a,2021-03-01 18:55:00,2021-03-01 07:15:00\nc0,a,2021-03-01 19:55:00,2021-03-01 19:50:00\nc0,a,2021-03-01 19:55:00,2021-03-01 19:55:00\nc0,b,2021-03-02 05:55:00,2021-03-02 05:55:00\nc1,b,2021-03-01 08:07:00,2021-03-01 08:37:00\nc1,a,2021-03-01 08:37:00,2021-03-01 08:37:00\nc1,d,2021-03-01 08:37:00,2021-03-01 08:32:00\nc1,a,2021-03-01 08:38:00,2021-03-01 08:38:00\nc1,a,2021-03-01 08:39:00,2021-03-01 08:40:00\nc1,a,2021-03-01 08:39:00,2021-03-01 08:39:00\nc2,d,2021-03-01 18:11:00,2021-03-01 18:11:00\nc3,d,2021-03-01 09:38:00,2021-03-01 09:08:00\nc3,d,2021-03-01 19:38:00,2021-03-01 19:38:00\nc3,a,2021-03-01 20:08:00,2021-03-01 08:28:00\nc3,b,2021-03-01 20:38:00,2021-03-01 20:33:00\nc3,c,2021-03-01 20:38:00,2021-03-01 20:38:00\n";
  const std::string f24 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,b,2021-03-01 09:22:00,2021-03-01 09:22:00\nc0,b,2021-03-01 09:50:00,2021-03-01 09:50:00\nc4,a,2021-03-01 18:15:00,2021-03-01 18:10:00\nc2,c,2021-03-01 08:45:00,2021-03-01 08:45:00\nc4,a,2021-03-02 05:45:00,2021-03-02 05:40:00\nc0,a,2021-03-01 08:50:00,2021-03-01 08:45:00\nc4,c,2021-03-02 04:45:00,2021-03-02 04:15:00\nc3,c,2021-03-01 09:49:00,2021-03-01 09:49:00\nc3,a,2021-03-01 20:54:00,2021-03-01 19:24:00\nc4,d,2021-03-02 04:15:00,2021-03-02 03:45:00\nc4,a,2021-03-02 04:45:00,2021-03-01 17:05:00\nc1,d,2021-03-01 08:22:00,2021-03-01 08:22:00\nc1,c,2021-03-01 09:23:00,2021-03-01 09:23:00\nc1,b,2021-03-01 10:23:00,2021-03-01 10:18:00\nc0,a,2021-03-01 08:50:00,2021-03-01 08:45:00\nc3,c,2021-03-01 19:54:00,2021-03-01 19:49:00\nc3,a,2021-03-01 09:54:00,2021-03-01 10:24:00\nc3,b,2021-03-01 09:19:00,2021-03-01 09:19:00\nc3,d,2021-03-01 09:54:00,\n";
  const std::string f25 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:50:00,2021-03-01 08:45:00\nc0,d,2021-03-01 18:50:00,2021-03-01 18:45:00\nc0,d,2021-03-02 04:50:00,2021-03-02 04:20:00\nc0,d,2021-03-02 04:50:00,2021-03-01 17:10:00\nc1,b,2021-03-01 08:43:00,2021-02-28 21:03:00\nc1,a,2021-03-01 18:43:00,2021-03-01 07:03:00\nc2,b,2021-03-01 18:57:00,2021-03-01 17:27:00\nc3,b,2021-03-01 18:14:00,2021-03-01 16:44:00\nc3,b,2021-03-01 18:44:00,2021-03-01 17:14:00\nc3,d,2021-03-01 19:14:00,2021-03-01 19:14:00\nc3,c,2021-03-01 19:14:00,2021-03-01 19:14:00\n";
  const std::string f26 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc2,d,2021-03-02 04:12:00,2021-03-02 04:12:00\nc0,c,2021-03-01 08:23:00,2021-03-01 08:18:00\nc1,b,2021-03-01 18:07:00,2021-03-01 17:37:00\nc2,a,2021-03-01 08:07:00,2021-03-01 07:37:00\nc0,d,2021-03-01 08:18:00,2021-03-01 07:48:00\nc2,c,2021-03-01 18:07:00,2021-03-01 17:37:00\nc2,b,2021-03-02 04:12:00,2021-03-02 04:07:00\nc2,a,2021-03-02 04:07:00,2021-03-01 16:27:00\nc2,b,2021-03-02 04:17:00,2021-03-01 16:37:00\nc1,a,2021-03-01 08:07:00,2021-03-01 08:07:00\nc0,c,2021-03-01 09:23:00,2021-02-28 21:43:00\n";
  const std::string f27 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:54:00,2021-03-01 08:49:00\nc0,b,2021-03-01 08:55:00,2021-02-28 21:15:00\nc0,c,2021-03-01 18:55:00,2021-03-01 17:25:00\nc0,b,2021-03-01 19:25:00,2021-03-01 19:25:00\nc0,d,2021-03-01 20:25:00,2021-03-01 08:45:00\nc0,c,2021-03-01 20:25:00,2021-03-01 19:55:00\n";
  const std::string f28 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:36:00,2021-03-01 07:06:00\nc0,c,2021-03-01 08:37:00,2021-03-01 07:07:00\nc0,a,2021-03-01 18:37:00,2021-03-01 18:37:00\nc0,d,2021-03-02 04:37:00,2021-03-02 04:32:00\nc0,b,2021-03-02 04:38:00,2021-03-02 04:08:00\nc1,a,2021-03-01 09:08:00,2021-03-01 09:08:00\nc1,d,2021-03-01 10:08:00,2021-03-01 10:08:00\nc1,d,2021-03-01 11:08:00,2021-03-01 09:38:00\nc1,a,2021-03-01 11:09:00,2021-03-01 11:10:00\nc1,a,2021-03-01 11:39:00,2021-03-01 11:39:00\nc2,b,2021-03-01 08:27:00,2021-03-01 06:57:00\nc2,a,2021-03-01 08:27:00,2021-03-01 08:22:00\nc2,d,2021-03-01 09:27:00,2021-03-01 09:28:00\nc3,a,2021-03-01 08:13:00,2021-03-01 08:08:00\nc3,b,2021-03-01 09:13:00,2021-02-28 21:33:00\nc3,a,2021-03-01 09:14:00,2021-03-01 07:44:00\n";
  const std::string f29 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 18:56:00,2021-03-01 18:56:00\nc0,c,2021-03-01 19:56:00,2021-03-01 19:56:00\nc0,d,2021-03-02 05:56:00,2021-03-01 18:16:00\nc0,a,2021-03-02 05:57:00,2021-03-02 05:27:00\nc0,c,2021-03-02 06:02:00,2021-03-02 06:03:00\nc0,d,2021-03-02 06:02:00,2021-03-02 06:02:00\n";
  const std::string f30 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 10:52:00,2021-03-01 10:22:00\nc3,c,2021-03-01 18:56:00,2021-03-01 17:26:00\nc3,d,2021-03-01 18:26:00,2021-03-01 18:27:00\nc2,c,2021-03-02 05:02:00,2021-03-02 03:32:00\nc0,b,2021-03-01 09:52:00,2021-03-01 09:22:00\nc0,d,2021-03-01 10:52:00,\nc0,a,2021-03-01 10:52:00,2021-03-01 10:52:00\nc2,c,2021-03-02 05:03:00,2021-03-02 05:03:00\nc2,b,2021-03-01 18:32:00,2021-03-01 18:02:00\nc2,a,2021-03-02 04:32:00,2021-03-02 03:02:00\nc1,c,2021-03-01 09:20:00,2021-03-01 09:21:00\nc2,b,2021-03-01 18:02:00,2021-03-01 17:32:00\nc1,c,2021-03-01 09:20:00,2021-03-01 07:50:00\nc2,b,2021-03-01 18:01:00,2021-03-01 16:31:00\nc1,b,2021-03-01 09:19:00,2021-03-01 08:49:00\nc0,c,2021-03-01 08:52:00,2021-03-01 08:22:00\n";
  const std::string f31 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:13:00,2021-03-01 08:13:00\nc0,b,2021-03-01 18:13:00,2021-03-01 18:13:00\nc0,d,2021-03-01 18:18:00,2021-03-01 06:38:00\nc0,d,2021-03-01 19:18:00,2021-03-01 19:18:00\nc0,c,2021-03-01 19:23:00,2021-03-01 19:23:00\nc1,d,2021-03-01 08:28:00,2021-03-01 06:58:00\nc1,c,2021-03-01 08:58:00,2021-03-01 08:53:00\nc1,d,2021-03-01 09:58:00,2021-03-01 09:28:00\nc1,a,2021-03-01 10:28:00,2021-03-01 08:58:00\nc1,b,2021-03-01 10:29:00,2021-03-01 10:29:00\nc1,a,2021-03-01 20:29:00,2021-03-01 20:29:00\n";
  const std::string f32 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:26:00,2021-03-01 08:21:00\nc1,a,2021-03-01 09:05:00,2021-03-01 09:05:00\nc1,a,2021-03-01 09:10:00,2021-02-28 21:30:00\nc1,d,2021-03-01 09:40:00,2021-03-01 09:40:00\n";
  const std::string f33 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,b,2021-03-01 19:07:00,2021-03-01 19:07:00\nc1,b,2021-03-01 19:08:00,2021-03-01 19:03:00\nc2,b,2021-03-01 08:49:00,2021-03-01 08:44:00\nc2,c,2021-03-01 11:29:00,2021-02-28 23:49:00\nc0,c,2021-03-01 08:36:00,2021-03-01 08:31:00\nc3,d,2021-03-01 09:29:00,2021-03-01 09:29:00\nc1,b,2021-03-01 08:37:00,2021-02-28 20:57:00\nc3,c,2021-03-01 08:28:00,2021-03-01 06:58:00\nc1,b,2021-03-01 09:07:00,2021-03-01 09:02:00\nc0,b,2021-03-01 09:07:00,2021-03-01 08:37:00\nc0,b,2021-03-01 08:36:00,2021-03-01 09:06:00\nc2,c,2021-03-01 10:29:00,2021-03-01 10:24:00\nc1,b,2021-03-01 09:07:00,2021-02-28 21:27:00\nc2,a,2021-03-01 08:59:00,2021-03-01 08:59:00\nc2,b,2021-03-01 09:29:00,2021-03-01 07:59:00\nc0,d,2021-03-01 09:06:00,2021-03-01 09:01:00\nc3,a,2021-03-01 08:29:00,2021-03-01 07:59:00\nc3,d,2021-03-01 09:34:00,2021-03-01 09:29:00\nc2,d,2021-03-01 08:54:00,2021-03-01 08:49:00\n";
  const std::string f34 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:01:00,2021-02-28 20:21:00\nc0,b,2021-03-01 08:02:00,2021-03-01 08:02:00\nc1,a,2021-03-01 08:36:00,2021-03-01 08:31:00\nc1,b,2021-03-01 08:41:00,2021-02-28 21:01:00\nc1,d,2021-03-01 09:41:00,2021-02-28 22:01:00\nc1,a,2021-03-01 10:11:00,2021-03-01 08:41:00\nc1,b,2021-03-01 10:16:00,2021-03-01 10:16:00\nc2,d,2021-03-01 08:39:00,2021-03-01 07:09:00\nc2,a,2021-03-01 18:39:00,2021-03-01 17:09:00\nc2,b,2021-03-01 18:44:00,2021-03-01 18:44:00\nc2,d,2021-03-01 18:49:00,2021-03-01 07:09:00\n";
  const std::string f35 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 18:13:00,2021-03-01 18:13:00\nc1,a,2021-03-01 08:49:00,2021-02-28 21:09:00\nc1,a,2021-03-01 09:19:00,2021-03-01 09:19:00\nc1,b,2021-03-01 09:20:00,2021-03-01 09:20:00\nc1,a,2021-03-01 09:25:00,2021-03-01 07:55:00\nc1,c,2021-03-01 09:55:00,2021-03-01 09:56:00\nc1,a,2021-03-01 10:00:00,2021-03-01 10:00:00\nc2,a,2021-03-01 09:11:00,2021-03-01 08:41:00\nc2,a,2021-03-01 09:11:00,2021-03-01 09:11:00\nc2,b,2021-03-01 10:11:00,2021-03-01 08:41:00\nc2,c,2021-03-01 11:11:00,2021-03-01 11:11:00\nc2,a,2021-03-01 11:12:00,2021-02-28 23:32:00\nc3,b,2021-03-01 08:30:00,2021-03-01 08:30:00\n";
  const std::string f36 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,d,2021-03-01 19:26:00,2021-03-01 07:46:00\nc0,d,2021-03-01 11:33:00,2021-03-01 11:33:00\nc3,a,2021-03-01 09:53:00,2021-03-01 09:48:00\nc1,d,2021-03-01 18:56:00,2021-03-01 18:56:00\nc1,d,2021-03-01 19:31:00,2021-03-01 19:31:00\nc2,c,2021-03-01 18:08:00,2021-03-01 18:38:00\nc2,a,2021-03-01 08:08:00,2021-03-01 06:38:00\nc3,b,2021-03-01 08:53:00,2021-03-01 08:53:00\nc0,d,2021-03-01 09:28:00,2021-03-01 09:28:00\nc3,b,2021-03-01 09:54:00,2021-03-01 09:49:00\nc1,a,2021-03-01 08:56:00,2021-03-01 08:51:00\nc1,a,2021-03-01 19:31:00,2021-03-01 19:31:00\nc0,b,2021-03-01 10:33:00,2021-02-28 22:53:00\nc0,b,2021-03-01 10:28:00,\nc0,c,2021-03-01 08:58:00,2021-03-01 08:53:00\nc4,a,2021-03-01 18:39:00,\nc2,c,2021-03-01 18:08:00,2021-03-01 16:38:00\n";
  const std::string f37 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:50:00,2021-02-28 21:10:00\nc0,b,2021-03-01 09:50:00,2021-03-01 09:50:00\nc0,b,2021-03-01 19:50:00,2021-03-01 08:10:00\nc1,d,2021-03-01 08:31:00,2021-03-01 08:31:00\nc1,a,2021-03-01 08:31:00,2021-03-01 08:31:00\nc2,c,2021-03-01 08:20:00,2021-03-01 07:50:00\n";
  const std::string f38 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc2,d,2021-03-01 18:25:00,2021-03-01 16:55:00\nc2,d,2021-03-01 08:20:00,2021-03-01 08:20:00\nc0,a,2021-03-01 18:05:00,2021-03-01 17:35:00\nc0,c,2021-03-01 18:35:00,2021-03-01 18:30:00\nc3,d,2021-03-01 08:03:00,2021-03-01 08:03:00\nc0,b,2021-03-01 18:00:00,2021-03-01 18:00:00\nc2,c,2021-03-01 18:26:00,2021-03-01 17:56:00\nc3,d,2021-03-01 09:08:00,2021-03-01 09:09:00\nc3,a,2021-03-01 09:09:00,2021-03-01 09:10:00\nc2,b,2021-03-01 18:20:00,2021-03-01 18:15:00\nc0,a,2021-03-02 04:40:00,2021-03-02 04:10:00\nc0,c,2021-03-02 04:35:00,2021-03-02 04:05:00\nc1,d,2021-03-01 18:55:00,2021-03-01 07:15:00\nc3,b,2021-03-01 09:03:00,2021-03-01 08:58:00\n";
  const std::string f39 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:37:00,2021-03-01 08:38:00\nc0,b,2021-03-01 09:07:00,2021-02-28 21:27:00\nc0,a,2021-03-01 09:37:00,2021-03-01 08:07:00\nc0,b,2021-03-01 10:07:00,2021-02-28 22:27:00\nc1,c,2021-03-01 08:44:00,\nc1,d,2021-03-01 08:49:00,2021-03-01 07:19:00\nc1,d,2021-03-01 18:49:00,2021-03-01 07:09:00\nc1,a,2021-03-01 18:49:00,2021-03-01 19:19:00\nc2,d,2021-03-01 08:59:00,2021-03-01 09:29:00\nc2,a,2021-03-01 09:04:00,2021-02-28 21:24:00\nc2,b,2021-03-01 10:04:00,2021-03-01 10:04:00\nc2,d,2021-03-01 10:34:00,2021-02-28 22:54:00\nc2,c,2021-03-01 11:04:00,2021-03-01 10:59:00\nc3,c,2021-03-01 08:25:00,2021-03-01 08:25:00\nc3,d,2021-03-01 18:25:00,2021-03-01 16:55:00\nc3,a,2021-03-01 18:25:00,2021-03-01 16:55:00\nc3,d,2021-03-01 18:25:00,\nc3,d,2021-03-01 18:26:00,2021-03-01 06:46:00\nc3,b,2021-03-01 18:56:00,2021-03-01 18:57:00\n";
  const std::string f40 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:43:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:44:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:14:00.000+00:00\"/><int key=\"start_timestamp\" value=\"-3\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:15:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:00.000+00:00\"/><int key=\"start_timestamp\" value=\"5\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:11:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:16:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:21:00.000+00:00\"/><int key=\"start_timestamp\" value=\"5\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:58:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:58:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:58:00.000+00:00\"/><int key=\"start_timestamp\" value=\"1\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:59:00.000+00:00\"/><int key=\"start_timestamp\" value=\"12\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:59:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string f41 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 09:31:00,early\nc0,a,2021-03-01 09:32:00,True\nc0,d,2021-03-01 09:01:00,2021-03-01 09:01:00\nc0,b,2021-03-01 09:33:00,\n";
  const std::string f42 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:16:00.000+00:00\"/><int key=\"start_timestamp\" value=\"-3\"/></event>\n</trace>\n</log>\n";
  const std::string f43 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 09:09:00,2021-03-01 09:04:00\nc0,a,2021-03-01 09:10:00,2021-02-28 21:30:00\nc0,d,2021-03-01 09:40:00,2021-03-01 09:35:00\nc0,a,2021-03-01 09:45:00,2021-03-01 09:45:00\nc0,d,2021-03-01 10:15:00,2021-02-28 22:35:00\n";
  const std::string f44 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:24:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:19:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:29:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-02-28T20:49:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:55:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T07:25:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:55:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:55:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:55:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-02-28T22:15:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:55:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T10:25:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:35:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:35:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:35:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:35:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:40:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-02-28T22:00:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:45:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:45:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:15:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T10:15:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:16:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T10:17:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:33:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:34:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:33:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:28:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:12:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:12:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:17:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:12:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:22:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:22:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string f45 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 09:00:00,2021-03-01 09:00:00\nc0,a,2021-03-01 09:05:00,2021-03-01 08:35:00\nc1,b,2021-03-01 08:44:00,2021-03-01 07:14:00\nc1,c,2021-03-01 08:44:00,2021-02-28 21:04:00\nc1,c,2021-03-01 09:44:00,2021-03-01 09:44:00\nc1,b,2021-03-01 09:44:00,2021-03-01 09:39:00\nc2,c,2021-03-01 08:01:00,2021-03-01 07:56:00\nc2,b,2021-03-01 08:01:00,2021-03-01 07:56:00\nc2,d,2021-03-01 18:01:00,2021-03-01 17:56:00\nc3,c,2021-03-01 09:29:00,2021-03-01 09:29:00\nc4,b,2021-03-01 18:56:00,2021-03-01 18:57:00\nc4,a,2021-03-01 19:01:00,2021-03-01 18:31:00\n";
  const std::string f46 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:22:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:52:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:22:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-02-28 22:42:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:52:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 10:52:00\"/></event>\n</trace>\n</log>\n";
  const std::string f47 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:06:00,2021-03-01 08:06:00\nc0,b,2021-03-01 08:11:00,\nc0,c,2021-03-01 18:11:00,2021-03-01 18:11:00\n";
  const std::string f48 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T06:38:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:26:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:26:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:31:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T07:01:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:03:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:58:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:16:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:16:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:46:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T07:16:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:15:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:16:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:15:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:10:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:20:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:20:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:20:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T17:50:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string f49 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:11:00,0.5\nc0,b,2021-03-01 09:11:00,-2.25\nc0,c,2021-03-01 10:11:00,-2.25\nc0,d,2021-03-01 10:41:00,3.0\nc1,c,2021-03-01 08:36:00,3.0\nc1,b,2021-03-01 08:36:00,-2.25\nc1,a,2021-03-01 18:36:00,-2.25\nc1,a,2021-03-01 19:06:00,3.0\n";
  const std::string f50 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T18:30:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:30:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T19:30:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:35:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T19:35:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:40:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T19:10:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T20:40:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T20:40:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:03:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T07:33:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T19:01:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:30:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T20:30:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T19:00:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T21:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T20:30:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:31:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:10:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:10:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:10:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T06:30:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:40:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T18:40:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:40:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T19:35:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string f51 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc3,b,2021-03-01 09:47:00,2021-03-01 09:42:00\nc2,a,2021-03-01 08:53:00,2021-03-01 08:23:00\nc3,a,2021-03-01 09:17:00,2021-03-01 08:47:00\nc4,a,2021-03-01 09:13:00,2021-02-28 21:33:00\nc2,c,2021-03-02 05:54:00,\nc2,c,2021-03-02 05:53:00,2021-03-02 04:23:00\nc4,d,2021-03-01 08:13:00,2021-03-01 08:08:00\nc0,c,2021-03-01 08:57:00,2021-03-01 08:57:00\nc2,a,2021-03-02 04:53:00,2021-03-01 17:13:00\nc2,d,2021-03-01 18:53:00,\nc0,c,2021-03-01 09:27:00,2021-03-01 08:57:00\nc1,a,2021-03-01 08:35:00,2021-03-01 08:35:00\nc2,a,2021-03-01 18:53:00,2021-03-01 18:53:00\n";
  const std::string f52 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:28:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:28:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:10:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:11:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:15:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:10:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:20:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T06:50:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:25:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:20:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:45:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:46:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:46:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:41:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:46:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T17:16:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:06:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:36:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:06:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T17:36:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-02T05:06:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-02T05:01:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-02T05:36:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-02T05:31:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-02T05:36:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-02T05:37:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string f53 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 09:25:00,2021-02-28 21:45:00\nc0,d,2021-03-01 09:55:00,2021-03-01 09:25:00\n";
  const std::string f54 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:18:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T18:18:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:18:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T18:19:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:23:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T16:53:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:53:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::vector<Case> cases = {
      {"pm def s69500.csv", "s69500.csv", f0, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,a>d=1,b>a=1,b>b=1,b>d=1,c>c=1,d>b=1,d>d=1 | S a=1,b=1,c=1,d=1 | X a=1,b=2,d=1 | P a>c=-360/-360,a>d=34200/34200,b>a=-1500/-1500,b>b=1500/1500,b>d=-3600/-3600,c>c=420/420,d>b=3660/3660,d>d=1740/1740"},
      {"pv def s69500.csv", "s69500.csv", f0, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>c=-360/-360,a>d=34200/34200,b>a=-1500/-1500,b>b=1500/1500,b>d=-3600/-3600,c>c=420/420,d>b=3660/3660,d>d=1740/1740"},
      {"pv bh s69500.csv", "s69500.csv", f0, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>c=0/0,a>d=32040/32040,b>a=0/0,b>b=1500/1500,b>d=0/0,c>c=420/420,d>b=3660/3660,d>d=1740/1740"},
      {"pt def s69500.csv", "s69500.csv", f0, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}}, "T pre:b=-1500/1 post:c=-360/1,d=34200/1 avg -1500/16920"},
      {"pt bh s69500.csv", "s69500.csv", f0, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}, {"p:passed_time_business_hours", "true"}}, "T pre:b=0/1 post:c=0/1,d=32040/1 avg 0/16020"},
      {"cy def s69500.csv", "s69500.csv", f0, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 11295"},
      {"df def s69500.csv", "s69500.csv", f0, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>c=1,a>d=1,b>a=1,b>b=1,b>d=1,c>c=1,d>b=1,d>d=1 | P a>c=-360,a>d=34200,b>a=-1500,b>b=1500,b>d=-3600,c>c=420,d>b=3660,d>d=1740"},
      {"df bh s69500.csv", "s69500.csv", f0, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F a>c=1,a>d=1,b>a=1,b>b=1,b>d=1,c>c=1,d>b=1,d>d=1 | P a>c=0,a>d=32040,b>a=0,b>b=1500,b>d=0,c>c=420,d>b=3660,d>d=1740"},
      {"pm def s69501.csv", "s69501.csv", f1, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>b=1,a>d=1,c>c=3,c>d=2,d>a=2 | S a=1,c=3,d=1 | X a=2,c=1,d=2 | P a>a=1500/1500,a>b=-60/-60,a>d=-300/-300,c>c=-3100/60,c>d=2880/3660,d>a=780/1860"},
      {"pv def s69501.csv", "s69501.csv", f1, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=1500/1500,a>b=-60/-60,a>d=-300/-300,c>c=-3100/-9300,c>d=2880/5760,d>a=780/1560"},
      {"pv bh s69501.csv", "s69501.csv", f1, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=1500/1500,a>b=0/0,a>d=0/0,c>c=0/0,c>d=1050/2100,d>a=930/1860"},
      {"pt def s69501.csv", "s69501.csv", f1, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}}, "T pre:a=1500/1,d=780/2 post:a=1500/1,b=-60/1,d=-300/1 avg 1020/380"},
      {"pt bh s69501.csv", "s69501.csv", f1, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}, {"p:passed_time_business_hours", "true"}}, "T pre:a=1500/1,d=930/2 post:a=1500/1,b=0/1,d=0/1 avg 1120/500"},
      {"cy def s69501.csv", "s69501.csv", f1, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 15852"},
      {"df def s69501.csv", "s69501.csv", f1, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=1,a>b=1,a>d=1,c>c=3,c>d=2,d>a=2 | P a>a=1500,a>b=-60,a>d=-300,c>c=-3100,c>d=2880,d>a=780"},
      {"df bh s69501.csv", "s69501.csv", f1, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F a>a=1,a>b=1,a>d=1,c>c=3,c>d=2,d>a=2 | P a>a=1500,a>b=0,a>d=0,c>c=0,c>d=1050,d>a=930"},
      {"pm def s69502.csv", "s69502.csv", f2, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E  | S b=1,d=1 | X b=1,d=1 | P "},
      {"pv def s69502.csv", "s69502.csv", f2, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V "},
      {"pv bh s69502.csv", "s69502.csv", f2, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V "},
      {"pt def s69502.csv", "s69502.csv", f2, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "b"}, {"p:passed_time_variant", "prepost"}}, "T pre: post: avg 0/0"},
      {"pt bh s69502.csv", "s69502.csv", f2, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "b"}, {"p:passed_time_variant", "prepost"}, {"p:passed_time_business_hours", "true"}}, "T pre: post: avg 0/0"},
      {"cy def s69502.csv", "s69502.csv", f2, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 0"},
      {"df def s69502.csv", "s69502.csv", f2, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F  | P "},
      {"df bh s69502.csv", "s69502.csv", f2, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F  | P "},
      {"pm def s69503.csv", "s69503.csv", f3, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=2,a>c=2,a>d=1,b>c=1,c>b=1,c>d=1,d>b=1,d>c=1 | S a=2,c=1 | X c=2,d=1 | P a>a=750/1500,a>c=-18000/1800,a>d=60/60,b>c=37800/37800,c>b=0/0,c>d=60/60,d>b=None/None,d>c=34200/34200"},
      {"pv def s69503.csv", "s69503.csv", f3, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=750/1500,a>c=-18000/-36000,a>d=60/60,b>c=37800/37800,c>b=0/0,c>d=60/60,d>b=None/0,d>c=34200/34200"},
      {"pv bh s69503.csv", "s69503.csv", f3, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "error: ValueError: cannot convert float NaN to integer"},
      {"pt bh s69503.csv", "s69503.csv", f3, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}, {"p:passed_time_business_hours", "true"}}, "error: ValueError: cannot convert float NaN to integer"},
      {"cy def s69503.csv", "s69503.csv", f3, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: ValueError: NaTType does not support timestamp"},
      {"df def s69503.csv", "s69503.csv", f3, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=2,a>c=2,a>d=1,b>c=1,c>b=1,c>d=1,d>b=1,d>c=1 | P a>a=750,a>c=-18000,a>d=60,b>c=37800,c>b=0,c>d=60,d>b=None,d>c=34200"},
      {"df bh s69503.csv", "s69503.csv", f3, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "error: ValueError: cannot convert float NaN to integer"},
      {"pm def s69504.csv", "s69504.csv", f4, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=1,b>b=1,b>d=2,c>a=1,c>b=1,d>c=1 | S c=2,d=1 | X a=1,b=1,d=1 | P a>b=-2100/-2100,b>b=-3900/-3900,b>d=-2730/3600,c>a=-4200/-4200,c>b=-1140/-1140,d>c=-2100/-2100"},
      {"pv def s69504.csv", "s69504.csv", f4, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=-2100/-2100,b>b=-3900/-3900,b>d=-2730/-5460,c>a=-4200/-4200,c>b=-1140/-1140,d>c=-2100/-2100"},
      {"pv bh s69504.csv", "s69504.csv", f4, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>b=0/0,b>b=0/0,b>d=0/0,c>a=0/0,c>b=0/0,d>c=0/0"},
      {"cy def s69504.csv", "s69504.csv", f4, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 26820"},
      {"df def s69504.csv", "s69504.csv", f4, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>b=1,b>b=1,b>d=2,c>a=1,c>b=1,d>c=1 | P a>b=-2100,b>b=-3900,b>d=-2730,c>a=-4200,c>b=-1140,d>c=-2100"},
      {"df bh s69504.csv", "s69504.csv", f4, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F a>b=1,b>b=1,b>d=2,c>a=1,c>b=1,d>c=1 | P a>b=0,b>b=0,b>d=0,c>a=0,c>b=0,d>c=0"},
      {"pm def s69505.csv", "s69505.csv", f5, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>a=1,b>b=1,b>c=1,b>d=1,c>a=1,c>b=1,c>c=3,d>c=1 | S b=2,c=2 | X a=2,c=1,d=1 | P b>a=60/60,b>b=3600/3600,b>c=-1740/-1740,b>d=1860/1860,c>a=None/None,c>b=1800/1800,c>c=0/1800,d>c=None/None"},
      {"pv def s69505.csv", "s69505.csv", f5, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>a=60/60,b>b=3600/3600,b>c=-1740/-1740,b>d=1860/1860,c>a=None/0,c>b=1800/1800,c>c=0/0,d>c=None/0"},
      {"pv bh s69505.csv", "s69505.csv", f5, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "error: ValueError: cannot convert float NaN to integer"},
      {"pt bh s69505.csv", "s69505.csv", f5, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}, {"p:passed_time_business_hours", "true"}}, "error: ValueError: cannot convert float NaN to integer"},
      {"cy def s69505.csv", "s69505.csv", f5, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: ValueError: NaTType does not support timestamp"},
      {"df def s69505.csv", "s69505.csv", f5, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F b>a=1,b>b=1,b>c=1,b>d=1,c>a=1,c>b=1,c>c=3,d>c=1 | P b>a=60,b>b=3600,b>c=-1740,b>d=1860,c>a=None,c>b=1800,c>c=0,d>c=None"},
      {"df bh s69505.csv", "s69505.csv", f5, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "error: ValueError: cannot convert float NaN to integer"},
      {"pm def s69506.csv", "s69506.csv", f6, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E  | S a=1,b=3,c=1 | X a=1,b=3,c=1 | P "},
      {"pm def s69507.csv", "s69507.csv", f7, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>d=1,d>b=1 | S b=1,d=1 | X b=2 | P b>d=1740/1740,d>b=3720/3720"},
      {"pv def s69507.csv", "s69507.csv", f7, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>d=1740/1740,d>b=3720/3720"},
      {"pv bh s69507.csv", "s69507.csv", f7, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V b>d=0/0,d>b=0/0"},
      {"cy def s69507.csv", "s69507.csv", f7, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 20130"},
      {"df def s69507.csv", "s69507.csv", f7, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F b>d=1,d>b=1 | P b>d=1740,d>b=3720"},
      {"df bh s69507.csv", "s69507.csv", f7, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F b>d=1,d>b=1 | P b>d=0,d>b=0"},
      {"pm def s69508.csv", "s69508.csv", f8, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=1,a>c=1,b>c=1,b>d=1,c>a=2,c>d=1,d>a=1,d>b=1,d>c=1 | S b=1,d=2 | X a=1,b=2 | P a>b=34500/34500,a>c=300/300,b>c=2100/2100,b>d=-600/-600,c>a=16950/34200,c>d=30000/30000,d>a=-35940/-35940,d>b=0/0,d>c=35700/35700"},
      {"pv def s69508.csv", "s69508.csv", f8, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=34500/34500,a>c=300/300,b>c=2100/2100,b>d=-600/-600,c>a=16950/33900,c>d=30000/30000,d>a=-35940/-35940,d>b=0/0,d>c=35700/35700"},
      {"pv bh s69508.csv", "s69508.csv", f8, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>b=28860/28860,a>c=0/0,b>c=0/0,b>d=0/0,c>a=0/0,c>d=0/0,d>a=0/0,d>b=0/0,d>c=27240/27240"},
      {"cy def s69508.csv", "s69508.csv", f8, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 3100"},
      {"df def s69508.csv", "s69508.csv", f8, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>b=1,a>c=1,b>c=1,b>d=1,c>a=2,c>d=1,d>a=1,d>b=1,d>c=1 | P a>b=34500,a>c=300,b>c=2100,b>d=-600,c>a=16950,c>d=30000,d>a=-35940,d>b=0,d>c=35700"},
      {"df bh s69508.csv", "s69508.csv", f8, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F a>b=1,a>c=1,b>c=1,b>d=1,c>a=2,c>d=1,d>a=1,d>b=1,d>c=1 | P a>b=28860,a>c=0,b>c=0,b>d=0,c>a=0,c>d=0,d>a=0,d>b=0,d>c=27240"},
      {"pm def s69509.csv", "s69509.csv", f9, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>b=1 | S b=1 | X b=1 | P b>b=-41940/-41940"},
      {"pv def s69509.csv", "s69509.csv", f9, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>b=-41940/-41940"},
      {"pv bh s69509.csv", "s69509.csv", f9, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V b>b=0/0"},
      {"df def s69509.csv", "s69509.csv", f9, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F b>b=1 | P b>b=-41940"},
      {"df bh s69509.csv", "s69509.csv", f9, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F b>b=1 | P b>b=0"},
      {"pm def s69510.csv", "s69510.csv", f10, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E  | S c=1 | X c=1 | P "},
      {"pm def s69511.csv", "s69511.csv", f11, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>b=1 | S b=1,d=1 | X b=1,d=1 | P b>b=-240/-240"},
      {"pv def s69511.csv", "s69511.csv", f11, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>b=-240/-240"},
      {"pv bh s69511.csv", "s69511.csv", f11, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V b>b=0/0"},
      {"df def s69511.csv", "s69511.csv", f11, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F b>b=1 | P b>b=-240"},
      {"pm def s69512.csv", "s69512.csv", f12, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>d=1,b>c=2,c>b=1,c>c=2,c>d=1,d>b=1,d>d=1 | S a=2,b=1,d=1 | X a=1,c=1,d=2 | P a>d=None/None,b>c=16080/30000,c>b=-36000/-36000,c>c=1740/1740,c>d=None/None,d>b=0/0,d>d=None/None"},
      {"pv def s69512.csv", "s69512.csv", f12, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>d=None/0,b>c=16080/32160,c>b=-36000/-36000,c>c=1740/1740,c>d=None/0,d>b=0/0,d>d=None/0"},
      {"df def s69512.csv", "s69512.csv", f12, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>d=1,b>c=2,c>b=1,c>c=2,c>d=1,d>b=1,d>d=1 | P a>d=None,b>c=16080,c>b=-36000,c>c=1740,c>d=None,d>b=0,d>d=None"},
      {"pm def s69513.csv", "s69513.csv", f13, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=2,b>c=1,b>d=1,c>d=1,d>a=1 | S a=1,b=1 | X b=1,d=1 | P a>b=16260/31020,b>c=1800/1800,b>d=-360/-360,c>d=0/0,d>a=-60/-60"},
      {"pv def s69513.csv", "s69513.csv", f13, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=16260/32520,b>c=1800/1800,b>d=-360/-360,c>d=0/0,d>a=-60/-60"},
      {"pv bh s69513.csv", "s69513.csv", f13, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>b=13980/27960,b>c=0/0,b>d=0/0,c>d=0/0,d>a=0/0"},
      {"cy def s69513.csv", "s69513.csv", f13, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 2700"},
      {"df def s69513.csv", "s69513.csv", f13, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>b=2,b>c=1,b>d=1,c>d=1,d>a=1 | P a>b=16260,b>c=1800,b>d=-360,c>d=0,d>a=-60"},
      {"df bh s69513.csv", "s69513.csv", f13, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}, {"p:dfg_business_hours", "true"}}, "F a>b=2,b>c=1,b>d=1,c>d=1,d>a=1 | P a>b=13980,b>c=0,b>d=0,c>d=0,d>a=0"},
      {"pm def s69514.csv", "s69514.csv", f14, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>b=1,a>d=1,b>a=1,b>b=1,b>d=1,c>a=1,d>a=2,d>b=1,d>d=1 | S a=3,d=2 | X a=3,b=1,c=1 | P a>a=1800/1800,a>b=36000/36000,a>d=None/None,b>a=0/0,b>b=1800/1800,b>d=-9300/-9300,c>a=-9000/-9000,d>a=-18150/1800,d>b=-1800/-1800,d>d=-1800/-1800"},
      {"pv def s69514.csv", "s69514.csv", f14, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=1800/1800,a>b=36000/36000,a>d=None/0,b>a=0/0,b>b=1800/1800,b>d=-9300/-9300,c>a=-9000/-9000,d>a=-18150/-36300,d>b=-1800/-1800,d>d=-1800/-1800"},
      {"df def s69514.csv", "s69514.csv", f14, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=1,a>b=1,a>d=1,b>a=1,b>b=1,b>d=1,c>a=1,d>a=2,d>b=1,d>d=1 | P a>a=1800,a>b=36000,a>d=None,b>a=0,b>b=1800,b>d=-9300,c>a=-9000,d>a=-18150,d>b=-1800,d>d=-1800"},
      {"pm def s69515.csv", "s69515.csv", f15, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,c>c=2,c>d=2,d>a=2,d>b=1,d>d=1 | S a=1,c=2,d=1 | X b=1,c=2,d=1 | P a>c=-5400/-5400,c>c=-20670/60,c>d=3300/6900,d>a=-20130/-60,d>b=300/300,d>d=1800/1800"},
      {"pv def s69515.csv", "s69515.csv", f15, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>c=-5400/-5400,c>c=-20670/-41340,c>d=3300/6600,d>a=-20130/-40260,d>b=300/300,d>d=1800/1800"},
      {"pv bh s69515.csv", "s69515.csv", f15, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>c=0/0,c>c=30/60,c>d=3450/6900,d>a=0/0,d>b=300/300,d>d=1800/1800"},
      {"pm def s69516.csv", "s69516.csv", f16, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,a>d=1,b>a=1,c>a=1,c>b=1,d>b=1,d>d=2 | S a=1,d=3 | X a=1,b=1,d=2 | P a>c=3900/3900,a>d=-1860/-1860,b>a=-60/-60,c>a=-2100/-2100,c>b=35700/35700,d>b=-300/-300,d>d=-900/60"},
      {"pv def s69516.csv", "s69516.csv", f16, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>c=3900/3900,a>d=-1860/-1860,b>a=-60/-60,c>a=-2100/-2100,c>b=35700/35700,d>b=-300/-300,d>d=-900/-1800"},
      {"pv bh s69516.csv", "s69516.csv", f16, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>c=0/0,a>d=0/0,b>a=0/0,c>a=0/0,c>b=0/0,d>b=0/0,d>d=30/60"},
      {"cy def s69516.csv", "s69516.csv", f16, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 19215"},
      {"pm def s69517.csv", "s69517.csv", f17, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>b=1,a>c=1,b>b=1,b>c=2,b>d=1,c>a=1,c>b=1,c>d=2,d>a=1,d>c=1,d>d=1 | S a=1,b=1,c=2,d=1 | X a=1,b=2,c=1,d=1 | P a>a=34200/34200,a>b=None/None,a>c=0/0,b>b=-300/-300,b>c=-4500/0,b>d=35700/35700,c>a=None/None,c>b=3600/3600,c>d=-2850/-1800,d>a=30600/30600,d>c=-1800/-1800,d>d=None/None"},
      {"pv def s69517.csv", "s69517.csv", f17, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=34200/34200,a>b=None/0,a>c=0/0,b>b=-300/-300,b>c=-4500/-9000,b>d=35700/35700,c>a=None/0,c>b=3600/3600,c>d=-2850/-5700,d>a=30600/30600,d>c=-1800/-1800,d>d=None/0"},
      {"df def s69517.csv", "s69517.csv", f17, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=1,a>b=1,a>c=1,b>b=1,b>c=2,b>d=1,c>a=1,c>b=1,c>d=2,d>a=1,d>c=1,d>d=1 | P a>a=34200,a>b=None,a>c=0,b>b=-300,b>c=-4500,b>d=35700,c>a=None,c>b=3600,c>d=-2850,d>a=30600,d>c=-1800,d>d=None"},
      {"pm def s69518.csv", "s69518.csv", f18, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=2,a>b=1,a>d=1,b>a=2,b>b=2,c>d=1,d>b=2,d>c=1,d>d=1 | S a=2,b=2,d=1 | X a=3,b=2 | P a>a=-870/60,a>b=-2100/-2100,a>d=300/300,b>a=30/60,b>b=16200/30600,c>d=-240/-240,d>b=180/300,d>c=0/0,d>d=-600/-600"},
      {"pv def s69518.csv", "s69518.csv", f18, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-870/-1740,a>b=-2100/-2100,a>d=300/300,b>a=30/60,b>b=16200/32400,c>d=-240/-240,d>b=180/360,d>c=0/0,d>d=-600/-600"},
      {"pv bh s69518.csv", "s69518.csv", f18, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=30/60,a>b=0/0,a>d=300/300,b>a=0/0,b>b=14220/28440,c>d=0/0,d>b=30/60,d>c=0/0,d>d=0/0"},
      {"cy def s69518.csv", "s69518.csv", f18, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 3240"},
      {"pm def s69519.csv", "s69519.csv", f19, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=1,b>c=1,b>d=2,c>b=1,c>c=1,c>d=2,d>a=1,d>b=1,d>c=1 | S a=1,b=1,c=1,d=1 | X a=2,b=2 | P a>b=-5400/-5400,b>c=0/0,b>d=60/3720,c>b=5400/5400,c>c=2100/2100,c>d=1350/1500,d>a=30600/30600,d>b=300/300,d>c=-12600/-12600"},
      {"pv def s69519.csv", "s69519.csv", f19, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=-5400/-5400,b>c=0/0,b>d=60/120,c>b=5400/5400,c>c=2100/2100,c>d=1350/2700,d>a=30600/30600,d>b=300/300,d>c=-12600/-12600"},
      {"pv bh s69519.csv", "s69519.csv", f19, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>b=0/0,b>c=0/0,b>d=0/0,c>b=0/0,c>c=2100/2100,c>d=1350/2700,d>a=23580/23580,d>b=0/0,d>c=0/0"},
      {"cy def s69519.csv", "s69519.csv", f19, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 21375"},
      {"pm def s69520.csv", "s69520.csv", f20, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=2,a>b=1,a>d=2,b>c=1,d>a=2,d>d=1 | S a=1,b=1,d=1 | X b=1,d=2 | P a>a=1800/1800,a>b=3300/3300,a>d=13350/34200,b>c=1800/1800,d>a=-7350/0,d>d=-40200/-40200"},
      {"pv def s69520.csv", "s69520.csv", f20, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=1800/3600,a>b=3300/3300,a>d=13350/26700,b>c=1800/1800,d>a=-7350/-14700,d>d=-40200/-40200"},
      {"pv bh s69520.csv", "s69520.csv", f20, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=1800/3600,a>b=3300/3300,a>d=10500/21000,b>c=1800/1800,d>a=0/0,d>d=0/0"},
      {"cy def s69520.csv", "s69520.csv", f20, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 15680"},
      {"pm def s69521.csv", "s69521.csv", f21, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=2,a>b=2,a>c=1,a>d=1,b>a=1,b>b=2,b>c=1,b>d=1,d>a=2,d>b=1 | S a=2,d=3 | X a=1,b=3,d=1 | P a>a=150/1800,a>b=16230/32400,a>c=0/0,a>d=-600/-600,b>a=-3900/-3900,b>b=1230/2100,b>c=-360/-360,b>d=1440/1440,d>a=-19050/-1800,d>b=300/300"},
      {"pv def s69521.csv", "s69521.csv", f21, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=150/300,a>b=16230/32460,a>c=0/0,a>d=-600/-600,b>a=-3900/-3900,b>b=1230/2460,b>c=-360/-360,b>d=1440/1440,d>a=-19050/-38100,d>b=300/300"},
      {"pv bh s69521.csv", "s69521.csv", f21, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=0/0,a>b=12990/25980,a>c=0/0,a>d=0/0,b>a=0/0,b>b=180/360,b>c=0/0,b>d=1440/1440,d>a=0/0,d>b=300/300"},
      {"cy def s69521.csv", "s69521.csv", f21, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 16356"},
      {"pm def s69522.csv", "s69522.csv", f22, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=1,a>c=2,b>a=1,c>a=1,c>c=2,c>d=1,d>a=1,d>b=1 | S a=1,d=1 | X a=1,c=1 | P a>b=0/0,a>c=1680/1860,b>a=5700/5700,c>a=None/None,c>c=1800/1800,c>d=0/0,d>a=0/0,d>b=None/None"},
      {"pv def s69522.csv", "s69522.csv", f22, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=0/0,a>c=1680/3360,b>a=5700/5700,c>a=None/0,c>c=1800/1800,c>d=0/0,d>a=0/0,d>b=None/0"},
      {"df def s69522.csv", "s69522.csv", f22, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>b=1,a>c=2,b>a=1,c>a=1,c>c=2,c>d=1,d>a=1,d>b=1 | P a>b=0,a>c=1680,b>a=5700,c>a=None,c>c=1800,c>d=0,d>a=0,d>b=None"},
      {"pm def s69523.csv", "s69523.csv", f23, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=4,a>b=2,a>d=1,b>a=2,b>c=1,d>b=2,d>d=1 | S b=2,d=2 | X a=1,b=1,c=1,d=1 | P a>a=45/60,a>b=-150/36000,a>d=-39600/-39600,b>a=20550/39300,b>c=0/0,d>b=1650/3300,d>d=36000/36000"},
      {"pv def s69523.csv", "s69523.csv", f23, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=45/180,a>b=-150/-300,a>d=-39600/-39600,b>a=20550/41100,b>c=0/0,d>b=1650/3300,d>d=36000/36000"},
      {"pv bh s69523.csv", "s69523.csv", f23, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=45/180,a>b=0/0,a>d=0/0,b>a=15450/30900,b>c=0/0,d>b=0/0,d>d=26520/26520"},
      {"cy def s69523.csv", "s69523.csv", f23, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 11670"},
      {"pm def s69524.csv", "s69524.csv", f24, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=3,a>b=1,a>c=1,a>d=1,b>c=2,c>a=2,c>b=1,c>d=1,d>b=1,d>c=1 | S a=2,b=1,c=1,d=1 | X a=2,b=2,c=1 | P a>a=-1400/34200,a>b=3600/3600,a>c=-3900/-3900,a>d=34200/34200,b>c=930/1800,c>a=2700/3300,c>b=3300/3300,c>d=None/None,d>b=3600/3600,d>c=0/0"},
      {"pv def s69524.csv", "s69524.csv", f24, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-1400/-4200,a>b=3600/3600,a>c=-3900/-3900,a>d=34200/34200,b>c=930/1860,c>a=2700/5400,c>b=3300/3300,c>d=None/0,d>b=3600/3600,d>c=0/0"},
      {"df def s69524.csv", "s69524.csv", f24, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=3,a>b=1,a>c=1,a>d=1,b>c=2,c>a=2,c>b=1,c>d=1,d>b=1,d>c=1 | P a>a=-1400,a>b=3600,a>c=-3900,a>d=34200,b>c=930,c>a=2700,c>b=3300,c>d=None,d>b=3600,d>c=0"},
      {"pm def s69525.csv", "s69525.csv", f25, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>d=1,b>a=1,b>b=1,b>d=1,d>c=1,d>d=2 | S a=1,b=3 | X a=1,b=1,c=1,d=1 | P a>d=30000/30000,b>a=-6000/-6000,b>b=-3600/-3600,b>d=1800/1800,d>c=0/0,d>d=-1050/34200"},
      {"pv def s69525.csv", "s69525.csv", f25, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>d=30000/30000,b>a=-6000/-6000,b>b=-3600/-3600,b>d=1800/1800,d>c=0/0,d>d=-1050/-2100"},
      {"pv bh s69525.csv", "s69525.csv", f25, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>d=29400/29400,b>a=0/0,b>b=0/0,b>d=0/0,d>c=0/0,d>d=0/0"},
      {"pm def s69526.csv", "s69526.csv", f26, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>b=2,b>c=1,b>d=1,c>b=1,c>d=1,d>c=1 | S a=2,d=1 | X b=2,c=1 | P a>a=30000/30000,a>b=-3600/34200,b>c=-38400/-38400,b>d=0/0,c>b=36000/36000,c>d=-5700/-5700,d>c=0/0"},
      {"pv def s69526.csv", "s69526.csv", f26, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=30000/30000,a>b=-3600/-7200,b>c=-38400/-38400,b>d=0/0,c>b=36000/36000,c>d=-5700/-5700,d>c=0/0"},
      {"pv bh s69526.csv", "s69526.csv", f26, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=30000/30000,a>b=15990/31980,b>c=0/0,b>d=0/0,c>b=0/0,c>d=0/0,d>c=0/0"},
      {"cy def s69526.csv", "s69526.csv", f26, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 14000"},
      {"pm def s69527.csv", "s69527.csv", f27, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>c=2,b>d=1,c>b=1,d>b=1 | S b=1 | X c=1 | P b>c=16230/30660,b>d=-600/-600,c>b=1800/1800,d>b=-41760/-41760"},
      {"pv def s69527.csv", "s69527.csv", f27, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>c=16230/32460,b>d=-600/-600,c>b=1800/1800,d>b=-41760/-41760"},
      {"pv bh s69527.csv", "s69527.csv", f27, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V b>c=14580/29160,b>d=0/0,c>b=0/0,d>b=0/0"},
      {"pm def s69528.csv", "s69528.csv", f28, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=2,a>b=1,a>d=2,b>a=2,b>d=1,c>a=1,d>a=1,d>c=1,d>d=1 | S a=2,b=1,d=1 | X a=2,b=1,d=1 | P a>a=-1080/1800,a>b=34260/34260,a>d=2730/3660,b>a=-2820/-300,b>d=-360/-360,c>a=36000/36000,d>a=3720/3720,d>c=-5340/-5340,d>d=-3600/-3600"},
      {"pv def s69528.csv", "s69528.csv", f28, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-1080/-2160,a>b=34260/34260,a>d=2730/5460,b>a=-2820/-5640,b>d=-360/-360,c>a=36000/36000,d>a=3720/3720,d>c=-5340/-5340,d>d=-3600/-3600"},
      {"pv bh s69528.csv", "s69528.csv", f28, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=900/1800,a>b=0/0,a>d=2730/5460,b>a=0/0,b>d=0/0,c>a=30180/30180,d>a=3720/3720,d>c=0/0,d>d=0/0"},
      {"cy def s69528.csv", "s69528.csv", f28, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 11835"},
      {"pm def s69529.csv", "s69529.csv", f29, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>d=1,b>c=1,c>a=1,d>b=1,d>c=1 | S b=1 | X d=1 | P a>d=300/300,b>c=3600/3600,c>a=34260/34260,d>b=-39600/-39600,d>c=60/60"},
      {"pv def s69529.csv", "s69529.csv", f29, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>d=300/300,b>c=3600/3600,c>a=34260/34260,d>b=-39600/-39600,d>c=60/60"},
      {"pv bh s69529.csv", "s69529.csv", f29, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>d=0/0,b>c=0/0,c>a=0/0,d>b=0/0,d>c=0/0"},
      {"cy def s69529.csv", "s69529.csv", f29, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 42060"},
      {"pm def s69530.csv", "s69530.csv", f30, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,a>d=1,b>a=1,b>b=2,b>c=1,b>d=1,c>b=2,c>c=1,c>d=1,d>a=1 | S b=2,c=1,d=1 | X a=1,c=3 | P a>c=-3600/-3600,a>d=None/None,b>a=30600/30600,b>b=-870/0,b>c=120/120,b>d=1800/1800,c>b=-30/1800,c>c=60/60,c>d=-1740/-1740,d>a=0/0"},
      {"pv def s69530.csv", "s69530.csv", f30, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>c=-3600/-3600,a>d=None/0,b>a=30600/30600,b>b=-870/-1740,b>c=120/120,b>d=1800/1800,c>b=-30/-60,c>c=60/60,c>d=-1740/-1740,d>a=0/0"},
      {"df def s69530.csv", "s69530.csv", f30, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>c=1,a>d=1,b>a=1,b>b=2,b>c=1,b>d=1,c>b=2,c>c=1,c>d=1,d>a=1 | P a>c=-3600,a>d=None,b>a=30600,b>b=-870,b>c=120,b>d=1800,c>b=-30,c>c=60,c>d=-1740,d>a=0"},
      {"pm def s69531.csv", "s69531.csv", f31, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>d=1,b>a=1,b>b=1,b>d=1,c>a=1,d>b=2,d>c=2 | S b=1,d=1 | X a=1,c=1 | P a>d=-3600/-3600,b>a=36000/36000,b>b=36000/36000,b>d=3900/3900,c>a=0/0,d>b=-17220/1860,d>c=900/1500"},
      {"pv def s69531.csv", "s69531.csv", f31, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>d=-3600/-3600,b>a=36000/36000,b>b=36000/36000,b>d=3900/3900,c>a=0/0,d>b=-17220/-34440,d>c=900/1800"},
      {"pv bh s69531.csv", "s69531.csv", f31, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>d=0/0,b>a=23460/23460,b>b=31620/31620,b>d=0/0,c>a=0/0,d>b=930/1860,d>c=750/1500"},
      {"cy def s69531.csv", "s69531.csv", f31, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 21000"},
      {"pm def s69532.csv", "s69532.csv", f32, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>d=1 | S a=2 | X a=1,d=1 | P a>a=-300/-300,a>d=2100/2100"},
      {"pv def s69532.csv", "s69532.csv", f32, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-300/-300,a>d=2100/2100"},
      {"pv bh s69532.csv", "s69532.csv", f32, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=0/0,a>d=2100/2100"},
      {"cy def s69532.csv", "s69532.csv", f32, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 21000"},
      {"pm def s69533.csv", "s69533.csv", f33, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,a>d=1,b>b=5,b>d=2,c>a=1,c>b=2,d>a=1,d>b=1,d>d=1 | S b=2,c=2 | X b=2,c=1,d=1 | P a>c=5100/5100,a>d=3600/3600,b>b=-1500/35760,b>d=-180/0,c>a=-1740/-1740,c>b=-6270/60,d>a=300/300,d>b=0/0,d>d=0/0"},
      {"pv def s69533.csv", "s69533.csv", f33, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>c=5100/5100,a>d=3600/3600,b>b=-1500/-7500,b>d=-180/-360,c>a=-1740/-1740,c>b=-6270/-12540,d>a=300/300,d>b=0/0,d>d=0/0"},
      {"pv bh s69533.csv", "s69533.csv", f33, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>c=5100/5100,a>d=3600/3600,b>b=5676/28380,b>d=0/0,c>a=0/0,c>b=30/60,d>a=300/300,d>b=0/0,d>d=0/0"},
      {"cy def s69533.csv", "s69533.csv", f33, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 13080"},
      {"pm def s69534.csv", "s69534.csv", f34, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>b=2,b>d=1,c>b=1,d>a=2,d>d=1 | S a=1,c=1,d=1 | X b=2,d=1 | P a>a=300/300,a>b=300/300,b>d=-38400/-38400,c>b=60/60,d>a=-5100/-4200,d>d=-5400/-5400"},
      {"pv def s69534.csv", "s69534.csv", f34, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=300/300,a>b=300/600,b>d=-38400/-38400,c>b=60/60,d>a=-5100/-10200,d>d=-5400/-5400"},
      {"pv bh s69534.csv", "s69534.csv", f34, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=300/300,a>b=150/300,b>d=0/0,c>b=60/60,d>a=0/0,d>d=0/0"},
      {"pm def s69535.csv", "s69535.csv", f35, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=3,a>b=2,a>c=1,b>a=1,b>c=1,c>a=1 | S a=2,b=2 | X a=2,b=2 | P a>a=-4220/-360,a>b=-870/60,a>c=7200/7200,b>a=-3600/-3600,b>c=2160/2160,c>a=300/300"},
      {"pv def s69535.csv", "s69535.csv", f35, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-4220/-12660,a>b=-870/-1740,a>c=7200/7200,b>a=-3600/-3600,b>c=2160/2160,c>a=300/300"},
      {"pv bh s69535.csv", "s69535.csv", f35, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>a=0/0,a>b=30/60,a>c=7200/7200,b>a=0/0,b>c=2160/2160,c>a=300/300"},
      {"cy def s69535.csv", "s69535.csv", f35, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 12645"},
      {"pm def s69536.csv", "s69536.csv", f36, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=1,a>c=1,a>d=1,b>a=1,b>c=1,c>c=1,c>d=1,d>a=2,d>b=1,d>d=2 | S a=3,b=1,c=1 | X a=2,b=1,c=1,d=1 | P a>b=-240/-240,a>c=30600/30600,a>d=36000/36000,b>a=3300/3300,b>c=-6000/-6000,c>c=1800/1800,c>d=1800/1800,d>a=-19050/0,d>b=None/None,d>d=4800/7500"},
      {"pv def s69536.csv", "s69536.csv", f36, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=-240/-240,a>c=30600/30600,a>d=36000/36000,b>a=3300/3300,b>c=-6000/-6000,c>c=1800/1800,c>d=1800/1800,d>a=-19050/-38100,d>b=None/0,d>d=4800/9600"},
      {"df def s69536.csv", "s69536.csv", f36, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>b=1,a>c=1,a>d=1,b>a=1,b>c=1,c>c=1,c>d=1,d>a=2,d>b=1,d>d=2 | P a>b=-240,a>c=30600,a>d=36000,b>a=3300,b>c=-6000,c>c=1800,c>d=1800,d>a=-19050,d>b=None,d>d=4800"},
      {"pm def s69537.csv", "s69537.csv", f37, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>b=1,c>b=1,d>a=1 | S c=2,d=1 | X a=1,b=1,c=1 | P b>b=-36000/-36000,c>b=-2400/-2400,d>a=0/0"},
      {"pv def s69537.csv", "s69537.csv", f37, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>b=-36000/-36000,c>b=-2400/-2400,d>a=0/0"},
      {"pv bh s69537.csv", "s69537.csv", f37, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V b>b=0/0,c>b=0/0,d>a=0/0"},
      {"pm def s69538.csv", "s69538.csv", f38, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=1,b>c=1,b>d=1,c>a=1,c>b=1,c>c=1,d>a=1,d>b=1,d>c=1,d>d=1 | S b=1,d=3 | X a=2,c=1,d=1 | P a>b=-300/-300,b>c=1800/1800,b>d=360/360,c>a=-1500/-1500,c>b=-660/-660,c>c=34200/34200,d>a=120/120,d>b=3300/3300,d>c=-1740/-1740,d>d=30900/30900"},
      {"pv def s69538.csv", "s69538.csv", f38, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=-300/-300,b>c=1800/1800,b>d=360/360,c>a=-1500/-1500,c>b=-660/-660,c>c=34200/34200,d>a=120/120,d>b=3300/3300,d>c=-1740/-1740,d>d=30900/30900"},
      {"pv bh s69538.csv", "s69538.csv", f38, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}, {"dfg_business_hours", "true"}}, "V a>b=0/0,b>c=0/0,b>d=360/360,c>a=0/0,c>b=0/0,c>c=0/0,d>a=120/120,d>b=3300/3300,d>c=0/0,d>d=30900/30900"},
      {"cy def s69538.csv", "s69538.csv", f38, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 10500"},
      {"pm def s69539.csv", "s69539.csv", f39, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=1,a>b=1,a>c=1,a>d=1,b>a=1,b>b=1,b>c=1,b>d=1,c>d=1,d>a=2,d>b=1,d>c=1,d>d=2 | S a=1,c=2,d=1 | X a=1,b=2,c=1 | P a>a=-3540/-3540,a>b=1920/1920,a>c=None/None,a>d=-36600/-36600,b>a=-7200/-7200,b>b=-38400/-38400,b>c=3300/3300,b>d=None/None,c>d=30600/30600,d>a=16200/37800,d>b=3900/3900,d>c=-36060/-36060,d>d=-22650/-3900"},
      {"pv def s69539.csv", "s69539.csv", f39, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-3540/-3540,a>b=1920/1920,a>c=None/0,a>d=-36600/-36600,b>a=-7200/-7200,b>b=-38400/-38400,b>c=3300/3300,b>d=None/0,c>d=30600/30600,d>a=16200/32400,d>b=3900/3900,d>c=-36060/-36060,d>d=-22650/-45300"},
      {"df def s69539.csv", "s69539.csv", f39, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=1,a>b=1,a>c=1,a>d=1,b>a=1,b>b=1,b>c=1,b>d=1,c>d=1,d>a=2,d>b=1,d>c=1,d>d=2 | P a>a=-3540,a>b=1920,a>c=None,a>d=-36600,b>a=-7200,b>b=-38400,b>c=3300,b>d=None,c>d=30600,d>a=16200,d>b=3900,d>c=-36060,d>d=-22650"},
      {"pm def x71000.xes", "x71000.xes", f40, "pm", {}, "E a>a=2,a>c=1,a>d=1,b>d=1,c>c=2,d>a=1,d>c=2 | S a=2,b=1,c=1 | X a=2,c=2 | Perr TypeError: cannot subtract DatetimeArray from ndarray[float64]"},
      {"pv def x71000.xes", "x71000.xes", f40, "pv", {{"dfg_statistical_variants", "performance"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[float64]"},
      {"pt def x71000.xes", "x71000.xes", f40, "pt", {{"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[float64]"},
      {"pm def x71001.csv", "x71001.csv", f41, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,c>b=1,d>a=1 | S d=1 | X b=1 | Perr TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"pv def x71001.csv", "x71001.csv", f41, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"pt def x71001.csv", "x71001.csv", f41, "pt", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:passed_time_activity", "a"}, {"p:passed_time_variant", "prepost"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"pm def x71002.xes", "x71002.xes", f42, "pm", {}, "E  | S d=1 | X d=1 | Perr TypeError: cannot subtract DatetimeArray from ndarray[float64]"},
      {"pm def x71003.csv", "x71003.csv", f43, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>d=1,d>a=1,d>d=2 | S d=1 | X d=1 | P a>d=-38100/-38100,d>a=300/300,d>d=-1350/1560"},
      {"pv def x71003.csv", "x71003.csv", f43, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>d=-38100/-38100,d>a=300/300,d>d=-1350/-2700"},
      {"pm def x71004.xes", "x71004.xes", f44, "pm", {}, "E a>b=1,a>d=1,b>a=1,c>a=1,c>b=2,c>c=1,c>d=1,d>a=2,d>c=1,d>d=1 | S a=1,b=1,c=2,d=1 | X a=1,b=1,c=3 | P a>b=120/120,a>d=600/600,b>a=300/300,c>a=-600/-600,c>b=30/60,c>c=-9000/-9000,c>d=3600/3600,d>a=2700/3600,d>c=1800/1800,d>d=-3900/-3900"},
      {"pv def x71004.xes", "x71004.xes", f44, "pv", {{"dfg_statistical_variants", "performance"}}, "V a>b=120/120,a>d=600/600,b>a=300/300,c>a=-600/-600,c>b=30/60,c>c=-9000/-9000,c>d=3600/3600,d>a=2700/5400,d>c=1800/1800,d>d=-3900/-3900"},
      {"pm def x71005.csv", "x71005.csv", f45, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>b=2,b>b=1,b>c=1,b>d=1,c>b=2 | S b=3,c=2 | X a=2,b=1,c=1,d=1 | P a>b=-270/-240,b>b=3300/3300,b>c=0/0,b>d=35700/35700,c>b=-2850/-300"},
      {"pv def x71005.csv", "x71005.csv", f45, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>b=-270/-540,b>b=3300/3300,b>c=0/0,b>d=35700/35700,c>b=-2850/-5700"},
      {"cy def x71005.csv", "x71005.csv", f45, "cy", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Y 8772"},
      {"pm def x71006.xes", "x71006.xes", f46, "pm", {}, "E a>c=1,c>d=1 | S c=1 | X d=1 | Perr TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"pm def x71007.csv", "x71007.csv", f47, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E b>c=1,c>b=1 | S b=1 | X c=1 | P b>c=36300/36300,c>b=None/None"},
      {"pv def x71007.csv", "x71007.csv", f47, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V b>c=36300/36300,c>b=None/0"},
      {"df def x71007.csv", "x71007.csv", f47, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F b>c=1,c>b=1 | P b>c=36300,c>b=None"},
      {"pm def x71008.xes", "x71008.xes", f48, "pm", {}, "E a>c=1,a>d=1,b>b=1,d>a=1,d>d=1 | S a=2,b=1,c=2 | X a=2,b=1,c=1,d=1 | P a>c=-300/-300,a>d=300/300,b>b=-1800/-1800,d>a=60/60,d>d=30600/30600"},
      {"pv def x71008.xes", "x71008.xes", f48, "pv", {{"dfg_statistical_variants", "performance"}}, "V a>c=-300/-300,a>d=300/300,b>b=-1800/-1800,d>a=60/60,d>d=30600/30600"},
      {"pm def x71009.csv", "x71009.csv", f49, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>c=1,b>a=1,b>c=1,c>a=1,c>c=1,c>d=1 | S c=2 | X a=1,d=1 | Perr TypeError: cannot subtract DatetimeArray from ndarray[float64]"},
      {"pm def x71010.xes", "x71010.xes", f50, "pm", {}, "E a>c=1,a>d=1,b>a=1,b>b=1,b>d=1,c>a=1,d>b=1,d>c=2,d>d=2 | S a=1,b=1,c=1,d=2 | X a=1,c=2,d=2 | P a>c=300/300,a>d=36000/36000,b>a=-600/-600,b>b=-36000/-36000,b>d=37800/37800,c>a=3900/3900,d>b=600/600,d>c=3300/3300,d>d=30/5400"},
      {"pv def x71010.xes", "x71010.xes", f50, "pv", {{"dfg_statistical_variants", "performance"}}, "V a>c=300/300,a>d=36000/36000,b>a=-600/-600,b>b=-36000/-36000,b>d=37800/37800,c>a=3900/3900,d>b=600/600,d>c=3300/3300,d>d=30/60"},
      {"pm def x71011.csv", "x71011.csv", f51, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E a>a=2,a>b=1,a>c=1,a>d=1,c>c=1,c>d=1,d>c=1 | S a=3,c=1,d=1 | X a=2,b=1,c=2 | P a>a=-3000/30000,a>b=1500/1500,a>c=34200/34200,a>d=-3900/-3900,c>c=0/0,c>d=None/None,d>c=None/None"},
      {"pv def x71011.csv", "x71011.csv", f51, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V a>a=-3000/-6000,a>b=1500/1500,a>c=34200/34200,a>d=-3900/-3900,c>c=0/0,c>d=None/0,d>c=None/0"},
      {"df def x71011.csv", "x71011.csv", f51, "df", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"p:dfg_measure", "both"}}, "F a>a=2,a>b=1,a>c=1,a>d=1,c>c=1,c>d=1,d>c=1 | P a>a=-3000,a>b=1500,a>c=34200,a>d=-3900,c>c=0,c>d=None,d>c=None"},
      {"pm def x71012.xes", "x71012.xes", f52, "pm", {}, "E a>a=1,a>d=1,b>d=1,c>a=1,c>b=1,c>c=2,d>a=1,d>c=1,d>d=1 | S c=3,d=1 | X a=2,c=2 | P a>a=60/60,a>d=-600/-600,b>d=35700/35700,c>a=30660/30660,c>b=30600/30600,c>c=0/0,d>a=1500/1500,d>c=600/600,d>d=-240/-240"},
      {"pv def x71012.xes", "x71012.xes", f52, "pv", {{"dfg_statistical_variants", "performance"}}, "V a>a=60/60,a>d=-600/-600,b>d=35700/35700,c>a=30660/30660,c>b=30600/30600,c>c=0/0,d>a=1500/1500,d>c=600/600,d>d=-240/-240"},
      {"pm def x71013.csv", "x71013.csv", f53, "pm", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "E d>d=1 | S d=1 | X d=1 | P d>d=0/0"},
      {"pv def x71013.csv", "x71013.csv", f53, "pv", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "performance"}}, "V d>d=0/0"},
      {"pm def x71014.xes", "x71014.xes", f54, "pm", {}, "E a>b=1,b>a=1,d>b=1 | S b=1 | X b=1 | P a>b=None/None,b>a=60/60,d>b=-300/-300"},
      {"pv def x71014.xes", "x71014.xes", f54, "pv", {{"dfg_statistical_variants", "performance"}}, "V a>b=None/0,b>a=60/60,d>b=-300/-300"},
  };
  for (const auto& c : cases) check(c.label, outcome(client, c.name, c.content, c.kind, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GU server checks passed\n";
  return 0;
}
