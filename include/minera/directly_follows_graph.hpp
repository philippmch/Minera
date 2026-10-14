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
  // Construct an exact declared graph without manufacturing event traces.
  // Registry order and zero-valued topology entries are retained. Counts do
  // not imply activity occurrences, variants or rework observations.
  [[nodiscard]] static DirectlyFollowsGraph from_frequencies(
      const std::vector<std::string>& activities,
      const std::vector<DirectlyFollowsEdge>& edges,
      const std::vector<std::uint64_t>& start_counts,
      const std::vector<std::uint64_t>& end_counts);

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

  // Registers an activity name in the id registry without recording any
  // occurrence (its counts stay 0). Lets an imported model's activities — which
  // the log may never show — resolve to ids in exports like everything else.
  std::size_t ensure_activity(const std::string& activity) {
    return intern_activity(activity);
  }

  [[nodiscard]] std::uint64_t activity_count(std::string_view activity) const;
  [[nodiscard]] std::uint64_t start_activity_count(
      std::string_view activity) const;
  [[nodiscard]] std::uint64_t end_activity_count(
      std::string_view activity) const;
  // In how many cases the activity occurs more than once — the standard
  // rework statistic, flagging repeated work per activity.
  [[nodiscard]] std::uint64_t rework_case_count(
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
  std::vector<std::uint64_t> rework_case_counts_;
  std::vector<DirectlyFollowsEdge> edges_;
  std::unordered_map<std::uint64_t, std::size_t> edge_ids_;
  std::vector<TraceVariant> variants_;
  std::unordered_map<std::string, std::size_t> variant_ids_;
};

[[nodiscard]] DirectlyFollowsGraph discover_directly_follows_graph(
    const EventLog& event_log);

}  // namespace minera
