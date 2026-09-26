#pragma once
#include "../zen/LocalTimeService.h"

#include "../ZenPrefs.h"
#include "QuietTimePolicy.h"
#include <stdint.h>

// Quiet Time is notification-presentation policy only. It does not affect
// message reception, storage, unread counts, routing or acknowledgements.
namespace quiettime {

static const uint32_t MIN_VALID_UNIX_TIME = 1000000000UL;

static inline bool active(const ZenPrefs* prefs, uint32_t utc_time, bool time_synced) {
  // A restored shutdown timestamp is plausible, but not a live clock sync.
  if (!time_synced || !prefs || !prefs->quiet_time_enabled || utc_time < MIN_VALID_UNIX_TIME)
    return false;

  uint16_t start = prefs->quiet_time_start_min;
  uint16_t end = prefs->quiet_time_end_min;
  zen::LocalTimeService local_time;
  local_time.bind(prefs);
  uint16_t now = local_time.minuteOfDay(utc_time);
  return intervalActive(now, start, end);
}

static inline void formatTime(char* out, size_t out_size, uint16_t minute_of_day) {
  minute_of_day %= MINUTES_PER_DAY;
  snprintf(out, out_size, "%02u:%02u",
           (unsigned)(minute_of_day / 60), (unsigned)(minute_of_day % 60));
}

} // namespace quiettime
