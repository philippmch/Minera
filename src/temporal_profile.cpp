#include "minera/temporal_profile.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr double kWeekSeconds = 7.0 * 86400.0;

std::vector<BusinessHourSlot> default_slots() {
  std::vector<BusinessHourSlot> slots;
  slots.reserve(5);
  for (int day = 0; day < 5; ++day) {
    const double start = day * 86400.0 + 7.0 * 3600.0;
    slots.push_back({start, start + 10.0 * 3600.0});
  }
  return slots;
}

std::vector<BusinessHourSlot> normalized_slots(
    const TemporalProfileOptions& options) {
  auto slots = options.business_hour_slots.empty() ? default_slots()
                                                    : options.business_hour_slots;
  for (const auto& slot : slots) {
    if (!std::isfinite(slot.start_seconds) || !std::isfinite(slot.end_seconds) ||
        slot.start_seconds < 0.0 || slot.end_seconds > kWeekSeconds ||
        slot.start_seconds >= slot.end_seconds) {
      throw std::invalid_argument(
          "temporal profile: business-hour slots must satisfy 0 <= start < end <= 604800");
    }
  }
  std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.start_seconds != rhs.start_seconds) {
      return lhs.start_seconds < rhs.start_seconds;
    }
    return lhs.end_seconds < rhs.end_seconds;
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& slot : slots) {
    if (!merged.empty() && slot.start_seconds <= merged.back().end_seconds) {
      merged.back().end_seconds = std::max(merged.back().end_seconds,
                                            slot.end_seconds);
    } else {
      merged.push_back(slot);
    }
  }
  return merged;
}

const std::string& configured_attribute(const std::string& primary,
                                        const std::string& alias) {
  return primary.empty() ? alias : primary;
}

std::optional<double> local_wall_instant(std::string_view text);

std::optional<double> event_instant(const Event& event,
                                    const std::string& primary,
                                    const std::string& alias) {
  const auto& attribute_name = configured_attribute(primary, alias);
  if (attribute_name.empty()) {
    return parse_timestamp(event.timestamp);
  }
  const auto* value = event.attribute(attribute_name);
  if (value == nullptr) return std::nullopt;
  return parse_timestamp(*value);
}

std::optional<double> event_local_instant(const Event& event,
                                          const std::string& primary,
                                          const std::string& alias) {
  const auto& attribute_name = configured_attribute(primary, alias);
  if (attribute_name.empty()) return local_wall_instant(event.timestamp);
  const auto* value = event.attribute(attribute_name);
  if (value == nullptr) return std::nullopt;
  return local_wall_instant(*value);
}

// Business-hour semantics follow PM4Py's wall-clock treatment: timezone
// offsets are validated but ignored when selecting the weekday and clock
// time. Ordinary durations continue to use the absolute UTC instants above.
std::optional<double> local_wall_instant(std::string_view text) {
  // Validate the full instant first, then remove only its timezone suffix and
  // parse the remaining wall-clock fields as if they were UTC. This preserves
  // the local weekday/time without duplicating the ISO-8601 parser.
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end > 0 &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
  if (timezone != std::string_view::npos) end = timezone;
  return parse_timestamp(text.substr(0, end));
}

double active_segment(double start, double end,
                      const std::vector<BusinessHourSlot>& slots) {
  double total = 0.0;
  for (const auto& slot : slots) {
    const double left = std::max(start, slot.start_seconds);
    const double right = std::min(end, slot.end_seconds);
    if (right > left) total += right - left;
  }
  return total;
}

double business_elapsed(double source, double target,
                        const std::vector<BusinessHourSlot>& slots) {
  if (target <= source || slots.empty()) return 0.0;
  // Unix epoch day zero is Thursday. Shift by three days so quotient
  // boundaries are Monday 00:00, matching BusinessHourSlot's coordinate
  // system, including for dates before 1970.
  constexpr double kEpochToMonday = 3.0 * 86400.0;
  const double adjusted_source = source + kEpochToMonday;
  const double adjusted_target = target + kEpochToMonday;
  const long long source_week = static_cast<long long>(
      std::floor(adjusted_source / kWeekSeconds));
  const long long target_week = static_cast<long long>(
      std::floor(adjusted_target / kWeekSeconds));
  const double source_offset =
      adjusted_source - source_week * kWeekSeconds;
  const double target_offset =
      adjusted_target - target_week * kWeekSeconds;
  const double full_week_seconds = [&] {
    double total = 0.0;
    for (const auto& slot : slots) total += slot.end_seconds - slot.start_seconds;
    return total;
  }();
  if (source_week == target_week) {
    return active_segment(source_offset, target_offset, slots);
  }
  const auto full_weeks = target_week - source_week - 1;
  double total = active_segment(source_offset, kWeekSeconds, slots) +
                 active_segment(0.0, target_offset, slots);
  if (full_weeks > 0) {
    total += static_cast<double>(full_weeks) * full_week_seconds;
  }
  return total;
}

double elapsed_between(double source, double target,
                       double source_local, double target_local,
                       const TemporalProfileOptions& options,
                       const std::vector<BusinessHourSlot>* slots) {
  if (!options.use_business_hours) return target - source;
  return business_elapsed(source_local, target_local, *slots);
}

void validate_slots(const TemporalProfileOptions& options) {
  // Calling this also validates custom slots when business-hours mode is off,
  // making configuration mistakes fail early and consistently.
  (void)normalized_slots(options);
}

using ProfileLookup =
    std::map<std::pair<std::string, std::string>,
             const TemporalProfileEntry*>;

