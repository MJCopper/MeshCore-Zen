#pragma once
#include "../solo/TimezonePolicy.h"

#include "../NodePrefs.h"
#include "QuietTimePolicy.h"
#include <stdint.h>

// Quiet Time is notification-presentation policy only. It does not affect
// message reception, storage, unread counts, routing or acknowledgements.
namespace quiettime {

static const uint32_t MIN_VALID_UNIX_TIME = 1000000000UL;

static inline uint16_t localMinuteOfDay(uint32_t utc_time, int8_t tz_offset_hours) {
  int64_t local = (int64_t)utc_time + (int64_t)tz_offset_hours * 3600;
  int32_t seconds = (int32_t)(local % 86400);
  if (seconds < 0) seconds += 86400;
  return (uint16_t)(seconds / 60);
}

static inline uint16_t localMinuteOfDayMinutes(uint32_t utc_time, int16_t offset_minutes) {
  int64_t local = (int64_t)utc_time + (int64_t)offset_minutes * 60;
  int32_t seconds = (int32_t)(local % 86400);
  if (seconds < 0) seconds += 86400;
  return (uint16_t)(seconds / 60);
}

static inline bool active(const NodePrefs* prefs, uint32_t utc_time, bool time_synced) {
  // A restored shutdown timestamp is plausible, but not a live clock sync.
  if (!time_synced || !prefs || !prefs->quiet_time_enabled || utc_time < MIN_VALID_UNIX_TIME)
    return false;

  uint16_t start = prefs->quiet_time_start_min;
  uint16_t end = prefs->quiet_time_end_min;
  int16_t offset = solo::TimezonePolicy::offsetMinutes(
      prefs->timezone_mode, prefs->timezone_manual_min, prefs->timezone_city, utc_time);
  uint16_t now = localMinuteOfDayMinutes(utc_time, offset);
  return intervalActive(now, start, end);
}

static inline void formatTime(char* out, size_t out_size, uint16_t minute_of_day) {
  minute_of_day %= MINUTES_PER_DAY;
  snprintf(out, out_size, "%02u:%02u",
           (unsigned)(minute_of_day / 60), (unsigned)(minute_of_day % 60));
}

} // namespace quiettime
