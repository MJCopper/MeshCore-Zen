#pragma once

#include <stddef.h>
#include <stdint.h>

namespace zen {

// Allocation-free sentence-boundary policy shared by every message editor.
// It deliberately handles only chat punctuation; language-specific abbreviation
// detection would add cost and unpredictability for very little benefit here.
class SentenceCase {
 public:
  static bool shouldCapitalize(const char* text, size_t cursor) {
    if (!text) return true;

    size_t start = cursor;
    while (start > 0) {
      char c = text[start - 1];
      if (c == '.' || c == '!' || c == '?') break;
      start--;
    }

    size_t pos = start;
    skipDecoration(text, cursor, pos);

    // A leading reply mention identifies the recipient, not sentence content.
    // Support more than one mention without coupling this policy to room or
    // channel reply formatting.
    while (pos < cursor && text[pos] == '@') {
      while (pos < cursor && !isSpace(text[pos])) pos++;
      skipDecoration(text, cursor, pos);
    }

    return pos == cursor;
  }

  static char apply(char c, const char* text, size_t cursor) {
    if (c >= 'a' && c <= 'z' && shouldCapitalize(text, cursor))
      return (char)(c - ('a' - 'A'));
    return c;
  }

 private:
  static bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }

  static bool isOpeningPunctuation(char c) {
    return c == '\'' || c == '"' || c == '(' || c == '[' || c == '{' ||
           c == '-' || c == ':';
  }

  static void skipDecoration(const char* text, size_t cursor, size_t& pos) {
    while (pos < cursor) {
      uint8_t c = (uint8_t)text[pos];
      if (isSpace((char)c) || isOpeningPunctuation((char)c)) {
        pos++;
      } else if (c >= 0x80) {
        // Emoji and other UTF-8 decoration do not consume the first word.
        pos++;
        while (pos < cursor && ((uint8_t)text[pos] & 0xC0) == 0x80) pos++;
      } else {
        break;
      }
    }
  }
};

} // namespace zen
