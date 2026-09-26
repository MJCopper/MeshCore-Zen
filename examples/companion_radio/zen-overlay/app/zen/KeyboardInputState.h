#pragma once

#include <Arduino.h>

namespace zen {

// Mutable state for the on-screen grid and classic multi-tap input source.
// Behaviour lives in KeyboardInputController; the shared widget facade exposes
// the state to that controller without duplicating it for each editor.
struct KeyboardInputState {
  bool cursor_mode = false;
  int row = 0;
  int col = 0;
  int page = 0;
  bool caps = false;
  bool caps_lock = false;
  int t9_cell = -1;
  int t9_cycle = 0;
  uint32_t t9_last_ms = 0;
  bool t9_caps = false;
};

} // namespace zen
