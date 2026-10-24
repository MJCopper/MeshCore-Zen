#pragma once

#include "TraceRoute.h"
#include "TraceTimingPolicy.h"

namespace zen {
// One bounded transaction, retaining both transmitted attempts until final commit.
class RepeaterTraceProbe {
public:
  enum StartResult { STARTED, BUSY, NO_REPEATERS, TOO_LONG, INVALID_PATH };
  enum ReceiveResult { IGNORED, STORED, COMPLETE };
  enum Failure { NO_RESULT, RADIO_BUSY, TRANSMIT_FAILED, RETRY_NOT_SENT };
  struct Result {
    uint32_t elapsed_millis;
    TraceRoute route;
    uint8_t snrs[2 * TraceRoute::MAX_REPEATERS];
    uint8_t attempt;
  };

  StartResult start(const TraceRoute& route, uint32_t tag, uint32_t auth, uint32_t now);
  bool beginRetry(uint32_t tag, uint32_t auth, uint32_t now, const TraceRoute* alternate = NULL);
  bool isActive() const { return _state != IDLE; }
  bool retryPending() const { return _state == RETRY_PENDING; }
  bool retryStarted() const { return _current == 1; }
  bool waitingForQueue() const { return _state == QUEUED; }
  bool currentTransmitted() const { return _attempts[_current].transmitted; }
  void resetQueueClock(uint32_t now) { _phase_at = now; }
  void retryAfterLocalFailure() { if (_current == 0) _state = RETRY_PENDING; }
  bool matchProgress(const mesh::Packet* packet, uint8_t& attempt, uint32_t& elapsed, uint32_t now) const;
  bool isFinalizing() const { return _state == FINAL_GRACE || _state == FAILURE_PENDING; }
  bool failurePending() const { return _state == FAILURE_PENDING; }
  Failure failure() const { return _failure; }
  void cancel();
  void setQueuedPacket(mesh::Packet* packet) { _packet = packet; }
  mesh::Packet* queuedPacket() const { return _state == QUEUED ? _packet : NULL; }
  void setAirtimeEstimate(uint32_t airtime);
  bool isQueueTimedOut(uint32_t now) const;
  void queueFailed(uint32_t now);
  bool onTxStarted(const mesh::Packet* packet, uint32_t now);
  void onTxComplete(const mesh::Packet* packet);
  bool onTxFailed(const mesh::Packet* packet, uint32_t now);
  bool isTimedOut(uint32_t now);
  void advanceFinal(uint32_t now);
  bool reportExpired(uint32_t now) const;
  ReceiveResult receive(const mesh::Packet* packet, uint32_t tag, uint32_t auth, uint8_t flags,
                        const uint8_t* snrs, const uint8_t* hashes, uint8_t length,
                        uint32_t now, Result& result);
  bool takeFallback(Result& result);
  uint32_t waitDeadline() const {
    if(_state==IN_FLIGHT) return _attempts[_current].started_at+_attempts[_current].timeout;
    return _phase_at+(_state==QUEUED ? TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS
                                   : TraceTimingPolicy::FINAL_GRACE_MILLIS);
  }
  const TraceRoute& route() const { return _attempts[_current].route; }
  const uint8_t* path() const { return route().path; }
  uint8_t pathBytes() const { return route().bytes; }
  uint8_t flags() const { return route().flags; }
  uint32_t tag() const { return _attempts[_current].tag; }
  uint32_t authCode() const { return _attempts[_current].auth; }

private:
  enum State { IDLE, QUEUED, IN_FLIGHT, RETRY_PENDING, FINAL_GRACE, FAILURE_PENDING };
  struct Attempt {
    TraceRoute route;
    uint32_t tag, auth, started_at, timeout;
    bool transmitted;
  };
  Attempt _attempts[2]{};
  Result _fallback{};
  mesh::Packet* _packet = NULL;
  uint32_t _phase_at = 0;
  uint8_t _current = 0;
  State _state = IDLE;
  Failure _failure = NO_RESULT;
  bool _fallback_valid = false;

