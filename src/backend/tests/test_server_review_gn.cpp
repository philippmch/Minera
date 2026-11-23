#undef NDEBUG
// Review GN: /api/mine's Alpha, temporal profile and Inductive families on
// XES uploads' frame shapes whose time:timestamp and start_timestamp columns
// hold other XES kinds, and the classic heuristics miner on CSV uploads'
// frames keyed by a column pm4py's event log lacks.
//
// - read_xes types each column (dates datetime64, text str, ints int64 or
//   float64 with a gap, booleans bool or object with a gap, mixes object, an
//   empty list or valueless value a gap); Polars' copy holds an object column
//   as text. pandas sorts the frame with each column's Categorical codes
//   (numbers beside dates are not ordered, a dict is unhashable), the
//   temporal profile compares and subtracts the pairs' values with pandas'
//   own errors; Polars sorts unstably with a missing value first, and its
//   temporal profile fails on the dtypes alone (comparison, arithmetic or
//   evaluation errors). Minera parsed every value as a date ("invalid
//   selected timestamp") and read a dates column's NaN floats as values.
// - The classic heuristics miner reads pm4py's event log of a CSV frame,
//   whose events lack the case columns: a log of single-event traces leaves
//   a net without activities (Minera refused the key).
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gn: tc/mkgn.py, ct/famval.py, ct/csva3.py).
//
// Usage: minera_server_review_gn_tests <minera_server>

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

// A number as Python's "%.17g" prints the float, ".0" added to an integer.
std::string num(const Json& value) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.17g", std::stod(value.text));
  std::string text = buffer;
  if (text.find_first_of(".eEn") == std::string::npos) text += ".0";
  return text;
}

