#pragma once

#include <Packet.h>
#include <helpers/PacketScope.h>

struct TraceRoute {
  static const uint8_t MAX_REPEATERS = 10;
  static const uint8_t MAX_PATH_BYTES = (2 * MAX_REPEATERS - 1) * 2;
  uint8_t path[MAX_PATH_BYTES];
  uint8_t bytes, repeaters, flags;

  enum Validation { VALID, INVALID, EMPTY, TOO_LONG };
  static Validation fromRequest(const mesh::Packet* packet, TraceRoute& route);
  static Validation fromOutbound(const uint8_t* via, uint8_t encoded,
                                 const uint8_t* target, TraceRoute& route);
  bool equals(const TraceRoute& other) const;
  uint8_t maxPacketLength() const { return 2 + 9 + bytes + 2 * repeaters - 1; }
};

class TraceRouteHistory;

// Bounded passive alternatives for the exact accepted command.
class TraceRouteCandidates {
  uint8_t _fingerprint[16];
  uint16_t _payload_length = 0;
  TraceRoute _original{};
  struct Candidate {
    TraceRoute route{};
    uint32_t seen = 0, sequence = 0;
    int8_t snr = -128;
    uint8_t observations = 0;
  } _candidates[3];
  uint8_t _count = 0;
  uint32_t _last_seen = 0, _sequence = 0;
  bool _active = false;
  mesh::PacketScope _scope;
public:
  void begin(const mesh::Packet* request, uint32_t now = 0, const mesh::PacketScope& scope = {});
  void observe(const mesh::Packet* packet, uint32_t now = 0, const mesh::PacketScope& scope = {});
  void beginDiscovered(const TraceRoute& route, uint32_t now, const mesh::PacketScope& scope);
  void observeDiscovered(const TraceRoute& route, uint32_t now, int8_t snr);
  void clear() { _active = false; _count = 0; }
  uint32_t lastSeen() const { return _last_seen; }
  uint8_t count() const { return _count; }
  const TraceRoute& candidate(uint8_t index) const { return _candidates[index].route; }
  const TraceRoute* best(const TraceRouteHistory& history, uint8_t channel, uint32_t profile,
                         uint32_t now, const uint32_t airtimes[3], const mesh::PacketScope& scope = {}, const uint8_t* target_key = NULL) const;
};
