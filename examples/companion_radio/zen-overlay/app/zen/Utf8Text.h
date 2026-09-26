#pragma once

#include <Arduino.h>
#include <helpers/ui/ZenDisplayDriver.h>

// Small, allocation-free UTF-8 helpers shared by text editing and rendering.
// Cursor positions in Zen are byte offsets, while movement is by codepoint.
namespace zen {
namespace utf8 {

inline int previousWidth(const char* text, int pos) {
  if (!text || pos <= 0) return 0;
  int width = 1;
  while (width < pos && width < 4 &&
         ((uint8_t)text[pos - width] & 0xC0) == 0x80) width++;
  return width;
}

inline int widthAt(const char* text, int pos, int length) {
  if (!text || pos < 0 || pos >= length) return 0;
  uint8_t c = (uint8_t)text[pos];
  int width = 1;
  if ((c & 0xE0) == 0xC0) width = 2;
  else if ((c & 0xF0) == 0xE0) width = 3;
  else if ((c & 0xF8) == 0xF0) width = 4;
  return pos + width <= length ? width : length - pos;
}

inline int length(const char* text) {
  int count = 0;
  const uint8_t* cursor = (const uint8_t*)text;
  while (cursor && *cursor) {
    ZenDisplayDriver::decodeCodepoint(cursor);
    count++;
  }
  return count;
}

inline void charAt(const char* text, int index, char out[5]) {
  if (!out) return;
  out[0] = '\0';
  const uint8_t* cursor = (const uint8_t*)text;
  for (int current = 0; cursor && *cursor; current++) {
    const uint8_t* start = cursor;
    ZenDisplayDriver::decodeCodepoint(cursor);
    if (current == index) {
      int width = (int)(cursor - start);
      memcpy(out, start, width);
      out[width] = '\0';
      return;
    }
  }
}

inline void applyAsciiCaps(const char* text, bool caps, char* out, size_t size) {
  if (!out || size == 0) return;
  size_t written = 0;
  const uint8_t* cursor = (const uint8_t*)text;
  while (cursor && *cursor && written + 1 < size) {
    uint32_t cp = ZenDisplayDriver::decodeCodepoint(cursor);
    if (caps && cp >= 'a' && cp <= 'z') cp -= 0x20;
    if (cp < 0x80) {
      out[written++] = (char)cp;
    } else if (written + 2 < size) {
      out[written++] = (char)(0xC0 | (cp >> 6));
      out[written++] = (char)(0x80 | (cp & 0x3F));
    } else {
      break;
    }
  }
  out[written] = '\0';
}

} // namespace utf8
} // namespace zen
