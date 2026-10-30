#pragma once

#include <Dispatcher.h>

// Optional repeater policy. No packet fields, routing logic, or on-air format
// changes: bounded local records track waiting/in-flight second attempts.
class FloodTextDoubler {
public:
  static constexpr uint32_t MIN_DELAY_MS = 500;
  static constexpr uint32_t MAX_DELAY_MS = 800;
  static constexpr uint32_t MAX_AGE_MS = 3000;
  static constexpr uint32_t RETRY_DELAY_MS = 200;
  static constexpr uint8_t MAX_PENDING = 4;
  static constexpr int FREE_PACKET_RESERVE = 4;

private:
  enum : uint8_t { CANCELLED = 1, IN_FLIGHT = 2 };
  struct Pending {
    mesh::Packet* packet = nullptr;
    uint32_t ready_at = 0;
    uint32_t expires_at = 0;
    uint8_t flags = 0;
  };
  Pending pending[MAX_PENDING];
  uint8_t mode = 0;
  const bool local_only;
  bool available = true;
  uint32_t generation = 0, first_generation = 0;
  mesh::Packet* first_active = nullptr;

  static bool reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
  }

public:
  explicit FloodTextDoubler(bool locally_originated_only = false) : local_only(locally_originated_only) {}
  uint8_t getMode() const { return mode; }
  void setMode(uint8_t value) {
    mode = value <= 2 ? value : 0;
    for (auto& p : pending) if (p.packet && !eligible(p.packet)) p.flags |= CANCELLED;
  }
  void invalidateContext() {
    ++generation;
    for (auto& p : pending) if (p.packet) p.flags |= CANCELLED;
  }
  void setAvailable(bool value) {
    if (value != available) { invalidateContext(); available = value; }
  }
  void onTransmitStarted(mesh::Packet* packet) {
    if (!packet) return;
    for (auto& p : pending) {
      if (p.packet == packet) { p.flags |= IN_FLIGHT; return; }
    }
    first_active = packet;
    first_generation = generation;
  }
  bool eligible(const mesh::Packet* packet) const {
    return available && mode != 0 && packet->isRouteFlood()
        && (!local_only || packet->getPathHashCount() == 0)
        && (mode == 2 || packet->getPayloadType() == PAYLOAD_TYPE_GRP_TXT);
  }

  unsigned pendingCount() const {
    unsigned count = 0;
    for (const auto& p : pending) if (p.packet) count++;
    return count;
  }

  bool forget(mesh::Packet* packet) {
    if (!packet) return false;
    if (first_active == packet) first_active = nullptr;
    for (auto& p : pending) {
      if (p.packet == packet) {
        p = Pending{};
        return true;
      }
    }
    return false;
  }

  void cancel(const mesh::Packet* packet) {
    for (auto& p : pending) if (p.packet == packet) p.flags |= CANCELLED;
  }

  bool isRepeat(const mesh::Packet* packet) const {
    if (!packet) return false;
    for (const auto& p : pending) if (p.packet == packet) return true;
    return false;
  }

  // Borrow a ready packet. The dispatcher owns it once selected for TX, but
  // keeps its record until completion/release to prevent a third transmission.
  mesh::Packet* nextReady(uint32_t now) const {
    for (const auto& p : pending) {
      if (p.packet && !(p.flags & IN_FLIGHT) && reached(now, p.ready_at)
          && canStart(p.packet, now)) return p.packet;
    }
    return nullptr;
  }

  // A local retry pause avoids repeatedly scanning a busy channel. It never
  // changes expiry or the dispatcher's normal-packet CAD/backoff timers.
  void deferReady(uint32_t now) {
    for (auto& p : pending) {
      if (p.packet && !(p.flags & IN_FLIGHT) && reached(now, p.ready_at)) p.ready_at = now + RETRY_DELAY_MS;
    }
  }

  // Discard expired or newly ineligible waiting copies. An in-flight copy must
  // finish normally, retaining its marker until completion/release.
  mesh::Packet* takeExpired(uint32_t now, const mesh::Packet* active, bool* cancelled = nullptr) {
    for (auto& p : pending) {
      if (p.packet && p.packet != active && !(p.flags & IN_FLIGHT)
          && ((p.flags & CANCELLED) || !eligible(p.packet) || reached(now, p.expires_at))) {
        mesh::Packet* packet = p.packet;
        if (cancelled) *cancelled = (p.flags & CANCELLED) || !eligible(p.packet);
        p = Pending{};
        return packet;
      }
    }
    return nullptr;
  }

  bool canStart(const mesh::Packet* packet, uint32_t now) const {
    for (const auto& p : pending) {
      if (p.packet == packet) return !(p.flags & CANCELLED) && eligible(packet) && !reached(now, p.expires_at);
    }
    return true;  // normal packets retain baseline behaviour
  }

  // Called only after local TX completion, with the caller still owning packet.
  // A true return transfers ownership to this module's bounded waiting list.
  bool onTransmitComplete(mesh::Packet* packet, mesh::PacketManager& manager,
                          mesh::RNG& rng, uint32_t now) {
    bool current = first_active == packet && first_generation == generation;
    if (forget(packet)) return false;  // second attempt: never repeat again
    if (!current || !eligible(packet) || manager.getFreeCount() < FREE_PACKET_RESERVE) return false;

    for (auto& p : pending) {
      if (!p.packet) {
        p.packet = packet;
        uint32_t delay = rng.nextInt(MIN_DELAY_MS, MAX_DELAY_MS + 1);
        p.ready_at = now + delay;
        p.expires_at = now + MAX_AGE_MS;
        p.flags = 0;
        return true;
      }
    }
    return false;  // bounded best effort under load
  }
};
static_assert(sizeof(FloodTextDoubler) <= (sizeof(void*) == 4 ? 80u : 120u),
              "Doubler policy exceeded its fixed RAM budget");
