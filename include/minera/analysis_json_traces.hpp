#pragma once

#include <string>

namespace minera {

class DirectlyFollowsGraph;
struct EventLog;

// While an instance lives on a thread, analysis_json and write_analysis_json
// on that thread leave out the "traces" array (every other key is written as
// usual). Routes whose callers ask for include_traces=false use it so a large
// log is not serialized for a client that never reads its traces.
class OmitAnalysisTraces {
 public:
  explicit OmitAnalysisTraces(bool omit = true);
  ~OmitAnalysisTraces();
  OmitAnalysisTraces(const OmitAnalysisTraces&) = delete;
  OmitAnalysisTraces& operator=(const OmitAnalysisTraces&) = delete;

 private:
  bool previous_;
};

// The "traces" array analysis_json writes inline (from "[" to "]"), byte for
// byte, for the same log and graph. GET /api/traces serves it on demand to a
// client that mined with include_traces=false; that mining response carries
// a "trace_summary" object in its place.
[[nodiscard]] std::string analysis_traces_json(const EventLog& log,
                                               const DirectlyFollowsGraph& graph);

}  // namespace minera
