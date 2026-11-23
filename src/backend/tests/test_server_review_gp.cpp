#undef NDEBUG
// Review GP: the temporal profile and the heuristics miners on XES uploads'
// frame shapes with typed timestamp and start columns, and the footprints of
// an XES session's frame with a missing activity.
//
// - The temporal profile with business hours: pandas applies the business
//   seconds per pair row (an empty pair frame raises ValueError "Cannot set
//   a DataFrame with multiple columns to the single column @@flow_time";
//   BusinessHours strips each cell's tzinfo, so text raises TypeError and
//   other kinds AttributeError), Polars filters the pairs first (comparison
//   errors between dtypes raise whatever the rows) and then runs the UDF.
// - The temporal profile with a custom timestamp or start key naming a typed
//   column: pm4py's default start key is start_timestamp; a start key equal
//   to the timestamp key reads the timestamp column once.
// - Heuristics++ with start_timestamp as the timestamp key: the start key
//   reads the same column, so to_interval keeps the log and the abstraction
//   keeps frame order (Minera sorted the start and end activities).
// - The classic performance view with a start key naming no column reads the
//   start before the timestamp (KeyError of the start key).
// - /api/discover-graph footprints on a frame with a missing activity: pm4py
//   converts the frame to an EventLog, where the gap is NaN (pandas) or None
//   (Polars), one more activity (Minera answered "activity selector is
//   missing or null"); the conversion groups the rows by case id, so the
//   traces without one are a single trace (Minera kept the XES traces).
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gp: tc/mkgp.py, ct/optval.py, ct/fpmiss.py).
//
// Usage: minera_server_review_gp_tests <minera_server>

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

std::string sorted_join(std::vector<std::string> parts, const std::string& by) { return join(std::move(parts), by); }

std::string label(const Json& value, const std::string& null_label) {
  return value.kind == Json::Kind::kNull ? null_label : value.text;
}

std::string fixed(const Json& value, int digits) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.*f", digits, std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

std::string member_names(const Json& object) {
  std::vector<std::string> names;
  for (const auto& [name, value] : object.members) names.push_back(name);
  return sorted_join(names, ",");
}

std::string heuristics(const Json& result, const std::string& null_label) {
  std::vector<std::string> activities, edges;
  for (const auto& a : result["activities"].items) activities.push_back(label(a, null_label));
  for (const auto& e : result["edges"].items)
    edges.push_back(activities[std::stoul(e["source"].text)] + ">" + activities[std::stoul(e["target"].text)]);
  return "A[" + sorted_join(activities, ",") + "] E" + sorted_join(edges, " ") + " S[" +
         member_names(result["metadata"]["start_activities"]) + "] F[" + member_names(result["metadata"]["end_activities"]) + "]";
}

using Fields = std::vector<std::pair<std::string, std::string>>;

