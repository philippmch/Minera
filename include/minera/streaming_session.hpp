#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "minera/live_stream.hpp"
#include "minera/streaming_dfg.hpp"
#include "minera/streaming_footprints.hpp"
#include "minera/streaming_temporal.hpp"
#include "minera/streaming_tbr.hpp"
#include "minera/declare_streaming_io.hpp"

namespace minera {
enum class StreamingSessionMethod { kDfg, kFootprints, kTemporal, kTbr, kDeclare };
enum class StreamingSessionAction { kStart, kDrain, kFinish, kRegister, kDeregister, kResetAll, kResetCase, kTerminateCase, kTerminateAll };
struct StreamingSessionOptions {
  StreamingSessionMethod method = StreamingSessionMethod::kDfg;
  std::string event_order = "input";
  StreamingDfgOptions dfg;
  StreamingFootprintsOptions footprints;
  StreamingTemporalOptions temporal;
  StreamingTbrOptions tbr;
  std::optional<Footprint> footprint_model;
  std::optional<TemporalProfile> temporal_profile;
  std::optional<PetriNet> net;
  std::optional<DeclareModel> declare_model;
};
class StreamingSessionConflict : public std::runtime_error {
 public: using std::runtime_error::runtime_error;
};
// One ordered worker feeds the selected native monitor. Mutations are serialized
// and revision checked; monitor instances survive append/snapshot requests.
class StreamingSession {
 public:
  explicit StreamingSession(StreamingSessionOptions options = {});
  ~StreamingSession();
  StreamingSession(const StreamingSession&) = delete;
  StreamingSession& operator=(const StreamingSession&) = delete;
  std::size_t append(std::vector<Event> events, const std::string& batch_id, std::size_t expected_revision);
  void action(StreamingSessionAction action, std::size_t expected_revision, const std::string& case_id = {});
  [[nodiscard]] std::size_t revision() const;
  [[nodiscard]] std::string snapshot_json(bool wait_until_drained = true);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace minera
