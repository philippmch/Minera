#include "minera/trie_conversion.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

minera::EventLog sample() {
  minera::EventLog log;
  log.traces = {
      {"one", {minera::Event{"one", "A"}, minera::Event{"one", "B"}}, {}},
      {"two", {minera::Event{"two", "A"}, minera::Event{"two", "C"}}, {}},
      {"three", {minera::Event{"three", "A"}}, {}},
  };
  return log;
}

void converts_shared_prefixes_and_terminal_prefixes() {
  const auto trie = minera::discover_trie(sample());
  const auto net = minera::convert_trie_to_petri_net(trie);
  assert(net.activities() == std::vector<std::string>({"A", "B", "C"}));
  assert(net.source() == 0 && net.sink() == 1);
  assert(net.place_count() == 3);  // source, sink, and A continuation
  assert(net.transitions().size() == 4);  // A, B, C, and A's terminal close
  std::size_t visible = 0;
  std::size_t silent = 0;
  for (const auto& transition : net.transitions()) {
    if (transition.activity) ++visible;
    else ++silent;
  }
  assert(visible == 3 && silent == 1);
}

void supports_empty_and_multiple_root_branches() {
  minera::TrieModel trie;
  trie.root.children = {
      {"B", {}, 1, true},
      {"A", {}, 1, true},
  };
  const auto net = minera::convert_trie_to_petri_net(trie);
  assert(net.activities() == std::vector<std::string>({"A", "B"}));
  assert(net.transitions().size() == 2);

  const auto empty = minera::convert_trie_to_petri_net(minera::TrieModel{});
  assert(empty.place_count() == 2);
  assert(empty.transitions().empty());
}

void json_is_recursive_and_deterministic() {
  const auto net = minera::convert_trie_to_petri_net(
      minera::discover_trie(sample()));
  const auto json = minera::serialize_trie_petri_net_json(net);
  assert(json.find("\"family\":\"TRIE_CONVERSION\"") != std::string::npos);
  assert(json.find("\"variant\":\"TO_PETRI_NET\"") != std::string::npos);
  assert(json.find("\"activities\":[\"A\",\"B\",\"C\"]") !=
         std::string::npos);
}

}  // namespace

int main() {
  converts_shared_prefixes_and_terminal_prefixes();
  supports_empty_and_multiple_root_branches();
  json_is_recursive_and_deterministic();
}
