#pragma once

#include "minera/declare.hpp"
#include "minera/event_log.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace minera {

struct DeclareStreamingConstraintIdentity {
  std::size_t constraint_index = 0;
  std::string template_name;
  std::string first;
  std::string second;
  bool binary = false;
};

struct DeclareStreamingEventResult {
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::size_t event_index = 0;
  std::size_t deviations = 0;
  std::vector<DeclareStreamingConstraintIdentity> violated_constraints;
};

struct DeclareStreamingTimeDeviation {
  std::string timestamp;
  std::size_t deviations = 0;
};

struct DeclareStreamingConstraintState {
  std::size_t constraint_index = 0;
  std::string template_name;
  std::string first;
  std::string second;
  bool binary = false;
  std::string state;
};

struct DeclareStreamingCaseSnapshot {
  std::size_t events = 0;
  std::size_t deviations = 0;
  std::vector<DeclareStreamingConstraintState> constraint_states;
};

struct DeclareStreamingSnapshot {
  std::size_t total_events_processed = 0;
  std::size_t total_deviations = 0;
  std::vector<DeclareStreamingTimeDeviation> deviations_per_time;
  std::map<std::string, DeclareStreamingCaseSnapshot> cases;
};

// Stateful, prefix-only Declare conformance monitor. It intentionally has no
// end-of-case operation: pending obligations are not finalized by receive().
class DeclareStreamingConformance {
 public:
  explicit DeclareStreamingConformance(const DeclareModel& model);
  DeclareStreamingConformance(const DeclareStreamingConformance&) = delete;
  DeclareStreamingConformance& operator=(const DeclareStreamingConformance&) = delete;
  DeclareStreamingConformance(DeclareStreamingConformance&&) = delete;
  DeclareStreamingConformance& operator=(DeclareStreamingConformance&&) = delete;

  [[nodiscard]] DeclareStreamingEventResult receive(const Event& event);
  [[nodiscard]] DeclareStreamingEventResult receive(std::string case_id,
                                                     std::string activity,
                                                     std::string timestamp = {});
  // A raw stream record as the reference reads it: a missing case id is
  // "undefined_case", a None case id is its own case (apart from "None"), and
  // a missing or None activity matches no constraint activity.
  [[nodiscard]] DeclareStreamingEventResult receive_record(
      std::optional<std::string> case_id, bool case_is_null,
      std::optional<std::string> activity, std::string timestamp = {});
  void reset_case(const std::string& case_id);
  void reset_all();
  [[nodiscard]] DeclareStreamingSnapshot snapshot() const;

 private:
  DeclareStreamingEventResult process(std::string case_id,
                                      std::optional<std::string> activity,
                                      std::string timestamp);
  struct Rule;
  struct Case;
  DeclareModel model_;
  std::vector<std::shared_ptr<Rule>> rules_;
  std::map<std::string, std::shared_ptr<Case>> cases_;
  std::size_t total_events_ = 0;
  std::size_t total_deviations_ = 0;
  std::vector<DeclareStreamingTimeDeviation> deviations_per_time_;
  mutable std::mutex mutex_;
};

using DeclareStreamingAutomata = DeclareStreamingConformance;

}  // namespace minera
