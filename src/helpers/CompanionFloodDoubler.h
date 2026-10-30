#pragma once

#include "DoublerFileStorage.h"
#include "FloodTextDoubler.h"
#include "DoublerContext.h"

// Companion-only adapter. The shared policy accepts only locally originated
// floods here. Explicit forwarding records also protect unusual/wrapped paths.
class CompanionFloodDoubler {
  DoublerSettings settings;
  DoublerContext context;
  // Match the companion's 16-packet pool. Unexpected overflow fails closed
  // until reboot, rather than risk treating untracked relays as local packets.
  mesh::Packet* forwarded[16] = {};
  bool tracking_failed = false;

  bool forgetForwarded(mesh::Packet* packet) {
    if (!packet) return false;
    for (auto& p : forwarded) {
      if (p == packet) { p = nullptr; return true; }
    }
    return false;
  }

public:
  FloodTextDoubler packets{true};

  void radioApplied(float freq, float bw, uint8_t sf, uint8_t cr) {
    context.radioApplied(packets, freq, bw, sf, cr);
  }
  void packetContext(const uint8_t* identity, uint8_t hash_mode, const uint8_t* scope) {
    context.packetContext(packets, identity, hash_mode, scope);
  }
  void onTransmitStarted(mesh::Packet* packet) { packets.onTransmitStarted(packet); }

  void noteForwarded(mesh::Packet* packet) {
    if (!packet) return;
    for (auto p : forwarded) if (p == packet) return;
    for (auto& p : forwarded) if (!p) { p = packet; return; }
    tracking_failed = true;
  }

  bool onTransmitComplete(mesh::Packet* packet, mesh::PacketManager& manager,
                          mesh::RNG& rng, uint32_t completed_at) {
    if (forgetForwarded(packet) || tracking_failed) return false;
    return packets.onTransmitComplete(packet, manager, rng, completed_at);
  }

  void onPacketReleased(mesh::Packet* packet) {
    forgetForwarded(packet);
    packets.forget(packet);
  }

  template<class FS>
  DoublerSettings::Status load(FS& fs, uint8_t write_mode) {
    DoublerFileStorage<FS> storage(fs, write_mode);
    auto status = settings.load(storage);
    packets.setMode(settings.getMode());
    return status;
  }

  // Match the existing companion CLI envelope, leaving unrelated commands and
  // their reply buffers untouched so the baseline handler can process them.
  template<class FS>
  bool handleCommand(const char* command, char* reply, FS& fs, uint8_t write_mode) {
    while (*command == ' ') command++;
    size_t prefix_length = strlen(command) > 4 && command[2] == '|' ? 3 : 0;
    DoublerFileStorage<FS> storage(fs, write_mode);
    if (!settings.handleCommand(command + prefix_length, reply + prefix_length, storage)) return false;
    if (prefix_length) memcpy(reply, command, prefix_length);
    packets.setMode(settings.getMode());
    return true;
  }

  bool hasPendingWork() const { return packets.pendingCount() != 0; }
};
