#pragma once

#include <stdint.h>
#include "../ZenPrefs.h"
#include "TimezonePolicy.h"

namespace zen {

// Read-only local-time facade. UTC remains authoritative; every consumer uses
// the same preference validation, DST rule and day-wrapping implementation.
class LocalTimeService {
  const ZenPrefs* _prefs = nullptr;

public:
  void bind(const ZenPrefs* prefs) { _prefs = prefs; }

  int16_t offsetMinutes(uint32_t utc) const {
    return _prefs ? TimezonePolicy::offsetMinutes(
        _prefs->timezone_mode, _prefs->timezone_manual_min,
        _prefs->timezone_city, utc) : 0;
  }

  int64_t localSeconds(uint32_t utc) const {
    return (int64_t)utc + (int64_t)offsetMinutes(utc) * 60;
  }

  uint16_t minuteOfDay(uint32_t utc) const {
    int64_t seconds = localSeconds(utc) % 86400;
    if (seconds < 0) seconds += 86400;
    return (uint16_t)(seconds / 60);
  }

  bool daylightTime(uint32_t utc) const {
    if (!_prefs || _prefs->timezone_mode != TimezonePolicy::CITY ||
        _prefs->timezone_city >= TimezonePolicy::CITY_COUNT) return false;
    return TimezonePolicy::inDst(
        TimezonePolicy::cities()[_prefs->timezone_city], utc);
  }

  const char* cityName() const {
    return TimezonePolicy::cityName(
        _prefs ? _prefs->timezone_city : TimezonePolicy::DEFAULT_CITY);
  }
};

} // namespace zen
