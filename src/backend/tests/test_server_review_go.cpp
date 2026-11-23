#undef NDEBUG
// Review GO: pm4py's heuristics miners and Declare conformance on XES uploads'
// frame shapes with typed timestamp and start columns, and the token replay
// of a CSV upload's log shape keyed by a column with gaps.
//
// - Heuristics++ on a pandas or Polars frame whose time:timestamp or
//   start_timestamp column holds other XES kinds (as read_xes types them):
//   to_interval subtracts each event's cells (numbers lack total_seconds,
//   text, None and dicts do not subtract, pandas' NaT does), the abstraction
//   sorts by the start cells and compares complete <= start, then reads
//   timestamp() of each; a lifecycle gap raises lower() only where the
//   transitions are read in order (Minera raised "requires a valid event
//   timestamp", or lower() before an earlier subtraction).
// - Declare conformance on an XES upload's Polars frame checks the columns
//   as read_xes types them, as discovery does (Minera refused a dates
//   column whose other cells are valueless lists).
// - The CSV log shape keyed by a column with gaps: convert_to_event_log keeps
//   NaN and NaT in each event, so the replay sorts the net's transitions by
//   name (a NaN node beside a text one raises), then the variants by count
//   and tuple (NaN beside text raises), and replays the cells, dates as UTC
//   Timestamps (Minera raised KeyError, or missed every date transition).
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-go: tc/mkgo.py, ct/optval.py, ct/dcval.py,
// ct/csvk.py).
//
// Usage: minera_server_review_go_tests <minera_server>

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

// A small JSON reader: enough for /api/mine's family results.
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

