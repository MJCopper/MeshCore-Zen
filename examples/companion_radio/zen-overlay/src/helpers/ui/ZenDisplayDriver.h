#pragma once

#include <stdint.h>
#include <string.h>
#include <Arduino.h>
#include <helpers/ui/DisplayDriver.h>

// Zen's presentation API extends the MeshCore display contract. Keeping the
// baseline class as the base preserves compatibility without shadowing or
// copying ownership of the MeshCore header.
class ZenDisplayDriver : public DisplayDriver {
  int _w, _h;
protected:
  bool _vw_dirty = true;
  bool _vw_result = false;
  char _marquee_text[256] = {0};
  int _marquee_width = -1;
  int _marquee_x = -1, _marquee_y = -1;
  uint16_t _marquee_offset = 0, _marquee_end = 0;
  uint8_t _marquee_phase = 0;  // start hold, forward, end hold, backward, stopped
  unsigned long _marquee_next = 0;
  int _marquee_delay = 0;
  ZenDisplayDriver(int w, int h) : DisplayDriver(w, h) { _w = w; _h = h; }
  void setDimensions(int w, int h) { _w = w; _h = h; }
public:
  // Compatibility colours used by the monochrome-oriented Solo UI helpers.
  // The v1.17 drivers accept ColorVal, so these values continue to map exactly
  // as they did before the UIColor theme palette was introduced.
  enum Color { DARK=0, LIGHT, RED, GREEN, BLUE, YELLOW, ORANGE };

  int width() const { return _w; }
  int height() const { return _h; }

  // One selected overflowing label owns the marquee in a frame. UITask uses
  // the returned delay to redraw only when the next animation step is due.
  void beginMarqueeFrame() { _marquee_delay = 0; }
  int marqueeDelay() const { return _marquee_delay; }
  virtual unsigned long marqueeStepMs() { return isEink() ? 2500 : 220; }
  virtual unsigned long marqueeHoldMs() { return isEink() ? 2500 : 700; }
  virtual uint8_t marqueeStepChars() { return isEink() ? 3 : 1; }

  virtual bool isOn() = 0;
  virtual bool isEink() { return false; } // default to non-eink, override in eink drivers
  virtual void turnOn() = 0;
  virtual void turnOff() = 0;
  virtual void clear() = 0;
  virtual void startFrame(ColorVal bkg = UIColor::window_bkg) = 0;
  virtual void setTextSize(int sz) = 0;
  virtual void setColor(ColorVal c) = 0;
  virtual void setCursor(int x, int y) = 0;
  virtual void print(const char* str) = 0;
  virtual void printWordWrap(const char* str, int max_width) { print(str); }   // fallback to basic print() if no override
  virtual void fillRect(int x, int y, int w, int h) = 0;
  virtual void drawRect(int x, int y, int w, int h) = 0;
  virtual void drawXbm(int x, int y, const uint8_t* bits, int w, int h) = 0;
  virtual uint16_t getTextWidth(const char* str) = 0;
  virtual int getCharWidth() const { return 6; }   // typical character advance width (px)
  virtual int getLineHeight() const { return 8; }  // pixel rows per text line
  virtual void setSingleFont(bool) { }              // no-op; both concrete drivers are permanently pinned to their one font
  virtual bool isSingleFont() const { return false; }
  // Layout helpers — derived from font metrics and screen size.
  // Use these instead of hardcoded pixel values so layouts adapt to any display.
  int lineStep()             const { return getLineHeight() + 2; }         // row pitch: text + gap
  int headerH()              const { return getLineHeight() + 3; }         // title bar height
  // y where list items begin: a 2px breathing gap below the header separator so
  // the first row doesn't touch the line (matches the hand-rolled hdr+2 used by
  // the graphical screens).
  int listStart()            const { return headerH() + 2; }
  int listVisible(int itemH) const { return (height() - listStart()) / itemH; }
  int listVisible()          const { return listVisible(lineStep()); }
  // x where a right-side value column starts (leaves ~8 chars for the value)
  int valCol()               const { return width() - getCharWidth() * 8; }
  // true only on landscape e-ink; use instead of comparing pixel counts or getLineHeight()
#ifdef EINK_DISPLAY_MODEL
  bool isLandscape()         const { return width() >= height(); }
#else
  bool isLandscape()         const { return false; }
#endif
  // separator line thickness: 2px on landscape e-ink, 1px everywhere else
  int sepH()                 const { return isLandscape() ? 2 : 1; }
  virtual void drawTextCentered(int mid_x, int y, const char* str) {
    int w = getTextWidth(str);
    setCursor(mid_x - w/2, y);
    print(str);
  }
  virtual void drawTextRightAlign(int x_anch, int y, const char* str) {
    int w = getTextWidth(str);
    setCursor(x_anch - w, y);
    print(str);
  }
  virtual void drawTextLeftAlign(int x_anch, int y, const char* str) {
    setCursor(x_anch, y);
    print(str);
  }
  
