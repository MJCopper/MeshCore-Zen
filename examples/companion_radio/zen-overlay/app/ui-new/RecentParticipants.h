#pragma once

#include <Arduino.h>

// Small RAM-only set used by channel and room reply pickers. Callers scan the
// current transcript newest-first; add() preserves that order while removing
// duplicates. Nothing here is persisted or shared between conversations.
class RecentParticipants {
public:
  static const int MAX_PARTICIPANTS = 6;
  static const int NAME_BYTES = 32;

private:
  char _names[MAX_PARTICIPANTS][NAME_BYTES];
  int  _count = 0;

public:
  void clear() { _count = 0; }
  int count() const { return _count; }
  const char* name(int index) const {
    return (index >= 0 && index < _count) ? _names[index] : "";
  }

  bool add(const char* name, int length = -1) {
    if (!name || _count >= MAX_PARTICIPANTS) return false;
    if (length < 0) length = strlen(name);
    while (length > 0 && name[length - 1] == ' ') length--;
    while (length > 0 && *name == ' ') { name++; length--; }
    if (length <= 0 || (length == 2 && strncmp(name, "Me", 2) == 0)) return false;
    for (int i = 0; i < length; i++) {
      uint8_t c = (uint8_t)name[i];
      if (c < 0x20 || c == ']') return false; // invalid in the @[...] wire prefix
    }
    if (length >= NAME_BYTES) length = NAME_BYTES - 1;

    for (int i = 0; i < _count; i++) {
      if ((int)strlen(_names[i]) == length && strncmp(_names[i], name, length) == 0)
        return false;
    }
    memcpy(_names[_count], name, length);
    _names[_count][length] = '\0';
    _count++;
    return true;
  }
};
