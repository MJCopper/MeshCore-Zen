#include "TraceRouteCandidates.h"
#include <Utils.h>
#include <string.h>

TraceRoute::Validation TraceRoute::fromRequest(const mesh::Packet* packet, TraceRoute& route) {
  if (!packet || !packet->isRouteFlood() || packet->getPayloadType() != PAYLOAD_TYPE_GRP_TXT ||
      !mesh::Packet::isValidPathLen(packet->path_len)) return INVALID;
  uint8_t count = packet->getPathHashCount(), size = packet->getPathHashSize();
  if (!count) return EMPTY;
  if (count > MAX_REPEATERS) return TOO_LONG;
  if (size > 2) return INVALID;
  route.repeaters = count;
  route.flags = size - 1;
  route.bytes = (2 * count - 1) * size;
  for (uint8_t i = 0; i < 2 * count - 1; i++) {
    uint8_t source = i < count ? count - 1 - i : i - count + 1;
    memcpy(&route.path[i * size], &packet->path[source * size], size);
  }
  return VALID;
}

bool TraceRoute::equals(const TraceRoute& other) const {
  return flags == other.flags && bytes == other.bytes && memcmp(path, other.path, bytes) == 0;
}

void TraceRouteCandidates::begin(const mesh::Packet* request) {
  // Called after channel MAC validation; later copies must match that ciphertext.
  clear();
  if (TraceRoute::fromRequest(request, _original) != TraceRoute::VALID ||
      request->payload_len < 3 || request->payload_len > MAX_PACKET_PAYLOAD) return;
  _payload_length = request->payload_len;
  mesh::Utils::sha256(_fingerprint, sizeof(_fingerprint), request->payload, request->payload_len);
  _active = true;
}

void TraceRouteCandidates::observe(const mesh::Packet* packet) {
  if (!_active || !packet || !packet->isRouteFlood() ||
      packet->getPayloadType() != PAYLOAD_TYPE_GRP_TXT || packet->payload_len != _payload_length) return;
  uint8_t fingerprint[sizeof(_fingerprint)];
  mesh::Utils::sha256(fingerprint, sizeof(fingerprint), packet->payload, packet->payload_len);
  if (memcmp(fingerprint, _fingerprint, sizeof(fingerprint)) != 0) return;
  TraceRoute route{};
  if (TraceRoute::fromRequest(packet, route) != TraceRoute::VALID || route.equals(_original)) return;
  // Equal-length candidates replace the previous one, preferring the most recent.
  if (!_has_alternate || route.repeaters <= _alternate.repeaters) {
    _alternate = route;
    _has_alternate = true;
  }
}
