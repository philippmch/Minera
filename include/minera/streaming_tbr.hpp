#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

enum class StreamingTbrVariant {
  kForward,
  kBackwards,
  FORWARD = kForward,
  BACKWARDS = kBackwards,
};

struct StreamingTbrOptions {
  StreamingTbrVariant variant = StreamingTbrVariant::kForward;
  std::string activity_attribute = "activity";
  std::string case_id_attribute = "case_id";
  std::size_t maximum_iterations_invisibles = 10;
  bool stop_immediately_unfit = false;
};

struct StreamingTbrCaseSnapshot {
  std::size_t events = 0;
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;
  bool is_fit = true;
  std::vector<int> marking;
};

struct StreamingTbrEventResult {
  std::string case_id;
  std::string activity;
  std::size_t event_index = 0;
  std::uint64_t missing_added = 0;
  bool accepted = false;
  bool is_fit = true;
};

struct StreamingTbrTerminationResult {
  std::vector<int> marking;
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;
  bool is_fit = false;
};

struct StreamingTbrSnapshot {
  std::string variant = "FORWARD";
  std::size_t total_events_processed = 0;
  std::uint64_t total_missing = 0;
  std::map<std::string, StreamingTbrCaseSnapshot> cases;
};

// Stateful token replay over open cases. Visible events advance one marking;
// silent transitions are searched with a bounded breadth-first closure.
class StreamingTokenReplay {
 public:
  explicit StreamingTokenReplay(PetriNet net, StreamingTbrOptions options = {});

  StreamingTokenReplay(const StreamingTokenReplay&) = delete;
  StreamingTokenReplay& operator=(const StreamingTokenReplay&) = delete;

  [[nodiscard]] const PetriNet& net() const noexcept { return net_; }
  [[nodiscard]] const StreamingTbrOptions& options() const noexcept {
    return options_;
  }

  [[nodiscard]] StreamingTbrEventResult receive(
      std::string_view case_id, std::string_view activity);
  [[nodiscard]] StreamingTbrEventResult receive(
      const Event& event, std::string_view case_id = {});

  [[nodiscard]] std::optional<StreamingTbrTerminationResult> terminate_case(
      const std::string& case_id);
  void terminate_all();
  void reset_case(const std::string& case_id);
  void reset_all();
  [[nodiscard]] StreamingTbrSnapshot snapshot() const;

 private:
  using Marking = std::vector<int>;
  struct CaseState {
    Marking marking;
    std::size_t events = 0;
    std::uint64_t missing = 0;
    bool hard_unfit = false;
  };
  struct Route {
    Marking marking;
    std::vector<std::size_t> silent_transitions;
  };

  [[nodiscard]] bool enabled(const Marking& marking,
                             std::size_t transition) const;
  std::uint64_t fire(Marking& marking, std::size_t transition) const;
  [[nodiscard]] std::optional<Route> route_to_visible(
      const Marking& marking,
      const std::vector<std::size_t>& visible_transitions) const;
  [[nodiscard]] std::optional<Route> route_to_final(
      const Marking& marking) const;
  [[nodiscard]] std::uint64_t final_missing(const Marking& marking) const;
  [[nodiscard]] std::uint64_t final_remaining(const Marking& marking) const;
  [[nodiscard]] std::string variant_name() const noexcept;

  PetriNet net_;
  StreamingTbrOptions options_;
  std::map<std::string, std::vector<std::size_t>> transitions_by_activity_;
  std::vector<std::size_t> silent_transitions_;
  std::map<std::string, CaseState> cases_;
  std::size_t total_events_processed_ = 0;
  std::uint64_t total_missing_ = 0;
  mutable std::mutex mutex_;
};

[[nodiscard]] std::string serialize_streaming_tbr_json(
    const StreamingTbrSnapshot& snapshot);

[[nodiscard]] inline std::string streaming_tbr_json(
    const StreamingTbrSnapshot& snapshot) {
  return serialize_streaming_tbr_json(snapshot);
}

}  // namespace minera
