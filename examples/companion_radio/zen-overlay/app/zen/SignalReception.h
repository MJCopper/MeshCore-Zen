#pragma once
#include <Packet.h>
#include <helpers/AdvertDataHelpers.h>
#include <string.h>

namespace zen {
// Parsed-envelope qualification. Semantic authentication stays with MeshCore;
// only a verified zero-hop advert may supply an identified passive link.
inline bool signalPayloadValid(const mesh::Packet& p) {
  if (p.getPayloadVer() != PAYLOAD_VER_1 || !mesh::Packet::isValidPathLen(p.path_len)) return false;
  unsigned minimum;
  switch (p.getPayloadType()) {
    case PAYLOAD_TYPE_REQ: case PAYLOAD_TYPE_RESPONSE: case PAYLOAD_TYPE_TXT_MSG:
    case PAYLOAD_TYPE_PATH: minimum = 5; break;
    case PAYLOAD_TYPE_ACK: minimum = 4; break;
    case PAYLOAD_TYPE_ADVERT: minimum = PUB_KEY_SIZE + 4 + SIGNATURE_SIZE + 1; break;
    case PAYLOAD_TYPE_GRP_TXT: case PAYLOAD_TYPE_GRP_DATA: minimum = 4; break;
    case PAYLOAD_TYPE_ANON_REQ: minimum = 1 + PUB_KEY_SIZE + 3; break;
    case PAYLOAD_TYPE_RAW_CUSTOM: minimum = 1; break;
    default: return false; // traces and control packets have specialised paths
  }
  return p.payload_len >= minimum && p.payload_len <= MAX_PACKET_PAYLOAD;
}

// Only an active matching zero-hop control response can identify discovery
// evidence. Keep this shared with tests so malformed/late responses are covered.
inline const uint8_t* signalDiscoveryKey(const mesh::Packet& p, uint32_t request_tag, bool active) {
  if (!active || !request_tag || !mesh::Packet::isValidPathLen(p.path_len) ||
      !p.isRouteDirect() || p.getPathHashCount() != 0 ||
      p.getPayloadVer() != PAYLOAD_VER_1 || p.getPayloadType() != PAYLOAD_TYPE_CONTROL ||
      p.payload_len < 6+PUB_KEY_SIZE || p.payload_len > MAX_PACKET_PAYLOAD ||
      p.payload[0] != (0x90 | ADV_TYPE_REPEATER)) return nullptr;
  uint32_t response_tag; memcpy(&response_tag,p.payload+2,4);
  return response_tag == request_tag ? p.payload+6 : nullptr;
}

// Preserve same-reception RSSI through the dispatcher's delayed flood queue.
// Matches the 32-packet pool. Pointer reuse, expiry, and configuration reset
// cannot turn a later packet's radio status into this advert's measurement.
class SignalAdvertCapture {
public:
  static constexpr unsigned CAPACITY = 32;
  struct Reading { uint32_t at = 0; int16_t rssi = 0; int8_t snr = 0; uint8_t token[8]{}; };
private:
  struct Entry { const mesh::Packet* packet = nullptr; Reading reading; };
  Entry _entries[CAPACITY];
  unsigned _head = 0;
public:
  void reset() { *this = SignalAdvertCapture(); }
  void remember(const mesh::Packet* packet, int rssi, uint32_t now) {
    for (auto& entry : _entries) if (entry.packet == packet) entry.packet = nullptr;
    Entry& e = _entries[_head]; e.packet = packet;
    e.reading.at = now; e.reading.rssi = rssi; e.reading.snr = packet->_snr;
    packet->calculatePacketHash(e.reading.token);
    _head = (_head+1) % CAPACITY;
  }
  bool take(const mesh::Packet* packet, uint32_t now, Reading& out) {
    uint8_t token[8]; packet->calculatePacketHash(token);
    for (auto& e : _entries) {
      if (e.packet != packet) continue;
      bool match = now-e.reading.at <= 33000 && !memcmp(token,e.reading.token,8);
      e.packet = nullptr;
      if (match) { out = e.reading; return true; }
    }
    return false;
  }
};
} // namespace zen
