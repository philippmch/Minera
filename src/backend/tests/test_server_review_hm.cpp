#undef NDEBUG
// Review HM: typed activity keys on the stochastic map, temporal and
// minimum self-distance families, POWL precision and playout.
//
// - /api/stochastic-map?miner=powl on a POWL net mined under a typed key
//   (powl_activity_key): a transition labelled by a falsy object (0, 0.0,
//   -0.0, False, '') gets the immediate distribution, its firing still
//   consuming its event, as pm4py's replay statistics time it.
// - /api/analyze-behavior temporal_profile / temporal_conformance on the
//   dataframe adapter of XES and CSV uploads keyed by typed columns:
//   pandas' merge of the pair frame with the profile raises its dtype
//   ValueError; a CSV frame reads the key as format_dataframe(read_csv)
//   types it.
// - minimum_self_distance on the pandas variant raises pm4py's string
//   concatenation TypeError at the first typed cell, a gap being an event
//   no entry keys; on the Polars variant typed values are named str() of
//   the Polars value, -0.0 grouped with 0.0.
// - the POWL net's ETConformance precision (/api/mine's token replay and
//   /api/evaluate) raises pm4py's join TypeError on a typed key, and
//   /api/playout?miner=powl names an event of an empty label.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-hm: tc/mkhm.py, ct/bh.py, ct/pdec.py, ct/pev.py).
//
// Usage: minera_server_review_hm_tests <minera_server>
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

std::string join(std::vector<std::string> parts, const std::string& by) {
  std::sort(parts.begin(), parts.end());
  std::string out;
  for (std::size_t k = 0; k < parts.size(); ++k) out += (k ? by : "") + parts[k];
  return out;
}

std::string sorted_join(std::vector<std::string> parts, const std::string& by) { return join(std::move(parts), by); }

std::string fixed(const Json& value, int digits) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.*f", digits, std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

using Fields = std::vector<std::pair<std::string, std::string>>;

std::string plain(const Json& value) {
  if (value.kind == Json::Kind::kNull) return "None";
  std::string out;
  for (const char c : value.text)
    if (c != '\x1f') out += c;
  return out;
}

std::string error_text(const Json& error) {
  if (error.kind == Json::Kind::kString) return error.text;
  const auto& type = error["type"].text;
  return (type.starts_with("builtins.") ? type.substr(9) + ": " : std::string()) + error["message"].text;
}

std::string norm(std::string message) {
  for (bool changed = true; changed;) {
    changed = false;
    for (const char* prefix : {"inductive miner: ", "Heuristics Miner: ", "powl_activity_key: ", "TypeError: ", "KeyError: ", "ValueError: ",
                               "AttributeError: ", "IndexError: ", "ZeroDivisionError: "})
      if (message.starts_with(prefix)) {
        message = message.substr(std::string(prefix).size());
        changed = true;
      }
  }
  return message;
}

std::string f_digits(const Json& value, int digits) {
  if (value.kind != Json::Kind::kNumber || value.text == "NaN") return "nan";
  return fixed(value, digits);
}

std::string lbl(const Json& value) { return plain(value); }

bool is_csv(const std::string& name) { return name.size() > 4 && name.substr(name.size() - 4) == ".csv"; }

struct Server {
  httplib::Client& client;
  const std::string& name;
  const std::string& content;

  Json parse(const httplib::Result& response) const {
    Json out;
    if (!response) {
      out.kind = Json::Kind::kObject;
      Json error;
      error.kind = Json::Kind::kString;
      error.text = "no response";
      out.members.emplace_back("error", error);
      return out;
    }
    JsonReader reader{response->body};
    return reader.value();
  }

  Json upload(const Fields& fields) const {
    httplib::MultipartFormDataItems items = {{"log", content, name, "application/octet-stream"},
                                             {"include_traces", "false", "", ""}};
    if (is_csv(name)) {
      items.push_back({"case_column", "case:concept:name", "", ""});
      items.push_back({"activity_column", "concept:name", "", ""});
      items.push_back({"timestamp_column", "time:timestamp", "", ""});
    }
    for (const auto& [key, value] : fields) items.push_back({key, value, "", ""});
    return parse(client.Post("/api/mine", items));
  }

  Json post(const std::string& route, const Fields& fields) const {
    httplib::Params params;
    for (const auto& [key, value] : fields) params.emplace(key, value);
    return parse(client.Post(route, params));
  }

  Json get(const std::string& route) const { return parse(client.Get(route)); }

  std::string source_id() const {
    const auto body = parse(client.Get("/api/log-query/schema"));
    return body["source_id"].text;
  }
};

std::string fail(const Json& error) { return "error: " + norm(error_text(error)); }

std::string none_or(const Json& value) { return value.kind == Json::Kind::kNull ? std::string("None") : value.text; }

bool error_set(const Json& value) {
  return value.kind != Json::Kind::kNull && !(value.kind == Json::Kind::kBool && value.text == "false") &&
         !(value.kind == Json::Kind::kString && value.text.empty());
}

std::string move_text(const Json& m) {
  const auto& type = m["type"].text;
  const std::string t = type == "sync" ? "s" : type == "log" ? "l" : type == "model" ? "m" : type;
  const bool silent = m["silent"].kind == Json::Kind::kBool && m["silent"].text == "true";
  return t + ":" + (silent ? std::string("~") : lbl(m["activity_name"]));
}

std::string g10(const Json& value) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.10g", std::strtod(value.text.c_str(), nullptr));
  return buffer;
}

