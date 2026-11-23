#undef NDEBUG
// Review GJ: Heuristics++ with a configured start key or business hours on
// CSV uploads (log, DataFrame and LazyFrame shapes) and on XES uploads'
// frame shapes.
//
// - pm4py's to_interval keeps the log as it is when the first event holds
//   the start key; on a frame's event log every event holds every column,
//   so a configured key naming any event column (the resource column too,
//   or the timestamp itself) is read as the frame holds it, and a key the
//   frame lacks (or a `case:` column, a trace attribute there) makes
//   to_interval read the lifecycle. Minera skipped pm4py's checks on the
//   frame shapes for any configured key or business hours (its own "valid
//   start timestamp" error, or a net where pm4py raised), and on the log
//   shape ignored a key naming the resource column.
// - With business hours the performance DFG and the service times measure
//   each span with BusinessHours: replace(tzinfo=None) raises on a str
//   (TypeError) or another non-date (AttributeError), a NaT raises
//   ValueError (cannot convert float NaN to integer). Minera reported the
//   service time's NaTType error.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gj: probes/refopts.py, ct/hppcsv4.py, ct/optval2.py).
//
// Usage: minera_server_review_gj_tests <minera_server>

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
  if (at < entry.size() && entry[at] == '"') return entry.substr(at + 1, entry.find('"', at + 1) - at - 1);
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

