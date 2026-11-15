#include "minera/trie.hpp"

#include "minera/dataframe_validation.hpp"
#include "minera/frame_keys.hpp"

#include <algorithm>
#include <optional>
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

TrieModel discover_trie(const EventLog& log, const TrieOptions& requested) {
  // Round CR: pm4py's discover_prefix_tree checks an upload's frame column
  // (check_pandas_dataframe_columns: a column the frame lacks, then one of
  // another dtype than str, then a gap) and keys the trie by its cells; a
  // max_path_length of 0 leaves the root alone (observed black-box).
  auto options = requested;
  std::optional<FrameKeyedActivities> keyed;
  if (log.dataframe_columns) {
    const std::string key = options.activity_key == "activity" ? std::string("concept:name") : options.activity_key;
    const auto columns = frame_keys(log, FrameKeyScope::kDataFrame);
    if (!columns.contains(key))
      throw std::invalid_argument("the specified activity column is not contained in the dataframe. Available columns: " +
                                  dataframe_available_columns({columns.begin(), columns.end()}));
    keyed = frame_keyed_activities(log, key, FrameKeyScope::kDataFrame);
    if (keyed) {
      if (const auto error = frame_activity_column_error(*keyed); !error.empty()) throw std::invalid_argument(error);
      options.activity_key = "concept:name";
    }
  }
  const auto& source = keyed ? keyed->log : log;

  TrieModel result;
  result.root.label.clear();
  result.root.depth = 0;
  result.root.final = false;
  result.input_traces = source.traces.size();

  for (const auto& trace : source.traces) {
    const std::size_t limit = options.max_path_length.value_or(trace.events.size());
    const std::size_t length = std::min(limit, trace.events.size());
    if (length < trace.events.size()) result.truncated = true;
    if (length == 0) continue;

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
