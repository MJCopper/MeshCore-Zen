#pragma once

#include <Arduino.h>

namespace zen {

template <int MaxItems, int ItemLength>
struct SuggestionModel {
  char items[MaxItems][ItemLength] = {};
  int count = 0;
  int replace_start = 0;
  int replace_end = 0;
  bool range_set = false;
  bool append_space = false;
  const char* title = "Placeholder:";

  void reset(int cursor) {
    count = 0;
    replace_start = replace_end = cursor;
    range_set = false;
    append_space = false;
    title = "Placeholder:";
  }

  void clear() { count = 0; }

  bool add(const char* value) {
    if (!value || count >= MaxItems) return false;
    snprintf(items[count], ItemLength, "%s", value);
    count++;
    return true;
  }

  void setRange(int start, int end, int text_length) {
    replace_start = start < 0 ? 0 : (start > text_length ? text_length : start);
    replace_end = end < replace_start ? replace_start :
        (end > text_length ? text_length : end);
    range_set = true;
  }
};

} // namespace zen
