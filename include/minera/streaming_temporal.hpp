#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

struct StreamingTemporalOptions {
  std::string activity_attribute;
  std::string start_timestamp_attribute;
  std::string timestamp_attribute;
  std::string case_id_attribute = "case_id";
  double zeta = 6.0;
};

struct StreamingTemporalCaseSnapshot {
  std::size_t events = 0;
  std::size_t deviations = 0;
};

struct StreamingTemporalEventResult {
  std::string case_id;
  std::string activity;
  std::size_t event_index = 0;
  std::vector<TemporalDeviation> deviations;
};

struct StreamingTemporalSnapshot {
  std::size_t total_events_processed = 0;
  std::size_t total_deviations = 0;
  std::map<std::string, StreamingTemporalCaseSnapshot> cases;
  std::map<std::string, std::vector<TemporalDeviation>> deviations;
};

// Prefix temporal-profile conformance monitor. Each case retains its prior
// timed events so every eligible earlier/later pair is checked on arrival.
class StreamingTemporalConformance {
 public:
  explicit StreamingTemporalConformance(
      TemporalProfile profile, StreamingTemporalOptions options = {});

  StreamingTemporalConformance(const StreamingTemporalConformance&) = delete;
  StreamingTemporalConformance& operator=(
      const StreamingTemporalConformance&) = delete;

  [[nodiscard]] const TemporalProfile& profile() const noexcept {
    return profile_;
  }
  [[nodiscard]] const StreamingTemporalOptions& options() const noexcept {
    return options_;
  }

  // Missing identifiers, activity values, or timestamps are skipped and
  // return an event_index of zero. Accepted events return new deviations.
  [[nodiscard]] StreamingTemporalEventResult receive(
      std::string_view case_id, std::string_view activity,
      std::string_view start_timestamp, std::string_view timestamp);
  [[nodiscard]] StreamingTemporalEventResult receive(
      const Event& event, std::string_view case_id = {});

  void reset_case(const std::string& case_id);
  void reset_all();
  [[nodiscard]] StreamingTemporalSnapshot snapshot() const;

 private:
  struct TimedEvent {
    double start = 0.0;
    double end = 0.0;
    std::string activity;
  };
  struct CaseState {
    std::size_t events = 0;
    std::size_t deviations = 0;
    std::vector<TimedEvent> events_in_order;
  };

  TemporalProfile profile_;
  StreamingTemporalOptions options_;
  std::map<std::pair<std::string, std::string>, const TemporalProfileEntry*>
      profile_lookup_;
  std::map<std::string, CaseState> cases_;
  std::map<std::string, std::vector<TemporalDeviation>> deviations_;
  std::size_t total_events_processed_ = 0;
  std::size_t total_deviations_ = 0;
  mutable std::mutex mutex_;
};

[[nodiscard]] std::string serialize_streaming_temporal_json(
    const StreamingTemporalSnapshot& snapshot);

[[nodiscard]] inline std::string streaming_temporal_json(
    const StreamingTemporalSnapshot& snapshot) {
  return serialize_streaming_temporal_json(snapshot);
}

}  // namespace minera
