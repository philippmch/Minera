#undef NDEBUG
// Review GW: pm4py's reads of an upload's start_timestamp column in the
// eventually-follows graph, the temporal profile and keyed DFG variants.
//
// - /api/mine's eventually-follows graph on the default log shape follows
//   pm4py's frame discovery: pairs (i<j) of a case whose end precedes the
//   later event's start_timestamp (CSV rows in the frame's index order, XES
//   rows sorted by case, start and timestamp); a start column of no dates
//   reports the reference's comparison or subtraction TypeError under
//   eventually_follows_error.
// - The temporal profile on the lazy shape with a uint64 start column:
//   Polars' Int64 cast beside dates, the u64/i64 Float64 supertype.
// - The pandas temporal profile with a text start column whose values all
//   parse as dates (the subtraction TypeError, or the business-hours UDF's
//   replace() error) and with a True/False start column that has a gap in a
//   row format_dataframe dropped (object dtype).
// - DFG variants keyed by such a start column.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gw: tc/mkgw.py, ct/*.py).
//
// Usage: minera_server_review_gw_tests <minera_server>

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

// One case's outcome in mkgw.py's canonical form.
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""}};
  for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto top = reader.value();
  if (top["error"].kind != Json::Kind::kNull)
    return (kind == "ef" ? "Gerr " : "error: ") + error_text(top["error"]);
  // The frame shapes wrap their analysis and report a family apart.
  const auto& body = top["analysis"].kind == Json::Kind::kObject ? top["analysis"] : top;
  std::vector<std::string> names;
  for (const auto& a : body["activities"].items) names.push_back(a["name"].text);
  const auto named = [&](const Json& id) { return names[std::stoul(id.text)]; };
  if (kind == "ef") {
    for (const auto* holder : {&top, &body})
      if ((*holder)["eventually_follows_error"].kind == Json::Kind::kString)
        return "Gerr " + (*holder)["eventually_follows_error"].text;
    std::vector<std::string> edges;
    for (const auto& e : body["eventually_follows"].items)
      edges.push_back(named(e["source"]) + ">" + named(e["target"]) + "=" + e["count"].text);
    return "G " + sorted_join(edges, ",");
  }
  if (kind == "tp") {
    const auto& family = top["family_results"]["temporal_profile"];
    if (family["executed"].text != "true") return "error: " + error_text(family["error"]);
    std::vector<std::string> entries;
    for (const auto& e : family["result"]["entries"].items)
      entries.push_back(name_of(e["source"]) + ">" + name_of(e["target"]) + "=" + g6(e["mean_seconds"]) + "/" +
                        g6(e["standard_deviation_seconds"]));
    return "Q " + sorted_join(entries, ",");
  }
  std::vector<std::string> edges;
  for (const auto& e : body["dfg_variants"]["statistical"].items[0]["frequency_dfg"].items)
    edges.push_back(named(e["source"]) + ">" + named(e["target"]) + "=" + e["count"].text);
  return "N " + sorted_join(edges, ",");
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
  const std::string f0 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,b,2021-03-01 08:51:00,2021-03-01 08:21:00\nc2,c,2021-03-01 08:47:00,2021-03-01 07:17:00\nc0,b,2021-03-01 09:49:00,2021-02-28 22:09:00\nc1,c,2021-03-01 08:52:00,2021-03-01 08:22:00\nc3,b,2021-03-01 18:17:00,2021-03-01 17:47:00\nc1,b,2021-03-01 08:51:00,2021-03-01 08:46:00\nc1,a,2021-03-01 09:22:00,2021-03-01 07:52:00\n";
  const std::string f1 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:23:00,2021-03-01 08:23:00\nc0,b,2021-03-01 18:23:00,2021-03-01 06:43:00\nc0,a,2021-03-01 19:23:00,2021-03-01 07:43:00\nc0,b,2021-03-01 19:53:00,2021-03-01 18:23:00\nc0,a,2021-03-01 20:23:00,2021-03-01 19:53:00\nc0,d,2021-03-01 20:24:00,2021-03-01 20:19:00\nc1,a,2021-03-01 08:21:00,2021-03-01 06:51:00\nc1,b,2021-03-01 08:26:00,2021-03-01 08:26:00\nc2,d,2021-03-01 09:41:00,2021-03-01 09:11:00\nc3,a,2021-03-01 09:04:00,2021-03-01 08:34:00\nc3,c,2021-03-01 09:05:00,2021-02-28 21:25:00\nc3,d,2021-03-01 09:05:00,2021-03-01 09:05:00\nc3,b,2021-03-01 09:10:00,2021-02-28 21:30:00\nc3,c,2021-03-01 09:11:00,2021-03-01 09:11:00\nc4,d,2021-03-01 09:12:00,2021-03-01 09:07:00\nc4,b,2021-03-01 19:12:00,2021-03-01 19:42:00\nc4,a,2021-03-02 05:12:00,2021-03-02 03:42:00\nc4,d,2021-03-02 06:12:00,2021-03-02 04:42:00\nc4,d,2021-03-02 06:17:00,2021-03-01 18:37:00\nc4,d,2021-03-02 07:17:00,2021-03-01 19:37:00\n";
  const std::string f2 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc3,d,2021-03-01 08:56:00,2021-03-01 08:56:00\nc0,d,2021-03-01 19:54:00,2021-03-01 18:24:00\nc2,c,2021-03-01 18:58:00,2021-03-01 19:28:00\nc1,a,2021-03-01 20:29:00,2021-03-01 20:29:00\nc1,a,2021-03-01 19:59:00,\nc0,b,2021-03-01 08:24:00,2021-03-01 06:54:00\nc0,a,2021-03-01 09:54:00,2021-03-01 09:24:00\nc0,d,2021-03-01 08:54:00,2021-03-01 08:54:00\nc3,c,2021-03-01 18:56:00,2021-03-01 18:26:00\nc1,b,2021-03-01 09:59:00,2021-03-01 09:29:00\nc3,a,2021-03-01 19:01:00,2021-03-01 19:01:00\nc2,b,2021-03-01 18:58:00,2021-03-01 18:53:00\nc0,b,2021-03-01 09:24:00,\n";
  const std::string f3 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc2,b,2021-03-01 08:45:00,2021-03-01 08:46:00\nc3,a,2021-03-01 09:32:00,2021-03-01 09:02:00\nc3,b,2021-03-01 08:27:00,2021-03-01 08:28:00\nc3,b,2021-03-01 10:02:00,2021-02-28 22:22:00\nc2,a,2021-03-01 08:46:00,2021-03-01 08:46:00\nc1,d,2021-03-01 08:36:00,2021-03-01 08:06:00\nc4,d,2021-03-01 18:36:00,2021-03-01 17:06:00\nc2,b,2021-03-01 08:15:00,2021-03-01 08:15:00\nc2,c,2021-03-01 09:16:00,2021-03-01 09:11:00\nc2,b,2021-03-01 09:16:00,2021-03-01 09:11:00\nc1,c,2021-03-01 08:36:00,2021-03-01 08:37:00\nc4,b,2021-03-01 18:41:00,2021-03-01 18:41:00\nc0,c,2021-03-01 08:46:00,2021-03-01 08:46:00\nc3,b,2021-03-01 08:32:00,2021-03-01 08:32:00\n";
  const std::string f4 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:03:00,2021-03-01 08:04:00\nc0,c,2021-03-01 09:03:00,2021-03-01 09:03:00\n";
  const std::string f5 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:24:00.000+00:00\"/><float key=\"start_timestamp\" value=\"0.5\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:24:00.000+00:00\"/><float key=\"start_timestamp\" value=\"3.0\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T20:24:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:58:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:28:00.000+00:00\"/><float key=\"start_timestamp\" value=\"0.5\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:28:00.000+00:00\"/><float key=\"start_timestamp\" value=\"0.5\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:58:00.000+00:00\"/><float key=\"start_timestamp\" value=\"0.5\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:58:00.000+00:00\"/><float key=\"start_timestamp\" value=\"0.5\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:50:00.000+00:00\"/><float key=\"start_timestamp\" value=\"-2.25\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:52:00.000+00:00\"/><float key=\"start_timestamp\" value=\"1.0\"/></event>\n</trace>\n</log>\n";
  const std::string f6 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:55:00,\n";
  const std::string f7 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:42:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:42:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:59:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:59:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:59:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:09:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:39:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:44:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:45:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:46:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:47:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:48:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:48:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:41:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:41:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n</trace>\n</log>\n";
  const std::string f8 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:05:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:10:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:50:00.000+00:00\"/><string key=\"start_timestamp\" value=\"y\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:50:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:50:00.000+00:00\"/><string key=\"start_timestamp\" value=\"late\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:20:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-02T05:20:00.000+00:00\"/><string key=\"start_timestamp\" value=\"y\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-02T05:21:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:50:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:22:00.000+00:00\"/><string key=\"start_timestamp\" value=\"y\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:52:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:57:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:57:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:13:00.000+00:00\"/><string key=\"start_timestamp\" value=\"y\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:18:00.000+00:00\"/></event>\n</trace>\n</log>\n";
  const std::string f9 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 19:10:00,\nc0,a,2021-03-01 08:10:00,True\nc3,a,2021-03-01 09:49:00,False\nc0,b,2021-03-01 08:10:00,False\nc2,b,2021-03-01 09:50:00,False\nc1,a,2021-03-01 19:03:00,True\nc2,a,2021-03-01 09:55:00,False\nc0,d,2021-03-01 08:09:00,True\nc0,d,2021-03-01 09:10:00,False\nc3,b,2021-03-01 08:49:00,False\nc2,b,2021-03-01 09:50:00,\nc0,d,2021-03-01 19:10:00,False\nc1,d,2021-03-01 20:08:00,True\nc3,d,2021-03-01 10:49:00,False\nc1,b,2021-03-01 21:08:00,False\nc3,a,2021-03-01 08:19:00,True\nc2,b,2021-03-01 10:00:00,False\nc3,d,2021-03-01 09:19:00,True\nc1,a,2021-03-01 19:08:00,True\nc2,c,2021-03-01 20:00:00,\nc1,d,2021-03-01 09:03:00,True\nc2,d,2021-03-01 09:55:00,\nc1,c,2021-03-01 19:08:00,False\n";
  const std::string f10 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:17:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:47:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:57:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:52:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:58:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:53:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:58:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:28:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:58:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 18:53:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:03:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 18:58:00\"/></event>\n</trace>\n</log>\n";
  const std::string f11 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:56:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:56:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:01:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T20:01:00.000+00:00\"/><int key=\"start_timestamp\" value=\"5\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:38:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T07:08:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:38:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-01T08:33:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:38:00.000+00:00\"/><int key=\"start_timestamp\" value=\"12\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:43:00.000+00:00\"/><string key=\"start_timestamp\" value=\"early\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:39:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:39:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:09:00.000+00:00\"/><int key=\"start_timestamp\" value=\"1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:17:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event>\n</trace>\n</log>\n";
  const std::string f12 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:10:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:10:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:10:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 10:05:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T11:10:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 11:10:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:29:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:59:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:30:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 10:00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:30:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 10:00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:31:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 10:01:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:01:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:01:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 17:56:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:01:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 04:01:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:02:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 03:32:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:48:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 07:18:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:48:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:49:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:49:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:50:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:19:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-02-28 21:39:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:49:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:50:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:20:00\"/></event>\n</trace>\n</log>\n";
  const std::string f13 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:28:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:28:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:28:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:48:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:49:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:19:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:19:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:31:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:31:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:36:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:06:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:11:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n</trace>\n</log>\n";
  const std::string f14 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:16:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:16:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:21:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:21:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:26:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-02-28 20:46:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:25:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 18:25:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:25:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 03:55:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:55:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 04:55:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:55:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 04:55:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-02T14:55:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 03:15:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-02T15:25:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 13:55:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:22:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-02-28 20:42:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:52:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:47:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:52:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:22:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:53:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:48:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:53:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:53:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 06:38:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:42:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-02-28 21:02:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:12:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 08:42:00\"/></event>\n</trace>\n</log>\n";
  const std::string f15 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:27:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-02-28 20:47:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:28:00.000+00:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:14:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 06:44:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:14:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 18:09:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:15:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 18:15:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T18:16:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:16:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 04:46:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-02T04:21:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-02 04:21:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:10:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:10:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 07:40:00\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:32:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 09:32:00\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T19:32:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-01 18:02:00\"/></event>\n</trace>\n</log>\n";
  const std::string f16 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:08:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:09:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:09:00.000+00:00\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:10:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:10:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:36:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:37:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:37:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:37:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event>\n</trace>\n</log>\n";
  const std::string f17 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 09:56:00,\nc0,d,2021-03-01 10:01:00,False\nc1,d,2021-03-01 09:01:00,False\nc2,d,2021-03-01 08:26:00,True\nc2,b,2021-03-01 09:26:00,False\nc2,c,2021-03-01 09:27:00,True\nc3,b,2021-03-01 09:36:00,False\nc3,c,2021-03-01 10:06:00,False\nc3,b,2021-03-01 10:06:00,True\nc3,d,2021-03-01 10:07:00,False\nc3,b,2021-03-01 10:08:00,True\nc3,b,2021-03-01 10:08:00,False\nc4,a,2021-03-01 08:42:00,True\n";
  const std::string f18 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-03 20:00:00,9223372036854775815\nc0,c,2021-03-05 12:00:00,9223372036854775815\nc0,a,2021-03-05 22:00:00,9223372036854775812\nc0,d,2021-03-05 22:00:00,9223372036854775816\nc0,a,2021-03-07 14:00:00,9223372036854775809\nc1,a,2021-03-12 04:00:00,9223372036854775817\nc1,c,2021-03-13 00:00:00,9223372036854775814\nc1,c,2021-03-13 20:00:00,9223372036854775813\nc2,b,2021-03-08 20:00:00,9223372036854775813\nc2,d,,9223372036854775810\nc2,c,2021-03-11 08:00:00,9223372036854775814\nc2,d,2021-03-12 14:00:00,9223372036854775812\nc2,a,2021-03-13 20:00:00,9223372036854775812\nc3,d,2021-03-13 00:00:00,9223372036854775811\nc3,c,2021-03-13 20:00:00,9223372036854775809\nc3,a,2021-03-14 16:00:00,9223372036854775813\nc3,c,,9223372036854775815\nc4,d,2021-03-10 22:00:00,9223372036854775809\nc4,b,2021-03-11 18:00:00,9223372036854775813\nc5,a,2021-03-09 16:00:00,9223372036854775812\nc5,b,2021-03-10 22:00:00,9223372036854775809\nc5,b,2021-03-12 04:00:00,9223372036854775813\nc5,c,2021-03-13 00:00:00,9223372036854775817\n";
  const std::string f19 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-06 18:00:00,9223372036854775811\nc0,b,2021-03-07 14:00:00,9223372036854775812\nc1,b,2021-03-13 20:00:00,9223372036854775815\nc1,b,2021-03-13 20:00:00,9223372036854775815\nc1,a,2021-03-14 06:00:00,9223372036854775809\nc1,d,,9223372036854775815\nc1,d,2021-03-15 22:00:00,9223372036854775810\nc2,c,2021-03-12 04:00:00,9223372036854775816\nc2,c,2021-03-13 10:00:00,9223372036854775811\nc2,b,2021-03-14 16:00:00,9223372036854775814\nc3,a,2021-03-02 14:00:00,9223372036854775813\n";
  const std::string f20 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:21:00,3\nc0,a,2021-03-01 08:23:00,9223372036854775809\nc0,a,2021-03-01 08:27:00,9223372036854775809\n";
  const std::string f21 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:03:00,9223372036854775810\nc0,b,,9223372036854775809\nc0,d,2021-03-01 08:06:00,9223372036854775816\nc1,c,,9223372036854775815\nc1,c,2021-03-01 08:18:00,9223372036854775813\nc2,b,2021-03-01 08:27:00,9223372036854775812\nc2,c,2021-03-01 08:28:00,9223372036854775814\nc2,c,2021-03-01 08:31:00,9223372036854775817\nc2,b,2021-03-01 08:32:00,9223372036854775814\n";
  const std::string f22 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,c,2021-03-01 08:21:00,late\nc0,b,2021-03-01 08:23:00,2021-03-01 08:23:00\nc0,c,,2021-03-01 08:24:00\nc0,c,,NA\nc0,c,2021-03-01 08:27:00,\nc1,d,2021-03-01 08:14:00,NA\nc2,b,,2021-03-01 08:07:00\n";
  const std::string f23 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,,True\nc0,a,2021-03-01 08:28:00,True\nc0,d,2021-03-01 08:31:00,False\nc1,d,2021-03-01 08:18:00,False\nc2,a,,\n";
  const std::string f24 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,b,2021-03-01 08:05:00,late\nc1,a,2021-03-01 08:27:00,late\n";
  const std::string f25 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:14:00,late\nc0,d,2021-03-01 08:17:00,2021-03-01 08:17:00\nc0,b,2021-03-01 08:20:00,NA\nc0,a,2021-03-01 08:24:00,\nc0,a,2021-03-01 08:26:00,2021-03-01 08:24:00\nc1,d,,2021-03-01 08:27:00\nc1,c,2021-03-01 08:28:00,2021-03-01 08:26:00\nc1,a,2021-03-01 08:31:00,2021-03-01 08:31:00\nc2,b,2021-03-01 08:28:00,2021-03-01 08:26:00\nc2,a,2021-03-01 08:29:00,NA\nc3,a,2021-03-01 08:09:00,2021-03-01 08:09:00\n";
  const std::string f26 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-06 18:00:00,late\nc0,a,2021-03-07 14:00:00,\nc1,b,2021-03-11 18:00:00,\n";
  const std::string f27 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,a,2021-03-01 08:31:00,True\nc0,a,2021-03-01 08:34:00,True\nc0,c,,True\nc0,c,,True\nc1,c,2021-03-01 08:11:00,True\nc1,a,,\nc1,c,2021-03-01 08:14:00,True\nc1,d,2021-03-01 08:15:00,False\nc2,d,2021-03-01 08:14:00,False\nc2,b,,True\nc3,b,2021-03-01 08:20:00,True\nc3,b,2021-03-01 08:24:00,False\nc3,d,2021-03-01 08:26:00,False\nc4,c,2021-03-01 08:03:00,False\nc4,c,2021-03-01 08:03:00,False\nc4,d,,False\nc4,c,2021-03-01 08:08:00,False\n";
  const std::string f28 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:14:00,late\nc0,d,2021-03-01 08:14:00,\nc0,c,2021-03-01 08:16:00,NA\nc0,d,2021-03-01 08:16:00,NA\n";
  const std::string f29 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:10:00,\nc0,a,,late\nc1,d,2021-03-01 08:17:00,2021-03-01 08:16:00\nc1,d,2021-03-01 08:18:00,NA\nc2,d,2021-03-01 08:19:00,NA\nc2,d,2021-03-01 08:20:00,2021-03-01 08:17:00\nc2,a,,late\n";
  const std::string f30 = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc0,d,2021-03-01 08:08:00,True\nc0,b,2021-03-01 08:09:00,False\nc1,c,2021-03-01 08:10:00,True\nc1,a,,True\nc1,a,2021-03-01 08:17:00,True\nc2,c,2021-03-01 08:18:00,True\nc2,c,2021-03-01 08:21:00,False\nc2,d,2021-03-01 08:23:00,True\n";
  const std::vector<Case> cases = {
      {"ef  s72500.csv", "s72500.csv", f0, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "G "},
      {"ef  s72501.csv", "s72501.csv", f1, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "G a>a=1,a>b=1,a>c=1,a>d=2,b>a=4,b>b=2,b>c=1,b>d=5,c>c=1,c>d=1,d>a=1,d>b=1,d>c=1,d>d=3"},
      {"ef  s72502.csv", "s72502.csv", f2, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "G a>a=1,a>d=1,b>a=3,b>d=3,c>a=1,d>a=2,d>c=1,d>d=1"},
      {"ef  s72503.csv", "s72503.csv", f3, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "G a>b=1,a>c=1,b>a=4,b>b=4,b>c=2,d>b=1,d>c=1"},
      {"ef  s72513.csv", "s72513.csv", f4, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "G a>c=1"},
      {"ef  x72800.xes", "x72800.xes", f5, "ef", {}, "Gerr TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"ef  x72801.csv", "x72801.csv", f6, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Gerr TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"ef  x72802.xes", "x72802.xes", f7, "ef", {}, "Gerr TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"ef  x72804.xes", "x72804.xes", f8, "ef", {}, "Gerr TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"ef  x72811.csv", "x72811.csv", f9, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Gerr TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"ef  x72820.xes", "x72820.xes", f10, "ef", {}, "Gerr TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"ef  x72822.xes", "x72822.xes", f11, "ef", {}, "Gerr TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"ef  x72834.xes", "x72834.xes", f12, "ef", {}, "Gerr TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"ef  x72838.xes", "x72838.xes", f13, "ef", {}, "Gerr TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"ef  x72844.xes", "x72844.xes", f14, "ef", {}, "Gerr TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"ef  x72866.xes", "x72866.xes", f15, "ef", {}, "Gerr TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"ef  x72868.xes", "x72868.xes", f16, "ef", {}, "Gerr TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"ef  x72939.csv", "x72939.csv", f17, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "Gerr TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"ef lz s72500.csv", "s72500.csv", f0, "ef", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}}, "G b>a=2,b>b=1,b>c=2,c>a=1"},
      {"tp lazyframe/def t72009.csv", "t72009.csv", f18, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/bh t72009.csv", "t72009.csv", f18, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_business_hours", "true"}}, "Q "},
      {"tp lazyframe/sst t72009.csv", "t72009.csv", f18, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/bh_sst t72009.csv", "t72009.csv", f18, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "Q "},
      {"tp lazyframe/t_st t72009.csv", "t72009.csv", f18, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: invalid series dtype: expected `Duration`, got `u64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"tp lazyframe/sts_bh t72009.csv", "t72009.csv", f18, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: unexpected value while building Series of type Float64; found value of type Int64: 72000\n\nThis error occurred with the following context stack:\n\t[1] while constructing Series 'time:timestamp'\n\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"time:timestamp_2\").python_udf()\n"},
      {"tp lazyframe/def t72013.csv", "t72013.csv", f19, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/sst t72013.csv", "t72013.csv", f19, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/t_st t72013.csv", "t72013.csv", f19, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: invalid series dtype: expected `Duration`, got `u64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"tp lazyframe/def t72024.csv", "t72024.csv", f20, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/sst t72024.csv", "t72024.csv", f20, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/t_st t72024.csv", "t72024.csv", f20, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: invalid series dtype: expected `Duration`, got `u64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"tp lazyframe/sts_bh t72024.csv", "t72024.csv", f20, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "Q a>a=240/0,b>a=240/169.706"},
      {"tp lazyframe/def t72025.csv", "t72025.csv", f21, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/sst t72025.csv", "t72025.csv", f21, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: - not allowed on u64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n        SELECT [col(\"case:concept:name\"), col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"@@index\")]\n          DF [\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"start_timestamp\", ...]; PROJECT */6 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp lazyframe/t_st t72025.csv", "t72025.csv", f21, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "lazyframe"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: invalid series dtype: expected `Duration`, got `u64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"tp dataframe/def t68506.csv", "t68506.csv", f22, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh t68506.csv", "t68506.csv", f22, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"tp dataframe/sst t68506.csv", "t68506.csv", f22, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh_sst t68506.csv", "t68506.csv", f22, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"tp dataframe/def t68509.csv", "t68509.csv", f23, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/bh t68509.csv", "t68509.csv", f23, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/sst t68509.csv", "t68509.csv", f23, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/bh_sst t68509.csv", "t68509.csv", f23, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/def t68513.csv", "t68513.csv", f24, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh t68513.csv", "t68513.csv", f24, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/sst t68513.csv", "t68513.csv", f24, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh_sst t68513.csv", "t68513.csv", f24, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/def t68536.csv", "t68536.csv", f25, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh t68536.csv", "t68536.csv", f25, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"tp dataframe/sst t68536.csv", "t68536.csv", f25, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh_sst t68536.csv", "t68536.csv", f25, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"tp dataframe/def t68541.csv", "t68541.csv", f26, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh t68541.csv", "t68541.csv", f26, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/sst t68541.csv", "t68541.csv", f26, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"tp dataframe/bh_sst t68541.csv", "t68541.csv", f26, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/def t68547.csv", "t68547.csv", f27, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/bh t68547.csv", "t68547.csv", f27, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/sst t68547.csv", "t68547.csv", f27, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/bh_sst t68547.csv", "t68547.csv", f27, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'bool'"},
      {"tp dataframe/bh t68563.csv", "t68563.csv", f28, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/bh_sst t68563.csv", "t68563.csv", f28, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/bh t68586.csv", "t68586.csv", f29, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp dataframe/bh_sst t68586.csv", "t68586.csv", f29, "tp", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"mining_input_shape", "dataframe"}, {"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"va start_timestamp t68500.csv", "t68500.csv", f30, "va", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"dfg_statistical_variants", "native"}, {"dfg_activity_attribute", "start_timestamp"}}, "N False>True=1,True>False=2,True>True=1"},
  };
  for (const auto& c : cases) check(c.label, outcome(client, c.name, c.content, c.kind, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GW server checks passed\n";
  return 0;
}
