#pragma once

#include <SPI.h>
#include <Wire.h>

#define ENABLE_GxEPD2_GFX 0

// Always use the patched copy of GxEPD2_BW.h from lib/GxEPD2-patch/src/: it
// adds writeInverseForRedrive() (needed by every e-ink build, see endFrame()).
// It needs the class's private framebuffer, so it has to live in the header. The
// include guard (_GxEPD2_BW_H_) prevents the installed library version from
// being compiled as well; the copy tracks the 1.6.2 pin every e-ink variant's
// lib_deps uses.
#include "../../../lib/GxEPD2-patch/src/GxEPD2_BW.h"
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>

#include "ZenDisplayDriver.h"
#include "MiscFixedFont.h"
#include "EmojiGlyphs.h"

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 0
#endif

// Panel native dimensions before rotation. Derived from the model class when
// EINK_DISPLAY_MODEL is set; override with EINK_PANEL_W/H for other panels.
#if defined(EINK_DISPLAY_MODEL)
  #ifndef EINK_PANEL_W
    #define EINK_PANEL_W EINK_DISPLAY_MODEL::WIDTH
  #endif
  #ifndef EINK_PANEL_H
    #define EINK_PANEL_H EINK_DISPLAY_MODEL::HEIGHT
  #endif
#else
  #ifndef EINK_PANEL_W
    #define EINK_PANEL_W 200
    #define EINK_PANEL_H 200
  #endif
#endif

// Odd rotations (1, 3) swap width and height.
#define EINK_DISP_W ((DISPLAY_ROTATION & 1) ? EINK_PANEL_H : EINK_PANEL_W)
#define EINK_DISP_H ((DISPLAY_ROTATION & 1) ? EINK_PANEL_W : EINK_PANEL_H)

class ZenGxEPDDisplay : public ZenDisplayDriver {
#if defined(EINK_DISPLAY_MODEL)
  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
#else
  GxEPD2_BW<GxEPD2_150_BN, 200> display;
#endif
  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  int last_display_crc_value = 0;
  int _text_sz = 1;
  uint8_t _full_refresh_interval = 0;
  uint8_t _partial_count = 0;

  uint8_t glyphXAdvance(uint32_t cp, int sc);
  int scale() const { return (width() >= height()) ? 2 : 1; }

public:
#if defined(EINK_DISPLAY_MODEL)
  ZenGxEPDDisplay() : ZenDisplayDriver(EINK_DISP_W, EINK_DISP_H),
    display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}
#else
  ZenGxEPDDisplay() : ZenDisplayDriver(EINK_DISP_W, EINK_DISP_H),
    display(GxEPD2_150_BN(DISP_CS, DISP_DC, DISP_RST, DISP_BUSY)) {}
#endif

  // Line height and approx. char width for each font size:
  //   1 = FreeSans9pt      (lineH=16, charW≈9)
  //   2 = FreeSansBold12pt (lineH=20, charW≈12)
  //   3 = FreeSans18pt     (lineH=28, charW≈17)
  // Built-in font scale used for size 4 (huge clock digits): 6×8 cell × 7 = 42×56.
  static const int BIG_TEXT_SCALE = 7;
  // Size 1 scales with orientation (portrait 1×, landscape 2×); size 2 is always 12×16;
  // size 3 is FreeSans18pt (~17×28); size 4 is the built-in font at 7× (~42×56),
  // used only for the stacked HH/MM clock on portrait e-ink. Landscape = width >= height.
  int getCharWidth() const override {
    int sc = scale();
    if (_text_sz == 4) return 6 * BIG_TEXT_SCALE;
    if (_text_sz == 3) return 17;
    if (_text_sz == 2) return 12 * sc;
    return 6 * sc;   // misc-fixed 6x9 is 6px wide
  }
  int getLineHeight() const override {
    int sc = scale();
    if (_text_sz == 4) return 8 * BIG_TEXT_SCALE;
    if (_text_sz == 3) return 28;
    if (_text_sz == 2) return 16 * sc;
    return (_text_sz == 1 ? 9 : 8) * sc;
  }
  void setSingleFont(bool) override { }   // single-font: ignore toggles, stay misc-fixed 6x9
  bool isSingleFont() const override { return true; }
  bool begin();

  bool isOn() override { return _isOn; }
  bool isEink() override { return true; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  uint16_t getCodepointWidth(uint32_t cp) override {
    if (_text_sz == 1) return glyphXAdvance(cp, scale());
    return ZenDisplayDriver::getCodepointWidth(cp);
  }
  void setDisplayRotation(uint8_t rot) override;
  void setFullRefreshInterval(uint8_t n) override { _full_refresh_interval = n; _partial_count = 0; }
  void endFrame() override;
};
