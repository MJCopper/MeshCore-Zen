#pragma once

#include <stdio.h>
#include "BatteryPolicy.h"
#include "BatteryChargeState.h"

namespace zen {
struct BatteryIndicator {
  template<class Display>
  static void glyph(Display& display, int x, int y, int scale,
                    BatteryChargeState::State state) {
    // Five by five glyphs fit inside the OLED battery's seven-pixel shell.
    static const uint8_t bolt[] = {6, 12, 31, 6, 12};
    static const uint8_t tick[] = {0, 1, 2, 20, 8};
    static const uint8_t plug[] = {10, 31, 14, 4, 4};
    if (state == BatteryChargeState::BATTERY) return;
    const uint8_t* rows = state == BatteryChargeState::COMPLETE ? tick
        : state == BatteryChargeState::CHARGING ? bolt : plug;
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 5; ++col)
        if (rows[row] & (1 << (4-col)))
          display.fillRect(x + col*scale, y + row*scale, scale, scale);
  }

  // Returns the left edge including any numeric-mode glyph reservation.
  template<class Display>
  static int draw(Display& display, uint16_t mv, uint8_t mode, int right_x,
                  BatteryChargeState::State state) {
    const int lh = display.getLineHeight();
    const int h = display.isSingleFont() ? lh - 2 : lh;
    const int scale = h >= 12 ? 2 : 1;
    const int glyph_w = 5 * scale;
    if (mode == 1 || mode == 2) {
      char buf[12];
      if (mode == 1) snprintf(buf, sizeof(buf), "%d%%", BatteryPolicy::percent(mv));
      else snprintf(buf, sizeof(buf), "%u.%02uV", mv / 1000, (mv % 1000) / 10);
      const int text_x = right_x - display.getTextWidth(buf);
      display.setCursor(text_x, 0);
      display.print(buf);
      if (state == BatteryChargeState::BATTERY) return text_x;
      const int left = text_x - glyph_w - 2;
      glyph(display, left, (h-glyph_w)/2, scale, state);
      return left;
    }
    const int w = lh * 2 - 4;
    const int left = right_x - w - 2;
    display.drawRect(left, 0, w, h);
    display.fillRect(left + w, (h-h/2)/2, 2, h/2);
    if (state == BatteryChargeState::BATTERY) {
      const int margin = display.isLandscape() ? 3 : 2;
      const int fill_w = BatteryPolicy::percent(mv) * (w - 2*margin) / 100;
      display.fillRect(left + margin, margin, fill_w, h - 2*margin);
    } else {
      glyph(display, left + (w-glyph_w)/2, (h-glyph_w)/2, scale, state);
    }
    return left;
  }
};
} // namespace zen
