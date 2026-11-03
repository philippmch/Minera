#include "minera/trie.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {

std::string event_label(const Event& event, std::string_view key) {
  if (key == "activity" || key == "concept:name") {
    return event.activity;
  }
  if (const auto* value = event.attribute(key)) return *value;
  // Missing custom attributes remain a deterministic empty label.  This also
  // mirrors Event::activity, which is allowed to be empty in native logs.
  return {};
}

// Children keep the order in which their labels are first seen, walking the
// traces in log order, as the reference trie does.
TrieNode* child_for(TrieNode& node, std::string label) {
  for (auto& child : node.children) {
    if (child.label == label) return &child;
  }
  node.children.push_back(TrieNode{std::move(label), {}, node.depth + 1, false});
  return &node.children.back();
}

}  // namespace

TrieModel discover_trie(const EventLog& log, const TrieOptions& options) {
  if (options.max_path_length && *options.max_path_length == 0) {
    throw std::invalid_argument("trie discovery: max_path_length must be positive");
  }

  TrieModel result;
  result.root.label.clear();
  result.root.depth = 0;
  result.root.final = false;
  result.input_traces = log.traces.size();

  for (const auto& trace : log.traces) {
    const std::size_t limit = options.max_path_length.value_or(trace.events.size());
    const std::size_t length = std::min(limit, trace.events.size());
    if (length == 0) continue;
    if (length < trace.events.size()) result.truncated = true;

    TrieNode* node = &result.root;
    for (std::size_t index = 0; index < length; ++index) {
      node = child_for(*node, event_label(trace.events[index],
                                          options.activity_key));
    }
    if (!node->final) {
      node->final = true;
      ++result.distinct_variants;
    }
  }

  const auto count_nodes = [&](const auto& self, const TrieNode& node)
      -> std::size_t {
    std::size_t count = 1;
    for (const auto& child : node.children) count += self(self, child);
    return count;
  };
  result.node_count = count_nodes(count_nodes, result.root);

  return result;
}

}  // namespace minera
