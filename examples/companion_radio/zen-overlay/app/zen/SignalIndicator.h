#pragma once

namespace zen {
// Twelve pixels includes one trailing padding pixel. Draw only active bars:
// a two-pixel outline has no interior and looks identical to a filled bar.
struct SignalIndicator {
  static constexpr int WIDTH = 12;
  template<class Display>
  static int draw(Display& display, unsigned bars) {
    const int x = display.width() - WIDTH;
    const int h = display.isSingleFont() ? display.getLineHeight() - 2
                                         : display.getLineHeight();
    if (!bars) {
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
