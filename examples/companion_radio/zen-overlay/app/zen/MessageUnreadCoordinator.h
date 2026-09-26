#pragma once

#include <stdint.h>
#include <string.h>

namespace zen {

// Single RAM-only owner of direct-message and room unread state. Conversation
// storage supplies the number of retained rows so counters never advertise
// messages that have already fallen out of the bounded history ring.
class MessageUnreadCoordinator {
public:
  enum Kind : uint8_t { DIRECT, ROOM };
  struct Entry { uint8_t prefix[4]; uint8_t count; uint8_t seen; };
  static const uint8_t DIRECT_CAPACITY = 16;
  static const uint8_t ROOM_CAPACITY = 32;

private:
  Entry _direct[DIRECT_CAPACITY]{};
  Entry _room[ROOM_CAPACITY]{};

  Entry* table(Kind kind) { return kind == DIRECT ? _direct : _room; }
  const Entry* table(Kind kind) const { return kind == DIRECT ? _direct : _room; }
  uint8_t capacity(Kind kind) const {
    return kind == DIRECT ? DIRECT_CAPACITY : ROOM_CAPACITY;
  }

public:
  void reset() { memset(_direct, 0, sizeof(_direct)); memset(_room, 0, sizeof(_room)); }

  void record(Kind kind, const uint8_t* pub_key, bool viewing) {
    if (!pub_key || viewing) return;
    Entry* entries = table(kind);
    int match = -1, empty = -1, reclaim = -1;
    for (uint8_t i = 0; i < capacity(kind); i++) {
      if (entries[i].seen && !memcmp(entries[i].prefix, pub_key, 4)) { match = i; break; }
      if (empty < 0 && !entries[i].seen) empty = i;
      if (reclaim < 0 && entries[i].seen && entries[i].count == 0) reclaim = i;
    }
    if (match >= 0) {
      if (entries[match].count < 99) entries[match].count++;
      return;
    }
    int slot = empty >= 0 ? empty : reclaim;
    if (slot < 0) return;
    memcpy(entries[slot].prefix, pub_key, 4);
    entries[slot].count = 1;
    entries[slot].seen = 1;
  }

  uint8_t get(Kind kind, const uint8_t* pub_key, int retained) const {
    if (!pub_key || retained <= 0) return 0;
    const Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++) {
      if (!entries[i].seen || memcmp(entries[i].prefix, pub_key, 4)) continue;
      return entries[i].count < retained ? entries[i].count : (uint8_t)retained;
    }
    return 0;
  }

  bool has(Kind kind, const uint8_t* pub_key) const {
    if (!pub_key) return false;
    const Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++)
      if (entries[i].seen && !memcmp(entries[i].prefix, pub_key, 4)) return true;
    return false;
  }

  void clear(Kind kind, const uint8_t* pub_key) {
    if (!pub_key) return;
    Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++)
      if (entries[i].seen && !memcmp(entries[i].prefix, pub_key, 4)) {
        entries[i].count = 0;
        return;
      }
  }

  void clearAll(Kind kind) {
    Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++) entries[i].count = 0;
  }

  void forget(Kind kind, const uint8_t* pub_key) {
    if (!pub_key) return;
    Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++)
      if (entries[i].seen && !memcmp(entries[i].prefix, pub_key, 4)) {
        memset(&entries[i], 0, sizeof(entries[i]));
        return;
      }
  }

  template<typename RetainedFn>
  int total(Kind kind, RetainedFn retained) const {
    int result = 0;
    const Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++) {
      if (!entries[i].seen || entries[i].count == 0) continue;
      int held = retained(entries[i].prefix);
      result += entries[i].count < held ? entries[i].count : held;
    }
    return result;
  }

  template<typename RetainedFn>
  void reconcile(Kind kind, RetainedFn retained) {
    Entry* entries = table(kind);
    for (uint8_t i = 0; i < capacity(kind); i++)
      if (entries[i].seen && entries[i].count && retained(entries[i].prefix) == 0)
        memset(&entries[i], 0, sizeof(entries[i]));
  }
};

} // namespace zen
