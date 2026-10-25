// algo.discovery.ocel.interleavings.utils.merge_dataframe_rel_cases against
// the public seeded bank (docs/interleavings-merge-bank.txt.gz,
// scripts/capture-interleavings-merge-bank.py). Result tables compare as their
// complete native carrier (columns, dtypes, index class and labels, every
// cell); errors compare by class and message.
#include "minera/dataframe_format_json.hpp"
#include "minera/interleavings_merge.hpp"

#include "../gzip_codec.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct BankCase {
  std::string name;
  std::string kind;
  InterleavingsMergeParameters parameters;
  std::string left, right, relations;
  std::string error;     // "type message"
  std::string expected;  // table JSON
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    const auto tag = line.substr(0, line.find(' '));
    const auto rest = line.substr(tag.size() + 1);
    if (tag == "C") {
      cases.emplace_back();
      cases.back().name = rest.substr(0, rest.find(' '));
      cases.back().kind = rest.substr(rest.find(' ') + 1);
      continue;
    }
    auto& item = cases.back();
    if (tag == "K") {
      const auto key = rest.substr(0, rest.find(' '));
      const auto value = rest.substr(rest.find(' ') + 1);
      auto& p = item.parameters;
      if (key == "CASE_ID_KEY") p.case_id_key = value;
      else if (key == "ACTIVITY_KEY") p.activity_key = value;
      else if (key == "TIMESTAMP_KEY") p.timestamp_key = value;
      else if (key == "INDEX_KEY") p.index_key = value;
      else if (key == "LEFT_SUFFIX") p.left_suffix = value;
      else if (key == "RIGHT_SUFFIX") p.right_suffix = value;
      else throw std::runtime_error("unknown parameter " + key);
    } else if (tag == "L") item.left = rest;
    else if (tag == "R") item.right = rest;
    else if (tag == "P") item.relations = rest;
    else if (tag == "E") item.error = rest;
    else if (tag == "O") item.expected = rest;
  }
  return cases;
}

std::string first_difference(const std::string& expected, const std::string& actual) {
  std::size_t at = 0;
  while (at < expected.size() && at < actual.size() && expected[at] == actual[at]) ++at;
  const auto from = at > 150 ? at - 150 : 0;
  return "\n  expected ..." + expected.substr(from, 400) + "\n  actual   ..." + actual.substr(from, 400);
}

std::size_t counts[3] = {};

// directly_follows_dataframe on one table.
std::string check_directly_follows(const BankCase& item) {
  ++counts[0];
  std::string actual;
  try {
    actual = serialize_event_table_input_json(
        interleavings_directly_follows_dataframe(parse_event_table_json(item.left), item.parameters));
  } catch (const InterleavingsMergeError& error) {
    ++counts[2];
    const auto raised = error.type + " " + error.what();
    return raised == item.error ? "" : "raised '" + raised + "' expected '" + item.error + "'";
  }
  if (!item.error.empty()) return "expected " + item.error;
  const auto expected = serialize_event_table_input_json(parse_event_table_json(item.expected));
  return expected == actual ? "" : "table differs" + first_difference(expected, actual);
}

// merge_dataframes on a left table, a right table and the case relations.
std::string check_merge(const BankCase& item) {
  ++counts[1];
  std::string actual;
  try {
    actual = serialize_event_table_input_json(interleavings_merge_dataframes(
        parse_event_table_json(item.left), parse_event_table_json(item.right),
        parse_event_table_json(item.relations), item.parameters));
  } catch (const InterleavingsMergeError& error) {
    ++counts[2];
    const auto raised = error.type + " " + error.what();
    return raised == item.error ? "" : "raised '" + raised + "' expected '" + item.error + "'";
  }
  if (!item.error.empty()) return "expected " + item.error;
  const auto expected = serialize_event_table_input_json(parse_event_table_json(item.expected));
  return expected == actual ? "" : "table differs" + first_difference(expected, actual);
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::size_t calls = 0, failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    ++calls;
    const auto failure = item.kind == "df" ? check_directly_follows(item) : check_merge(item);
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << " (" << item.kind << "): " << failure << '\n';
  }
  std::cout << calls << " calls compared: " << counts[0] << " directly-follows tables, " << counts[1]
            << " merges (" << counts[2] << " reference errors), " << failures << " failures\n";
  return failures || calls < 6000 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
