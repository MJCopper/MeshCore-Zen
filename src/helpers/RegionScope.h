#pragma once
#include "PacketScope.h"
#include "RegionMap.h"
#include "RoutingPolicy.h"

namespace mesh {
// Resolve this packet, never the previous receive's mutable RegionEntry pointer.
// Region lookup may populate the bounded RAM key cache; it performs no writes.
inline PacketScope resolvePacketScope(const Packet& packet, RegionMap& regions) {
  PacketScope scope;
  if (!packet.hasTransportCodes()) return scope;
  scope.kind = PacketScope::UNRESOLVED;
  scope.route_type = packet.getRouteType();
  memcpy(scope.codes, packet.transport_codes, sizeof(scope.codes));
  for (int i = 0; i < regions.getCount(); i++) {
    const auto& region = *regions.getByIdx(i);
    if (region.flags & REGION_DENY_FLOOD) continue;
    TransportKey keys[4];
    int count = regions.getTransportKeysFor(region, keys, 4);
    for (int j = 0; j < count; j++) {
      if (keys[j].isNull() || keys[j].calcTransportCode(&packet) != packet.transport_codes[0]) continue;
      // A short transport code can match different keys. Keep such evidence unresolved.
      if (scope.kind == PacketScope::RESOLVED && memcmp(scope.key, keys[j].key, sizeof(scope.key))) {
        scope.kind = PacketScope::UNRESOLVED;
        return scope;
      }
      scope.kind = PacketScope::RESOLVED;
      memcpy(scope.key, keys[j].key, sizeof(scope.key));
    }
  }
  return scope;
}

inline TransportKey selectReplyTransportScope(const TransportKey& request, bool request_known, bool unscoped,
                                               const TransportKey& fallback) {
  switch (chooseReplyScope(request_known, unscoped, !fallback.isNull())) {
    case REPLY_SCOPE_REQUEST: return request;
    case REPLY_SCOPE_DEFAULT: return fallback;
    default: return TransportKey{};
  }
}

inline TransportKey captureReplyTransportScope(const Packet& packet, const PacketScope& scope,
                                                const TransportKey& fallback) {
  TransportKey request{};
  if (scope.kind == PacketScope::RESOLVED) memcpy(request.key, scope.key, sizeof(request.key));
  return selectReplyTransportScope(request, scope.kind == PacketScope::RESOLVED, packet.getRouteType() == ROUTE_TYPE_FLOOD, fallback);
}

// Shared by production and native adapters. Codes belong to the new payload.
template<class MeshType>
void sendCapturedFlood(MeshType& mesh, const TransportKey& scope, Packet* packet,
                       uint32_t delay, uint8_t width) {
  if (scope.isNull()) mesh.sendFlood(packet, delay, width);
  else {
    uint16_t codes[2] = {scope.calcTransportCode(packet), 0};
    mesh.sendFlood(packet, codes, delay, width);
  }
}
}
