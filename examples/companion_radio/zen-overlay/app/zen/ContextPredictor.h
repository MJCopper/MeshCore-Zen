#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "T9Predictor.h"
#include "ZenBigramData.h"

namespace zen {

// Allocation-free previous-word prediction over the generated compact table.
class ContextPredictor {
  static const char* wordForId(uint16_t id, bool display = false) {
    if (id == zen_context_data::WORD_A) return "a";
    if (id == zen_context_data::WORD_I) return display ? "I" : "i";
    return WordCompleter::wordAt(id);
  }

  static bool isWordChar(char c) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return (c >= 'a' && c <= 'z') || c == '\'';
  }

  static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
  }

  static void canonicalize(char* word, size_t size) {
    struct Alias { const char* contraction; const char* word; };
    static const Alias aliases[] = {
      { "i'm", "am" }, { "i'll", "will" }, { "i've", "have" },
      { "i'd", "would" }, { "you're", "are" }, { "we're", "are" },
      { "they're", "are" }, { "it's", "is" }, { "that's", "is" },
      { "there's", "is" }, { "isn't", "is" }, { "don't", "not" },
      { "can't", "not" }, { "won't", "not" },
    };
    for (const Alias& alias : aliases)
      if (strcmp(word, alias.contraction) == 0) {
        snprintf(word, size, "%s", alias.word);
        return;
      }
  }

  static bool readPrevious(const char* text, size_t before,
                           char* out, size_t out_size, size_t& start,
                           bool& sentence_start) {
    if (!text || !out || out_size < 2) return false;
    size_t end = before;
    while (end > 0 && !isWordChar(text[end - 1])) {
      char c = text[end - 1];
      if (c == '.' || c == '!' || c == '?') {
        sentence_start = true;
        return false;
      }
      end--;
    }
    start = end;
    while (start > 0 && isWordChar(text[start - 1])) start--;
    if (start == end) {
      sentence_start = true;
      return false;
    }
    if (start > 0 && text[start - 1] == '@') {
      sentence_start = true;
      return false;
    }
    size_t length = end - start;
    if (length >= out_size) return false;
    for (size_t i = 0; i < length; i++) out[i] = lower(text[start + i]);
    out[length] = '\0';
    canonicalize(out, out_size);
    return true;
  }

  struct Words {
    char recent[WordCompleter::MAX_WORD_LEN];
    char earlier[WordCompleter::MAX_WORD_LEN];
    bool has_recent;
    bool has_earlier;
    bool sentence_start;
  };

  static Words context(const char* text, size_t current_start) {
    Words words = {};
    size_t recent_start = current_start;
    words.has_recent = readPrevious(text, current_start, words.recent,
                                    sizeof(words.recent), recent_start,
                                    words.sentence_start);
    if (!words.has_recent) return words;
    size_t earlier_start = recent_start;
    bool boundary = false;
    words.has_earlier = readPrevious(text, recent_start, words.earlier,
                                     sizeof(words.earlier), earlier_start,
                                     boundary);
    return words;
  }

  static const zen_context_data::Entry* find(const char* previous) {
    int low = 0;
    int high = (int)zen_context_data::ENTRY_COUNT - 1;
    while (low <= high) {
      int mid = low + (high - low) / 2;
      const char* word = wordForId(zen_context_data::ENTRIES[mid].previous);
      int order = strcmp(previous, word ? word : "");
      if (order == 0) return &zen_context_data::ENTRIES[mid];
      if (order < 0) high = mid - 1;
      else low = mid + 1;
    }
    return nullptr;
  }

  static const zen_context_data::TrigramEntry* findTrigram(
      uint16_t first, uint16_t second) {
    int low = 0;
    int high = (int)zen_context_data::TRIGRAM_COUNT - 1;
    uint32_t wanted = ((uint32_t)first << 16) | second;
    while (low <= high) {
      int mid = low + (high - low) / 2;
      const zen_context_data::TrigramEntry& entry =
          zen_context_data::TRIGRAMS[mid];
      uint32_t key = ((uint32_t)entry.first << 16) | entry.second;
      if (wanted == key) return &entry;
      if (wanted < key) high = mid - 1;
      else low = mid + 1;
    }
    return nullptr;
  }

  static bool startsWith(const char* word, const char* prefix, size_t length) {
    for (size_t i = 0; i < length; i++)
      if (!word[i] || lower(word[i]) != lower(prefix[i])) return false;
    return true;
  }

  static bool append(const char* word,
                     char out[][WordCompleter::MAX_WORD_LEN], uint8_t& count,
                     uint8_t max_results, size_t max_bytes, bool initial_cap) {
    if (!word || !word[0] || strlen(word) > max_bytes || count >= max_results)
      return false;
    for (uint8_t i = 0; i < count; i++)
      if (strcmp(out[i], word) == 0) return false;
    snprintf(out[count], WordCompleter::MAX_WORD_LEN, "%s", word);
    if (initial_cap && out[count][0] >= 'a' && out[count][0] <= 'z')
      out[count][0] -= 'a' - 'A';
    count++;
    return true;
  }

  static uint8_t appendPrefixIds(
      const uint16_t* ids, uint8_t id_count, const char* prefix,
      size_t prefix_len, char out[][WordCompleter::MAX_WORD_LEN],
      uint8_t count, uint8_t max_results, bool initial_cap) {
    for (uint8_t i = 0; i < id_count && count < max_results; i++) {
      if (ids[i] == zen_context_data::NO_WORD) break;
      const char* word = wordForId(ids[i], true);
      if (strlen(word) > prefix_len && startsWith(word, prefix, prefix_len))
        append(word, out, count, max_results,
               WordCompleter::MAX_WORD_LEN - 1, initial_cap);
    }
    return count;
  }

  static uint8_t appendT9Ids(
      const uint16_t* ids, uint8_t id_count, const char* digits,
      size_t digit_count, char out[][WordCompleter::MAX_WORD_LEN],
      uint8_t count, uint8_t max_results, size_t max_bytes) {
    for (uint8_t pass = 0; pass < 2 && count < max_results; pass++) {
      bool exact = pass == 0;
      for (uint8_t i = 0; i < id_count && count < max_results; i++) {
        if (ids[i] == zen_context_data::NO_WORD) break;
        const char* word = wordForId(ids[i], true);
        if ((T9Predictor::digitLength(word) == digit_count) != exact ||
            !T9Predictor::matches(word, digits, digit_count)) continue;
        append(word, out, count, max_results, max_bytes, false);
      }
    }
    return count;
  }

 public:
  static uint8_t suggestPrefix(
      const char* text, size_t current_start, const char* prefix,
      size_t prefix_len, char out[][WordCompleter::MAX_WORD_LEN],
      uint8_t max_results) {
    if (!prefix || prefix_len == 0 || !out || max_results == 0) return 0;
    Words words = context(text, current_start);
    uint8_t count = 0;
    bool initial_cap = prefix[0] >= 'A' && prefix[0] <= 'Z';
    const zen_context_data::Entry* recent = words.has_recent ? find(words.recent) : nullptr;
    if (recent && words.has_earlier) {
      const zen_context_data::Entry* earlier = find(words.earlier);
      const zen_context_data::TrigramEntry* trigram = earlier ?
          findTrigram(earlier->previous, recent->previous) : nullptr;
      if (trigram)
        count = appendPrefixIds(trigram->next,
            zen_context_data::TRIGRAM_SUCCESSOR_COUNT, prefix, prefix_len,
            out, count, max_results, initial_cap);
    }
    if (recent)
      count = appendPrefixIds(recent->next, zen_context_data::SUCCESSOR_COUNT,
          prefix, prefix_len, out, count, max_results, initial_cap);
    if (words.sentence_start)
      count = appendPrefixIds(zen_context_data::SENTENCE_START,
          sizeof(zen_context_data::SENTENCE_START) /
              sizeof(zen_context_data::SENTENCE_START[0]),
          prefix, prefix_len, out, count, max_results, initial_cap);
    return count;
  }

  static uint8_t suggestT9(
      const char* text, size_t current_start, const char* digits,
      size_t digit_count, char out[][WordCompleter::MAX_WORD_LEN],
      uint8_t max_results,
      size_t max_bytes = WordCompleter::MAX_WORD_LEN - 1) {
    if (!digits || digit_count == 0 || !out || max_results == 0) return 0;
    Words words = context(text, current_start);
    uint8_t count = 0;
    const zen_context_data::Entry* recent = words.has_recent ? find(words.recent) : nullptr;
    if (recent && words.has_earlier) {
      const zen_context_data::Entry* earlier = find(words.earlier);
      const zen_context_data::TrigramEntry* trigram = earlier ?
          findTrigram(earlier->previous, recent->previous) : nullptr;
      if (trigram)
        count = appendT9Ids(trigram->next,
            zen_context_data::TRIGRAM_SUCCESSOR_COUNT, digits, digit_count,
            out, count, max_results, max_bytes);
    }
    if (recent)
      count = appendT9Ids(recent->next, zen_context_data::SUCCESSOR_COUNT,
          digits, digit_count, out, count, max_results, max_bytes);
    if (words.sentence_start)
      count = appendT9Ids(zen_context_data::SENTENCE_START,
          sizeof(zen_context_data::SENTENCE_START) /
              sizeof(zen_context_data::SENTENCE_START[0]),
          digits, digit_count, out, count, max_results, max_bytes);
    return count;
  }
};

} // namespace zen