std::string join(std::vector<std::string> parts, const std::string& by) {
  std::sort(parts.begin(), parts.end());
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string heuristics(const Json& activities_json, const Json& edges_json, const std::string& null_label) {
  std::vector<std::string> activities, edges;
  for (const auto& a : activities_json.items) activities.push_back(a.kind == Json::Kind::kNull ? null_label : a.text);
  for (const auto& e : edges_json.items)
    edges.push_back(activities[std::stoul(e["source"].text)] + ">" + activities[std::stoul(e["target"].text)]);
  return "A[" + join(activities, ",") + "] E" + join(edges, " ");
}

std::string python_bool(const Json& value) { return value.text == "true" ? "True" : "False"; }

using Fields = std::vector<std::pair<std::string, std::string>>;

// One case's outcome in mkgo.py's canonical form.
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& shape, const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items;
  std::string route = "/api/mine";
  if (kind == "declc") {
    route = "/api/declare-workbench";
    items = {{"model_file", R"({"rules": {"response": [{"first": "a", "second": "b", "binary": true, "support": 1, "confidence": 1}]}})", "m.json", "application/json"},
             {"input_file", content, name, "application/octet-stream"},
             {"operation", "conformance", "", ""},
             {"mining_input_shape", shape, "", ""}};
  } else {
    items = {{"log", content, name, "application/octet-stream"}, {"include_traces", "false", "", ""}};
    if (kind == "mine") items.push_back({"mining_input_shape", shape, "", ""});
  }
  for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  const auto response = client.Post(route, items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  if (kind == "declc") {
    const auto& error = body["error"];
    if (body["result"].kind == Json::Kind::kNull || error.kind != Json::Kind::kNull)
      return "error: " + (error.kind == Json::Kind::kObject ? error["message"].text : error.text);
    std::vector<std::string> traces;
    for (const auto& r : body["result"]["value"].items)
      traces.push_back(r["no_constr_total"].text + "/" + r["no_dev_total"].text + "/" + python_bool(r["is_fit"]));
    std::string out;
    for (std::size_t k = 0; k < traces.size(); ++k) out += (k ? ";" : "") + traces[k];
    return out;
  }
  if (kind == "mine") {
    const auto& x = body["family_results"]["heuristic"];
    if (x.kind != Json::Kind::kObject) return "no family result: " + response->body.substr(0, 200);
    if (x["executed"].text != "true") {
      const auto& type = x["error"]["type"].text;
      return "error: " + (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + x["error"]["message"].text;
    }
    return heuristics(x["result"]["activities"], x["result"]["edges"], shape == "dataframe" ? "nan" : "None");
  }
  if (body["error"].kind != Json::Kind::kNull) {
    std::string message = body["error"].text;
    if (message.starts_with("Heuristics Miner: ")) message = message.substr(18);
    return "error: " + message;
  }
  const auto& t = body["conformance"]["heuristic"]["token_replay"];
  return heuristics(body["heuristics_net"]["activities"], body["heuristics_net"]["edges"], "None") + " T" +
         t["fitting_traces"].text + "/" + t["produced"].text + "/" + t["consumed"].text + "/" + t["missing"].text + "/" +
         t["remaining"].text;
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
    std::string label, name, content, shape, kind;
    Fields fields;
    std::string expected;
  };
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event></trace>\n</log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:05:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:05:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"66\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:05:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:13:00.000+01:00\"/></event></trace>\n</log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:17:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T11:18:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:44:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:45:00.000-05:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:40:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:40:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:40:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:24:00.000-05:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 09:24:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:24:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:24:00.000+05:30\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T18:47:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 18:46:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\" value=\"2021-03-05 18:46:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:46:00.000+05:30\"/></event></trace>\n</log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05 08:25:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"p\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"2021-03-05 09:25:00\"/></event></trace>\n</log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:18:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"22.5\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"27.5\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:16:00.000-05:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"37.5\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:47:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"142.0\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:52:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"-2.0\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:06:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"636.0\"/><date key=\"start_timestamp\" value=\"2021-03-05T18:36:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"start_timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/><list key=\"start_timestamp\"/></event></trace>\n</log>\n";
  const std::string in7 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:33:00.000-05:00\"/><string key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><int key=\"start_timestamp\" value=\"92\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:10:00.000+00:00\"/><int key=\"start_timestamp\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/></event></trace>\n</log>\n";
  const std::string in8 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><int key=\"time:timestamp\" value=\"0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"-2.0\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"-2.0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"12\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"6\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"72.0\"/></event></trace>\n</log>\n";
  const std::string in9 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:13:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:42:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:30:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:30:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in10 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><id key=\"time:timestamp\" value=\"x1\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:36:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><id key=\"time:timestamp\" value=\"x1\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:36:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:07:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:06:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><id key=\"time:timestamp\" value=\"x1\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:37:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:39:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><id key=\"time:timestamp\" value=\"x1\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:39:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:40:00.000+01:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:39:00.000-05:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:25:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:25:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:25:00.000+01:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><id key=\"time:timestamp\" value=\"x1\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:24:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in11 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:09:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"true\"/><int key=\"start_timestamp\" value=\"1\"/></event></trace>\n</log>\n";
  const std::string in12 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"x1\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event></trace>\n</log>\n";
  const std::string in13 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"s043\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"p\"/><id key=\"start_timestamp\" value=\"x1\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"q\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"s111\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><id key=\"start_timestamp\" value=\"2021-03-05T08:00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"n/a\"/><id key=\"start_timestamp\" value=\"x1\"/></event><event><id key=\"start_timestamp\" value=\"x1\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"s086\"/><id key=\"start_timestamp\" value=\"x1\"/></event></trace>\n</log>\n";
  const std::string in14 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"suspend\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n</log>\n";
  const std::string in15 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05 09:30:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"2021-03-05T10:30:00\"/><string key=\"lifecycle:transition\" value=\"suspend\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"120.0\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05 10:00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"2021-03-05T10:01:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05T20:01:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"17.5\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"2021-03-05T09:07:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"14.0\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"2021-03-05 09:14:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n</log>\n";
  const std::string in16 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"690.0\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-06T05:30:00.000+01:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-06T06:30:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"33.5\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"33.5\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><date key=\"time:timestamp\" value=\"2021-03-05T10:19:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T20:19:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T20:24:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-06T06:24:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n</log>\n";
  const std::string in17 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"4\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><int key=\"time:timestamp\" value=\"4\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"0\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"27\"/></event><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"2\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"62\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"627\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n</log>\n";
  const std::string in18 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><event><list key=\"time:timestamp\"/></event></trace>\n</log>\n";
  const std::string in19 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"-2.0\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"95.0\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><id key=\"time:timestamp\" value=\"x1\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"a\"/><id key=\"time:timestamp\" value=\"x1\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"47.5\"/><string key=\"lifecycle:transition\" value=\"suspend\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"48.5\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"648.0\"/><string key=\"lifecycle:transition\" value=\"suspend\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/></event></trace>\n</log>\n";
  const std::string in20 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:27:00.000+05:30\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:28:00.000-05:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:01:00.000+05:30\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:02:00.000+01:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:10:00.000-05:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event></trace>\n</log>\n";
  const std::string in21 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T18:35:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T18:36:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:17:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:38:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:38:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:38:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in22 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"3\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in23 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:12:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:04:00.000-05:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"true\"/><date key=\"start_timestamp\" value=\"2021-03-06T04:16:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/><date key=\"start_timestamp\" value=\"2021-03-06T14:15:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in24 = "case:concept:name,concept:name,ts,lifecycle:transition,concept:instance,org:resource,start_timestamp,st2\nc0,c,2021-03-01 09:11:00,True,,y,2021-03-01T09:11:00+01:00,2021-03-01 09:11:00\nc0,c,,True,001,y,,\nc1,b,2021-03-01 08:25:00,True,1,x,2021-03-01T08:25:00+01:00,2021-03-01 08:25:00\nc1,b,2021-03-01 09:25:00,1,2,z,2021-03-01T09:25:00+01:00,2021-03-01 09:25:00\nc1,a,2021-03-01 13:25:00,True,x,y,2021-03-01T13:25:00+01:00,2021-03-01 12:25:00\nc1,a,2021-03-02 10:25:00,2021-03-01 10:25:00,1.0,x,2021-03-02T10:25:00+01:00,2021-03-02 10:25:00\nc1,a,2021-03-02 11:25:00,NA,x,x,2021-03-02T11:25:00+01:00,2021-03-02 11:25:00\n";
  const std::string in25 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,st2\nc0,b,2021-03-05 08:16:00,complete,1,z,2021-03-05 08:16:00\nc0,a,2021-03-08 08:16:00,complete,1,y,2021-03-08 08:16:00\nc1,d,2021-03-03 08:05:00,,1,x,2021-03-03 08:05:00\nc2,c,,start,1,z,\nc2,c,2021-03-10 08:02:00,complete,1,x,2021-03-10 08:02:00\nc2,a,2021-03-14 08:02:00,start,1,z,2021-03-14 08:02:00\nc2,a,2021-03-14 08:02:00,complete,1,z,\nc2,c,2021-03-17 08:02:00,start,1,z,\nc2,c,2021-03-20 08:02:00,complete,1,y,2021-03-20 08:02:00\nc2,b,,complete,1,x,2021-03-27 08:02:00\nc2,a,,start,1,x,\nc2,a,2021-03-31 08:02:00,complete,1,y,2021-03-31 08:02:00\nc3,a,2021-03-05 08:10:00,start,1,y,2021-03-05 08:10:00\nc3,a,2021-03-07 08:10:00,complete,1,y,2021-03-07 08:10:00\nc3,a,2021-03-11 08:10:00,start,,z,2021-03-11 08:10:00\nc3,a,2021-03-14 08:10:00,complete,1,y,2021-03-14 08:10:00\nc3,d,2021-03-15 08:10:00,start,1,x,\nc3,d,2021-03-18 08:10:00,complete,1,y,\n";
  const std::string in26 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,start_timestamp,st2\nc0,b,2021-03-01 11:28:00,2021-03-01 11:28:00,2,z,2021-03-01 11:28:00.696,2021-03-01 11:28:00\nc1,d,2021-03-01 12:03:00,True,2,z,2021-03-01 12:03:00.991,None\nc0,b,,2021-03-01 11:28:00,2,y,2021-03-01 11:28:00.696,2021-03-01 11:28:00\n";
  const std::string in27 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc0,a,2021-03-01 08:21:00,start,01,z,2021-03-01 08:19:00\nc0,a,,complete,+1,z,2021-03-01 08:24:00\n";
  const std::string in28 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 09:20:00,,,2021-03-01 08:00:00,3,,1\nc1,a,2021-03-01 08:27:00,r,,2021-03-03 10:00:00,2,True,\nc2,b,2021-03-01 08:41:00,p,,2021-03-03 10:00:00,2,True,\n";
  const std::string in29 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:14:00,p,3.0,2021-03-02 09:30:00,3,False,a\nc1,a,2021-03-01 09:44:00,,2.5,2021-03-02 09:30:00,1,True,a\nc2,b,2021-03-01 09:42:00,p,3.0,,,True,1\nc3,a,2021-03-01 08:34:00,p,1.5,2021-03-02 09:30:00,3,,a\nc4,b,2021-03-01 09:04:00,q,3.0,2021-03-02 09:30:00,,False,\n";
  const std::string in30 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:28:00,,2.5,2021-03-01 08:00:00,2,False,\nc1,c,2021-03-01 08:12:00,r,,2021-03-03 10:00:00,,False,1\nc2,c,2021-03-01 09:36:00,p,-1.0,2021-03-01 08:00:00,3,False,b\nc3,b,2021-03-01 09:04:00,q,1.5,2021-03-01 08:00:00,,True,\nc4,c,2021-03-01 08:53:00,,1.5,2021-03-02 09:30:00,1,True,b\nc5,a,2021-03-01 08:15:00,s,-1.0,,,,b\n";
  const std::string in31 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:50:00,q,2.5,2021-03-01 08:00:00,2,True,\nc1,a,2021-03-01 08:46:00,p,2.5,,1,True,1\n";
  const std::string in32 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 09:44:00,p,,2021-03-01 08:00:00,1,True,\nc0,c,2021-03-01 09:45:00,q,2.5,2021-03-01 08:00:00,2,False,a\nc1,a,2021-03-01 08:47:00,,2.5,,2,False,1\nc2,b,2021-03-01 08:31:00,q,,2021-03-01 08:00:00,1,,a\nc2,c,2021-03-01 08:36:00,q,2.5,2021-03-01 08:00:00,2,True,\nc3,b,2021-03-01 08:52:00,q,2.5,,2,,a\nc4,a,2021-03-01 09:50:00,p,1.5,2021-03-01 08:00:00,2,False,\nc4,c,2021-03-01 10:50:00,q,2.5,,2,False,1\nc4,b,2021-03-01 10:55:00,,2.5,2021-03-01 08:00:00,2,False,1\nc5,c,2021-03-01 08:36:00,q,2.5,,2,False,1\nc5,b,2021-03-01 09:36:00,,1.5,2021-03-01 08:00:00,2,True,\n";
  const std::vector<Case> cases = {
      {"hpp_err_msg g40000.xes", "g40000.xes", in0, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'NoneType'"},
      {"hpp_err_msg g40001.xes", "g40001.xes", in1, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: AttributeError: 'int' object has no attribute 'total_seconds'"},
      {"hpp_err_msg g40002.xes", "g40002.xes", in2, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'dict' and 'dict'"},
      {"hperf_nat_only g40003.xes", "g40003.xes", in3, "dataframe", "mine", {{"heuristic_decoration", "performance"}}, "A[a,b,c] Eb>a b>b c>a"},
      {"hpp_err_msg g40005.xes", "g40005.xes", in4, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"hpp_err_msg g40007.xes", "g40007.xes", in5, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'float' and 'Timestamp'"},
      {"hperf_ref_only g40012.xes", "g40012.xes", in6, "dataframe", "mine", {{"heuristic_decoration", "performance"}}, "error: AttributeError: 'float' object has no attribute 'total_seconds'"},
      {"hpp_err_msg g40014.xes", "g40014.xes", in7, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'Timestamp' and 'float'"},
      {"hpp_err_msg g40016.xes", "g40016.xes", in8, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: AttributeError: 'float' object has no attribute 'total_seconds'"},
      {"hpp_err_msg g40020.xes", "g40020.xes", in9, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: ValueError: NaTType does not support timestamp"},
      {"hpp_err_msg g40021.xes", "g40021.xes", in10, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'str' and 'Timestamp'"},
      {"hpp_err_msg g40023.xes", "g40023.xes", in11, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'int' and 'datetime.datetime'"},
      {"hpp_err_msg g40032.xes", "g40032.xes", in12, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'NoneType' and 'str'"},
      {"hperf_err_msg g40032.xes", "g40032.xes", in12, "dataframe", "mine", {{"heuristic_decoration", "performance"}}, "error: TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'"},
      {"hpp_err_msg g40000.xes", "g40000.xes", in0, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'NoneType'"},
      {"hpp_err_msg g40001.xes", "g40001.xes", in1, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"hperf_err_msg g40003.xes", "g40003.xes", in3, "lazyframe", "mine", {{"heuristic_decoration", "performance"}}, "error: TypeError: unsupported operand type(s) for -: 'NoneType' and 'datetime.datetime'"},
      {"hpp_err_msg g40007.xes", "g40007.xes", in5, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'float' and 'datetime.datetime'"},
      {"hpp_err_msg g40010.xes", "g40010.xes", in13, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<=' not supported between instances of 'NoneType' and 'str'"},
      {"hpp_err_msg g40014.xes", "g40014.xes", in7, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: '<' not supported between instances of 'float' and 'NoneType'"},
      {"hpp_err_msg g40016.xes", "g40016.xes", in8, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: AttributeError: 'float' object has no attribute 'total_seconds'"},
      {"hpp_err_msg g40020.xes", "g40020.xes", in9, "lazyframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'"},
      {"hpp_err_msg g43014.xes", "g43014.xes", in14, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'"},
      {"hpp_err_msg g43015.xes", "g43015.xes", in15, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"hpp_err_msg g43044.xes", "g43044.xes", in16, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: AttributeError: 'float' object has no attribute 'total_seconds'"},
      {"hpp_err_msg g43169.xes", "g43169.xes", in17, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: AttributeError: 'int' object has no attribute 'total_seconds'"},
      {"hpp_err_msg g43555.xes", "g43555.xes", in18, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'"},
      {"hpp_err_msg g43568.xes", "g43568.xes", in19, "dataframe", "mine", {{"heuristic_variant", "plusplus"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"declare conformance g40040", "g40040.xes", in20, "lazyframe", "declc", {}, "1/1/False;1/1/False;1/0/True"},
      {"declare conformance g40091", "g40091.xes", in21, "lazyframe", "declc", {}, "1/0/True;1/0/True;1/0/True"},
      {"declare conformance g40022", "g40022.xes", in22, "dataframe", "declc", {}, "error: the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity."},
      {"declare conformance g40142", "g40142.xes", in23, "dataframe", "declc", {}, "1/1/False;1/0/True;1/1/False;1/1/False"},
      {"csv log j13663 keyed concept:instance", "j13663.csv", in24, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"heuristic_activity_attribute", "concept:instance"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"csv log j13675 keyed lifecycle:transition", "j13675.csv", in25, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "lifecycle:transition"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"csv log j13690 keyed st2", "j13690.csv", in26, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "st2"}}, "A[2021-03-01 11:28:00+00:00,NaT] E T2/4/4/0/0"},
      {"csv log j13695 keyed time:timestamp", "j13695.csv", in27, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "time:timestamp"}}, "A[2021-03-01 08:21:00+00:00] E T1/2/2/0/0"},
      {"csv log k47003 keyed K1", "k47003.csv", in28, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"csv log k47025 keyed K6", "k47025.csv", in29, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"csv log k47042 keyed K1", "k47042.csv", in30, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'str' and 'float'"},
      {"csv log k47003 keyed K5", "k47003.csv", in28, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}}, "A[True,nan] E T3/6/6/0/0"},
      {"csv log k47025 keyed K5", "k47025.csv", in29, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K5"}}, "A[False,True,nan] E T5/10/10/0/0"},
      {"csv log k47016 keyed K3", "k47016.csv", in31, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}}, "A[2021-03-01 08:00:00+00:00,NaT] E T2/4/4/0/0"},
      {"csv log k47025 keyed K3", "k47025.csv", in29, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K3"}}, "A[2021-03-02 09:30:00+00:00,NaT] E T5/10/10/0/0"},
      {"csv log k47010 keyed K1", "k47010.csv", in32, "log", "csvlog", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
  };
  for (const auto& c : cases)
    check(c.label + " (" + c.shape + ")", outcome(client, c.name, c.content, c.shape, c.kind, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GO server checks passed\n";
  return 0;
}
