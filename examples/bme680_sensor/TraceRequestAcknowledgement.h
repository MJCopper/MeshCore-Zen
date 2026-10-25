#pragma once
#include "TraceTimingPolicy.h"
#include "PublicResponseQueue.h"
#include <helpers/UTF8Helpers.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Independent of the result report. Owns observation, never radio allocation.
class TraceRequestAcknowledgement {
public:
  bool begin(uint32_t now, uint32_t seconds, uint32_t watchdog, const char* name = nullptr,
             const char* destination = nullptr, const char* fallback = nullptr) {
    cancel();
    char suffix[64];
    int tail = snprintf(suffix, sizeof(suffix), ", please wait up to %lu seconds.", (unsigned long)seconds);
    int head = snprintf(_text, sizeof(_text), "%s%sTrace requested%s",
                        name ? name : "", name ? ": " : "", destination ? " to " : "");
    if (head < 0 || tail < 0 || head + tail > PublicResponseQueue::MAX_PART_LENGTH) return false;
    size_t length = 0;
    if (destination) {
      size_t available = PublicResponseQueue::MAX_PART_LENGTH - head - tail;
      length = mesh::validUtf8PrefixLength(destination, available);
      while (length && destination[length - 1] == ' ') --length;
      if (!length) {
        destination = fallback;
        if (!destination || !destination[0] || strlen(destination) > available) return false;
        length = mesh::validUtf8PrefixLength(destination, available);
        if (length != strlen(destination)) return false;
      }
      memcpy(_text + head, destination, length);
    }
    memcpy(_text + head + length, suffix, tail + 1);
    _state = WAITING; _began = _due = now; _watchdog = _tx_budget = watchdog;
    return true;
  }
  uint32_t due() const { return _due; }
  bool active() const { return _state != IDLE; }
  bool ready(uint32_t now) const { return _state == WAITING && (int32_t)(now - _due) >= 0; }
  bool inFlight() const { return _state == TRANSMITTING; }
  const char* text() const { return _text; }
  const void* queuedPacket() const { return _state == QUEUED ? _packet : nullptr; }
  bool expired(uint32_t now) const {
    return active() && (uint32_t)(now - (inFlight() ? _started : _began)) >=
        (inFlight() ? _tx_budget : TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS);
  }
  bool queued(const void* packet, uint32_t watchdog = 0) {
    if (!packet || watchdog > _watchdog) return false;
    _tx_budget = watchdog ? watchdog : _watchdog;
    _packet = packet; _state = QUEUED; return true;
  }
  void retry(uint32_t now) {
    _packet = nullptr; _state = WAITING;
    _due = now + TraceTimingPolicy::localBackoff(_failures);
    if (_failures < 255) ++_failures;
  }
  bool started(const void* packet, uint32_t now) {
    if (!packet || _state != QUEUED || packet != _packet) return false;
    _started = now; _state = TRANSMITTING; return true;
  }
  void completed(const void* packet) { if (packet && inFlight() && packet == _packet) cancel(); }
  void failed(const void* packet, uint32_t now, bool definitely_not_sent) {
    if (!packet || packet != _packet || !active()) return;
    if (inFlight() || !definitely_not_sent) cancel();
    else retry(now);
  }
  void cancel() { _packet = nullptr; _state = IDLE; _failures = 0; }
private:
  enum State { IDLE, WAITING, QUEUED, TRANSMITTING } _state = IDLE;
  const void* _packet = nullptr;
  char _text[PublicResponseQueue::MAX_PART_LENGTH + 1]{};
  uint32_t _began = 0, _due = 0, _started = 0, _watchdog = 0, _tx_budget = 0;
  uint8_t _failures = 0;
};