std::string join(std::vector<std::string> parts, const std::string& by, bool sorted = true) {
  if (sorted) std::sort(parts.begin(), parts.end());
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

// The family's outcome on /api/mine in mkgn.py's canonical form.
std::string family(httplib::Client& client, const std::string& name, const std::string& content,
                   const std::string& shape, const std::string& id,
                   const std::vector<std::pair<std::string, std::string>>& fields) {
  httplib::MultipartFormDataItems items = {
      {"log", content, name, "application/octet-stream"},
      {"include_traces", "false", "", ""},
      {"mining_input_shape", shape, "", ""},
  };
  if (name.ends_with(".csv")) {
    items.push_back({"case_column", "case:concept:name", "", ""});
    items.push_back({"activity_column", "concept:name", "", ""});
    items.push_back({"timestamp_column", "time:timestamp", "", ""});
  }
  for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  const auto& x = body["family_results"][id];
  if (x.kind != Json::Kind::kObject) return "no family result: " + response->body.substr(0, 200);
  if (x["executed"].text != "true") return "error: " + x["error"]["message"].text;
  const auto& r = x["result"];
  const std::string null_label = shape == "dataframe" ? "nan" : "None";
  const auto label = [&](const Json& value) { return value.kind == Json::Kind::kNull ? null_label : value.text; };
  if (id == "alpha") {
    const auto places = static_cast<std::size_t>(std::stoul(r["place_count"].text));
    std::vector<std::vector<std::string>> ins(places), outs(places);
    std::vector<std::string> transitions;
    for (const auto& t : r["transitions"].items) {
      const auto l = t["activity_is_null"].text == "true" ? null_label : t["activity_name"].text;
      transitions.push_back(l);
      for (const auto& p : t["inputs"].items) outs[std::stoul(p.text)].push_back(l);
      for (const auto& p : t["outputs"].items) ins[std::stoul(p.text)].push_back(l);
    }
    std::vector<std::string> shown;
    for (std::size_t p = 0; p < places; ++p)
      shown.push_back("(" + join(ins[p], ",") + "|" + join(outs[p], ",") + ")" +
                      (std::to_string(p) == r["source"].text ? "s" : "") + (std::to_string(p) == r["sink"].text ? "f" : ""));
    return "T[" + join(transitions, ",") + "] P" + join(shown, " ");
  }
  if (id == "temporal_profile") {
    std::vector<std::string> entries;
    for (const auto& e : r["entries"].items)
      entries.push_back(e["source"].text + ">" + e["target"].text + "=" + num(e["mean_seconds"]) + "/" +
                        num(e["standard_deviation_seconds"]));
    return join(entries, " ");
  }
  if (id == "inductive") {
    const auto& names = r["activities"].items;
    const std::map<std::string, std::string> ops = {
        {"SEQUENCE", "->"}, {"EXCLUSIVE", "X"}, {"PARALLEL", "+"}, {"LOOP", "*"}, {"OR", "O"}};
    std::function<std::string(const Json&)> walk = [&](const Json& node) -> std::string {
      const auto& op = node["op"].text;
      if (op == "TAU") return "tau";
      if (op == "ACTIVITY") return label(names[std::stoul(node["activity"].text)]);
      std::vector<std::string> children;
      for (const auto& child : node["children"].items) children.push_back(walk(child));
      return ops.at(op) + "(" + join(children, ",", false) + ")";
    };
    return walk(r["root"]);
  }
  std::vector<std::string> activities, edges;
  for (const auto& a : r["activities"].items) activities.push_back(label(a));
  for (const auto& e : r["edges"].items)
    edges.push_back(activities[std::stoul(e["source"].text)] + ">" + activities[std::stoul(e["target"].text)]);
  return "A[" + join(activities, ",") + "] E" + join(edges, " ");
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

  using Fields = std::vector<std::pair<std::string, std::string>>;
  struct Case {
    std::string label, name, content, shape, family;
    Fields fields;
    std::string expected;
  };
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><int key=\"start_timestamp\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><int key=\"start_timestamp\" value=\"2\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><int key=\"start_timestamp\" value=\"1\"/></event></trace>\n</log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><float key=\"start_timestamp\" value=\"1.5\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><float key=\"start_timestamp\" value=\"2.5\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><float key=\"start_timestamp\" value=\"1.5\"/></event></trace>\n</log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"false\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><boolean key=\"start_timestamp\" value=\"true\"/></event></trace>\n</log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><string key=\"start_timestamp\" value=\"y\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><string key=\"start_timestamp\" value=\"x\"/></event></trace>\n</log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 08:30:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 09:30:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><string key=\"start_timestamp\" value=\"2021-03-05 08:30:00\"/></event></trace>\n</log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><int key=\"start_timestamp\" value=\"3\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T07:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:00:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><int key=\"start_timestamp\" value=\"3\"/></event></trace>\n</log>\n";
  const std::string in7 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"2\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in8 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"y\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in9 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><int key=\"start_timestamp\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"y\"/><int key=\"start_timestamp\" value=\"2\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><int key=\"start_timestamp\" value=\"1\"/></event></trace>\n</log>\n";
  const std::string in10 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/><string key=\"start_timestamp\" value=\"x\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"2\"/><string key=\"start_timestamp\" value=\"y\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/><string key=\"start_timestamp\" value=\"x\"/></event></trace>\n</log>\n";
  const std::string in11 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><string key=\"start_timestamp\" value=\"p\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"y\"/><string key=\"start_timestamp\" value=\"q\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><string key=\"start_timestamp\" value=\"p\"/></event></trace>\n</log>\n";
  const std::string in12 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><boolean key=\"start_timestamp\" value=\"false\"/></event><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/><boolean key=\"start_timestamp\" value=\"true\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><boolean key=\"start_timestamp\" value=\"false\"/></event></trace>\n</log>\n";
  const std::string in13 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/><float key=\"start_timestamp\" value=\"1.5\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"2\"/><float key=\"start_timestamp\" value=\"2.5\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/><float key=\"start_timestamp\" value=\"1.5\"/></event></trace>\n</log>\n";
  const std::string in14 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><int key=\"start_timestamp\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/><int key=\"start_timestamp\" value=\"2\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/><int key=\"start_timestamp\" value=\"1\"/></event></trace>\n</log>\n";
  const std::string in15 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><boolean key=\"start_timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"y\"/><boolean key=\"start_timestamp\" value=\"false\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/><boolean key=\"start_timestamp\" value=\"true\"/></event></trace>\n</log>\n";
  const std::string in16 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/></event><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"y\"/></event></trace>\n</log>\n";
  const std::string in17 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"2\"/></event></trace>\n</log>\n";
  const std::string in18 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"false\"/></event></trace>\n</log>\n";
  const std::string in19 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><float key=\"start_timestamp\" value=\"NaN\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T11:00:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/><list key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/><string key=\"start_timestamp\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T10:00:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:30:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in20 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:41:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"NaN\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:42:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in21 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"5\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:00:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in22 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/></event><event><string key=\"concept:name\" value=\"b\"/><list key=\"time:timestamp\"><values><string key=\"x\" value=\"1\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"y\"/></event></trace>\n</log>\n";
  const std::string in23 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><string key=\"time:timestamp\" value=\"q\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"p\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"r\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"p\"/></event><event><string key=\"concept:name\" value=\"c\"/><string key=\"time:timestamp\" value=\"r\"/></event></trace>\n</log>\n";
  const std::string in24 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"3\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"7\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"9\"/></event><event><string key=\"concept:name\" value=\"a\"/><int key=\"time:timestamp\" value=\"2\"/></event></trace>\n</log>\n";
  const std::string in25 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><int key=\"time:timestamp\" value=\"3\"/></event><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"x\"/></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"0.5\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><int key=\"time:timestamp\" value=\"9\"/></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"NaN\"/></event></trace>\n</log>\n";
  const std::string in26 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/></event><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"true\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><boolean key=\"time:timestamp\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"a\"/><boolean key=\"time:timestamp\" value=\"false\"/></event></trace>\n</log>\n";
  const std::string in27 = "case:concept:name,concept:name,time:timestamp,st2\nc0,b,2021-03-01 11:28:00,2021-03-01 11:28:00\nc1,d,2021-03-01 12:03:00,None\nc0,b,,2021-03-01 11:28:00\n";
  const std::vector<Case> cases = {
      {"temporal profile: a dates timestamp, int starts", "gn1.xes", in0, "dataframe", "temporal_profile", {},
       "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal profile: a dates timestamp, int starts", "gn1.xes", in0, "lazyframe", "temporal_profile", {},
       "error: - not allowed on i64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Int64) <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"temporal profile: a dates timestamp, float starts", "gn1.xes", in1, "dataframe", "temporal_profile", {},
       "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal profile: a dates timestamp, float starts", "gn1.xes", in1, "lazyframe", "temporal_profile", {},
       "error: - not allowed on f64 and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\").cast(Float64) <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"temporal profile: a dates timestamp, boolean starts", "gn1.xes", in2, "dataframe", "temporal_profile", {},
       "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal profile: a dates timestamp, boolean starts", "gn1.xes", in2, "lazyframe", "temporal_profile", {},
       "error: - not allowed on bool and datetime[\u03bcs, UTC]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"temporal profile: a dates timestamp, text starts", "gn1.xes", in3, "dataframe", "temporal_profile", {},
       "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal profile: a dates timestamp, text starts", "gn1.xes", in3, "lazyframe", "temporal_profile", {},
       "error: cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'filter' failed\n\t[2] 'with_columns'\n"},
      {"temporal profile: a dates timestamp, starts as date text", "gn1.xes", in4, "dataframe", "temporal_profile", {},
       "error: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
      {"temporal profile: a dates timestamp, starts as date text", "gn1.xes", in4, "lazyframe", "temporal_profile", {},
       "error: cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'filter' failed\n\t[2] 'with_columns'\n"},
      {"temporal profile: a dates timestamp, dates and an int as starts", "gn1.xes", in5, "dataframe", "temporal_profile", {},
       "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"temporal profile: a dates timestamp, dates and an int as starts", "gn1.xes", in5, "lazyframe", "temporal_profile", {},
       "error: cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'filter' failed\n\t[2] 'with_columns'\n"},
      {"temporal profile: one measured pair beside an int start", "gn1.xes", in6, "dataframe", "temporal_profile", {},
       "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"temporal profile: one measured pair beside an int start", "gn1.xes", in6, "lazyframe", "temporal_profile", {},
       "error: cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'filter' failed\n\t[2] 'with_columns'\n"},
      {"temporal profile: int timestamps, dates as starts", "gn1.xes", in7, "dataframe", "temporal_profile", {},
       "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal profile: int timestamps, dates as starts", "gn1.xes", in7, "lazyframe", "temporal_profile", {},
       "error: - not allowed on datetime[\u03bcs, UTC] and i64\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\").cast(Int64)\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n"},
      {"temporal profile: text timestamps, dates as starts", "gn1.xes", in8, "dataframe", "temporal_profile", {},
       "error: TypeError: '>=' not supported between instances of 'Timestamp' and 'str'"},
      {"temporal profile: text timestamps, dates as starts", "gn1.xes", in8, "lazyframe", "temporal_profile", {},
       "error: cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\nFILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n    INNER JOIN:\n    LEFT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    RIGHT PLAN ON: [col(\"case:concept:name\")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n          SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n            DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n    END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'filter' failed\n\t[2] 'with_columns'\n"},
      {"temporal profile: text timestamps, int starts", "gn1.xes", in9, "dataframe", "temporal_profile", {},
       "error: TypeError: '<=' not supported between instances of 'str' and 'int'"},
      {"temporal profile: text timestamps, int starts", "gn1.xes", in9, "lazyframe", "temporal_profile", {},
       "error: arithmetic on dtypes i64 and str is not allowed (lhs: column 'start_timestamp_2', rhs: column 'time:timestamp'); try an explicit cast first\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' failed <---\n WITH_COLUMNS:\n [((col(\"start_timestamp_2\") - col(\"time:timestamp\"))).dt.total_seconds().alias(\"@@flow_time\")] \n  FILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\n  FROM\n    FILTER col(\"@@index\") < col(\"@@index_2\")\n    FROM\n      INNER JOIN:\n      LEFT PLAN ON: [col(\"case:concept:name\")]\n        ROW INDEX name: @@index, offset: 0\n          SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n            SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n              DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n      RIGHT PLAN ON: [col(\"case:concept:name\")]\n        ROW INDEX name: @@index, offset: 0\n          SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n            SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n              DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n      END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns' failed\n"},
      {"temporal profile: int timestamps, text starts", "gn1.xes", in10, "dataframe", "temporal_profile", {},
       "error: TypeError: '>=' not supported between instances of 'str' and 'int'"},
      {"temporal profile: int timestamps, text starts", "gn1.xes", in10, "lazyframe", "temporal_profile", {},
       "error: arithmetic on dtypes str and i64 is not allowed (lhs: column 'start_timestamp_2', rhs: column 'time:timestamp'); try an explicit cast first\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' failed <---\n WITH_COLUMNS:\n [((col(\"start_timestamp_2\") - col(\"time:timestamp\"))).dt.total_seconds().alias(\"@@flow_time\")] \n  FILTER col(\"time:timestamp\") <= col(\"start_timestamp_2\")\n  FROM\n    FILTER col(\"@@index\") < col(\"@@index_2\")\n    FROM\n      INNER JOIN:\n      LEFT PLAN ON: [col(\"case:concept:name\")]\n        ROW INDEX name: @@index, offset: 0\n          SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n            SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n              DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n      RIGHT PLAN ON: [col(\"case:concept:name\")]\n        ROW INDEX name: @@index, offset: 0\n          SORT BY [col(\"case:concept:name\"), col(\"start_timestamp\"), col(\"time:timestamp\")]\n            SELECT [col(\"concept:name\"), col(\"time:timestamp\"), col(\"start_timestamp\"), col(\"case:concept:name\")]\n              DF [\"concept:name\", \"time:timestamp\", \"start_timestamp\", \"case:concept:name\"]; PROJECT */4 COLUMNS\n      END INNER JOIN\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns' failed\n"},
      {"temporal profile: text timestamps and starts", "gn1.xes", in11, "dataframe", "temporal_profile", {},
       "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal profile: text timestamps and starts", "gn1.xes", in11, "lazyframe", "temporal_profile", {},
       "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"temporal profile: boolean timestamps and starts", "gn1.xes", in12, "dataframe", "temporal_profile", {},
       "error: TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` operator, or the logical_xor function instead."},
      {"temporal profile: boolean timestamps and starts", "gn1.xes", in12, "lazyframe", "temporal_profile", {},
       "error: `subtract` operation not supported for dtype `bool`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"temporal profile: int timestamps, float starts", "gn1.xes", in13, "dataframe", "temporal_profile", {},
       "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal profile: int timestamps, float starts", "gn1.xes", in13, "lazyframe", "temporal_profile", {},
       "error: invalid series dtype: expected `Duration`, got `f64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\").cast(Float64))].dt.total_seconds()\n"},
      {"temporal profile: boolean timestamps, int starts", "gn1.xes", in14, "dataframe", "temporal_profile", {},
       "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal profile: boolean timestamps, int starts", "gn1.xes", in14, "lazyframe", "temporal_profile", {},
       "error: invalid series dtype: expected `Duration`, got `i64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\").cast(Int64))].dt.total_seconds()\n"},
      {"temporal profile: text timestamps, boolean starts", "gn1.xes", in15, "dataframe", "temporal_profile", {},
       "error: TypeError: '<=' not supported between instances of 'str' and 'bool'"},
      {"temporal profile: text timestamps, boolean starts", "gn1.xes", in15, "lazyframe", "temporal_profile", {},
       "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\").cast(String)) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\").cast(String)) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"temporal profile: text timestamps, no start column", "gn1.xes", in16, "dataframe", "temporal_profile", {},
       "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"temporal profile: text timestamps, no start column", "gn1.xes", in16, "lazyframe", "temporal_profile", {},
       "error: sub operation not supported for dtypes `str` and `str`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"temporal profile: int timestamps, no start column", "gn1.xes", in17, "dataframe", "temporal_profile", {},
       "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal profile: int timestamps, no start column", "gn1.xes", in17, "lazyframe", "temporal_profile", {},
       "error: invalid series dtype: expected `Duration`, got `i64` for series with name `start_timestamp_2`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"temporal profile: boolean timestamps, no start column", "gn1.xes", in18, "dataframe", "temporal_profile", {},
       "error: TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` operator, or the logical_xor function instead."},
      {"temporal profile: boolean timestamps, no start column", "gn1.xes", in18, "lazyframe", "temporal_profile", {},
       "error: `subtract` operation not supported for dtype `bool`\n\nThis error occurred in the following expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))]\nwhile evaluating this larger expression:\n\t[(col(\"start_timestamp_2\")) - (col(\"time:timestamp\"))].dt.total_seconds()\n"},
      {"temporal profile: a dates column with NaN, an empty list and None", "gn1.xes", in19, "dataframe", "temporal_profile", {},
       "b>c=7200.0/0.0"},
      {"temporal profile: a dates column with NaN, an empty list and None", "gn1.xes", in19, "lazyframe", "temporal_profile", {},
       "a>b=7200.0/0.0 a>c=9900.0/1272.7922061357856 b>c=6300.0/1272.7922061357856"},
      {"temporal profile: NaN floats among dates", "gn1.xes", in20, "dataframe", "temporal_profile", {},
       "d>d=0.0/0.0"},
      {"temporal profile: NaN floats among dates", "gn1.xes", in20, "lazyframe", "temporal_profile", {},
       ""},
      {"alpha: a number beside dates", "gn2.xes", in21, "dataframe", "alpha", {},
       "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"alpha: a number beside dates", "gn2.xes", in21, "lazyframe", "alpha", {},
       "T[a,b,c] P(a|c) (c|)f (c|b) (|a)s"},
      {"inductive: a number beside dates", "gn2.xes", in21, "dataframe", "inductive", {},
       "error: TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."},
      {"inductive: a number beside dates", "gn2.xes", in21, "lazyframe", "inductive", {},
       "->(a,c,b)"},
      {"alpha: a list among text timestamps", "gn2.xes", in22, "dataframe", "alpha", {},
       "error: TypeError: unhashable type: 'dict'"},
      {"alpha: a list among text timestamps", "gn2.xes", in22, "lazyframe", "alpha", {},
       "T[a,b,c] P(a|c) (c|)f (c|b) (|a)s"},
      {"inductive: a list among text timestamps", "gn2.xes", in22, "dataframe", "inductive", {},
       "error: TypeError: unhashable type: 'dict'"},
      {"inductive: a list among text timestamps", "gn2.xes", in22, "lazyframe", "inductive", {},
       "->(a,c,b)"},
      {"alpha: text timestamps", "gn2.xes", in23, "dataframe", "alpha", {},
       "T[a,b,c] P(a|b) (a|c) (b|c) (c|)f (|a,b)s"},
      {"alpha: text timestamps", "gn2.xes", in23, "lazyframe", "alpha", {},
       "T[a,b,c] P(a|b) (a|c) (b|c) (c|)f (|a,b)s"},
      {"inductive: text timestamps", "gn2.xes", in23, "dataframe", "inductive", {},
       "->(a,X(tau,b),c)"},
      {"inductive: text timestamps", "gn2.xes", in23, "lazyframe", "inductive", {},
       "->(a,X(tau,b),c)"},
      {"alpha: int timestamps", "gn2.xes", in24, "dataframe", "alpha", {},
       "T[a,b,c] P(a,c|)f (a|b) (a|c) (b|c) (|b,c)s"},
      {"alpha: int timestamps", "gn2.xes", in24, "lazyframe", "alpha", {},
       "T[a,b,c] P(a,c|)f (a|b) (a|c) (b|c) (|b,c)s"},
      {"inductive: int timestamps", "gn2.xes", in24, "dataframe", "inductive", {},
       "->(a,X(tau,b),c)"},
      {"inductive: int timestamps", "gn2.xes", in24, "lazyframe", "inductive", {},
       "->(a,X(tau,b),c)"},
      {"alpha: ints, text, floats and NaN", "gn2.xes", in25, "dataframe", "alpha", {},
       "T[a,b,c] P(a,c|)f (b|a) (c|a) (c|b) (|b,c)s"},
      {"alpha: ints, text, floats and NaN", "gn2.xes", in25, "lazyframe", "alpha", {},
       "T[a,b,c] P(a,c|)f (a|c) (b|a) (c|b) (|b,c)s"},
      {"inductive: ints, text, floats and NaN", "gn2.xes", in25, "dataframe", "inductive", {},
       "->(c,X(tau,b),a)"},
      {"inductive: ints, text, floats and NaN", "gn2.xes", in25, "lazyframe", "inductive", {},
       "+(a,->(c,X(tau,b)))"},
      {"alpha: boolean timestamps", "gn2.xes", in26, "dataframe", "alpha", {},
       "T[a,b,c] P(a,c|)f (a|b) (a|c) (b|c) (|b,c)s"},
      {"alpha: boolean timestamps", "gn2.xes", in26, "lazyframe", "alpha", {},
       "T[a,b,c] P(a,c|)f (a|b) (a|c) (b|c) (|b,c)s"},
      {"inductive: boolean timestamps", "gn2.xes", in26, "dataframe", "inductive", {},
       "->(a,X(tau,b),c)"},
      {"inductive: boolean timestamps", "gn2.xes", in26, "lazyframe", "inductive", {},
       "->(a,X(tau,b),c)"},
      {"alpha: a dates column with NaN, an empty list and None", "gn2.xes", in19, "dataframe", "alpha", {},
       "T[a,b,c] P(a|b) (b|c) (c|)f (c|a) (|a)s"},
      {"alpha: a dates column with NaN, an empty list and None", "gn2.xes", in19, "lazyframe", "alpha", {},
       "T[a,b,c] P(a|b) (b|c) (c|)f (|a)s"},
      {"inductive: a dates column with NaN, an empty list and None", "gn2.xes", in19, "dataframe", "inductive", {},
       "->(a,b,c)"},
      {"inductive: a dates column with NaN, an empty list and None", "gn2.xes", in19, "lazyframe", "inductive", {},
       "+(a,c,b)"},
      {"heuristics: the case column as the key, single-event cases", "gn3.csv", in27, "dataframe", "heuristic", {{"heuristic_activity_attribute", "case:concept:name"}},
       "A[] E"},
      {"heuristics: the case column as the key, single-event cases", "gn3.csv", in27, "lazyframe", "heuristic", {{"heuristic_activity_attribute", "case:concept:name"}},
       "A[] E"},
      {"heuristics: a missing column as the key, single-event cases", "gn3.csv", in27, "dataframe", "heuristic", {{"heuristic_activity_attribute", "zzz"}},
       "A[] E"},
      {"heuristics: a missing column as the key, single-event cases", "gn3.csv", in27, "lazyframe", "heuristic", {{"heuristic_activity_attribute", "zzz"}},
       "A[] E"},
  };
  for (const auto& c : cases)
    check(c.label + " (" + c.shape + ")", family(client, c.name, c.content, c.shape, c.family, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GN server checks passed\n";
  return 0;
}
