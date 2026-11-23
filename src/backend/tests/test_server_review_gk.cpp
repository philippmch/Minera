#undef NDEBUG
// Review GK: the classic heuristics performance view with business hours or
// a configured start key on CSV uploads (log, DataFrame and LazyFrame
// shapes) and on XES uploads' frame and log shapes; Heuristics++ with the activity
// column as the start key and with business hours over offset starts on
// CSV frames.
//
// - pm4py's classic performance DFG measures each directly-follows pair,
//   trace by trace: BusinessHours(complete(earlier), start(later)) with
//   business hours, else start(later) - complete(earlier), on the Python
//   objects the frame (or its event log) holds. A non-date raises TypeError
//   (or BusinessHours' replace errors), a NaT reads as a zero wait (with
//   business hours ValueError), Polars' None raises, a start key no event
//   holds (a `case:` key too) raises KeyError at the first pair. Minera
//   raised its own "valid start timestamp" or "missing the configured start
//   timestamp attribute" errors and read offset starts in local time.
// - On an XES upload's log shape (the hybrid legacy log) each pair reads
//   start(later) and then complete(earlier) (complete first with business
//   hours) from event dicts holding only the keys they carry: a missing key
//   raises KeyError, a non-date TypeError, where Minera raised its own
//   "valid event timestamp" errors.
// - A wait's seconds follow the backend: pandas' Timedelta.total_seconds()
//   of the exact difference floored to the microsecond on a CSV upload's
//   log and the pandas frames (1.118 s is 1.1179999999999999), Python's
//   timedelta.total_seconds() on an XES upload's legacy log and the Polars
//   copies (1.118).
// - Heuristics++ reads the activity column as the start key like any text
//   column; on CSV frames business hours read the UTC clock.
//
// Expected values are pm4py 2.7.22 black-box on the same inputs
// (review-harness round-gk: probes/refgk.py, probes/xl3.py, probes/exact.py, ct/hppcsv5.py,
// ct/optval3.py, ct/xeslog.py).
//
// Usage: minera_server_review_gk_tests <minera_server>

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
  const auto classic = [](const std::string& key, bool hours = false) {
    Fields fields = {{"heuristic_decoration", "performance"}};
    if (!key.empty()) fields.push_back({"heuristic_start_timestamp_attribute", key});
    if (hours) fields.push_back({"heuristic_business_hours", "true"});
    return fields;
  };
  const std::string csv_start_gap = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,2021-03-01 07:00:00\nc1,b,2021-03-01 09:00:00,\nc1,c,2021-03-01 10:00:00,2021-03-01 09:10:00\nc2,a,2021-03-01 08:00:00,2021-03-01 07:00:00\nc2,b,2021-03-01 09:30:00,2021-03-01 09:00:00\n";
  const std::string csv_start_text = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,early\nc1,b,2021-03-01 09:00:00,late\n";
  const std::string csv_start_int = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,1\nc1,b,2021-03-01 09:00:00,2\n";
  const std::string csv_start_full = "case:concept:name,concept:name,time:timestamp,org:resource,start_timestamp\nc1,a,2021-03-01 08:00:00,r1,2021-03-01 07:00:00\nc1,b,2021-03-01 09:00:00,r2,2021-03-01 08:20:00\nc2,a,2021-03-01 08:00:00,r1,2021-03-01 07:00:00\nc2,b,2021-03-01 09:30:00,r2,2021-03-01 08:40:00\n";
  const std::string csv_start_offset = "case:concept:name,concept:name,time:timestamp,start_timestamp\nc1,a,2021-03-01 08:00:00,2021-03-01T07:00:00+01:00\nc1,b,2021-03-02 08:00:00,2021-03-01T16:00:00+01:00\nc2,a,2021-03-05 08:00:00,2021-03-05T07:00:00+01:00\nc2,b,2021-03-08 10:00:00,2021-03-08T09:30:00+01:00\n";
  const std::string csv_lifecycle_offset = "case:concept:name,concept:name,time:timestamp,lifecycle:transition,start_timestamp\nc1,a,2021-03-01 08:00:00,start,2021-03-01T07:00:00+01:00\nc1,a,2021-03-01 09:00:00,complete,2021-03-01T08:30:00+01:00\nc1,b,2021-03-02 10:00:00,complete,2021-03-01T16:00:00+01:00\nc2,b,2021-03-08 10:00:00,complete,2021-03-05T15:00:00+01:00\n";
  const std::string xes_bools = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><boolean key=\"zzz\" value=\"true\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><boolean key=\"zzz\" value=\"false\"/></event></trace></log>\n";
  const std::string xes_start_gap = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"st2\" value=\"2021-03-01T07:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:00:00.000+00:00\"/><date key=\"st2\" value=\"2021-03-01T09:10:00.000+00:00\"/></event></trace><trace><string key=\"concept:name\" value=\"c2\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"st2\" value=\"2021-03-01T07:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:30:00.000+00:00\"/><date key=\"st2\" value=\"2021-03-01T09:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string xes_time_gap = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/></event><event><string key=\"concept:name\" value=\"c\"/><date key=\"time:timestamp\" value=\"2021-03-01T10:00:00.000+00:00\"/></event></trace></log>\n";
  const std::string xes_label_gap = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><date key=\"time:timestamp\" value=\"2021-03-01T08:00:00.000+00:00\"/><date key=\"st2\" value=\"2021-03-01T07:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><date key=\"st2\" value=\"2021-03-01T08:30:00.000+00:00\"/></event></trace></log>\n";
  const std::string sub = "error: unsupported operand type(s) for -: ";
  struct Case {
    std::string label, csv;
    Fields fields;
    std::string log, dataframe, lazyframe;
  };
  const std::vector<Case> cases = {
      {"a start column with a gap reads a zero wait", csv_start_gap, classic("start_timestamp"),
       "a>b=0.66666666666666663@1800.0 b>c=0.5@600.0", "a>b=0.66666666666666663@1800.0 b>c=0.5@600.0",
       sub + "'NoneType' and 'datetime.datetime'"},
      {"business hours over a start column with a gap", csv_start_gap, classic("start_timestamp", true),
       "error: cannot convert float NaN to integer", "error: cannot convert float NaN to integer",
       "error: 'NoneType' object has no attribute 'replace'"},
      {"text starts", csv_start_text, classic("start_timestamp"),
       sub + "'str' and 'Timestamp'", sub + "'str' and 'Timestamp'", sub + "'str' and 'datetime.datetime'"},
      {"int starts", csv_start_int, classic("start_timestamp"),
       sub + "'int' and 'Timestamp'", sub + "'int' and 'Timestamp'", sub + "'int' and 'datetime.datetime'"},
      {"a start key no event holds", csv_start_full, classic("zzz"), "error: 'zzz'", "error: 'zzz'", "error: 'zzz'"},
      {"a case column as the start key", csv_start_full, classic("case:concept:name"),
       "error: 'case:concept:name'", "error: 'case:concept:name'", "error: 'case:concept:name'"},
      {"the resource column as the start key", csv_start_full, classic("org:resource"),
       sub + "'str' and 'Timestamp'", sub + "'str' and 'Timestamp'", sub + "'str' and 'datetime.datetime'"},
      {"the activity column as the start key", csv_start_full, classic("concept:name"),
       sub + "'str' and 'Timestamp'", sub + "'str' and 'Timestamp'", sub + "'str' and 'datetime.datetime'"},
      {"a start column", csv_start_full, classic("start_timestamp"),
       "a>b=0.66666666666666663@1800.0", "a>b=0.66666666666666663@1800.0", "a>b=0.66666666666666663@1800.0"},
      {"business hours over a start column", csv_start_full, classic("start_timestamp", true),
       "a>b=0.66666666666666663@1800.0", "a>b=0.66666666666666663@1800.0", "a>b=0.66666666666666663@1800.0"},
      {"business hours over offset starts read the UTC clock", csv_start_offset, classic("start_timestamp", true),
       "a>b=0.66666666666666663@31500.0", "a>b=0.66666666666666663@31500.0", "a>b=0.66666666666666663@31500.0"},
      {"Heuristics++ with the activity column as the start key", csv_start_full,
       {{"heuristic_variant", "plusplus"}, {"heuristic_start_timestamp_attribute", "concept:name"}},
       "error: '<=' not supported between instances of 'Timestamp' and 'str'",
       "error: '<=' not supported between instances of 'Timestamp' and 'str'",
       "error: '<=' not supported between instances of 'datetime.datetime' and 'str'"},
      {"Heuristics++ business hours over offset starts", csv_lifecycle_offset,
       {{"heuristic_variant", "plusplus"}, {"heuristic_business_hours", "true"}, {"heuristic_decoration", "performance"}},
       "a>b=1.0@21600.0", "a>b=1.0@21600.0", "a>b=1.0@21600.0"},
  };
  for (const auto& c : cases) {
    check(c.label + " (log)", heuristics(client, "gk1.csv", c.csv, "log", c.fields), c.log);
    check(c.label + " (DataFrame)", heuristics(client, "gk1.csv", c.csv, "dataframe", c.fields), c.dataframe);
    check(c.label + " (LazyFrame)", heuristics(client, "gk1.csv", c.csv, "lazyframe", c.fields), c.lazyframe);
  }

  // XES uploads' frame shapes (pm4py on read_xes's frame and its Polars copy).
  struct XesCase {
    std::string label, xes;
    Fields fields;
    std::string dataframe, lazyframe;
  };
  const std::vector<XesCase> xes_cases = {
      {"a boolean start column", xes_bools, classic("zzz"), sub + "'bool' and 'Timestamp'", sub + "'bool' and 'datetime.datetime'"},
      {"a start column with a gap reads a zero wait", xes_start_gap, classic("st2"),
       "a>b=0.66666666666666663@1800.0 b>c=0.5@600.0", sub + "'NoneType' and 'datetime.datetime'"},
      {"business hours over a missing time", xes_time_gap, classic("", true),
       "error: cannot convert float NaN to integer", "error: 'NoneType' object has no attribute 'replace'"},
      {"the timestamp as the start key over a missing time", xes_time_gap, classic("time:timestamp"),
       "a>b=0.5@0.0 b>c=0.5@0.0", sub + "'NoneType' and 'datetime.datetime'"},
      {"a missing label after the waits", xes_label_gap, classic("st2"),
       "error: 'float' object is not subscriptable", "error: 'NoneType' object is not subscriptable"},
      {"business hours over a start column with a gap", xes_start_gap, classic("st2", true),
       "error: cannot convert float NaN to integer", "error: 'NoneType' object has no attribute 'replace'"},
  };
  for (const auto& c : xes_cases) {
    check("XES " + c.label + " (DataFrame)", heuristics(client, "gk1.xes", c.xes, "dataframe", c.fields), c.dataframe);
    check("XES " + c.label + " (LazyFrame)", heuristics(client, "gk1.xes", c.xes, "lazyframe", c.fields), c.lazyframe);
  }

  // XES uploads' log shape (pm4py on the hybrid legacy log, whose events
  // hold only the keys they carry): each pair reads start(later), then
  // complete(earlier) (complete first with business hours); a key the
  // event lacks raises KeyError, a non-date TypeError.
  const std::string xes_text_time = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><string key=\"time:timestamp\" value=\"n/a\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2021-03-01T09:00:00.000+00:00\"/><int key=\"zzz\" value=\"4\"/></event></trace></log>\n";
  struct LogCase {
    std::string label, xes;
    Fields fields;
    std::string expected;
  };
  const std::vector<LogCase> log_cases = {
      {"a missing time", xes_time_gap, classic(""), "error: 'time:timestamp'"},
      {"business hours over a missing time", xes_time_gap, classic("", true), "error: 'time:timestamp'"},
      {"the activity as the start key before a missing time", xes_time_gap, classic("concept:name"),
       sub + "'str' and 'datetime.datetime'"},
      {"a start key one event lacks", xes_start_gap, classic("st2"), "error: 'st2'"},
      {"business hours over a start key one event lacks", xes_start_gap, classic("st2", true), "error: 'st2'"},
      {"a boolean start key", xes_bools, classic("zzz"), sub + "'bool' and 'datetime.datetime'"},
      {"business hours over a boolean start key", xes_bools, classic("zzz", true),
       "error: 'bool' object has no attribute 'replace'"},
      {"a text time", xes_text_time, classic(""), sub + "'datetime.datetime' and 'str'"},
      {"an int start key over a text time", xes_text_time, classic("zzz"), sub + "'int' and 'str'"},
      {"business hours over a text time", xes_text_time, classic("zzz", true),
       "error: str.replace() takes no keyword arguments"},
  };
  for (const auto& c : log_cases)
    check("XES log " + c.label, heuristics(client, "gk1.xes", c.xes, "log", c.fields), c.expected);

  // A wait's seconds: pandas' Timedelta.total_seconds() of the exact
  // difference floored to the microsecond (a CSV upload's log, the pandas
  // frames), Python's timedelta.total_seconds() of instants truncated to the
  // microsecond (an XES upload's legacy log, the Polars copies).
  const std::string csv_wait = "case:concept:name,concept:name,time:timestamp\nc1,a,2020-01-01 00:00:00.000\nc1,b,2020-01-01 00:00:01.118\n";
  const std::string xes_wait = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<log xes.version=\"1849-2016\" xmlns=\"http://www.xes-standard.org/\">\n<trace><string key=\"concept:name\" value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:00:00.000+00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:00:01.118+00:00\"/></event></trace></log>\n";
  const std::string csv_nanos = "case:concept:name,concept:name,time:timestamp,st\nc1,a,2020-01-01 00:00:00.000000500,2020-01-01 00:00:00\nc1,b,2020-01-01 00:00:20,2020-01-01 00:00:14.5\n";
  const std::string pandas_wait = "a>b=0.5@1.1179999999999999", python_wait = "a>b=0.5@1.1180000000000001";
  check("CSV wait seconds (log)", heuristics(client, "gk2.csv", csv_wait, "log", classic("")), pandas_wait);
  check("CSV wait seconds (DataFrame)", heuristics(client, "gk2.csv", csv_wait, "dataframe", classic("")), pandas_wait);
  check("CSV wait seconds (LazyFrame)", heuristics(client, "gk2.csv", csv_wait, "lazyframe", classic("")), python_wait);
  check("XES wait seconds (log)", heuristics(client, "gk2.xes", xes_wait, "log", classic("")), python_wait);
  check("XES wait seconds (DataFrame)", heuristics(client, "gk2.xes", xes_wait, "dataframe", classic("")), pandas_wait);
  check("XES wait seconds (LazyFrame)", heuristics(client, "gk2.xes", xes_wait, "lazyframe", classic("")), python_wait);
  check("CSV start-key nanoseconds (log)", heuristics(client, "gk3.csv", csv_nanos, "log", classic("st")), "a>b=0.5@14.499999000000001");
  check("CSV start-key nanoseconds (DataFrame)", heuristics(client, "gk3.csv", csv_nanos, "dataframe", classic("st")), "a>b=0.5@14.499999000000001");
  check("CSV start-key nanoseconds (LazyFrame)", heuristics(client, "gk3.csv", csv_nanos, "lazyframe", classic("st")), "a>b=0.5@14.5");

  ::kill(server, SIGTERM);
  int status = 0;
  ::waitpid(server, &status, 0);
  if (g_failures) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review GK server checks passed\n";
  return 0;
}
