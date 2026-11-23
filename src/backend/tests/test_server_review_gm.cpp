#undef NDEBUG
// Review GM: a custom activity key naming a typed column on XES uploads'
// frame shapes and CSV uploads' three shapes.
//
// - pm4py's classic heuristics miner on read_xes's frame (or its Polars
//   copy) reads the key's column as pandas typed it: a dict is unhashable,
//   the first directly-follows pair whose first element is no str raises
//   "'<type>' object is not subscriptable", the AND measure sorts each
//   node's successors as Python sorts their values, equal numbers (1, 1.0)
//   are one node named by the first, each NaN of a float column is its own
//   node, and the heuristics Petri conversion raises "can only concatenate
//   str (not \"<type>\") to str" for a node of another type with an incoming
//   edge. Polars' copy holds an object column as text and a gap as None.
//   Minera read the column's text (a dict as text, None-only columns
//   dropped).
// - On a CSV upload's frames the key's column is read_csv's (ints int, a gap
//   NaN, the timestamp column Timestamps); Minera refused the resource and
//   lifecycle columns and read the others as text. The log shape's token
//   replay and decorations in /api/mine read the resource and lifecycle
//   columns too (KeyError before).
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gm: tc/mkgm.py, ct/xfa2.py, ct/csva2.py, ct/csva3.py).
// Frames also give the heuristic_petri_conversion record (" | conv: ...").
//
// Usage: minera_server_review_gm_tests <minera_server>

#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
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

// The JSON object or array that starts at `open`, strings and escapes skipped.
std::string value_at(const std::string& body, std::size_t open) {
  int depth = 0;
  bool in_string = false;
  for (std::size_t i = open; i < body.size(); ++i) {
    const char c = body[i];
    if (in_string) {
      if (c == '\\') ++i;
      else if (c == '"') in_string = false;
    } else if (c == '"') {
      in_string = true;
    } else if (c == '{' || c == '[') {
      ++depth;
    } else if ((c == '}' || c == ']') && --depth == 0) {
      return body.substr(open, i - open + 1);
    }
  }
  return {};
}

// The raw value after `"key":` (optionally a space) in `entry`: a number, or a
// string without its quotes.
std::string field(const std::string& entry, const std::string& key) {
  auto at = entry.find("\"" + key + "\":");
  if (at == std::string::npos) return {};
  at += key.size() + 3;
  while (at < entry.size() && entry[at] == ' ') ++at;
  if (at < entry.size() && entry[at] == '"') {
    std::string text;
    for (std::size_t i = at + 1; i < entry.size() && entry[i] != '"'; ++i) {
      if (entry[i] == '\\' && i + 1 < entry.size()) ++i;
      text += entry[i];
    }
    return text;
  }
  return entry.substr(at, entry.find_first_of(",}]", at) - at);
}

// The objects of the array that starts at `open`.
std::vector<std::string> objects(const std::string& body, std::size_t open) {
  const auto array = value_at(body, open);
  std::vector<std::string> out;
  for (std::size_t at = array.find('{'); at != std::string::npos;) {
    out.push_back(value_at(array, at));
    at = array.find('{', at + out.back().size());
  }
  return out;
}

// The strings of the array that starts at `open`.
std::vector<std::string> strings(const std::string& body, std::size_t open) {
  const auto array = value_at(body, open);
  std::vector<std::string> out;
  for (std::size_t at = array.find('"'); at != std::string::npos;) {
    const auto close = array.find('"', at + 1);
    out.push_back(array.substr(at + 1, close - at - 1));
    at = array.find('"', close + 1);
  }
  return out;
}

// A number as Python prints a float of that value ("1" and "1.0" are 1.0).
std::string number(std::string text) {
  if (text.find_first_of(".eE") == std::string::npos && !text.empty() && text != "null") text += ".0";
  return text;
}

// The edges of a heuristics net JSON ("a>b=dependency", with "@representation"
// for the performance decoration), sorted; `names` resolves the activity ids.
std::string edges(const std::string& net, const std::vector<std::string>& names, bool performance) {
  std::vector<std::string> out;
  for (const auto& edge : objects(net, net.find('[', net.find("\"edges\":")))) {
    const auto source = std::stoul(field(edge, "source")), target = std::stoul(field(edge, "target"));
    auto text = (source < names.size() ? names[source] : "?") + ">" + (target < names.size() ? names[target] : "?") +
                "=" + number(field(edge, "dependency"));
    if (performance) text += "@" + number(field(edge, "representation"));
    out.push_back(std::move(text));
  }
  std::sort(out.begin(), out.end());
  std::string joined;
  for (const auto& text : out) joined += (joined.empty() ? "" : " ") + text;
  return joined;
}

