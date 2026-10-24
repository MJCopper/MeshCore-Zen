#pragma once

#include <stdio.h>
#include "BatteryPolicy.h"
#include "BatteryChargeState.h"

namespace zen {
struct BatteryIndicator {
  static bool boltVisible(uint32_t now) { return (now / 1000u) % 2u == 0; }
  static unsigned refreshDelay(BatteryChargeState::State state, uint32_t now) {
    return state == BatteryChargeState::CHARGING ? 1000u - now % 1000u : 0;
  }
  static const uint8_t* symbolRows(BatteryChargeState::State state) {
    static const uint8_t bolt[] = {4,12,31,2,4};
    static const uint8_t tick[] = {0,1,2,20,8};
    static const uint8_t plug[] = {10,31,14,4,4};
    return state == BatteryChargeState::COMPLETE ? tick
        : state == BatteryChargeState::CHARGING ? bolt : plug;
  }
  template<class Display>
  static void glyph(Display& display, int x, int y, int scale,
                    BatteryChargeState::State state) {
    if (state == BatteryChargeState::BATTERY) return;
    const uint8_t* rows=symbolRows(state);
    for (int row=0;row<5;++row)
      for (int col=0;col<5;++col)
        if (rows[row] & (1 << (4-col)))
          display.fillRect(x+col*scale,y+row*scale,scale,scale);
  }
  template<class Display>
  static void outlinedSymbol(Display& display, int x, int y, int scale,
                             BatteryChargeState::State state) {
    const uint8_t* rows=symbolRows(state);
    display.setColor(Display::DARK);
    for (int row=0;row<5;++row)
      for (int col=0;col<5;++col)
        if (rows[row] & (1 << (4-col)))
          display.fillRect(x+col*scale-1,y+row*scale-1,scale+2,scale+2);
    display.setColor(Display::LIGHT);
    glyph(display,x,y,scale,state);
  }

  // Returns the left edge including any numeric-mode glyph reservation.
  template<class Display>
  static int draw(Display& display, uint16_t mv, uint8_t mode, int right_x,
                  BatteryChargeState::State state, bool bolt_on = true) {
    display.setColor(Display::LIGHT);
    const int lh=display.getLineHeight();
    const int h=display.isSingleFont() ? lh-2 : lh;
    const int scale=h>=12 ? 2 : 1;
    const int glyph_w=5*scale;
    if (mode == 1 || mode == 2) {
      char buf[12];
      if (mode == 1) snprintf(buf,sizeof(buf),"%d%%",BatteryPolicy::percent(mv));
      else snprintf(buf,sizeof(buf),"%u.%02uV",mv/1000,(mv%1000)/10);
      const int text_x=right_x-display.getTextWidth(buf);
      display.setCursor(text_x,0);
      display.print(buf);
      if (state == BatteryChargeState::BATTERY) return text_x;
      const int left=text_x-glyph_w-2;
      if (state != BatteryChargeState::CHARGING || bolt_on)
        glyph(display,left,(h-glyph_w)/2,scale,state);
      return left;
    }

    // Original shell height and level-bar margins, shared by every charge state.
    // Align with the other top-bar icons; overlays may mask the shell without enlarging it.
    const int top=0;
    const int w=lh*2-4;
    const int left=right_x-w-2;
    display.drawRect(left,top,w,h);
    display.fillRect(left+w,top+(h-h/2)/2,2,h/2);
    if (state != BatteryChargeState::EXTERNAL_UNKNOWN) {
      const int margin=display.isLandscape() ? 3 : 2;
      const int pct=state == BatteryChargeState::COMPLETE ? 100 : BatteryPolicy::percent(mv);
      display.fillRect(left+margin,top+margin,pct*(w-2*margin)/100,h-2*margin);
    }
    const int gx=left+(w-glyph_w+1)/2;
    const int gy=top+(h-glyph_w)/2;
    if ((state == BatteryChargeState::CHARGING && bolt_on) || state == BatteryChargeState::COMPLETE)
      outlinedSymbol(display,gx,gy,scale,state);
    else if (state == BatteryChargeState::EXTERNAL_UNKNOWN)
      glyph(display,gx,gy,scale,state);
    return left;
  }
};
} // namespace zen
