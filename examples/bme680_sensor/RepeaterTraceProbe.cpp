#include "RepeaterTraceProbe.h"
#include <string.h>

RepeaterTraceProbe::StartResult RepeaterTraceProbe::start(const mesh::Packet* request,
                                                          uint32_t tag, uint32_t auth, uint32_t now) {
  if (isActive()) return BUSY;
  TraceRoute route{};
  switch (TraceRoute::fromRequest(request, route)) {
    case TraceRoute::EMPTY: return NO_REPEATERS;
    case TraceRoute::TOO_LONG: return TOO_LONG;
    case TraceRoute::INVALID: return INVALID_PATH;
    case TraceRoute::VALID: break;
  }
  return startRoute(route, tag, auth, now);
}

RepeaterTraceProbe::StartResult RepeaterTraceProbe::startRoute(const TraceRoute& route,
                                        uint32_t tag, uint32_t auth, uint32_t now) {
  if (isActive()) return BUSY;
  if (!route.repeaters || route.repeaters > TraceRoute::MAX_REPEATERS || route.flags > 1 ||
      route.bytes != (2 * route.repeaters - 1) * (1 << route.flags)) return INVALID_PATH;
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

bool RepeaterTraceProbe::beginRetry(uint32_t tag, uint32_t auth, uint32_t now,
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

void RepeaterTraceProbe::cancel() {
  _packet = NULL;
  _fallback_valid = false;
  _state = IDLE;
}

void RepeaterTraceProbe::setAirtimeEstimate(uint32_t airtime) {
  _attempts[_current].timeout = TraceTimingPolicy::attemptTimeout(route().repeaters, airtime);
}

bool RepeaterTraceProbe::isQueueTimedOut(uint32_t now) const {
  return _state == QUEUED && (uint32_t)(now - _phase_at) >= TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS;
}

void RepeaterTraceProbe::fail(Failure reason, uint32_t now) {
  _packet = NULL;
  _failure = reason;
  _phase_at = now;
  _state = _attempts[0].transmitted || _attempts[1].transmitted ? FINAL_GRACE : FAILURE_PENDING;
}

void RepeaterTraceProbe::queueFailed(uint32_t now) {
  fail(_current ? RETRY_NOT_SENT : RADIO_BUSY, now);
}

bool RepeaterTraceProbe::onTxStarted(const mesh::Packet* packet, uint32_t now) {
  if (_state != QUEUED || !packet || packet != _packet) return false;
  auto& attempt = _attempts[_current];
  attempt.started_at = now;
  attempt.transmitted = true;
  _state = IN_FLIGHT;
  return true;
}

void RepeaterTraceProbe::onTxComplete(const mesh::Packet* packet) {
  if (packet == _packet) _packet = NULL;
}

bool RepeaterTraceProbe::onTxFailed(const mesh::Packet* packet, uint32_t now) {
  if (!isActive() || !packet || packet != _packet) return false;
  fail(_current && !_attempts[1].transmitted ? RETRY_NOT_SENT : TRANSMIT_FAILED, now);
  return true;
}

bool RepeaterTraceProbe::isTimedOut(uint32_t now) {
  auto& attempt = _attempts[_current];
  if (_state != IN_FLIGHT || (uint32_t)(now - attempt.started_at) < attempt.timeout) return false;
  _packet = NULL;
  if (_current == 0) _state = RETRY_PENDING;
  else fail(NO_RESULT, now);
  return true;
}

void RepeaterTraceProbe::advanceFinal(uint32_t now) {
  if (_state == FINAL_GRACE && (uint32_t)(now - _phase_at) >= TraceTimingPolicy::FINAL_GRACE_MILLIS) {
    _state = FAILURE_PENDING;
    _phase_at = now;
  }
}

bool RepeaterTraceProbe::reportExpired(uint32_t now) const {
  return _state == FAILURE_PENDING &&
      (uint32_t)(now - _phase_at) >= TraceTimingPolicy::REPORT_TIMEOUT_MILLIS;
}

RepeaterTraceProbe::ReceiveResult RepeaterTraceProbe::receive(
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

bool RepeaterTraceProbe::matchProgress(const mesh::Packet* packet, uint8_t& index,
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

bool RepeaterTraceProbe::takeFallback(Result& result) {
  if (!_fallback_valid) return false;
  result = _fallback;
  _fallback_valid = false;
  return true;
}
