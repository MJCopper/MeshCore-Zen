#pragma once
// Configures automatic adverts and the location privacy policy shared by every
// self-advert path (automatic, on-device manual, and companion-app manual).
// Included by UITask.cpp with the other settings screens.

class AutoAdvertScreen : public UIScreen {
  UITask*    _task;
  NodePrefs* _prefs;
  bool       _dirty;
  uint32_t   _initial_interval;
  uint8_t    _initial_loc_policy;
  int        _sel;
  int        _scroll;

  static const int OPT_COUNT = 4;
  static const uint32_t OPTS[OPT_COUNT];
  static const char*    OPT_LABELS[OPT_COUNT];

  int currentIdx() const {
    for (int i = 0; i < OPT_COUNT; i++)
      if (OPTS[i] == _prefs->advert_auto_interval_sec) return i;
    return 0;
  }

public:
  AutoAdvertScreen(UITask* task, NodePrefs* prefs) : _task(task), _prefs(prefs) {}

  void onShow() override {
    _dirty = false;
    _sel = _scroll = 0;
    _initial_interval = _prefs->advert_auto_interval_sec;
    _initial_loc_policy = _prefs->advert_loc_policy;
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

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("Advert");

    drawList(display, 2, _sel, _scroll, [&](int item, int y, bool selected, int reserve) {
      drawRowSelection(display, y, selected, reserve);
      display.setCursor(2, y);
      display.print(item == 0 ? "Auto Advert" : "GPS Details");
      const char* value = item == 0 ? OPT_LABELS[currentIdx()]
                                    : (_prefs->advert_loc_policy == ADVERT_LOC_NONE ? "Hide" : "Share");
      display.drawTextRightAlign(display.width() - reserve - 2, y, value);
      display.setColor(DisplayDriver::LIGHT);
    });
    return UI_REFRESH_STATIC_MS;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL) {
      _task->gotoHomeScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) return true;
    if (c == KEY_UP)   { _sel = wrapSelection(_sel, 2, -1); return true; }
    if (c == KEY_DOWN) { _sel = wrapSelection(_sel, 2, +1); return true; }
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
