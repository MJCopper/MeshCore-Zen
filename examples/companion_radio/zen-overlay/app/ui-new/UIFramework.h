#pragma once

#include <stdint.h>
#include <stddef.h>
#include <helpers/ui/ZenUIScreen.h>
#include "icons.h"

// Small, allocation-free presentation models shared by Zen screens. Feature
// code owns actions and values; these descriptors keep labels, row semantics
// and refresh behaviour consistent without introducing a second UI runtime.
namespace zenui {

enum class Refresh : int {
  Input    = 50,
  Active   = UI_REFRESH_ACTIVE_MS,
  Status   = 1000,
  Content  = 2000,
  Idle     = 5000,
  Static   = UI_REFRESH_STATIC_MS,
};

static inline int refreshMs(Refresh value) { return static_cast<int>(value); }

enum class RowKind : uint8_t {
  Action,
  Toggle,
  Choice,
  Numeric,
  Subscreen,
  ReadOnly,
};

struct MenuItem {
  const char* label;
  uint8_t action;
  bool enabled;
};

template <size_t Count>
class MenuModel {
  const MenuItem (&_items)[Count];
public:
  explicit constexpr MenuModel(const MenuItem (&items)[Count]) : _items(items) {}
  constexpr int count() const { return (int)Count; }
  const char* label(int index) const {
    return index >= 0 && index < count() ? _items[index].label : "";
  }
  bool enabled(int index) const {
    return index >= 0 && index < count() && _items[index].enabled;
  }
  uint8_t action(int index) const {
    return index >= 0 && index < count() ? _items[index].action : 0;
  }
};

struct SettingRow {
  const char* label;
  RowKind kind;
  uint8_t id;
};

template <typename Label>
static int renderMenu(ZenDisplayDriver& display, int count, int selected,
                      int& scroll, Label label) {
  return drawList(display, count, selected, scroll,
      [&](int index, int y, bool active, int reserve) {
        drawRowSelection(display, y, active, reserve);
        display.drawTextEllipsized(2, y, display.width() - 4 - reserve,
                                   label(index), active);
      });
}

static inline bool moveWrapped(char key, int count, int& selected) {
  if ((uint8_t)key == KEY_UP) {
    selected = wrapSelection(selected, count, -1);
    return true;
  }
  if ((uint8_t)key == KEY_DOWN) {
    selected = wrapSelection(selected, count, 1);
    return true;
  }
  return false;
}

template <typename Value>
static int renderSettings(ZenDisplayDriver& display, const SettingRow* rows,
                          int count, int selected, int& scroll, Value value) {
  return drawList(display, count, selected, scroll,
      [&](int index, int y, bool active, int reserve) {
        drawRowSelection(display, y, active, reserve);
        char text[24];
        value(rows[index].id, text, sizeof(text));
        drawLabelValueRow(display, y, rows[index].label, text, reserve, active);
        display.setColor(ZenDisplayDriver::LIGHT);
      });
}

} // namespace zenui
