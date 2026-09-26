#pragma once
// Tools › Repeater — repeater toggle on a dedicated screen.
// Repeater mode always uses the companion's current
// radio parameters, so changing Settings > Radio changes both roles together.
// Fixed forwarding timing lives in zen/RepeaterTiming.h. Live forwarding
// stats live separately on Tools › Diagnostics.
//
// Included by UITask.cpp.

#include <helpers/ui/ZenDisplayDriver.h>
#include <helpers/ui/ZenUIScreen.h>
#include "icons.h"
#include "UIFramework.h"
#include "../MyMesh.h"

extern MyMesh the_mesh;

class RepeaterScreen : public ZenUIScreen {
  UITask* _task;
  bool    _dirty;
  bool    _initial_enabled;
  int     _sel;
  int     _scroll;    // first visible row (render keeps _sel in view)

  enum Item { IT_REPEATER, ITEM_COUNT };
  static const zenui::SettingRow ROWS[ITEM_COUNT];

  static const char* itemLabel(int item) {
    switch (item) {
      case IT_REPEATER: return "Repeater";
    }
    return "";
  }

  void itemValue(int item, ZenPrefs* p, char* buf, size_t n) const {
    if (!p) { strncpy(buf, "Off", n); buf[n-1]=0; return; }
    switch (item) {
      case IT_REPEATER: strncpy(buf, p->isRepeatEn() ? "On" : "Off", n); break;
      default: strncpy(buf, "", n); break;
    }
    buf[n - 1] = '\0';
  }

public:
  RepeaterScreen(UITask* task) : _task(task), _dirty(false), _initial_enabled(false), _sel(0), _scroll(0) {}

  void onShow() override {
    ZenPrefs* p = _task->getNodePrefs();
    if (!_dirty) _initial_enabled = p && p->isRepeatEn();
    _sel = 0; _scroll = 0;
  }
  void onHide() override { _task->savePrefsIfDirty(_dirty); }

  int render(ZenDisplayDriver& display) override {
    ZenPrefs* p = _task->getNodePrefs();
    display.setTextSize(1);
    display.setColor(ZenDisplayDriver::LIGHT);
    display.drawCenteredHeader("Repeater");

    // Config only — live forwarding stats live on Tools › Diagnostics.
    zenui::renderSettings(display, ROWS, ITEM_COUNT, _sel, _scroll,
      [&](uint8_t item, char* value, size_t n) { itemValue(item, p, value, n); });
    return zenui::refreshMs(zenui::Refresh::Static);
  }

  bool handleInput(char c) override {
    ZenPrefs* p = _task->getNodePrefs();

    if (c == KEY_CANCEL) {
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) return true;
    if (zenui::moveWrapped(c, ITEM_COUNT, _sel)) return true;
    if (!p) return false;

    bool right = keyIsNext(c);
    bool left  = keyIsPrev(c);
    bool enter = (c == KEY_ENTER);
    int item = _sel;

    if (item == IT_REPEATER && (left || right || enter)) {
      p->setRepeatEn(!p->isRepeatEn());
      _dirty = (p->isRepeatEn() != _initial_enabled);
      _task->showAlert(p->isRepeatEn() ? "Repeater: On" : "Repeater: Off", 900);
      return true;
    }
    return false;
  }
};

const zenui::SettingRow RepeaterScreen::ROWS[RepeaterScreen::ITEM_COUNT] = {
  { "Repeater", zenui::RowKind::Toggle, RepeaterScreen::IT_REPEATER },
};
