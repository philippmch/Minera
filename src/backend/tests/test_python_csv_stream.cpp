#include "minera/python_csv_stream.hpp"
#include "minera/streaming_dfg.hpp"

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace minera;

namespace {

StreamingDfgSnapshot stream_dfg(const std::string& text) {
  std::istringstream input(text);
  const auto records = read_python_csv_event_stream(input);
  StreamingDfgDiscovery dfg({"concept:name", "case:concept:name"});
  for (std::size_t i = 0; i < records.size(); ++i)
    if (const auto case_id = python_csv_case_key(records[i], "case:concept:name"))
      dfg.receive(python_csv_record_event(records[i], i), *case_id);
  return dfg.snapshot();
}

}  // namespace

int main() {
  // csv.DictReader over a text-mode file, as the reference streaming CSV
  // importer reads it (PM4Py 2.7.22, public calls).
  {
    std::istringstream input(
        "case:concept:name,concept:name\nc1\nc1,A,extra\n\n  c2 , B \n"
        "\"c3\",\"multi\r\nline\"\n,\nc1,\"a\rb\"\n\"ab\"cd,x\"y\n\"open");
    const auto records = read_python_csv_event_stream(input);
    assert(records.size() == 8);
    assert(*records[0].find("case:concept:name") == std::optional<std::string>("c1"));
    assert(!records[0].find("concept:name")->has_value());  // short row: None
    assert(records[1].extras == std::vector<std::string>{"extra"});
    assert(**records[2].find("case:concept:name") == "  c2 ");  // no trimming
    assert(**records[3].find("concept:name") == "multi\nline");
    assert(**records[4].find("case:concept:name") == "" && **records[4].find("concept:name") == "");
    assert(**records[5].find("concept:name") == "a\nb");  // lone CR reads LF
    assert(**records[6].find("case:concept:name") == "abcd" && **records[6].find("concept:name") == "x\"y");
    assert(**records[7].find("case:concept:name") == "open");  // unterminated quote
    assert(!records[7].find("concept:name")->has_value());
  }
  {
    // Repeated header names keep the first position and the last value.
    std::istringstream input("k,a,k\r\n1,2,3\r\n4\r\n");
    const auto records = read_python_csv_event_stream(input);
    assert(records[0].fields.size() == 2 && records[0].fields[0].first == "k" &&
           **records[0].find("k") == "3");
    assert(!records[1].find("k")->has_value());  // the missing third cell wins
  }
  {
    // Reference streaming DFG of the first input.
    const auto s = stream_dfg(
        "case:concept:name,concept:name\nc1\nc1,A,extra\n\n  c2 , B \n"
        "\"c3\",\"multi\r\nline\"\n,\nc1,\"a\rb\"\n");
    assert(s.graph.size() == 2 && s.graph.at({"None", "A"}) == 1 && s.graph.at({"A", "a\nb"}) == 1);
    assert(s.activities.size() == 6 && s.activities.at("None") == 1 && s.activities.at(" B ") == 1);
    assert(s.start_activities.size() == 4 && s.start_activities.at("") == 1 &&
           s.start_activities.at("None") == 1 && s.start_activities.at("multi\nline") == 1);
    assert(s.end_activities.size() == 4 && s.end_activities.at("a\nb") == 1);
  }
  {
    // A None case id joins the text case "None"; a row without the case
    // column is skipped.
    const auto s = stream_dfg(
        "concept:name,case:concept:name\n,None\nc2,x\"y\nc1,None\n\"c,3\",,z\n\"c,3\"\nc1,A\nc2,A\n");
    assert(s.graph.size() == 3 && s.graph.at({"", "c1"}) == 1 &&
           s.graph.at({"c1", "c,3"}) == 1 && s.graph.at({"c1", "c2"}) == 1);
    assert(s.end_activities.at("c,3") == 2 && s.end_activities.at("c2") == 2);
    const auto other = stream_dfg("case:concept:name,other\nc1,A\n");
    assert(other.events == 0);
  }
  {
    // NUL is an ordinary character in values, quoted values and headers, and
    // the DFG JSON escapes it.
    const std::string text("case:concept:name,concept\0name\nc\0,A\0B\nc\0,\"Q\0\"\n", 46);
    std::istringstream input(text);
    const auto records = read_python_csv_event_stream(input);
    assert(records.size() == 2 && records[0].fields[1].first == std::string("concept\0name", 12));
    assert(**records[0].find("case:concept:name") == std::string("c\0", 2));
    assert(**records[1].find(std::string("concept\0name", 12)) == std::string("Q\0", 2));
    StreamingDfgDiscovery dfg({std::string("concept\0name", 12), "case:concept:name"});
    for (std::size_t i = 0; i < records.size(); ++i)
      dfg.receive(python_csv_record_event(records[i], i), *python_csv_case_key(records[i], "case:concept:name"));
    const auto json = serialize_streaming_dfg_json(dfg.snapshot());
    assert(json.find("A\\u0000B") != std::string::npos && json.find(std::string(1, '\0')) == std::string::npos);
  }
  {
    // Strict UTF-8 text: a malformed byte fails before any record.
    for (const std::string bad : {std::string("a\n\xff\n"), std::string("a\n\xc3\n"), std::string("a\n\xed\xa0\x80\n"),
                                  std::string("a\n\xc0\xaf\n")}) {
      std::istringstream input(bad);
      bool threw = false;
      try { (void)read_python_csv_event_stream(input); } catch (const std::runtime_error& e) {
        threw = std::string(e.what()).find("'utf-8' codec can't decode") == 0;
      }
      assert(threw);
    }
  }
  {
    // csv.field_size_limit(): 131072 characters, counted as code points.
    const auto read = [](const std::string& field) {
      std::istringstream input("a,b\n" + field + ",y\n");
      return read_python_csv_event_stream(input);
    };
    assert(read(std::string(131072, 'x')).size() == 1);
    std::string accented;
    for (int i = 0; i < 70001; ++i) accented += "\xc3\xa9";
    assert(read(accented).size() == 1);
    assert(read("\"" + std::string(131071, 'x') + "\"\"\"").size() == 1);
    bool threw = false;
    try { (void)read(std::string(131073, 'x')); } catch (const std::runtime_error& e) {
      threw = std::string(e.what()) == "field larger than field limit (131072)";
    }
    assert(threw);
  }
  return 0;
}
