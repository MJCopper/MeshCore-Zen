#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

namespace zen {

// RAM-only facts at the Zen/MeshCore boundary. Payloads, credentials and
// private key material are deliberately never recorded.
class TransportTrace {
public:
  enum Event : uint8_t {
    DM_QUEUED, LOGIN_QUEUED, TELEMETRY_QUEUED, ADMIN_QUEUED,
    ACK_RECEIVED, MESSAGE_RECEIVED, RESPONSE_RECEIVED, ADMIN_RECEIVED,
    PATH_RECEIVED, SEND_REJECTED, TX_COMPLETE, TX_FAILED
  };
  struct Entry {
    uint32_t occurred_ms;
    uint8_t event;
    uint8_t detail;
    uint8_t key[4];
  };
  enum : uint8_t { CAPACITY = 16 };

private:
  Entry _entries[CAPACITY]{};
  uint8_t _head = 0;
  uint8_t _size = 0;

public:
  void add(Event event, const uint8_t* key = nullptr, uint8_t detail = 0) {
    Entry& entry = _entries[_head];
    entry.occurred_ms = millis();
    entry.event = event;
    entry.detail = detail;
    if (key) memcpy(entry.key, key, sizeof(entry.key));
    else memset(entry.key, 0, sizeof(entry.key));
    _head = (_head + 1) % CAPACITY;
    if (_size < CAPACITY) _size++;
  }
  uint8_t size() const { return _size; }
  void clear() { memset(_entries, 0, sizeof(_entries)); _head = _size = 0; }
  const Entry* newest(uint8_t index) const {
    if (index >= _size) return nullptr;
    return &_entries[(_head + CAPACITY - 1 - index) % CAPACITY];
  }
  static const char* name(Event event) {
    switch (event) {
      case DM_QUEUED: return "DM queued";
      case LOGIN_QUEUED: return "Login queued";
      case TELEMETRY_QUEUED: return "Telem queued";
      case ADMIN_QUEUED: return "Admin queued";
      case ACK_RECEIVED: return "ACK received";
      case MESSAGE_RECEIVED: return "DM received";
      case RESPONSE_RECEIVED: return "Response rx";
      case ADMIN_RECEIVED: return "Admin rx";
      case PATH_RECEIVED: return "Path received";
      case SEND_REJECTED: return "Send rejected";
      case TX_COMPLETE: return "Radio TX ok";
      default: return "Radio TX fail";
    }
  }
};

} // namespace zen