  void fail(Failure reason, uint32_t now);
};

inline RepeaterTraceProbe::StartResult RepeaterTraceProbe::start(const TraceRoute& route,
                                                          uint32_t tag, uint32_t auth, uint32_t now) {
  if (isActive()) return BUSY;
  if (!route.repeaters || route.repeaters>TraceRoute::MAX_REPEATERS || route.flags>1 ||
      route.bytes!=(2*route.repeaters-1)*(1<<route.flags)) return INVALID_PATH;
  memset(_attempts, 0, sizeof(_attempts));
  _attempts[0].route = route;
  _attempts[0].tag = tag;
  _attempts[0].auth = auth;
  _current = 0;
  _packet = NULL;
  _fallback_valid = false;
  _phase_at = now;
  _state = QUEUED;
  setAirtimeEstimate(0);
  return STARTED;
}

inline bool RepeaterTraceProbe::beginRetry(uint32_t tag, uint32_t auth, uint32_t now,
                                     const TraceRoute* alternate) {
  if (_state != RETRY_PENDING) return false;
  if (alternate && (!alternate->repeaters || alternate->repeaters > TraceRoute::MAX_REPEATERS ||
      alternate->flags > 1 || alternate->bytes !=
      (2 * alternate->repeaters - 1) * (1 << alternate->flags))) return false;
  if (tag == _attempts[0].tag && auth == _attempts[0].auth) tag++;
  _attempts[1] = {};
  _attempts[1].route = alternate ? *alternate : _attempts[0].route;
  _attempts[1].tag = tag;
  _attempts[1].auth = auth;
  _current = 1;
  _packet = NULL;
  _phase_at = now;
  _state = QUEUED;
  setAirtimeEstimate(0);
  return true;
}

inline void RepeaterTraceProbe::cancel() {
  _packet = NULL;
  _fallback_valid = false;
  _state = IDLE;
}

inline void RepeaterTraceProbe::setAirtimeEstimate(uint32_t airtime) {
  _attempts[_current].timeout = TraceTimingPolicy::attemptTimeout(route().repeaters, airtime);
}

inline bool RepeaterTraceProbe::isQueueTimedOut(uint32_t now) const {
  return _state == QUEUED && (uint32_t)(now - _phase_at) >= TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS;
}

inline void RepeaterTraceProbe::fail(Failure reason, uint32_t now) {
  _packet = NULL;
  _failure = reason;
  _phase_at = now;
  _state = _attempts[0].transmitted || _attempts[1].transmitted ? FINAL_GRACE : FAILURE_PENDING;
}

inline void RepeaterTraceProbe::queueFailed(uint32_t now) {
  fail(_current ? RETRY_NOT_SENT : RADIO_BUSY, now);
}

inline bool RepeaterTraceProbe::onTxStarted(const mesh::Packet* packet, uint32_t now) {
  if (_state != QUEUED || !packet || packet != _packet) return false;
  auto& attempt = _attempts[_current];
  attempt.started_at = now;
  attempt.transmitted = true;
  _state = IN_FLIGHT;
  return true;
}

inline void RepeaterTraceProbe::onTxComplete(const mesh::Packet* packet) {
  if (packet == _packet) _packet = NULL;
}

inline bool RepeaterTraceProbe::onTxFailed(const mesh::Packet* packet, uint32_t now) {
  if (!isActive() || !packet || packet != _packet) return false;
  fail(_current && !_attempts[1].transmitted ? RETRY_NOT_SENT : TRANSMIT_FAILED, now);
  return true;
}

inline bool RepeaterTraceProbe::isTimedOut(uint32_t now) {
  auto& attempt = _attempts[_current];
  if (_state != IN_FLIGHT || (uint32_t)(now - attempt.started_at) < attempt.timeout) return false;
  _packet = NULL;
  if (_current == 0) _state = RETRY_PENDING;
  else fail(NO_RESULT, now);
  return true;
}

inline void RepeaterTraceProbe::advanceFinal(uint32_t now) {
  if (_state == FINAL_GRACE && (uint32_t)(now - _phase_at) >= TraceTimingPolicy::FINAL_GRACE_MILLIS) {
    _state = FAILURE_PENDING;
    _phase_at = now;
  }
}

inline bool RepeaterTraceProbe::reportExpired(uint32_t now) const {
  return _state == FAILURE_PENDING &&
      (uint32_t)(now - _phase_at) >= TraceTimingPolicy::REPORT_TIMEOUT_MILLIS;
}

inline RepeaterTraceProbe::ReceiveResult RepeaterTraceProbe::receive(
    const mesh::Packet* packet, uint32_t tag, uint32_t auth, uint8_t flags,
    const uint8_t* snrs, const uint8_t* hashes, uint8_t length, uint32_t now, Result& result) {
  if (!isActive() || !packet || !packet->isRouteDirect() ||
      packet->getPayloadType() != PAYLOAD_TYPE_TRACE || !snrs || !hashes) return IGNORED;
  for (uint8_t i = 0; i <= _current; i++) {
    const auto& attempt = _attempts[i];
    if (!attempt.transmitted || tag != attempt.tag || auth != attempt.auth ||
        flags != attempt.route.flags || length != attempt.route.bytes ||
        packet->path_len != 2 * attempt.route.repeaters - 1 ||
        memcmp(hashes, attempt.route.path, length) != 0) continue;
    Result value{};
    value.elapsed_millis = (uint32_t)(now - attempt.started_at);
    value.route = attempt.route;
    memcpy(value.snrs, snrs, 2 * attempt.route.repeaters - 1);
    value.snrs[2 * attempt.route.repeaters - 1] = (uint8_t)(int8_t)(packet->getSNR() * 4);
    value.attempt = i;
    result = value;
    // Prefer the retry while it is still in flight, but never lose the first result.
    if (i == 0 && _current == 1 && _state == IN_FLIGHT) {
      if (!_fallback_valid) { _fallback = value; _fallback_valid = true; }
      return STORED;
    }
    return COMPLETE;
  }
  return IGNORED;
}

inline bool RepeaterTraceProbe::matchProgress(const mesh::Packet* packet, uint8_t& index,
                                      uint32_t& elapsed, uint32_t now) const {
  if (!isActive() || !packet || !packet->isRouteDirect() ||
      packet->getPayloadType() != PAYLOAD_TYPE_TRACE || packet->payload_len < 9) return false;
  uint32_t tag, auth;
  memcpy(&tag, packet->payload, 4);
  memcpy(&auth, packet->payload + 4, 4);
  for (uint8_t i = 0; i <= _current; i++) {
    const auto& attempt = _attempts[i];
    if (!attempt.transmitted || tag != attempt.tag || auth != attempt.auth ||
        packet->payload[8] != attempt.route.flags || packet->payload_len != 9 + attempt.route.bytes ||
        packet->path_len == 0 || packet->path_len > 2 * attempt.route.repeaters - 1 ||
        memcmp(packet->payload + 9, attempt.route.path, attempt.route.bytes) != 0) continue;
    index = i;
    elapsed = now - attempt.started_at;
    return true;
  }
  return false;
}

inline bool RepeaterTraceProbe::takeFallback(Result& result) {
  if (!_fallback_valid) return false;
  result = _fallback;
  _fallback_valid = false;
  return true;
}

} // namespace zen
