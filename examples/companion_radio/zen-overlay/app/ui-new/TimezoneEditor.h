#pragma once

#include <helpers/ui/ZenDisplayDriver.h>
#include <helpers/ui/ZenUIScreen.h>
#include "../ZenPrefs.h"
#include "../zen/LocalTimeService.h"

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

  static void drawValueRow(ZenDisplayDriver& display, int y, bool selected,
                           const char* label, const char* value) {
    display.drawSelectionRow(0, y - 1, display.width(),
                             display.getLineHeight() + 1, selected);
    display.drawTextLeftAlign(2, y, label);
    display.drawTextRightAlign(display.width() - 2, y, value);
    display.setColor(ZenDisplayDriver::LIGHT);
  }

public:
  void begin() { _active = true; _selected = 0; }
  bool active() const { return _active; }
  void close() { _active = false; }

  bool handleInput(char c, ZenPrefs& prefs) {
    if (!_active) return false;
    if (c == KEY_CANCEL) { _active = false; return true; }
    if (c == KEY_UP || c == KEY_DOWN) { _selected ^= 1; return true; }

    int direction = keyIsNext(c) ? 1 : (keyIsPrev(c) ? -1 : 0);
    if (!direction) return c == KEY_ENTER;
    if (_selected == 0) {
      prefs.timezone_mode = prefs.timezone_mode == zen::TimezonePolicy::CITY
          ? zen::TimezonePolicy::MANUAL : zen::TimezonePolicy::CITY;
      return true;
    }
    if (prefs.timezone_mode == zen::TimezonePolicy::CITY) {
      int city = prefs.timezone_city + direction;
      if (city < 0) city = zen::TimezonePolicy::CITY_COUNT - 1;
      if (city >= zen::TimezonePolicy::CITY_COUNT) city = 0;
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

  void render(ZenDisplayDriver& display, const ZenPrefs& prefs, uint32_t utc_time) const {
    display.drawCenteredHeader("Time Zone");
    int y = display.listStart();
    bool city_mode = prefs.timezone_mode == zen::TimezonePolicy::CITY;
    drawValueRow(display, y, _selected == 0, "Mode", city_mode ? "City" : "Fixed UTC");
    y += display.lineStep();

    char value[16];
    if (city_mode) {
      snprintf(value, sizeof(value), "%s", zen::TimezonePolicy::cityName(prefs.timezone_city));
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
    zen::LocalTimeService local_time;
    local_time.bind(&prefs);
    int16_t effective = local_time.offsetMinutes(utc_time);
    formatOffset(value, sizeof(value), effective);
    char summary[24];
    bool daylight = city_mode && local_time.daylightTime(utc_time);
    snprintf(summary, sizeof(summary), "Offset %s%s", value, daylight ? " DST" : "");
    display.drawTextLeftAlign(2, y, summary);
  }
};