  // Common selection-row pattern: when sel, fills (x,y,w,h) with ink and sets
  // colour to DARK so the next text render appears inverted; when not sel,
  // just sets ink colour to LIGHT. Replaces the 5-line if/else copy-paste
  // present in every list-style screen.
  void drawSelectionRow(int x, int y, int w, int h, bool sel) {
    setColor(LIGHT);
    if (sel) {
      fillRect(x, y, w, h);
      setColor(DARK);
    }
  }

  // Format a small unread count into buf: "1".."99", then "99+". count >= 1.
  // No stdio — ZenDisplayDriver.h only pulls stdint/string.
  static void fmtBadgeCount(char* buf, int count) {
    if (count > 99)       { buf[0]='9'; buf[1]='9'; buf[2]='+'; buf[3]=0; }
    else if (count >= 10) { buf[0]=(char)('0'+count/10); buf[1]=(char)('0'+count%10); buf[2]=0; }
    else                  { buf[0]=(char)('0'+count); buf[1]=0; }
  }
  // Trailing blank column baked into a backend's own advance-based
  // getTextWidth() (0 if it's already ink-tight). Adafruit_GFX's classic
  // built-in font measures every character as a fixed 6px advance cell (5px
  // glyph + 1px inter-character gap) regardless of the glyph actually drawn —
  // so getTextWidth() always reports 1px more than the real ink, all of it
  // trailing the last character. Centring on the raw width then leaves 1px
  // more slack on the right than the left. Overridden by backends that use
  // that font.
  virtual int textWidthTrailingGap() const { return 0; }
  // Pixel width the pill from drawUnreadBadge(count) occupies — for reserving
  // the name column before it. Mirrors the pill's horizontal padding.
  int unreadBadgeWidth(int count) {
    char buf[5]; fmtBadgeCount(buf, count);
    int pad = sepH() + 1;
    return getTextWidth(buf) - textWidthTrailingGap() + pad * 2;
  }
  // Right-aligned unread-count "pill": a filled capsule ending at right_x,
  // aligned to a text row of height getLineHeight() at y, with the count
  // knocked out. On a selected/inverted row pass sel=true so the pill inverts
  // too (paper capsule + ink digits) and stays visible. The four corners are
  // knocked back to the surrounding colour for a rounded-capsule look.
  // Restores ink to LIGHT. Returns the pill width.
  int drawUnreadBadge(int right_x, int y, int count, bool sel) {
    char buf[5]; fmtBadgeCount(buf, count);
    int pad = sepH() + 1;
    int pw = getTextWidth(buf) - textWidthTrailingGap() + pad * 2;
    int ph = getLineHeight();
    int px = right_x - pw;
    Color body = sel ? DARK : LIGHT;
    Color ink  = sel ? LIGHT : DARK;
    setColor(body);
    fillRect(px, y, pw, ph);
    setColor(ink);
    fillRect(px, y, 1, 1);
    fillRect(px + pw - 1, y, 1, 1);
    fillRect(px, y + ph - 1, 1, 1);
    fillRect(px + pw - 1, y + ph - 1, 1, 1);
    setCursor(px + pad, y);
    print(buf);
    setColor(LIGHT);
    return pw;
  }

  // Inverted title bar: light background, dark ellipsized label, then the
  // standard separator line. The label is UTF-8 translated by
  // drawTextEllipsized. Leaves ink colour LIGHT for following content.
  // Pixel width the ≡ context-menu hint reserves at the header's right edge.
  int menuHintWidth() const { return getCharWidth() + 3; }

  // Small ≡ glyph at the header's top-right, signalling the screen has a
  // Hold-Enter context menu. Three stacked bars, drawn in colour c (LIGHT on a
  // plain header, DARK on an inverted bar). When active (the menu is open) the
  // corner cell is highlighted and the bars knocked out, tying the glyph to the
  // popup it spawned. Call after the header body.
  void drawContextMenuHint(Color c = LIGHT, bool active = false) {
    int gw = getCharWidth() + 1;
    int gx = width() - gw - 1;
    int th = sepH();
    int gy = (getLineHeight() - (th * 3 + 4)) / 2;
    if (gy < 0) gy = 0;
    Color bars = c;
    if (active) {
      setColor(c);
      fillRect(gx - 1, 0, gw + 2, headerH() - sepH());
      bars = (c == LIGHT) ? DARK : LIGHT;
    }
    setColor(bars);
    for (int i = 0; i < 3; i++) fillRect(gx, gy + i * (th + 2), gw, th);
    setColor(LIGHT);
  }

