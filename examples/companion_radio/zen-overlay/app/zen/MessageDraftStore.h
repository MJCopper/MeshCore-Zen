#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace zen {

// RAM-only drafts for the most recently edited conversations. Contact prefixes
// match the four-byte identity used by Zen's other compact per-contact tables;
// channel drafts use the configured channel slot. Oldest entries are evicted.
class MessageDraftStore {
 public:
  static const uint8_t SLOT_COUNT = 8;
  static const uint8_t CONTACT_PREFIX_LEN = 4;
  static const uint16_t TEXT_CAPACITY = 161;

  enum Kind : uint8_t { CONTACT, CHANNEL };

  struct Key {
    Kind kind;
    uint8_t id[CONTACT_PREFIX_LEN];
  };

  MessageDraftStore() : _clock(0) { memset(_slots, 0, sizeof(_slots)); }

  bool loadContact(const uint8_t* key, char* out, size_t out_size,
                   uint8_t* reply_prefix_len = nullptr) {
    return load(makeContactKey(key), out, out_size, reply_prefix_len);
  }
  bool loadChannel(uint8_t channel, char* out, size_t out_size,
                   uint8_t* reply_prefix_len = nullptr) {
    return load(makeChannelKey(channel), out, out_size, reply_prefix_len);
  }
  void saveContact(const uint8_t* key, const char* text,
                   uint8_t reply_prefix_len = 0) {
    save(makeContactKey(key), text, reply_prefix_len);
  }
  void saveChannel(uint8_t channel, const char* text,
                   uint8_t reply_prefix_len = 0) {
    save(makeChannelKey(channel), text, reply_prefix_len);
  }
  void clearContact(const uint8_t* key) { clear(makeContactKey(key)); }
  void clearChannel(uint8_t channel) { clear(makeChannelKey(channel)); }

 private:
  struct Slot {
    Key key;
    char text[TEXT_CAPACITY];
    uint32_t touched;
    uint8_t reply_prefix_len;
    bool used;
  };

  Slot _slots[SLOT_COUNT];
  uint32_t _clock;

  static Key makeContactKey(const uint8_t* key) {
    Key result = { CONTACT, { 0, 0, 0, 0 } };
    if (key) memcpy(result.id, key, CONTACT_PREFIX_LEN);
    return result;
  }
  static Key makeChannelKey(uint8_t channel) {
    Key result = { CHANNEL, { channel, 0, 0, 0 } };
    return result;
  }
  static bool matches(const Key& a, const Key& b) {
    return a.kind == b.kind && memcmp(a.id, b.id, sizeof(a.id)) == 0;
  }
  int find(const Key& key) const {
    for (uint8_t i = 0; i < SLOT_COUNT; i++)
      if (_slots[i].used && matches(_slots[i].key, key)) return i;
    return -1;
  }
  int replacement() const {
    int oldest = 0;
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
      if (!_slots[i].used) return i;
      if (_slots[i].touched < _slots[oldest].touched) oldest = i;
    }
    return oldest;
  }
  bool load(const Key& key, char* out, size_t out_size,
            uint8_t* reply_prefix_len) {
    if (!out || out_size == 0) return false;
    int index = find(key);
    if (index < 0) { out[0] = '\0'; return false; }
    strncpy(out, _slots[index].text, out_size - 1);
    out[out_size - 1] = '\0';
    if (reply_prefix_len) *reply_prefix_len = _slots[index].reply_prefix_len;
    _slots[index].touched = ++_clock;
    return true;
  }
  void save(const Key& key, const char* text, uint8_t reply_prefix_len) {
    if (!text || !text[0]) { clear(key); return; }
    int index = find(key);
    if (index < 0) index = replacement();
    Slot& slot = _slots[index];
    slot.key = key;
    strncpy(slot.text, text, sizeof(slot.text) - 1);
    slot.text[sizeof(slot.text) - 1] = '\0';
    slot.reply_prefix_len = reply_prefix_len < strlen(slot.text)
        ? reply_prefix_len : 0;
    slot.touched = ++_clock;
    slot.used = true;
  }
  void clear(const Key& key) {
    int index = find(key);
    if (index >= 0) _slots[index].used = false;
  }
};

} // namespace zen
