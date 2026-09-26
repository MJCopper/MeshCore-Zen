#pragma once

#include <helpers/ui/ZenUIScreen.h>
#include "../zen/TextEditorTypes.h"

namespace keyboardinput {

inline zen::EditorAction actionForKey(char key) {
  uint8_t value = (uint8_t)key;
  if (value == KEY_UP) return zen::EditorAction::NAV_UP;
  if (value == KEY_DOWN) return zen::EditorAction::NAV_DOWN;
  if (value == KEY_LEFT) return zen::EditorAction::NAV_LEFT;
  if (value == KEY_RIGHT) return zen::EditorAction::NAV_RIGHT;
  if (value == KEY_ENTER) return zen::EditorAction::ACTIVATE;
  if (value == KEY_CONTEXT_MENU) return zen::EditorAction::HOLD_ACTIVATE;
  if (value == KEY_CANCEL) return zen::EditorAction::CANCEL;
  if (value == KEY_DOUBLE_CANCEL) return zen::EditorAction::DOUBLE_CANCEL;
  if (value == 0x08) return zen::EditorAction::BACKSPACE;
  if (value == KEY_KB_ENTER) return zen::EditorAction::SUBMIT;
  return zen::EditorAction::NONE;
}

} // namespace keyboardinput
