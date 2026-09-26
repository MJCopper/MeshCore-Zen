#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>

namespace zen {

// MeshCore echoes a two-character CLI prefix followed by '|'. Match it and
// the full identity before dispatching a reply. App traffic still has priority.
class AdminSession {
  uint8_t _key[32]{};
  bool _authorized = false;
  bool _pending = false;
  bool _quarantined = false;
  uint32_t _deadline = 0;
  uint32_t _quiet_until = 0;
  uint16_t _issued = 0;
  char _tag[3]{};

public:
  static const uint32_t DRAIN_MS = 60000;
  static const size_t TEXT_LIMIT = 157;  // 160 wire bytes less the echoed prefix
  const uint8_t* key() const { return _key; }
  bool pending() const { return _pending; }
  bool matches(const uint8_t* key) const { return memcmp(key, _key, sizeof(_key)) == 0; }
  void authorize(const uint8_t* key) {
    memcpy(_key, key, sizeof(_key));
    _authorized = true;
  }
  bool ready(const uint8_t* key, uint32_t now) const {
    return _authorized && matches(key) && !_pending && _issued < 4096 &&
           (!_quarantined || (int32_t)(now - _quiet_until) >= 0);
  }
  void begin(uint32_t now, uint32_t timeout) { _pending = true; _deadline = now + timeout; }
  bool formatRequest(char* out, size_t size, const char* command) {
    if (_issued >= 4096 || !command[0] || strlen(command) > TEXT_LIMIT) return false;
    static const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    _tag[0] = ALPHABET[_issued / 64];
    _tag[1] = ALPHABET[_issued % 64];
    _tag[2] = '|';
    _issued++;  // never reuse an ID during this boot, including failed sends
    int n = snprintf(out, size, "%c%c|%s", _tag[0], _tag[1], command);
    return n >= 0 && (size_t)n < size;
  }
  bool expired(uint32_t now) const { return _pending && (int32_t)(now - _deadline) >= 0; }
  bool complete(const uint8_t* key, const char* reply, uint32_t now) {
    if (!_pending || !matches(key) || expired(now) || strlen(reply) < 3 || memcmp(reply, _tag, 3)) return false;
    _pending = false;
    return true;
  }
  void cancel(uint32_t now) {
    (void)now;
    // Each local request uses a never-reused tag, so a late reply cannot
    // complete a later request. Local cancellation therefore needs no drain
    // period; app traffic still establishes one in appCommand().
    _pending = false;
  }
  void close(uint32_t now) { cancel(now); _authorized = false; }
  void appCommand(uint32_t now, uint32_t timeout) {
    cancel(now);
    _quarantined = true;
    _quiet_until = now + timeout + DRAIN_MS;
  }
};

} // namespace zen
