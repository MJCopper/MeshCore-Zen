#pragma once

#include <Packet.h>

struct TraceRoute {
  static const uint8_t MAX_REPEATERS = 10;
  static const uint8_t MAX_PATH_BYTES = (2 * MAX_REPEATERS - 1) * 2;
  uint8_t path[MAX_PATH_BYTES];
  uint8_t bytes, repeaters, flags;

  enum Validation { VALID, INVALID, EMPTY, TOO_LONG };
  static Validation fromRequest(const mesh::Packet* packet, TraceRoute& route);
  bool equals(const TraceRoute& other) const;
  uint8_t maxPacketLength() const { return 2 + 9 + bytes + 2 * repeaters - 1; }
};

// One passive alternate for the exact accepted command, with no persistent state.
class TraceRouteCandidates {
  uint8_t _fingerprint[16];
  uint16_t _payload_length = 0;
  TraceRoute _original{}, _alternate{};
  bool _active = false, _has_alternate = false;
public:
  void begin(const mesh::Packet* request);
  void observe(const mesh::Packet* packet);
  void clear() { _active = _has_alternate = false; }
  const TraceRoute* alternate() const { return _has_alternate ? &_alternate : NULL; }
};