  void drawInvertedHeader(const char* label, bool menu_hint = false, bool menu_open = false) {
    int hdr = headerH();
    setColor(LIGHT);
    fillRect(0, 0, width(), hdr - 1);
    int reserve = menu_hint ? menuHintWidth() : 0;
    setColor(DARK);
    drawTextEllipsized(2, 1, width() - 4 - reserve, (label && label[0]) ? label : "");
    if (menu_hint) drawContextMenuHint(DARK, menu_open);
    setColor(LIGHT);
    fillRect(0, hdr - 1, width(), sepH());
  }

  // Centred screen title + bottom separator line — the standard top bar for
  // full-screen list/detail views. Leaves ink LIGHT; callers can draw extra
  // header content (counters, etc.) afterwards. menu_hint adds the ≡ glyph;
  // menu_open highlights it while the context menu is on screen.
  void drawCenteredHeader(const char* title, bool menu_hint = false, bool menu_open = false) {
    setColor(LIGHT);
    int reserve = menu_hint ? menuHintWidth() : 0;
    char buf[96];
    strncpy(buf, (title && title[0]) ? title : "", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int avail = width() - reserve - 4;
    if (avail < 0) avail = 0;
    if (getTextWidth(buf) <= avail) {
      int w = getTextWidth(buf);
      setCursor((width() - reserve) / 2 - w / 2, 0);   // centred, clear of the ≡ hint
      print(buf);
    } else {
      // Too wide to centre without print() wrapping onto a second line → left-align
      // and ellipsize, like a list row. (Long DM / room-server / channel names.)
      drawTextEllipsized(2, 0, avail, buf);
    }
    fillRect(0, headerH() - sepH(), width(), sepH());
    if (menu_hint) drawContextMenuHint(LIGHT, menu_open);
  }

  // Advance a UTF-8 pointer by one codepoint, returning the decoded value.
  // Invalid sequences return 0xFFFD and consume trailing continuation bytes.
  static uint32_t decodeCodepoint(const uint8_t*& p) {
    uint8_t c = *p++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
      uint32_t cp = c & 0x1F;
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      return cp;
    }
    if ((c & 0xF0) == 0xE0) {
      uint32_t cp = c & 0x0F;
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      return cp;
    }
    if ((c & 0xF8) == 0xF0) {
      uint32_t cp = c & 0x07;
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      return cp;
    }
    while (*p && (*p & 0xC0) == 0x80) p++;
    return 0xFFFD;
  }

  // Width of a single codepoint in pixels. Default: fall back to getTextWidth
  // on a one-codepoint UTF-8 string. Drivers can override for O(1) lookup.
  virtual uint16_t getCodepointWidth(uint32_t cp) {
    char buf[5];
    int n = 0;
    if (cp < 0x80) { buf[n++] = (char)cp; }
    else if (cp < 0x800) { buf[n++] = 0xC0 | (cp >> 6); buf[n++] = 0x80 | (cp & 0x3F); }
    else if (cp < 0x10000) { buf[n++] = 0xE0 | (cp >> 12); buf[n++] = 0x80 | ((cp >> 6) & 0x3F); buf[n++] = 0x80 | (cp & 0x3F); }
    else { buf[n++] = 0xF0 | (cp >> 18); buf[n++] = 0x80 | ((cp >> 12) & 0x3F); buf[n++] = 0x80 | ((cp >> 6) & 0x3F); buf[n++] = 0x80 | (cp & 0x3F); }
    buf[n] = '\0';
    return getTextWidth(buf);
  }


