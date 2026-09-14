#pragma once

#include <stdint.h>
#include <helpers/ui/UIScreen.h>

// Fixed-capacity return stack. It avoids heap allocation and, when full,
// retains the most recent destinations by discarding the oldest one.
template <uint8_t Capacity>
class ScreenHistory {
  UIScreen* _items[Capacity] = {};
  uint8_t _count = 0;

public:
  void clear() { _count = 0; }
  uint8_t size() const { return _count; }
  void push(UIScreen* screen) {
    if (!screen || Capacity == 0) return;
    if (_count == Capacity) {
      for (uint8_t i = 1; i < Capacity; i++) _items[i - 1] = _items[i];
      _count--;
    }
    _items[_count++] = screen;
  }
  UIScreen* pop() { return _count ? _items[--_count] : nullptr; }
};

struct ScreenTransition {
  static bool apply(UIScreen*& current, UIScreen* next) {
    if (!next || current == next) return false;
    if (current) current->onHide();
    current = next;
    current->onShow();
    return true;
  }
};
