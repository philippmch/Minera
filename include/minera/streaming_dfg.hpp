#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "minera/dfg_variants.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct StreamingDfgOptions {
  std::string activity_attribute = "activity";
  std::string case_id_attribute = "case_id";
};

struct StreamingDfgSnapshot {
  DfgFrequency graph;
  std::map<std::string, std::uint64_t> activities;
  std::map<std::string, std::uint64_t> start_activities;
  std::map<std::string, std::uint64_t> end_activities;
  std::map<std::string, std::string> open_cases;
  std::uint64_t events = 0;
};

// Incrementally discovers a directly-follows graph from case-ordered events.
// The selected activity of an event for the stream: a literal attribute of that
// name wins (a present null reads "None"), otherwise the projected first-class
// activity when the selector names it. nullopt when the key is missing.
[[nodiscard]] std::optional<std::string> streaming_event_activity(
    const Event& event, std::string_view selector);

// Every accepted event updates its activity count and either starts a case or
// increments the edge from that case's previous activity. End counts are
// derived from the current last activity of every open case at snapshot time.
// Operations are serialized so a single instance can safely receive and read
// from different threads.
class StreamingDfgDiscovery {
 public:
  explicit StreamingDfgDiscovery(StreamingDfgOptions options = {});

  [[nodiscard]] const StreamingDfgOptions& options() const noexcept {
    return options_;
  }

  // Empty case ids and activities are ordinary values, as in the reference
  // stream: every call is accepted and returns true.
  bool receive(std::string_view case_id, std::string_view activity);

  // Reads the activity named by options().activity_attribute (see
  // streaming_event_activity). Returns false, skipping the event, when the
  // event has no such key.
  bool receive(const Event& event, std::string_view case_id);

  [[nodiscard]] StreamingDfgSnapshot snapshot() const;

  void reset();

 private:
  StreamingDfgOptions options_;
  mutable std::mutex mutex_;
  DfgFrequency graph_;
  std::map<std::string, std::uint64_t> activities_;
  std::map<std::string, std::uint64_t> start_activities_;
  std::unordered_map<std::string, std::string> last_by_case_;
  std::uint64_t events_ = 0;
};

[[nodiscard]] std::string serialize_streaming_dfg_json(
    const StreamingDfgSnapshot& snapshot);

[[nodiscard]] inline std::string streaming_dfg_json(
    const StreamingDfgSnapshot& snapshot) {
  return serialize_streaming_dfg_json(snapshot);
}

}  // namespace minera
