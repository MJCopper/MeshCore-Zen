#pragma once

#include <Arduino.h>
#include "../Features.h"
#if ZEN_FEATURE_AUTOCOMPLETE
#include "PredictionSession.h"
#endif

namespace zen {

// Active predictive-word state, separate from the editor buffer and renderer.
struct KeyboardPredictionState {
  bool _t9_no_match = false;
  bool _t9_capacity_limited = false;
#if ZEN_FEATURE_AUTOCOMPLETE
  using T9SuggestFn = T9SuggestionProvider;
  T9SuggestFn _t9_suggest = nullptr;
  bool _predictive_t9_enabled = false;
  bool _context_prediction_enabled = false;
  bool _t9_literal_mode = false;
  char _t9_digits[WordCompleter::MAX_WORD_LEN] = {};
  char _t9_candidate[WordCompleter::MAX_WORD_LEN] = {};
  uint8_t _t9_digit_count = 0;
  int _t9_word_start = 0;
  int _t9_word_end = 0;
  bool _t9_predict_caps = false;
  PredictionSession _prediction;
  bool _ph_t9_actions = false;
#endif
};

} // namespace zen
