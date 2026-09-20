#include "RepeaterTraceProbe.h"

#include <string.h>

RepeaterTraceProbe::RepeaterTraceProbe()
    : _path_bytes(0), _repeater_count(0), _flags(0), _tag(0), _auth_code(0),
      _started_at(0), _queued_at(0), _timeout_millis(SHORT_TIMEOUT_MILLIS),
      _packet(NULL), _state(IDLE) {
  memset(_path, 0, sizeof(_path));
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
  _queued_at = now_millis;
  _packet = NULL;
  // Longer round trips get a little more room for forwarding and queueing.
  _timeout_millis = count <= 4 ? SHORT_TIMEOUT_MILLIS : LONG_TIMEOUT_MILLIS;
  _state = QUEUED;
  return STARTED;
}

bool RepeaterTraceProbe::isQueueTimedOut(uint32_t now_millis) const {
  return _state == QUEUED && (uint32_t)(now_millis - _queued_at) >= QUEUE_TIMEOUT_MILLIS;
}

bool RepeaterTraceProbe::onTxStarted(const mesh::Packet* packet, uint32_t now_millis) {
  if (_state != QUEUED || packet != _packet) return false;
  _started_at = now_millis;
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
  cancel();
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
  cancel();
  return true;
}
