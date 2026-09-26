#pragma once

#include <stdint.h>

namespace zen {

// Shared notification-sound catalogue. Selection values are persisted: keep
// every explicit ID stable and append future values with a schema migration.
class BuiltinMelodies {
public:
  enum Selection : uint8_t {
    MESSAGE = 0, KERPLOP = 1, CHIME = 2, RIPPLE = 3,
    BEACON = 4, CHEER = 5, ORBIT = 6, ALERT = 7,
    CUSTOM1 = 8, CUSTOM2 = 9, NONE = 10, COUNT = 11
  };

  static const char* label(uint8_t selection) {
    static const char* const LABELS[COUNT] = {
      "Message", "Kerplop", "Chime", "Ripple", "Beacon", "Cheer",
      "Orbit", "Alert", "Custom1", "Custom2", "None"
    };
    return LABELS[selection < COUNT ? selection : MESSAGE];
  }

  static const char* melody(uint8_t selection) {
    static const char* const MELODIES[8] = {
      "MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7",
      "kerplop:d=16,o=6,b=120:32g#,32c#",
      "Chime:d=16,o=6,b=140:8c,8e,4g",
      "Ripple:d=16,o=6,b=150:c,e,g,e",
      "Beacon:d=8,o=5,b=120:g,c6,g6",
      "Cheer:d=16,o=6,b=190:32c,32e,32g,16c7,16e7,8g7",
      "Orbit:d=8,o=5,b=95:8c#,16g#,8d#6,16g#,4c#6",
      "Alert:d=8,o=6,b=220:16c7,4f#"
    };
    return selection < CUSTOM1 ? MELODIES[selection] : nullptr;
  }

  static uint8_t validate(uint8_t selection, uint8_t fallback = MESSAGE) {
    return selection < COUNT ? selection : fallback;
  }

  // Override value 0 follows the global selection; other stored values are
  // the catalogue selection plus one.
  static uint8_t resolveOverride(uint8_t override_value, uint8_t global_selection) {
    return override_value ? validate(override_value - 1) : validate(global_selection);
  }

  // Values written before schema 0x2A were Built-in, Melody1, Melody2, None.
  static uint8_t migrateLegacyGlobal(uint8_t value, bool channel) {
    static const uint8_t MAP[4] = { MESSAGE, CUSTOM1, CUSTOM2, NONE };
    if (value == 0 && channel) return KERPLOP;
    return value < 4 ? MAP[value] : (channel ? KERPLOP : MESSAGE);
  }

  // Override value 0 means Global; non-zero values store selection + 1.
  static uint8_t migrateLegacyOverride(uint8_t value) {
    return value == 1 ? CUSTOM1 + 1 : value == 2 ? CUSTOM2 + 1 : 0;
  }
};

} // namespace zen
