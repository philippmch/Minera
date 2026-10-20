#include "minera/filtering_boundary_common.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace minera {
namespace {

SortedBoundaryActivities stable_frequency_order(
    const OrderedBoundaryActivityCounts& counts) {
  std::set<std::string> labels;
  for (const auto& [label, count] : counts) {
    if (!labels.insert(label).second)
      throw std::invalid_argument("boundary frequency mapping has a duplicate label");
  }
  auto result = counts;
  std::stable_sort(result.begin(), result.end(),
      [](const auto& left, const auto& right) { return left.second > right.second; });
  return result;
}

// Comparing uint64_t directly with double rounds the integer first and loses
// strict comparisons above 2^53. Compare against the integral floor instead;
// the range check also makes the float-to-integer conversion well defined.
bool frequency_exceeds(std::uint64_t frequency, double boundary) {
  if (std::isnan(boundary)) return false;
  if (boundary < 0.0) return true;
  if (boundary >= 0x1p64) return false;
  return frequency > static_cast<std::uint64_t>(boundary);
}

std::uint64_t adaptive_threshold(
    const SortedBoundaryActivities& activities, double decreasing_factor) {
  if (activities.empty()) throw std::out_of_range("list index out of range");
  auto threshold = activities.front().second;
  for (std::size_t i = 1; i < activities.size(); ++i) {
    const auto candidate = activities[i].second;
    if (frequency_exceeds(candidate,
            static_cast<double>(threshold) * decreasing_factor))
      threshold = candidate;
  }
  return threshold;
}

}  // namespace

SortedBoundaryActivities get_sorted_start_activities_list(
    const OrderedBoundaryActivityCounts& counts) {
  return stable_frequency_order(counts);
}
SortedBoundaryActivities get_sorted_end_activities_list(
    const OrderedBoundaryActivityCounts& counts) {
  return stable_frequency_order(counts);
}
std::uint64_t get_start_activities_threshold(
    const SortedBoundaryActivities& activities, double decreasing_factor) {
  return adaptive_threshold(activities, decreasing_factor);
}
std::uint64_t get_end_activities_threshold(
    const SortedBoundaryActivities& activities, double decreasing_factor) {
  return adaptive_threshold(activities, decreasing_factor);
}

}  // namespace minera
