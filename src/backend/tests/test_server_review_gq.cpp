#undef NDEBUG
// Review GQ: the temporal profile's conformance on XES uploads' frame shapes,
// /api/mine's footprints family on XES frame sessions, a CSV upload's log
// shape keyed by a float64 column with several gaps, and transition systems
// keyed by a typed column of an XES frame.
//
// - Temporal profile conformance on a frame session reads the frame as the
//   discovery does (round GP), the case column first (pandas KeyError, a
//   Polars select failure), with business hours per pair row; Polars plan
//   errors past the join name the join's plan; the route's zeta is 1.0.
// - /api/mine's footprints family on a frame session reads only the models
//   its selection needs and the frame as pm4py's converted EventLog (a gap
//   one more activity, traces without a case id one trace).
// - A CSV log keyed by a float64 column: each gap is its own NaN object, so
//   pm4py's heuristics miner names one node per gap (generalization and
//   simplicity count them).
// - A transition system keyed by a typed column: str() names each value,
//   Python's == matches labels (a NaN or NaT never), a state's list or set
//   matches a NaN by its object; pandas' set view hashes a dict cell.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gq: tc/mkgq.py, ct/tpcval.py, ct/fpmine.py,
// ct/csvk.py, ct/tsgraph.py).
//
// Usage: minera_server_review_gq_tests <minera_server>

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

const char* const kProfile =
    "[[\"a\", \"b\", 60.0, 10.0], [\"a\", \"c\", 300.0, 0.0], [\"b\", \"c\", 30.0, 5.0], [\"c\", \"d\", 10.0, 100.0], "
    "[\"b\", \"a\", 3600.0, 60.0], [\"a\", \"a\", 0.0, 1.0], [\"d\", \"a\", 120.0, 30.0], [\"b\", \"d\", 600.0, 0.0]]";

Json post_json(httplib::Client& client, const std::string& path, const httplib::Params& params) {
  const auto response = client.Post(path, params);
  if (!response) return Json{};
  JsonReader reader{response->body};
  return reader.value();
}

