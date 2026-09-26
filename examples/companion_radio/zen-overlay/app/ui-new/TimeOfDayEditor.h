#pragma once

#include <helpers/ui/ZenDisplayDriver.h>
#include <helpers/ui/ZenUIScreen.h>

// Compact HH:MM digit editor shared by settings that store a minute-of-day.
// LEFT/RIGHT selects a digit, UP/DOWN changes it, Enter accepts and Cancel
// restores the caller-owned value.
struct TimeOfDayEditor {
  enum Result { NONE, DONE, CANCELLED };

  uint16_t value = 0;
  int8_t cursor = 0;
  bool editing = false;

  void begin(uint16_t minute_of_day) {
    value = minute_of_day < 1440 ? minute_of_day : 0;
    cursor = 0;
    editing = true;
  }

  bool active() const { return editing; }

  Result handleInput(char c) {
    if (!editing) return CANCELLED;
    if (keyIsPrev(c)) { if (cursor > 0) cursor--; return NONE; }
    if (keyIsNext(c)) { if (cursor < 3) cursor++; return NONE; }
    if (c == KEY_ENTER)  { editing = false; return DONE; }
    if (c == KEY_CANCEL) { editing = false; return CANCELLED; }
    if (c != KEY_UP && c != KEY_DOWN) return NONE;

    uint8_t hour = value / 60;
    uint8_t minute = value % 60;
    uint8_t digits[4] = {
      (uint8_t)(hour / 10), (uint8_t)(hour % 10),
      (uint8_t)(minute / 10), (uint8_t)(minute % 10)
    };
    int dir = c == KEY_UP ? 1 : -1;
    uint8_t max_digit = cursor == 0 ? 2 :
                        (cursor == 1 && digits[0] == 2 ? 3 :
                         (cursor == 2 ? 5 : 9));
    digits[cursor] = (digits[cursor] + max_digit + 1 + dir) % (max_digit + 1);
    if (digits[0] == 2 && digits[1] > 3) digits[1] = 3;
    hour = digits[0] * 10 + digits[1];
    minute = digits[2] * 10 + digits[3];
    value = hour * 60 + minute;
    return NONE;
  }

  void render(ZenDisplayDriver& display, int x, int y) const {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(value / 60), (unsigned)(value % 60));
    int char_cursor = cursor < 2 ? cursor : cursor + 1;  // skip ':'
    int cw = display.getCharWidth();
    for (int i = 0; buf[i]; i++) {
      int cx = x + i * cw;
      if (i == char_cursor) {
        display.setColor(ZenDisplayDriver::DARK);
        display.fillRect(cx, y - 1, cw, display.getLineHeight() + 1);
        display.setColor(ZenDisplayDriver::LIGHT);
      } else {
        display.setColor(ZenDisplayDriver::DARK);
      }
      char glyph[2] = { buf[i], '\0' };
      display.setCursor(cx, y);
      display.print(glyph);
    }
    display.setColor(ZenDisplayDriver::LIGHT);
  }
};
