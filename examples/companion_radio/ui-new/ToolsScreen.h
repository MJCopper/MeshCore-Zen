#pragma once
// Custom screen — not part of upstream UITask.cpp.
// Kept as a flat list: this focused build has few enough tools that category
// submenus only add an unnecessary navigation step.

#include "../solo/SoloFeatures.h"

class ToolsScreen : public UIScreen {
  UITask* _task;

  enum Action {
    ACT_NEARBY, ACT_DISCOVER,
#if SOLO_FEAT_NAVIGATION && SOLO_FEAT_LOCATION_TOOLS
    ACT_LIVESHARE, ACT_TRAIL, ACT_LOCATOR, ACT_COMPASS,
#endif
#if SOLO_FEAT_REMOTE_BOT
    ACT_BOT,
#endif
#if SOLO_FEAT_REPEATER
    ACT_REPEATER,
#endif
#if SOLO_FEAT_CLOCK_TOOLS
    ACT_CLOCK,
#endif
    ACT_RINGTONE, ACT_DIAGNOSTICS
#if defined(PIN_GPIO1) && SOLO_FEAT_GPIO
    , ACT_GPIO
#endif
  };

  struct Tool { const char* label; const MiniIcon* icon; Action action; };
  static const Tool TOOLS[];
  static const int TOOL_COUNT;

  int _sel = 0;
  int _scroll = 0;

  void dispatch(Action a) {
    switch (a) {
      case ACT_NEARBY:      _task->gotoNearbyScreen();      break;
      case ACT_DISCOVER:    _task->gotoDiscoverScreen();    break;
#if SOLO_FEAT_NAVIGATION && SOLO_FEAT_LOCATION_TOOLS
      case ACT_LIVESHARE:   _task->gotoLiveShareScreen();   break;
      case ACT_TRAIL:       _task->gotoTrailScreen();       break;
      case ACT_LOCATOR:     _task->gotoLocatorScreen();     break;
      case ACT_COMPASS:     _task->gotoCompassScreen();     break;
#endif
#if SOLO_FEAT_REMOTE_BOT
      case ACT_BOT:         _task->gotoBotScreen();         break;
#endif
#if SOLO_FEAT_REPEATER
      case ACT_REPEATER:    _task->gotoRepeaterScreen();    break;
#endif
#if SOLO_FEAT_CLOCK_TOOLS
      case ACT_CLOCK:       _task->gotoClockTools();        break;
#endif
      case ACT_RINGTONE:    _task->gotoRingtoneEditor();    break;
      case ACT_DIAGNOSTICS: _task->gotoDiagnosticsScreen(); break;
#if defined(PIN_GPIO1) && SOLO_FEAT_GPIO
      case ACT_GPIO:        _task->gotoGpioScreen();        break;
#endif
    }
  }

public:
  explicit ToolsScreen(UITask* task) : _task(task) {}

  static int itemCount() { return TOOL_COUNT; }
  static const char* itemLabel(int index) {
    return (index >= 0 && index < TOOL_COUNT) ? TOOLS[index].label : "";
  }
  void openItem(int index) {
    if (index >= 0 && index < TOOL_COUNT) dispatch(TOOLS[index].action);
  }

  void onShow() override { _sel = _scroll = 0; }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    display.drawCenteredHeader("Tools");

    drawList(display, TOOL_COUNT, _sel, _scroll,
      [&](int idx, int y, bool selected, int reserve) {
        drawRowSelection(display, y, selected, reserve);
        display.drawTextEllipsized(2, y, display.width() - 4 - reserve,
                                   TOOLS[idx].label, selected);
      });
    return UI_REFRESH_STATIC_MS;
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL) {
      _task->gotoHomeScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) return true;
    if (c == KEY_UP && TOOL_COUNT > 0) {
      _sel = _sel > 0 ? _sel - 1 : TOOL_COUNT - 1;
      return true;
    }
    if (c == KEY_DOWN && TOOL_COUNT > 0) {
      _sel = _sel < TOOL_COUNT - 1 ? _sel + 1 : 0;
      return true;
    }
    if (c == KEY_ENTER && TOOL_COUNT > 0) {
      dispatch(TOOLS[_sel].action);
      return true;
    }
    return false;
  }
};

const ToolsScreen::Tool ToolsScreen::TOOLS[] = {
  { "Discover Repeaters", &ICON_MAP_CONTACT, ACT_DISCOVER },
  { "Node List",          &ICON_MAP_CONTACT, ACT_NEARBY },
#if SOLO_FEAT_NAVIGATION && SOLO_FEAT_LOCATION_TOOLS
  { "Live Share",      &ICON_GPS,          ACT_LIVESHARE },
  { "Trail",           &ICON_TRAIL,        ACT_TRAIL },
  { "Locator",         &ICON_MAP_WAYPOINT, ACT_LOCATOR },
  { "Compass",         &ICON_MAP_NORTH,    ACT_COMPASS },
#endif
#if SOLO_FEAT_REMOTE_BOT
  { "Remote Bot",      &ICON_BOT,          ACT_BOT },
#endif
#if SOLO_FEAT_REPEATER
  { "Repeater Mode",   &ICON_REPEATER,     ACT_REPEATER },
#endif
#if SOLO_FEAT_CLOCK_TOOLS
  { "Clock Tools",     &ICON_ALARM,        ACT_CLOCK },
#endif
  { "Ringtone Editor", &ICON_NOTE,         ACT_RINGTONE },
  { "Diagnostics",     &ICON_CHART,        ACT_DIAGNOSTICS },
#if defined(PIN_GPIO1) && SOLO_FEAT_GPIO
  { "GPIO",            &ICON_GEAR,         ACT_GPIO },
#endif
};
const int ToolsScreen::TOOL_COUNT = sizeof(ToolsScreen::TOOLS) / sizeof(ToolsScreen::TOOLS[0]);
