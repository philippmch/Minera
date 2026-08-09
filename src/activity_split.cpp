#include "minera/activity_split.hpp"

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

void validate_options(const ActivitySplitOptions& options) {
  if (options.activities.empty()) {
    throw std::invalid_argument(
        "activity split: at least one activity is required");
  }
  if (options.subcase_separator.empty()) {
    throw std::invalid_argument(
        "activity split: subcase separator must not be empty");
  }
  for (const auto& activity : options.activities) {
    if (activity.empty()) {
      throw std::invalid_argument(
          "activity split: activity names must not be empty");
    }
  }
}

}  // namespace

ActivitySplitOptions make_activity_split_options(
    std::vector<std::string> activities, ActivitySplitCutMode cut_mode,
    std::string_view subcase_separator) {
  ActivitySplitOptions options;
  options.activities = std::move(activities);
  options.cut_mode = cut_mode;
  options.subcase_separator = std::string(subcase_separator);
  validate_options(options);
  return options;
}

EventLog split_activity(const EventLog& event_log,
                        const ActivitySplitOptions& options) {
  validate_options(options);
  const std::unordered_set<std::string> anchors(options.activities.begin(),
                                                options.activities.end());

  EventLog result;
  result.attributes = event_log.attributes;
  result.traces.reserve(event_log.traces.size());

  for (const auto& trace : event_log.traces) {
    if (trace.events.empty()) continue;

    std::vector<Trace> pieces;
    std::size_t subcase = 0;
    auto ensure_piece = [&](std::size_t index) -> Trace& {
      while (pieces.size() <= index) {
        Trace piece;
        piece.case_id = trace.case_id + options.subcase_separator +
                        std::to_string(pieces.size());
        piece.attributes = trace.attributes;
        pieces.push_back(std::move(piece));
      }
      return pieces[index];
    };

    for (const auto& source_event : trace.events) {
      const bool is_anchor =
          anchors.find(source_event.activity) != anchors.end();
      if (is_anchor && options.cut_mode == ActivitySplitCutMode::kThis) {
        ++subcase;
      }

      auto& piece = ensure_piece(subcase);
      Event event = source_event;
      event.case_id = piece.case_id;
      piece.events.push_back(std::move(event));

      if (is_anchor && options.cut_mode == ActivitySplitCutMode::kNext) {
        ++subcase;
      }
    }

    for (auto& piece : pieces) {
      if (!piece.events.empty()) result.traces.push_back(std::move(piece));
    }
  }

  return result;
}

EventLog filter_activity_split(const EventLog& event_log,
                               const ActivitySplitOptions& options) {
  return split_activity(event_log, options);
}

EventLog split_event_log_on_activity(const EventLog& event_log,
                                     const ActivitySplitOptions& options) {
  return split_activity(event_log, options);
}

}  // namespace minera
