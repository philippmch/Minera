#include "minera/dfg_io.hpp"
#include "minera/directly_follows_graph.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using minera::DfgCleanAnalysis;

bool throws(const std::string& text) {
  try {
    (void)minera::parse_dfg(text);
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

void round_trip_is_deterministic() {
  DfgCleanAnalysis source;
  source.graph[{"B", "C"}] = 4;
  source.graph[{"A", "B"}] = 7;
  source.start_activities["A"] = 2;
  source.end_activities["C"] = 2;
  const auto text = minera::to_dfg(source);
  assert(text == "3\nA\nB\nC\n1\n0x2\n1\n2x2\n0>1x7\n1>2x4\n");
  const auto reread = minera::parse_dfg(text);
  assert(reread.graph == source.graph);
  assert(reread.start_activities == source.start_activities);
  assert(reread.end_activities == source.end_activities);
  assert(minera::serialize_dfg(reread) == text);
  std::istringstream input(text);
  assert(minera::read_dfg(input).graph == source.graph);
}

void preserves_utf8_labels() {
  const auto dfg = minera::parse_dfg(
      "2\nAuftrag ✓\n送信\n1\n0x3\n1\n1x3\n0>1x3\n");
  assert(dfg.start_activities.at("Auftrag ✓") == 3);
  assert(dfg.end_activities.at("送信") == 3);
  assert(dfg.graph.at(std::pair<std::string, std::string>{"Auftrag ✓", "送信"}) == 3);
  assert(minera::to_dfg(dfg).find("Auftrag ✓") != std::string::npos);
}

void projects_discovered_graph_without_zero_registry_entries() {
  minera::EventLog log;
  log.traces = {
      {"one", {minera::Event{"one", "A", "", "", 0, {}, {}},
                minera::Event{"one", "B", "", "", 1, {}, {}}}, {}},
      {"two", {minera::Event{"two", "A", "", "", 0, {}, {}}}, {}},
  };
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto projected = minera::to_dfg_analysis(graph);
  assert(projected.graph.at({"A", "B"}) == 1);
  assert(projected.start_activities.at("A") == 2);
  assert(projected.end_activities.at("A") == 1);
  assert(projected.end_activities.at("B") == 1);
  assert(minera::to_dfg(projected).find("C") == std::string::npos);
}

void rejects_malformed_and_duplicate_rows() {
  assert(throws("1\nA\n1\n0x1\n"));  // missing end count
  assert(throws("1\nA\n0\n0\n0>1x1\n"));  // dangling edge id
  assert(throws("1\nA\n0\n0\n0>0x1 extra\n"));  // malformed count
  assert(throws("1\nA\n0\n0\n0>0x1\n\n"));  // blank row (the reference fails too)
  assert(throws("\xef\xbb\xbf" "1\nA\n0\n0\n"));  // BOM (as in the reference)
  assert(throws("1\nA\n0\n0\n0>0x-1\n"));  // negative count
}

// pm4py 2.7.22 read_dfg captures (review F3 fixtures).
void reads_what_the_reference_reads() {
  // write_dfg output with a padded label and a zero count.
  const auto padded = minera::parse_dfg(
      "3\na\nb\n sp \n2\n0x2\n2x1\n1\n1x3\n1>0x3\n0>1x2\n2>0x1\n0>0x0\n");
  assert((padded.graph == std::map<std::pair<std::string, std::string>, std::uint64_t>{
      {{"b", "a"}, 3}, {{"a", "b"}, 2}, {{"sp", "a"}, 1}, {{"a", "a"}, 0}}));
  assert((padded.start_activities == std::map<std::string, std::uint64_t>{{"a", 2}, {"sp", 1}}));
  assert((padded.end_activities == std::map<std::string, std::uint64_t>{{"b", 3}}));
  // Duplicate labels name one activity; a repeated row replaces the earlier one.
  const auto duplicate = minera::parse_dfg("2\na\na\n1\n0x1\n1\n1x1\n0>1x1\n");
  assert(duplicate.graph.at({"a", "a"}) == 1 && duplicate.start_activities.at("a") == 1);
  const auto repeated = minera::parse_dfg("2\na\nb\n1\n0x1\n1\n1x1\n0>1x1\n0>1x4\n");
  assert(repeated.graph.at({"a", "b"}) == 4);
  // Spaced numbers and CRLF rows.
  const auto spaced = minera::parse_dfg("2\r\na\r\nb\r\n1\r\n0x1\r\n1\r\n1x1\r\n0 > 1 x 1\r\n");
  assert(spaced.graph.at({"a", "b"}) == 1);
  // Zero counts are written back.
  assert(minera::parse_dfg(minera::to_dfg(padded)).graph == padded.graph);
}

// Numbers follow int() and ids follow Python indexing (pm4py 2.7.22 read_dfg).
void reads_numbers_like_python_int() {
  using Counts = std::map<std::string, std::uint64_t>;
  // Negative ids count from the end; -0 is zero.
  const auto negative = minera::parse_dfg("2\nA\nB\n1\n-2x3\n1\n-1x1\n-2>-1x2\n");
  assert(negative.graph.at({"A", "B"}) == 2);
  assert((negative.start_activities == Counts{{"A", 3}}));
  assert((negative.end_activities == Counts{{"B", 1}}));
  const auto zero = minera::parse_dfg("2\nA\nB\n1\n0x-0\n1\n1x-0\n-0>1x-0\n");
  assert(zero.graph.at({"A", "B"}) == 0 && zero.start_activities.at("A") == 0 &&
         zero.end_activities.at("B") == 0);
  // Unicode decimal digits (Arabic-Indic, fullwidth) and underscores.
  const auto unicode = minera::parse_dfg(
      "\xd9\xa2\nA\nB\n\xd9\xa1\n\xd9\xa0x\xef\xbc\x93\n1\n1x1_0\n0>1x\xd9\xa2\n");
  assert(unicode.start_activities.at("A") == 3 && unicode.end_activities.at("B") == 10 &&
         unicode.graph.at({"A", "B"}) == 2);
  // str.strip() of labels and rows and int() whitespace inside rows (NBSP).
  const auto nbsp = minera::parse_dfg(
      "2\n\xc2\xa0" "A\nB\n1\n\xc2\xa0" "0x\xc2\xa0" "3\n1\n1x1\n0 > 1 x 2\n");
  assert(nbsp.start_activities.at("A") == 3 && nbsp.graph.at({"A", "B"}) == 2);
  // A negative section size reads no rows, as range() does.
  const auto empty_starts = minera::parse_dfg("2\nA\nB\n-1\n1\n1x1\n0>1x2\n");
  assert(empty_starts.start_activities.empty() && empty_starts.end_activities.at("B") == 1);
  // Out-of-range negative ids fail as Python indexing does.
  assert(throws("2\nA\nB\n1\n-3x3\n1\n1x1\n0>1x2\n"));
  assert(throws("-1\n1\n0x1\n"));
  // int() refuses a sign separated from its digits, doubled underscores and
  // \x1c (which str.strip() removes but int() does not).
  assert(throws("1\nA\n1\n0x- 1\n0\n"));
  assert(throws("1\nA\n1\n0x1__0\n0\n"));
  assert(throws("1\nA\n1\n0x\x1c" "1\n0\n"));
  // Declared: counts are unsigned 64-bit, so negative and huge counts are refused.
  assert(throws("2\nA\nB\n1\n0x-3\n1\n1x1\n0>1x2\n"));
  assert(throws("2\nA\nB\n1\n0x99999999999999999999999\n1\n1x1\n0>1x2\n"));
}

void imports_and_exports_files() {
  DfgCleanAnalysis source;
  source.graph[{"α", "β"}] = 1;
  source.start_activities["α"] = 1;
  source.end_activities["β"] = 1;
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_dfg_io_roundtrip.dfg";
  minera::export_dfg(source, path);
  const auto loaded = minera::import_dfg(path);
  std::filesystem::remove(path);
  assert(loaded.graph == source.graph);
  assert(loaded.start_activities == source.start_activities);
  assert(loaded.end_activities == source.end_activities);
}

// write_dfg writes empty and padded labels as given and read_dfg strips them
// again (review X2-5); only a line break or NUL cannot be written.
void writes_empty_and_padded_labels() {
  DfgCleanAnalysis source;
  source.graph[{"", "a"}] = 1;
  source.start_activities[""] = 1;
  source.end_activities["a"] = 1;
  const auto text = minera::to_dfg(source);
  assert(text == "2\n\na\n1\n0x1\n1\n1x1\n0>1x1\n");
  const auto reread = minera::parse_dfg(text);
  assert(reread.graph == source.graph && reread.start_activities == source.start_activities);
  DfgCleanAnalysis padded;
  padded.graph[{" a", "b"}] = 1;
  padded.start_activities[" a"] = 1;
  padded.end_activities["b"] = 1;
  const auto padded_text = minera::to_dfg(padded);
  assert(padded_text == "2\n a\nb\n1\n0x1\n1\n1x1\n0>1x1\n");
  assert(minera::parse_dfg(padded_text).graph.at({"a", "b"}) == 1);
  const auto refused = [](const std::string& label) {
    DfgCleanAnalysis bad;
    bad.start_activities[label] = 1;
    try {
      (void)minera::to_dfg(bad);
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  assert(refused("a\nb") && refused("a\rb") && refused(std::string("a\0b", 3)));
  assert(!refused("") && !refused("  "));
}

}  // namespace

int main() {
  writes_empty_and_padded_labels();
  round_trip_is_deterministic();
  preserves_utf8_labels();
  projects_discovered_graph_without_zero_registry_entries();
  rejects_malformed_and_duplicate_rows();
  reads_what_the_reference_reads();
  reads_numbers_like_python_int();
  imports_and_exports_files();
}
