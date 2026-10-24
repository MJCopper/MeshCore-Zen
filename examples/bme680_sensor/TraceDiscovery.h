#pragma once
#include "TraceTimingPolicy.h"
#include "TraceRouteCandidates.h"
#include <string.h>

// One bounded authenticated discovery. Peer/tag records outlive completion so
// delayed replies can be consumed without installing management routes.
class TraceDiscovery {
public:
  enum Phase { IDLE, QUEUE, WAIT, COLLECT, FAILED };
  enum Error { TIMEOUT, BUSY, TX_FAILED };
private:
  struct Peer {
    uint8_t key[32]{};
    uint32_t tags[2]{};
    bool sent[2]{};
    bool valid = false;
  } _peers[3];
  Phase _phase = IDLE;
  Error _error = TIMEOUT;
  mesh::Packet* _packet = NULL;
  uint32_t _phase_at = 0, _next_at = 0, _started = 0, _timeout = 0;
  uint8_t _attempt = 0, _failures = 0;
  void miss(uint32_t now, Error reason) {
    _packet = NULL;
    if (_attempt == 0) {
      _attempt = 1; _phase = QUEUE; _phase_at = _next_at = now; _failures = 0;
    } else { _phase = FAILED; _error = reason; }
  }
public:
  void begin(const uint8_t* key, uint32_t now) {
    _peers[2] = _peers[1]; _peers[1] = _peers[0]; _peers[0] = Peer{};
    memcpy(_peers[0].key, key, 32); _peers[0].valid = true;
    _phase = QUEUE; _attempt = _failures = 0; _packet = NULL;
    _phase_at = _next_at = now;
  }
  Phase phase() const { return _phase; }
  bool active() const { return _phase == QUEUE || _phase == WAIT; }
  const uint8_t* key() const { return _peers[0].key; }
  const uint8_t* peer(uint8_t index) const { return index < 3 && _peers[index].valid ? _peers[index].key : NULL; }
  bool owns(const uint8_t* key, uint32_t tag) const {
    for (const auto& peer : _peers) if (peer.valid && memcmp(key, peer.key, 32) == 0)
      for (uint8_t i = 0; i < 2; i++) if (peer.sent[i] && peer.tags[i] == tag) return true;
    return false;
  }
  bool current(const uint8_t* key, uint32_t tag) const {
    if ((!active() && _phase != COLLECT) || memcmp(key, _peers[0].key, 32)) return false;
    for (uint8_t i = 0; i < 2; i++) if (_peers[0].sent[i] && _peers[0].tags[i] == tag) return true;
    return false;
  }
  mesh::Packet* queuedPacket() const { return _phase == QUEUE ? _packet : NULL; }
  bool queueExpired(uint32_t now) const {
    return _phase == QUEUE && (uint32_t)(now - _phase_at) >= TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS;
  }
  bool ready(uint32_t now) const { return _phase == QUEUE && !_packet && (int32_t)(now - _next_at) >= 0; }
  void queued(mesh::Packet* packet, uint32_t tag, uint32_t timeout) {
    _packet = packet; _peers[0].tags[_attempt] = tag; _timeout = timeout;
  }
  void rejected(uint32_t now, uint32_t jitter) {
    _next_at = now + TraceTimingPolicy::localBackoff(_failures) + jitter % 251;
    if (_failures < 255) _failures++;
  }
  void tick(uint32_t now) {
    if (queueExpired(now)) miss(now, BUSY);
    else if (_phase == WAIT && (uint32_t)(now - _started) >= _timeout) miss(now, TIMEOUT);
  }
  void started(mesh::Packet* packet, uint32_t now) {
    if (_phase != QUEUE || packet != _packet) return;
    _peers[0].sent[_attempt] = true; _started = now; _phase = WAIT;
  }
  void completed(mesh::Packet* packet) { if (_packet == packet) _packet = NULL; }
  void failed(mesh::Packet* packet, uint32_t now) {
    if (active() && packet == _packet) miss(now, TX_FAILED);
    else completed(packet);
  }
  void collect() { _phase = COLLECT; _packet = NULL; }
  void cancel() { _phase = IDLE; _packet = NULL; }
  const char* failureText() const {
    return _error == BUSY ? "Trace: discovery radio busy" :
           _error == TX_FAILED ? "Trace: discovery transmit failed" : "Trace: discovery timed out";
  }
};
