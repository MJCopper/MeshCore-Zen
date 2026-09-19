#include "RepeaterTraceProbe.h"

#include <string.h>

RepeaterTraceProbe::RepeaterTraceProbe()
    : _path_bytes(0), _repeater_count(0), _flags(0), _tag(0), _auth_code(0),
      _started_at(0), _active(false) {
  memset(_path, 0, sizeof(_path));
}

RepeaterTraceProbe::StartResult RepeaterTraceProbe::start(const mesh::Packet* request,
                                                          uint32_t tag, uint32_t auth_code,
                                                          uint32_t now_millis) {
  if (_active) return BUSY;
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
  _started_at = now_millis;
  _active = true;
  return STARTED;
}

bool RepeaterTraceProbe::isTimedOut(uint32_t now_millis) {
  if (!_active || (uint32_t)(now_millis - _started_at) < TIMEOUT_MILLIS) return false;
  _active = false;
  return true;
}

bool RepeaterTraceProbe::complete(const mesh::Packet* packet, uint32_t tag,
                                  uint32_t auth_code, uint8_t flags,
                                  const uint8_t* path_hashes, uint8_t path_len,
                                  uint32_t now_millis, uint32_t& elapsed_millis,
                                  uint8_t& repeater_count) {
  if (!_active || !packet || tag != _tag || auth_code != _auth_code || flags != _flags ||
      path_len != _path_bytes || !path_hashes ||
      memcmp(path_hashes, _path, _path_bytes) != 0 ||
      packet->path_len != 2 * _repeater_count - 1) return false;

  elapsed_millis = (uint32_t)(now_millis - _started_at);
  repeater_count = _repeater_count;
  _active = false;
  return true;
}