// The Heuristics++ outcome of an upload on `shape` (log, dataframe or
// lazyframe) with the extra mine fields: "error: <message>" or its edges.
std::string plusplus(httplib::Client& client, const std::string& name, const std::string& content,
                     const std::string& shape, const std::vector<std::pair<std::string, std::string>>& fields) {
  const bool csv = name.ends_with(".csv");
  httplib::MultipartFormDataItems items = {
      {"include_traces", "false", "", ""},
      {"heuristic_variant", "plusplus", "", ""},
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
    std::vector<std::string> names;
    for (const auto& activity : objects(body, body.find('[', body.find("\"activities\":")))) names.push_back(field(activity, "name"));
    return edges(value_at(body, body.find('{', net_at)), names, performance);
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
  const Fields hours = {{"heuristic_business_hours", "true"}};
  const Fields hours_perf = {{"heuristic_business_hours", "true"}, {"heuristic_decoration", "performance"}};
  const auto key = [](const std::string& column, bool performance = false) {
    Fields fields = {{"heuristic_start_timestamp_attribute", column}};
    if (performance) fields.push_back({"heuristic_decoration", "performance"});
    return fields;
  };
  const std::string head = "case:concept:name,concept:name,time:timestamp,";
  const std::string start_gap = head +
                                "start_timestamp\nc1,a,2021-03-01 08:00:00,\nc1,b,2021-03-01 09:01:00,2021-03-01 08:30:00\n"
                                "c2,a,2021-03-01 08:00:00,2021-03-01 07:30:00\n";
  const std::string text_starts = head + "start_timestamp\nc1,a,2021-03-01 08:00:00,early\nc2,b,2021-03-01 08:01:00,late\n";
  const std::string st2_gap = head + "st2\nc1,a,2021-03-01 08:00:00,\nc1,b,2021-03-01 08:01:00,2021-03-01 08:00:30\n";
  const std::string st2 = head +
                          "st2\nc1,a,2021-03-01 08:00:00,2021-03-01 07:00:00\nc1,b,2021-03-01 08:01:00,2021-03-01 08:00:30\n"
                          "c2,b,2021-03-01 09:00:00,2021-03-01 08:00:00\n";
  const std::string resources = head + "org:resource\nc1,a,2021-03-01 08:00:00,r1\nc1,b,2021-03-01 08:01:00,r2\n";
  const std::string lifecycle_gap = head +
                                    "lifecycle:transition\nc1,a,2021-03-01 08:00:00,start\nc1,a,2021-03-01 08:01:00,\n"
                                    "c1,b,2021-03-01 08:02:00,complete\n";
  const std::string spans = head +
                            "lifecycle:transition,start_timestamp\nc1,a,2021-03-01 08:00:00,start,2021-03-01 07:00:00\n"
                            "c1,a,2021-03-01 09:00:00,complete,2021-03-01 08:30:00\n"
                            "c1,b,2021-03-02 10:00:00,complete,2021-03-01 16:00:00\n"
                            "c2,b,2021-03-06 10:00:00,complete,2021-03-05 15:00:00\n";
  const std::string lifecycle_spans = head +
                                      "lifecycle:transition\nc1,a,2021-03-01 08:00:00,start\nc1,a,2021-03-01 19:00:00,complete\n"
                                      "c1,b,2021-03-02 10:00:00,start\nc1,b,2021-03-03 10:00:00,complete\n"
                                      "c2,b,2021-03-05 15:00:00,start\nc2,b,2021-03-08 09:00:00,complete\n";
  const std::string none_date = "'<' not supported between instances of 'datetime.datetime' and 'NoneType'";
  struct Case {
    std::string label, csv;
    Fields fields;
    std::string log, dataframe, lazyframe;
  };
  const std::vector<Case> cases = {
      {"business hours over a start column with a gap", start_gap, hours,
       "error: cannot convert float NaN to integer", "error: cannot convert float NaN to integer", "error: " + none_date},
      {"business hours, performance, over a start column with a gap", start_gap, hours_perf,
       "error: cannot convert float NaN to integer", "error: cannot convert float NaN to integer", "error: " + none_date},
      {"business hours over text starts of one-event cases", text_starts, hours,
       "error: str.replace() takes no keyword arguments", "error: str.replace() takes no keyword arguments",
       "error: str.replace() takes no keyword arguments"},
      {"text starts of one-event cases", text_starts, {},
       "error: 'str' object has no attribute 'timestamp'", "error: 'str' object has no attribute 'timestamp'",
       "error: 'str' object has no attribute 'timestamp'"},
      {"a configured start key with a gap", st2_gap, key("st2"),
       "error: NaTType does not support timestamp", "error: NaTType does not support timestamp", "error: " + none_date},
      {"a configured start key, performance", st2, key("st2", true),
       "a>b=1.0@30.0", "a>b=1.0@30.0", "a>b=1.0@30.0"},
      {"the resource column as the start key", resources, key("org:resource"),
       "error: '<=' not supported between instances of 'Timestamp' and 'str'",
       "error: '<=' not supported between instances of 'Timestamp' and 'str'",
       "error: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"a start key the frame lacks reads the lifecycle", lifecycle_gap, key("zzz"),
       "error: 'float' object has no attribute 'lower'", "error: 'float' object has no attribute 'lower'",
       "error: 'NoneType' object has no attribute 'lower'"},
      {"a case column as the start key reads the lifecycle", lifecycle_gap, key("case:concept:name"),
       "error: 'float' object has no attribute 'lower'", "error: 'float' object has no attribute 'lower'",
       "error: 'NoneType' object has no attribute 'lower'"},
      {"business hours read the lifecycle", lifecycle_gap, hours,
       "error: 'float' object has no attribute 'lower'", "error: 'float' object has no attribute 'lower'",
       "error: 'NoneType' object has no attribute 'lower'"},
      {"the timestamp as the start key keeps the log", lifecycle_gap, key("time:timestamp"),
       "a>b=1.0", "a>b=1.0", "a>b=1.0"},
      {"business hours over start columns", spans, hours_perf,
       "a>b=1.0@25200.0", "a>b=1.0@25200.0", "a>b=1.0@25200.0"},
      {"business hours over lifecycle pairs", lifecycle_spans, hours_perf,
       "a>b=1.0@10800.0", "a>b=1.0@10800.0", "a>b=1.0@10800.0"},
  };
  for (const auto& c : cases) {
    check(c.label + " (log)", plusplus(client, "gj1.csv", c.csv, "log", c.fields), c.log);
    check(c.label + " (DataFrame)", plusplus(client, "gj1.csv", c.csv, "dataframe", c.fields), c.dataframe);
    check(c.label + " (LazyFrame)", plusplus(client, "gj1.csv", c.csv, "lazyframe", c.fields), c.lazyframe);
  }

  // XES uploads' frame shapes read the same keys (pm4py on read_xes's frame).
  const std::string xes_head =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n";
  const auto event = [](const std::string& activity, const std::string& time, const std::string& extra) {
    return "<event><string key=\"concept:name\" value=\"" + activity + "\"/><date key=\"time:timestamp\" value=\"" +
           time + "\"/>" + extra + "</event>";
  };
  const std::string xes_gap =
      xes_head + "<trace><string key=\"concept:name\" value=\"c1\"/>" +
      event("a", "2021-03-01T08:00:00.000+00:00", "<string key=\"lifecycle:transition\" value=\"start\"/>") +
      event("a", "2021-03-01T09:00:00.000+00:00", "") +
      event("b", "2021-03-01T10:00:00.000+00:00", "<string key=\"lifecycle:transition\" value=\"complete\"/>") +
      "</trace></log>\n";
  const std::string xes_starts =
      xes_head + "<trace><string key=\"concept:name\" value=\"c1\"/>" +
      event("a", "2021-03-01T09:00:00.000+00:00", "<date key=\"st2\" value=\"2021-03-01T08:00:00.000+00:00\"/>") +
      event("b", "2021-03-02T10:00:00.000+00:00", "") + "</trace></log>\n";
  for (const auto& shape : {std::string("dataframe"), std::string("lazyframe")}) {
    const std::string lower = shape == "dataframe" ? "'float'" : "'NoneType'";
    check("XES business hours read the lifecycle (" + shape + ")", plusplus(client, "gj1.xes", xes_gap, shape, hours),
          "error: " + lower + " object has no attribute 'lower'");
    check("XES start key the frame lacks (" + shape + ")", plusplus(client, "gj1.xes", xes_gap, shape, key("zzz")),
          "error: " + lower + " object has no attribute 'lower'");
    check("XES timestamp as the start key (" + shape + ")",
          plusplus(client, "gj1.xes", xes_gap, shape, key("time:timestamp")), "a>b=1.0");
    const std::string none_first = "error: '<' not supported between instances of 'NoneType' and 'datetime.datetime'";
    check("XES start key with a gap (" + shape + ")", plusplus(client, "gj1.xes", xes_starts, shape, key("st2")),
          shape == "dataframe" ? "error: NaTType does not support timestamp" : none_first);
    check("XES business hours over a start key with a gap (" + shape + ")",
          plusplus(client, "gj1.xes", xes_starts, shape, {{"heuristic_business_hours", "true"},
                                                          {"heuristic_start_timestamp_attribute", "st2"}}),
          shape == "dataframe" ? "error: cannot convert float NaN to integer" : none_first);
  }

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GJ server checks passed\n";
  return 0;
}
