#pragma once

// Small, reusable emoji picker for text fields that explicitly opt in. The
// catalogue is intentionally separate from KeyboardWidget: adding categories
// or more entries later will not alter UTF-8 editing or message transport.

#include "PopupMenu.h"

class EmojiPicker {
  PopupMenu _menu;

  static const char* label(int index) {
    static const char* const LABELS[] = {
      "\xF0\x9F\x91\x8D Like",     // U+1F44D thumbs up
      "\xF0\x9F\x91\x8E Dislike",  // U+1F44E thumbs down
      "\xF0\x9F\x99\x82 Smile",    // U+1F642 slightly smiling face
      "\xF0\x9F\x99\x81 Frown",    // U+1F641 slightly frowning face
    };
    return LABELS[index];
  }

  static const char* value(int index) {
    static const char* const VALUES[] = {
      "\xF0\x9F\x91\x8D", "\xF0\x9F\x91\x8E",
      "\xF0\x9F\x99\x82", "\xF0\x9F\x99\x81",
    };
    return VALUES[index];
  }

public:
  bool active() const { return _menu.active; }
  void close() { _menu.active = false; }

  void open() {
    _menu.begin("Emoji", 4);
    for (int i = 0; i < 4; i++) _menu.addItem(label(i));
  }

  void render(ZenDisplayDriver& display) { if (_menu.active) _menu.render(display); }

  // Returns the selected UTF-8 sequence, or nullptr while navigating/cancelling.
  const char* handleInput(char c) {
    PopupMenu::Result result = _menu.handleInput(c);
    if (result != PopupMenu::SELECTED) return nullptr;
    int selected = _menu.selectedIndex();
    return selected >= 0 && selected < 4 ? value(selected) : nullptr;
  }
};
