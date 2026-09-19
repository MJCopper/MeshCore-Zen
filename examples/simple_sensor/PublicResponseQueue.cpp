#include "PublicResponseQueue.h"

#include <string.h>

static const char* TRUNCATED = "\n...truncated";

// Prefer complete lines, then a word boundary for an unusually long line.
static size_t findCut(const char* text, size_t limit) {
  size_t cut = 0;
  for (size_t i = 0; i < limit && text[i]; i++)
    if (text[i] == '\n') cut = i;
  if (cut) return cut;
  for (size_t i = 0; i < limit && text[i]; i++)
    if (text[i] == ' ') cut = i;
  return cut ? cut : limit;
}

static const char* skipBreak(const char* text) {
  while (*text == '\n' || *text == ' ') text++;
  return text;
}

PublicResponseQueue::PublicResponseQueue() {
  memset(_pending, 0, sizeof(_pending));
}

bool PublicResponseQueue::split(const char* response, size_t max_body,
                                char first[MAX_PART_LENGTH + 1],
                                char second[MAX_PART_LENGTH + 1]) {
  first[0] = second[0] = 0;
  if (!response || max_body < 4 + strlen(TRUNCATED)) return false;
  if (max_body > MAX_PART_LENGTH) max_body = MAX_PART_LENGTH;

  size_t length = strlen(response);
  if (length <= max_body) {
    memcpy(first, response, length + 1);
    return false;
  }

  const size_t content_limit = max_body - 4;  // "1/2\n" and "2/2\n"
  size_t first_len = findCut(response, content_limit);
  memcpy(first, "1/2\n", 4);
  memcpy(first + 4, response, first_len);
  first[4 + first_len] = 0;

  const char* remaining = skipBreak(response + first_len);
  memcpy(second, "2/2\n", 4);
  size_t remaining_len = strlen(remaining);
  if (remaining_len <= content_limit) {
    memcpy(second + 4, remaining, remaining_len + 1);
  } else {
    size_t marker_len = strlen(TRUNCATED);
    size_t second_len = 0;
    for (size_t i = 0; i < content_limit - marker_len && remaining[i]; i++)
      if (remaining[i] == '\n') second_len = i;
    memcpy(second + 4, remaining, second_len);
    const char* marker = second_len ? TRUNCATED : TRUNCATED + 1;
    strcpy(second + 4 + second_len, marker);
  }
  return true;
}

bool PublicResponseQueue::schedule(const char* second, uint32_t due_at) {
  if (!second || !second[0] || strlen(second) > MAX_PART_LENGTH) return false;
  for (uint8_t i = 0; i < MAX_PENDING; i++) {
    if (_pending[i].active) continue;
    strcpy(_pending[i].text, second);
    _pending[i].due_at = due_at;
    _pending[i].active = true;
    return true;
  }
  return false;
}

bool PublicResponseQueue::takeDue(uint32_t now_millis, char part[MAX_PART_LENGTH + 1]) {
  for (uint8_t i = 0; i < MAX_PENDING; i++) {
    if (!_pending[i].active || (int32_t)(now_millis - _pending[i].due_at) < 0) continue;
    strcpy(part, _pending[i].text);
    _pending[i].active = false;
    return true;
  }
  return false;
}
