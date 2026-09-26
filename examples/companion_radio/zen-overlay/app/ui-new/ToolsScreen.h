#pragma once
// Custom screen — not part of upstream UITask.cpp.
// Kept as a flat list: this focused build has few enough tools that category
// submenus only add an unnecessary navigation step.

#include "../zen/ZenFeatures.h"
#include "UIFramework.h"

class ToolsScreen : public ZenUIScreen {
  UITask* _task;

  enum Action {
    ACT_NEARBY, ACT_DISCOVER,
#if ZEN_FEATURE_REPEATER
    ACT_REPEATER,
#endif
    ACT_RINGTONE, ACT_DIAGNOSTICS
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
#if ZEN_FEATURE_REPEATER
      case ACT_REPEATER:    _task->gotoRepeaterScreen();    break;
#endif
      case ACT_RINGTONE:    _task->gotoRingtoneEditor();    break;
      case ACT_DIAGNOSTICS: _task->gotoDiagnosticsScreen(); break;
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

  int render(ZenDisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(ZenDisplayDriver::LIGHT);
    display.drawCenteredHeader("Tools");

    zenui::renderMenu(display, TOOL_COUNT, _sel, _scroll,
                      [&](int idx) { return TOOLS[idx].label; });
    return zenui::refreshMs(zenui::Refresh::Static);
  }

  bool handleInput(char c) override {
    if (c == KEY_CANCEL) {
      _task->gotoHomeScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) return true;
    if (zenui::moveWrapped(c, TOOL_COUNT, _sel)) return true;
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
#if ZEN_FEATURE_REPEATER
  { "Repeater Mode",   &ICON_REPEATER,     ACT_REPEATER },
#endif
  { "Ringtone Editor", &ICON_NOTE,         ACT_RINGTONE },
  { "Diagnostics",     &ICON_CHART,        ACT_DIAGNOSTICS },
};
const int ToolsScreen::TOOL_COUNT = sizeof(ToolsScreen::TOOLS) / sizeof(ToolsScreen::TOOLS[0]);
