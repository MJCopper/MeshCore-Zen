#include "RepeaterTraceProbe.h"

#include <string.h>

RepeaterTraceProbe::RepeaterTraceProbe()
    : _path_bytes(0), _repeater_count(0), _flags(0), _tag(0), _auth_code(0),
      _first_tag(0), _first_auth_code(0), _first_started_at(0),
      _started_at(0), _queued_at(0), _timeout_millis(0),
      _packet(NULL), _state(IDLE), _retried(false), _fallback_valid(false) {
  memset(_path, 0, sizeof(_path));
  memset(&_fallback, 0, sizeof(_fallback));
}

RepeaterTraceProbe::StartResult RepeaterTraceProbe::start(const mesh::Packet* request,
                                                          uint32_t tag, uint32_t auth_code,
                                                          uint32_t now_millis) {
  if (isActive()) return BUSY;
  if (!request || !request->isRouteFlood() || !mesh::Packet::isValidPathLen(request->path_len))
    return INVALID_PATH;

  uint8_t count = request->getPathHashCount();
  if (count == 0) return NO_REPEATERS;
  if (count > MAX_REPEATERS) return TOO_LONG;

  uint8_t hash_size = request->getPathHashSize();
  // TRACE addresses hops with 1, 2, 4 or 8-byte hashes; a 3-byte flood path
  // cannot be mirrored without changing its node matching semantics.
  if (hash_size > 2) return INVALID_PATH;
  _path_bytes = (2 * count - 1) * hash_size;
  _repeater_count = count;
  _flags = hash_size - 1;
  for (uint8_t i = 0; i < 2 * count - 1; i++) {
    uint8_t source = i < count ? count - 1 - i : i - count + 1;
    memcpy(&_path[i * hash_size], &request->path[source * hash_size], hash_size);
  }
  _tag = tag;
  _auth_code = auth_code;
  _first_tag = tag;
  _first_auth_code = auth_code;
  _queued_at = now_millis;
  _packet = NULL;
  // Allow for both legs through each repeater, rounded up to whole seconds.
  uint32_t timeout = BASE_TIMEOUT_MILLIS + count * PER_REPEATER_TIMEOUT_MILLIS;
  _timeout_millis = ((timeout + 999) / 1000) * 1000;
  _retried = false;
  _fallback_valid = false;
  _state = QUEUED;
  return STARTED;
}

bool RepeaterTraceProbe::beginRetry(uint32_t tag, uint32_t auth_code,
                                    uint32_t now_millis) {
  if (_state != RETRY_PENDING) return false;
  if (tag == _tag && auth_code == _auth_code) tag++;
  _tag = tag;
  _auth_code = auth_code;
  _queued_at = now_millis;
  _packet = NULL;
  _retried = true;
  _state = QUEUED;
  return true;
}

bool RepeaterTraceProbe::isQueueTimedOut(uint32_t now_millis) const {
  return _state == QUEUED && (uint32_t)(now_millis - _queued_at) >= QUEUE_TIMEOUT_MILLIS;
}

bool RepeaterTraceProbe::onTxStarted(const mesh::Packet* packet, uint32_t now_millis) {
  if (_state != QUEUED || packet != _packet) return false;
  _started_at = now_millis;
  if (!_retried) _first_started_at = now_millis;
  _state = IN_FLIGHT;
  return true;
}

void RepeaterTraceProbe::onTxComplete(const mesh::Packet* packet) {
  if (_state == IN_FLIGHT && packet == _packet) _packet = NULL;
}

bool RepeaterTraceProbe::onTxFailed(const mesh::Packet* packet) {
  if (_state != IDLE && packet == _packet) {
    cancel();
    return true;
  }
  return false;
}

bool RepeaterTraceProbe::isTimedOut(uint32_t now_millis) {
  if (_state != IN_FLIGHT || (uint32_t)(now_millis - _started_at) < _timeout_millis) return false;
  if (_retried) cancel();
  else { _packet = NULL; _state = RETRY_PENDING; }
  return true;
}

bool RepeaterTraceProbe::complete(const mesh::Packet* packet, uint32_t tag,
                                  uint32_t auth_code, uint8_t flags,
                                  const uint8_t* path_hashes, uint8_t path_len,
                                  uint32_t now_millis, uint32_t& elapsed_millis,
                                  uint8_t& repeater_count) {
  if (_state != IN_FLIGHT || (uint32_t)(now_millis - _started_at) >= _timeout_millis ||
      !packet || tag != _tag || auth_code != _auth_code || flags != _flags ||
      path_len != _path_bytes || !path_hashes ||
      memcmp(path_hashes, _path, _path_bytes) != 0 ||
      packet->path_len != 2 * _repeater_count - 1) return false;

  elapsed_millis = (uint32_t)(now_millis - _started_at);
  repeater_count = _repeater_count;
  _fallback_valid = false;
  cancel();
  return true;
}

RepeaterTraceProbe::LateResult RepeaterTraceProbe::captureLateFirst(
    const mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
    const uint8_t* path_snrs, const uint8_t* path_hashes, uint8_t path_len,
    uint32_t now_millis) {
  if (!_retried || _fallback_valid || (_state != QUEUED && _state != IN_FLIGHT) ||
      !packet || tag != _first_tag || auth_code != _first_auth_code ||
      flags != _flags || path_len != _path_bytes || !path_snrs || !path_hashes ||
      memcmp(path_hashes, _path, _path_bytes) != 0 ||
      packet->path_len != 2 * _repeater_count - 1) return NOT_FIRST;

  _fallback.elapsed_millis = (uint32_t)(now_millis - _first_started_at);
  _fallback.repeater_count = _repeater_count;
  memcpy(_fallback.snrs, path_snrs, _repeater_count);
  _fallback_valid = true;
  return _state == QUEUED ? RETRY_QUEUED : FALLBACK_SAVED;
}

bool RepeaterTraceProbe::takeFallback(Result& result) {
  if (!_fallback_valid) return false;
  result = _fallback;
  _fallback_valid = false;
  return true;
}