std::string mine_error(const Json& family) {
  const auto& type = family["error"]["type"].text;
  return "error: " + (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + family["error"]["message"].text;
}

// One case's outcome in mkgp.py's canonical form.
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& shape, const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""},
                                           {"mining_input_shape", shape, "", ""}};
  for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  const std::string null_label = shape == "dataframe" ? "nan" : "None";
  if (kind == "fp") {
    const auto graph_response = client.Post(
        "/api/discover-graph",
        httplib::Params{{"family", "footprints"}, {"footprints_model", "heuristic"}, {"source_id", body["source_id"].text}});
    if (!graph_response) return "no response";
    JsonReader graph_reader{graph_response->body};
    const auto graph = graph_reader.value();
    if (graph["error"].kind != Json::Kind::kNull) return "error: " + graph["error"].text;
    if (graph["result_error"].kind != Json::Kind::kNull) return "error: " + graph["result_error"].text;
    std::vector<std::string> names;
    for (const auto& a : graph["result"]["activities"].items) names.push_back(a["name"].text);
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
    const auto& model = graph["model_footprint"];
    const auto& footprints = graph["result"]["footprints"];
    const auto& log = footprints["discovery"]["entire_event_log"];
    return "M " + pairs(model["sequence"], false) + "|" + pairs(model["parallel"], false) + "|" +
           list(model["start_activities"]) + "|" + list(model["activities"]) + " L " + list(log["activities"]) + "|" +
           list(log["start_activities"]) + "|" + list(log["end_activities"]) + "|" + pairs(log["dfg"], true) + " P " +
           fixed(footprints["metrics"]["precision"], 6) + "/" + fixed(footprints["metrics"]["log_fitness"], 6);
  }
  const bool temporal = kind.starts_with("tp");
  const auto& x = body["family_results"][temporal ? "temporal_profile" : "heuristic"];
  if (x.kind != Json::Kind::kObject) return "no family result: " + response->body.substr(0, 200);
  if (x["executed"].text != "true") return mine_error(x);
  if (!temporal) return heuristics(x["result"], null_label);
  std::vector<std::string> entries;
  for (const auto& e : x["result"]["entries"].items)
    entries.push_back(label(e["source"], null_label) + ">" + label(e["target"], null_label) + ":" +
                      fixed(e["mean_seconds"], 4) + "/" + fixed(e["standard_deviation_seconds"], 4));
  return sorted_join(entries, ";");
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
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05T10:09:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05 20:09:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"2021-03-05T20:10:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05T09:00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"2021-03-05 09:29:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"2021-03-05T10:29:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05T20:29:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"2021-03-05T20:34:00\"/></event></trace>\n</log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"134.0\"/><int key=\"start_timestamp\" value=\"134\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"13.5\"/><int key=\"start_timestamp\" value=\"6\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"-2.0\"/><int key=\"start_timestamp\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"-2.0\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"60.0\"/><int key=\"start_timestamp\" value=\"3\"/></event></trace>\n</log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"609\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"60\"/><date key=\"start_timestamp\" value=\"2021-03-05T18:09:00.000+01:00\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"6\"/><string key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:06:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:05:00.000+05:30\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:07:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:06:00.000+01:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"69\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:35:00.000+05:30\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"5\"/></event><event><date key=\"time:timestamp\" value=\"2021-03-05T19:40:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"0\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"12\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:12:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:44:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:44:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:48:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:48:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:49:00.000+01:00\"/></event></trace>\n</log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/></event><event><string key=\"concept:name\" value=\"d\"/></event></trace>\n</log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\" value=\"2021-03-05 08:19:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/><boolean key=\"start_timestamp\" value=\"false\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\" value=\"2021-03-05 08:49:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><boolean key=\"start_timestamp\" value=\"true\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"false\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><boolean key=\"start_timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><boolean key=\"start_timestamp\" value=\"false\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/><boolean key=\"start_timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"67\"/><string key=\"start_timestamp\" value=\"2021-03-05T19:18:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\" value=\"2021-03-05T08:42:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"false\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:10:00.000+01:00\"/><float key=\"start_timestamp\" value=\"20.5\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:15:00.000+00:00\"/><float key=\"start_timestamp\" value=\"25.5\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:15:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:14:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:15:00.000+00:00\"/><float key=\"start_timestamp\" value=\"45.5\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:08:00.000+00:00\"/><float key=\"start_timestamp\" value=\"17.5\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:08:00.000-05:00\"/><float key=\"start_timestamp\" value=\"68.0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:58:00.000+00:00\"/><float key=\"start_timestamp\" value=\"58.0\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:03:00.000-05:00\"/><float key=\"start_timestamp\" value=\"63.0\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:04:00.000+00:00\"/><float key=\"start_timestamp\" value=\"63.0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:12:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in7 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><event><string key=\"time:timestamp\"/><float key=\"start_timestamp\" value=\"-2.0\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\"/><float key=\"start_timestamp\" value=\"21.0\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\"/><float key=\"start_timestamp\" value=\"-2.0\"/></event><event><string key=\"time:timestamp\"/><float key=\"start_timestamp\" value=\"622.0\"/></event></trace>\n</log>\n";
  const std::string in8 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"a\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"2021-03-05 09:07:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/></event></trace>\n</log>\n";
  const std::string in9 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"n/a\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event></trace>\n<trace><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"s615\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\" value=\"p\"/><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"0\"/><string key=\"start_timestamp\" value=\"2021-03-05 18:14:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"q\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"time:timestamp\" value=\"n/a\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><int key=\"time:timestamp\" value=\"6\"/><string key=\"start_timestamp\" value=\"2021-03-05T09:00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"2\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"66\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"p\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in10 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><string key=\"time:timestamp\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\"/><int key=\"start_timestamp\" value=\"6\"/></event></trace>\n</log>\n";
  const std::string in11 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"5.5\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"40.5\"/></event><event><float key=\"time:timestamp\" value=\"90.0\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"41.5\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"691.0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"14.5\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"14.5\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"615.0\"/></event></trace>\n</log>\n";
  const std::string in12 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"a\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><string key=\"concept:instance\" value=\"1\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"a\"/><id key=\"time:timestamp\" value=\"x1\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"b\"/><id key=\"time:timestamp\" value=\"2021-03-05T08:00:00\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n</log>\n";
  const std::string in13 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:58:00.000-05:00\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"0\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"0\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/><string key=\"concept:instance\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><string key=\"start_timestamp\"/></event></trace>\n</log>\n";
  const std::string in14 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><event><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:27:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:28:00.000-05:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:28:00.000-05:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:28:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in15 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:41:00.000+00:00\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><boolean key=\"time:timestamp\" value=\"true\"/><list key=\"start_timestamp\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"false\"/><date key=\"start_timestamp\" value=\"2021-03-05T17:50:00.000+05:30\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T18:20:00.000+00:00\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T18:25:00.000+01:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T17:55:00.000+05:30\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:26:00.000+01:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:26:00.000+00:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:25:00.000-05:00\"/></event></trace>\n</log>\n";
  const std::string in16 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"2\"/></values></list><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"false\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"true\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:33:00.000-05:00\"/></event></trace>\n<trace><event><float key=\"time:timestamp\" value=\"17.5\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"47.5\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"true\"/><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/><float key=\"start_timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in17 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><date key=\"time:timestamp\" value=\"2021-03-05T09:40:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in18 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"d\"/><boolean key=\"time:timestamp\" value=\"false\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><float key=\"time:timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in19 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:35:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:35:00.000+01:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:12:00.000+00:00\"/></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:24:00.000-05:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c3\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:34:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T18:34:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in20 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event></trace>\n<trace><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event></trace>\n</log>\n";
  const std::vector<Case> cases = {
      {"tp_bh g50000.xes", "g50000.xes", in0, "dataframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"tp_t_st g50000.xes", "g50000.xes", in0, "dataframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_bh_sts g50000.xes", "g50000.xes", in0, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"hpp_t_st g50000.xes", "g50000.xes", in0, "dataframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_bh_sts g50001.xes", "g50001.xes", in1, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: AttributeError: 'float' object has no attribute 'replace'"},
      {"tp_sts g50001.xes", "g50001.xes", in1, "dataframe", "tp_sts", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"hpp_t_st g50001.xes", "g50001.xes", in1, "dataframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<' not supported between instances of 'dict' and 'int'"},
      {"tp_t_st g50002.xes", "g50002.xes", in2, "dataframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "c>b:60.0000/0.0000"},
      {"hpp_t_st g50002.xes", "g50002.xes", in2, "dataframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: ValueError: NaTType does not support timestamp"},
      {"tp_t_st g50003.xes", "g50003.xes", in3, "dataframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "a>a:36060.0000/0.0000;a>b:36000.0000/0.0000;b>a:60.0000/0.0000;c>a:18270.0000/25498.2705;c>b:36240.0000/0.0000"},
      {"tp_bh_sts g50003.xes", "g50003.xes", in3, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"h_sk_st_perf g50004.xes", "g50004.xes", in4, "dataframe", "h_sk_st_perf", {{"heuristic_decoration", "performance"}, {"heuristic_start_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_t_st g50005.xes", "g50005.xes", in5, "dataframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<=' not supported between instances of 'bool' and 'str'"},
      {"tp_bh_sts g50005.xes", "g50005.xes", in5, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: AttributeError: 'bool' object has no attribute 'replace'"},
      {"tp_bh_sts g50007.xes", "g50007.xes", in6, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "a>a:870.0000/810.1852;a>b:300.0000/0.0000;a>d:27900.0000/0.0000;b>a:60.0000/0.0000;c>a:0.0000/0.0000;c>d:27900.0000/0.0000;d>a:300.0000/0.0000;d>c:300.0000/0.0000;d>d:28200.0000/0.0000"},
      {"tp_sts g50007.xes", "g50007.xes", in6, "dataframe", "tp_sts", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "a>a:870.0000/810.1852;a>b:300.0000/0.0000;a>d:36000.0000/0.0000;b>a:60.0000/0.0000;c>a:0.0000/0.0000;c>d:36000.0000/0.0000;d>a:300.0000/0.0000;d>c:300.0000/0.0000;d>d:36300.0000/0.0000"},
      {"tp_start g50007.xes", "g50007.xes", in6, "dataframe", "tp_start", {{"temporal_source_timestamp_attribute", "time:timestamp"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "a>a:870.0000/810.1852;a>b:300.0000/0.0000;a>d:36000.0000/0.0000;b>a:60.0000/0.0000;c>a:0.0000/0.0000;c>d:36000.0000/0.0000;d>a:300.0000/0.0000;d>c:300.0000/0.0000;d>d:36300.0000/0.0000"},
      {"hpp_t_st g50008.xes", "g50008.xes", in7, "dataframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'case:concept:name'"},
      {"tp_bh g50010.xes", "g50010.xes", in8, "dataframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp_bh_sts g50010.xes", "g50010.xes", in8, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp_bh_sts g50011.xes", "g50011.xes", in9, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: '<=' not supported between instances of 'int' and 'str'"},
      {"tp_sts g50011.xes", "g50011.xes", in9, "dataframe", "tp_sts", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: '<=' not supported between instances of 'int' and 'str'"},
      {"tp_bh g50000.xes", "g50000.xes", in0, "lazyframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: TypeError: str.replace() takes no keyword arguments\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"start_timestamp_2\").python_udf()\n"},
      {"tp_t_st g50000.xes", "g50000.xes", in0, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: unable to find column \"start_timestamp\"; valid columns: [\"concept:name\", \"time:timestamp\", \"case:concept:name\"]\n\nDid you mean \"time:timestamp\"?\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nDF [\"concept:name\", \"time:timestamp\", \"case:concept:name\"]; PROJECT */3 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp_bh_sts g50000.xes", "g50000.xes", in0, "lazyframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"time:timestamp_2\").python_udf()\n"},
      {"hpp_t_st g50000.xes", "g50000.xes", in0, "lazyframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_bh g50001.xes", "g50001.xes", in1, "lazyframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: cannot compare string with numeric type (f64)\n\nThis error occurred in the following expression:\n\t[(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))]\nwhile evaluating this larger expression:\n\t[([(col(\"@@index\")) < (col(\"@@index_2\"))]) & ([(col(\"time:timestamp\")) <= (col(\"start_timestamp_2\"))])]\n"},
      {"tp_t_st g50001.xes", "g50001.xes", in1, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"tp_bh_sts g50001.xes", "g50001.xes", in1, "lazyframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: AttributeError: 'float' object has no attribute 'replace'\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"time:timestamp_2\").python_udf()\n"},
      {"hpp_t_st g50001.xes", "g50001.xes", in1, "lazyframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"tp_t_st g50002.xes", "g50002.xes", in2, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, ""},
      {"hpp_t_st g50002.xes", "g50002.xes", in2, "lazyframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"tp_t_st g50003.xes", "g50003.xes", in3, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "a>a:36060.0000/0.0000;a>b:36000.0000/0.0000;b>a:60.0000/0.0000;c>a:18270.0000/25498.2705;c>b:36240.0000/0.0000"},
      {"h_sk_st_perf g50004.xes", "g50004.xes", in4, "lazyframe", "h_sk_st_perf", {{"heuristic_decoration", "performance"}, {"heuristic_start_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_t_st g50008.xes", "g50008.xes", in7, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: unable to find column \"case:concept:name\"; valid columns: [\"start_timestamp\", \"concept:name\"]\n\nDid you mean \"concept:name\"?\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'sort' <---\nSELECT [col(\"start_timestamp\"), col(\"concept:name\")]\n  DF [\"time:timestamp\", \"start_timestamp\", \"concept:name\"]; PROJECT */3 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'sort'\n"},
      {"hpp_t_st g50008.xes", "g50008.xes", in7, "lazyframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'case:concept:name'"},
      {"tp_bh g50010.xes", "g50010.xes", in8, "lazyframe", "tp_bh", {{"temporal_business_hours", "true"}}, ""},
      {"tp_bh_sts g50010.xes", "g50010.xes", in8, "lazyframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, ""},
      {"tp_t_st g50013.xes", "g50013.xes", in10, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: invalid series dtype: expected `Duration`, got `f64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"tp_bh g50014.xes", "g50014.xes", in11, "lazyframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: AttributeError: 'float' object has no attribute 'replace'\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"start_timestamp_2\").python_udf()\n"},
      {"tp_bh g51000.xes", "g51000.xes", in12, "dataframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"tp_t_st g51000.xes", "g51000.xes", in12, "dataframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_bh_sts g51000.xes", "g51000.xes", in12, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: TypeError: str.replace() takes no keyword arguments"},
      {"hpp_t_st g51000.xes", "g51000.xes", in12, "dataframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_bh g51001.xes", "g51001.xes", in13, "dataframe", "tp_bh", {{"temporal_business_hours", "true"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp_bh_sts g51001.xes", "g51001.xes", in13, "dataframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, "error: ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time"},
      {"tp_sts g51001.xes", "g51001.xes", in13, "dataframe", "tp_sts", {{"temporal_target_timestamp_attribute", "time:timestamp"}}, ""},
      {"tp_start g51001.xes", "g51001.xes", in13, "dataframe", "tp_start", {{"temporal_source_timestamp_attribute", "time:timestamp"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, ""},
      {"tp_bh g51000.xes", "g51000.xes", in12, "lazyframe", "tp_bh", {{"temporal_business_hours", "true"}}, ""},
      {"tp_t_st g51000.xes", "g51000.xes", in12, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: unable to find column \"start_timestamp\"; valid columns: [\"concept:name\", \"time:timestamp\", \"concept:instance\", \"case:concept:name\"]\n\nDid you mean \"time:timestamp\"?\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nDF [\"concept:name\", \"time:timestamp\", \"concept:instance\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"tp_bh_sts g51000.xes", "g51000.xes", in12, "lazyframe", "tp_bh_sts", {{"temporal_business_hours", "true"}, {"temporal_target_timestamp_attribute", "time:timestamp"}}, ""},
      {"hpp_t_st g51000.xes", "g51000.xes", in12, "lazyframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"tp_t_st g51001.xes", "g51001.xes", in13, "lazyframe", "tp_t_st", {{"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"start_timestamp\"))].dt.total_seconds()\n"},
      {"hpp_t_st g51001.xes", "g51001.xes", in13, "lazyframe", "hpp_t_st", {{"heuristic_variant", "plusplus"}, {"heuristic_timestamp_attribute", "start_timestamp"}}, "error: TypeError: '<' not supported between instances of 'NoneType' and 'NoneType'"},
      {"footprints missing activity g50140", "g50140.xes", in14, "dataframe", "fp", {}, "M b>c|c>d,d>c|b|b,c,d L b,c,d,nan|b,nan|c,nan|b>c=1,c>d=1,d>c=1 P 1.000000/0.857143"},
      {"footprints missing activity g50140", "g50140.xes", in14, "lazyframe", "fp", {}, "M b>c|c>d,d>c|b|b,c,d L None,b,c,d|None,b|None,c|b>c=1,c>d=1,d>c=1 P 1.000000/0.857143"},
      {"footprints missing activity g50018", "g50018.xes", in15, "dataframe", "fp", {}, "M c>b,d>c|a>b,b>a|c,d|a,b,c,d L a,b,c,d,nan|c,d,nan|b,c,d,nan|a>b=1,b>a=1,b>d=1,c>b=1,d>b=1,d>c=1 P 1.000000/0.769231"},
      {"footprints missing activity g50018", "g50018.xes", in15, "lazyframe", "fp", {}, "M c>b,d>c|a>b,b>a|c,d|a,b,c,d L None,a,b,c,d|None,c,d|None,b,c,d|a>b=1,b>a=1,b>d=1,c>b=1,d>b=1,d>c=1 P 1.000000/0.769231"},
      {"footprints missing activity g50084", "g50084.xes", in16, "dataframe", "fp", {}, "M b>c,c>a|a>a,c>d,d>c|b,c|a,b,c,d L a,b,c,d,nan|b,c,nan|a,c,nan|a>a=1,b>c=1,c>a=1,c>d=1,d>c=1 P 1.000000/0.909091"},
      {"footprints missing activity g50084", "g50084.xes", in16, "lazyframe", "fp", {}, "M b>c,c>a|a>a,c>d,d>c|b,c|a,b,c,d L None,a,b,c,d|None,b,c|None,a,c|a>a=1,b>c=1,c>a=1,c>d=1,d>c=1 P 1.000000/0.909091"},
      {"footprints without an activity column g50491", "g50491.xes", in17, "dataframe", "fp", {}, "error: KeyError: 'concept:name'"},
      {"footprints without an activity column g50491", "g50491.xes", in17, "lazyframe", "fp", {}, "error: KeyError: 'concept:name'"},
      {"footprints traces without case id g50146", "g50146.xes", in18, "dataframe", "fp", {}, "M c>d|a>a,b>d,d>b|a,c|a,b,c,d L a,b,c,d|a,c|a,d|a>a=1,b>d=1,c>d=1,d>b=1 P 1.000000/1.000000"},
      {"footprints traces without case id g50146", "g50146.xes", in18, "lazyframe", "fp", {}, "M c>d|a>a,b>d,d>b|a,c|a,b,c,d L a,b,c,d|a,c|a,d|a>a=1,b>d=1,c>d=1,d>b=1 P 1.000000/1.000000"},
      {"footprints traces without case id g50027", "g50027.xes", in19, "dataframe", "fp", {}, "M a>b,a>d|a>a|a,d|a,b,d L a,b,d|a,d|b,d|a>a=1,a>b=1,a>d=1 P 1.000000/1.000000"},
      {"footprints traces without case id g50027", "g50027.xes", in19, "lazyframe", "fp", {}, "M a>b,a>d|a>a|a,d|a,b,d L a,b,d|a,d|b,d|a>a=1,a>b=1,a>d=1 P 1.000000/1.000000"},
      {"footprints traces without case id g50108", "g50108.xes", in20, "dataframe", "fp", {}, "M c>b,d>c||d|b,c,d L b,c,d|d|b,d|c>b=1,d>c=1 P 1.000000/1.000000"},
      {"footprints traces without case id g50108", "g50108.xes", in20, "lazyframe", "fp", {}, "M c>b,d>c||d|b,c,d L b,c,d|d|b,d|c>b=1,d>c=1 P 1.000000/1.000000"},
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
  std::cout << "review GP server checks passed\n";
  return 0;
}
