#pragma once

#include "KeyboardWidget.h"
#if ZEN_FEATURE_AUTOCOMPLETE
#include "../zen/CliCompleter.h"
#endif

// Opt-in console adapter. KeyboardWidget keeps the normal layout and input
// controls; only prediction/completion vocabulary changes for this editor.
namespace consoleeditor {

#if ZEN_FEATURE_AUTOCOMPLETE
inline uint8_t candidates(const KeyboardWidget& kb,
                           char out[][zen::WordCompleter::MAX_WORD_LEN]) {
  auto range = zen::CliCompleter::currentToken(kb.buf, kb.len, kb.cursor_pos);
  size_t capacity = kb.max_len - (kb.len - (range.end - range.start));
  return zen::CliCompleter::suggest(kb.buf + range.start, kb.cursor_pos - range.start,
      out, zen::WordCompleter::MAX_SUGGESTIONS, capacity);
}

inline void refresh(KeyboardWidget& kb, void*) {
  auto range = zen::CliCompleter::currentToken(kb.buf, kb.len, kb.cursor_pos);
  kb.clearPlaceholders();
  kb.setCompletionRange(range.start, range.end);
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN];
  uint8_t count = candidates(kb, matches);
  for (uint8_t i = 0; i < count; i++) kb.addPlaceholder(matches[i]);
}

inline bool preview(const KeyboardWidget& kb, void*, char* word, size_t word_size,
                    char* suffix, size_t suffix_size, char* list, size_t list_size) {
  if (!word_size || !suffix_size || !list_size) return false;
  word[0] = suffix[0] = list[0] = 0;
  if (kb.cursor_pos != kb.len) return false;
  auto range = zen::CliCompleter::currentToken(kb.buf, kb.len, kb.cursor_pos);
  size_t prefix_len = kb.cursor_pos - range.start;
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN];
  uint8_t count = candidates(kb, matches);
  if (!count) return false;
  snprintf(word, word_size, "%s", matches[0]);
  snprintf(suffix, suffix_size, "%s", matches[0] + prefix_len);
  size_t used = 0;
  for (uint8_t i = 0; i < count && used + 1 < list_size; i++) {
    int n = snprintf(list + used, list_size - used, "%s%s", i ? ", " : "", matches[i]);
    if (n < 0 || (size_t)n >= list_size - used) break;
    used += n;
  }
  return true;
}
#endif

inline void enable(KeyboardWidget& kb) {
  kb.configureProfile(zen::EditorProfile::CONSOLE);
#if ZEN_FEATURE_AUTOCOMPLETE
  // The caller may begin a literal field before deciding it is a console.
  // Set the profile-owned feature policy here without clearing its text.
  kb.setT9Provider(zen::CliCompleter::suggestT9);
  kb.setPredictiveT9(true);
  kb.setPlaceholderRefresh(refresh, nullptr, "Complete:", true);
  kb.setCompletionPreview(preview, nullptr);
#else
  (void)kb;
#endif
}

} // namespace consoleeditor
