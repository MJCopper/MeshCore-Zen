#pragma once
// Tools › Repeater — repeater toggle on a dedicated screen.
// Repeater mode always uses the companion's current
// radio parameters, so changing Settings > Radio changes both roles together.
// Fixed forwarding timing lives in solo/RepeaterTiming.h. Live forwarding
// stats live separately on Tools › Diagnostics.
//
// Included by UITask.cpp.

#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include "icons.h"
#include "../MyMesh.h"

extern MyMesh the_mesh;

class RepeaterScreen : public UIScreen {
  UITask* _task;
  bool    _dirty;
  bool    _initial_enabled;
  int     _sel;
  int     _scroll;    // first visible row (render keeps _sel in view)

  enum Item { IT_REPEATER, ITEM_COUNT };

  static const char* itemLabel(int item) {
    switch (item) {
      case IT_REPEATER: return "Repeater";
    }
    return "";
  }

  void itemValue(int item, NodePrefs* p, char* buf, size_t n) const {
    if (!p) { strncpy(buf, "Off", n); buf[n-1]=0; return; }
    switch (item) {
      case IT_REPEATER: strncpy(buf, p->client_repeat ? "On" : "Off", n); break;
      default: strncpy(buf, "", n); break;
    }
    buf[n - 1] = '\0';
  }

public:
  RepeaterScreen(UITask* task) : _task(task), _dirty(false), _initial_enabled(false), _sel(0), _scroll(0) {}

  void onShow() override {
    NodePrefs* p = _task->getNodePrefs();
    _initial_enabled = p && p->client_repeat;
    _dirty = false; _sel = 0; _scroll = 0;
  }
  void onHide() override { _task->savePrefsIfDirty(_dirty); }

  int render(DisplayDriver& display) override {
    NodePrefs* p = _task->getNodePrefs();
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("Repeater");

    // Config only — live forwarding stats live on Tools › Diagnostics.
    drawList(display, ITEM_COUNT, _sel, _scroll, [&](int item, int y, bool sel, int reserve) {
      drawRowSelection(display, y, sel, reserve);
      display.setCursor(2, y);
      display.print(itemLabel(item));
      char val[16];
      itemValue(item, p, val, sizeof(val));
      display.drawTextRightAlign(display.width() - reserve - 2, y, val);
      display.setColor(DisplayDriver::LIGHT);
    });
    return UI_REFRESH_STATIC_MS;
  }

  bool handleInput(char c) override {
    NodePrefs* p = _task->getNodePrefs();

    if (c == KEY_CANCEL) {
      _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) return true;
    if (c == KEY_UP)   { _sel = (_sel > 0) ? _sel - 1 : ITEM_COUNT - 1; return true; }
    if (c == KEY_DOWN) { _sel = (_sel < ITEM_COUNT - 1) ? _sel + 1 : 0; return true; }
    if (!p) return false;

    bool right = keyIsNext(c);
    bool left  = keyIsPrev(c);
    bool enter = (c == KEY_ENTER);
    int item = _sel;

    if (item == IT_REPEATER && (left || right || enter)) {
      p->client_repeat ^= 1;
      _dirty = (p->client_repeat != _initial_enabled);
      _task->showAlert(p->client_repeat ? "Repeater: On" : "Repeater: Off", 900);
      return true;
    }
    return false;
  }
};
