#pragma once

#include <Arduino.h>
#include "Utf8Text.h"

namespace zen {

// Fixed-capacity UTF-8 editor model. Storage limits remain byte based because
// MeshCore packets and persisted fields are byte limited.
template <int Capacity>
struct TextBuffer {
  char buf[Capacity + 1] = {};
  int len = 0;
  int max_len = Capacity;
  int cursor_pos = 0;

  void reset(const char* initial, int requested_max) {
    max_len = requested_max < 0 ? 0 :
        (requested_max > Capacity ? Capacity : requested_max);
    snprintf(buf, (size_t)max_len + 1, "%s", initial ? initial : "");
    len = (int)strlen(buf);
    cursor_pos = len;
  }

  bool insert(const char* text) {
    if (!text) return false;
    int width = (int)strlen(text);
    if (width <= 0 || len + width > max_len) return false;
    memmove(buf + cursor_pos + width, buf + cursor_pos,
            (size_t)(len - cursor_pos));
    memcpy(buf + cursor_pos, text, (size_t)width);
    len += width;
    cursor_pos += width;
    buf[len] = '\0';
    return true;
  }

  bool replace(int start, int end, const char* text) {
    if (start < 0 || end < start || end > len || !text) return false;
    int width = (int)strlen(text);
    int tail = len - end;
    int new_len = start + width + tail;
    if (new_len > max_len) return false;
    memmove(buf + start + width, buf + end, (size_t)tail);
    if (width) memcpy(buf + start, text, (size_t)width);
    len = new_len;
    cursor_pos = start + width;
    buf[len] = '\0';
    return true;
  }

  bool erasePrevious() {
    int width = utf8::previousWidth(buf, cursor_pos);
    if (!width) return false;
    memmove(buf + cursor_pos - width, buf + cursor_pos,
            (size_t)(len - cursor_pos));
    len -= width;
    cursor_pos -= width;
    buf[len] = '\0';
    return true;
  }

  void movePrevious() {
    if (cursor_pos > 0) cursor_pos -= utf8::previousWidth(buf, cursor_pos);
  }
  void moveNext() {
    if (cursor_pos < len) cursor_pos += utf8::widthAt(buf, cursor_pos, len);
  }
  void moveHome() { cursor_pos = 0; }
  void moveEnd() { cursor_pos = len; }
  void clear() { len = cursor_pos = 0; buf[0] = '\0'; }
};

} // namespace zen