// The heuristics outcome of an upload on `shape` (log, dataframe or
// lazyframe) with the extra mine fields: "error: <message>" or its edges.
std::string heuristics(httplib::Client& client, const std::string& name, const std::string& content,
                       const std::string& shape, const std::vector<std::pair<std::string, std::string>>& fields) {
  const bool csv = name.ends_with(".csv");
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"log", content, name, csv ? "text/csv" : "application/xml"},
  };
  if (csv) {
    items.push_back({"case_column", "case:concept:name", "", ""});
    items.push_back({"activity_column", "concept:name", "", ""});
    items.push_back({"timestamp_column", "time:timestamp", "", ""});
  }
  if (shape != "log") items.push_back({"mining_input_shape", shape, "", ""});
  bool performance = false;
  for (const auto& [key, value] : fields) {
    items.push_back({key, value, "", ""});
    if (key == "heuristic_decoration" && value == "performance") performance = true;
  }
  const auto result = client.Post("/api/mine", items);
  if (!result) return "no response";
  const auto& body = result->body;
  if (shape == "log") {
    const auto net_at = body.find("\"heuristics_net\":");
    if (net_at == std::string::npos) return "error: " + field(body, "error");
    const auto net = value_at(body, body.find('{', net_at));
    std::vector<std::string> names;
    if (field(net, "activity_ids_are_local") == "true")
      names = strings(net, net.find('[', net.find("\"activities\":")));
    else
      for (const auto& activity : objects(body, body.find('[', body.find("\"activities\":")))) names.push_back(field(activity, "name"));
    return edges(net, names, performance);
  }
  const auto families = body.find("\"family_results\":");
  if (families == std::string::npos) return "no family results: " + body.substr(0, 200);
  const auto at = body.find("\"heuristic\":", families);
  if (at == std::string::npos) return "no heuristic family";
  const auto family = value_at(body, body.find('{', at));
  if (field(family, "executed") != "true") {
    const auto error = family.find("\"error\":", family.find("\"result\":"));
    return "error: " + field(family.substr(error), "message");
  }
  const auto net = value_at(family, family.find('{', family.find("\"result\":")));
  // The frame's activities: a missing label is null (pandas' NaN, Polars' None).
  auto names = strings(net, net.find('[', net.find("\"activities\":")));
  {
    const auto array = value_at(net, net.find('[', net.find("\"activities\":")));
    names.clear();
    std::size_t at = 1;
    while (at < array.size() && array[at] != ']') {
      if (array[at] == '"') {
        std::string text;
        std::size_t i = at + 1;
        for (; i < array.size() && array[i] != '"'; ++i) {
          if (array[i] == '\\' && i + 1 < array.size()) ++i;
          text += array[i];
        }
        names.push_back(text);
        at = i + 1;
      } else if (array.compare(at, 4, "null") == 0) {
        names.push_back(shape == "dataframe" ? "nan" : "None");
        at += 4;
      } else {
        ++at;
      }
    }
  }
  std::string conversion = "ok";
  if (const auto record = body.find("\"heuristic_petri_conversion\":", families); record != std::string::npos) {
    const auto entry = value_at(body, body.find('{', record));
    if (field(entry, "executed") != "true") {
      const auto error = entry.find("\"error\":", entry.find("\"result\":"));
      conversion = "error: " + field(entry.substr(error == std::string::npos ? 0 : error), "message");
    }
  }
  return edges(net, names, performance) + " | conv: " + conversion;
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
    std::string label, name, content, shape;
    Fields fields;
    std::string expected;
  };
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><int key=\"act2\" value=\"1\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"1\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><list key=\"act2\"><values><string key=\"x\" value=\"y\"/></values></list><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"1\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><float key=\"act2\" value=\"1.0\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in7 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in8 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><float key=\"act2\" value=\"nan\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><float key=\"act2\" value=\"nan\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in9 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><boolean key=\"act2\" value=\"true\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><boolean key=\"act2\" value=\"true\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in10 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><int key=\"act2\" value=\"1\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in11 = "case:concept:name,concept:name,time:timestamp,n,txt,org:resource,lifecycle:transition\nc1,a,2021-03-01 08:00:00,1,x,r1,start\nc1,b,2021-03-01 09:00:00,2,,r2,complete\nc1,c,2021-03-01 10:00:00,3,y,r3,start\nc2,a,2021-03-01 08:00:00,1,x,r1,complete\nc2,c,2021-03-01 09:30:00,3,y,r2,complete\n";
  const std::string in12 = "case:concept:name,concept:name,time:timestamp,n,txt,org:resource,lifecycle:transition\nc1,a,2021-03-01 08:00:00,1,,r1,start\nc1,b,2021-03-01 09:00:00,2,x,r2,complete\nc2,a,2021-03-01 08:00:00,1,y,r1,complete\n";
  const std::vector<Case> cases = {
      {"XES frame: int activities", "gm1.xes", in0, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: 'int' object is not subscriptable"},
      {"XES frame: int activities", "gm1.xes", in0, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "error: 'int' object is not subscriptable"},
      {"XES frame: an int ending a pair", "gm1.xes", in1, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: '<' not supported between instances of 'str' and 'int'"},
      {"XES frame: an int ending a pair", "gm1.xes", in1, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "a>2=0.5 a>b=0.5 | conv: ok"},
      {"XES frame: an int beside text successors", "gm1.xes", in2, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: '<' not supported between instances of 'str' and 'int'"},
      {"XES frame: an int beside text successors", "gm1.xes", in2, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "a>2=0.5 a>b=0.5 b>2=0.5 | conv: ok"},
      {"XES frame: a dict activity", "gm1.xes", in3, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: unhashable type: 'dict'"},
      {"XES frame: a dict activity", "gm1.xes", in3, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "a>b=0.5 a>{'value': None, 'children': [('x', 'y')]}=0.5 | conv: ok"},
      {"XES frame: a pair without the activity", "gm1.xes", in4, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: '<' not supported between instances of 'str' and 'float'"},
      {"XES frame: a pair without the activity", "gm1.xes", in4, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "error: '<' not supported between instances of 'str' and 'NoneType'"},
      {"XES frame: a pair led by no activity", "gm1.xes", in5, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: 'float' object is not subscriptable"},
      {"XES frame: a pair led by no activity", "gm1.xes", in5, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "error: 'NoneType' object is not subscriptable"},
      {"XES frame: 1 and 1.0 in one column", "gm1.xes", in6, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "a>1=0.66666666666666663 | conv: error: can only concatenate str (not \"int\") to str"},
      {"XES frame: 1 and 1.0 in one column", "gm1.xes", in6, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "a>1.0=0.5 a>1=0.5 | conv: ok"},
      {"XES frame: an activity column of None alone", "gm1.xes", in7, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "error: 'NoneType' object is not subscriptable"},
      {"XES frame: an activity column of None alone", "gm1.xes", in7, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "error: 'NoneType' object is not subscriptable"},
      {"XES frame: NaN floats as activities", "gm1.xes", in8, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "a>nan=0.66666666666666663 | conv: error: can only concatenate str (not \"float\") to str"},
      {"XES frame: NaN floats as activities", "gm1.xes", in8, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "a>None=0.66666666666666663 | conv: error: can only concatenate str (not \"NoneType\") to str"},
      {"XES frame: a boolean ending pairs", "gm1.xes", in9, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       "a>True=0.5 b>True=0.5 | conv: error: can only concatenate str (not \"bool\") to str"},
      {"XES frame: a boolean ending pairs", "gm1.xes", in9, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       "a>True=0.5 b>True=0.5 | conv: ok"},
      {"XES frame: an int in a single-event case", "gm1.xes", in10, "dataframe", {{"heuristic_activity_attribute", "act2"}},
       " | conv: ok"},
      {"XES frame: an int in a single-event case", "gm1.xes", in10, "lazyframe", {{"heuristic_activity_attribute", "act2"}},
       " | conv: ok"},
      {"CSV: an int column as the activity key", "gm2.csv", in11, "log", {{"heuristic_activity_attribute", "n"}},
       "error: Heuristics Miner: TypeError: 'int' object is not subscriptable"},
      {"CSV: an int column as the activity key", "gm2.csv", in11, "dataframe", {{"heuristic_activity_attribute", "n"}},
       "error: 'int' object is not subscriptable"},
      {"CSV: an int column as the activity key", "gm2.csv", in11, "lazyframe", {{"heuristic_activity_attribute", "n"}},
       "error: 'int' object is not subscriptable"},
      {"CSV: a text column with a gap as the activity key", "gm2.csv", in11, "log", {{"heuristic_activity_attribute", "txt"}},
       "error: Heuristics Miner: TypeError: 'float' object is not subscriptable"},
      {"CSV: a text column with a gap as the activity key", "gm2.csv", in11, "dataframe", {{"heuristic_activity_attribute", "txt"}},
       "error: 'float' object is not subscriptable"},
      {"CSV: a text column with a gap as the activity key", "gm2.csv", in11, "lazyframe", {{"heuristic_activity_attribute", "txt"}},
       "error: 'NoneType' object is not subscriptable"},
      {"CSV: a gap leading a pair", "gm2.csv", in12, "log", {{"heuristic_activity_attribute", "txt"}},
       "error: Heuristics Miner: TypeError: 'float' object is not subscriptable"},
      {"CSV: a gap leading a pair", "gm2.csv", in12, "dataframe", {{"heuristic_activity_attribute", "txt"}},
       "error: 'float' object is not subscriptable"},
      {"CSV: a gap leading a pair", "gm2.csv", in12, "lazyframe", {{"heuristic_activity_attribute", "txt"}},
       "error: 'NoneType' object is not subscriptable"},
      {"CSV: the resource column as the activity key", "gm2.csv", in11, "log", {{"heuristic_activity_attribute", "org:resource"}},
       "r1>r2=0.66666666666666663 r2>r3=0.5"},
      {"CSV: the resource column as the activity key", "gm2.csv", in11, "dataframe", {{"heuristic_activity_attribute", "org:resource"}},
       "r1>r2=0.66666666666666663 r2>r3=0.5 | conv: ok"},
      {"CSV: the resource column as the activity key", "gm2.csv", in11, "lazyframe", {{"heuristic_activity_attribute", "org:resource"}},
       "r1>r2=0.66666666666666663 r2>r3=0.5 | conv: ok"},
      {"CSV: the lifecycle column as the activity key", "gm2.csv", in11, "log", {{"heuristic_activity_attribute", "lifecycle:transition"}},
       "complete>complete=0.5"},
      {"CSV: the lifecycle column as the activity key", "gm2.csv", in11, "dataframe", {{"heuristic_activity_attribute", "lifecycle:transition"}},
       "complete>complete=0.5 | conv: ok"},
      {"CSV: the lifecycle column as the activity key", "gm2.csv", in11, "lazyframe", {{"heuristic_activity_attribute", "lifecycle:transition"}},
       "complete>complete=0.5 | conv: ok"},
      {"CSV: the resource column with the performance view", "gm2.csv", in11, "dataframe", {{"heuristic_activity_attribute", "org:resource"}, {"heuristic_decoration", "performance"}},
       "r1>r2=0.66666666666666663@4500.0 r2>r3=0.5@3600.0 | conv: ok"},
      {"CSV: the resource column with the performance view", "gm2.csv", in11, "lazyframe", {{"heuristic_activity_attribute", "org:resource"}, {"heuristic_decoration", "performance"}},
       "r1>r2=0.66666666666666663@4500.0 r2>r3=0.5@3600.0 | conv: ok"},
      {"CSV: the timestamp column as the activity key", "gm2.csv", in11, "dataframe", {{"heuristic_activity_attribute", "time:timestamp"}},
       "error: 'Timestamp' object is not subscriptable"},
      {"CSV: the timestamp column as the activity key", "gm2.csv", in11, "lazyframe", {{"heuristic_activity_attribute", "time:timestamp"}},
       "error: 'datetime.datetime' object is not subscriptable"},
  };
  for (const auto& c : cases)
    check(c.label + " (" + c.shape + ")", heuristics(client, c.name, c.content, c.shape, c.fields), c.expected);

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GM server checks passed\n";
  return 0;
}
