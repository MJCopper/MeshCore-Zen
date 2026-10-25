#pragma once

#include "PublicResponseQueue.h"
#include "TraceTimingPolicy.h"
#include <stdio.h>
#include <string.h>

// Owns immutable report text, never the dispatcher's packet allocation.
class TraceReportDelivery {
public:
  enum Outcome { NONE, DELIVERED, EXPIRED, UNCERTAIN, INVALID, CANCELLED };
  bool begin(const char* text, const char* name, const SensorReplyContext& context, uint32_t now,
             uint32_t delay, uint32_t budget, bool failure = false) {
    reset();
    size_t prefix = strlen(name) + 2;
    if (prefix >= PublicResponseQueue::MAX_PART_LENGTH - 16) { _outcome = INVALID; return false; }
    char first[PublicResponseQueue::MAX_PART_LENGTH + 1], second[PublicResponseQueue::MAX_PART_LENGTH + 1];
    _count = PublicResponseQueue::split(text, PublicResponseQueue::MAX_PART_LENGTH - prefix, first, second) ? 2 : 1;
    snprintf(_parts[0], sizeof(_parts[0]), "%s: %s", name, first);
    if (_count == 2) snprintf(_parts[1], sizeof(_parts[1]), "%s: %s", name, second);
    _context = context;
    _began = _part_at = now;
    _due = now + delay;
    _budget = budget;
    _failure = failure;
    _state = WAITING;
    return true;
  }
  void reset() { _state = IDLE; _packet = NULL; _part = 0; _failures = 0; _outcome = NONE; }
  uint32_t due() const { return _due; }
  bool active() const { return _state != IDLE; }
  bool inFlight() const { return _state == TRANSMITTING; }
  bool failure() const { return _failure; }
  bool ready(uint32_t now) const { return _state == WAITING && (int32_t)(now - _due) >= 0; }
  bool waiting() const { return _state == WAITING; }
  uint32_t delay(uint32_t now) const { return (int32_t)(_due - now) > 0 ? _due - now : 0; }
  bool expired(uint32_t now) const {
    if (!active()) return false;
    if (inFlight()) {
      // Do not free the radio's packet. Stop observing a stalled report after
      // the Dispatcher watchdog plus scheduling grace, even if its callback is lost.
      int32_t age = (int32_t)(now - _tx_at);
      return age >= 0 && (uint32_t)age >= _tx_budget;
    }
    int32_t age = (int32_t)(now - _began);
    return age >= 0 && ((uint32_t)age >= _budget ||
        (int32_t)(now - _part_at) >= (int32_t)TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS);
  }
  const char* text() const { return _parts[_part < 2 ? _part : 1]; }
  uint8_t channel() const { return _context.channel; }
  const SensorReplyContext& context() const { return _context; }
  const void* queuedPacket() const { return _state == QUEUED ? _packet : NULL; }
  void queued(const void* packet, uint32_t tx_budget) { _packet = packet; _tx_budget = tx_budget; _state = QUEUED; }
  void retry(uint32_t now, uint32_t jitter = 0) {
    _packet = NULL;
    _state = WAITING;
    uint32_t next = now + TraceTimingPolicy::localBackoff(_failures) + jitter;
    if (_failures < 255) _failures++;
    if ((int32_t)(next - _due) > 0) _due = next;
  }
  bool started(const void* packet, uint32_t now) {
    if (_state != QUEUED || !packet || packet != _packet) return false;
    _tx_at = now;
    _state = TRANSMITTING;
    return true;
  }
  bool completed(const void* packet, uint32_t now) {
    if (_state != TRANSMITTING || packet != _packet) return false;
    _packet = NULL;
    if (++_part == _count) finish(DELIVERED);
    else {
      _state = WAITING;
      _failures = 0;
      _due = now + 3000;
      _part_at = _due;  // The inter-part gap is not queue admission time.
    }
    return true;
  }
  bool failed(const void* packet, uint32_t now, bool definitely_not_sent = false) {
    if (!active() || !packet || packet != _packet) return false;
    if (inFlight() || !definitely_not_sent) finish(UNCERTAIN);  // Never repeat a possible emission.
    else retry(now);
    return true;
  }
  void expire(bool transmission_uncertain = false) { finish(transmission_uncertain ? UNCERTAIN : EXPIRED); }
  void cancel() { finish(CANCELLED); }
  Outcome outcome() const { return _outcome; }
private:
  enum State { IDLE, WAITING, QUEUED, TRANSMITTING };
  char _parts[2][PublicResponseQueue::MAX_PART_LENGTH + 1]{};
  const void* _packet = NULL;
  uint32_t _tx_at = 0, _tx_budget = 0;
  uint32_t _began = 0, _part_at = 0, _due = 0, _budget = 0;
  SensorReplyContext _context;
  uint8_t _part = 0, _count = 0, _failures = 0;
  bool _failure = false;
  State _state = IDLE;
  Outcome _outcome = NONE;
  void finish(Outcome outcome) { _state = IDLE; _packet = NULL; _outcome = outcome; }
};
