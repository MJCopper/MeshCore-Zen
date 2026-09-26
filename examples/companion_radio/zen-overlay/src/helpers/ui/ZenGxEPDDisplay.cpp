#include "ZenGxEPDDisplay.h"
#include "LemonIcons.h"
#include "MiscFixedRenderer.h"

// This driver retains the monochrome UI's semantic colour mapping: DARK means
// white paper/background and LIGHT means black ink/foreground (see setColor).
ColorVal UIColor::window_bkg = ZenDisplayDriver::DARK;
ColorVal UIColor::title_bkg = ZenDisplayDriver::DARK;
ColorVal UIColor::title_txt = ZenDisplayDriver::LIGHT;
ColorVal UIColor::primary_txt = ZenDisplayDriver::LIGHT;
ColorVal UIColor::secondary_txt = ZenDisplayDriver::LIGHT;
ColorVal UIColor::warning_txt = ZenDisplayDriver::LIGHT;
ColorVal UIColor::popup_bkg = ZenDisplayDriver::DARK;
ColorVal UIColor::popup_txt = ZenDisplayDriver::LIGHT;
ColorVal UIColor::corp_blue = ZenDisplayDriver::LIGHT;

#ifdef EXP_PIN_BACKLIGHT
  #include <PCA9557.h>
  extern PCA9557 expander;
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 0
#endif

#ifdef ESP32
  SPIClass SPI1 = SPIClass(FSPI);
#endif

// GFX fonts use the baseline as the cursor origin. UI code assumes top-of-cell
// coordinates (same convention as the OLED driver). Add the font ascender so
// the two conventions match.
static int fontAscender(int sz, int scale) {
  if (sz == 3) return 26;                       // FreeSans18pt7b: proportional, baseline origin
  if (sz == 1) return 7 * scale;                // misc-fixed 6x9 GFX font
  return 0;                                     // GFX built-in font: cursor is top-left of cell
}

uint8_t ZenGxEPDDisplay::glyphXAdvance(uint32_t cp, int sc) {
  return miscFixedXAdvance(cp, sc);
}

bool ZenGxEPDDisplay::begin() {
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#ifdef ESP32
  SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
#else
  SPI1.begin();
#endif
  display.init(115200, true, 2, false);
  display.setRotation(DISPLAY_ROTATION);
  setTextSize(1);
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.fillScreen(GxEPD_WHITE);
  display.display(true);
#if DISP_BACKLIGHT
  digitalWrite(DISP_BACKLIGHT, LOW);
  pinMode(DISP_BACKLIGHT, OUTPUT);
#endif
  _init = true;
  return true;
}

void ZenGxEPDDisplay::turnOn() {
  if (!_init) begin();
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, HIGH);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, HIGH);
#endif
  _isOn = true;
}

void ZenGxEPDDisplay::turnOff() {
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, LOW);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, LOW);
#endif
  _isOn = false;
}

void ZenGxEPDDisplay::clear() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
}

void ZenGxEPDDisplay::startFrame(ColorVal bkg) {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(_curr_color = GxEPD_BLACK);
  _text_sz = 1;
  int sc = scale();
  display.setFont(&MiscFixed);
  display.setTextSize(sc);
  display_crc.reset();
}

void ZenGxEPDDisplay::setTextSize(int sz) {
  _text_sz = sz;
  _vw_dirty = true;
  display_crc.update<int>(sz);
  // Size 1 scales with orientation: 1× in portrait (≈OLED width), 2× in landscape.
  // Size 2 always uses 2× built-in (12×16) — fixed because layout Y-positions are hardcoded.
  // Size 3 always uses FreeSans18pt for large headings.
  int sc = scale();
  switch (sz) {
    case 4:
      // Huge clock digits: built-in font scaled up. Cursor stays top-left
      // (fontAscender returns 0 for the built-in font), so layout maths is plain.
      display.setFont(NULL);
      display.setTextSize(BIG_TEXT_SCALE);
      break;
    case 3:
      display.setFont(&FreeSans18pt7b);
      display.setTextSize(1);
      break;
    case 2:
      display.setFont(NULL);
      display.setTextSize(scale() * 2);
      break;
    default:
      display.setFont(&MiscFixed);
      display.setTextSize(sc);
      break;
  }
}

