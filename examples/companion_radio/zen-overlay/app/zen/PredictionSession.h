#pragma once

#include <Arduino.h>
#include "WordCompleter.h"
#include "T9Predictor.h"
#include "ContextPredictor.h"

namespace zen {

using T9SuggestionProvider = uint8_t (*)(const char*, size_t,
    char[][WordCompleter::MAX_WORD_LEN], uint8_t, size_t);

// Per-editor prediction cache. It contains no UI or text-buffer state and can
// therefore be invalidated solely when its explicit inputs change.
class PredictionSession {
  char _digits[WordCompleter::MAX_WORD_LEN] = {};
  char _candidates[WordCompleter::MAX_SUGGESTIONS]
                  [WordCompleter::MAX_WORD_LEN] = {};
  uint8_t _count = 0;
  int _capacity = -1;
  bool _caps = false;
  bool _valid = false;

 public:
  void invalidate() {
    _valid = false;
    _count = 0;
    _digits[0] = '\0';
  }

  uint8_t suggest(const char* text, size_t word_start,
                  const char* digits, size_t digit_count,
                  bool use_context, bool capitalize, int capacity,
                  T9SuggestionProvider provider,
                  char out[][WordCompleter::MAX_WORD_LEN], uint8_t max_results) {
    if (max_results > WordCompleter::MAX_SUGGESTIONS)
      max_results = WordCompleter::MAX_SUGGESTIONS;
    if (!_valid || _capacity != capacity || _caps != capitalize ||
        strcmp(_digits, digits ? digits : "") != 0) {
      char ranked[WordCompleter::MAX_SUGGESTIONS][WordCompleter::MAX_WORD_LEN] = {};
      _count = use_context ? ContextPredictor::suggestT9(
          text, word_start, digits, digit_count, ranked,
          WordCompleter::MAX_SUGGESTIONS, capacity > 0 ? (size_t)capacity : 0) : 0;

      char fallback[WordCompleter::MAX_SUGGESTIONS][WordCompleter::MAX_WORD_LEN] = {};
      uint8_t fallback_count = provider ? provider(
          digits, digit_count, fallback, WordCompleter::MAX_SUGGESTIONS,
          capacity > 0 ? (size_t)capacity : 0) : T9Predictor::suggest(
          digits, digit_count, fallback, WordCompleter::MAX_SUGGESTIONS,
          capacity > 0 ? (size_t)capacity : 0);
      for (uint8_t i = 0; i < fallback_count &&
                          _count < WordCompleter::MAX_SUGGESTIONS; i++) {
        bool duplicate = false;
        for (uint8_t j = 0; j < _count; j++) {
          if (strcmp(ranked[j], fallback[i]) == 0) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate)
          memcpy(ranked[_count++], fallback[i], WordCompleter::MAX_WORD_LEN);
      }

      for (uint8_t i = 0; i < _count; i++) {
        snprintf(_candidates[i], WordCompleter::MAX_WORD_LEN, "%s", ranked[i]);
        if (capitalize && _candidates[i][0] >= 'a' && _candidates[i][0] <= 'z')
          _candidates[i][0] -= 'a' - 'A';
      }
      snprintf(_digits, sizeof(_digits), "%s", digits ? digits : "");
      _capacity = capacity;
      _caps = capitalize;
      _valid = true;
    }

    uint8_t count = _count < max_results ? _count : max_results;
    for (uint8_t i = 0; i < count; i++)
      memcpy(out[i], _candidates[i], WordCompleter::MAX_WORD_LEN);
    return count;
  }
};

} // namespace zen