  // Selected overflowing rows swing between their beginning and end. Other
  // rows retain the static ellipsis and incur no extra redraws.
  virtual int drawTextEllipsized(int x, int y, int max_width, const char* str,
                                 bool selected = false) {
    char temp_str[256];  // reasonable buffer size
    strncpy(temp_str, str ? str : "", sizeof(temp_str) - 1);
    temp_str[sizeof(temp_str) - 1] = '\0';

    // Fold newlines into spaces: this draws ONE line clipped to max_width, but
    // print() acts on '\n' by returning to x=0 one row down, which would spill
    // the tail onto whatever is drawn below. Message bodies (the compact
    // one-line previews in the history list) are the texts that carry them;
    // for labels and names this is a no-op. A space keeps the words apart and
    // measures the same, so the width/ellipsis maths below is unaffected.
    for (char* q = temp_str; *q; q++) if (*q == '\n' || *q == '\r') *q = ' ';

    if (getTextWidth(temp_str) <= max_width) {
      setCursor(x, y);
      print(temp_str);
      return 0;
    }

    if (selected) {
      unsigned long now = millis();
      bool changed = strcmp(temp_str, _marquee_text) || max_width != _marquee_width ||
                     x != _marquee_x || y != _marquee_y;
      if (changed) {
        strncpy(_marquee_text, temp_str, sizeof(_marquee_text) - 1);
        _marquee_text[sizeof(_marquee_text) - 1] = 0;
        _marquee_width = max_width;
        _marquee_x = x; _marquee_y = y;
        _marquee_offset = 0;
        _marquee_phase = 0;
        _marquee_next = now + marqueeHoldMs();
        const uint8_t* p = (const uint8_t*)temp_str;
        int remaining = getTextWidth(temp_str);
        _marquee_end = 0;
        while (*p && remaining > max_width) {
          remaining -= getCodepointWidth(decodeCodepoint(p));
          _marquee_end++;
        }
      }
      if (_marquee_phase != 4 && (int32_t)(now - _marquee_next) >= 0) {
        uint8_t step = marqueeStepChars();
        if (_marquee_phase <= 1) {
          _marquee_phase = 1;
          _marquee_offset += step;
          if (_marquee_offset >= _marquee_end) {
            _marquee_offset = _marquee_end;
            // E-ink performs one slow traversal and then stops at the end.
            // Repeated back-and-forth partial refreshes cost appreciable power.
            if (isEink()) {
              _marquee_phase = 4;
              _marquee_next = 0;
            } else {
              _marquee_phase = 2;
              _marquee_next = now + marqueeHoldMs();
            }
          } else _marquee_next = now + marqueeStepMs();
        } else {
          _marquee_phase = 3;
          if (_marquee_offset <= step) {
            _marquee_offset = 0;
            _marquee_phase = 0;
            _marquee_next = now + marqueeHoldMs();
          } else {
            _marquee_offset -= step;
            _marquee_next = now + marqueeStepMs();
          }
        }
      }
      const uint8_t* start = (const uint8_t*)temp_str;
      for (uint16_t i = 0; i < _marquee_offset && *start; i++) decodeCodepoint(start);
      // Copy complete UTF-8 codepoints only. Byte-at-a-time clipping can leave
      // a continuation byte behind and turn the last glyph into a placeholder.
      const uint8_t* end = start;
      const uint8_t* scan = start;
      int window_width = 0;
      while (*scan) {
        const uint8_t* before = scan;
        uint32_t cp = decodeCodepoint(scan);
        int cp_width = getCodepointWidth(cp);
        if (window_width + cp_width > max_width) break;
        window_width += cp_width;
        end = scan;
        if (scan == before) break;
      }
      char window[256];
      size_t length = (size_t)(end - start);
      if (length >= sizeof(window)) length = sizeof(window) - 1;
      memcpy(window, start, length);
      window[length] = 0;
      setCursor(x, y);
      print(window);
      _marquee_delay = _marquee_phase == 4 ? 0
          : (_marquee_next > now ? (int)(_marquee_next - now) : 1);
      return _marquee_delay;
    }
    
    // for variable-width fonts (GxEPD), add space after ellipsis
    // for fixed-width fonts (OLED), keep tight spacing to save precious characters
    const char* ellipsis;
    // use a simple heuristic: if 'i' and 'l' have different widths, it's variable-width
    if (_vw_dirty) {
      _vw_result = (getTextWidth("i") != getTextWidth("l"));
      _vw_dirty = false;
    }
    if (_vw_result) {
      ellipsis = "... ";  // variable-width fonts: add space
    } else {
      ellipsis = "...";   // fixed-width fonts: no space
    }
    
    int ellipsis_width = getTextWidth(ellipsis);
    int str_len = strlen(temp_str);
    
    while (str_len > 0 && getTextWidth(temp_str) > max_width - ellipsis_width) {
      temp_str[--str_len] = 0;
    }
    // Strip orphaned UTF-8 leading byte left by byte-at-a-time trimming above.
    while (str_len > 0 && ((uint8_t)temp_str[str_len - 1] & 0xC0) == 0xC0) {
      temp_str[--str_len] = 0;
    }
    strcat(temp_str, ellipsis);
    
    setCursor(x, y);
    print(temp_str);
    return 0;
  }
  
  virtual void setBrightness(uint8_t level) { }  // level 0-4 (min to max), no-op default
  virtual void setDisplayRotation(uint8_t rot) { }  // 0-3, no-op for fixed-orientation displays
  virtual void setFullRefreshInterval(uint8_t n) { }  // e-ink: do full refresh every n partial refreshes (0=never)
  virtual void endFrame() = 0;

};
