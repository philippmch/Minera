#pragma once

namespace minera {

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

}  // namespace minera
