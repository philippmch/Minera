#include "minera/timestamp.hpp"

#include "../zone_database.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace zd = minera::zone_database;

// Ubuntu 24.04 moved backward links such as America/Godthab out of the
// zoneinfo tree (tzdata-legacy) and keeps them only in tzdata.zi, which the
// standard backend reads and the portable one does not.
bool missing_backward_link(std::string_view zone) {
  if (zone != "America/Godthab" || !zd::portable_backend()) return false;
  try {
    (void)zd::info_at_utc(zone, 0);
    return false;
  } catch (const std::runtime_error&) {
    return true;
  }
}

// MSVC's database takes abbreviations from the system ICU in the user's
// locale ("GMT+2" or "MEZ" for CEST; declared, review AY1); offsets and
// transitions are IANA's everywhere.
bool abbreviation_is(const zd::SysInfo& info, std::string_view expected) {
#if defined(_MSC_VER)
  (void)expected;
  return !info.abbrev.empty();
#else
  return info.abbrev == expected;
#endif
}

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
  assert(utc_info.offset_seconds == 7200 && abbreviation_is(utc_info, "CEST"));
  assert(abbreviation_is(zd::info_at_utc("Europe/Berlin", kJanuary15), "CET"));
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

// zic writes explicit transitions only up to 2037; later years follow each
// file's POSIX rule. Expected values come from Python zoneinfo.
void recurring_rules_after_2037() {
  constexpr std::int64_t kJuly15_2038 = 2162808000;     // 12:00 UTC
  constexpr std::int64_t kJanuary15_2038 = 2147169600;  // 12:00 UTC
  auto at = zd::info_at_utc("Europe/Berlin", kJuly15_2038);
  assert(at.offset_seconds == 7200 && abbreviation_is(at, "CEST"));
  assert(zd::info_at_utc("America/New_York", kJuly15_2038).offset_seconds == -4 * 3600);
  assert(zd::info_at_utc("Australia/Sydney", kJuly15_2038).offset_seconds == 10 * 3600);
  assert(zd::info_at_utc("Australia/Sydney", kJanuary15_2038).offset_seconds == 11 * 3600);

  const auto summer = minera::localize_timestamp_wall_time(wall(kJuly15_2038), "Europe/Berlin");
  assert(summer && summer->offset_minutes == 120);
  // Northern gap/overlap (Berlin) and southern ones (Sydney), all wall times.
  constexpr std::int64_t kBerlinGap = 2153356200;      // 2038-03-28 02:30
  constexpr std::int64_t kBerlinOverlap = 2172105000;  // 2038-10-31 02:30
  constexpr std::int64_t kSydneyOverlap = 2153961000;  // 2038-04-04 02:30
  constexpr std::int64_t kSydneyGap = 2169685800;      // 2038-10-03 02:30
  auto local = zd::info_at_local("Europe/Berlin", kBerlinGap);
  assert(local.result == zd::LocalResult::kNonexistent &&
         local.first_offset_seconds == 3600 && local.second_offset_seconds == 7200);
  local = zd::info_at_local("Europe/Berlin", kBerlinOverlap);
  assert(local.result == zd::LocalResult::kAmbiguous &&
         local.first_offset_seconds == 7200 && local.second_offset_seconds == 3600);
  local = zd::info_at_local("Australia/Sydney", kSydneyOverlap);
  assert(local.result == zd::LocalResult::kAmbiguous &&
         local.first_offset_seconds == 39600 && local.second_offset_seconds == 36000);
  local = zd::info_at_local("Australia/Sydney", kSydneyGap);
  assert(local.result == zd::LocalResult::kNonexistent &&
         local.first_offset_seconds == 36000 && local.second_offset_seconds == 39600);
  assert(!minera::localize_timestamp_wall_time(wall(kBerlinGap), "Europe/Berlin"));
  assert(!minera::localize_timestamp_wall_time(wall(kBerlinOverlap), "Europe/Berlin"));
  assert(!minera::localize_timestamp_wall_time(wall(kSydneyGap), "Australia/Sydney"));

  struct Expected { std::string_view zone; std::int64_t utc; std::int64_t offset; };
  static constexpr Expected kExpected[] = {
#include "zone_database_future_offsets.inc"
  };
  for (const auto& expected : kExpected) {
    if (missing_backward_link(expected.zone)) continue;
    assert(zd::info_at_utc(expected.zone, expected.utc).offset_seconds == expected.offset);
    // The same instant read back as wall time is unique at that offset.
    const auto back = zd::info_at_local(expected.zone, expected.utc + expected.offset);
    assert(back.result == zd::LocalResult::kUnique && back.first_offset_seconds == expected.offset);
  }

  struct ExpectedLocal {
    std::string_view zone;
    std::int64_t wall;
    int result;
    std::int64_t first;
    std::int64_t second;
  };
  static constexpr ExpectedLocal kExpectedLocal[] = {
#include "zone_database_future_local.inc"
  };
  for (const auto& expected : kExpectedLocal) {
    if (missing_backward_link(expected.zone)) continue;
    const auto info = zd::info_at_local(expected.zone, expected.wall);
    assert(static_cast<int>(info.result) == expected.result);
    assert(info.first_offset_seconds == expected.first && info.second_offset_seconds == expected.second);
    const auto localized = minera::localize_timestamp_wall_time(wall(expected.wall), expected.zone);
    assert(localized.has_value() == (expected.result == 0));
    if (localized) assert(localized->offset_minutes * 60 == expected.first);
  }
}

// Greenland's footer rules switch at a negative hour (M3.5.0/-1), which the
// rule parser must accept rather than freeze the last offset.
void negative_transition_hours() {
  constexpr std::int64_t kJuly15_2038 = 2162808000;  // 12:00 UTC
  constexpr std::int64_t kGap = 2531950200;          // 2050-03-26 23:30
  constexpr std::int64_t kOverlap = 2550699000;      // 2050-10-29 23:30
  for (const std::string_view zone : {"America/Nuuk", "America/Godthab"}) {
    if (missing_backward_link(zone)) continue;
    assert(zd::info_at_utc(zone, kJuly15_2038).offset_seconds == -3600);
    auto local = zd::info_at_local(zone, kGap);
    assert(local.result == zd::LocalResult::kNonexistent &&
           local.first_offset_seconds == -7200 && local.second_offset_seconds == -3600);
    local = zd::info_at_local(zone, kOverlap);
    assert(local.result == zd::LocalResult::kAmbiguous &&
           local.first_offset_seconds == -3600 && local.second_offset_seconds == -7200);
    assert(!minera::localize_timestamp_wall_time(wall(kGap), zone));
    assert(!minera::localize_timestamp_wall_time(wall(kOverlap), zone));
  }
  assert(zd::info_at_utc("America/Scoresbysund", kJuly15_2038).offset_seconds == -3600);
}

// A unique wall time reports no second offset at all.
void unique_local_times_have_no_second_offset() {
  for (const std::int64_t wall_time : {kJanuary15, kJuly15, std::int64_t{2162808000}}) {
    const auto info = zd::info_at_local("Europe/Berlin", wall_time);
    assert(info.result == zd::LocalResult::kUnique && info.second_offset_seconds == 0);
  }
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
  recurring_rules_after_2037();
  negative_transition_hours();
  unique_local_times_have_no_second_offset();
  unknown_zones_are_rejected();
  return 0;
}