// One case's outcome in mkgq.py's canonical form.
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& shape, const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""}};
  if (kind != "csvk") items.push_back({"mining_input_shape", shape, "", ""});
  if (kind == "csvk" || kind == "fpm")
    for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  if (kind == "csvk") {
    if (body["error"].kind != Json::Kind::kNull) return "error: " + error_text(body["error"]);
    std::vector<std::string> activities;
    for (const auto& a : body["heuristics_net"]["activities"].items) activities.push_back(a.text);
    const auto& t = body["conformance"]["heuristic"]["token_replay"];
    return "A[" + sorted_join(activities, ",") + "] T" + t["fitting_traces"].text + "/" + t["produced"].text + "/" +
           t["consumed"].text + "/" + t["missing"].text + "/" + t["remaining"].text + " G " +
           number(t["generalization"], "%.6f") + "/" + number(t["simplicity"], "%.6f") + "/" + number(t["precision"], "%.6f");
  }
  if (kind == "fpm") {
    const auto& x = body["family_results"]["footprints"];
    if (x.kind != Json::Kind::kObject) return "no family result: " + response->body.substr(0, 200);
    if (x["executed"].text != "true") return "error: " + error_text(x["error"]);
    std::vector<std::string> names;
    for (const auto& a : body["analysis"]["activities"].items) names.push_back(name_of(a["name"]));
    const auto at = [&](const Json& index) { return names[std::stoul(index.text)]; };
    const auto list = [&](const Json& indices) {
      std::vector<std::string> out;
      for (const auto& i : indices.items) out.push_back(at(i));
      return sorted_join(out, ",");
    };
    const auto pairs = [&](const Json& edges, bool counted) {
      std::vector<std::string> out;
      for (const auto& e : edges.items)
        out.push_back(at(e["source"]) + ">" + at(e["target"]) + (counted ? "=" + e["count"].text : std::string()));
      return sorted_join(out, ",");
    };
    const auto& footprints = body["analysis"]["footprints"];
    const auto& log = footprints["discovery"]["entire_event_log"];
    std::string flags;
    const auto& traces = footprints["conformance"]["trace_extensive"].items;
    for (const auto& t : traces) flags += t["is_footprints_fit"].text == "true" ? "1" : "0";
    const auto& metrics = footprints["metrics"];
    return "L " + list(log["activities"]) + "|" + list(log["start_activities"]) + "|" + list(log["end_activities"]) + "|" +
           pairs(log["dfg"], true) + " V " + pairs(footprints["conformance"]["log_model"]["violations"], false) + " T " +
           std::to_string(traces.size()) + "/" + flags + " P " + number(metrics["precision"], "%.6f") + "/" +
           number(metrics["log_fitness"], "%.6f") + "/" + number(metrics["perc_fit_traces"], "%.6f");
  }
  const auto source_id = body["source_id"].text;
  httplib::Params params;
  if (kind == "tpc") {
    params = {{"family", "temporal_conformance"}, {"source_id", source_id}, {"variant", shape},
              {"temporal_profile_json", kProfile}, {"include_traces", "false"}};
  } else {
    params = {{"family", "transition_system"}, {"source_id", source_id}};
  }
  for (const auto& [key, value] : fields) params.emplace(key, value);
  const auto out = post_json(client, kind == "tpc" ? "/api/analyze-behavior" : "/api/discover-graph", params);
  if (out.kind != Json::Kind::kObject) return "no response";
  if (out["error"].kind != Json::Kind::kNull) return "error: " + error_text(out["error"]);
  if (kind == "tpc") {
    std::vector<std::string> traces;
    for (const auto& t : out["conformance"]["traces"].items) {
      std::vector<std::string> deviations;
      for (const auto& x : t.items)
        deviations.push_back(name_of(x["source"]) + ">" + name_of(x["target"]) + ":" + number(x["elapsed_seconds"], "%.6g") + "/" +
                             number(x["standard_deviation_distance"], "%.6g"));
      traces.push_back(plain_join(deviations, ","));
    }
    return shape == "lazyframe" ? sorted_join(traces, "|") : plain_join(traces, "|");
  }
  const auto& result = out["native_result"];
  const bool unordered = result["options"]["view"].text != "sequence";
  const auto state = [&](const Json& payload) {
    std::vector<std::string> values;
    for (const auto& v : payload.items) values.push_back(name_of(v));
    return "(" + (unordered ? sorted_join(values, ",") : plain_join(values, ",")) + ")";
  };
  std::map<std::string, std::string> state_of;
  std::vector<std::string> states, transitions;
  for (const auto& s : result["states"].items) {
    state_of[s["id"].text] = state(s["payload"]);
    if (s["in_root"].text != "false") states.push_back(state(s["payload"]));
  }
  for (const auto& t : result["transitions"].items)
    transitions.push_back(state_of[t["source"].text] + ">" + state_of[t["target"].text] + ":" + name_of(t["label"]));
  return "S " + sorted_join(states, " ") + " T " + sorted_join(transitions, " ");
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
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/></event><event><float key=\"time:timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"b\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/></event><event><string key=\"concept:name\" value=\"b\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><int key=\"time:timestamp\" value=\"7\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:12:00.000-05:00\"/><int key=\"start_timestamp\" value=\"6\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"77\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"7\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:07:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 09:07:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:07:00.000+01:00\"/><int key=\"start_timestamp\" value=\"66\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:12:00.000-05:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 09:12:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:30:00.000+01:00\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"4\"/><int key=\"start_timestamp\" value=\"1\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:06:00.000+00:00\"/><list key=\"start_timestamp\"/></event></trace>\n</log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"7.5\"/></event></trace>\n</log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:25:00.000+00:00\"/><int key=\"start_timestamp\" value=\"0\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><int key=\"start_timestamp\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T18:23:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/><int key=\"start_timestamp\" value=\"59\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:09:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:44:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:43:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:44:00.000+00:00\"/><int key=\"start_timestamp\" value=\"163\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:49:00.000-05:00\"/><int key=\"start_timestamp\" value=\"168\"/></event></trace>\n</log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"27.0\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"32.0\"/></event></trace>\n</log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:31:00.000+01:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:03:00.000+05:30\"/></event></trace>\n</log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:49:00.000-05:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"-2.0\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"169.0\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"144.0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"76.0\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:21:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in7 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"time:timestamp\"/><int key=\"start_timestamp\" value=\"11\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><int key=\"start_timestamp\" value=\"9\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><float key=\"start_timestamp\" value=\"45.5\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><int key=\"start_timestamp\" value=\"4\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:40:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:25:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:55:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:25:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in8 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:01:00.000-05:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"0\"/><date key=\"start_timestamp\" value=\"2021-03-06T05:00:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:15:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"0\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:45:00.000+01:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:31:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"suspend\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"6\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:36:00.000-05:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:07:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:01:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:31:00.000+00:00\"/><string key=\"lifecycle:transition\" value=\"complete\"/></event></trace>\n</log>\n";
  const std::string in9 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"3\"/><string key=\"lifecycle:transition\" value=\"start\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list><string key=\"lifecycle:transition\" value=\"start\"/><string key=\"concept:instance\" value=\"1\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"1\"/><string key=\"lifecycle:transition\" value=\"suspend\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n</log>\n";
  const std::string in10 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"34.0\"/><string key=\"start_timestamp\" value=\"q\"/><string key=\"lifecycle:transition\" value=\"start\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"44.5\"/><float key=\"start_timestamp\" value=\"64.0\"/><string key=\"lifecycle:transition\" value=\"start\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><string key=\"start_timestamp\" value=\"p\"/><string key=\"lifecycle:transition\" value=\"complete\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><boolean key=\"start_timestamp\" value=\"false\"/><string key=\"lifecycle:transition\" value=\"complete\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><boolean key=\"start_timestamp\" value=\"false\"/><string key=\"lifecycle:transition\" value=\"start\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"-2.0\"/><string key=\"start_timestamp\" value=\"n/a\"/><string key=\"lifecycle:transition\" value=\"complete\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/><string key=\"start_timestamp\" value=\"q\"/><string key=\"lifecycle:transition\" value=\"start\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\"/><string key=\"start_timestamp\" value=\"p\"/><string key=\"lifecycle:transition\" value=\"complete\"/><string key=\"concept:instance\" value=\"1\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\"/><float key=\"start_timestamp\" value=\"77.0\"/><string key=\"lifecycle:transition\" value=\"complete\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"32.5\"/><boolean key=\"start_timestamp\" value=\"false\"/><string key=\"lifecycle:transition\" value=\"start\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n</log>\n";
  const std::string in11 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"start_timestamp\" value=\"NaN\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"1\"/></values></list><float key=\"start_timestamp\" value=\"NaN\"/><string key=\"concept:instance\" value=\"1\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list><float key=\"start_timestamp\" value=\"NaN\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"start_timestamp\" value=\"NaN\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"d\"/><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"start_timestamp\" value=\"NaN\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n</log>\n";
  const std::string in12 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 09:31:00,,1.5,2021-03-02 09:30:00,,False,b\nc1,b,2021-03-01 09:00:00,p,3.0,2021-03-01 08:00:00,,False,a\n";
  const std::string in13 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:38:00,p,3.0,2021-03-01 08:00:00,2,,a\nc1,b,2021-03-01 09:21:00,q,,2021-03-01 08:00:00,,True,a\nc2,a,2021-03-01 08:23:00,r,1.5,2021-03-02 09:30:00,,True,\n";
  const std::string in14 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:33:00,r,1.5,,1,False,a\nc1,b,2021-03-01 08:41:00,s,,2021-03-03 10:00:00,,True,1\nc2,b,2021-03-01 09:07:00,q,,2021-03-03 10:00:00,,False,b\nc3,c,2021-03-01 09:13:00,r,1.5,2021-03-02 09:30:00,7,False,b\n";
  const std::string in15 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:08:00,,3.0,2021-03-02 09:30:00,,,1\nc1,b,2021-03-01 09:44:00,r,3.0,2021-03-01 08:00:00,,,\nc2,c,2021-03-01 08:22:00,r,3.0,2021-03-02 09:30:00,2,,1\nc3,a,2021-03-01 08:35:00,,3.0,2021-03-02 09:30:00,,True,\n";
  const std::string in16 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,q,1.5,2021-03-02 09:30:00,,False,1\nc1,a,2021-03-01 09:43:00,r,3.0,2021-03-02 09:30:00,2,True,\nc2,b,2021-03-01 08:15:00,q,,,1,False,1\nc3,c,2021-03-01 09:05:00,p,,2021-03-02 09:30:00,3,,\nc4,b,2021-03-01 08:38:00,q,,2021-03-01 08:00:00,3,False,1\nc5,a,2021-03-01 08:17:00,,2.5,2021-03-02 09:30:00,,,\n";
  const std::string in17 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:01:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T18:32:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:02:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:49:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:23:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:29:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T20:29:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::vector<Case> cases = {
      {"def g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"bh g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"zeta g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_zeta", "0.5"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"sts g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"bh_sts g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"sst g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"bh_sst g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"t_st g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"bh_t_st g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"start g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_source_timestamp_attribute", "time:timestamp"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"szzz g60000.xes", "g60000.xes", in0, "dataframe", "tpc", {{"temporal_target_timestamp_attribute", "zzz"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"sts g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"bh_sts g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"sst g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"bh_sst g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"t_st g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'int' and 'str'"},
      {"bh_t_st g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'int' and 'str'"},
      {"start g60001.xes", "g60001.xes", in1, "dataframe", "tpc", {{"temporal_source_timestamp_attribute", "time:timestamp"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"def g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"bh g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_business_hours", "true"}}, "|"},
      {"zeta g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_zeta", "0.5"}}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"sts g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `time:timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"bh_sts g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "|"},
      {"sst g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"bh_sst g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "|"},
      {"t_st g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: unable to find column \"start_timestamp\"; valid columns: [\"concept:name\", \"case:concept:name\", \"time:timestamp\"]\n\nDid you mean \"time:timestamp\"?\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nDF [\"concept:name\", \"case:concept:name\", \"time:timestamp\"]; PROJECT */3 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"bh_t_st g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: unable to find column \"start_timestamp\"; valid columns: [\"concept:name\", \"case:concept:name\", \"time:timestamp\"]\n\nDid you mean \"time:timestamp\"?\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nDF [\"concept:name\", \"case:concept:name\", \"time:timestamp\"]; PROJECT */3 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"start g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_source_timestamp_attribute", "time:timestamp"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `time:timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"szzz g60000.xes", "g60000.xes", in0, "lazyframe", "tpc", {{"temporal_target_timestamp_attribute", "zzz"}}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `zzz_2`\n\nThis error occurred in the following expression:\n\t[(col(\"zzz_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"bh g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_business_hours", "true"}}, "error: TypeError: str.replace() takes no keyword arguments\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"start_timestamp_2\").python_udf()\n"},
      {"sts g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp_2\")) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"time:timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"bh_sts g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"time:timestamp_2\").python_udf()\n"},
      {"sst g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"bh_sst g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"start_timestamp_2\").python_udf()\n"},
      {"t_st g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"bh_t_st g60001.xes", "g60001.xes", in1, "lazyframe", "tpc", {{"temporal_business_hours", "true"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments\n\nThis error occurred in the following expression:\n\tas_struct(\"start_timestamp\", \"start_timestamp_2\").python_udf()\n"},
      {"footprints family g60002.xes", "g60002.xes", in2, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,c|a,c|a|a>a=1,c>a=1 V  T 2/11 P 1.000000/1.000000/100.000000"},
      {"footprints family g60004.xes", "g60004.xes", in3, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c,d|b,c,d|b,d|a>d=1,c>d=1,d>a=1,d>b=1,d>d=1 V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g60005.xes", "g60005.xes", in4, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,d|a,d|a,b|a>b=1,d>a=1 V  T 2/11 P 1.000000/1.000000/100.000000"},
      {"footprints family g60006.xes", "g60006.xes", in5, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,c|a,c|a,c| V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g60014.xes", "g60014.xes", in6, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c,d|a,c,d|a,c|a>a=1,b>a=1,c>c=1,d>b=1 V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g60002.xes", "g60002.xes", in2, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,c|a,c|a|a>a=1,c>a=1 V  T 2/11 P 1.000000/1.000000/100.000000"},
      {"footprints family g60004.xes", "g60004.xes", in3, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c,d|b,c,d|b,d|a>d=1,c>d=1,d>a=1,d>b=1,d>d=1 V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g60005.xes", "g60005.xes", in4, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,d|a,d|a,b|a>b=1,d>a=1 V  T 2/11 P 1.000000/1.000000/100.000000"},
      {"footprints family g60006.xes", "g60006.xes", in5, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,c|a,c|a,c| V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g60071.xes", "g60071.xes", in7, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L None,a,b,c|None,a,c|None,b|a>b=1,b>b=1,c>b=1,c>c=2 V  T 3/011 P 1.000000/0.888889/66.666667"},
      {"footprints family g61007.xes", "g61007.xes", in8, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c,d|b,d|b,c|a>b=1,b>b=2,b>c=2,c>c=1,d>a=1 V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g61008.xes", "g61008.xes", in9, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L b,d|b|b,d|b>d=1 V  T 2/11 P 1.000000/1.000000/100.000000"},
      {"footprints family g61014.xes", "g61014.xes", in10, "dataframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c|b,c|a,b,c|a>b=1,b>a=1,b>b=1,b>c=1,c>a=1,c>b=1 V  T 4/1111 P 1.000000/1.000000/100.000000"},
      {"footprints family g61007.xes", "g61007.xes", in8, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c,d|b,d|b,c|a>b=1,b>b=2,b>c=2,c>c=1,d>a=1 V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"footprints family g61014.xes", "g61014.xes", in10, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c|b,c|a,b,c|a>b=1,b>a=1,b>b=1,b>c=1,c>a=1,c>b=1 V  T 4/1111 P 1.000000/1.000000/100.000000"},
      {"footprints family g61016.xes", "g61016.xes", in11, "lazyframe", "fpm", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}}, "L a,b,c,d|a,c|a|a>b=1,b>a=1,c>d=1,d>a=1 V  T 3/111 P 1.000000/1.000000/100.000000"},
      {"K4 key k62021.csv", "k62021.csv", in12, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}}, "A[nan,nan] T2/4/4/0/0 G 0.000000/1.000000/1.000000"},
      {"K4 key k62072.csv", "k62072.csv", in13, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}}, "A[2.0,nan,nan] T3/6/6/0/0 G 0.000000/0.714286/1.000000"},
      {"K2 key k62081.csv", "k62081.csv", in14, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}}, "A[1.5,nan,nan] T4/8/8/0/0 G 0.097631/0.714286/1.000000"},
      {"K4 key k62081.csv", "k62081.csv", in14, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}}, "A[1.0,7.0,nan,nan] T4/8/8/0/0 G 0.000000/0.600000/1.000000"},
      {"K4 key k62095.csv", "k62095.csv", in15, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}}, "A[2.0,nan,nan,nan] T4/8/8/0/0 G 0.000000/0.600000/1.000000"},
      {"K6 key k62095.csv", "k62095.csv", in15, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K6"}}, "A[1.0,nan,nan] T4/8/8/0/0 G 0.097631/0.714286/1.000000"},
      {"K2 key k62105.csv", "k62105.csv", in16, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K2"}}, "A[1.5,2.5,3.0,nan,nan,nan] T6/12/12/0/0 G 0.000000/0.500000/1.000000"},
      {"K4 key k62105.csv", "k62105.csv", in16, "log", "csvk", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"heuristic_activity_attribute", "K4"}}, "A[1.0,2.0,3.0,nan,nan] T6/12/12/0/0 G 0.058579/0.538462/1.000000"},
      {"act_ts g60000.xes", "g60000.xes", in0, "dataframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (None) (None) (None,None) (None,None) (None,None) (None,None) T (None)>():None (None)>():None (None,None)>(None):None (None,None)>(None):None (None,None)>(None,None):None (None,None)>(None,None):None"},
      {"act_ts g60001.xes", "g60001.xes", in1, "dataframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (2021-03-05 08:06:00+00:00) (2021-03-05 08:30:00+00:00,4) (2021-03-05 09:07:00+00:00,2021-03-05 09:07:00+00:00) (2021-03-05 09:07:00+00:00,2021-03-05 09:12:00+00:00) (2021-03-05 09:12:00+00:00) (2021-03-05 09:12:00+00:00,77) (4) (7,2021-03-05 08:30:00+00:00) (7,2021-03-05 09:12:00+00:00) (77,7) T (2021-03-05 08:06:00+00:00)>():2021-03-05 08:06:00+00:00 (2021-03-05 08:30:00+00:00,4)>(4):2021-03-05 08:30:00+00:00 (2021-03-05 09:07:00+00:00,2021-03-05 09:07:00+00:00)>(2021-03-05 09:07:00+00:00,2021-03-05 09:12:00+00:00):2021-03-05 09:07:00+00:00 (2021-03-05 09:07:00+00:00,2021-03-05 09:12:00+00:00)>(2021-03-05 09:12:00+00:00):2021-03-05 09:07:00+00:00 (2021-03-05 09:12:00+00:00)>():2021-03-05 09:12:00+00:00 (2021-03-05 09:12:00+00:00,77)>(77,7):2021-03-05 09:12:00+00:00 (4)>():4 (7,2021-03-05 08:30:00+00:00)>(2021-03-05 08:30:00+00:00,4):7 (7,2021-03-05 09:12:00+00:00)>(2021-03-05 09:12:00+00:00,77):7 (77,7)>(7,2021-03-05 08:30:00+00:00):77"},
      {"act_st g60001.xes", "g60001.xes", in1, "dataframe", "ts", {{"transition_activity_attribute", "start_timestamp"}}, "S () (1) (2021-03-05 09:07:00,66) (2021-03-05 09:12:00) (6,None) (66,2021-03-05 09:12:00) (None) (None,1) (None,6) (None,None) T (1)>():1 (2021-03-05 09:07:00,66)>(66,2021-03-05 09:12:00):2021-03-05 09:07:00 (2021-03-05 09:12:00)>():2021-03-05 09:12:00 (6,None)>(None,None):6 (66,2021-03-05 09:12:00)>(2021-03-05 09:12:00):66 (None)>():None (None,1)>(1):None (None,6)>(6,None):None (None,None)>(None,1):None (None,None)>(None,None):None"},
      {"act_st_set g60001.xes", "g60001.xes", in1, "dataframe", "ts", {{"transition_activity_attribute", "start_timestamp"}, {"transition_view", "set"}}, "S () (1) (1,None) (2021-03-05 09:07:00,66) (2021-03-05 09:12:00) (2021-03-05 09:12:00,66) (6,None) (None) T (1)>():1 (1,None)>(1):None (2021-03-05 09:07:00,66)>(2021-03-05 09:12:00,66):2021-03-05 09:07:00 (2021-03-05 09:12:00)>():2021-03-05 09:12:00 (2021-03-05 09:12:00,66)>(2021-03-05 09:12:00):66 (6,None)>(6,None):None (6,None)>(None):6 (None)>():None (None)>(1,None):None (None)>(None):None"},
      {"act_ts g60003.xes", "g60003.xes", in17, "dataframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (None) (None) (None,None) (None,None) (None,None) (None,None) (None,None) T (None)>():None (None)>():None (None,None)>(None):None (None,None)>(None):None (None,None)>(None,None):None (None,None)>(None,None):None (None,None)>(None,None):None"},
      {"act_st g60003.xes", "g60003.xes", in17, "dataframe", "ts", {{"transition_activity_attribute", "start_timestamp"}}, "S () (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00) (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00) (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00) (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00) (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00) (2021-03-05 19:02:00+00:00) (2021-03-05 20:29:00+00:00) T (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00)>(2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00):2021-03-05 09:01:00+00:00 (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00)>(2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00):2021-03-05 09:49:00+00:00 (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00)>(2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00):2021-03-05 10:23:00+00:00 (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00)>(2021-03-05 20:29:00+00:00):2021-03-05 10:29:00+00:00 (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00)>(2021-03-05 19:02:00+00:00):2021-03-05 18:32:00+00:00 (2021-03-05 19:02:00+00:00)>():2021-03-05 19:02:00+00:00 (2021-03-05 20:29:00+00:00)>():2021-03-05 20:29:00+00:00"},
      {"act_st_set g60003.xes", "g60003.xes", in17, "dataframe", "ts", {{"transition_activity_attribute", "start_timestamp"}, {"transition_view", "set"}}, "S () (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00) (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00) (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00) (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00) (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00) (2021-03-05 19:02:00+00:00) (2021-03-05 20:29:00+00:00) T (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00)>(2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00):2021-03-05 09:01:00+00:00 (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00)>(2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00):2021-03-05 09:49:00+00:00 (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00)>(2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00):2021-03-05 10:23:00+00:00 (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00)>(2021-03-05 20:29:00+00:00):2021-03-05 10:29:00+00:00 (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00)>(2021-03-05 19:02:00+00:00):2021-03-05 18:32:00+00:00 (2021-03-05 19:02:00+00:00)>():2021-03-05 19:02:00+00:00 (2021-03-05 20:29:00+00:00)>():2021-03-05 20:29:00+00:00"},
      {"act_ts g60004.xes", "g60004.xes", in3, "dataframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (2021-03-05 08:25:00+00:00) (2021-03-05 09:44:00+00:00,2021-03-05 10:44:00+00:00) (2021-03-05 10:44:00+00:00,2021-03-05 10:49:00+00:00) (2021-03-05 10:49:00+00:00) (NaT,2021-03-05 09:44:00+00:00) T (2021-03-05 08:25:00+00:00)>():2021-03-05 08:25:00+00:00 (2021-03-05 09:44:00+00:00,2021-03-05 10:44:00+00:00)>(2021-03-05 10:44:00+00:00,2021-03-05 10:49:00+00:00):2021-03-05 09:44:00+00:00 (2021-03-05 10:44:00+00:00,2021-03-05 10:49:00+00:00)>(2021-03-05 10:49:00+00:00):2021-03-05 10:44:00+00:00 (2021-03-05 10:49:00+00:00)>():2021-03-05 10:49:00+00:00 (NaT,2021-03-05 09:44:00+00:00)>(2021-03-05 09:44:00+00:00,2021-03-05 10:44:00+00:00):NaT"},
      {"act_st g60004.xes", "g60004.xes", in3, "dataframe", "ts", {{"transition_activity_attribute", "start_timestamp"}}, "S () (0) (163,168) (168) (2021-03-05 09:09:00+00:00,2021-03-05 09:43:00+00:00) (2021-03-05 09:43:00+00:00,163) T (0)>():0 (163,168)>(168):163 (168)>():168 (2021-03-05 09:09:00+00:00,2021-03-05 09:43:00+00:00)>(2021-03-05 09:43:00+00:00,163):2021-03-05 09:09:00+00:00 (2021-03-05 09:43:00+00:00,163)>(163,168):2021-03-05 09:43:00+00:00"},
      {"act_st_set g60004.xes", "g60004.xes", in3, "dataframe", "ts", {{"transition_activity_attribute", "start_timestamp"}, {"transition_view", "set"}}, "S () (0) (163,168) (163,2021-03-05 09:43:00+00:00) (168) (2021-03-05 09:09:00+00:00,2021-03-05 09:43:00+00:00) T (0)>():0 (163,168)>(168):163 (163,2021-03-05 09:43:00+00:00)>(163,168):2021-03-05 09:43:00+00:00 (168)>():168 (2021-03-05 09:09:00+00:00,2021-03-05 09:43:00+00:00)>(163,2021-03-05 09:43:00+00:00):2021-03-05 09:09:00+00:00"},
      {"act_ts g60000.xes", "g60000.xes", in0, "lazyframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (None) (None,None) T (None)>():None (None,None)>(None):None (None,None)>(None,None):None"},
      {"act_ts g60001.xes", "g60001.xes", in1, "lazyframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (2021-03-05 08:06:00+00:00) (2021-03-05 08:30:00+00:00,4) (2021-03-05 09:07:00+00:00,2021-03-05 09:07:00+00:00) (2021-03-05 09:07:00+00:00,2021-03-05 09:12:00+00:00) (2021-03-05 09:12:00+00:00) (2021-03-05 09:12:00+00:00,77) (4) (7,2021-03-05 08:30:00+00:00) (7,2021-03-05 09:12:00+00:00) (77,7) T (2021-03-05 08:06:00+00:00)>():2021-03-05 08:06:00+00:00 (2021-03-05 08:30:00+00:00,4)>(4):2021-03-05 08:30:00+00:00 (2021-03-05 09:07:00+00:00,2021-03-05 09:07:00+00:00)>(2021-03-05 09:07:00+00:00,2021-03-05 09:12:00+00:00):2021-03-05 09:07:00+00:00 (2021-03-05 09:07:00+00:00,2021-03-05 09:12:00+00:00)>(2021-03-05 09:12:00+00:00):2021-03-05 09:07:00+00:00 (2021-03-05 09:12:00+00:00)>():2021-03-05 09:12:00+00:00 (2021-03-05 09:12:00+00:00,77)>(77,7):2021-03-05 09:12:00+00:00 (4)>():4 (7,2021-03-05 08:30:00+00:00)>(2021-03-05 08:30:00+00:00,4):7 (7,2021-03-05 09:12:00+00:00)>(2021-03-05 09:12:00+00:00,77):7 (77,7)>(7,2021-03-05 08:30:00+00:00):77"},
      {"act_st g60001.xes", "g60001.xes", in1, "lazyframe", "ts", {{"transition_activity_attribute", "start_timestamp"}}, "S () (1) (2021-03-05 09:07:00,66) (2021-03-05 09:12:00) (6,None) (66,2021-03-05 09:12:00) (None) (None,1) (None,6) (None,None) T (1)>():1 (2021-03-05 09:07:00,66)>(66,2021-03-05 09:12:00):2021-03-05 09:07:00 (2021-03-05 09:12:00)>():2021-03-05 09:12:00 (6,None)>(None,None):6 (66,2021-03-05 09:12:00)>(2021-03-05 09:12:00):66 (None)>():None (None,1)>(1):None (None,6)>(6,None):None (None,None)>(None,1):None (None,None)>(None,None):None"},
      {"act_st_set g60001.xes", "g60001.xes", in1, "lazyframe", "ts", {{"transition_activity_attribute", "start_timestamp"}, {"transition_view", "set"}}, "S () (1) (1,None) (2021-03-05 09:07:00,66) (2021-03-05 09:12:00) (2021-03-05 09:12:00,66) (6,None) (None) T (1)>():1 (1,None)>(1):None (2021-03-05 09:07:00,66)>(2021-03-05 09:12:00,66):2021-03-05 09:07:00 (2021-03-05 09:12:00)>():2021-03-05 09:12:00 (2021-03-05 09:12:00,66)>(2021-03-05 09:12:00):66 (6,None)>(6,None):None (6,None)>(None):6 (None)>():None (None)>(1,None):None (None)>(None):None"},
      {"act_ts g60002.xes", "g60002.xes", in2, "lazyframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (7.5) (None) (None,None) T (7.5)>():7.5 (None)>():None (None,None)>(None):None (None,None)>(None,None):None"},
      {"act_ts g60003.xes", "g60003.xes", in17, "lazyframe", "ts", {{"transition_activity_attribute", "time:timestamp"}}, "S () (None) (None,None) T (None)>():None (None,None)>(None):None (None,None)>(None,None):None"},
      {"act_st g60003.xes", "g60003.xes", in17, "lazyframe", "ts", {{"transition_activity_attribute", "start_timestamp"}}, "S () (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00) (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00) (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00) (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00) (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00) (2021-03-05 19:02:00+00:00) (2021-03-05 20:29:00+00:00) T (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00)>(2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00):2021-03-05 09:01:00+00:00 (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00)>(2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00):2021-03-05 09:49:00+00:00 (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00)>(2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00):2021-03-05 10:23:00+00:00 (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00)>(2021-03-05 20:29:00+00:00):2021-03-05 10:29:00+00:00 (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00)>(2021-03-05 19:02:00+00:00):2021-03-05 18:32:00+00:00 (2021-03-05 19:02:00+00:00)>():2021-03-05 19:02:00+00:00 (2021-03-05 20:29:00+00:00)>():2021-03-05 20:29:00+00:00"},
      {"act_st_set g60003.xes", "g60003.xes", in17, "lazyframe", "ts", {{"transition_activity_attribute", "start_timestamp"}, {"transition_view", "set"}}, "S () (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00) (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00) (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00) (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00) (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00) (2021-03-05 19:02:00+00:00) (2021-03-05 20:29:00+00:00) T (2021-03-05 09:01:00+00:00,2021-03-05 18:32:00+00:00)>(2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00):2021-03-05 09:01:00+00:00 (2021-03-05 09:49:00+00:00,2021-03-05 10:23:00+00:00)>(2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00):2021-03-05 09:49:00+00:00 (2021-03-05 10:23:00+00:00,2021-03-05 10:29:00+00:00)>(2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00):2021-03-05 10:23:00+00:00 (2021-03-05 10:29:00+00:00,2021-03-05 20:29:00+00:00)>(2021-03-05 20:29:00+00:00):2021-03-05 10:29:00+00:00 (2021-03-05 18:32:00+00:00,2021-03-05 19:02:00+00:00)>(2021-03-05 19:02:00+00:00):2021-03-05 18:32:00+00:00 (2021-03-05 19:02:00+00:00)>():2021-03-05 19:02:00+00:00 (2021-03-05 20:29:00+00:00)>():2021-03-05 20:29:00+00:00"},
      {"case_res g60000.xes", "g60000.xes", in0, "lazyframe", "ts", {{"transition_case_id_attribute", "concept:name"}}, "error: projections contained duplicate output name 'concept:name'. It's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\nDF [\"concept:name\", \"case:concept:name\", \"time:timestamp\"]; PROJECT */3 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'select'\n\t[2] 'group_by'\n"},
      {"case_res g60001.xes", "g60001.xes", in1, "lazyframe", "ts", {{"transition_case_id_attribute", "concept:name"}}, "error: projections contained duplicate output name 'concept:name'. It's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\nDF [\"time:timestamp\", \"start_timestamp\", \"case:concept:name\", \"concept:name\"]; PROJECT */4 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'select'\n\t[2] 'group_by'\n"},
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
  std::cout << "review GQ server checks passed\n";
  return 0;
}
