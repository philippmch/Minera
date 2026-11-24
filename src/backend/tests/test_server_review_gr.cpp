#undef NDEBUG
// Review GR: /api/mine's footprints family on XES frame sessions with a
// custom footprints activity or case key, a CSV upload's log shape keyed by
// a typed column or one with gaps for Alpha, the temporal profile, the
// transition system and the log skeleton, and the temporal profile on a CSV
// upload's frame shapes.
//
// - Footprints with a custom key on a frame session read the frame as
//   pm4py's EventLog conversion keyed by that column does: rows grouped by
//   the case key in first-appearance order (a float NaN its own case, text
//   gaps one), labels by Python equality, a missing key a KeyError (case
//   key first); a failed model discovery is the bundle's error.
// - A CSV log keyed by a typed column: str() names each value, a float64
//   gap is its own NaN object per cell (never equal), a date gap NaT.
// - The temporal profile on a CSV frame without a start key reads the
//   frame's start_timestamp column; pandas refuses a start column of
//   numbers or text before anything else.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gr: tc/mkgr.py, tc/mktpd.py, ct/fpmine.py,
// ct/csvfam.py, ct/tpdcsv.py).
//
// Usage: minera_server_review_gr_tests <minera_server>

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

// One case's outcome in mkgr.py's / mktpd.py's canonical form.
std::string outcome(httplib::Client& client, const std::string& name, const std::string& content,
                    const std::string& shape, const std::string& kind, const Fields& fields) {
  httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                           {"include_traces", "false", "", ""}};
  const bool cfam = kind.starts_with("cfam:");
  if (!cfam) items.push_back({"mining_input_shape", shape, "", ""});
  for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
  const auto response = client.Post("/api/mine", items);
  if (!response) return "no response";
  JsonReader reader{response->body};
  const auto body = reader.value();
  if (cfam) {
    if (body["error"].kind != Json::Kind::kNull) return "error: " + error_text(body["error"]);
    std::vector<std::string> names;
    for (const auto& a : body["activities"].items) names.push_back(name_of(a["name"]));
    const auto nm = [&](const Json& index) {
      return index.kind == Json::Kind::kNull ? std::string("nan") : names[std::stoul(index.text)];
    };
    const auto family = kind.substr(5);
    if (family == "alpha") {
      std::vector<std::string> labels;
      for (const auto& t : body["petri_net"]["transitions"].items) labels.push_back(nm(t["activity"]));
      const auto& t = body["conformance"]["alpha"]["token_replay"];
      return "A[" + sorted_join(labels, ",") + "] T" + t["fitting_traces"].text + "/" + t["produced"].text + "/" +
             t["consumed"].text + "/" + t["missing"].text + "/" + t["remaining"].text;
    }
    if (family == "temporal_profile") {
      std::vector<std::string> entries;
      for (const auto& e : body["temporal_profile"].items)
        entries.push_back(nm(e["source"]) + ">" + nm(e["target"]) + "=" + number(e["mean_seconds"], "%.6g") + "/" +
                          number(e["standard_deviation_seconds"], "%.6g"));
      return "P " + sorted_join(entries, ",");
    }
    if (family == "transition_system") {
      const auto& ts = body["transition_system"];
      std::map<std::string, std::string> state_of;
      std::vector<std::string> states, transitions;
      for (const auto& s : ts["states"].items) {
        std::vector<std::string> values;
        for (const auto& v : s["name_order"].items) values.push_back(nm(v));
        state_of[s["id"].text] = "(" + plain_join(values, ",") + ")";
        if (s["in_root"].text == "true") states.push_back(state_of[s["id"].text]);
      }
      for (const auto& x : ts["transitions"].items)
        transitions.push_back(state_of[x["source"].text] + ">" + state_of[x["target"].text] + ":" + nm(x["activity"]));
      return "S " + sorted_join(states, " ") + " T " + sorted_join(transitions, " ");
    }
    const auto& skeleton = body["log_skeleton"];
    std::string out;
    for (const char* relation : {"equivalence", "always_after", "always_before", "never_together", "directly_follows"}) {
      std::vector<std::string> pairs;
      for (const auto& p : skeleton[relation].items) pairs.push_back(nm(p["source"]) + ">" + nm(p["target"]));
      out += (out.empty() ? "" : " ") + std::string(relation).substr(0, 3) + "[" + sorted_join(pairs, ",") + "]";
    }
    std::vector<std::string> frequencies;
    for (const auto& [index, counts] : skeleton["activ_freq"].members) {
      std::vector<std::string> values;
      for (const auto& v : counts.items) values.push_back(v.text);
      Json key;
      key.kind = Json::Kind::kNumber;
      key.text = index;
      frequencies.push_back(nm(key) + "=" + plain_join(values, "/"));
    }
    return out + " F[" + sorted_join(frequencies, ",") + "]";
  }
  if (kind == "tpd:") {
    if (body["error"].kind != Json::Kind::kNull && body["family_results"].kind == Json::Kind::kNull)
      return "mine error: " + error_text(body["error"]);
    const auto& x = body["family_results"]["temporal_profile"];
    if (x["executed"].text != "true") return "error: " + error_text(x["error"]);
    std::vector<std::string> entries;
    for (const auto& e : x["result"]["entries"].items)
      entries.push_back(name_of(e["source"]) + ">" + name_of(e["target"]) + "=" + number(e["mean_seconds"], "%.6g") + "/" +
                        number(e["standard_deviation_seconds"], "%.6g"));
    return "P " + sorted_join(entries, ",");
  }
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
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:32:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:31:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:37:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:37:00.000+00:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:19:00.000+01:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:49:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:19:00.000-05:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T09:18:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:19:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:18:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:20:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:20:00.000+01:00\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:31:00.000+01:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T19:31:00.000+01:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T20:31:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T20:30:00.000-05:00\"/></event></trace>\n</log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:11:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T09:11:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"d\"/><list key=\"time:timestamp\"/></event></trace>\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"c\"/><list key=\"time:timestamp\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:49:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"b\"/><boolean key=\"time:timestamp\" value=\"true\"/><date key=\"start_timestamp\" value=\"2021-03-05T10:31:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"-2.0\"/></event><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"3.5\"/></event></trace>\n</log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"13.5\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"2\"/></values></list></event><event><string key=\"concept:name\" value=\"c\"/><float key=\"time:timestamp\" value=\"-2.0\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"a\"/><float key=\"time:timestamp\" value=\"-2.0\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T21:03:00.000+00:00\"/><list key=\"start_timestamp\"><values><string key=\"x\" value=\"0\"/></values></list></event></trace>\n</log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:26:00.000-05:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-05T08:27:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000-05:00\"/></event><event><list key=\"time:timestamp\"><values><string key=\"x\" value=\"0\"/></values></list><date key=\"start_timestamp\" value=\"2021-03-05T09:26:00.000-05:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-05T19:27:00.000+00:00\"/><date key=\"start_timestamp\" value=\"2021-03-05T18:57:00.000+01:00\"/></event></trace>\n</log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"d\"/><float key=\"time:timestamp\" value=\"-2.0\"/><date key=\"start_timestamp\" value=\"2021-03-05T08:00:00.000+00:00\"/></event></trace>\n</log>\n";
  const std::string in7 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:48:00,,2.5,2021-03-01 08:00:00,2,True,\nc0,c,2021-03-01 08:49:00,,,2021-03-01 08:00:00,2,False,1\nc0,c,2021-03-01 09:49:00,q,1.5,2021-03-01 08:00:00,,False,1\nc0,c,2021-03-01 09:50:00,p,2.5,2021-03-01 08:00:00,2,False,a\nc1,c,2021-03-01 09:22:00,,1.5,,2,True,\nc2,b,2021-03-01 08:18:00,p,2.5,2021-03-01 08:00:00,1,False,1\nc3,c,2021-03-01 08:20:00,p,1.5,,,,\nc4,c,2021-03-01 09:47:00,q,1.5,,1,,a\nc4,c,2021-03-01 09:52:00,p,1.5,,2,False,\nc4,c,2021-03-01 09:53:00,p,1.5,2021-03-01 08:00:00,2,,a\nc4,c,2021-03-01 10:53:00,q,1.5,,2,,a\nc5,b,2021-03-01 09:06:00,p,,2021-03-01 08:00:00,,False,a\nc5,a,2021-03-01 10:06:00,p,1.5,2021-03-01 08:00:00,2,True,a\n";
  const std::string in8 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 09:06:00,p,,2021-03-02 09:30:00,3,True,b\nc1,b,2021-03-01 08:11:00,q,,2021-03-02 09:30:00,3,True,b\nc2,a,2021-03-01 08:51:00,q,,2021-03-01 08:00:00,3,False,\nc3,b,2021-03-01 09:07:00,q,1.5,2021-03-01 08:00:00,,True,b\nc4,a,2021-03-01 08:14:00,,,2021-03-02 09:30:00,1,False,\n";
  const std::string in9 = "case:concept:name,concept:name,time:timestamp,K1,K2,K3,K4,K5,K6\nc0,a,2021-03-01 08:48:00,p,-1.0,,,False,\nc1,c,2021-03-01 08:24:00,r,1.5,2021-03-02 09:30:00,2,True,2\nc1,b,2021-03-01 08:25:00,q,-1.0,,,,\nc2,c,2021-03-01 08:10:00,r,,2021-03-02 09:30:00,3,,b\n";
  const std::vector<Case> cases = {
      {"footprints ca g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L a,b,c,d|a,b,c,d|a,b,c,d|b>b=2,c>c=1,d>d=1 V c>c T 4/0100 P 0.181818/0.750000/25.000000"},
      {"footprints ca g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L a,b,c,d|a,b,c,d|a,b,c,d|b>b=2,c>c=1,d>d=1 V c>c T 4/0100 P 0.181818/0.750000/25.000000"},
      {"footprints cs g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "L a,b,c,d|a,b,c,d|a,b,c,d| V  T 8/11101010 P 0.000000/1.000000/62.500000"},
      {"footprints cs g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "L a,b,c,d|a,b,c,d|a,b,c,d| V  T 8/11101010 P 0.000000/1.000000/62.500000"},
      {"footprints ct g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L a,b,c,d|a,b,c,d|a,b,c,d|b>d=1 V  T 7/1111010 P 0.090909/0.777778/71.428571"},
      {"footprints ct g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L a,b,c,d|a,b,c,d|a,b,c,d|b>d=1 V  T 7/1111010 P 0.090909/0.777778/71.428571"},
      {"footprints ar g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints ar g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints as g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "L 2021-03-05 08:31:00+00:00,2021-03-05 08:37:00+00:00,2021-03-05 08:49:00+00:00,2021-03-05 09:18:00+00:00,2021-03-05 19:18:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00,2021-03-05 20:30:00+00:00|2021-03-05 08:31:00+00:00,2021-03-05 08:49:00+00:00,2021-03-05 19:31:00+00:00|2021-03-05 08:37:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 20:30:00+00:00|2021-03-05 08:31:00+00:00>2021-03-05 08:37:00+00:00=1,2021-03-05 08:49:00+00:00>2021-03-05 09:18:00+00:00=1,2021-03-05 09:18:00+00:00>2021-03-05 19:18:00+00:00=1,2021-03-05 19:18:00+00:00>2021-03-05 19:20:00+00:00=1,2021-03-05 19:31:00+00:00>2021-03-05 20:30:00+00:00=1 V 2021-03-05 08:31:00+00:00>2021-03-05 08:37:00+00:00,2021-03-05 08:49:00+00:00>2021-03-05 09:18:00+00:00,2021-03-05 09:18:00+00:00>2021-03-05 19:18:00+00:00,2021-03-05 19:18:00+00:00>2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00>2021-03-05 20:30:00+00:00 T 3/000 P 0.000000/0.272727/0.000000"},
      {"footprints as g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "L 2021-03-05 08:31:00+00:00,2021-03-05 08:37:00+00:00,2021-03-05 08:49:00+00:00,2021-03-05 09:18:00+00:00,2021-03-05 19:18:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00,2021-03-05 20:30:00+00:00|2021-03-05 08:31:00+00:00,2021-03-05 08:49:00+00:00,2021-03-05 19:31:00+00:00|2021-03-05 08:37:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 20:30:00+00:00|2021-03-05 08:31:00+00:00>2021-03-05 08:37:00+00:00=1,2021-03-05 08:49:00+00:00>2021-03-05 09:18:00+00:00=1,2021-03-05 09:18:00+00:00>2021-03-05 19:18:00+00:00=1,2021-03-05 19:18:00+00:00>2021-03-05 19:20:00+00:00=1,2021-03-05 19:31:00+00:00>2021-03-05 20:30:00+00:00=1 V 2021-03-05 08:31:00+00:00>2021-03-05 08:37:00+00:00,2021-03-05 08:49:00+00:00>2021-03-05 09:18:00+00:00,2021-03-05 09:18:00+00:00>2021-03-05 19:18:00+00:00,2021-03-05 19:18:00+00:00>2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00>2021-03-05 20:30:00+00:00 T 3/000 P 0.000000/0.272727/0.000000"},
      {"footprints at g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L 2021-03-05 08:32:00+00:00,2021-03-05 08:37:00+00:00,2021-03-05 09:19:00+00:00,2021-03-05 19:19:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00,2021-03-05 20:31:00+00:00|2021-03-05 08:32:00+00:00,2021-03-05 09:19:00+00:00,2021-03-05 19:31:00+00:00|2021-03-05 08:37:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 20:31:00+00:00|2021-03-05 08:32:00+00:00>2021-03-05 08:37:00+00:00=1,2021-03-05 09:19:00+00:00>2021-03-05 09:19:00+00:00=1,2021-03-05 09:19:00+00:00>2021-03-05 19:19:00+00:00=1,2021-03-05 19:19:00+00:00>2021-03-05 19:20:00+00:00=1,2021-03-05 19:31:00+00:00>2021-03-05 20:31:00+00:00=1 V 2021-03-05 08:32:00+00:00>2021-03-05 08:37:00+00:00,2021-03-05 09:19:00+00:00>2021-03-05 09:19:00+00:00,2021-03-05 09:19:00+00:00>2021-03-05 19:19:00+00:00,2021-03-05 19:19:00+00:00>2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00>2021-03-05 20:31:00+00:00 T 3/000 P 0.000000/0.272727/0.000000"},
      {"footprints at g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L 2021-03-05 08:32:00+00:00,2021-03-05 08:37:00+00:00,2021-03-05 09:19:00+00:00,2021-03-05 19:19:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00,2021-03-05 20:31:00+00:00|2021-03-05 08:32:00+00:00,2021-03-05 09:19:00+00:00,2021-03-05 19:31:00+00:00|2021-03-05 08:37:00+00:00,2021-03-05 19:20:00+00:00,2021-03-05 20:31:00+00:00|2021-03-05 08:32:00+00:00>2021-03-05 08:37:00+00:00=1,2021-03-05 09:19:00+00:00>2021-03-05 09:19:00+00:00=1,2021-03-05 09:19:00+00:00>2021-03-05 19:19:00+00:00=1,2021-03-05 19:19:00+00:00>2021-03-05 19:20:00+00:00=1,2021-03-05 19:31:00+00:00>2021-03-05 20:31:00+00:00=1 V 2021-03-05 08:32:00+00:00>2021-03-05 08:37:00+00:00,2021-03-05 09:19:00+00:00>2021-03-05 09:19:00+00:00,2021-03-05 09:19:00+00:00>2021-03-05 19:19:00+00:00,2021-03-05 19:19:00+00:00>2021-03-05 19:20:00+00:00,2021-03-05 19:31:00+00:00>2021-03-05 20:31:00+00:00 T 3/000 P 0.000000/0.272727/0.000000"},
      {"footprints ac g64000.xes", "g64000.xes", in0, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ac g64000.xes", "g64000.xes", in0, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ca g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L b,c,d|b,c,d|b,c,d|c>c=2 V  T 3/100 P 0.333333/0.714286/33.333333"},
      {"footprints ca g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L b,c,d|b,c,d|b,c,d|c>c=2 V  T 3/100 P 0.333333/0.714286/33.333333"},
      {"footprints cs g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints cs g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints ct g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L b,c,d|b,c,d|b,c|c>c=1,d>c=1 V d>c T 3/100 P 0.333333/0.571429/33.333333"},
      {"footprints ct g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L b,c,d|b,c,d|b,c|c>c=1,d>c=1 V d>c T 3/100 P 0.333333/0.571429/33.333333"},
      {"footprints ar g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints ar g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints as g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints as g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints at g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L 2021-03-05 08:49:00+00:00,2021-03-05 09:11:00+00:00,NaT|2021-03-05 09:11:00+00:00,NaT|2021-03-05 08:49:00+00:00,NaT|2021-03-05 09:11:00+00:00>2021-03-05 09:11:00+00:00=1,2021-03-05 09:11:00+00:00>NaT=1,NaT>2021-03-05 08:49:00+00:00=1 V 2021-03-05 09:11:00+00:00>2021-03-05 09:11:00+00:00,2021-03-05 09:11:00+00:00>NaT,NaT>2021-03-05 08:49:00+00:00 T 2/00 P 0.000000/0.285714/0.000000"},
      {"footprints at g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L 2021-03-05 08:49:00+00:00,2021-03-05 09:11:00+00:00,None|2021-03-05 09:11:00+00:00,None|2021-03-05 08:49:00+00:00,None|2021-03-05 09:11:00+00:00>2021-03-05 09:11:00+00:00=1,2021-03-05 09:11:00+00:00>None=1,None>2021-03-05 08:49:00+00:00=1 V 2021-03-05 09:11:00+00:00>2021-03-05 09:11:00+00:00,2021-03-05 09:11:00+00:00>None,None>2021-03-05 08:49:00+00:00 T 2/00 P 0.000000/0.285714/0.000000"},
      {"footprints ac g64001.xes", "g64001.xes", in1, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ac g64001.xes", "g64001.xes", in1, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ct g64002.xes", "g64002.xes", in2, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L b|b|b| V  T 1/1 P 1.000000/1.000000/100.000000"},
      {"footprints ct g64002.xes", "g64002.xes", in2, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L b|b|b| V  T 1/1 P 1.000000/1.000000/100.000000"},
      {"footprints ar g64002.xes", "g64002.xes", in2, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints ar g64002.xes", "g64002.xes", in2, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints as g64002.xes", "g64002.xes", in2, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "L 2021-03-05 10:31:00+00:00|2021-03-05 10:31:00+00:00|2021-03-05 10:31:00+00:00| V  T 1/0 P 1.000000/1.000000/0.000000"},
      {"footprints as g64002.xes", "g64002.xes", in2, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "L 2021-03-05 10:31:00+00:00|2021-03-05 10:31:00+00:00|2021-03-05 10:31:00+00:00| V  T 1/0 P 1.000000/1.000000/0.000000"},
      {"footprints at g64002.xes", "g64002.xes", in2, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L True|True|True| V  T 1/0 P 1.000000/1.000000/0.000000"},
      {"footprints at g64002.xes", "g64002.xes", in2, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L True|True|True| V  T 1/0 P 1.000000/1.000000/0.000000"},
      {"footprints ac g64002.xes", "g64002.xes", in2, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ac g64002.xes", "g64002.xes", in2, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ca g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L c,d|c,d|c,d| V  T 2/10 P 0.000000/1.000000/50.000000"},
      {"footprints ca g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L c,d|c,d|c,d| V  T 2/10 P 0.000000/1.000000/50.000000"},
      {"footprints cs g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints cs g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints ct g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L c,d|c,d|c,d| V  T 2/10 P 0.000000/1.000000/50.000000"},
      {"footprints ct g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L c,d|c,d|c,d| V  T 2/10 P 0.000000/1.000000/50.000000"},
      {"footprints ar g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints ar g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints as g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints as g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"footprints at g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L -2.0,3.5|-2.0|3.5|-2.0>3.5=1 V -2.0>3.5 T 1/0 P 0.000000/0.333333/0.000000"},
      {"footprints at g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L -2.0,3.5|-2.0|3.5|-2.0>3.5=1 V -2.0>3.5 T 1/0 P 0.000000/0.333333/0.000000"},
      {"footprints ac g64003.xes", "g64003.xes", in3, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ac g64003.xes", "g64003.xes", in3, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ca g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L a,c,d|a,c,d|a,c,d|a>a=1 V a>a T 3/000 P 0.000000/0.571429/0.000000"},
      {"footprints ca g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "L a,c,d|a,c,d|a,c,d|a>a=1 V a>a T 3/000 P 0.000000/0.571429/0.000000"},
      {"footprints cs g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: TypeError: unhashable type: 'dict'"},
      {"footprints cs g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "L a,c,d|a,c|a,d|a>d=1,c>a=1 V  T 2/10 P 0.666667/0.833333/50.000000"},
      {"footprints ct g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L a,c,d|a,c,d|a,d|c>a=1 V  T 3/100 P 0.333333/0.666667/33.333333"},
      {"footprints ct g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L a,c,d|a,c,d|a,d|c>a=1 V  T 3/100 P 0.333333/0.666667/33.333333"},
      {"footprints ar g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints ar g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints as g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: TypeError: unhashable type: 'dict'"},
      {"footprints as g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "L {'value': None, 'children': [('x', '0')]},{'value': None, 'children': [('x', '2')]}|{'value': None, 'children': [('x', '2')]}|{'value': None, 'children': [('x', '0')]}|{'value': None, 'children': [('x', '0')]}>{'value': None, 'children': [('x', '0')]}=2,{'value': None, 'children': [('x', '2')]}>{'value': None, 'children': [('x', '0')]}=1 V {'value': None, 'children': [('x', '0')]}>{'value': None, 'children': [('x', '0')]},{'value': None, 'children': [('x', '2')]}>{'value': None, 'children': [('x', '0')]} T 1/0 P 0.000000/0.250000/0.000000"},
      {"footprints at g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L -2.0,13.5,2021-03-05 21:03:00+00:00|13.5|2021-03-05 21:03:00+00:00|-2.0>-2.0=1,-2.0>2021-03-05 21:03:00+00:00=1,13.5>-2.0=1 V -2.0>-2.0,-2.0>2021-03-05 21:03:00+00:00,13.5>-2.0 T 1/0 P 0.000000/0.200000/0.000000"},
      {"footprints at g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "L -2.0,13.5,2021-03-05 21:03:00+00:00|13.5|2021-03-05 21:03:00+00:00|-2.0>-2.0=1,-2.0>2021-03-05 21:03:00+00:00=1,13.5>-2.0=1 V -2.0>-2.0,-2.0>2021-03-05 21:03:00+00:00,13.5>-2.0 T 1/0 P 0.000000/0.200000/0.000000"},
      {"footprints ac g64004.xes", "g64004.xes", in4, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ac g64004.xes", "g64004.xes", in4, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: KeyError: 'case:concept:name'"},
      {"footprints ca g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints ca g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "concept:name"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints cs g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints cs g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "start_timestamp"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints ct g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints ct g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints ar g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints ar g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints as g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints as g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "start_timestamp"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints at g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints at g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "time:timestamp"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints ac g64005.xes", "g64005.xes", in5, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: TypeError: 'float' object is not subscriptable"},
      {"footprints ac g64005.xes", "g64005.xes", in5, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "case:concept:name"}}, "error: TypeError: 'NoneType' object is not subscriptable"},
      {"footprints ct g64006.xes", "g64006.xes", in6, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L d|d|d| V  T 1/1 P 1.000000/1.000000/100.000000"},
      {"footprints ct g64006.xes", "g64006.xes", in6, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_case_id_attribute", "time:timestamp"}}, "L d|d|d| V  T 1/1 P 1.000000/1.000000/100.000000"},
      {"footprints ar g64006.xes", "g64006.xes", in6, "dataframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"footprints ar g64006.xes", "g64006.xes", in6, "lazyframe", "fpk:", {{"footprints_model", "heuristic"}, {"footprints_conformance_variants", "log_model,log_extensive,trace_extensive"}, {"footprints_discovery_variants", "entire_event_log,trace_by_trace"}, {"footprints_activity_attribute", "org:resource"}}, "error: KeyError: 'org:resource'"},
      {"alpha K1 k66000.csv", "k66000.csv", in7, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"temporal_profile K1 k66000.csv", "k66000.csv", in7, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K1"}}, "P nan>nan=60/0,nan>p=3690/42.4264,nan>q=3630/42.4264,p>p=1830/2503.16,p>q=3630/42.4264,q>p=240/158.745,q>q=3960/0"},
      {"transition_system K1 k66000.csv", "k66000.csv", in7, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K1"}}, "S () (nan) (nan,nan) (nan,q) (p) (p,p) (p,q) (q) (q,p) T (nan)>():nan (nan,nan)>(nan,q):nan (nan,q)>(q,p):nan (p)>():p (p,p)>(p):p (p,p)>(p,q):p (p,q)>(q):p (q)>():q (q,p)>(p):q (q,p)>(p,p):q"},
      {"log_skeleton K1 k66000.csv", "k66000.csv", in7, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "equ[nan>nan,q>p] alw[] alw[] nev[] dir[] F[nan=0/1/2,p=0/1/2,q=0/1/2]"},
      {"alpha K2 k66000.csv", "k66000.csv", in7, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "A[1.5,2.5,nan,nan] T2/20/19/6/7"},
      {"temporal_profile K2 k66000.csv", "k66000.csv", in7, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K2"}}, "P 1.5>1.5=1990/1923.53,1.5>2.5=60/0,2.5>1.5=3660/0,2.5>2.5=3720/0,2.5>nan=60/0,nan>1.5=3600/0,nan>1.5=3600/0,nan>2.5=3660/0"},
      {"transition_system K2 k66000.csv", "k66000.csv", in7, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K2"}}, "S () (1.5) (1.5,1.5) (1.5,2.5) (2.5) (2.5,nan) (nan,1.5) (nan,1.5) T (1.5)>():1.5 (1.5,1.5)>(1.5):1.5 (1.5,1.5)>(1.5,1.5):1.5 (1.5,2.5)>(2.5):1.5 (2.5)>():2.5 (2.5,nan)>(nan,1.5):2.5 (nan,1.5)>(1.5):nan (nan,1.5)>(1.5,2.5):nan"},
      {"log_skeleton K2 k66000.csv", "k66000.csv", in7, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "equ[nan>1.5,nan>1.5,nan>nan,nan>nan] alw[nan>1.5,nan>1.5,nan>2.5] alw[nan>2.5] nev[2.5>nan,nan>2.5,nan>nan,nan>nan] dir[nan>1.5,nan>1.5] F[1.5=0/1/4,2.5=0/1/2,nan=0/1,nan=0/1]"},
      {"alpha K3 k66000.csv", "k66000.csv", in7, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "A[2021-03-01 08:00:00+00:00,NaT] T3/19/19/7/7"},
      {"temporal_profile K3 k66000.csv", "k66000.csv", in7, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, "P 2021-03-01 08:00:00+00:00>2021-03-01 08:00:00+00:00=2622.86/1751.24,2021-03-01 08:00:00+00:00>NaT=3600/0,NaT>2021-03-01 08:00:00+00:00=210/212.132,NaT>NaT=2640/2032.04"},
      {"transition_system K3 k66000.csv", "k66000.csv", in7, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K3"}}, "S () (2021-03-01 08:00:00+00:00) (2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00) (2021-03-01 08:00:00+00:00,NaT) (NaT) (NaT,2021-03-01 08:00:00+00:00) (NaT,NaT) T (2021-03-01 08:00:00+00:00)>():2021-03-01 08:00:00+00:00 (2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00)>(2021-03-01 08:00:00+00:00):2021-03-01 08:00:00+00:00 (2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00)>(2021-03-01 08:00:00+00:00,2021-03-01 08:00:00+00:00):2021-03-01 08:00:00+00:00 (2021-03-01 08:00:00+00:00,NaT)>(NaT):2021-03-01 08:00:00+00:00 (NaT)>():NaT (NaT)>():NaT (NaT)>():NaT (NaT,2021-03-01 08:00:00+00:00)>(2021-03-01 08:00:00+00:00,NaT):NaT (NaT,NaT)>(NaT,2021-03-01 08:00:00+00:00):NaT"},
      {"log_skeleton K3 k66000.csv", "k66000.csv", in7, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "equ[NaT>NaT] alw[] alw[] nev[] dir[] F[2021-03-01 08:00:00+00:00=0/1/2/4,NaT=0/1/3]"},
      {"alpha K4 k66000.csv", "k66000.csv", in7, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "A[1.0,2.0,nan,nan,nan] T3/17/18/6/5"},
      {"temporal_profile K4 k66000.csv", "k66000.csv", in7, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "P 1.0>2.0=1540/2096,2.0>2.0=2460/1859.42,2.0>nan=3630/42.4264,nan>2.0=3600/0,nan>2.0=60/0"},
      {"transition_system K4 k66000.csv", "k66000.csv", in7, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K4"}}, "S () (1.0) (1.0,2.0) (2.0) (2.0,2.0) (2.0,nan) (nan) (nan,2.0) (nan,2.0) T (1.0)>():1.0 (1.0,2.0)>(2.0,2.0):1.0 (2.0)>():2.0 (2.0,2.0)>(2.0):2.0 (2.0,2.0)>(2.0,2.0):2.0 (2.0,2.0)>(2.0,nan):2.0 (2.0,nan)>(nan,2.0):2.0 (nan)>():nan (nan,2.0)>(2.0):nan (nan,2.0)>(2.0):nan"},
      {"log_skeleton K4 k66000.csv", "k66000.csv", in7, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "equ[nan>2.0,nan>nan,nan>nan,nan>nan] alw[nan>2.0,nan>2.0] alw[nan>2.0] nev[1.0>nan,1.0>nan,1.0>nan,2.0>nan,nan>1.0,nan>1.0,nan>1.0,nan>2.0,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan] dir[nan>2.0,nan>2.0] F[1.0=0/1,2.0=0/1/3,nan=0/1,nan=0/1,nan=0/1]"},
      {"alpha K5 k66000.csv", "k66000.csv", in7, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K5"}}, "A[False,True,nan] T3/19/19/7/7"},
      {"temporal_profile K5 k66000.csv", "k66000.csv", in7, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K5"}}, "P False>False=2440/2061.36,False>True=3600/0,False>nan=1860/2545.58,True>False=2480/2096,nan>False=300/0,nan>nan=2640/1982.73"},
      {"transition_system K5 k66000.csv", "k66000.csv", in7, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K5"}}, "S () (False) (False,False) (False,True) (False,nan) (True) (True,False) (nan) (nan,False) (nan,nan) T (False)>():False (False,False)>(False):False (False,False)>(False,False):False (False,True)>(True):False (False,nan)>(nan,nan):False (True)>():True (True,False)>(False,False):True (nan)>():nan (nan)>():nan (nan,False)>(False,nan):nan (nan,nan)>(nan):nan"},
      {"log_skeleton K5 k66000.csv", "k66000.csv", in7, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "equ[nan>nan] alw[] alw[] nev[True>nan,nan>True] dir[] F[False=0/1/3,True=0/1,nan=0/1/3]"},
      {"alpha K6 k66000.csv", "k66000.csv", in7, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K6"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"temporal_profile K6 k66000.csv", "k66000.csv", in7, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K6"}}, "P 1>1=3600/0,1>a=1860/2545.58,a>a=2880/1688.55,a>nan=300/0,nan>1=1860/2545.58,nan>a=2480/2096"},
      {"transition_system K6 k66000.csv", "k66000.csv", in7, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K6"}}, "S () (1) (1,1) (1,a) (a) (a,a) (a,nan) (nan) (nan,1) (nan,a) T (1)>():1 (1,1)>(1,a):1 (1,a)>(a):1 (a)>():a (a,a)>(a):a (a,nan)>(nan,a):a (nan)>():nan (nan)>():nan (nan,1)>(1,1):nan (nan,a)>(a,a):nan"},
      {"log_skeleton K6 k66000.csv", "k66000.csv", in7, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "equ[nan>nan] alw[] alw[] nev[] dir[] F[1=0/1/2,a=0/1/2/3,nan=0/1]"},
      {"alpha K1 k66001.csv", "k66001.csv", in8, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K1"}}, "error: TypeError: '<' not supported between instances of 'float' and 'str'"},
      {"transition_system K1 k66001.csv", "k66001.csv", in8, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K1"}}, "S () (nan) (p) (q) T (nan)>():nan (p)>():p (q)>():q"},
      {"log_skeleton K1 k66001.csv", "k66001.csv", in8, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "equ[nan>nan] alw[] alw[] nev[nan>p,nan>q,p>nan,p>q,q>nan,q>p] dir[] F[nan=0/1,p=0/1,q=0/1]"},
      {"alpha K2 k66001.csv", "k66001.csv", in8, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "A[1.5,nan,nan,nan,nan] T5/10/10/0/0"},
      {"transition_system K2 k66001.csv", "k66001.csv", in8, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K2"}}, "S () (1.5) (nan) (nan) (nan) (nan) T (1.5)>():1.5 (nan)>():nan (nan)>():nan (nan)>():nan (nan)>():nan"},
      {"log_skeleton K2 k66001.csv", "k66001.csv", in8, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "equ[nan>nan,nan>nan,nan>nan,nan>nan] alw[] alw[] nev[1.5>nan,1.5>nan,1.5>nan,1.5>nan,nan>1.5,nan>1.5,nan>1.5,nan>1.5,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan,nan>nan] dir[] F[1.5=0/1,nan=0/1,nan=0/1,nan=0/1,nan=0/1]"},
      {"alpha K3 k66001.csv", "k66001.csv", in8, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "A[2021-03-01 08:00:00+00:00,2021-03-02 09:30:00+00:00] T5/10/10/0/0"},
      {"transition_system K3 k66001.csv", "k66001.csv", in8, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K3"}}, "S () (2021-03-01 08:00:00+00:00) (2021-03-02 09:30:00+00:00) T (2021-03-01 08:00:00+00:00)>():2021-03-01 08:00:00+00:00 (2021-03-02 09:30:00+00:00)>():2021-03-02 09:30:00+00:00"},
      {"log_skeleton K3 k66001.csv", "k66001.csv", in8, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "equ[] alw[] alw[] nev[2021-03-01 08:00:00+00:00>2021-03-02 09:30:00+00:00,2021-03-02 09:30:00+00:00>2021-03-01 08:00:00+00:00] dir[] F[2021-03-01 08:00:00+00:00=0/1,2021-03-02 09:30:00+00:00=0/1]"},
      {"alpha K4 k66001.csv", "k66001.csv", in8, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "A[1.0,3.0,nan] T5/10/10/0/0"},
      {"transition_system K4 k66001.csv", "k66001.csv", in8, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K4"}}, "S () (1.0) (3.0) (nan) T (1.0)>():1.0 (3.0)>():3.0 (nan)>():nan"},
      {"log_skeleton K4 k66001.csv", "k66001.csv", in8, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K4"}}, "equ[nan>nan] alw[] alw[] nev[1.0>3.0,1.0>nan,3.0>1.0,3.0>nan,nan>1.0,nan>3.0] dir[] F[1.0=0/1,3.0=0/1,nan=0/1]"},
      {"log_skeleton K5 k66001.csv", "k66001.csv", in8, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K5"}}, "equ[] alw[] alw[] nev[False>True,True>False] dir[] F[False=0/1,True=0/1]"},
      {"transition_system K6 k66001.csv", "k66001.csv", in8, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K6"}}, "S () (b) (nan) T (b)>():b (nan)>():nan (nan)>():nan"},
      {"log_skeleton K6 k66001.csv", "k66001.csv", in8, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K6"}}, "equ[nan>nan] alw[] alw[] nev[b>nan,nan>b] dir[] F[b=0/1,nan=0/1]"},
      {"log_skeleton K1 k66002.csv", "k66002.csv", in9, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K1"}}, "equ[q>r] alw[] alw[q>r] nev[p>q,p>r,q>p,r>p] dir[] F[p=0/1,q=0/1,r=0/1]"},
      {"alpha K2 k66002.csv", "k66002.csv", in9, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K2"}}, "A[-1.0,1.5,nan] T1/7/9/2/0"},
      {"transition_system K2 k66002.csv", "k66002.csv", in9, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K2"}}, "S () (-1.0) (1.5,-1.0) (nan) T (-1.0)>():-1.0 (1.5,-1.0)>(-1.0):1.5 (nan)>():nan"},
      {"log_skeleton K2 k66002.csv", "k66002.csv", in9, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K2"}}, "equ[1.5>-1.0,nan>nan] alw[1.5>-1.0] alw[] nev[-1.0>nan,1.5>nan,nan>-1.0,nan>1.5] dir[1.5>-1.0] F[-1.0=0/1,1.5=0/1,nan=0/1]"},
      {"alpha K3 k66002.csv", "k66002.csv", in9, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K3"}}, "A[2021-03-02 09:30:00+00:00,NaT] T0/9/9/2/2"},
      {"temporal_profile K3 k66002.csv", "k66002.csv", in9, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K3"}}, "P 2021-03-02 09:30:00+00:00>NaT=60/0"},
      {"transition_system K3 k66002.csv", "k66002.csv", in9, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K3"}}, "S () (2021-03-02 09:30:00+00:00) (2021-03-02 09:30:00+00:00,NaT) (NaT) T (2021-03-02 09:30:00+00:00)>():2021-03-02 09:30:00+00:00 (2021-03-02 09:30:00+00:00,NaT)>(NaT):2021-03-02 09:30:00+00:00 (NaT)>():NaT (NaT)>():NaT"},
      {"log_skeleton K3 k66002.csv", "k66002.csv", in9, "log", "cfam:log_skeleton", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"log_skeleton_activity_attribute", "K3"}}, "equ[NaT>NaT] alw[] alw[] nev[] dir[] F[2021-03-02 09:30:00+00:00=0/1,NaT=0/1]"},
      {"alpha K4 k66002.csv", "k66002.csv", in9, "log", "cfam:alpha", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"alpha_activity_attribute", "K4"}}, "A[2.0,3.0,nan,nan] T3/7/7/0/0"},
      {"temporal_profile K4 k66002.csv", "k66002.csv", in9, "log", "cfam:temporal_profile", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_activity_attribute", "K4"}}, "P 2.0>nan=60/0"},
      {"transition_system K4 k66002.csv", "k66002.csv", in9, "log", "cfam:transition_system", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"transition_activity_attribute", "K4"}}, "S () (2.0,nan) (3.0) (nan) (nan) T (2.0,nan)>(nan):2.0 (3.0)>():3.0 (nan)>():nan (nan)>():nan"},
  };
  const std::string tp0 = "Case,Activity,ts,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc4,d,2021-03-01 08:09:00,complete,n/a,x,\nc0,a,,complete,<NA>,x,2\nc5,d,2021-03-01 08:22:00,complete,1,y,\nc5,a,2021-03-01 08:24:00,complete,null,z,1.5\nc1,d,2021-03-01 08:22:00,complete,NA,x,2\nc2,b,2021-03-01 08:19:00,,1,y,\nc0,b,2021-03-01 08:21:00,complete,,y,1.5\nc0,a,2021-03-01 08:13:00,start,n/a,z,2\nc1,d,2021-03-01 08:17:00,complete,None,y,\nc4,b,,complete,NA,x,\nc0,b,2021-03-01 08:19:00,start,<NA>,z,\nc2,a,2021-03-01 08:23:00,complete,1,y,2\nc0,a,2021-03-01 08:10:00,complete,,x,2\nc4,b,2021-03-01 08:12:00,start,Null,z,1.5\nc3,b,2021-03-01 08:23:00,complete,x,z,2\nc2,a,2021-03-01 08:22:00,start,,y,2\nc5,d,2021-03-01 08:22:00,start,2,x,2\nc2,b,2021-03-01 08:16:00,start,,y,1.5\nc0,a,2021-03-01 08:09:00,start,,z,2\nc3,c,,complete,null,z,\nc3,b,2021-03-01 08:20:00,start,NA,z,2\nc5,c,2021-03-01 08:28:00,complete,Null,z,1.5\nc1,d,2021-03-01 08:21:00,,NA,z,1.5\nc1,d,2021-03-01 08:15:00,start,n/a,z,1.5\n";
  const std::string tp1 = "Case,concept:name,ts,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc1,c,2021-03-02 08:16:00,1,2,z,\nc0,d,2021-03-03 08:16:00,0,2,x,1.5\nc0,c,2021-03-06 08:16:00,1,2,y,\nc1,c,2021-03-07 08:16:00,1,2,y,\nc1,c,,1,,y,1.5\nc0,c,2021-03-05 08:16:00,2,NaN,z,2\nc0,d,2021-03-03 08:16:00,0,2.0,x,2\nc1,c,2021-03-04 08:16:00,2,NaN,z,1.5\n";
  const std::string tp2 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc2,a,,start,,y,2021-03-01 08:12:00\nc5,a,,start,2,x,NA\nc2,d,2021-03-01 08:16:00,complete,,y,early\nc4,c,2021-03-01 08:30:00,start,1,y,late\nc0,c,,complete,2,y,\nc2,a,,complete,1,x,NA\nc0,d,2021-03-01 08:20:00,,2.0,x,late\nc0,c,2021-03-01 08:26:00,complete,NaN,y,2021-03-01 08:26:00\nc0,c,2021-03-01 08:23:00,start,,x,late\nc4,c,2021-03-01 08:22:00,start,1,z,early\nc2,d,2021-03-01 08:14:00,start,1,x,NA\nc0,c,2021-03-01 08:33:00,start,,y,early\nc1,b,2021-03-01 08:14:00,complete,,z,\nc4,b,2021-03-01 08:34:00,start,NaN,x,NA\nc0,d,,complete,2.0,y,NA\nc4,c,2021-03-01 08:22:00,,2.0,x,early\nc4,b,2021-03-01 08:26:00,complete,,x,late\nc4,b,2021-03-01 08:28:00,complete,2.0,x,late\nc3,b,2021-03-01 08:13:00,complete,,x,2021-03-01 08:13:00\nc2,a,2021-03-01 08:17:00,start,x,z,2021-03-01 08:17:00\nc4,b,2021-03-01 08:35:00,complete,,z,NA\nc4,c,2021-03-01 08:30:00,complete,,z,NA\nc2,a,2021-03-01 08:19:00,complete,,z,2021-03-01 08:19:00\nc5,a,2021-03-01 08:12:00,complete,NaN,z,early\nc3,b,2021-03-01 08:12:00,start,1,x,NA\nc0,d,2021-03-01 08:21:00,complete,,y,NA\n";
  const std::string tp3 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,start_timestamp,st2\nc0,c,2021-03-01 08:30:00,start,01,z,2021-03-01 08:29:00,2021-03-01 08:30:00\nc0,c,2021-03-01 08:33:00,complete, 2,z,2021-03-01 08:31:00,2021-03-01 08:30:00\nc0,a,2021-03-01 08:37:00,start,001,y,2021-03-01 08:34:00,2021-03-01 08:36:00\nc0,a,2021-03-01 08:40:00,complete,,y,2021-03-01 08:38:00,2021-03-01 08:40:00\nc0,c,2021-03-01 08:42:00,start,02,y,2021-03-01 08:41:00,2021-03-01 08:41:00\nc0,c,2021-03-01 08:42:00,complete,2,x,2021-03-01 08:39:00,2021-03-01 08:40:00\nc0,d,2021-03-01 08:46:00,start,,y,2021-03-01 08:43:00,2021-03-01 08:46:00\nc0,d,2021-03-01 08:47:00,complete,001,y,2021-03-01 08:46:00,2021-03-01 08:43:00\nc1,d,2021-03-01 08:17:00,start,1,z,2021-03-01 08:13:00,2021-03-01 08:16:00\nc1,d,2021-03-01 08:20:00,complete,,z,,2021-03-01 08:20:00\nc1,a,2021-03-01 08:22:00,complete,001,x,2021-03-01 08:21:00,2021-03-01 08:22:00\nc1,b,2021-03-01 08:29:00,start,1,z,2021-03-01 08:26:00,2021-03-01 08:27:00\nc1,b,2021-03-01 08:30:00,complete,2,y,,2021-03-01 08:26:00\nc1,a,,start,1,z,2021-03-01 08:30:00,2021-03-01 08:30:00\nc1,a,2021-03-01 08:32:00,,01,x,,2021-03-01 08:31:00\nc1,c,2021-03-01 08:32:00,start, 2,z,2021-03-01 08:31:00,2021-03-01 08:28:00\nc1,c,2021-03-01 08:34:00,complete,+1,y,2021-03-01 08:30:00,2021-03-01 08:33:00\nc2,d,2021-03-01 08:37:00,start,,x,2021-03-01 08:36:00,2021-03-01 08:37:00\nc2,d,2021-03-01 08:40:00,complete,2,x,2021-03-01 08:39:00,2021-03-01 08:36:00\nc3,a,,start, 2,y,2021-03-01 08:22:00,2021-03-01 08:24:00\nc3,a,,complete,2,z,2021-03-01 08:25:00,2021-03-01 08:23:00\nc3,c,2021-03-01 08:29:00,start,001,z,2021-03-01 08:28:00,2021-03-01 08:26:00\nc3,c,2021-03-01 08:31:00,complete,02,x,2021-03-01 08:30:00,2021-03-01 08:27:00\nc3,b,2021-03-01 08:38:00,start,001,y,2021-03-01 08:35:00,2021-03-01 08:34:00\nc3,b,2021-03-01 08:41:00,complete,,z,2021-03-01 08:38:00,2021-03-01 08:37:00\nc3,d,2021-03-01 08:42:00,,,z,2021-03-01 08:42:00,2021-03-01 08:41:00\nc3,d,2021-03-01 08:42:00,complete,2,z,,2021-03-01 08:42:00\nc3,b,,start,,y,2021-03-01 08:46:00,2021-03-01 08:48:00\nc3,b,2021-03-01 08:49:00,complete,001,y,2021-03-01 08:47:00,2021-03-01 08:47:00\n";
  const std::string tp4 = "Case,Activity,ts,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc0,d,2021-03-01 08:17:00,0,2.0,y,3\nc2,a,2021-03-01 08:34:00,1,2,z,1\nc1,c,2021-03-01 08:32:00,2,NaN,z,6\nc2,a,2021-03-01 08:31:00,1,2,y,8\nc0,b,2021-03-01 08:26:00,1,2,x,7\nc1,b,2021-03-01 08:31:00,2,1,z,0\nc1,c,2021-03-01 08:32:00,1,,x,0\nc0,a,2021-03-01 08:22:00,0,1,x,4\nc0,b,2021-03-01 08:29:00,2,NaN,x,6\nc0,d,2021-03-01 08:15:00,0,nan,z,2\n";
  const std::string tp5 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc0,a,2021-03-15 08:15:00,complete,2,x,2021-03-15 08:15:00\nc0,a,,complete,1,x,2021-03-07 08:15:00\nc0,a,2021-03-04 08:15:00,start,,y,2021-03-04 08:15:00\nc0,b,,complete,2,z,2021-03-10 08:15:00\nc0,c,2021-03-16 08:15:00,complete,2,y,2021-03-16 08:15:00\nc0,b,2021-03-09 08:15:00,start,2,x,2021-03-09 08:15:00\n";
  const std::string tp6 = "case:concept:name,concept:name,ts,lifecycle:transition,concept:instance,org:resource,start_timestamp,st2\nc0,d,2021-03-01 15:19:00,Start,001,x,2021-03-01 12:19:00,2021-03-01 14:19:00\nc0,d,2021-03-02 10:19:00,complete,001,z,2021-03-02 10:19:00,2021-03-02 10:19:00\nc0,d,2021-03-02 10:19:00,, 2,x,2021-03-02 10:19:00,2021-03-02 10:19:00\nc0,b,2021-03-01 17:19:00,,1,x,2021-03-01 15:19:00,2021-03-01 13:19:00\nc0,d,2021-03-01 16:19:00,complete,02,z,2021-03-01 14:19:00,2021-03-01 13:19:00\nc0,b,2021-03-01 10:19:00,Start, 2,y,2021-03-01 10:19:00,\nc0,b,2021-03-01 12:19:00,complete,1,z,2021-03-01 11:19:00,2021-03-01 09:19:00\nc0,b,2021-03-01 16:19:00,Start, 2,z,2021-03-01 14:19:00,2021-03-01 12:19:00\n";
  const std::string tp7 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc2,c,2021-03-01 08:26:00,start,,y,2021-03-01 08:26:00\nc0,c,2021-03-01 08:17:00,,x,z,late\nc1,b,,start,x,z,\nc0,c,2021-03-01 08:20:00,complete,x,x,\nc0,a,,complete,2,z,\nc0,b,,complete,2,y,early\nc0,b,2021-03-01 08:20:00,start,x,y,NA\nc2,c,2021-03-01 08:27:00,complete,,y,NA\nc0,d,2021-03-01 08:12:00,start,,z,late\nc0,a,2021-03-01 08:24:00,start,2,z,NA\nc0,d,2021-03-01 08:15:00,complete,1,y,NA\nc1,b,2021-03-01 08:25:00,complete,1,z,NA\n";
  const std::string tp8 = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,concept:instance,org:resource\nc2,a,2021-03-05 08:00:00,complete,2,y\nc5,d,2021-03-05 08:17:00,complete,x,z\nc4,d,2021-03-16 08:00:00,start,,z\nc3,c,,start,1,z\nc3,c,,complete,01,x\nc4,d,2021-03-13 08:00:00,complete,001,y\nc1,b,2021-03-10 08:12:00,complete,,z\nc5,b,2021-03-11 08:17:00,complete,001,x\nc4,d,2021-03-13 08:00:00,start,,x\nc4,d,2021-03-18 08:00:00,complete,001,z\nc5,d,2021-03-05 08:17:00,,,z\nc0,c,2021-03-07 08:05:00,,01,z\nc4,c,2021-03-05 08:00:00,complete,2,y\nc3,b,,complete,01,x\nc2,d,,start,01,z\nc2,a,2021-03-06 08:00:00,start,001,x\nc5,b,,start,01,x\nc2,a,2021-03-19 08:00:00,complete,1,x\nc0,c,2021-03-14 08:05:00,start,1.0,y\nc0,c,2021-03-04 08:05:00,,1,x\nc0,c,2021-03-15 08:05:00,complete,,x\nc2,d,2021-03-15 08:00:00,complete,1.0,z\nc5,c,2021-03-18 08:17:00,complete,x,x\nc2,a,2021-03-19 08:00:00,complete,x,x\nc1,b,2021-03-07 08:12:00,start,,y\nc2,a,,complete,x,y\nc4,a,,start,1,x\nc1,d,2021-03-04 08:12:00,,x,z\nc1,d,2021-03-02 08:12:00,start,,z\nc4,a,,complete,2,z\n";
  const std::string tp9 = "Case,Activity,ts,lifecycle:transition,concept:instance,org:resource,start_timestamp\nc0,a,,Start, 2,z,\nc0,a,2021-03-02 08:19:00,Complete,+1,x,2021-03-02 08:19:00\nc0,b,2021-03-02 13:19:00,COMPLETE,01,x,2021-03-02 13:19:00\nc1,a,2021-03-01 11:05:00,Complete, 2,y,2021-03-01 11:05:00\nc1,a,2021-03-01 15:05:00,START,1,y,\nc1,a,2021-03-01 16:05:00,COMPLETE,01,y,\nc1,d,2021-03-01 17:05:00,START,01,z,2021-03-01 16:05:00\nc1,d,,complete, 2,z,2021-03-02 09:05:00\nc1,a,,start,1,x,\nc1,a,,,,z,2021-03-02 13:05:00\nc2,d,,,001,z,2021-03-01 11:21:00\nc2,d,2021-03-01 16:21:00,COMPLETE,+1,x,\nc2,c,,Complete,+1,x,2021-03-02 08:21:00\nc2,b,,,01,z,\nc2,b,2021-03-02 14:21:00,complete,001,y,2021-03-02 10:21:00\nc2,d,2021-03-02 15:21:00,START,01,x,2021-03-02 13:21:00\nc2,d,2021-03-02 16:21:00,COMPLETE,+1,x,2021-03-02 16:21:00\nc3,c,2021-03-01 11:12:00,complete,,x,\nc3,d,2021-03-01 17:12:00,Start,2,x,\nc3,d,2021-03-01 17:12:00,,,y,\nc4,c,,start,001,x,\nc4,c,2021-03-01 12:06:00,complete, 2,y,2021-03-01 09:06:00\nc5,d,2021-03-01 13:08:00,,,x,2021-03-01 11:08:00\nc5,d,2021-03-01 16:08:00,,,y,2021-03-01 16:08:00\nc5,d,2021-03-02 09:08:00,Complete,+1,z,\nc5,a,2021-03-02 09:08:00,Start,02,z,\nc5,a,2021-03-02 12:08:00,COMPLETE, 2,x,2021-03-02 09:08:00\nc5,d,2021-03-02 17:08:00,complete, 2,x,2021-03-02 17:08:00\nc5,c,2021-03-03 10:08:00,Start,,y,\nc5,c,,COMPLETE,2,z,2021-03-03 11:08:00\n";
  const std::vector<Case> tpd_cases = {
      {"temporal_profile def j13600.csv", "j13600.csv", tp0, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile bh j13600.csv", "j13600.csv", tp0, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile bh j13600.csv", "j13600.csv", tp0, "lazyframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P "},
      {"temporal_profile sst j13600.csv", "j13600.csv", tp0, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile t_st j13600.csv", "j13600.csv", tp0, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal_profile def j13601.csv", "j13601.csv", tp1, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile bh j13601.csv", "j13601.csv", tp1, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile bh j13601.csv", "j13601.csv", tp1, "lazyframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P "},
      {"temporal_profile sst j13601.csv", "j13601.csv", tp1, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile t_st j13601.csv", "j13601.csv", tp1, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal_profile def j13602.csv", "j13602.csv", tp2, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal_profile bh j13602.csv", "j13602.csv", tp2, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal_profile sst j13602.csv", "j13602.csv", tp2, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal_profile t_st j13602.csv", "j13602.csv", tp2, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"temporal_profile def j13603.csv", "j13603.csv", tp3, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=60/0,a>b=240/0,a>c=288/213.822,a>d=360/146.969,b>b=300/274.955,b>c=60/48.9898,b>d=150/127.279,c>a=270/173.205,c>b=600/341.526,c>c=360/251.714,c>d=516/333.706,d>a=150/127.279,d>b=375/113.578,d>c=720/109.545,d>d=60/84.8528"},
      {"temporal_profile def j13603.csv", "j13603.csv", tp3, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=60/0,a>b=240/0,a>c=288/213.822,a>d=360/146.969,b>b=300/274.955,b>c=60/48.9898,b>d=150/127.279,c>a=270/173.205,c>b=600/341.526,c>c=360/251.714,c>d=516/333.706,d>a=150/127.279,d>b=375/113.578,d>c=720/109.545,d>d=60/84.8528"},
      {"temporal_profile bh j13603.csv", "j13603.csv", tp3, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "P a>a=60/0,a>b=240/0,a>c=288/213.822,a>d=360/146.969,b>b=300/274.955,b>c=60/48.9898,b>d=150/127.279,c>a=270/173.205,c>b=600/341.526,c>c=360/251.714,c>d=516/333.706,d>a=150/127.279,d>b=375/113.578,d>c=720/109.545,d>d=60/84.8528"},
      {"temporal_profile bh j13603.csv", "j13603.csv", tp3, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "P a>a=60/0,a>b=240/0,a>c=288/213.822,a>d=360/146.969,b>b=300/274.955,b>c=60/48.9898,b>d=150/127.279,c>a=270/173.205,c>b=600/341.526,c>c=360/251.714,c>d=516/333.706,d>a=150/127.279,d>b=375/113.578,d>c=720/109.545,d>d=60/84.8528"},
      {"temporal_profile sst j13603.csv", "j13603.csv", tp3, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=60/0,a>b=240/0,a>c=288/213.822,a>d=360/146.969,b>b=300/274.955,b>c=60/48.9898,b>d=150/127.279,c>a=270/173.205,c>b=600/341.526,c>c=360/251.714,c>d=516/333.706,d>a=150/127.279,d>b=375/113.578,d>c=720/109.545,d>d=60/84.8528"},
      {"temporal_profile sst j13603.csv", "j13603.csv", tp3, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=60/0,a>b=240/0,a>c=288/213.822,a>d=360/146.969,b>b=300/274.955,b>c=60/48.9898,b>d=150/127.279,c>a=270/173.205,c>b=600/341.526,c>c=360/251.714,c>d=516/333.706,d>a=150/127.279,d>b=375/113.578,d>c=720/109.545,d>d=60/84.8528"},
      {"temporal_profile t_st j13603.csv", "j13603.csv", tp3, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P a>a=240/0,a>b=300/0,a>c=350/209.284,a>d=510/173.205,b>b=480/274.955,b>c=270/42.4264,b>d=330/127.279,c>a=360/154.919,c>b=660/341.526,c>c=440/259.23,c>d=612/314.706,d>a=480/0,d>b=540/339.411,d>c=1050/42.4264,d>d=180/0"},
      {"temporal_profile t_st j13603.csv", "j13603.csv", tp3, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P a>a=240/0,a>b=300/0,a>c=350/209.284,a>d=510/173.205,b>b=480/274.955,b>c=270/42.4264,b>d=330/127.279,c>a=360/154.919,c>b=660/341.526,c>c=440/259.23,c>d=612/314.706,d>a=480/0,d>b=540/339.411,d>c=1050/42.4264,d>d=180/0"},
      {"temporal_profile def j13604.csv", "j13604.csv", tp4, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile bh j13604.csv", "j13604.csv", tp4, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile bh j13604.csv", "j13604.csv", tp4, "lazyframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P "},
      {"temporal_profile sst j13604.csv", "j13604.csv", tp4, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"temporal_profile t_st j13604.csv", "j13604.csv", tp4, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"temporal_profile def j13605.csv", "j13605.csv", tp5, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=950400/0,a>b=432000/0,a>c=561600/672034,b>a=518400/0,b>c=604800/0"},
      {"temporal_profile def j13605.csv", "j13605.csv", tp5, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=950400/0,a>b=432000/0,a>c=561600/672034,b>a=518400/0,b>c=604800/0"},
      {"temporal_profile bh j13605.csv", "j13605.csv", tp5, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "P a>a=252000/0,a>b=108000/0,a>c=162000/178191,b>a=144000/0,b>c=180000/0"},
      {"temporal_profile bh j13605.csv", "j13605.csv", tp5, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "P a>a=252000/0,a>b=108000/0,a>c=162000/178191,b>a=144000/0,b>c=180000/0"},
      {"temporal_profile sst j13605.csv", "j13605.csv", tp5, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=950400/0,a>b=432000/0,a>c=561600/672034,b>a=518400/0,b>c=604800/0"},
      {"temporal_profile sst j13605.csv", "j13605.csv", tp5, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=950400/0,a>b=432000/0,a>c=561600/672034,b>a=518400/0,b>c=604800/0"},
      {"temporal_profile t_st j13605.csv", "j13605.csv", tp5, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P a>a=950400/0,a>b=432000/0,a>c=561600/672034,b>a=518400/0,b>c=604800/0"},
      {"temporal_profile t_st j13605.csv", "j13605.csv", tp5, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P a>a=950400/0,a>b=432000/0,a>c=561600/672034,b>a=518400/0,b>c=604800/0"},
      {"temporal_profile def j13606.csv", "j13606.csv", tp6, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}}, "P b>b=10800/5692.1,b>d=51000/33660.3,d>b=0/0,d>d=53280/29838.8"},
      {"temporal_profile def j13606.csv", "j13606.csv", tp6, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}}, "P b>b=10800/5692.1,b>d=51000/33660.3,d>b=0/0,d>d=53280/29838.8"},
      {"temporal_profile bh j13606.csv", "j13606.csv", tp6, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P b>b=10800/5692.1,b>d=17590/11866.2,d>b=0/0,d>d=12960/7465.12"},
      {"temporal_profile bh j13606.csv", "j13606.csv", tp6, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P b>b=10800/5692.1,b>d=17590/11866.2,d>b=0/0,d>d=12960/7465.12"},
      {"temporal_profile sst j13606.csv", "j13606.csv", tp6, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P b>b=10800/5692.1,b>d=51000/33660.3,d>b=0/0,d>d=53280/29838.8"},
      {"temporal_profile sst j13606.csv", "j13606.csv", tp6, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P b>b=10800/5692.1,b>d=51000/33660.3,d>b=0/0,d>d=53280/29838.8"},
      {"temporal_profile t_st j13606.csv", "j13606.csv", tp6, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P b>b=10800/6023.95,b>d=54600/34353.2,d>b=5400/4647.58,d>d=51600/37389.2"},
      {"temporal_profile t_st j13606.csv", "j13606.csv", tp6, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "ts"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P b>b=10800/6023.95,b>d=54600/34353.2,d>b=5400/4647.58,d>d=51600/37389.2"},
      {"temporal_profile def j13607.csv", "j13607.csv", tp7, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal_profile bh j13607.csv", "j13607.csv", tp7, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal_profile sst j13607.csv", "j13607.csv", tp7, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "error: TypeError: Invalid comparison between dtype=datetime64[us, UTC] and StringArray"},
      {"temporal_profile t_st j13607.csv", "j13607.csv", tp7, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: TypeError: unsupported operand type(s) for -: 'str' and 'str'"},
      {"temporal_profile def j13608.csv", "j13608.csv", tp8, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=792000/581945,a>d=820800/61094,b>b=259200/0,b>c=604800/0,c>c=576000/339791,c>d=864000/211636,d>a=345600/0,d>b=489600/141091,d>c=1.1232e+06/0,d>d=216000/166514"},
      {"temporal_profile def j13608.csv", "j13608.csv", tp8, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}}, "P a>a=792000/581945,a>d=820800/61094,b>b=259200/0,b>c=604800/0,c>c=576000/339791,c>d=864000/211636,d>a=345600/0,d>b=489600/141091,d>c=1.1232e+06/0,d>d=216000/166514"},
      {"temporal_profile bh j13608.csv", "j13608.csv", tp8, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_business_hours", "true"}}, "P a>a=234600/170100,a>d=199800/22910.3,b>b=76320/0,b>c=180000/0,c>c=156000/99913.6,c>d=250200/52622.4,d>a=144000/0,d>b=142560/46936,d>c=324000/0,d>d=55800/43922.5"},
      {"temporal_profile sst j13608.csv", "j13608.csv", tp8, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=792000/581945,a>d=820800/61094,b>b=259200/0,b>c=604800/0,c>c=576000/339791,c>d=864000/211636,d>a=345600/0,d>b=489600/141091,d>c=1.1232e+06/0,d>d=216000/166514"},
      {"temporal_profile sst j13608.csv", "j13608.csv", tp8, "lazyframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=792000/581945,a>d=820800/61094,b>b=259200/0,b>c=604800/0,c>c=576000/339791,c>d=864000/211636,d>a=345600/0,d>b=489600/141091,d>c=1.1232e+06/0,d>d=216000/166514"},
      {"temporal_profile t_st j13608.csv", "j13608.csv", tp8, "dataframe", "tpd:", {{"case_column", "case:concept:name"}, {"activity_column", "concept:name"}, {"timestamp_column", "time:timestamp"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "error: KeyError: 'start_timestamp'"},
      {"temporal_profile def j13609.csv", "j13609.csv", tp9, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}}, "P a>a=0/0,a>b=18000/0,a>d=13680/11775.9,b>d=7200/0,d>a=44400/38828.9,d>b=64800/0,d>d=56571.4/40820.2"},
      {"temporal_profile def j13609.csv", "j13609.csv", tp9, "lazyframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}}, "P a>a=0/0,a>b=18000/0,a>d=13680/11775.9,b>d=7200/0,d>a=44400/38828.9,d>b=64800/0,d>d=56571.4/40820.2"},
      {"temporal_profile bh j13609.csv", "j13609.csv", tp9, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P a>a=0/0,a>b=18000/0,a>d=13488/11579.1,b>d=7200/0,d>a=10800/10800,d>b=14400/0,d>d=27565.7/16155.3"},
      {"temporal_profile bh j13609.csv", "j13609.csv", tp9, "lazyframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_business_hours", "true"}}, "P a>a=0/0,a>b=18000/0,a>d=13488/11579.1,b>d=7200/0,d>a=10800/10800,d>b=14400/0,d>d=27565.7/16155.3"},
      {"temporal_profile sst j13609.csv", "j13609.csv", tp9, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=0/0,a>b=18000/0,a>d=13680/11775.9,b>d=7200/0,d>a=44400/38828.9,d>b=64800/0,d>d=56571.4/40820.2"},
      {"temporal_profile sst j13609.csv", "j13609.csv", tp9, "lazyframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_target_timestamp_attribute", "start_timestamp"}}, "P a>a=0/0,a>b=18000/0,a>d=13680/11775.9,b>d=7200/0,d>a=44400/38828.9,d>b=64800/0,d>d=56571.4/40820.2"},
      {"temporal_profile t_st j13609.csv", "j13609.csv", tp9, "dataframe", "tpd:", {{"case_column", "Case"}, {"activity_column", "Activity"}, {"timestamp_column", "ts"}, {"temporal_source_timestamp_attribute", "start_timestamp"}}, "P a>b=18000/0,a>d=23400/7636.75,b>d=16200/7636.75,d>a=70200/12727.9,d>d=56700/49480.9"},
  };
  for (const auto* group : {&cases, &tpd_cases})
    for (const auto& c : *group)
      check(c.label + " (" + c.shape + ")", outcome(client, c.name, c.content, c.shape, c.kind, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GR server checks passed\n";
  return 0;
}
