#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include "../NodePrefs.h"
#include "../solo/TimezonePolicy.h"

// Dedicated timezone editor. City mode applies the selected city's DST rules;
// Fixed UTC uses one unchanging offset. Only the relevant value is presented.
class TimezoneEditor {
  bool _active = false;
  uint8_t _selected = 0;

  static void formatOffset(char* out, size_t size, int16_t minutes) {
    int magnitude = minutes < 0 ? -minutes : minutes;
    snprintf(out, size, "%c%02d:%02d", minutes < 0 ? '-' : '+',
             magnitude / 60, magnitude % 60);
  }

  static void drawValueRow(DisplayDriver& display, int y, bool selected,
                           const char* label, const char* value) {
    display.drawSelectionRow(0, y - 1, display.width(),
                             display.getLineHeight() + 1, selected);
    display.drawTextLeftAlign(2, y, label);
    display.drawTextRightAlign(display.width() - 2, y, value);
    display.setColor(DisplayDriver::LIGHT);
  }

public:
  void begin() { _active = true; _selected = 0; }
  bool active() const { return _active; }
  void close() { _active = false; }

  bool handleInput(char c, NodePrefs& prefs) {
    if (!_active) return false;
    if (c == KEY_CANCEL) { _active = false; return true; }
    if (c == KEY_UP || c == KEY_DOWN) { _selected ^= 1; return true; }

    int direction = keyIsNext(c) ? 1 : (keyIsPrev(c) ? -1 : 0);
    if (!direction) return c == KEY_ENTER;
    if (_selected == 0) {
      prefs.timezone_mode = prefs.timezone_mode == solo::TimezonePolicy::CITY
          ? solo::TimezonePolicy::MANUAL : solo::TimezonePolicy::CITY;
      return true;
    }
    if (prefs.timezone_mode == solo::TimezonePolicy::CITY) {
      int city = prefs.timezone_city + direction;
      if (city < 0) city = solo::TimezonePolicy::CITY_COUNT - 1;
      if (city >= solo::TimezonePolicy::CITY_COUNT) city = 0;
      prefs.timezone_city = (uint8_t)city;
    } else {
      int minutes = prefs.timezone_manual_min + direction * 15;
      if (minutes > 840) minutes = -720;
      if (minutes < -720) minutes = 840;
      prefs.timezone_manual_min = (int16_t)minutes;
      prefs.tz_offset_hours = (int8_t)(minutes / 60); // compatibility mirror
    }
    return true;
  }

  void render(DisplayDriver& display, const NodePrefs& prefs, uint32_t utc_time) const {
    display.drawCenteredHeader("Time Zone");
    int y = display.listStart();
    bool city_mode = prefs.timezone_mode == solo::TimezonePolicy::CITY;
    drawValueRow(display, y, _selected == 0, "Mode", city_mode ? "City" : "Fixed UTC");
    y += display.lineStep();

    char value[16];
    if (city_mode) {
      snprintf(value, sizeof(value), "%s", solo::TimezonePolicy::cityName(prefs.timezone_city));
      drawValueRow(display, y, _selected == 1, "City", value);
    } else {
      formatOffset(value, sizeof(value), prefs.timezone_manual_min);
      drawValueRow(display, y, _selected == 1, "UTC Offset", value);
    }
    y += display.lineStep();

    if (utc_time < 1000000000UL) {
      display.drawTextLeftAlign(2, y, "Offset --:--");
      return;
    }
    int16_t effective = solo::TimezonePolicy::offsetMinutes(
        prefs.timezone_mode, prefs.timezone_manual_min, prefs.timezone_city, utc_time);
    formatOffset(value, sizeof(value), effective);
    char summary[24];
    bool daylight = city_mode && solo::TimezonePolicy::inDst(
        solo::TimezonePolicy::cities()[prefs.timezone_city], utc_time);
    snprintf(summary, sizeof(summary), "Offset %s%s", value, daylight ? " DST" : "");
    display.drawTextLeftAlign(2, y, summary);
  }
};
