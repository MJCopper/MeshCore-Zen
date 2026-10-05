#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// Destination-level branching: even levels split, odd levels mature in place.
// IDs are grouped by level. All progression data stays in firmware, not RAM.
struct Evolution {
  static constexpr uint8_t LEVELS = 12;
  static constexpr uint8_t FORMS = 189;
  static uint8_t offset(uint8_t level) {
    static const uint8_t OFFSETS[] = {0,1,3,5,9,13,21,29,45,61,93,125};
    return OFFSETS[level >= 1 && level <= LEVELS ? level - 1 : 0];
  }
  static uint8_t count(uint8_t level) { return 1u << (level / 2); }
  static uint8_t level(uint8_t form) {
    for (uint8_t level = LEVELS; level > 1; --level)
      if (form >= offset(level)) return level;
    return 1;
  }
  static uint8_t choices(uint8_t level) {
    return level >= LEVELS ? 0 : (level & 1) ? 2 : 1;
  }
  static uint8_t child(uint8_t form, uint8_t choice) {
    uint8_t current = level(form), paths = choices(current);
    if (!paths || choice >= paths) return form;
    return offset(current + 1) + (form - offset(current)) * paths + choice;
  }
  static uint16_t xp(uint8_t level) {
    // Calibrated by the host care simulation: ~60 days, eight hours sleep,
    // no mesh bonuses, and training that leaves at least 25 fullness.
    static const uint16_t XP[] = {160,280,640,1190,2210,4170,7530,12570,
                                  19570,28810,41050,41050};
    return XP[level - 1];
  }
  static uint8_t bond(uint8_t level) {
    static const uint8_t BOND[] = {20,30,40,50,60,70,80,90,95,98,100,100};
    return BOND[level - 1];
  }
};

} } // namespace zen::pet
