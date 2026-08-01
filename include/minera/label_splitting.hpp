#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class LabelSplittingVariant {
  CONTEXTUAL,
  kContextual = CONTEXTUAL,
};

struct LabelSplittingOptions {
  LabelSplittingVariant variant = LabelSplittingVariant::CONTEXTUAL;
  std::string activity_key = "activity";
  std::string target_key = "activity";
  std::string activities_suffix = "_";
  std::size_t prefix_length = 2;
  std::size_t suffix_length = 2;
  double min_edge_weight = 0.0;
  std::vector<std::string> target_activities;
};

struct LabelSplittingResult {
  LabelSplittingVariant variant = LabelSplittingVariant::CONTEXTUAL;
  EventLog log;
};

[[nodiscard]] LabelSplittingResult split_activity_labels(
    const EventLog& log, const LabelSplittingOptions& options = {});

[[nodiscard]] inline LabelSplittingResult contextual_label_splitting(
    const EventLog& log, const LabelSplittingOptions& options = {}) {
  return split_activity_labels(log, options);
}

}  // namespace minera
