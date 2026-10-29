#pragma once

#include <Dispatcher.h>

// Optional repeater policy. No packet fields, routing logic, or on-air format
// changes: the pending pointers identify second attempts until they are released.
class FloodTextDoubler {
public:
  static constexpr uint32_t MIN_DELAY_MS = 500;
  static constexpr uint32_t MAX_DELAY_MS = 800;
  static constexpr uint8_t MAX_PENDING = 4;
  static constexpr int FREE_PACKET_RESERVE = 4;
  static constexpr uint8_t REPEAT_PRIORITY = 254;

private:
  mesh::Packet* pending[MAX_PENDING] = {};

public:
  unsigned pendingCount() const {
    unsigned count = 0;
    for (auto p : pending) if (p) count++;
    return count;
  }

  bool forget(mesh::Packet* packet) {
    if (!packet) return false;
    for (auto& p : pending) {
      if (p == packet) {
        p = nullptr;
        return true;
      }
    }
    return false;
  }

  // Called only after local TX completion, with the caller still owning packet.
  // A true return transfers that ownership to the normal outbound queue.
  bool onTransmitComplete(mesh::Packet* packet, mesh::PacketManager& manager,
                          mesh::RNG& rng, uint32_t now) {
    if (forget(packet)) return false;  // second attempt: never repeat again
    if (!packet->isRouteFlood() || packet->getPayloadType() != PAYLOAD_TYPE_GRP_TXT
        || manager.getFreeCount() < FREE_PACKET_RESERVE) return false;

    for (auto& p : pending) {
      if (!p) {
        p = packet;
        uint32_t delay = rng.nextInt(MIN_DELAY_MS, MAX_DELAY_MS + 1);
        // StaticPoolPacketManager queues and pool have equal capacity. Holding
        // packet guarantees a vacant outbound slot; no allocation is needed.
        manager.queueOutbound(packet, REPEAT_PRIORITY, now + delay);
        return true;
      }
    }
    return false;  // bounded best effort under load
  }
};
