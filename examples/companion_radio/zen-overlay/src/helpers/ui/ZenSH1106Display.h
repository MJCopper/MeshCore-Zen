#pragma once

#include "ZenDisplayDriver.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#define SH110X_NO_SPLASH
#include <Adafruit_SH110X.h>

#ifndef PIN_OLED_RESET
#define PIN_OLED_RESET -1
#endif

#ifndef DISPLAY_ADDRESS
#define DISPLAY_ADDRESS 0x3C
#endif

class ZenSH1106Display : public ZenDisplayDriver
{
  Adafruit_SH1106G display;
  bool _isOn;
  uint8_t _color;
  uint8_t _contrast;
  uint8_t _precharge;
  int  _text_sz;
  // Frame-skip: endFrame() hashes the GFX buffer (FNV-1a, no external dep — the
  // CRC32 lib is only wired into e-ink builds) and skips the I²C flush when it's
  // byte-identical to the last one pushed. _force_redraw guarantees the first
  // frame and the frame after turnOn()/clear() always flush.
  uint32_t _last_frame_hash = 0;
  bool     _force_redraw = true;

  bool i2c_probe(TwoWire &wire, uint8_t addr);
  // Thin wrapper over the shared misc-fixed renderer (MiscFixedRenderer.h), kept
  // out of this header so the font tables land in one translation unit only.
  uint8_t glyphXAdvance(uint32_t cp);

public:
  ZenSH1106Display() : ZenDisplayDriver(128, 64), display(128, 64, &Wire, PIN_OLED_RESET) {
    _isOn = false; _contrast = 255; _precharge = 0x1F; _text_sz = 1;
  }
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char *str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t *bits, int w, int h) override;
  uint16_t getTextWidth(const char *str) override;
  uint16_t getCodepointWidth(uint32_t cp) override {
    return glyphXAdvance(cp);
  }
  int getCharWidth() const override { return 6 * _text_sz; }   // misc-fixed 6x9 is 6px wide
  int getLineHeight() const override { return 9 * _text_sz; }
  // Only the built-in classic font pads every measured string by one trailing
  // advance column (see ZenDisplayDriver::textWidthTrailingGap()); the lemon
  // font's width comes from its own glyph table (ink-tight, no padding).
  int textWidthTrailingGap() const override { return 0; }
  void setSingleFont(bool) override { }
  bool isSingleFont() const override { return true; }
  void setBrightness(uint8_t level) override;
  void endFrame() override;
  
};
