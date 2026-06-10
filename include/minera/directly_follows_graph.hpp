#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct DirectlyFollowsEdge {
  std::size_t source = 0;
  std::size_t target = 0;
  std::uint64_t count = 0;
};

struct TraceVariant {
  std::vector<std::string> activities;
  std::uint64_t count = 0;
};

class DirectlyFollowsGraph {
 public:
  [[nodiscard]] const std::vector<std::string>& activities() const noexcept {
    return activities_;
  }

  [[nodiscard]] const std::vector<DirectlyFollowsEdge>& edges() const noexcept {
    return edges_;
  }

  [[nodiscard]] const std::vector<TraceVariant>& variants() const noexcept {
    return variants_;
  }

  [[nodiscard]] std::optional<std::size_t> activity_id(
      std::string_view activity) const;

  [[nodiscard]] std::uint64_t activity_count(std::string_view activity) const;
  [[nodiscard]] std::uint64_t start_activity_count(
      std::string_view activity) const;
  [[nodiscard]] std::uint64_t end_activity_count(
      std::string_view activity) const;
  [[nodiscard]] std::uint64_t edge_count(
      std::string_view source,
      std::string_view target) const;

 private:
  friend DirectlyFollowsGraph discover_directly_follows_graph(
      const EventLog& event_log);

  std::size_t intern_activity(const std::string& activity);
  void add_edge(std::size_t source, std::size_t target);
  void add_variant(std::vector<std::string> activities);

  std::vector<std::string> activities_;
  std::unordered_map<std::string, std::size_t> activity_ids_;
  std::vector<std::uint64_t> activity_counts_;
  std::vector<std::uint64_t> start_activity_counts_;
  std::vector<std::uint64_t> end_activity_counts_;
  std::vector<DirectlyFollowsEdge> edges_;
  std::unordered_map<std::uint64_t, std::size_t> edge_ids_;
  std::vector<TraceVariant> variants_;
  std::unordered_map<std::string, std::size_t> variant_ids_;
};

[[nodiscard]] DirectlyFollowsGraph discover_directly_follows_graph(
    const EventLog& event_log);

}  // namespace minera
