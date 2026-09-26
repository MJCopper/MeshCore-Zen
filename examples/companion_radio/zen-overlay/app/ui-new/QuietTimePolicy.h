#pragma once

#include <stdint.h>

namespace quiettime {

static const uint16_t MINUTES_PER_DAY = 24 * 60;

static inline bool intervalActive(uint16_t now, uint16_t start, uint16_t end) {
  if (now >= MINUTES_PER_DAY || start >= MINUTES_PER_DAY ||
      end >= MINUTES_PER_DAY || start == end)
    return false;
  if (start < end) return now >= start && now < end;
  return now >= start || now < end;
}

} // namespace quiettime
