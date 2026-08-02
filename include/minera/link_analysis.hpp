#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Options for the classic event-link relation algorithm. Columns may name a
// first-class Event/Trace field (case_id, activity, timestamp, resource,
// lifecycle) or a generic event/trace attribute.
struct LinkAnalysisOptions {
  std::string out_column = "case_id";
  std::string in_column = "case_id";
  std::string sorting_column = "timestamp";
  bool look_forward = true;
  bool keep_first_occurrence = false;
  bool propagate = false;
  std::size_t max_links = 1'000'000;
};

struct LinkAnalysisEvent {
  std::size_t index = 0;
  std::size_t trace_index = 0;
  std::size_t event_index = 0;
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::string value;
};

struct LinkAnalysisLink {
  LinkAnalysisEvent source;
  LinkAnalysisEvent target;
};

struct LinkAnalysisResult {
  LinkAnalysisOptions options;
  std::size_t event_count = 0;
  std::vector<LinkAnalysisLink> links;
};

[[nodiscard]] LinkAnalysisResult discover_link_analysis(
    const EventLog& log, const LinkAnalysisOptions& options = {});

[[nodiscard]] std::string link_analysis_json(
    const LinkAnalysisResult& result);

}  // namespace minera
