#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "WordCompleter.h"

namespace zen {

// Allocation-free predictive T9 lookup over the shared conversational
// dictionary. Exact-length and recently selected words lead the underlying
// frequency order. A partial digit sequence also matches the start of longer
// words, allowing the editor to display a useful provisional word after every
// key without storing a second dictionary.
class T9Predictor {
  static const uint8_t RECENT_COUNT = 8;

  static char (*recentWords())[WordCompleter::MAX_WORD_LEN] {
    static char words[RECENT_COUNT][WordCompleter::MAX_WORD_LEN] = {};
    return words;
  }

  static bool sameWord(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
  }

  static bool alreadyAdded(const char* word,
                           char results[][WordCompleter::MAX_WORD_LEN],
                           uint8_t count) {
    for (uint8_t i = 0; i < count; i++)
      if (sameWord(word, results[i])) return true;
    return false;
  }

  static bool appendResult(const char* word,
                           char results[][WordCompleter::MAX_WORD_LEN],
                           uint8_t& count, uint8_t max_results, size_t max_bytes) {
    if (!word || !word[0] || count >= max_results ||
        strlen(word) > max_bytes ||
        alreadyAdded(word, results, count)) return false;
    size_t n = 0;
    while (word[n] && n + 1 < WordCompleter::MAX_WORD_LEN) {
      results[count][n] = word[n];
      n++;
    }
    results[count][n] = '\0';
    count++;
    return true;
  }

public:
  static char digitFor(char letter) {
    if (letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
    if (letter >= 'a' && letter <= 'c') return '2';
    if (letter >= 'd' && letter <= 'f') return '3';
    if (letter >= 'g' && letter <= 'i') return '4';
    if (letter >= 'j' && letter <= 'l') return '5';
    if (letter >= 'm' && letter <= 'o') return '6';
    if (letter >= 'p' && letter <= 's') return '7';
    if (letter >= 't' && letter <= 'v') return '8';
    if (letter >= 'w' && letter <= 'z') return '9';
    return 0;
  }

  static bool matches(const char* word, const char* digits, size_t digit_count) {
    if (!word || !digits || digit_count == 0) return false;
    size_t wi = 0;
    for (size_t di = 0; di < digit_count; di++) {
      // Apostrophes are part of the displayed candidate but consume no key,
      // allowing natural chat contractions such as "I'm" and "don't".
      while (word[wi] && !digitFor(word[wi])) wi++;
      if (!word[wi] || digitFor(word[wi]) != digits[di]) return false;
      wi++;
    }
    return true;
  }

  static size_t digitLength(const char* word) {
    size_t count = 0;
    if (word)
      for (; *word; word++)
        if (digitFor(*word)) count++;
    return count;
  }

  // Number of candidate bytes visible after `digit_count` T9 presses. Any
  // punctuation immediately following the last entered letter is included so
  // "I'm" does not awkwardly render as "I'|m" after both digits are entered.
  static size_t prefixBytes(const char* word, size_t digit_count) {
    if (!word || digit_count == 0) return 0;
    size_t bytes = 0;
    size_t digits = 0;
    while (word[bytes] && digits < digit_count) {
      if (digitFor(word[bytes])) digits++;
      bytes++;
    }
    while (word[bytes] && !digitFor(word[bytes])) bytes++;
    return bytes;
  }

  static void remember(const char* word) {
    if (!word || !word[0]) return;
    char (*recent)[WordCompleter::MAX_WORD_LEN] = recentWords();
    int found = -1;
    for (uint8_t i = 0; i < RECENT_COUNT; i++)
      if (sameWord(word, recent[i])) { found = i; break; }
    if (found < 0) found = RECENT_COUNT - 1;
    for (int i = found; i > 0; i--)
      memcpy(recent[i], recent[i - 1], WordCompleter::MAX_WORD_LEN);
    snprintf(recent[0], WordCompleter::MAX_WORD_LEN, "%s", word);
  }

  static uint8_t suggest(const char* digits, size_t digit_count,
                         char results[][WordCompleter::MAX_WORD_LEN],
                         uint8_t max_results,
                         size_t max_bytes = WordCompleter::MAX_WORD_LEN - 1) {
    if (!digits || digit_count == 0 || !results || max_results == 0) return 0;
    if (max_results > WordCompleter::MAX_SUGGESTIONS)
      max_results = WordCompleter::MAX_SUGGESTIONS;
    uint8_t count = 0;
    static const char* const SINGLE_WORDS[] = { "a", "I" };
    static const char* const CONTRACTIONS[] = {
      "I'm", "I'll", "I've", "I'd", "don't", "can't", "won't", "isn't",
      "it's", "you're", "you've", "we're", "they're", "that's", "there's"
    };

    // Exact-length words feel like classic T9; longer prefix completions are
    // useful, but should never displace the word the entered digits spell.
    for (uint8_t pass = 0; pass < 2 && count < max_results; pass++) {
      bool exact = pass == 0;
      char (*recent)[WordCompleter::MAX_WORD_LEN] = recentWords();
      for (uint8_t i = 0; i < RECENT_COUNT && count < max_results; i++) {
        if (!recent[i][0] || (digitLength(recent[i]) == digit_count) != exact ||
            !matches(recent[i], digits, digit_count)) continue;
        appendResult(recent[i], results, count, max_results, max_bytes);
      }
      for (size_t i = 0; i < sizeof(SINGLE_WORDS) / sizeof(SINGLE_WORDS[0]) &&
                         count < max_results; i++) {
        const char* word = SINGLE_WORDS[i];
        if ((digitLength(word) == digit_count) != exact ||
            !matches(word, digits, digit_count)) continue;
        appendResult(word, results, count, max_results, max_bytes);
      }
      for (size_t i = 0; i < sizeof(CONTRACTIONS) / sizeof(CONTRACTIONS[0]) &&
                         count < max_results; i++) {
        const char* word = CONTRACTIONS[i];
        if ((digitLength(word) == digit_count) != exact ||
            !matches(word, digits, digit_count)) continue;
        appendResult(word, results, count, max_results, max_bytes);
      }
      for (size_t i = 0; i < WordCompleter::dictionarySize() && count < max_results; i++) {
        const char* word = WordCompleter::wordAt(i);
        if ((digitLength(word) == digit_count) != exact ||
            !matches(word, digits, digit_count)) continue;
        appendResult(word, results, count, max_results, max_bytes);
      }
    }
    return count;
  }
};

} // namespace zen
