#pragma once

namespace zen {
// Twelve pixels includes one trailing padding pixel. Draw only active bars:
// a two-pixel outline has no interior and looks identical to a filled bar.
struct SignalIndicator {
  static constexpr int WIDTH = 12;
  template<class Display>
  static int draw(Display& display, unsigned bars, bool unknown = false) {
    const int x = display.width() - WIDTH;
    const int h = display.isSingleFont() ? display.getLineHeight() - 2
                                         : display.getLineHeight();
    if (unknown) {
      // Full text-row height fits a hollow right triangle and a separate 3x3 X.
      // It rises left-to-right, with a flat bottom and vertical right edge.
      const int th=display.getLineHeight();
      for (int i=0;i<WIDTH;++i)
        display.fillRect(x+i,(th-1)*(WIDTH-1-i)/(WIDTH-1),1,1);
      display.fillRect(x,th-1,WIDTH,1);
      display.fillRect(x+WIDTH-1,0,1,th);
      const int cy=2*(th-1)/3;
      for (int i=0;i<3;++i) {
        display.fillRect(x+7+i,cy-1+i,1,1);
        display.fillRect(x+9-i,cy-1+i,1,1);
      }
    } else if (!bars) {
      const int top = (h - 6) / 2;
      for (int i = 0; i < 6; ++i) {
        display.fillRect(x + 2 + i, top + i, 1, 1);
        display.fillRect(x + 7 - i, top + i, 1, 1);
      }
    } else {
      for (unsigned i = 0; i < bars && i < 4; ++i) {
        const int bar_h = (1 + i * 2) * h / 7;
        display.fillRect(x + i * 3, h - bar_h, 2, bar_h);
      }
    }
    return x;
  }
};
} // namespace zen
