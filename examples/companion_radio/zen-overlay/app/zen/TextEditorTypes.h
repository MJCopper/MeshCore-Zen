#pragma once

#include <Arduino.h>

namespace zen {

enum class EditorProfile : uint8_t {
  LITERAL,
  CREDENTIAL,
  MESSAGE,
  QUICK_REPLY,
  CONSOLE
};

enum class EditorAction : uint8_t {
  NONE,
  NAV_UP,
  NAV_DOWN,
  NAV_LEFT,
  NAV_RIGHT,
  ACTIVATE,
  HOLD_ACTIVATE,
  CANCEL,
  DOUBLE_CANCEL,
  BACKSPACE,
  SUBMIT,
  OPEN_SUGGESTIONS,
  OPEN_EMOJI
};

struct EditorFeatures {
  bool sentence_case;
  bool emoji;
  bool predictive_t9;
  bool context_prediction;
};

inline EditorFeatures featuresFor(EditorProfile profile) {
  switch (profile) {
    case EditorProfile::MESSAGE:
    case EditorProfile::QUICK_REPLY:
      return { true, true, true, true };
    case EditorProfile::CONSOLE:
      return { false, false, true, false };
    case EditorProfile::LITERAL:
    case EditorProfile::CREDENTIAL:
    default:
      return { false, false, false, false };
  }
}

} // namespace zen