void ZenGxEPDDisplay::setColor(ColorVal c) {
  display_crc.update<ColorVal>(c);
  // e-ink: DARK background = white paper, LIGHT foreground = black ink
  if (c == DARK) {
    display.setTextColor(_curr_color = GxEPD_WHITE);
  } else {
    display.setTextColor(_curr_color = GxEPD_BLACK);
  }
}

void ZenGxEPDDisplay::setCursor(int x, int y) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  // Offset y by the font ascender: callers pass top-of-cell y, GFX fonts
  // expect baseline y. Without this, text would be clipped at the top.
  int sc = scale();
  display.setCursor(x, y + fontAscender(_text_sz, sc));
}

void ZenGxEPDDisplay::print(const char* str) {
  display_crc.update<char>(str, strlen(str));
  // misc-fixed path only for sz=1 — setTextSize(2/3) switches GFX to other fonts.
  if (_text_sz == 1) {
    const int sc = scale();
    // Adapt the GFX baseline to the shared renderer's top-of-row coordinates.
    display.setCursor(display.getCursorX(), display.getCursorY() - 7 * sc);
    miscFixedPrint(display, str, sc, _curr_color);
    display.setCursor(display.getCursorX(), display.getCursorY() + 7 * sc);
    return;
  }
  int sc = scale();
  for (const char* p = str; *p; p++) {
    if ((uint8_t)*p == 0xDB) {
      int cx = display.getCursorX();
      int cy = display.getCursorY();
      // Default GFX font: cursor is top-left of cell (fontAscender=0), so cy=original_y.
      // Draw block at cy (not cy-8*sc — that formula assumes Lemon's baseline offset).
      display.fillRect(cx, cy, 5 * sc, 8 * sc, _curr_color);
      display.setCursor(cx + 6 * sc, cy);
    } else {
      char tmp[2] = {*p, 0};
      display.print(tmp);
    }
  }
}

void ZenGxEPDDisplay::fillRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.fillRect(x, y, w, h, _curr_color);
}

void ZenGxEPDDisplay::drawRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.drawRect(x, y, w, h, _curr_color);
  if (scale() == 2 && w > 2 && h > 2)
    display.drawRect(x + 1, y + 1, w - 2, h - 2, _curr_color);
}

void ZenGxEPDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uintptr_t>((uintptr_t)bits);
  uint16_t widthInBytes = (w + 7) / 8;
  for (uint16_t by = 0; by < h; by++) {
    for (uint16_t bx = 0; bx < w; bx++) {
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      if (pgm_read_byte(bits + byteOffset) & bitMask) {
        display.drawPixel(x + bx, y + by, _curr_color);
      }
    }
  }
}

uint16_t ZenGxEPDDisplay::getTextWidth(const char* str) {
  if (_text_sz == 1) {
    return miscFixedTextWidth(str, scale());
  }
  display.setTextWrap(false);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  display.setTextWrap(true);
  return w;
}

void ZenGxEPDDisplay::setDisplayRotation(uint8_t rot) {
  display.setRotation(rot & 3);
  setDimensions(display.width(), display.height());
  last_display_crc_value = -1;  // force redraw on next endFrame
}

void ZenGxEPDDisplay::endFrame() {
  uint32_t crc = display_crc.finalize();
  if (crc != last_display_crc_value) {
    bool partial = true;
    if (_full_refresh_interval > 0 && ++_partial_count >= _full_refresh_interval) {
      partial = false;
      _partial_count = 0;
    }
    // Drive every pixel, not just the ones that changed since the last frame.
    // A partial update is differential — it drives only what differs from the
    // controller's "previous image" RAM and leaves the rest to hold its own
    // charge, which this panel doesn't do well: text went grey a few updates
    // after it was drawn while whatever had just changed stayed crisp. Priming
    // that RAM with the inverse of the incoming frame makes every pixel a
    // difference, so all of them get driven to their target.
    //
    // It must be the inverse and not a flat white — white makes only
    // white->black a difference, so ink gets re-driven but never erased and
    // every screen ever shown accumulates as a ghost.
    //
    // Costs one extra full-screen RAM write (a few ms of SPI); the refresh
    // itself takes the same time either way, as the waveform clocks the whole
    // panel regardless of how many pixels it actually drives. Clearing ghosts
    // is a separate job and stays with the periodic full refresh above.
    if (partial) display.writeInverseForRedrive();
    display.display(partial);
    last_display_crc_value = crc;
  }
}
