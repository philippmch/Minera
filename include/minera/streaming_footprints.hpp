#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/footprints.hpp"

namespace minera {

struct StreamingFootprintsOptions {
  std::string activity_attribute = "activity";
  std::string case_id_attribute = "case_id";
};

struct StreamingFootprintsCaseSnapshot {
  std::size_t events = 0;
  std::size_t deviations = 0;
  bool is_fit = true;
  std::string first_activity;
  std::string last_activity;
};

struct StreamingFootprintsEventResult {
  std::string case_id;
  std::string activity;
  std::size_t event_index = 0;
  std::size_t deviations = 0;
  bool is_fit = true;
};

struct StreamingFootprintsSnapshot {
  std::size_t total_events_processed = 0;
  std::size_t total_deviations = 0;
  std::map<std::string, StreamingFootprintsCaseSnapshot> cases;
};

// Prefix conformance monitor for a footprint model. Cases stay open until
// terminate_case() is called; this keeps prefix diagnostics separate from the
// final end-activity check.
class StreamingFootprintsConformance {
 public:
  explicit StreamingFootprintsConformance(
      Footprint model, StreamingFootprintsOptions options = {});

  StreamingFootprintsConformance(const StreamingFootprintsConformance&) =
      delete;
  StreamingFootprintsConformance& operator=(
      const StreamingFootprintsConformance&) = delete;

  [[nodiscard]] const Footprint& model() const noexcept { return model_; }
  [[nodiscard]] const StreamingFootprintsOptions& options() const noexcept {
    return options_;
  }

  // Empty identifiers are ordinary values. The result holds only deviations
  // introduced by that event. The Event overload reads the selected activity
  // and skips (default result) an event whose key is missing or null.
  [[nodiscard]] StreamingFootprintsEventResult receive(
      std::string_view case_id, std::string_view activity);
  [[nodiscard]] StreamingFootprintsEventResult receive(
      const Event& event, std::string_view case_id);

  // Finalizes one open case by checking its last activity against the model's
  // end set. Missing cases return false; otherwise the case is removed.
  bool terminate_case(const std::string& case_id);
  void terminate_all();
  void reset_case(const std::string& case_id);
  void reset_all();

  [[nodiscard]] StreamingFootprintsSnapshot snapshot() const;

 private:
  struct CaseState {
    std::size_t events = 0;
    std::size_t deviations = 0;
    std::string first_activity;
    std::string last_activity;
  };

  Footprint model_;
  StreamingFootprintsOptions options_;
  std::set<FootprintPair> allowed_pairs_;
  std::map<std::string, CaseState> cases_;
  std::size_t total_events_processed_ = 0;
  std::size_t total_deviations_ = 0;
  mutable std::mutex mutex_;
};

[[nodiscard]] std::string serialize_streaming_footprints_json(
    const StreamingFootprintsSnapshot& snapshot);

[[nodiscard]] inline std::string streaming_footprints_json(
    const StreamingFootprintsSnapshot& snapshot) {
  return serialize_streaming_footprints_json(snapshot);
}

}  // namespace minera
