#include "minera/timestamp.hpp"

#include "../zone_database.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

namespace zd = minera::zone_database;

// 2021-03-28 and 2021-10-31 are Europe/Berlin's DST transitions.
constexpr std::int64_t kMarch28 = 1616889600;    // 2021-03-28T00:00:00 (wall)
constexpr std::int64_t kOctober31 = 1635638400;  // 2021-10-31T00:00:00 (wall)
constexpr std::int64_t kJanuary15 = 1610668800;  // 2021-01-15T00:00:00
constexpr std::int64_t kJuly15 = 1626307200;     // 2021-07-15T00:00:00

minera::ParsedExactTimestamp wall(std::int64_t seconds) {
  minera::ParsedExactTimestamp value;
  value.seconds = seconds;
  return value;
}

void utc_and_fixed_offsets() {
  const auto utc = minera::localize_timestamp_wall_time(wall(kJanuary15), "UTC");
  assert(utc && utc->timezone_aware && utc->seconds == kJanuary15 && utc->offset_minutes == 0);
  const auto plus = minera::localize_timestamp_wall_time(wall(kJanuary15), "UTC+05:30");
  assert(plus && plus->offset_minutes == 330 && plus->seconds == kJanuary15 - 330 * 60);
  const auto minus = minera::localize_timestamp_wall_time(wall(kJanuary15), "UTC-03:00");
  assert(minus && minus->offset_minutes == -180 && minus->seconds == kJanuary15 + 180 * 60);
}

void iana_zones() {
  const auto winter = minera::localize_timestamp_wall_time(wall(kJanuary15 + 12 * 3600), "Europe/Berlin");
  assert(winter && winter->offset_minutes == 60 && winter->seconds == kJanuary15 + 11 * 3600);
  const auto summer = minera::localize_timestamp_wall_time(wall(kJuly15 + 12 * 3600), "Europe/Berlin");
  assert(summer && summer->offset_minutes == 120 && summer->seconds == kJuly15 + 10 * 3600);
  const auto new_york = minera::localize_timestamp_wall_time(wall(kJuly15), "America/New_York");
  assert(new_york && new_york->offset_minutes == -240);

  const auto utc_info = zd::info_at_utc("Europe/Berlin", kJuly15);
  assert(utc_info.offset_seconds == 7200 && utc_info.abbrev == "CEST");
  assert(zd::info_at_utc("Europe/Berlin", kJanuary15).abbrev == "CET");
}

void dst_gap_and_overlap() {
  // 02:30 does not exist on the spring-forward day.
  const auto gap = zd::info_at_local("Europe/Berlin", kMarch28 + 2 * 3600 + 1800);
  assert(gap.result == zd::LocalResult::kNonexistent);
  assert(gap.first_offset_seconds == 3600 && gap.second_offset_seconds == 7200);
  assert(!minera::localize_timestamp_wall_time(wall(kMarch28 + 2 * 3600 + 1800), "Europe/Berlin"));
  // 02:30 happens twice on the fall-back day.
  const auto overlap = zd::info_at_local("Europe/Berlin", kOctober31 + 2 * 3600 + 1800);
  assert(overlap.result == zd::LocalResult::kAmbiguous);
  assert(overlap.first_offset_seconds == 7200 && overlap.second_offset_seconds == 3600);
  assert(!minera::localize_timestamp_wall_time(wall(kOctober31 + 2 * 3600 + 1800), "Europe/Berlin"));
  // Either side of the transitions stays unique.
  assert(zd::info_at_local("Europe/Berlin", kMarch28 + 3 * 3600 + 1800).result == zd::LocalResult::kUnique);
  assert(zd::info_at_local("Europe/Berlin", kOctober31 + 3 * 3600 + 1800).result == zd::LocalResult::kUnique);
}

void unknown_zones_are_rejected() {
  bool threw = false;
  try { (void)zd::info_at_utc("Not/AZone", 0); } catch (const std::runtime_error&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::localize_timestamp_wall_time(wall(0), "Not/AZone"); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

}  // namespace

int main() {
  utc_and_fixed_offsets();
  iana_zones();
  dst_gap_and_overlap();
  unknown_zones_are_rejected();
  return 0;
}
