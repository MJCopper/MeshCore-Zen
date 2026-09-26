#pragma once

#include "../ZenPrefs.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include "DigitEditor.h"

// Small policy helper kept independent of the screens so upstream UI changes
// only need to call these predicates. This is intentionally a practical UI
// lock; physical flash access remains a recovery/bypass path.
namespace childmode {

// PIN entry uses the normal screen colours for unselected digits and inverts
// only the active digit (white background, black glyph). DigitEditor itself is
// left unchanged because its usual callers render inside highlighted rows.
static inline void renderPinEditor(ZenDisplayDriver& display, DigitEditor& editor, int x, int y) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%06lu", (unsigned long)editor.value);
  const int cw = display.getCharWidth();
  for (int i = 0; i < 6; i++) {
    int cx = x + i * cw;
    if (i == editor.cursor) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.fillRect(cx, y - 1, cw, display.getLineHeight() + 1);
      display.setColor(ZenDisplayDriver::DARK);
    } else {
      display.setColor(ZenDisplayDriver::LIGHT);
    }
    char digit[2] = { buf[i], '\0' };
    display.setCursor(cx, y);
    display.print(digit);
  }
  display.setColor(ZenDisplayDriver::LIGHT);
}

static inline uint32_t pinHash(uint32_t pin) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 6; i++) {
    h ^= (uint8_t)(pin % 10);
    h *= 16777619u;
    pin /= 10;
  }
  return h ^ 0x4348494Cu;  // "CHIL", avoids the unset value being a useful PIN
}

} // namespace childmode
