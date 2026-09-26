#pragma once
// Configures automatic adverts and the location privacy policy shared by every
// self-advert path (automatic, on-device manual, and companion-app manual).
// Included by UITask.cpp with the other settings screens.

#include "UIFramework.h"

class AutoAdvertScreen : public ZenUIScreen {
  UITask*    _task;
  ZenPrefs* _prefs;
  bool       _dirty;
  uint32_t   _initial_interval;
  uint8_t    _initial_loc_policy;
  int        _sel;
  int        _scroll;

  static const int OPT_COUNT = 4;
  static const uint32_t OPTS[OPT_COUNT];
  static const char*    OPT_LABELS[OPT_COUNT];
  static const zenui::SettingRow ROWS[2];

  int currentIdx() const {
    for (int i = 0; i < OPT_COUNT; i++)
      if (OPTS[i] == _prefs->advert_auto_interval_sec) return i;
    return 0;
  }

public:
  AutoAdvertScreen(UITask* task, ZenPrefs* prefs)
      : _task(task), _prefs(prefs), _dirty(false), _initial_interval(0),
        _initial_loc_policy(0), _sel(0), _scroll(0) {}

  void onShow() override {
    _sel = _scroll = 0;
    if (!_dirty) {
      _initial_interval = _prefs->advert_auto_interval_sec;
      _initial_loc_policy = _prefs->advert_loc_policy;
    }
    for (int i = 0; i < OPT_COUNT; i++)
      if (OPTS[i] == _prefs->advert_auto_interval_sec) return;
    // Retired short intervals must not keep running while the screen shows Off.
    _prefs->advert_auto_interval_sec = 0;
    _dirty = true;
  }

  void onHide() override {
    _dirty = _prefs->advert_auto_interval_sec != _initial_interval ||
             _prefs->advert_loc_policy != _initial_loc_policy;
    _task->savePrefsIfDirty(_dirty);
  }

  int render(ZenDisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(ZenDisplayDriver::LIGHT);
    display.drawCenteredHeader("Advert");

    zenui::renderSettings(display, ROWS, 2, _sel, _scroll,
      [&](uint8_t item, char* value, size_t n) {
        snprintf(value, n, "%s", item == 0 ? OPT_LABELS[currentIdx()]
            : (_prefs->advert_loc_policy == ADVERT_LOC_NONE ? "Hide" : "Share"));
      });
    return zenui::refreshMs(zenui::Refresh::Static);
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL) {
      _task->gotoHomeScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) return true;
    if (zenui::moveWrapped(c, 2, _sel)) return true;
    bool right = keyIsNext(c);
    bool left  = keyIsPrev(c);
    if (c == KEY_ENTER && _sel == 1) right = true;
    if (right || left) {
      if (_sel == 0) {
        int idx = currentIdx();
        idx = right ? (idx + 1) % OPT_COUNT : (idx + OPT_COUNT - 1) % OPT_COUNT;
        _prefs->advert_auto_interval_sec = OPTS[idx];
      } else {
        _prefs->advert_loc_policy = _prefs->advert_loc_policy == ADVERT_LOC_NONE
            ? ADVERT_LOC_SHARE : ADVERT_LOC_NONE;
      }
      _dirty = true;
      return true;
    }
    return false;
  }
};

const uint32_t AutoAdvertScreen::OPTS[AutoAdvertScreen::OPT_COUNT]       = { 0, 3600, 10800, 21600 };
const char*    AutoAdvertScreen::OPT_LABELS[AutoAdvertScreen::OPT_COUNT] = { "Off", "1h", "3h", "6h" };
const zenui::SettingRow AutoAdvertScreen::ROWS[2] = {
  { "Auto Advert", zenui::RowKind::Choice, 0 },
  { "GPS Details", zenui::RowKind::Toggle, 1 },
};
