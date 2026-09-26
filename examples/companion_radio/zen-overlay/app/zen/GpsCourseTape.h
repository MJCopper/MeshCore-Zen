#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace zen {

// Display model for a scrolling 32-point course tape. The eight named compass
// points occupy every fourth slot; the intervening dots are meaningful 11.25°
// increments rather than decoration.
class GpsCourseTape {
public:
  static constexpr uint8_t COUNT = 32;
  static constexpr long STEP_MILLIDEG = 11250L;
  static constexpr long HALF_STEP_MILLIDEG = STEP_MILLIDEG / 2;
  static constexpr long HYSTERESIS_MILLIDEG = 2000L;

  static uint8_t index(long course_millideg) {
    long course = course_millideg % 360000L;
    if (course < 0) course += 360000L;
    return (uint8_t)(((course + HALF_STEP_MILLIDEG) / STEP_MILLIDEG) % COUNT);
  }

  static uint8_t offset(uint8_t index, int8_t steps) {
    int value = (int)index + steps;
    while (value < 0) value += COUNT;
    return (uint8_t)(value % COUNT);
  }

  static const char* label(uint8_t index) {
    static const char* const DIRECTIONS[8] = {
      "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    };
    index %= COUNT;
    return (index % 4) == 0 ? DIRECTIONS[index / 4] : ".";
  }

  static uint8_t stabilise(int8_t current, long course_millideg) {
    uint8_t proposed = index(course_millideg);
    if (current < 0 || proposed == (uint8_t)current) return proposed;

    long centre = (long)current * STEP_MILLIDEG;
    long delta = course_millideg - centre;
    while (delta > 180000L) delta -= 360000L;
    while (delta < -180000L) delta += 360000L;
    return labs(delta) >= HALF_STEP_MILLIDEG + HYSTERESIS_MILLIDEG
        ? proposed : (uint8_t)current;
  }

  static long course(uint8_t index) {
    return (long)(index % COUNT) * STEP_MILLIDEG;
  }
};

} // namespace zen
