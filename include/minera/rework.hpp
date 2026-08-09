#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

enum class ReworkVariant {
  kActivities,
  kCases,

  ACTIVITIES = kActivities,
  CASES = kCases,
};

struct ReworkOptions {
  ReworkVariant variant = ReworkVariant::kActivities;
  std::string activity_key = "activity";
  std::string case_id_key = "case_id";
};

struct ReworkCaseStats {
  std::uint64_t number_activities = 0;
  std::uint64_t rework = 0;

  bool operator==(const ReworkCaseStats&) const = default;
};

struct ReworkResult {
  ReworkVariant variant = ReworkVariant::kActivities;
  std::map<std::string, std::uint64_t> activity_cases;
  std::map<std::string, ReworkCaseStats> cases;
};

[[nodiscard]] ReworkResult discover_rework(const EventLog& log,
                                           const ReworkOptions& options = {});

[[nodiscard]] inline ReworkResult rework(
    const EventLog& log, const ReworkOptions& options = {}) {
  return discover_rework(log, options);
}

[[nodiscard]] const char* rework_variant_name(ReworkVariant variant);

}  // namespace minera
