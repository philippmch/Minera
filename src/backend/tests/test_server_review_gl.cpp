#undef NDEBUG
// Review GL: the classic heuristics performance view with another timestamp
// key on XES uploads (frame and log shapes) and CSV uploads (log, DataFrame
// and LazyFrame shapes), XES frame columns of any kind, and a custom typed
// activity key on an XES upload's log shape.
//
// - pm4py's classic performance DFG with another timestamp key measures
//   start(later) - complete(earlier) with the key's cell as the complete and
//   time:timestamp as the start (a start key replaces it). Minera read the
//   key through its own clock ("missing timestamp attribute" errors) or not
//   at all.
// - An XES frame column holds what read_xes types it as: datetime64 for
//   dates (None and gaps NaT), str for text (NaN), float64 for numbers with a
//   gap, object for mixes (each cell its Python type, a <list> a dict, None
//   None, a gap NaN), a column of None alone too; Polars' copy holds an object
//   column as String. The subtraction raises TypeError for non-dates, pandas'
//   own TypeError for Timestamp - int, AttributeError (total_seconds) for two
//   numbers; NaT beside an int is NaT.
// - On an XES upload's log shape a custom activity key reads each event's
//   Python value: a dict is unhashable, a pair without the key raises
//   KeyError, a pair whose first element is no str is not subscriptable, the
//   AND measure sorts each node's successors (str beside int raises), equal
//   numbers are one node, and /api/mine's net conversion concatenates str
//   with a node's name.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gl: tc/mkgl.py, ct/xesframe.py, ct/xeslog2.py, ct/csvt2.py).
//
// Usage: minera_server_review_gl_tests <minera_server>

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
  return edges(net, strings(net, net.find('[', net.find("\"activities\":"))), performance);
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
  const std::string in0 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T08:30:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T09:10:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T10:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T08:20:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:30:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T09:40:00.000+00:00\"/></event></trace></log>\n";
  const std::string in1 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T08:30:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T10:05:00.000+00:00\"/></event></trace></log>\n";
  const std::string in2 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T08:30:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><string key=\"ts2\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:00:00.000+00:00\"/><string key=\"ts2\" value=\"x\"/></event></trace></log>\n";
  const std::string in3 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><list key=\"ts2\"><values><string key=\"x\" value=\"y\"/></values></list></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><date key=\"ts2\" value=\"2021-03-01T09:10:00.000+00:00\"/></event></trace></log>\n";
  const std::string in4 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"ts2\" value=\"5\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><int key=\"ts2\" value=\"7\"/></event></trace></log>\n";
  const std::string in5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><string key=\"ts2\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><string key=\"ts2\"/></event></trace></log>\n";
  const std::string in6 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in7 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><int key=\"ts2\" value=\"5\"/><int key=\"st2\" value=\"1\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><int key=\"ts2\" value=\"7\"/><int key=\"st2\" value=\"3\"/></event></trace></log>\n";
  const std::string in8 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"n/a\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in9 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><int key=\"act2\" value=\"1\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in10 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"2\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in11 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><list key=\"act2\"><values><string key=\"x\" value=\"y\"/></values></list><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"act2\" value=\"b\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in12 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in13 = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c0\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><int key=\"act2\" value=\"1\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"act2\" value=\"a\"/><string key=\"concept:name\" value=\"k0\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><float key=\"act2\" value=\"1.0\"/><string key=\"concept:name\" value=\"k1\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string in14 = "case:concept:name,concept:name,time:timestamp,st2,res\nc1,a,2021-03-01 08:00:00,2021-03-01 08:30:00,r\nc1,b,2021-03-01 09:00:00,2021-03-01 09:10:00,r\nc2,a,2021-03-01 08:00:00,2021-03-01 08:20:00,r\nc2,b,2021-03-01 09:30:00,2021-03-01 09:40:00,r\n";
  const std::string in15 = "case:concept:name,concept:name,time:timestamp,st2,res\nc1,a,2021-03-01 08:00:00,2021-03-01 08:30:00,r\nc1,b,2021-03-01 09:00:00,,r\nc1,c,2021-03-01 10:00:00,2021-03-01 10:05:00,r\n";
  const std::string in16 = "case:concept:name,concept:name,time:timestamp,st2,res\nc1,a,2021-03-01 08:00:00,early,r\nc1,b,2021-03-01 09:00:00,late,r\n";
  const std::string in17 = "case:concept:name,concept:name,time:timestamp,st2,st\nc1,a,2021-03-01 08:00:00,,1\nc1,b,2021-03-01 09:00:00,2021-03-01 09:10:00,2\nc1,c,2021-03-01 10:00:00,2021-03-01 10:00:00,3\n";
  const std::vector<Case> cases = {
      {"XES frame: another timestamp column", "gl1.xes", in0, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "a>b=0.66666666666666663@3000.0 b>c=0.5@3000.0"},
      {"XES frame: another timestamp column", "gl1.xes", in0, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "a>b=0.66666666666666663@3000.0 b>c=0.5@3000.0"},
      {"XES frame: another timestamp column with a gap", "gl1.xes", in1, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "a>b=0.5@1800.0 b>c=0.5@0.0"},
      {"XES frame: another timestamp column with a gap", "gl1.xes", in1, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'NoneType'"},
      {"XES frame: business hours over another timestamp column with a gap", "gl1.xes", in1, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_business_hours", "true"}},
       "error: cannot convert float NaN to integer"},
      {"XES frame: business hours over another timestamp column with a gap", "gl1.xes", in1, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_business_hours", "true"}},
       "error: 'NoneType' object has no attribute 'replace'"},
      {"XES frame: None and text in another timestamp column", "gl1.xes", in2, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'NoneType'"},
      {"XES frame: None and text in another timestamp column", "gl1.xes", in2, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'str'"},
      {"XES frame: a dict in another timestamp column", "gl1.xes", in3, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'dict'"},
      {"XES frame: a dict in another timestamp column", "gl1.xes", in3, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'str'"},
      {"XES frame: business hours over a dict", "gl1.xes", in3, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_business_hours", "true"}},
       "error: 'dict' object has no attribute 'replace'"},
      {"XES frame: business hours over a dict", "gl1.xes", in3, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_business_hours", "true"}},
       "error: str.replace() takes no keyword arguments"},
      {"XES frame: an int timestamp column", "gl1.xes", in4, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: Addition/subtraction of integers and integer-arrays with Timestamp is no longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`"},
      {"XES frame: an int timestamp column", "gl1.xes", in4, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'int'"},
      {"XES frame: a timestamp column of None alone", "gl1.xes", in5, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'NoneType'"},
      {"XES frame: a timestamp column of None alone", "gl1.xes", in5, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'NoneType'"},
      {"XES frame: neither the start nor the timestamp column", "gl1.xes", in6, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_start_timestamp_attribute", "st2"}},
       "error: 'st2'"},
      {"XES frame: neither the start nor the timestamp column", "gl1.xes", in6, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_start_timestamp_attribute", "st2"}},
       "error: 'st2'"},
      {"XES frame: int timestamps and int starts", "gl1.xes", in7, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_start_timestamp_attribute", "st2"}},
       "error: 'int' object has no attribute 'total_seconds'"},
      {"XES frame: int timestamps and int starts", "gl1.xes", in7, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_start_timestamp_attribute", "st2"}},
       "error: 'int' object has no attribute 'total_seconds'"},
      {"XES frame: a text time:timestamp", "gl1.xes", in8, "dataframe", {{"heuristic_decoration", "performance"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'str'"},
      {"XES frame: a text time:timestamp", "gl1.xes", in8, "lazyframe", {{"heuristic_decoration", "performance"}},
       "error: unsupported operand type(s) for -: 'str' and 'str'"},
      {"XES log: another timestamp key", "gl1.xes", in0, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "a>b=0.66666666666666663@3000.0 b>c=0.5@3000.0"},
      {"XES log: another timestamp key one event lacks", "gl1.xes", in1, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: 'ts2'"},
      {"XES log: None in another timestamp key", "gl1.xes", in2, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'NoneType'"},
      {"XES log: a dict in another timestamp key", "gl1.xes", in3, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'dict'"},
      {"XES log: int timestamps and int starts", "gl1.xes", in7, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "ts2"}, {"heuristic_start_timestamp_attribute", "st2"}},
       "error: 'int' object has no attribute 'total_seconds'"},
      {"XES log: an int activity key", "gl2.xes", in9, "log", {{"heuristic_activity_attribute", "act2"}},
       "error: 'int' object is not subscriptable"},
      {"XES log: an int beside text successors", "gl2.xes", in10, "log", {{"heuristic_activity_attribute", "act2"}},
       "error: '<' not supported between instances of 'str' and 'int'"},
      {"XES log: a dict activity", "gl2.xes", in11, "log", {{"heuristic_activity_attribute", "act2"}},
       "error: unhashable type: 'dict'"},
      {"XES log: a pair without the activity key", "gl2.xes", in12, "log", {{"heuristic_activity_attribute", "act2"}},
       "error: 'act2'"},
      {"XES log: 1 and 1.0 are one activity", "gl2.xes", in13, "log", {{"heuristic_activity_attribute", "act2"}},
       "error: can only concatenate str (not \"int\") to str"},
      {"CSV: another timestamp column", "gl3.csv", in14, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "a>b=0.66666666666666663@3000.0"},
      {"CSV: another timestamp column", "gl3.csv", in14, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "a>b=0.66666666666666663@3000.0"},
      {"CSV: another timestamp column", "gl3.csv", in14, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "a>b=0.66666666666666663@3000.0"},
      {"CSV: another timestamp column with a gap", "gl3.csv", in15, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "a>b=0.5@1800.0 b>c=0.5@0.0"},
      {"CSV: another timestamp column with a gap", "gl3.csv", in15, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "a>b=0.5@1800.0 b>c=0.5@0.0"},
      {"CSV: another timestamp column with a gap", "gl3.csv", in15, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'NoneType'"},
      {"CSV: a text timestamp column", "gl3.csv", in16, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'str'"},
      {"CSV: a text timestamp column", "gl3.csv", in16, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'str'"},
      {"CSV: a text timestamp column", "gl3.csv", in16, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'str'"},
      {"CSV: the resource column as the timestamp key", "gl3.csv", in14, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "res"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'str'"},
      {"CSV: the resource column as the timestamp key", "gl3.csv", in14, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "res"}},
       "error: unsupported operand type(s) for -: 'Timestamp' and 'str'"},
      {"CSV: the resource column as the timestamp key", "gl3.csv", in14, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "res"}},
       "error: unsupported operand type(s) for -: 'datetime.datetime' and 'str'"},
      {"CSV: int starts beside a missing time", "gl3.csv", in17, "log", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}, {"heuristic_start_timestamp_attribute", "st"}},
       "error: unsupported operand type(s) for -: 'int' and 'Timestamp'"},
      {"CSV: int starts beside a missing time", "gl3.csv", in17, "dataframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}, {"heuristic_start_timestamp_attribute", "st"}},
       "error: unsupported operand type(s) for -: 'int' and 'Timestamp'"},
      {"CSV: int starts beside a missing time", "gl3.csv", in17, "lazyframe", {{"heuristic_decoration", "performance"}, {"heuristic_timestamp_attribute", "st2"}, {"heuristic_start_timestamp_attribute", "st"}},
       "error: unsupported operand type(s) for -: 'int' and 'NoneType'"},
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
  std::cout << "review GL server checks passed\n";
  return 0;
}