ProfileLookup build_profile_lookup(const TemporalProfile& profile) {
  ProfileLookup lookup;
  for (const auto& entry : profile.entries) {
    if (entry.count == 0 || !std::isfinite(entry.mean_seconds) ||
        entry.mean_seconds < 0.0 ||
        !std::isfinite(entry.standard_deviation_seconds) ||
        entry.standard_deviation_seconds < 0.0) {
      throw std::invalid_argument(
          "temporal profile: count must be positive and timing statistics must be finite and non-negative");
    }
    if (!lookup
             .emplace(std::make_pair(entry.source, entry.target), &entry)
             .second) {
      throw std::invalid_argument(
          "temporal profile: duplicate source/target profile entry");
    }
  }
  return lookup;
}

struct Aggregate {
  std::uint64_t count = 0;
  double mean = 0.0;
  double m2 = 0.0;

  void add(double value) {
    ++count;
    const double delta = value - mean;
    mean += delta / static_cast<double>(count);
    m2 += delta * (value - mean);
  }
};

}  // namespace

const TemporalProfileEntry* TemporalProfile::find(const std::string& source,
                                                   const std::string& target) const noexcept {
  // Discovery emits sorted entries, but accepting a manually assembled
  // profile in any order makes the conformance API less surprising.
  for (const auto& entry : entries) {
    if (entry.source == source && entry.target == target) return &entry;
  }
  return nullptr;
}

TemporalProfile discover_temporal_profile(const EventLog& log,
                                           const TemporalProfileOptions& options) {
  validate_slots(options);
  const auto slots = options.use_business_hours ? normalized_slots(options)
                                                : std::vector<BusinessHourSlot>{};
  const auto& source_attribute = configured_attribute(
      options.source_timestamp_attribute,
      options.source_completion_timestamp_attribute);
  const auto& target_attribute = configured_attribute(
      options.target_timestamp_attribute,
      options.target_start_timestamp_attribute);

  std::map<std::pair<std::string, std::string>, Aggregate> aggregates;
  for (const auto& trace : log.traces) {
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      const auto source_time =
          event_instant(trace.events[i], source_attribute, "");
      if (!source_time) continue;
      const auto source_local =
          options.use_business_hours
              ? event_local_instant(trace.events[i], source_attribute, "")
              : source_time;
      if (!source_local) continue;
      for (std::size_t j = i + 1; j < trace.events.size(); ++j) {
        const auto target_time =
            event_instant(trace.events[j], target_attribute, "");
        if (!target_time || *target_time < *source_time) continue;
        const auto target_local =
            options.use_business_hours
                ? event_local_instant(trace.events[j], target_attribute, "")
                : target_time;
        if (!target_local) continue;
        const double duration = elapsed_between(
            *source_time, *target_time, *source_local, *target_local, options,
            options.use_business_hours ? &slots : nullptr);
        aggregates[{trace.events[i].activity, trace.events[j].activity}].add(duration);
      }
    }
  }

  TemporalProfile profile;
  profile.entries.reserve(aggregates.size());
  for (const auto& [key, aggregate] : aggregates) {
    TemporalProfileEntry entry;
    entry.source = key.first;
    entry.target = key.second;
    entry.count = aggregate.count;
    entry.mean_seconds = aggregate.mean;
    entry.standard_deviation_seconds =
        aggregate.count > 1
            ? std::sqrt(aggregate.m2 / static_cast<double>(aggregate.count - 1))
            : 0.0;
    profile.entries.push_back(std::move(entry));
  }
  return profile;
}

std::vector<TemporalDeviation> check_temporal_profile_conformance(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  if (!std::isfinite(options.zeta) || options.zeta <= 0.0) {
    throw std::invalid_argument("temporal profile: zeta must be finite and greater than zero");
  }
  const auto profile_lookup = build_profile_lookup(profile);
  validate_slots(options);
  const auto slots = options.use_business_hours ? normalized_slots(options)
                                                : std::vector<BusinessHourSlot>{};
  const auto& source_attribute = configured_attribute(
      options.source_timestamp_attribute,
      options.source_completion_timestamp_attribute);
  const auto& target_attribute = configured_attribute(
      options.target_timestamp_attribute,
      options.target_start_timestamp_attribute);

  std::vector<TemporalDeviation> deviations;
  for (const auto& trace : log.traces) {
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      const auto source_time =
          event_instant(trace.events[i], source_attribute, "");
      if (!source_time) continue;
      const auto source_local =
          options.use_business_hours
              ? event_local_instant(trace.events[i], source_attribute, "")
              : source_time;
      if (!source_local) continue;
      for (std::size_t j = i + 1; j < trace.events.size(); ++j) {
        const auto entry_it = profile_lookup.find(
            {trace.events[i].activity, trace.events[j].activity});
        if (entry_it == profile_lookup.end()) continue;
        const auto* entry = entry_it->second;
        const auto target_time =
            event_instant(trace.events[j], target_attribute, "");
        if (!target_time || *target_time < *source_time) continue;
        const auto target_local =
            options.use_business_hours
                ? event_local_instant(trace.events[j], target_attribute, "")
                : target_time;
        if (!target_local) continue;
        const double duration = elapsed_between(
            *source_time, *target_time, *source_local, *target_local, options,
            options.use_business_hours ? &slots : nullptr);
        const double difference = std::abs(duration - entry->mean_seconds);
        double distance = 0.0;
        if (entry->standard_deviation_seconds == 0.0) {
          if (difference == 0.0) continue;
          distance = std::numeric_limits<double>::infinity();
        } else {
          distance = difference / entry->standard_deviation_seconds;
          if (distance <= options.zeta) continue;
        }
        deviations.push_back({trace.case_id, trace.events[i].activity,
                              trace.events[j].activity, duration, distance});
      }
    }
  }
  return deviations;
}

}  // namespace minera
