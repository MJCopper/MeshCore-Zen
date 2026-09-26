#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ZenWordDictionaryBase.h"
#include "ZenWordDictionaryExtra.h"

namespace zen {

// Small, allocation-free prefix completer for conversational text. The word
// list lives in flash and is deliberately independent of KeyboardWidget so it
// can be reused by another UI without pulling in display/input code.
class WordCompleter {
public:
  // Keep enough candidates for a compact editor to fill its one-line hint;
  // callers can still request a smaller result set when space is constrained.
  // Shared candidate capacity also accommodates dotted CLI setting names.
  enum : uint8_t { MAX_SUGGESTIONS = 12, MAX_WORD_LEN = 32 };

  static size_t dictionarySize() { return WORD_COUNT; }
  static const char* wordAt(size_t index) {
    if (index < BASE_WORD_COUNT) return zen_dictionary::BASE_WORDS[index];
    index -= BASE_WORD_COUNT;
    return index < zen_dictionary::EXTRA_WORD_COUNT
        ? zen_dictionary::EXTRA_WORDS[index] : nullptr;
  }

  struct WordRange {
    size_t start;
    size_t end;
  };

  static WordRange currentWord(const char* text, size_t len, size_t cursor) {
    if (!text) return { 0, 0 };
    if (cursor > len) cursor = len;
    size_t start = cursor;
    while (start > 0 && isWordChar(text[start - 1])) start--;
    size_t end = cursor;
    while (end < len && isWordChar(text[end])) end++;
    return { start, end };
  }

  // Writes up to max_results NUL-terminated matches and returns their count.
  // Empty prefixes intentionally return no words: the picker can still show
  // message placeholders without dumping the whole dictionary.
  static uint8_t suggest(const char* prefix, size_t prefix_len,
                         char results[][MAX_WORD_LEN], uint8_t max_results) {
    if (!prefix || prefix_len == 0 || max_results == 0) return 0;
    if (max_results > MAX_SUGGESTIONS) max_results = MAX_SUGGESTIONS;
    uint8_t count = 0;
    for (size_t i = 0; i < zen_dictionary::PINNED_WORD_COUNT &&
                       count < max_results; i++)
      appendMatch(zen_dictionary::PINNED_WORDS[i], prefix, prefix_len,
                  results, count);
    for (size_t i = 0; i < WORD_COUNT && count < max_results; i++) {
      const char* word = wordAt(i);
      bool duplicate = false;
      for (uint8_t j = 0; j < count; j++)
        if (sameWord(word, results[j])) { duplicate = true; break; }
      if (!duplicate) appendMatch(word, prefix, prefix_len, results, count);
    }
    return count;
  }

private:
  static bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
  static char toLower(char c) { return isUpper(c) ? (char)(c + ('a' - 'A')) : c; }
  static char toUpper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - ('a' - 'A')) : c; }
  static bool isWordChar(char c) {
    c = toLower(c);
    return (c >= 'a' && c <= 'z') || c == '\'';
  }
  static size_t strLength(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
  }
  static bool sameWord(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] && b[i] && toLower(a[i]) == toLower(b[i])) i++;
    return a[i] == '\0' && b[i] == '\0';
  }
  static void appendMatch(const char* word, const char* prefix,
                          size_t prefix_len, char results[][MAX_WORD_LEN],
                          uint8_t& count) {
    if (!startsWith(word, prefix, prefix_len)) return;
    size_t n = strLength(word);
    // An exact match adds nothing and can hide longer, useful completions.
    if (n <= prefix_len) return;
    if (n >= MAX_WORD_LEN) n = MAX_WORD_LEN - 1;
    for (size_t j = 0; j < n; j++) results[count][j] = word[j];
    results[count][n] = '\0';
    if (isUpper(prefix[0])) results[count][0] = toUpper(results[count][0]);
    count++;
  }
  static bool startsWith(const char* word, const char* prefix, size_t prefix_len) {
    for (size_t i = 0; i < prefix_len; i++) {
      if (!word[i] || toLower(word[i]) != toLower(prefix[i])) return false;
    }
    return true;
  }

  static const size_t BASE_WORD_COUNT = zen_dictionary::BASE_WORD_COUNT;
  static const size_t WORD_COUNT = BASE_WORD_COUNT + zen_dictionary::EXTRA_WORD_COUNT;
};

} // namespace zen
