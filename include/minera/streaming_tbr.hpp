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
  // Token counts per place; empty puts one token on the net's source (initial)
  // or sink (final).
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  // Per transition, its input places in document arc order (each once); the
  // reference checks missing tokens in this order. Empty uses net order.
  std::vector<std::vector<std::size_t>> input_place_order;
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

// Forward termination counts as the reference does: missing adds
// final - marking over the final places and remaining is the token total
// minus the final total, both signed (a surplus final token lowers missing,
// a missing final token lowers remaining).
struct StreamingTbrTerminationResult {
  std::vector<int> marking;
  std::int64_t missing = 0;
  std::int64_t remaining = 0;
  bool is_fit = false;
};

struct StreamingTbrSnapshot {
  std::string variant = "FORWARD";
  std::size_t total_events_processed = 0;
  std::int64_t total_missing = 0;  // signed: terminations can lower it
  std::map<std::string, StreamingTbrCaseSnapshot> cases;
};

// Stateful token replay over open cases. The forward variant follows the
// reference streaming replay: an enabled transition of the label fires;
// otherwise silent shortest paths, each marked place outside the
// transition's preset serving one input place, may enable it; otherwise
// missing tokens are inserted for the first transition of the label.
// Ordinary arcs act by weight, each arc on its own (zero-weight and parallel
// arcs included); negative weights are rejected. Termination compares the
// marking with the final marking exactly. BACKWARDS keeps a bounded
// breadth-first silent closure.
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
    // Marked places in the order they gained a token, as the reference
    // marking dictionary iterates them; this order picks silent-path sources.
    std::vector<std::size_t> order;
    std::size_t events = 0;
    std::uint64_t missing = 0;
    bool hard_unfit = false;
  };
  struct Route {
    Marking marking;
    std::vector<std::size_t> silent_transitions;
  };
  // One ordinary arc with its weight. Repeated-unit endpoints of a place form
  // one arc; explicit occurrence weights keep parallel arcs apart.
  struct Arc {
    std::size_t place = 0;
    int weight = 1;
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
  void ordered_fire(CaseState& state, std::size_t transition) const;
  [[nodiscard]] std::vector<Arc> ordered_input_arcs(std::size_t transition) const;
  // Returns the state that enables the transition. Otherwise *stalled holds
  // the marking the reference's silent loop stopped at without failing (a
  // path that left the marking unchanged, or the iteration limit), and stays
  // empty when the reference's search failed (no path, or a blocked one).
  [[nodiscard]] std::optional<CaseState> enable_with_invisibles(
      const CaseState& state, std::size_t transition,
      std::optional<CaseState>* stalled = nullptr) const;

  PetriNet net_;
  StreamingTbrOptions options_;
  std::map<std::string, std::vector<std::size_t>> transitions_by_activity_;
  std::vector<std::size_t> silent_transitions_;
  std::vector<std::vector<Arc>> input_arcs_;
  std::vector<std::vector<Arc>> output_arcs_;
  Marking initial_;
  std::vector<std::size_t> initial_order_;
  Marking final_;
  // silent_paths_[from][to]: a shortest sequence of silent transitions moving
  // a token from one place to another (breadth first, index order).
  std::vector<std::vector<std::optional<std::vector<std::size_t>>>> silent_paths_;
  std::map<std::string, CaseState> cases_;
  std::size_t total_events_processed_ = 0;
  std::int64_t total_missing_ = 0;
  mutable std::mutex mutex_;
};

[[nodiscard]] std::string serialize_streaming_tbr_json(
    const StreamingTbrSnapshot& snapshot);

[[nodiscard]] inline std::string streaming_tbr_json(
    const StreamingTbrSnapshot& snapshot) {
  return serialize_streaming_tbr_json(snapshot);
}

}  // namespace minera