// One case's outcome in mkhm.py's canonical form (its srv()).
std::string outcome(const Server& server, const std::string& kind, const std::string& key, const std::string& extra,
                    const std::string& model) {
  if (kind == "pal" || kind == "pdec" || kind == "psm" || kind == "pev" || kind == "play") {
    const auto mined = server.upload({{"powl_activity_key", key}});
    if (error_set(mined["error"])) return fail(mined["error"]);
    const auto& c = mined["conformance"]["powl"];
    if (c.kind == Json::Kind::kObject && error_set(c["error"]) &&
        error_text(c["error"]).find("powl_activity_key") != std::string::npos)
      return fail(c["error"]);
    if (kind == "pev") {
      if (extra == "mine") {
        const auto& t = c["token_replay"];
        if (error_set(t["precision_error"])) return fail(t["precision_error"]);
        return "E " + f_digits(t["precision"], 6);
      }
      const auto e = server.post("/api/evaluate", {{"miner", "powl"},
                                                   {"evaluation_family", "precision"},
                                                   {"precision_variant", "etconformance_token"}});
      if (error_set(e["error"])) return fail(e["error"]);
      if (error_set(e["precision"]["error"])) return fail(e["precision"]["error"]);
      return "E " + f_digits(e["precision"]["precision"], 6);
    }
    if (kind == "play") {
      // Every event of the playout carries its name; the names met are the
      // net's visible labels.
      const auto response = server.client.Get("/api/playout?miner=powl");
      if (!response) return "error: no response";
      if (response->status != 200) {
        JsonReader reader{response->body};
        return fail(reader.value()["error"]);
      }
      const auto& xml = response->body;
      std::size_t events = 0, named = 0;
      std::vector<std::string> names;
      const std::string tag = "      <string key=\"concept:name\" value=\"";
      for (std::size_t at = xml.find("<event>"); at != std::string::npos; at = xml.find("<event>", at + 1)) ++events;
      for (std::size_t at = xml.find(tag); at != std::string::npos; at = xml.find(tag, at + 1)) {
        ++named;
        const auto start = at + tag.size();
        const auto name = xml.substr(start, xml.find('"', start) - start);
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
      }
      std::sort(names.begin(), names.end());
      std::string out = "L " + std::to_string(events - named);
      for (const auto& name : names) out += " [" + name + "]";
      return out;
    }
    if (kind == "pal") {
      Fields fields = {{"miner", "powl"}, {"cost_profile", "standard"}};
      if (extra != "default") fields.push_back({"alignment_variant", extra});
      const auto a = server.post("/api/align", fields);
      const bool computed = a["computed"].kind == Json::Kind::kBool && a["computed"].text == "true";
      if (!computed) return error_set(a["error"]) ? fail(a["error"]) : std::string("error: uncomputed");
      std::vector<std::string> rows;
      for (const auto& v : a["variants"].items) {
        std::vector<std::string> moves;
        for (const auto& m : v["moves"].items) moves.push_back(move_text(m));
        std::sort(moves.begin(), moves.end());
        std::string row = v["cost"].text + " " + f_digits(v["fitness"], 6);
        for (const auto& m : moves) row += " " + m;
        const auto count = std::strtoll(v["count"].text.c_str(), nullptr, 10);
        for (long long k = 0; k < count; ++k) rows.push_back(row);
      }
      return "A " + sorted_join(rows, " ; ") + " F " + f_digits(a["fitness"], 6) + " " + a["fitting_traces"].text + " " +
             a["total_cost"].text;
    }
    if (kind == "pdec") {
      const auto& net = mined["powl_petri_net"];
      const auto& decoration = mined["powl_petri_net_decoration"];
      if (net.kind == Json::Kind::kNull || decoration.kind == Json::Kind::kNull) return "error: no powl decoration";
      const auto& activities = mined["activities"].items;
      std::vector<std::string> rows;
      const auto& transitions = net["transitions"].items;
      for (std::size_t t = 0; t < transitions.size(); ++t) {
        const auto& activity = transitions[t]["activity"];
        std::string label = "~";
        if (activity.kind != Json::Kind::kNull) {
          const auto at = static_cast<std::size_t>(std::strtoull(activity.text.c_str(), nullptr, 10));
          const auto& named = activities.at(at);
          label = lbl(named.kind == Json::Kind::kObject ? named["name"] : named);
        }
        rows.push_back(label + "=" + decoration["transitions"].items.at(t).text);
      }
      std::sort(rows.begin(), rows.end());
      std::string out = "D " + none_or(decoration["decorated_traces"]) + " ";
      for (std::size_t k = 0; k < rows.size(); ++k) out += (k ? " " : "") + rows[k];
      return out;
    }
    const auto map = server.get("/api/stochastic-map?miner=powl");
    if (error_set(map["error"])) return fail(map["error"]);
    std::vector<std::string> rows;
    for (const auto& t : map["map"]["transitions"].items) {
      const auto& d = t["distribution"].text;
      std::string row;
      if (d == "uniform" || d == "normal") row = d + " " + g10(t["parameter"]) + " " + g10(t["secondary_parameter"]);
      else if (d == "exponential") row = "exponential " + g10(t["parameter"]) + " 0";
      else if (d == "immediate" || d == "constant0") row = "immediate 0 0";
      else row = d + " " + t["parameter"].text;
      rows.push_back(row + " " + g10(t["activation_weight"]));
    }
    return "S " + sorted_join(rows, " ; ");
  }
  const auto mined = server.upload({});
  if (error_set(mined["error"])) return fail(mined["error"]);
  if (kind == "msd") {
    const auto d = server.post("/api/analyze-behavior", {{"family", "minimum_self_distance"},
                                                         {"source_id", server.source_id()},
                                                         {"variant", extra},
                                                         {"minimum_self_distance_activity_key", key}});
    if (error_set(d["error"])) return fail(d["error"]);
    const auto& result = d["result"];
    const auto& activities = result["activities"].items;
    std::vector<std::string> rows;
    for (const auto& x : result["minimum_self_distances"].items) {
      std::string label = "None";
      if (x["activity"].kind != Json::Kind::kNull) {
        const auto& named = activities.at(static_cast<std::size_t>(std::strtoull(x["activity"].text.c_str(), nullptr, 10)));
        label = lbl(named.kind == Json::Kind::kObject ? named["name"] : named);
      }
      rows.push_back(label + "=" + x["distance"].text);
    }
    return "M " + sorted_join(rows, " ; ");
  }
  Fields fields = {{"family", kind == "tp" ? "temporal_profile" : "temporal_conformance"},
                   {"source_id", server.source_id()},
                   {"variant", extra},
                   {"temporal_activity_attribute", key}};
  if (kind == "tpc") {
    fields.push_back({"temporal_profile_json", model});
    fields.push_back({"temporal_zeta", "0.5"});
  }
  const auto d = server.post("/api/analyze-behavior", fields);
  if (error_set(d["error"])) return fail(d["error"]);
  if (kind == "tp") {
    std::vector<std::string> rows;
    for (const auto& e : d["profile"].items)
      rows.push_back(lbl(e.items.at(0)) + "|" + lbl(e.items.at(1)) + "|" + f_digits(e.items.at(2), 6) + "|" +
                     f_digits(e.items.at(3), 6));
    return "P " + sorted_join(rows, " ; ");
  }
  std::string out = "C ";
  const auto& traces = d["conformance"]["traces"].items;
  for (std::size_t k = 0; k < traces.size(); ++k) {
    std::vector<std::string> rows;
    for (const auto& x : traces[k].items)
      rows.push_back(lbl(x["source"]) + "|" + lbl(x["target"]) + "|" + f_digits(x["elapsed_seconds"], 6) + "|" +
                     f_digits(x["standard_deviation_distance"], 6));
    out += (k ? " ; " : "") + sorted_join(rows, ",");
  }
  return out;
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
    std::string label, name, content, kind, key, extra, model, expected;
  };
  const std::string f0 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:47.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:02.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:32.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:10.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:52.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:22.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:02.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:10.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:48.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n</log>\n";
  const std::string f1 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:27.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:35.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:20.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:51.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:58.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:02.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:24.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:59.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:08.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:31.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:40.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:42.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f2 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:15.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:35.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:52.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:35.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:02.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:14.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:58.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:27.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:39.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:52.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:40.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n</log>\n";
  const std::string f3 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:15.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:25.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:10.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:42.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:18.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:23.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:48.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:33.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:46.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:06.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n</log>\n";
  const std::string f4 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:32.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:02.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:10.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:29.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:12.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:51.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:05.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:47.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:25.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:30.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:05.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:43.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:22.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:30.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:54.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:37.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:51.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:54.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:22.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:08:53.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:09:43.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:10:20.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:10:48.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:11:26.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n</log>\n";
  const std::string f5 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:39.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:46.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:36.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:13.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:25.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:04.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f6 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:42.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:06.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:51.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:36.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:53.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:18.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f7 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:32.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:33.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:13.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:58.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:12.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:33.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:34.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:15.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:50.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:56.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n</log>\n";
  const std::string f8 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:40.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:42.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:09.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:29.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:41.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:17.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:02.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:35.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:23.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:25.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:59.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:33.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"a\"/></event>\n</trace>\n</log>\n";
  const std::string f9 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:29.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:07.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:42.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:51.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:07.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:27.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:08.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:14.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:19.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:38.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:14.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n</log>\n";
  const std::string f10 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:40.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:08.000+00:00\"/><int key=\"K1\" value=\"1\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:36.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"1\"/><string key=\"K4\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n</log>\n";
  const std::string f11 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:29.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"9\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:01.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:36.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"false\"/><string key=\"K4\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:07.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:48.000+00:00\"/><string key=\"K1\"/><float key=\"K2\" value=\"1.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:54.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:56.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:14.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"1.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:52.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.123+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:38.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:08.000+00:00\"/><int key=\"K1\" value=\"-1\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:22.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"1\"/><string key=\"K4\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:48.000+00:00\"/><string key=\"K1\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"f\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:25.000+00:00\"/><int key=\"K1\" value=\"255\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:34.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"-3.5\"/><string key=\"K3\"/><string key=\"K4\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:50.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\"/></event>\n</trace>\n</log>\n";
  const std::string f12 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:24.000+00:00\"/><int key=\"K1\" value=\"2\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:35.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:07.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:31.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><string key=\"K4\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:01.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K5\"/><string key=\"K6\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:06.000+00:00\"/><int key=\"K1\" value=\"3\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:17.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:51.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:25.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:38.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:06.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:24.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:08.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:35.000+00:00\"/><int key=\"K1\" value=\"2\"/><string key=\"K2\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:40.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"1e3\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:13.000+00:00\"/><string key=\"K1\"/><float key=\"K2\" value=\"7.0\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f13 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:31.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:07.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:52.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"2.25\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:42.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:24.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><string key=\"K2\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:35.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:09.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:14.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"false\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:45.000+00:00\"/><int key=\"K1\" value=\"0\"/><string key=\"K2\"/><string key=\"K3\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:16.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"z\"/></event>\n</trace>\n</log>\n";
  const std::string f14 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:07.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:33.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><string key=\"K5\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:15.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:42.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:47.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><string key=\"K4\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:18.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"inf\"/><string key=\"K3\"/><string key=\"K4\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:26.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:52.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:17.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:54.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f15 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:08:00,True,p,1.5,2021-03-01 08:00:00,2,True,b\nc0,b,2021-03-01 08:09:00,False,q,3.0,2021-03-02 09:30:00,,False,1\nc1,c,2021-03-01 08:10:00,True,r,3.0,2021-03-01 08:00:00,3,,a\nc1,a,,True,q,2.5,2021-03-02 09:30:00,1,False,\nc1,a,2021-03-01 08:17:00,True,r,3.0,2021-03-01 08:00:00,3,False,a\nc2,c,2021-03-01 08:18:00,True,r,2.5,,,True,a\nc2,c,2021-03-01 08:21:00,False,,1.5,,1,True,b\nc2,d,2021-03-01 08:23:00,True,p,,,2,True,1\n";
  const std::string f16 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 18:00:00,1,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-12 14:00:00,18446744073709551625,,3.0,2021-03-01 08:00:00,2,True,1\nc0,a,2021-03-13 10:00:00,2,r,2.5,2021-03-01 08:00:00,3,False,a\nc0,c,2021-03-15 02:00:00,0,,1.5,2021-03-02 09:30:00,1,False,1\n";
  const std::string f17 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-11 08:00:00,0,,2.5,,3,True,\n";
  const std::string f18 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,9223372036854775809,p,1.5,2021-03-02 09:30:00,,True,a\nc0,a,2021-03-01 08:09:00,3,,3.0,2021-03-02 09:30:00,1,True,b\nc0,d,2021-03-01 08:11:00,9223372036854775809,p,3.0,,,True,a\n";
  const std::string f19 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,,0,r,1.5,2021-03-01 08:00:00,7,False,\nc0,a,2021-03-02 04:00:00,False,s,,2021-03-03 10:00:00,,False,a\nc0,d,2021-03-02 06:00:00,True,,-1.0,2021-03-02 09:30:00,,True,\nc0,d,2021-03-02 07:00:00,False,p,-1.0,2021-03-03 10:00:00,1,False,\nc0,a,2021-03-02 10:00:00,True,s,,2021-03-02 09:30:00,7,False,b\nc1,b,2021-03-02 09:00:00,1,,2.5,2021-03-02 09:30:00,3,True,a\nc1,b,2021-03-02 13:00:00,0,,,,,False,1\nc2,d,2021-03-01 13:00:00,1,,2.5,2021-03-03 10:00:00,7,False,b\nc3,a,2021-03-01 09:00:00,1,,,,,,\nc3,a,2021-03-01 11:00:00,False,q,,,,,\nc3,d,,0,,1.5,2021-03-01 08:00:00,,False,b\nc3,b,2021-03-01 15:00:00,2,q,1.5,2021-03-03 10:00:00,,False,\nc3,a,2021-03-01 19:00:00,True,q,1.5,2021-03-02 09:30:00,7,False,\n";
  const std::string f20 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:32:00,2021-03-01 08:31:00,p,1.5,,,True,b\nc0,c,2021-03-01 08:35:00,2021-03-01 08:34:00,r,1.5,2021-03-01 08:00:00,1,,1\nc0,a,,2021-03-01 08:33:00,q,2.5,,3,False,a\nc0,b,,2021-03-01 08:34:00,p,1.5,2021-03-01 08:00:00,3,False,1\nc1,a,2021-03-01 08:15:00,2021-03-01 08:14:00,p,3.0,2021-03-01 08:00:00,2,False,\nc2,c,,2021-03-01 08:05:00,p,3.0,,3,False,b\nc2,a,2021-03-01 08:07:00,2021-03-01 08:07:00,r,3.0,,2,True,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:04:00,r,,2021-03-02 09:30:00,,False,1\nc2,a,2021-03-01 08:11:00,2021-03-01 08:08:00,r,1.5,2021-03-02 09:30:00,3,,1\nc3,a,,2021-03-01 08:28:00,q,1.5,,3,,\nc3,c,2021-03-01 08:30:00,2021-03-01 08:29:00,r,1.5,2021-03-01 08:00:00,1,True,b\nc3,d,2021-03-01 08:31:00,2021-03-01 08:28:00,r,,,2,False,b\nc4,b,,2021-03-01 08:20:00,,3.0,,1,False,b\nc4,a,2021-03-01 08:23:00,2021-03-01 08:22:00,q,2.5,,1,False,\nc4,d,2021-03-01 08:26:00,2021-03-01 08:23:00,,1.5,2021-03-01 08:00:00,3,True,1\nc4,a,2021-03-01 08:30:00,2021-03-01 08:30:00,q,,2021-03-01 08:00:00,1,,1\n";
  const std::string f21 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,d,2021-03-01 08:02:00,2021-03-01 08:00:00,p,2.5,,,,\nc0,d,2021-03-01 08:04:00,2021-03-01 08:04:00,q,1.5,2021-03-01 08:00:00,1,True,a\nc0,b,2021-03-01 08:05:00,2021-03-01 08:05:00,q,2.5,,1,False,a\nc0,a,2021-03-01 08:07:00,2021-03-01 08:07:00,p,,2021-03-01 08:00:00,1,,\nc1,d,2021-03-01 08:30:00,,,2.5,2021-03-01 08:00:00,1,False,a\nc1,d,2021-03-01 08:30:00,,q,,2021-03-01 08:00:00,1,False,a\nc2,b,2021-03-01 08:05:00,2021-03-01 08:05:00,,2.5,2021-03-01 08:00:00,1,False,1\nc2,c,2021-03-01 08:07:00,2021-03-01 08:07:00,,1.5,2021-03-01 08:00:00,2,,1\n";
  const std::string f22 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:24:00,True,p,2.5,2021-03-01 08:00:00,1,True,\nc1,b,2021-03-01 08:03:00,True,r,3.0,,2,False,\nc1,d,2021-03-01 08:06:00,False,q,,,,True,\nc1,c,2021-03-01 08:06:00,True,q,2.5,,,True,\nc1,a,2021-03-01 08:10:00,True,q,1.5,2021-03-01 08:00:00,2,,b\nc1,b,2021-03-01 08:10:00,True,r,,2021-03-02 09:30:00,3,False,1\n";
  const std::string f23 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,c,2021-03-01 08:19:00,True,,2.5,,1,,\nc0,c,2021-03-01 08:19:00,True,p,,2021-03-01 08:00:00,1,True,a\nc0,a,2021-03-01 08:21:00,True,p,,,2,,a\nc1,b,2021-03-01 08:23:00,True,p,1.5,2021-03-01 08:00:00,2,False,1\nc1,c,2021-03-01 08:25:00,True,p,,2021-03-01 08:00:00,1,,a\n";
  const std::string f24 = "case:concept:name,concept:name,time:timestamp,start_timestamp,K1,K2,K3,K4,K5,K6\nc0,b,2021-03-01 08:05:00,late,,3.0,2021-03-01 08:00:00,2,True,1\nc1,a,2021-03-01 08:27:00,late,s,,2021-03-03 10:00:00,2,False,1\n";
  const std::string f25 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:44.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:14.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:26.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:55.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:43.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:55.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:08.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n</log>\n";
  const std::string f26 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:25.000+00:00\"/><int key=\"K1\" value=\"3\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:42.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:19.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:42.000+00:00\"/><int key=\"K1\" value=\"3\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"True\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:46.000+00:00\"/><int key=\"K1\" value=\"255\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:32.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:00.000+00:00\"/><float key=\"K2\" value=\"0.5\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:44.000+00:00\"/><boolean key=\"K3\" value=\"True\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:58.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:20.000+00:00\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><string key=\"K6\" value=\"\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:57.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"0.0\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n</log>\n";
  const std::string f27 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:06.000+00:00\"/><int key=\"K1\" value=\"100\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:50.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:33.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:40.000+00:00\"/><int key=\"K1\" value=\"7\"/><boolean key=\"K3\" value=\"1\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:46.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><int key=\"K5\" value=\"-1\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:27.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"7.0\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:14.000+00:00\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"g\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:57.000+00:00\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T08:00:00Z\"/><float key=\"K5\" value=\"3.0\"/><string key=\"K6\" value=\"010\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:05.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:13.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"B\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:42.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"K6\" value=\"a\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:10.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:13.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c5\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:58.000+00:00\"/><int key=\"K1\" value=\"100\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"10\"/><string key=\"K6\" value=\"B\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:10.000+00:00\"/><int key=\"K1\" value=\"9223372036854775807\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"a\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:06:35.000+00:00\"/><int key=\"K1\" value=\"7\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"1\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"10\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:07:18.000+00:00\"/><int key=\"K1\" value=\"18446744073709551615\"/><float key=\"K2\" value=\"NaN\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"z\"/></event>\n</trace>\n</log>\n";
  const std::string f28 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:28.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"1.0\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><int key=\"K5\" value=\"0\"/></event>\n</trace>\n</log>\n";
  const std::string f29 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:50.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:00.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"1\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:26.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"2.25\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:57.000+00:00\"/><int key=\"K1\" value=\"2\"/><float key=\"K2\" value=\"2.25\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"2\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:45.000+00:00\"/><int key=\"K1\" value=\"-9223372036854775808\"/><float key=\"K2\" value=\"1.0\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-02T08:00:00.000-05:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:25.000+00:00\"/><float key=\"K2\" value=\"1.5\"/><boolean key=\"K3\" value=\"FALSE\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"a\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:28.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"-3.5\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><float key=\"K5\" value=\"2.5\"/><string key=\"K6\" value=\"\"/></event>\n</trace>\n</log>\n";
  const std::string f30 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\">\n<trace><string key=\"concept:name\" value=\"c0\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:14.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:37.000+00:00\"/><int key=\"K1\" value=\"10\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c1\"/>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:54.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:55.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:10.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><int key=\"K5\" value=\"0\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:11.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:01:34.000+00:00\"/><float key=\"K2\" value=\"1e3\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2020-12-31T23:59:59.999999+00:00\"/><float key=\"K5\" value=\"1.0\"/><string key=\"K6\" value=\"010\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c2\"/>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:02.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c3\"/>\n<event><string key=\"concept:name\" value=\"d\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:42.000+00:00\"/><int key=\"K1\" value=\"0\"/><float key=\"K2\" value=\"0.5\"/><boolean key=\"K3\" value=\"FALSE\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"-1\"/></event>\n<event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:02:59.000+00:00\"/><int key=\"K1\" value=\"9223372036854775808\"/><float key=\"K2\" value=\"-0.0\"/><boolean key=\"K3\" value=\"true\"/><date key=\"K4\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"K5\" value=\"1\"/><string key=\"K6\" value=\"z\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:03:31.000+00:00\"/><int key=\"K1\" value=\"42\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:04:21.000+00:00\"/><int key=\"K1\" value=\"100\"/><boolean key=\"K3\" value=\"0\"/><date key=\"K4\" value=\"2021-03-01T09:00:00.000+01:00\"/><int key=\"K5\" value=\"0\"/><string key=\"K6\" value=\"9\"/></event>\n<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:08.000+00:00\"/><int key=\"K1\" value=\"42\"/><float key=\"K2\" value=\"inf\"/><boolean key=\"K3\" value=\"1\"/><date key=\"K4\" value=\"2021-03-03T10:30:00+02:00\"/><float key=\"K5\" value=\"0.5\"/><string key=\"K6\" value=\"x y\"/></event>\n</trace>\n<trace><string key=\"concept:name\" value=\"c4\"/>\n<event><string key=\"concept:name\" value=\"e\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:05:45.000+00:00\"/><int key=\"K1\" value=\"100\"/><float key=\"K2\" value=\"2.25\"/><boolean key=\"K3\" value=\"0\"/><int key=\"K5\" value=\"0\"/></event>\n</trace>\n</log>\n";
  const std::string m0 = "[[\"b\", \"c\", 15.0, 0.0], [\"b\", \"d\", 37.0, 2.8284271247461903], [\"c\", \"d\", 22.0, 2.8284271247461903], [\"d\", \"d\", 11.5, 10.606601717798213], [\"0\", \"42\", 22.0, 2.8284271247461903], [\"3\", \"0\", 15.0, 0.0], [\"3\", \"42\", 37.0, 2.8284271247461903], [\"42\", \"42\", 11.5, 10.606601717798213]]";
  const std::string m1 = "[[\"b\", \"c\", 15.0, 0.0], [\"b\", \"d\", 37.0, 2.8284271247461903], [\"c\", \"d\", 22.0, 2.8284271247461903], [\"d\", \"d\", 11.5, 10.606601717798213], [\"0.5\", \"1.5\", 15.0, 0.0], [\"0.5\", \"1000.0\", 37.0, 2.8284271247461903], [\"1.5\", \"1000.0\", 22.0, 2.8284271247461903], [\"1000.0\", \"1000.0\", 11.5, 10.606601717798213]]";
  const std::string m2 = "[[\"b\", \"c\", 15.0, 0.0], [\"b\", \"d\", 37.0, 2.8284271247461903], [\"c\", \"d\", 22.0, 2.8284271247461903], [\"d\", \"d\", 11.5, 10.606601717798213], [\"False\", \"False\", 16.75, 8.770214744615247], [\"True\", \"False\", 29.666666666666668, 12.858201014657272]]";
  const std::string m3 = "[[\"b\", \"c\", 15.0, 0.0], [\"b\", \"d\", 37.0, 2.8284271247461903], [\"c\", \"d\", 22.0, 2.8284271247461903], [\"d\", \"d\", 11.5, 10.606601717798213], [\"2020-12-31 23:59:59.999999+00:00\", \"2020-12-31 23:59:59.999999+00:00\", 11.5, 10.606601717798213], [\"2021-03-01 08:00:00+00:00\", \"2020-12-31 23:59:59.999999+00:00\", 37.0, 2.8284271247461903], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 09:00:00+00:00\", 15.0, 0.0], [\"2021-03-01 09:00:00+00:00\", \"2020-12-31 23:59:59.999999+00:00\", 22.0, 2.8284271247461903]]";
  const std::string m4 = "[[\"b\", \"c\", 15.0, 0.0], [\"b\", \"d\", 37.0, 2.8284271247461903], [\"c\", \"d\", 22.0, 2.8284271247461903], [\"d\", \"d\", 11.5, 10.606601717798213], [\"-1.0\", \"0.5\", 15.0, 0.0], [\"-1.0\", \"2.5\", 37.0, 2.8284271247461903], [\"0.5\", \"2.5\", 22.0, 2.8284271247461903], [\"2.5\", \"2.5\", 11.5, 10.606601717798213]]";
  const std::string m5 = "[[\"b\", \"c\", 15.0, 0.0], [\"b\", \"d\", 37.0, 2.8284271247461903], [\"c\", \"d\", 22.0, 2.8284271247461903], [\"d\", \"d\", 11.5, 10.606601717798213], [\"-1\", \"9\", 37.0, 2.8284271247461903], [\"-1\", \"B\", 15.0, 0.0], [\"9\", \"9\", 11.5, 10.606601717798213], [\"B\", \"9\", 22.0, 2.8284271247461903]]";
  const std::string m6 = "[[\"c\", \"b\", 36.75, 2.4748737341529163], [\"c\", \"c\", 2.25, 1.7677669529663689], [\"c\", \"d\", 16.916666666666668, 12.997756216619338], [\"d\", \"b\", 19.833333333333332, 14.37300710823359], [\"d\", \"d\", 19.0, 8.845903006477066], [\"g\", \"c\", 24.5, 2.8284271247461903], [\"g\", \"g\", 4.0, 0.0], [\"-1\", \"-1\", 4.0, 0.0], [\"-1\", \"9223372036854775808\", 24.5, 2.8284271247461903], [\"9223372036854775807\", \"42\", 19.833333333333332, 14.37300710823359], [\"9223372036854775807\", \"9223372036854775807\", 19.0, 8.845903006477066], [\"9223372036854775808\", \"42\", 36.75, 2.4748737341529163], [\"9223372036854775808\", \"9223372036854775807\", 16.916666666666668, 12.997756216619338], [\"9223372036854775808\", \"9223372036854775808\", 2.25, 1.7677669529663689]]";
  const std::string m7 = "[[\"c\", \"b\", 36.75, 2.4748737341529163], [\"c\", \"c\", 2.25, 1.7677669529663689], [\"c\", \"d\", 16.916666666666668, 12.997756216619338], [\"d\", \"b\", 19.833333333333332, 14.37300710823359], [\"d\", \"d\", 19.0, 8.845903006477066], [\"g\", \"c\", 24.5, 2.8284271247461903], [\"g\", \"g\", 4.0, 0.0], [\"-3.5\", \"-3.5\", 19.0, 8.845903006477066], [\"-3.5\", \"1.5\", 19.833333333333332, 14.37300710823359], [\"-0.0\", \"-0.0\", 4.0, 0.0], [\"-0.0\", \"2.25\", 24.5, 2.8284271247461903], [\"2.25\", \"-3.5\", 16.916666666666668, 12.997756216619338], [\"2.25\", \"1.5\", 36.75, 2.4748737341529163], [\"2.25\", \"2.25\", 2.25, 1.7677669529663689]]";
  const std::string m8 = "[[\"c\", \"b\", 36.75, 2.4748737341529163], [\"c\", \"c\", 2.25, 1.7677669529663689], [\"c\", \"d\", 16.916666666666668, 12.997756216619338], [\"d\", \"b\", 19.833333333333332, 14.37300710823359], [\"d\", \"d\", 19.0, 8.845903006477066], [\"g\", \"c\", 24.5, 2.8284271247461903], [\"g\", \"g\", 4.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 4.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-03 10:30:00+00:00\", 24.5, 2.8284271247461903], [\"2021-03-01 08:00:00.123000+00:00\", \"2021-03-01 08:00:00+00:00\", 19.833333333333332, 14.37300710823359], [\"2021-03-01 08:00:00.123000+00:00\", \"2021-03-01 08:00:00.123000+00:00\", 19.0, 8.845903006477066], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 08:00:00+00:00\", 36.75, 2.4748737341529163], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 08:00:00.123000+00:00\", 16.916666666666668, 12.997756216619338], [\"2021-03-03 10:30:00+00:00\", \"2021-03-03 10:30:00+00:00\", 2.25, 1.7677669529663689]]";
  const std::string m9 = "[[\"c\", \"b\", 36.75, 2.4748737341529163], [\"c\", \"c\", 2.25, 1.7677669529663689], [\"c\", \"d\", 16.916666666666668, 12.997756216619338], [\"d\", \"b\", 19.833333333333332, 14.37300710823359], [\"d\", \"d\", 19.0, 8.845903006477066], [\"g\", \"c\", 24.5, 2.8284271247461903], [\"g\", \"g\", 4.0, 0.0], [\"-1.0\", \"-1.0\", 19.0, 8.845903006477066], [\"-1.0\", \"1.0\", 19.833333333333332, 14.37300710823359], [\"2.0\", \"-1.0\", 16.916666666666668, 12.997756216619338], [\"2.0\", \"1.0\", 36.75, 2.4748737341529163], [\"2.0\", \"2.0\", 2.25, 1.7677669529663689], [\"2.5\", \"2.0\", 24.5, 2.8284271247461903], [\"2.5\", \"2.5\", 4.0, 0.0]]";
  const std::string m10 = "[[\"b\", \"c\", 36.5, 0.0], [\"b\", \"d\", 49.0, 0.0], [\"b\", \"g\", 45.833333333333336, 25.662878508330536], [\"c\", \"d\", 12.5, 0.0], [\"c\", \"g\", 21.25, 21.5667568261897], [\"d\", \"f\", 36.75, 9.545941546018392], [\"d\", \"g\", 16.25, 10.960155108391486], [\"f\", \"f\", 13.5, 0.0], [\"g\", \"c\", 14.5, 0.0], [\"g\", \"d\", 16.75, 14.495689014324224], [\"g\", \"f\", 28.25, 9.545941546018392], [\"g\", \"g\", 34.0, 15.548311805466213], [\"1\", \"1\", 13.5, 0.0], [\"7\", \"10\", 21.25, 21.5667568261897], [\"7\", \"42\", 12.5, 0.0], [\"10\", \"1\", 28.25, 9.545941546018392], [\"10\", \"7\", 14.5, 0.0], [\"10\", \"10\", 34.0, 15.548311805466213], [\"10\", \"42\", 16.75, 14.495689014324224], [\"42\", \"1\", 36.75, 9.545941546018392], [\"42\", \"10\", 16.25, 10.960155108391486], [\"9223372036854775807\", \"7\", 36.5, 0.0], [\"9223372036854775807\", \"10\", 45.833333333333336, 25.662878508330536], [\"9223372036854775807\", \"42\", 49.0, 0.0]]";
  const std::string m11 = "[[\"b\", \"c\", 36.5, 0.0], [\"b\", \"d\", 49.0, 0.0], [\"b\", \"g\", 45.833333333333336, 25.662878508330536], [\"c\", \"d\", 12.5, 0.0], [\"c\", \"g\", 21.25, 21.5667568261897], [\"d\", \"f\", 36.75, 9.545941546018392], [\"d\", \"g\", 16.25, 10.960155108391486], [\"f\", \"f\", 13.5, 0.0], [\"g\", \"c\", 14.5, 0.0], [\"g\", \"d\", 16.75, 14.495689014324224], [\"g\", \"f\", 28.25, 9.545941546018392], [\"g\", \"g\", 34.0, 15.548311805466213], [\"-3.5\", \"-3.5\", 34.0, 15.548311805466213], [\"-3.5\", \"-0.0\", 28.25, 9.545941546018392], [\"-3.5\", \"2.25\", 14.5, 0.0], [\"-0.0\", \"-0.0\", 13.5, 0.0], [\"2.25\", \"-3.5\", 21.25, 21.5667568261897], [\"7.0\", \"-3.5\", 45.833333333333336, 25.662878508330536], [\"7.0\", \"2.25\", 36.5, 0.0]]";
  const std::string m12 = "[[\"b\", \"c\", 36.5, 0.0], [\"b\", \"d\", 49.0, 0.0], [\"b\", \"g\", 45.833333333333336, 25.662878508330536], [\"c\", \"d\", 12.5, 0.0], [\"c\", \"g\", 21.25, 21.5667568261897], [\"d\", \"f\", 36.75, 9.545941546018392], [\"d\", \"g\", 16.25, 10.960155108391486], [\"f\", \"f\", 13.5, 0.0], [\"g\", \"c\", 14.5, 0.0], [\"g\", \"d\", 16.75, 14.495689014324224], [\"g\", \"f\", 28.25, 9.545941546018392], [\"g\", \"g\", 34.0, 15.548311805466213], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 28.666666666666668, 13.344162269197218], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00.123000+00:00\", 14.5, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 09:00:00+00:00\", 16.75, 14.495689014324224], [\"2021-03-01 08:00:00.123000+00:00\", \"2021-03-01 08:00:00+00:00\", 21.25, 21.5667568261897], [\"2021-03-01 08:00:00.123000+00:00\", \"2021-03-01 09:00:00+00:00\", 12.5, 0.0], [\"2021-03-01 09:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 26.5, 14.508618128546908], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 08:00:00+00:00\", 45.833333333333336, 25.662878508330536], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 08:00:00.123000+00:00\", 36.5, 0.0], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 09:00:00+00:00\", 49.0, 0.0]]";
  const std::string m13 = "[[\"b\", \"c\", 36.5, 0.0], [\"b\", \"d\", 49.0, 0.0], [\"b\", \"g\", 45.833333333333336, 25.662878508330536], [\"c\", \"d\", 12.5, 0.0], [\"c\", \"g\", 21.25, 21.5667568261897], [\"d\", \"f\", 36.75, 9.545941546018392], [\"d\", \"g\", 16.25, 10.960155108391486], [\"f\", \"f\", 13.5, 0.0], [\"g\", \"c\", 14.5, 0.0], [\"g\", \"d\", 16.75, 14.495689014324224], [\"g\", \"f\", 28.25, 9.545941546018392], [\"g\", \"g\", 34.0, 15.548311805466213], [\"0.0\", \"0.0\", 34.0, 15.548311805466213], [\"0.0\", \"1.0\", 16.75, 14.495689014324224], [\"0.0\", \"3.0\", 28.25, 9.545941546018392], [\"0.0\", \"10.0\", 14.5, 0.0], [\"1.0\", \"0.0\", 16.25, 10.960155108391486], [\"1.0\", \"3.0\", 36.75, 9.545941546018392], [\"2.5\", \"0.0\", 45.833333333333336, 25.662878508330536], [\"2.5\", \"1.0\", 49.0, 0.0], [\"2.5\", \"10.0\", 36.5, 0.0], [\"3.0\", \"3.0\", 13.5, 0.0], [\"10.0\", \"0.0\", 21.25, 21.5667568261897], [\"10.0\", \"1.0\", 12.5, 0.0]]";
  const std::string m14 = "[[\"a\", \"a\", 32.5, 22.664950915455343], [\"a\", \"c\", 15.166666666666666, 8.082903768654761], [\"b\", \"a\", 43.5, 27.242736034889496], [\"b\", \"c\", 27.5, 0.0], [\"c\", \"a\", 28.833333333333332, 11.184066046538412], [\"c\", \"c\", 10.0, 0.0], [\"-9223372036854775808\", \"-9223372036854775808\", 32.5, 22.664950915455343], [\"-9223372036854775808\", \"2\", 15.166666666666666, 8.082903768654761], [\"2\", \"-9223372036854775808\", 28.833333333333332, 11.184066046538412], [\"2\", \"2\", 10.0, 0.0], [\"7\", \"-9223372036854775808\", 43.5, 27.242736034889496], [\"7\", \"2\", 27.5, 0.0]]";
  const std::string m15 = "[[\"a\", \"a\", 9.5, 0.0], [\"a\", \"c\", 29.2, 11.127668219353057], [\"a\", \"d\", 51.5, 0.0], [\"a\", \"f\", 7.0, 0.0], [\"a\", \"g\", 41.0, 12.020815280171307], [\"b\", \"b\", 29.5, 0.0], [\"b\", \"c\", 14.0, 0.0], [\"b\", \"d\", 19.5, 0.0], [\"b\", \"f\", 23.5, 0.0], [\"b\", \"g\", 35.5, 0.0], [\"c\", \"a\", 8.75, 6.7175144212722016], [\"c\", \"b\", 16.75, 12.141526537740905], [\"c\", \"c\", 23.166666666666668, 10.39631344916713], [\"c\", \"d\", 35.75, 3.8890872965260113], [\"c\", \"f\", 42.5, 0.0], [\"c\", \"g\", 23.125, 22.32851316142658], [\"d\", \"f\", 4.0, 0.0], [\"d\", \"g\", 16.0, 0.0], [\"e\", \"a\", 23.75, 6.7175144212722016], [\"e\", \"c\", 32.5, 24.748737341529164], [\"f\", \"c\", 30.5, 13.435028842544403], [\"f\", \"g\", 27.25, 21.5667568261897], [\"g\", \"b\", 23.25, 20.859650045003153], [\"g\", \"c\", 14.75, 10.960155108391486], [\"g\", \"d\", 19.0, 0.0], [\"-9223372036854775808\", \"-9223372036854775808\", 9.5, 0.0], [\"-9223372036854775808\", \"42\", 51.5, 0.0], [\"-9223372036854775808\", \"255\", 41.0, 12.020815280171307], [\"-9223372036854775808\", \"9223372036854775807\", 7.0, 0.0], [\"-9223372036854775808\", \"9223372036854775808\", 29.2, 11.127668219353057], [\"3\", \"3\", 29.5, 0.0], [\"3\", \"42\", 19.5, 0.0], [\"3\", \"255\", 35.5, 0.0], [\"3\", \"9223372036854775807\", 23.5, 0.0], [\"3\", \"9223372036854775808\", 14.0, 0.0], [\"10\", \"-9223372036854775808\", 23.75, 6.7175144212722016], [\"10\", \"9223372036854775808\", 32.5, 24.748737341529164], [\"42\", \"255\", 16.0, 0.0], [\"42\", \"9223372036854775807\", 4.0, 0.0], [\"255\", \"3\", 23.25, 20.859650045003153], [\"255\", \"42\", 19.0, 0.0], [\"255\", \"9223372036854775808\", 14.75, 10.960155108391486], [\"9223372036854775807\", \"255\", 27.25, 21.5667568261897], [\"9223372036854775807\", \"9223372036854775808\", 30.5, 13.435028842544403], [\"9223372036854775808\", \"-9223372036854775808\", 8.75, 6.7175144212722016], [\"9223372036854775808\", \"3\", 16.75, 12.141526537740905], [\"9223372036854775808\", \"42\", 35.75, 3.8890872965260113], [\"9223372036854775808\", \"255\", 23.125, 22.32851316142658], [\"9223372036854775808\", \"9223372036854775807\", 42.5, 0.0], [\"9223372036854775808\", \"9223372036854775808\", 23.166666666666668, 10.39631344916713]]";
  const std::string m16 = "[[\"b\", \"a\", 44.0, 0.0], [\"b\", \"e\", 18.5, 0.0], [\"b\", \"f\", 24.5, 0.0], [\"e\", \"a\", 25.5, 0.0], [\"e\", \"f\", 6.0, 0.0], [\"f\", \"a\", 19.5, 0.0], [\"-9223372036854775808\", \"255\", 19.5, 0.0], [\"10\", \"-9223372036854775808\", 24.5, 0.0], [\"10\", \"255\", 44.0, 0.0], [\"10\", \"9223372036854775808\", 18.5, 0.0], [\"9223372036854775808\", \"-9223372036854775808\", 6.0, 0.0], [\"9223372036854775808\", \"255\", 25.5, 0.0]]";
  const std::string m17 = "[]";
  const std::string m18 = "[[\"a\", \"a\", 120.0, 127.27922061357856], [\"a\", \"d\", 0.0, 0.0], [\"c\", \"a\", 30.0, 0.0], [\"c\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 0.0], [\"p\", \"r\", 60.0, 0.0], [\"q\", \"q\", 210.0, 0.0], [\"r\", \"r\", 30.0, 0.0]]";
  const std::string m19 = "[[\"a\", \"a\", 120.0, 127.27922061357856], [\"a\", \"d\", 0.0, 0.0], [\"c\", \"a\", 30.0, 0.0], [\"c\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 0.0], [\"1.5\", \"1.5\", 60.0, 0.0], [\"2.5\", \"1.5\", 0.0, 0.0], [\"3.0\", \"1.5\", 30.0, 0.0]]";
  const std::string m20 = "[[\"a\", \"a\", 120.0, 127.27922061357856], [\"a\", \"d\", 0.0, 0.0], [\"c\", \"a\", 30.0, 0.0], [\"c\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 120.0, 0.0], [\"2021-03-02 09:30:00+00:00\", \"2021-03-02 09:30:00+00:00\", 30.0, 0.0]]";
  const std::string m21 = "[[\"a\", \"a\", 120.0, 127.27922061357856], [\"a\", \"d\", 0.0, 0.0], [\"c\", \"a\", 30.0, 0.0], [\"c\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 0.0], [\"1.0\", \"1.0\", 210.0, 0.0], [\"1.0\", \"3.0\", 0.0, 0.0], [\"2.0\", \"3.0\", 30.0, 0.0], [\"3.0\", \"1.0\", 120.0, 0.0]]";
  const std::string m22 = "[[\"a\", \"a\", 120.0, 127.27922061357856], [\"a\", \"d\", 0.0, 0.0], [\"c\", \"a\", 30.0, 0.0], [\"c\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 0.0], [\"False\", \"True\", 0.0, 0.0]]";
  const std::string m23 = "[[\"a\", \"a\", 120.0, 127.27922061357856], [\"a\", \"d\", 0.0, 0.0], [\"c\", \"a\", 30.0, 0.0], [\"c\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 0.0], [\"1\", \"1\", 60.0, 51.96152422706632], [\"b\", \"1\", 60.0, 0.0]]";
  const std::string m24 = "[[\"b\", \"a\", 60.0, 0.0], [\"b\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 42.42640687119285], [\"d\", \"b\", 60.0, 42.42640687119285], [\"d\", \"d\", 60.0, 0.0], [\"1.5\", \"2.5\", 30.0, 0.0], [\"2.5\", \"1.5\", 60.0, 0.0], [\"2.5\", \"2.5\", 90.0, 0.0]]";
  const std::string m25 = "[[\"b\", \"a\", 60.0, 0.0], [\"b\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 42.42640687119285], [\"d\", \"b\", 60.0, 42.42640687119285], [\"d\", \"d\", 60.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 75.0, 21.213203435596427]]";
  const std::string m26 = "[[\"b\", \"a\", 60.0, 0.0], [\"b\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 42.42640687119285], [\"d\", \"b\", 60.0, 42.42640687119285], [\"d\", \"d\", 60.0, 0.0], [\"1.0\", \"1.0\", 60.0, 30.0], [\"1.0\", \"2.0\", 60.0, 0.0]]";
  const std::string m27 = "[[\"b\", \"a\", 60.0, 0.0], [\"b\", \"c\", 60.0, 0.0], [\"d\", \"a\", 120.0, 42.42640687119285], [\"d\", \"b\", 60.0, 42.42640687119285], [\"d\", \"d\", 60.0, 0.0], [\"True\", \"False\", 30.0, 0.0]]";
  const std::string m28 = "[[\"a\", \"a\", 15.0, 0.0], [\"a\", \"b\", 11.375, 7.110731326663946], [\"b\", \"a\", 24.0, 0.0], [\"b\", \"b\", 24.333333333333332, 15.782374134880131], [\"-9223372036854775808\", \"-9223372036854775808\", 15.0, 0.0], [\"-9223372036854775808\", \"2\", 11.375, 7.110731326663946], [\"2\", \"-9223372036854775808\", 24.0, 0.0], [\"2\", \"2\", 24.333333333333332, 15.782374134880131]]";
  const std::string m29 = "[[\"a\", \"a\", 15.0, 0.0], [\"a\", \"b\", 11.375, 7.110731326663946], [\"b\", \"a\", 24.0, 0.0], [\"b\", \"b\", 24.333333333333332, 15.782374134880131], [\"0.0\", \"0.0\", 24.333333333333332, 15.782374134880131], [\"2.25\", \"0.0\", 13.5, 10.606601717798213], [\"2.25\", \"2.25\", 15.0, 0.0]]";
  const std::string m30 = "[[\"a\", \"a\", 15.0, 0.0], [\"a\", \"b\", 11.375, 7.110731326663946], [\"b\", \"a\", 24.0, 0.0], [\"b\", \"b\", 24.333333333333332, 15.782374134880131], [\"False\", \"False\", 24.333333333333332, 15.782374134880131], [\"False\", \"True\", 24.0, 0.0], [\"True\", \"False\", 11.375, 7.110731326663946], [\"True\", \"True\", 15.0, 0.0]]";
  const std::string m31 = "[[\"a\", \"a\", 15.0, 0.0], [\"a\", \"b\", 11.375, 7.110731326663946], [\"b\", \"a\", 24.0, 0.0], [\"b\", \"b\", 24.333333333333332, 15.782374134880131], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 15.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 09:00:00+00:00\", 9.25, 4.596194077712559], [\"2021-03-01 09:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 24.0, 0.0], [\"2021-03-01 09:00:00+00:00\", \"2021-03-01 09:00:00+00:00\", 24.333333333333332, 15.782374134880131]]";
  const std::string m32 = "[[\"a\", \"a\", 15.0, 0.0], [\"a\", \"b\", 11.375, 7.110731326663946], [\"b\", \"a\", 24.0, 0.0], [\"b\", \"b\", 24.333333333333332, 15.782374134880131], [\"-1.0\", \"-1.0\", 15.0, 0.0], [\"-1.0\", \"1.0\", 13.166666666666666, 7.522189397597839], [\"1.0\", \"-1.0\", 24.0, 0.0], [\"1.0\", \"1.0\", 36.5, 0.0]]";
  const std::string m33 = "[[\"a\", \"a\", 15.0, 0.0], [\"a\", \"b\", 11.375, 7.110731326663946], [\"b\", \"a\", 24.0, 0.0], [\"b\", \"b\", 24.333333333333332, 15.782374134880131], [\"010\", \"010\", 24.333333333333332, 15.782374134880131], [\"010\", \"10\", 24.0, 0.0], [\"10\", \"010\", 11.375, 7.110731326663946], [\"10\", \"10\", 15.0, 0.0]]";
  const std::string m34 = "[[\"a\", \"a\", 36.5, 0.0], [\"a\", \"b\", 18.5, 0.0], [\"a\", \"c\", 11.5, 0.0], [\"a\", \"e\", 13.5, 0.0], [\"b\", \"a\", 36.75, 25.809397513308983], [\"b\", \"c\", 30.0, 0.0], [\"b\", \"e\", 32.0, 0.0], [\"c\", \"a\", 25.0, 0.0], [\"c\", \"e\", 2.0, 0.0], [\"e\", \"a\", 23.0, 0.0], [\"e\", \"d\", 22.0, 0.0], [\"3.0\", \"7.0\", 25.0, 0.0], [\"3.0\", \"255.0\", 2.0, 0.0], [\"7.0\", \"3.0\", 11.5, 0.0], [\"7.0\", \"7.0\", 36.5, 0.0], [\"7.0\", \"255.0\", 13.5, 0.0], [\"255.0\", \"7.0\", 23.0, 0.0], [\"9.223372036854776e+18\", \"3.0\", 30.0, 0.0], [\"9.223372036854776e+18\", \"7.0\", 36.75, 25.809397513308983], [\"9.223372036854776e+18\", \"255.0\", 32.0, 0.0]]";
  const std::string m35 = "[[\"a\", \"a\", 36.5, 0.0], [\"a\", \"b\", 18.5, 0.0], [\"a\", \"c\", 11.5, 0.0], [\"a\", \"e\", 13.5, 0.0], [\"b\", \"a\", 36.75, 25.809397513308983], [\"b\", \"c\", 30.0, 0.0], [\"b\", \"e\", 32.0, 0.0], [\"c\", \"a\", 25.0, 0.0], [\"c\", \"e\", 2.0, 0.0], [\"e\", \"a\", 23.0, 0.0], [\"e\", \"d\", 22.0, 0.0], [\"0.0\", \"2.25\", 30.0, 0.0]]";
  const std::string m36 = "[[\"a\", \"a\", 36.5, 0.0], [\"a\", \"b\", 18.5, 0.0], [\"a\", \"c\", 11.5, 0.0], [\"a\", \"e\", 13.5, 0.0], [\"b\", \"a\", 36.75, 25.809397513308983], [\"b\", \"c\", 30.0, 0.0], [\"b\", \"e\", 32.0, 0.0], [\"c\", \"a\", 25.0, 0.0], [\"c\", \"e\", 2.0, 0.0], [\"e\", \"a\", 23.0, 0.0], [\"e\", \"d\", 22.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 36.5, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-03 10:30:00+00:00\", 13.5, 0.0], [\"2021-03-01 09:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 36.75, 25.809397513308983], [\"2021-03-01 09:00:00+00:00\", \"2021-03-03 10:30:00+00:00\", 32.0, 0.0], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 08:00:00+00:00\", 23.0, 0.0]]";
  const std::string m37 = "[[\"a\", \"a\", 36.5, 0.0], [\"a\", \"b\", 18.5, 0.0], [\"a\", \"c\", 11.5, 0.0], [\"a\", \"e\", 13.5, 0.0], [\"b\", \"a\", 36.75, 25.809397513308983], [\"b\", \"c\", 30.0, 0.0], [\"b\", \"e\", 32.0, 0.0], [\"c\", \"a\", 25.0, 0.0], [\"c\", \"e\", 2.0, 0.0], [\"e\", \"a\", 23.0, 0.0], [\"e\", \"d\", 22.0, 0.0], [\"0.0\", \"0.0\", 36.5, 0.0], [\"0.0\", \"1.0\", 12.5, 1.4142135623730951], [\"1.0\", \"0.0\", 24.0, 1.4142135623730951], [\"1.0\", \"1.0\", 2.0, 0.0], [\"1.0\", \"2.5\", 22.0, 0.0], [\"3.0\", \"0.0\", 36.75, 25.809397513308983], [\"3.0\", \"1.0\", 31.0, 1.4142135623730951]]";
  const std::string m38 = "[[\"a\", \"a\", 4.0, 0.0], [\"a\", \"b\", 40.0, 0.0], [\"a\", \"c\", 18.5, 0.0], [\"a\", \"d\", 6.0, 0.0], [\"b\", \"a\", 27.5, 2.8284271247461903], [\"b\", \"c\", 3.5, 0.0], [\"b\", \"g\", 21.5, 0.0], [\"c\", \"b\", 21.5, 0.0], [\"c\", \"c\", 13.25, 16.617009357883866], [\"d\", \"b\", 34.0, 0.0], [\"d\", \"c\", 12.5, 0.0], [\"e\", \"a\", 71.5, 2.8284271247461903], [\"e\", \"b\", 44.0, 0.0], [\"e\", \"g\", 43.0, 31.81980515339464], [\"g\", \"a\", 28.5, 26.083200212652844], [\"g\", \"b\", 23.5, 0.0], [\"g\", \"g\", 45.0, 0.0], [\"0.0\", \"2.0\", 20.5, 0.0], [\"0.0\", \"100.0\", 71.5, 2.8284271247461903], [\"2.0\", \"100.0\", 51.0, 2.8284271247461903], [\"7.0\", \"7.0\", 13.25, 16.617009357883866], [\"7.0\", \"1.8446744073709552e+19\", 21.5, 0.0], [\"100.0\", \"7.0\", 18.5, 0.0], [\"100.0\", \"100.0\", 4.0, 0.0], [\"100.0\", \"9.223372036854776e+18\", 6.0, 0.0], [\"100.0\", \"1.8446744073709552e+19\", 40.0, 0.0], [\"9.223372036854776e+18\", \"7.0\", 12.5, 0.0], [\"9.223372036854776e+18\", \"1.8446744073709552e+19\", 34.0, 0.0], [\"1.8446744073709552e+19\", \"7.0\", 3.5, 0.0]]";
  const std::string m39 = "[[\"a\", \"a\", 4.0, 0.0], [\"a\", \"b\", 40.0, 0.0], [\"a\", \"c\", 18.5, 0.0], [\"a\", \"d\", 6.0, 0.0], [\"b\", \"a\", 27.5, 2.8284271247461903], [\"b\", \"c\", 3.5, 0.0], [\"b\", \"g\", 21.5, 0.0], [\"c\", \"b\", 21.5, 0.0], [\"c\", \"c\", 13.25, 16.617009357883866], [\"d\", \"b\", 34.0, 0.0], [\"d\", \"c\", 12.5, 0.0], [\"e\", \"a\", 71.5, 2.8284271247461903], [\"e\", \"b\", 44.0, 0.0], [\"e\", \"g\", 43.0, 31.81980515339464], [\"g\", \"a\", 28.5, 26.083200212652844], [\"g\", \"b\", 23.5, 0.0], [\"g\", \"g\", 45.0, 0.0], [\"0.5\", \"0.5\", 1.5, 0.0], [\"1.0\", \"0.5\", 12.5, 0.0], [\"2.25\", \"2.25\", 4.0, 0.0], [\"7.0\", \"2.25\", 51.0, 2.8284271247461903], [\"1000.0\", \"2.25\", 71.5, 2.8284271247461903], [\"1000.0\", \"7.0\", 20.5, 0.0]]";
  const std::string m40 = "[[\"a\", \"a\", 4.0, 0.0], [\"a\", \"b\", 40.0, 0.0], [\"a\", \"c\", 18.5, 0.0], [\"a\", \"d\", 6.0, 0.0], [\"b\", \"a\", 27.5, 2.8284271247461903], [\"b\", \"c\", 3.5, 0.0], [\"b\", \"g\", 21.5, 0.0], [\"c\", \"b\", 21.5, 0.0], [\"c\", \"c\", 13.25, 16.617009357883866], [\"d\", \"b\", 34.0, 0.0], [\"d\", \"c\", 12.5, 0.0], [\"e\", \"a\", 71.5, 2.8284271247461903], [\"e\", \"b\", 44.0, 0.0], [\"e\", \"g\", 43.0, 31.81980515339464], [\"g\", \"a\", 28.5, 26.083200212652844], [\"g\", \"b\", 23.5, 0.0], [\"g\", \"g\", 45.0, 0.0], [\"2020-12-31 23:59:59.999999+00:00\", \"2021-03-01 08:00:00+00:00\", 43.0, 31.81980515339464], [\"2020-12-31 23:59:59.999999+00:00\", \"2021-03-01 09:00:00+00:00\", 71.5, 2.8284271247461903], [\"2020-12-31 23:59:59.999999+00:00\", \"2021-03-03 10:30:00+00:00\", 44.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 45.0, 0.0], [\"2021-03-01 08:00:00+00:00\", \"2021-03-01 09:00:00+00:00\", 28.5, 26.083200212652844], [\"2021-03-01 08:00:00+00:00\", \"2021-03-03 10:30:00+00:00\", 28.75, 7.424621202458749], [\"2021-03-01 09:00:00+00:00\", \"2021-03-01 08:00:00+00:00\", 6.0, 0.0], [\"2021-03-01 09:00:00+00:00\", \"2021-03-01 09:00:00+00:00\", 4.0, 0.0], [\"2021-03-01 09:00:00+00:00\", \"2021-03-03 10:30:00+00:00\", 40.0, 0.0], [\"2021-03-02 08:00:00+00:00\", \"2021-03-02 08:00:00+00:00\", 1.5, 0.0], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 08:00:00+00:00\", 21.5, 0.0], [\"2021-03-03 10:30:00+00:00\", \"2021-03-01 09:00:00+00:00\", 27.5, 2.8284271247461903]]";
  const std::string m41 = "[[\"a\", \"a\", 4.0, 0.0], [\"a\", \"b\", 40.0, 0.0], [\"a\", \"c\", 18.5, 0.0], [\"a\", \"d\", 6.0, 0.0], [\"b\", \"a\", 27.5, 2.8284271247461903], [\"b\", \"c\", 3.5, 0.0], [\"b\", \"g\", 21.5, 0.0], [\"c\", \"b\", 21.5, 0.0], [\"c\", \"c\", 13.25, 16.617009357883866], [\"d\", \"b\", 34.0, 0.0], [\"d\", \"c\", 12.5, 0.0], [\"e\", \"a\", 71.5, 2.8284271247461903], [\"e\", \"b\", 44.0, 0.0], [\"e\", \"g\", 43.0, 31.81980515339464], [\"g\", \"a\", 28.5, 26.083200212652844], [\"g\", \"b\", 23.5, 0.0], [\"g\", \"g\", 45.0, 0.0], [\"-1.0\", \"0.5\", 44.0, 0.0], [\"-1.0\", \"3.0\", 43.0, 31.81980515339464], [\"-1.0\", \"10.0\", 71.5, 2.8284271247461903], [\"0.5\", \"1.0\", 3.5, 0.0], [\"0.5\", \"3.0\", 21.5, 0.0], [\"0.5\", \"10.0\", 27.5, 2.8284271247461903], [\"1.0\", \"0.5\", 27.75, 8.838834764831844], [\"1.0\", \"1.0\", 12.5, 0.0], [\"3.0\", \"0.5\", 23.5, 0.0], [\"3.0\", \"3.0\", 45.0, 0.0], [\"3.0\", \"10.0\", 28.5, 26.083200212652844], [\"10.0\", \"0.5\", 40.0, 0.0], [\"10.0\", \"1.0\", 12.25, 8.838834764831844], [\"10.0\", \"10.0\", 4.0, 0.0]]";
  const std::vector<Case> cases = {
      {"f90000.xes psm K1", "f90000.xes", f0, "psm", "K1", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.4444444444 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5555555556 ; immediate 0 0 0.8 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90000.xes tp K1 dataframe", "f90000.xes", f0, "tp", "K1", "dataframe", "", "P 0|42|44.000000|5.656854 ; 3|0|30.000000|0.000000 ; 3|42|74.000000|5.656854 ; 42|42|23.000000|21.213203"},
      {"f90000.xes tpc K1 dataframe", "f90000.xes", f0, "tpc", "K1", "dataframe", m0, "error: You are trying to merge on int64 and str columns for key 'K1'. If you wish to proceed you should use pd.concat"},
      {"f90000.xes msd K1 pandas", "f90000.xes", f0, "msd", "K1", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"f90000.xes msd K1 polars", "f90000.xes", f0, "msd", "K1", "polars", "", "M 42=0"},
      {"f90000.xes pev K1 mine", "f90000.xes", f0, "pev", "K1", "mine", "", "error: sequence item 0: expected str instance, int found"},
      {"f90000.xes pev K1 evaluate", "f90000.xes", f0, "pev", "K1", "evaluate", "", "error: sequence item 0: expected str instance, int found"},
      {"f90000.xes play K1", "f90000.xes", f0, "play", "K1", "", "", "L 0 [0] [100] [3] [42]"},
      {"f90000.xes tp K2 dataframe", "f90000.xes", f0, "tp", "K2", "dataframe", "", "P 0.5|1.5|30.000000|0.000000 ; 0.5|1000.0|74.000000|5.656854 ; 1.5|1000.0|44.000000|5.656854 ; 1000.0|1000.0|23.000000|21.213203"},
      {"f90000.xes tpc K2 dataframe", "f90000.xes", f0, "tpc", "K2", "dataframe", m1, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"f90000.xes msd K2 pandas", "f90000.xes", f0, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes msd K2 polars", "f90000.xes", f0, "msd", "K2", "polars", "", "M 1000.0=0"},
      {"f90000.xes pev K2 mine", "f90000.xes", f0, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90000.xes pev K2 evaluate", "f90000.xes", f0, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90000.xes psm K3", "f90000.xes", f0, "psm", "K3", "", "", "S immediate 0 0 0.4 ; immediate 0 0 0.4166666667 ; immediate 0 0 0.5833333333 ; immediate 0 0 0.6 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90000.xes tp K3 dataframe", "f90000.xes", f0, "tp", "K3", "dataframe", "", "P False|False|33.500000|17.540429 ; True|False|59.333333|25.716402"},
      {"f90000.xes tpc K3 dataframe", "f90000.xes", f0, "tpc", "K3", "dataframe", m2, "C  ;  ;  ;  ; "},
      {"f90000.xes msd K3 pandas", "f90000.xes", f0, "msd", "K3", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"f90000.xes msd K3 polars", "f90000.xes", f0, "msd", "K3", "polars", "", "M False=0"},
      {"f90000.xes pev K3 mine", "f90000.xes", f0, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90000.xes pev K3 evaluate", "f90000.xes", f0, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90000.xes play K3", "f90000.xes", f0, "play", "K3", "", "", "L 0 [False] [True]"},
      {"f90000.xes psm K4", "f90000.xes", f0, "psm", "K4", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.4444444444 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.8 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 8 40 0.5555555556"},
      {"f90000.xes tp K4 dataframe", "f90000.xes", f0, "tp", "K4", "dataframe", "", "P 2020-12-31 23:59:59.999999+00:00|2020-12-31 23:59:59.999999+00:00|23.000000|21.213203 ; 2021-03-01 08:00:00+00:00|2020-12-31 23:59:59.999999+00:00|74.000000|5.656854 ; 2021-03-01 08:00:00+00:00|2021-03-01 09:00:00+00:00|30.000000|0.000000 ; 2021-03-01 09:00:00+00:00|2020-12-31 23:59:59.999999+00:00|44.000000|5.656854"},
      {"f90000.xes tpc K4 dataframe", "f90000.xes", f0, "tpc", "K4", "dataframe", m3, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"f90000.xes msd K4 pandas", "f90000.xes", f0, "msd", "K4", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"f90000.xes msd K4 polars", "f90000.xes", f0, "msd", "K4", "polars", "", "M 2020-12-31 23:59:59.999999+00:00=0"},
      {"f90000.xes pev K4 mine", "f90000.xes", f0, "pev", "K4", "mine", "", "error: sequence item 0: expected str instance, datetime.datetime found"},
      {"f90000.xes pev K4 evaluate", "f90000.xes", f0, "pev", "K4", "evaluate", "", "error: sequence item 0: expected str instance, datetime.datetime found"},
      {"f90000.xes play K4", "f90000.xes", f0, "play", "K4", "", "", "L 0 [2020-12-31 23:59:59.999999+00:00] [2021-03-01 08:00:00+00:00] [2021-03-01 09:00:00+00:00] [2021-03-03 10:30:00+00:00]"},
      {"f90000.xes psm K5", "f90000.xes", f0, "psm", "K5", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.4444444444 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.8 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 8 40 0.5555555556"},
      {"f90000.xes tp K5 dataframe", "f90000.xes", f0, "tp", "K5", "dataframe", "", "P -1.0|0.5|30.000000|0.000000 ; -1.0|2.5|74.000000|5.656854 ; 0.5|2.5|44.000000|5.656854 ; 2.5|2.5|23.000000|21.213203"},
      {"f90000.xes tpc K5 dataframe", "f90000.xes", f0, "tpc", "K5", "dataframe", m4, "error: You are trying to merge on float64 and str columns for key 'K5'. If you wish to proceed you should use pd.concat"},
      {"f90000.xes msd K5 pandas", "f90000.xes", f0, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90000.xes msd K5 polars", "f90000.xes", f0, "msd", "K5", "polars", "", "M 2.5=0"},
      {"f90000.xes pev K5 mine", "f90000.xes", f0, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90000.xes pev K5 evaluate", "f90000.xes", f0, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90000.xes play K5", "f90000.xes", f0, "play", "K5", "", "", "L 0 [-1] [0.5] [10] [2.5]"},
      {"f90000.xes psm K6", "f90000.xes", f0, "psm", "K6", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.4444444444 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.8 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 8 40 0.5555555556"},
      {"f90000.xes tp K6 dataframe", "f90000.xes", f0, "tp", "K6", "dataframe", "", "P -1|9|74.000000|5.656854 ; -1|B|30.000000|0.000000 ; 9|9|23.000000|21.213203 ; B|9|44.000000|5.656854"},
      {"f90000.xes tpc K6 dataframe", "f90000.xes", f0, "tpc", "K6", "dataframe", m5, "C  ;  ; 9|9|38.000000|2.498444 ; -1|9|70.000000|11.667262,-1|9|78.000000|14.495689,-1|B|30.000000|9223372036854775808.000000,B|9|40.000000|6.363961,B|9|48.000000|9.192388 ; "},
      {"f90000.xes msd K6 pandas", "f90000.xes", f0, "msd", "K6", "pandas", "", "M 9=0"},
      {"f90000.xes msd K6 polars", "f90000.xes", f0, "msd", "K6", "polars", "", "M 9=0"},
      {"f90000.xes pev K6 mine", "f90000.xes", f0, "pev", "K6", "mine", "", "E 0.950000"},
      {"f90000.xes pev K6 evaluate", "f90000.xes", f0, "pev", "K6", "evaluate", "", "E 0.950000"},
      {"f90000.xes play K6", "f90000.xes", f0, "play", "K6", "", "", "L 0 [-1] [9] [B] [x y]"},
      {"f90001.xes psm K1", "f90001.xes", f1, "psm", "K1", "", "", "S immediate 0 0 0.25 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.75 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 4 35 1"},
      {"f90001.xes tpc K1 dataframe", "f90001.xes", f1, "tpc", "K1", "dataframe", m6, "C  ;  ;  ; "},
      {"f90001.xes msd K1 pandas", "f90001.xes", f1, "msd", "K1", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"f90001.xes pev K1 mine", "f90001.xes", f1, "pev", "K1", "mine", "", "error: sequence item 0: expected str instance, int found"},
      {"f90001.xes pev K1 evaluate", "f90001.xes", f1, "pev", "K1", "evaluate", "", "error: sequence item 0: expected str instance, int found"},
      {"f90001.xes psm K2", "f90001.xes", f1, "psm", "K2", "", "", "S immediate 0 0 0.25 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.75 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 4 35 1"},
      {"f90001.xes tpc K2 dataframe", "f90001.xes", f1, "tpc", "K2", "dataframe", m7, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"f90001.xes msd K2 pandas", "f90001.xes", f1, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes msd K2 polars", "f90001.xes", f1, "msd", "K2", "polars", "", "M -0.0=0 ; -3.5=0 ; 2.25=0"},
      {"f90001.xes pev K2 mine", "f90001.xes", f1, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90001.xes pev K2 evaluate", "f90001.xes", f1, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90001.xes play K2", "f90001.xes", f1, "play", "K2", "", "", "L 0 [-0.0] [-3.5] [1.5] [2.25]"},
      {"f90001.xes psm K3", "f90001.xes", f1, "psm", "K3", "", "", "S immediate 0 0 0.4166666667 ; immediate 0 0 0.4285714286 ; immediate 0 0 0.5714285714 ; immediate 0 0 0.5833333333 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90001.xes msd K3 pandas", "f90001.xes", f1, "msd", "K3", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"f90001.xes msd K3 polars", "f90001.xes", f1, "msd", "K3", "polars", "", "M False=0 ; True=0"},
      {"f90001.xes pev K3 mine", "f90001.xes", f1, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90001.xes pev K3 evaluate", "f90001.xes", f1, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90001.xes tpc K4 dataframe", "f90001.xes", f1, "tpc", "K4", "dataframe", m8, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"f90001.xes msd K4 pandas", "f90001.xes", f1, "msd", "K4", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"f90001.xes msd K4 polars", "f90001.xes", f1, "msd", "K4", "polars", "", "M 2021-03-01 08:00:00+00:00=0 ; 2021-03-01 08:00:00.123000+00:00=0 ; 2021-03-03 10:30:00+00:00=0"},
      {"f90001.xes pev K4 mine", "f90001.xes", f1, "pev", "K4", "mine", "", "error: sequence item 0: expected str instance, datetime.datetime found"},
      {"f90001.xes pev K4 evaluate", "f90001.xes", f1, "pev", "K4", "evaluate", "", "error: sequence item 0: expected str instance, datetime.datetime found"},
      {"f90001.xes tpc K5 dataframe", "f90001.xes", f1, "tpc", "K5", "dataframe", m9, "error: You are trying to merge on float64 and str columns for key 'K5'. If you wish to proceed you should use pd.concat"},
      {"f90001.xes msd K5 pandas", "f90001.xes", f1, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90001.xes msd K5 polars", "f90001.xes", f1, "msd", "K5", "polars", "", "M -1.0=0 ; 2.0=0 ; 2.5=0"},
      {"f90001.xes pev K5 mine", "f90001.xes", f1, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90001.xes pev K5 evaluate", "f90001.xes", f1, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90002.xes tpc K1 dataframe", "f90002.xes", f2, "tpc", "K1", "dataframe", m10, "error: You are trying to merge on uint64 and str columns for key 'K1'. If you wish to proceed you should use pd.concat"},
      {"f90002.xes msd K1 pandas", "f90002.xes", f2, "msd", "K1", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"f90002.xes pev K1 mine", "f90002.xes", f2, "pev", "K1", "mine", "", "error: sequence item 0: expected str instance, int found"},
      {"f90002.xes pev K1 evaluate", "f90002.xes", f2, "pev", "K1", "evaluate", "", "error: sequence item 0: expected str instance, int found"},
      {"f90002.xes tpc K2 dataframe", "f90002.xes", f2, "tpc", "K2", "dataframe", m11, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"f90002.xes msd K2 pandas", "f90002.xes", f2, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90002.xes psm K3", "f90002.xes", f2, "psm", "K3", "", "", "S immediate 0 0 0.2222222222 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.7777777778 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90002.xes msd K3 pandas", "f90002.xes", f2, "msd", "K3", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"f90002.xes msd K3 polars", "f90002.xes", f2, "msd", "K3", "polars", "", "M False=0"},
      {"f90002.xes pev K3 mine", "f90002.xes", f2, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90002.xes pev K3 evaluate", "f90002.xes", f2, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90002.xes tpc K4 dataframe", "f90002.xes", f2, "tpc", "K4", "dataframe", m12, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"f90002.xes msd K4 pandas", "f90002.xes", f2, "msd", "K4", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"f90002.xes msd K4 polars", "f90002.xes", f2, "msd", "K4", "polars", "", "M 2021-03-01 08:00:00+00:00=0"},
      {"f90002.xes pev K4 mine", "f90002.xes", f2, "pev", "K4", "mine", "", "error: sequence item 0: expected str instance, datetime.datetime found"},
      {"f90002.xes pev K4 evaluate", "f90002.xes", f2, "pev", "K4", "evaluate", "", "error: sequence item 0: expected str instance, datetime.datetime found"},
      {"f90002.xes psm K5", "f90002.xes", f2, "psm", "K5", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.25 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.6 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.75 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90002.xes tpc K5 dataframe", "f90002.xes", f2, "tpc", "K5", "dataframe", m13, "error: You are trying to merge on float64 and str columns for key 'K5'. If you wish to proceed you should use pd.concat"},
      {"f90002.xes msd K5 pandas", "f90002.xes", f2, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90002.xes msd K5 polars", "f90002.xes", f2, "msd", "K5", "polars", "", "M 0.0=1 ; 3.0=0"},
      {"f90002.xes pev K5 mine", "f90002.xes", f2, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90002.xes pev K5 evaluate", "f90002.xes", f2, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90003.xes tpc K1 dataframe", "f90003.xes", f3, "tpc", "K1", "dataframe", m14, "error: You are trying to merge on int64 and str columns for key 'K1'. If you wish to proceed you should use pd.concat"},
      {"f90003.xes msd K2 polars", "f90003.xes", f3, "msd", "K2", "polars", "", "M 1.0=0 ; inf=0"},
      {"f90003.xes pev K2 mine", "f90003.xes", f3, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90003.xes pev K2 evaluate", "f90003.xes", f3, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90003.xes psm K3", "f90003.xes", f3, "psm", "K3", "", "", "S immediate 0 0 0.07692307692 ; immediate 0 0 0.4 ; immediate 0 0 0.5 ; immediate 0 0 0.6428571429 ; immediate 0 0 1 ; uniform 5 77 0.6666666667"},
      {"f90003.xes msd K5 polars", "f90003.xes", f3, "msd", "K5", "polars", "", "M 10.0=0 ; 2.5=0"},
      {"f90003.xes psm K6", "f90003.xes", f3, "psm", "K6", "", "", "S immediate 0 0 0.3333333333 ; immediate 0 0 0.375 ; immediate 0 0 0.4 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.6 ; immediate 0 0 0.625 ; immediate 0 0 0.6666666667 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90003.xes play K6", "f90003.xes", f3, "play", "K6", "", "", "L 0 [] [10] [B] [a]"},
      {"f90004.xes tpc K1 dataframe", "f90004.xes", f4, "tpc", "K1", "dataframe", m15, "C  ;  ;  ;  ; "},
      {"f90004.xes psm K2", "f90004.xes", f4, "psm", "K2", "", "", "S immediate 0 0 0.0652173913 ; immediate 0 0 0.07142857143 ; immediate 0 0 0.1363636364 ; immediate 0 0 0.2 ; immediate 0 0 0.2 ; immediate 0 0 0.2142857143 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.4444444444 ; immediate 0 0 0.5 ; immediate 0 0 0.5555555556 ; immediate 0 0 0.6428571429 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.8 ; immediate 0 0 0.8 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 31 70 0.347826087"},
      {"f90004.xes play K6", "f90004.xes", f4, "play", "K6", "", "", "L 0 [] [-1] [10] [9] [B] [a] [z]"},
      {"f90005.xes tpc K1 dataframe", "f90005.xes", f5, "tpc", "K1", "dataframe", m16, "C  ;  ; "},
      {"f90006.xes play K6", "f90006.xes", f6, "play", "K6", "", "", "L 0 [] [010] [B] [a]"},
      {"f90007.xes msd K2 polars", "f90007.xes", f7, "msd", "K2", "polars", "", "M inf=0"},
      {"f90008.xes psm K2", "f90008.xes", f8, "psm", "K2", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.2 ; immediate 0 0 0.25 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.35 ; immediate 0 0 0.3636363636 ; immediate 0 0 0.375 ; immediate 0 0 0.5 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.7142857143 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90010.xes psm K6", "f90010.xes", f9, "psm", "K6", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.2 ; immediate 0 0 0.25 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.5 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90200.xes psm K4", "f90200.xes", f10, "psm", "K4", "", "", "error: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"f90200.xes msd K4 polars", "f90200.xes", f10, "msd", "K4", "polars", "", "M "},
      {"f90200.xes pev K4 mine", "f90200.xes", f10, "pev", "K4", "mine", "", "error: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"f90200.xes pev K4 evaluate", "f90200.xes", f10, "pev", "K4", "evaluate", "", "error: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"f90200.xes play K4", "f90200.xes", f10, "play", "K4", "", "", "error: '<' not supported between instances of 'datetime.datetime' and 'NoneType'"},
      {"f90201.xes psm K1", "f90201.xes", f11, "psm", "K1", "", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes msd K1 polars", "f90201.xes", f11, "msd", "K1", "polars", "", "M 3.0=0"},
      {"f90201.xes pev K1 mine", "f90201.xes", f11, "pev", "K1", "mine", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes pev K1 evaluate", "f90201.xes", f11, "pev", "K1", "evaluate", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes play K1", "f90201.xes", f11, "play", "K1", "", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes psm K2", "f90201.xes", f11, "psm", "K2", "", "", "error: '<' not supported between instances of 'NoneType' and 'float'"},
      {"f90201.xes pev K2 mine", "f90201.xes", f11, "pev", "K2", "mine", "", "error: '<' not supported between instances of 'NoneType' and 'float'"},
      {"f90201.xes pev K2 evaluate", "f90201.xes", f11, "pev", "K2", "evaluate", "", "error: '<' not supported between instances of 'NoneType' and 'float'"},
      {"f90201.xes play K2", "f90201.xes", f11, "play", "K2", "", "", "error: '<' not supported between instances of 'NoneType' and 'float'"},
      {"f90201.xes psm K3", "f90201.xes", f11, "psm", "K3", "", "", "error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes pev K3 mine", "f90201.xes", f11, "pev", "K3", "mine", "", "error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes pev K3 evaluate", "f90201.xes", f11, "pev", "K3", "evaluate", "", "error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes play K3", "f90201.xes", f11, "play", "K3", "", "", "error: '<' not supported between instances of 'NoneType' and 'bool'"},
      {"f90201.xes psm K5", "f90201.xes", f11, "psm", "K5", "", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes pev K5 mine", "f90201.xes", f11, "pev", "K5", "mine", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes pev K5 evaluate", "f90201.xes", f11, "pev", "K5", "evaluate", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes play K5", "f90201.xes", f11, "play", "K5", "", "", "error: '<' not supported between instances of 'NoneType' and 'int'"},
      {"f90201.xes psm K6", "f90201.xes", f11, "psm", "K6", "", "", "error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes msd K6 pandas", "f90201.xes", f11, "msd", "K6", "pandas", "", "M -1=2 ; 10=0"},
      {"f90201.xes pev K6 mine", "f90201.xes", f11, "pev", "K6", "mine", "", "error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes pev K6 evaluate", "f90201.xes", f11, "pev", "K6", "evaluate", "", "error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90201.xes play K6", "f90201.xes", f11, "play", "K6", "", "", "error: '<' not supported between instances of 'NoneType' and 'str'"},
      {"f90202.xes msd K1 polars", "f90202.xes", f12, "msd", "K1", "polars", "", "M 2.0=0 ; 3.0=0"},
      {"f90202.xes msd K6 pandas", "f90202.xes", f12, "msd", "K6", "pandas", "", "M 010=0 ; 10=0"},
      {"f90203.xes psm K1", "f90203.xes", f13, "psm", "K1", "", "", "S immediate 0 0 0.2 ; immediate 0 0 0.25 ; immediate 0 0 0.4285714286 ; immediate 0 0 0.75 ; immediate 0 0 0.8 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 11 45 0.5714285714"},
      {"f90205.xes psm K1", "f90205.xes", f14, "psm", "K1", "", "", "S immediate 0 0 0.3333333333 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.6666666667 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90205.xes psm K2", "f90205.xes", f14, "psm", "K2", "", "", "S immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90205.xes msd K6 pandas", "f90205.xes", f14, "msd", "K6", "pandas", "", "M 10=0 ; a=0"},
      {"s68500.csv psm K1", "s68500.csv", f15, "psm", "K1", "", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv tp K1 dataframe", "s68500.csv", f15, "tp", "K1", "dataframe", "", "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv tpc K1 dataframe", "s68500.csv", f15, "tpc", "K1", "dataframe", m17, "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv msd K1 pandas", "s68500.csv", f15, "msd", "K1", "pandas", "", "M r=0"},
      {"s68500.csv msd K1 polars", "s68500.csv", f15, "msd", "K1", "polars", "", "M r=0"},
      {"s68500.csv pev K1 mine", "s68500.csv", f15, "pev", "K1", "mine", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv pev K1 evaluate", "s68500.csv", f15, "pev", "K1", "evaluate", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv play K1", "s68500.csv", f15, "play", "K1", "", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68500.csv tp K2 dataframe", "s68500.csv", f15, "tp", "K2", "dataframe", "", "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv tpc K2 dataframe", "s68500.csv", f15, "tpc", "K2", "dataframe", m17, "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv msd K2 pandas", "s68500.csv", f15, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv msd K2 polars", "s68500.csv", f15, "msd", "K2", "polars", "", "M 3.0=0"},
      {"s68500.csv psm K3", "s68500.csv", f15, "psm", "K3", "", "", "S immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.6666666667 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"s68500.csv tp K3 dataframe", "s68500.csv", f15, "tp", "K3", "dataframe", "", "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv tpc K3 dataframe", "s68500.csv", f15, "tpc", "K3", "dataframe", m17, "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv msd K3 pandas", "s68500.csv", f15, "msd", "K3", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"s68500.csv msd K3 polars", "s68500.csv", f15, "msd", "K3", "polars", "", "M 2021-03-01 08:00:00+00:00=0 ; None=0"},
      {"s68500.csv pev K3 mine", "s68500.csv", f15, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, Timestamp found"},
      {"s68500.csv pev K3 evaluate", "s68500.csv", f15, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, Timestamp found"},
      {"s68500.csv play K3", "s68500.csv", f15, "play", "K3", "", "", "L 0 [2021-03-01 08:00:00+00:00] [2021-03-02 09:30:00+00:00] [NaT]"},
      {"s68500.csv psm K4", "s68500.csv", f15, "psm", "K4", "", "", "S immediate 0 0 0.3333333333 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.6666666667 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"s68500.csv tp K4 dataframe", "s68500.csv", f15, "tp", "K4", "dataframe", "", "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv tpc K4 dataframe", "s68500.csv", f15, "tpc", "K4", "dataframe", m17, "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv msd K4 pandas", "s68500.csv", f15, "msd", "K4", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"s68500.csv msd K4 polars", "s68500.csv", f15, "msd", "K4", "polars", "", "M 3.0=0"},
      {"s68500.csv pev K4 mine", "s68500.csv", f15, "pev", "K4", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"s68500.csv pev K4 evaluate", "s68500.csv", f15, "pev", "K4", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"s68500.csv tp K5 dataframe", "s68500.csv", f15, "tp", "K5", "dataframe", "", "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv tpc K5 dataframe", "s68500.csv", f15, "tpc", "K5", "dataframe", m17, "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv msd K5 pandas", "s68500.csv", f15, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"s68500.csv msd K5 polars", "s68500.csv", f15, "msd", "K5", "polars", "", "M True=0"},
      {"s68500.csv psm K6", "s68500.csv", f15, "psm", "K6", "", "", "S immediate 0 0 0.3333333333 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.6666666667 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 60 120 1"},
      {"s68500.csv tp K6 dataframe", "s68500.csv", f15, "tp", "K6", "dataframe", "", "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv tpc K6 dataframe", "s68500.csv", f15, "tpc", "K6", "dataframe", m17, "error: Invalid comparison between dtype=datetime64[us, UTC] and ndarray"},
      {"s68500.csv msd K6 pandas", "s68500.csv", f15, "msd", "K6", "pandas", "", "M a=0"},
      {"s68500.csv msd K6 polars", "s68500.csv", f15, "msd", "K6", "polars", "", "M a=0"},
      {"s68500.csv pev K6 mine", "s68500.csv", f15, "pev", "K6", "mine", "", "E 1.000000"},
      {"s68500.csv pev K6 evaluate", "s68500.csv", f15, "pev", "K6", "evaluate", "", "E 1.000000"},
      {"s68500.csv play K6", "s68500.csv", f15, "play", "K6", "", "", "L 0 [1] [a] [b]"},
      {"s68501.csv msd K1 pandas", "s68501.csv", f16, "msd", "K1", "pandas", "", "M "},
      {"s68501.csv msd K1 polars", "s68501.csv", f16, "msd", "K1", "polars", "", "M None=1"},
      {"s68501.csv psm K2", "s68501.csv", f16, "psm", "K2", "", "", "S immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"s68501.csv msd K2 pandas", "s68501.csv", f16, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"s68501.csv pev K2 mine", "s68501.csv", f16, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"s68501.csv pev K2 evaluate", "s68501.csv", f16, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"s68501.csv play K2", "s68501.csv", f16, "play", "K2", "", "", "L 0 [1.5] [2.5] [3.0] [nan]"},
      {"s68501.csv msd K3 pandas", "s68501.csv", f16, "msd", "K3", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"s68501.csv msd K3 polars", "s68501.csv", f16, "msd", "K3", "polars", "", "M 2021-03-01 08:00:00+00:00=0"},
      {"s68501.csv pev K3 mine", "s68501.csv", f16, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, Timestamp found"},
      {"s68501.csv pev K3 evaluate", "s68501.csv", f16, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, Timestamp found"},
      {"s68501.csv msd K4 pandas", "s68501.csv", f16, "msd", "K4", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"s68501.csv msd K4 polars", "s68501.csv", f16, "msd", "K4", "polars", "", "M 1=2"},
      {"s68501.csv pev K4 mine", "s68501.csv", f16, "pev", "K4", "mine", "", "error: sequence item 0: expected str instance, int found"},
      {"s68501.csv pev K4 evaluate", "s68501.csv", f16, "pev", "K4", "evaluate", "", "error: sequence item 0: expected str instance, int found"},
      {"s68501.csv play K4", "s68501.csv", f16, "play", "K4", "", "", "L 0 [1] [2] [3]"},
      {"s68501.csv psm K5", "s68501.csv", f16, "psm", "K5", "", "", "S immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"s68501.csv msd K5 pandas", "s68501.csv", f16, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"s68501.csv pev K5 mine", "s68501.csv", f16, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"s68501.csv pev K5 evaluate", "s68501.csv", f16, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"s68501.csv play K5", "s68501.csv", f16, "play", "K5", "", "", "L 0 [False] [True]"},
      {"s68502.csv psm K1", "s68502.csv", f17, "psm", "K1", "", "", "S immediate 0 0 1"},
      {"s68502.csv pev K1 mine", "s68502.csv", f17, "pev", "K1", "mine", "", "E 1.000000"},
      {"s68502.csv pev K1 evaluate", "s68502.csv", f17, "pev", "K1", "evaluate", "", "E 1.000000"},
      {"s68502.csv play K1", "s68502.csv", f17, "play", "K1", "", "", "L 0 [nan]"},
      {"s68502.csv msd K2 pandas", "s68502.csv", f17, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"s68502.csv pev K2 mine", "s68502.csv", f17, "pev", "K2", "mine", "", "E 1.000000"},
      {"s68502.csv pev K2 evaluate", "s68502.csv", f17, "pev", "K2", "evaluate", "", "E 1.000000"},
      {"s68502.csv msd K3 pandas", "s68502.csv", f17, "msd", "K3", "pandas", "", "M "},
      {"s68502.csv msd K3 polars", "s68502.csv", f17, "msd", "K3", "polars", "", "M "},
      {"s68502.csv pev K3 mine", "s68502.csv", f17, "pev", "K3", "mine", "", "E 1.000000"},
      {"s68502.csv pev K3 evaluate", "s68502.csv", f17, "pev", "K3", "evaluate", "", "E 1.000000"},
      {"s68502.csv msd K4 pandas", "s68502.csv", f17, "msd", "K4", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"s68502.csv pev K4 mine", "s68502.csv", f17, "pev", "K4", "mine", "", "E 1.000000"},
      {"s68502.csv pev K4 evaluate", "s68502.csv", f17, "pev", "K4", "evaluate", "", "E 1.000000"},
      {"s68502.csv psm K5", "s68502.csv", f17, "psm", "K5", "", "", "S immediate 0 0 1"},
      {"s68502.csv msd K5 pandas", "s68502.csv", f17, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"s68502.csv pev K5 mine", "s68502.csv", f17, "pev", "K5", "mine", "", "E 1.000000"},
      {"s68502.csv pev K5 evaluate", "s68502.csv", f17, "pev", "K5", "evaluate", "", "E 1.000000"},
      {"s68503.csv pev K2 mine", "s68503.csv", f18, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"s68503.csv pev K2 evaluate", "s68503.csv", f18, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"s68503.csv msd K3 pandas", "s68503.csv", f18, "msd", "K3", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"s68503.csv msd K3 polars", "s68503.csv", f18, "msd", "K3", "polars", "", "M 2021-03-02 09:30:00+00:00=0"},
      {"s68503.csv pev K3 mine", "s68503.csv", f18, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, Timestamp found"},
      {"s68503.csv pev K3 evaluate", "s68503.csv", f18, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, Timestamp found"},
      {"s68503.csv msd K4 polars", "s68503.csv", f18, "msd", "K4", "polars", "", "M None=1"},
      {"s68503.csv pev K4 mine", "s68503.csv", f18, "pev", "K4", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"s68503.csv pev K4 evaluate", "s68503.csv", f18, "pev", "K4", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"s68503.csv pev K5 mine", "s68503.csv", f18, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"s68503.csv pev K5 evaluate", "s68503.csv", f18, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"s68504.csv msd K1 pandas", "s68504.csv", f19, "msd", "K1", "pandas", "", "M q=0 ; s=2"},
      {"s68504.csv msd K1 polars", "s68504.csv", f19, "msd", "K1", "polars", "", "M None=0 ; q=0 ; s=2"},
      {"s68504.csv msd K2 polars", "s68504.csv", f19, "msd", "K2", "polars", "", "M -1.0=0 ; 1.5=0 ; None=0"},
      {"s68504.csv pev K2 mine", "s68504.csv", f19, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"s68504.csv pev K2 evaluate", "s68504.csv", f19, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"s68504.csv msd K4 polars", "s68504.csv", f19, "msd", "K4", "polars", "", "M None=0"},
      {"s68504.csv psm K5", "s68504.csv", f19, "psm", "K5", "", "", "S immediate 0 0 0.2857142857 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.4285714286 ; immediate 0 0 0.5714285714 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.7142857143 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"s68504.csv msd K5 polars", "s68504.csv", f19, "msd", "K5", "polars", "", "M False=0 ; None=0"},
      {"s68504.csv pev K5 mine", "s68504.csv", f19, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"s68504.csv pev K5 evaluate", "s68504.csv", f19, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"s68504.csv psm K6", "s68504.csv", f19, "psm", "K6", "", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68504.csv msd K6 pandas", "s68504.csv", f19, "msd", "K6", "pandas", "", "M "},
      {"s68504.csv msd K6 polars", "s68504.csv", f19, "msd", "K6", "polars", "", "M None=0"},
      {"s68504.csv pev K6 mine", "s68504.csv", f19, "pev", "K6", "mine", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68504.csv pev K6 evaluate", "s68504.csv", f19, "pev", "K6", "evaluate", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68504.csv play K6", "s68504.csv", f19, "play", "K6", "", "", "error: '<' not supported between instances of 'float' and 'str'"},
      {"s68505.csv tp K1 dataframe", "s68505.csv", f20, "tp", "K1", "dataframe", "", "P p|r|120.000000|0.000000 ; q|q|420.000000|0.000000 ; r|r|60.000000|0.000000"},
      {"s68505.csv tpc K1 dataframe", "s68505.csv", f20, "tpc", "K1", "dataframe", m18, "C p|r|120.000000|9223372036854775808.000000 ;  ; r|r|60.000000|9223372036854775808.000000,r|r|60.000000|9223372036854775808.000000 ;  ; q|q|420.000000|9223372036854775808.000000"},
      {"s68505.csv tp K2 dataframe", "s68505.csv", f20, "tp", "K2", "dataframe", "", "P 1.5|1.5|120.000000|0.000000 ; 2.5|1.5|0.000000|0.000000 ; 3.0|1.5|60.000000|0.000000"},
      {"s68505.csv tpc K2 dataframe", "s68505.csv", f20, "tpc", "K2", "dataframe", m19, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"s68505.csv tp K3 dataframe", "s68505.csv", f20, "tp", "K3", "dataframe", "", "P 2021-03-01 08:00:00+00:00|2021-03-01 08:00:00+00:00|240.000000|0.000000 ; 2021-03-02 09:30:00+00:00|2021-03-02 09:30:00+00:00|60.000000|0.000000"},
      {"s68505.csv tpc K3 dataframe", "s68505.csv", f20, "tpc", "K3", "dataframe", m20, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K3'. If you wish to proceed you should use pd.concat"},
      {"s68505.csv tp K4 dataframe", "s68505.csv", f20, "tp", "K4", "dataframe", "", "P 1.0|1.0|420.000000|0.000000 ; 1.0|3.0|0.000000|0.000000 ; 2.0|3.0|60.000000|0.000000 ; 3.0|1.0|240.000000|0.000000"},
      {"s68505.csv tpc K4 dataframe", "s68505.csv", f20, "tpc", "K4", "dataframe", m21, "error: You are trying to merge on float64 and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"s68505.csv psm K5", "s68505.csv", f20, "psm", "K5", "", "", "S immediate 0 0 0.125 ; immediate 0 0 0.1428571429 ; immediate 0 0 0.4 ; immediate 0 0 0.5 ; immediate 0 0 0.5714285714 ; immediate 0 0 0.6 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"s68505.csv tp K5 dataframe", "s68505.csv", f20, "tp", "K5", "dataframe", "", "P False|True|0.000000|0.000000"},
      {"s68505.csv tpc K5 dataframe", "s68505.csv", f20, "tpc", "K5", "dataframe", m22, "C  ;  ;  ;  ; "},
      {"s68505.csv tp K6 dataframe", "s68505.csv", f20, "tp", "K6", "dataframe", "", "P 1|1|120.000000|103.923048 ; b|1|120.000000|0.000000"},
      {"s68505.csv tpc K6 dataframe", "s68505.csv", f20, "tpc", "K6", "dataframe", m23, "C b|1|120.000000|9223372036854775808.000000 ;  ;  ;  ; 1|1|240.000000|3.464102"},
      {"s68507.csv tp K1 dataframe", "s68507.csv", f21, "tp", "K1", "dataframe", "", "P p|p|300.000000|0.000000 ; p|q|150.000000|42.426407 ; q|p|150.000000|42.426407 ; q|q|60.000000|0.000000"},
      {"s68507.csv msd K1 pandas", "s68507.csv", f21, "msd", "K1", "pandas", "", "M p=2 ; q=0"},
      {"s68507.csv msd K1 polars", "s68507.csv", f21, "msd", "K1", "polars", "", "M None=0 ; p=2 ; q=0"},
      {"s68507.csv tp K2 dataframe", "s68507.csv", f21, "tp", "K2", "dataframe", "", "P 1.5|2.5|60.000000|0.000000 ; 2.5|1.5|120.000000|0.000000 ; 2.5|2.5|180.000000|0.000000"},
      {"s68507.csv tpc K2 dataframe", "s68507.csv", f21, "tpc", "K2", "dataframe", m24, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"s68507.csv tp K3 dataframe", "s68507.csv", f21, "tp", "K3", "dataframe", "", "P 2021-03-01 08:00:00+00:00|2021-03-01 08:00:00+00:00|150.000000|42.426407"},
      {"s68507.csv tpc K3 dataframe", "s68507.csv", f21, "tpc", "K3", "dataframe", m25, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K3'. If you wish to proceed you should use pd.concat"},
      {"s68507.csv tp K4 dataframe", "s68507.csv", f21, "tp", "K4", "dataframe", "", "P 1.0|1.0|120.000000|60.000000 ; 1.0|2.0|120.000000|0.000000"},
      {"s68507.csv tpc K4 dataframe", "s68507.csv", f21, "tpc", "K4", "dataframe", m26, "error: You are trying to merge on float64 and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"s68507.csv tp K5 dataframe", "s68507.csv", f21, "tp", "K5", "dataframe", "", "P True|False|60.000000|0.000000"},
      {"s68507.csv tpc K5 dataframe", "s68507.csv", f21, "tpc", "K5", "dataframe", m27, "C  ;  ; "},
      {"s68507.csv msd K5 polars", "s68507.csv", f21, "msd", "K5", "polars", "", "M False=0 ; None=2"},
      {"s68507.csv tp K6 dataframe", "s68507.csv", f21, "tp", "K6", "dataframe", "", "P 1|1|120.000000|0.000000 ; a|a|60.000000|0.000000"},
      {"s68507.csv msd K6 pandas", "s68507.csv", f21, "msd", "K6", "pandas", "", "M 1=0 ; a=0"},
      {"s68507.csv msd K6 polars", "s68507.csv", f21, "msd", "K6", "polars", "", "M 1=0 ; None=2 ; a=0"},
      {"s68511.csv msd K2 polars", "s68511.csv", f22, "msd", "K2", "polars", "", "M None=2"},
      {"s68511.csv msd K6 pandas", "s68511.csv", f22, "msd", "K6", "pandas", "", "M "},
      {"s68511.csv msd K6 polars", "s68511.csv", f22, "msd", "K6", "polars", "", "M None=0"},
      {"s68512.csv msd K2 polars", "s68512.csv", f23, "msd", "K2", "polars", "", "M None=0"},
      {"s68512.csv msd K5 polars", "s68512.csv", f23, "msd", "K5", "polars", "", "M None=1"},
      {"s68513.csv msd K6 pandas", "s68513.csv", f24, "msd", "K6", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"f90100.xes psm K1", "f90100.xes", f25, "psm", "K1", "", "", "S immediate 0 0 0.4285714286 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 12 13 0.5714285714"},
      {"f90100.xes tp K1 dataframe", "f90100.xes", f25, "tp", "K1", "dataframe", "", "P -9223372036854775808|-9223372036854775808|30.000000|0.000000 ; -9223372036854775808|2|22.750000|14.221463 ; 2|-9223372036854775808|48.000000|0.000000 ; 2|2|48.666667|31.564748"},
      {"f90100.xes tpc K1 dataframe", "f90100.xes", f25, "tpc", "K1", "dataframe", m28, "error: You are trying to merge on int64 and str columns for key 'K1'. If you wish to proceed you should use pd.concat"},
      {"f90100.xes msd K1 pandas", "f90100.xes", f25, "msd", "K1", "pandas", "", "error: can only concatenate str (not \"int\") to str"},
      {"f90100.xes msd K1 polars", "f90100.xes", f25, "msd", "K1", "polars", "", "M -9223372036854775808=0 ; 2=0"},
      {"f90100.xes pev K1 mine", "f90100.xes", f25, "pev", "K1", "mine", "", "error: sequence item 0: expected str instance, int found"},
      {"f90100.xes pev K1 evaluate", "f90100.xes", f25, "pev", "K1", "evaluate", "", "error: sequence item 0: expected str instance, int found"},
      {"f90100.xes play K1", "f90100.xes", f25, "play", "K1", "", "", "L 0 [-9223372036854775808] [2]"},
      {"f90100.xes psm K2", "f90100.xes", f25, "psm", "K2", "", "", "error: 'K2'"},
      {"f90100.xes tp K2 dataframe", "f90100.xes", f25, "tp", "K2", "dataframe", "", "P 0.0|0.0|48.666667|31.564748 ; 2.25|0.0|27.000000|21.213203 ; 2.25|2.25|30.000000|0.000000"},
      {"f90100.xes tpc K2 dataframe", "f90100.xes", f25, "tpc", "K2", "dataframe", m29, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"f90100.xes msd K2 pandas", "f90100.xes", f25, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90100.xes msd K2 polars", "f90100.xes", f25, "msd", "K2", "polars", "", "M 0.0=0 ; 2.25=0"},
      {"f90100.xes pev K2 mine", "f90100.xes", f25, "pev", "K2", "mine", "", "error: 'K2'"},
      {"f90100.xes pev K2 evaluate", "f90100.xes", f25, "pev", "K2", "evaluate", "", "error: 'K2'"},
      {"f90100.xes play K2", "f90100.xes", f25, "play", "K2", "", "", "error: 'K2'"},
      {"f90100.xes psm K3", "f90100.xes", f25, "psm", "K3", "", "", "S immediate 0 0 0.4285714286 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 0.5714285714 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90100.xes tp K3 dataframe", "f90100.xes", f25, "tp", "K3", "dataframe", "", "P False|False|48.666667|31.564748 ; False|True|48.000000|0.000000 ; True|False|22.750000|14.221463 ; True|True|30.000000|0.000000"},
      {"f90100.xes tpc K3 dataframe", "f90100.xes", f25, "tpc", "K3", "dataframe", m30, "C  ; "},
      {"f90100.xes msd K3 pandas", "f90100.xes", f25, "msd", "K3", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"f90100.xes msd K3 polars", "f90100.xes", f25, "msd", "K3", "polars", "", "M False=0 ; True=0"},
      {"f90100.xes pev K3 mine", "f90100.xes", f25, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90100.xes pev K3 evaluate", "f90100.xes", f25, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90100.xes play K3", "f90100.xes", f25, "play", "K3", "", "", "L 0 [False] [True]"},
      {"f90100.xes psm K4", "f90100.xes", f25, "psm", "K4", "", "", "error: 'K4'"},
      {"f90100.xes tp K4 dataframe", "f90100.xes", f25, "tp", "K4", "dataframe", "", "P 2021-03-01 08:00:00+00:00|2021-03-01 08:00:00+00:00|30.000000|0.000000 ; 2021-03-01 08:00:00+00:00|2021-03-01 09:00:00+00:00|18.500000|9.192388 ; 2021-03-01 09:00:00+00:00|2021-03-01 08:00:00+00:00|48.000000|0.000000 ; 2021-03-01 09:00:00+00:00|2021-03-01 09:00:00+00:00|48.666667|31.564748"},
      {"f90100.xes tpc K4 dataframe", "f90100.xes", f25, "tpc", "K4", "dataframe", m31, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"f90100.xes msd K4 pandas", "f90100.xes", f25, "msd", "K4", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"f90100.xes msd K4 polars", "f90100.xes", f25, "msd", "K4", "polars", "", "M 2021-03-01 08:00:00+00:00=0 ; 2021-03-01 09:00:00+00:00=0"},
      {"f90100.xes pev K4 mine", "f90100.xes", f25, "pev", "K4", "mine", "", "error: 'K4'"},
      {"f90100.xes pev K4 evaluate", "f90100.xes", f25, "pev", "K4", "evaluate", "", "error: 'K4'"},
      {"f90100.xes play K4", "f90100.xes", f25, "play", "K4", "", "", "error: 'K4'"},
      {"f90100.xes psm K5", "f90100.xes", f25, "psm", "K5", "", "", "error: 'K5'"},
      {"f90100.xes tp K5 dataframe", "f90100.xes", f25, "tp", "K5", "dataframe", "", "P -1.0|-1.0|30.000000|0.000000 ; -1.0|1.0|26.333333|15.044379 ; 1.0|-1.0|48.000000|0.000000 ; 1.0|1.0|73.000000|0.000000"},
      {"f90100.xes tpc K5 dataframe", "f90100.xes", f25, "tpc", "K5", "dataframe", m32, "error: You are trying to merge on float64 and str columns for key 'K5'. If you wish to proceed you should use pd.concat"},
      {"f90100.xes msd K5 pandas", "f90100.xes", f25, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90100.xes msd K5 polars", "f90100.xes", f25, "msd", "K5", "polars", "", "M -1.0=0 ; 1.0=2"},
      {"f90100.xes pev K5 mine", "f90100.xes", f25, "pev", "K5", "mine", "", "error: 'K5'"},
      {"f90100.xes pev K5 evaluate", "f90100.xes", f25, "pev", "K5", "evaluate", "", "error: 'K5'"},
      {"f90100.xes play K5", "f90100.xes", f25, "play", "K5", "", "", "error: 'K5'"},
      {"f90100.xes psm K6", "f90100.xes", f25, "psm", "K6", "", "", "S immediate 0 0 0.4285714286 ; immediate 0 0 0.5 ; immediate 0 0 0.5 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1 ; uniform 12 13 0.5714285714"},
      {"f90100.xes tp K6 dataframe", "f90100.xes", f25, "tp", "K6", "dataframe", "", "P 010|010|48.666667|31.564748 ; 010|10|48.000000|0.000000 ; 10|010|22.750000|14.221463 ; 10|10|30.000000|0.000000"},
      {"f90100.xes tpc K6 dataframe", "f90100.xes", f25, "tpc", "K6", "dataframe", m33, "C 10|010|42.000000|4.306871,10|10|30.000000|9223372036854775808.000000 ; 010|010|13.000000|0.718101,010|010|60.000000|2.259905,010|010|73.000000|3.083609,010|10|48.000000|9223372036854775808.000000,10|010|25.000000|1.916118"},
      {"f90100.xes msd K6 pandas", "f90100.xes", f25, "msd", "K6", "pandas", "", "M 010=0 ; 10=0"},
      {"f90100.xes msd K6 polars", "f90100.xes", f25, "msd", "K6", "polars", "", "M 010=0 ; 10=0"},
      {"f90100.xes pev K6 mine", "f90100.xes", f25, "pev", "K6", "mine", "", "E 0.642857"},
      {"f90100.xes pev K6 evaluate", "f90100.xes", f25, "pev", "K6", "evaluate", "", "E 0.642857"},
      {"f90100.xes play K6", "f90100.xes", f25, "play", "K6", "", "", "L 0 [010] [10]"},
      {"f90101.xes psm K1", "f90101.xes", f26, "psm", "K1", "", "", "error: 'K1'"},
      {"f90101.xes tpc K1 dataframe", "f90101.xes", f26, "tpc", "K1", "dataframe", m34, "error: You are trying to merge on float64 and str columns for key 'K1'. If you wish to proceed you should use pd.concat"},
      {"f90101.xes msd K1 pandas", "f90101.xes", f26, "msd", "K1", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90101.xes msd K1 polars", "f90101.xes", f26, "msd", "K1", "polars", "", "M 7.0=2 ; None=0"},
      {"f90101.xes pev K1 mine", "f90101.xes", f26, "pev", "K1", "mine", "", "error: 'K1'"},
      {"f90101.xes pev K1 evaluate", "f90101.xes", f26, "pev", "K1", "evaluate", "", "error: 'K1'"},
      {"f90101.xes play K1", "f90101.xes", f26, "play", "K1", "", "", "error: 'K1'"},
      {"f90101.xes tpc K2 dataframe", "f90101.xes", f26, "tpc", "K2", "dataframe", m35, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"f90101.xes msd K2 pandas", "f90101.xes", f26, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90101.xes msd K2 polars", "f90101.xes", f26, "msd", "K2", "polars", "", "M None=0"},
      {"f90101.xes psm K3", "f90101.xes", f26, "psm", "K3", "", "", "error: 'K3'"},
      {"f90101.xes msd K3 pandas", "f90101.xes", f26, "msd", "K3", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"f90101.xes msd K3 polars", "f90101.xes", f26, "msd", "K3", "polars", "", "M False=0 ; True=1"},
      {"f90101.xes pev K3 mine", "f90101.xes", f26, "pev", "K3", "mine", "", "error: 'K3'"},
      {"f90101.xes pev K3 evaluate", "f90101.xes", f26, "pev", "K3", "evaluate", "", "error: 'K3'"},
      {"f90101.xes play K3", "f90101.xes", f26, "play", "K3", "", "", "error: 'K3'"},
      {"f90101.xes tpc K4 dataframe", "f90101.xes", f26, "tpc", "K4", "dataframe", m36, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"f90101.xes msd K4 pandas", "f90101.xes", f26, "msd", "K4", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"f90101.xes msd K4 polars", "f90101.xes", f26, "msd", "K4", "polars", "", "M 2021-03-01 08:00:00+00:00=2"},
      {"f90101.xes tpc K5 dataframe", "f90101.xes", f26, "tpc", "K5", "dataframe", m37, "error: You are trying to merge on float64 and str columns for key 'K5'. If you wish to proceed you should use pd.concat"},
      {"f90101.xes msd K5 pandas", "f90101.xes", f26, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90101.xes msd K5 polars", "f90101.xes", f26, "msd", "K5", "polars", "", "M 0.0=2 ; 1.0=0"},
      {"f90101.xes psm K6", "f90101.xes", f26, "psm", "K6", "", "", "error: 'K6'"},
      {"f90101.xes msd K6 pandas", "f90101.xes", f26, "msd", "K6", "pandas", "", "M "},
      {"f90101.xes pev K6 mine", "f90101.xes", f26, "pev", "K6", "mine", "", "error: 'K6'"},
      {"f90101.xes pev K6 evaluate", "f90101.xes", f26, "pev", "K6", "evaluate", "", "error: 'K6'"},
      {"f90101.xes play K6", "f90101.xes", f26, "play", "K6", "", "", "error: 'K6'"},
      {"f90102.xes tpc K1 dataframe", "f90102.xes", f27, "tpc", "K1", "dataframe", m38, "error: You are trying to merge on float64 and str columns for key 'K1'. If you wish to proceed you should use pd.concat"},
      {"f90102.xes msd K1 pandas", "f90102.xes", f27, "msd", "K1", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90102.xes msd K1 polars", "f90102.xes", f27, "msd", "K1", "polars", "", "M 100.0=0 ; 7.0=0 ; None=0"},
      {"f90102.xes tpc K2 dataframe", "f90102.xes", f27, "tpc", "K2", "dataframe", m39, "error: You are trying to merge on float64 and str columns for key 'K2'. If you wish to proceed you should use pd.concat"},
      {"f90102.xes msd K2 pandas", "f90102.xes", f27, "msd", "K2", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90102.xes msd K2 polars", "f90102.xes", f27, "msd", "K2", "polars", "", "M 0.5=0 ; 2.25=0 ; None=0"},
      {"f90102.xes psm K3", "f90102.xes", f27, "psm", "K3", "", "", "S immediate 0 0 0.28 ; immediate 0 0 0.3333333333 ; immediate 0 0 0.4 ; immediate 0 0 0.6666666667 ; immediate 0 0 0.8666666667 ; immediate 0 0 1"},
      {"f90102.xes msd K3 pandas", "f90102.xes", f27, "msd", "K3", "pandas", "", "error: can only concatenate str (not \"bool\") to str"},
      {"f90102.xes msd K3 polars", "f90102.xes", f27, "msd", "K3", "polars", "", "M False=0 ; True=1"},
      {"f90102.xes pev K3 mine", "f90102.xes", f27, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90102.xes pev K3 evaluate", "f90102.xes", f27, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90102.xes tpc K4 dataframe", "f90102.xes", f27, "tpc", "K4", "dataframe", m40, "error: You are trying to merge on datetime64[us, UTC] and str columns for key 'K4'. If you wish to proceed you should use pd.concat"},
      {"f90102.xes msd K4 pandas", "f90102.xes", f27, "msd", "K4", "pandas", "", "error: unsupported operand type(s) for +: 'Timestamp' and 'str'"},
      {"f90102.xes msd K4 polars", "f90102.xes", f27, "msd", "K4", "polars", "", "M 2021-03-01 08:00:00+00:00=1 ; 2021-03-01 09:00:00+00:00=0 ; 2021-03-02 08:00:00+00:00=0 ; None=0"},
      {"f90102.xes tpc K5 dataframe", "f90102.xes", f27, "tpc", "K5", "dataframe", m41, "error: You are trying to merge on float64 and str columns for key 'K5'. If you wish to proceed you should use pd.concat"},
      {"f90102.xes msd K5 pandas", "f90102.xes", f27, "msd", "K5", "pandas", "", "error: can only concatenate str (not \"float\") to str"},
      {"f90102.xes msd K5 polars", "f90102.xes", f27, "msd", "K5", "polars", "", "M 1.0=0 ; 10.0=0 ; 3.0=1"},
      {"f90102.xes msd K6 pandas", "f90102.xes", f27, "msd", "K6", "pandas", "", "M 010=1 ; 10=1 ; B=0"},
      {"f90103.xes tpc K1 dataframe", "f90103.xes", f28, "tpc", "K1", "dataframe", m17, "error: 'K1'"},
      {"f90103.xes pev K1 mine", "f90103.xes", f28, "pev", "K1", "mine", "", "E 1.000000"},
      {"f90103.xes pev K1 evaluate", "f90103.xes", f28, "pev", "K1", "evaluate", "", "E 1.000000"},
      {"f90103.xes psm K2", "f90103.xes", f28, "psm", "K2", "", "", "S immediate 0 0 1"},
      {"f90103.xes tpc K2 dataframe", "f90103.xes", f28, "tpc", "K2", "dataframe", m17, "error: 'K2'"},
      {"f90103.xes pev K2 mine", "f90103.xes", f28, "pev", "K2", "mine", "", "E 1.000000"},
      {"f90103.xes pev K2 evaluate", "f90103.xes", f28, "pev", "K2", "evaluate", "", "E 1.000000"},
      {"f90103.xes play K2", "f90103.xes", f28, "play", "K2", "", "", "L 0 [1.0]"},
      {"f90103.xes tp K3 dataframe", "f90103.xes", f28, "tp", "K3", "dataframe", "", "error: \"['K3'] not in index\""},
      {"f90103.xes tpc K3 dataframe", "f90103.xes", f28, "tpc", "K3", "dataframe", m17, "error: \"['K3'] not in index\""},
      {"f90103.xes psm K4", "f90103.xes", f28, "psm", "K4", "", "", "S immediate 0 0 1"},
      {"f90103.xes tpc K4 dataframe", "f90103.xes", f28, "tpc", "K4", "dataframe", m17, "error: 'K4'"},
      {"f90103.xes msd K4 polars", "f90103.xes", f28, "msd", "K4", "polars", "", "M "},
      {"f90103.xes pev K4 mine", "f90103.xes", f28, "pev", "K4", "mine", "", "E 1.000000"},
      {"f90103.xes pev K4 evaluate", "f90103.xes", f28, "pev", "K4", "evaluate", "", "E 1.000000"},
      {"f90103.xes play K4", "f90103.xes", f28, "play", "K4", "", "", "L 0 [2021-03-03 10:30:00+00:00]"},
      {"f90103.xes psm K5", "f90103.xes", f28, "psm", "K5", "", "", "S immediate 0 0 1"},
      {"f90103.xes tpc K5 dataframe", "f90103.xes", f28, "tpc", "K5", "dataframe", m17, "error: 'K5'"},
      {"f90103.xes msd K5 polars", "f90103.xes", f28, "msd", "K5", "polars", "", "M "},
      {"f90103.xes pev K5 mine", "f90103.xes", f28, "pev", "K5", "mine", "", "E 1.000000"},
      {"f90103.xes pev K5 evaluate", "f90103.xes", f28, "pev", "K5", "evaluate", "", "E 1.000000"},
      {"f90103.xes play K5", "f90103.xes", f28, "play", "K5", "", "", "L 0 [0]"},
      {"f90103.xes tp K6 dataframe", "f90103.xes", f28, "tp", "K6", "dataframe", "", "error: \"['K6'] not in index\""},
      {"f90103.xes tpc K6 dataframe", "f90103.xes", f28, "tpc", "K6", "dataframe", m17, "error: \"['K6'] not in index\""},
      {"f90104.xes msd K1 polars", "f90104.xes", f29, "msd", "K1", "polars", "", "M -9.223372036854776e+18=2 ; 2.0=0"},
      {"f90104.xes pev K2 mine", "f90104.xes", f29, "pev", "K2", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90104.xes pev K2 evaluate", "f90104.xes", f29, "pev", "K2", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90104.xes pev K5 mine", "f90104.xes", f29, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90104.xes pev K5 evaluate", "f90104.xes", f29, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90104.xes play K6", "f90104.xes", f29, "play", "K6", "", "", "L 0 [] [-1] [a] [x y]"},
      {"f90105.xes psm K3", "f90105.xes", f30, "psm", "K3", "", "", "S immediate 0 0 0.2727272727 ; immediate 0 0 0.4545454545 ; immediate 0 0 0.5454545455 ; immediate 0 0 0.7272727273 ; immediate 0 0 1 ; immediate 0 0 1 ; immediate 0 0 1"},
      {"f90105.xes pev K3 mine", "f90105.xes", f30, "pev", "K3", "mine", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90105.xes pev K3 evaluate", "f90105.xes", f30, "pev", "K3", "evaluate", "", "error: sequence item 0: expected str instance, bool found"},
      {"f90105.xes pev K5 mine", "f90105.xes", f30, "pev", "K5", "mine", "", "error: sequence item 0: expected str instance, float found"},
      {"f90105.xes pev K5 evaluate", "f90105.xes", f30, "pev", "K5", "evaluate", "", "error: sequence item 0: expected str instance, float found"},
      {"f90105.xes msd K6 pandas", "f90105.xes", f30, "msd", "K6", "pandas", "", "M x y=1 ; z=2"},
  };
  for (const auto& c : cases) {
    const Server target{client, c.name, c.content};
    check(c.label, outcome(target, c.kind, c.key, c.extra, c.model), c.expected);
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review HM server checks passed\n";
  return 0;
}
