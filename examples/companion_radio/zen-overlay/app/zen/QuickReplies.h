#pragma once

#include <stdint.h>

namespace zen {

// Immutable replies live in firmware; only the five custom slots consume the
// existing preferences storage. Keeping the catalogue here makes the picker
// independent of SettingsScreen and straightforward to extend later.
class QuickReplies {
public:
  static constexpr uint8_t BUILTIN_COUNT = 10;
  static constexpr uint8_t CUSTOM_COUNT = 5;
  static constexpr uint8_t MAX_VISIBLE_COUNT = BUILTIN_COUNT + CUSTOM_COUNT;

  static const char* builtin(uint8_t index) {
    static const char* const REPLIES[BUILTIN_COUNT] = {
      "Yes",
      "No",
      "Okay",
      "Thanks",
      "Sounds good",
      "Not right now",
      "I can't",
      "On my way",
      "Please wait",
      "Maybe"
    };
    return index < BUILTIN_COUNT ? REPLIES[index] : "";
  }
};

} // namespace zen
