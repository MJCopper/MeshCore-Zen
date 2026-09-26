#pragma once

#include <Arduino.h>

namespace zen {

template <int WordLength, int ListLength>
struct SuggestionPreview {
  char word[WordLength] = {};
  char suffix[WordLength] = {};
  char candidates[ListLength] = {};
  uint32_t revision = UINT32_MAX;
  bool available = false;

  void invalidate() { revision = UINT32_MAX; }
  void clear(uint32_t current_revision) {
    word[0] = suffix[0] = candidates[0] = '\0';
    available = false;
    revision = current_revision;
  }
};

} // namespace zen
