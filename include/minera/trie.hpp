#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Options controlling conversion of an event log into a prefix trie.
struct TrieOptions {
  // "activity" selects Event::activity.  Other names select a generic event
  // attribute; "concept:name" is accepted as an alias for activity.
  std::string activity_key = "activity";
  // When present, only this many events are retained per variant.  A
  // truncated endpoint is terminal in the resulting trie.
  std::optional<std::size_t> max_path_length;
};

struct TrieNode {
  std::string label;
  std::vector<TrieNode> children;
  std::size_t depth = 0;
  bool final = false;

  friend bool operator==(const TrieNode&, const TrieNode&) = default;
};

struct TrieModel {
  TrieNode root;

  friend bool operator==(const TrieModel&, const TrieModel&) = default;
};

using TrieDiscoveryOptions = TrieOptions;
using LogToTrieOptions = TrieOptions;
using Trie = TrieModel;

[[nodiscard]] TrieModel discover_trie(const EventLog& log,
                                      const TrieOptions& options = {});

[[nodiscard]] inline TrieModel log_to_trie(
    const EventLog& log, const TrieOptions& options = {}) {
  return discover_trie(log, options);
}

}  // namespace minera
