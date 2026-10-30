#pragma once
#include <Dispatcher.h>

namespace zen {
// Adapted from meshcore/doubler 4b3df353. Packets remain pool-owned values:
// this policy holds at most four existing pointers, never a payload copy.
class MultiFlood {
public:
  static constexpr uint32_t MIN_DELAY_MS=500,MAX_DELAY_MS=800,MAX_AGE_MS=3000,RETRY_DELAY_MS=200;
  static constexpr uint8_t MAX_PENDING=4;
  static constexpr int FREE_PACKET_RESERVE=4;
private:
  enum : uint8_t { CANCELLED=1,IN_FLIGHT=2,TRACE=4 };
  struct Pending {
    mesh::Packet* packet=nullptr;
    uint32_t ready_at=0,expires_at=0;
    uint8_t flags=0;
  } pending[MAX_PENDING];
  uint32_t context=0;
  uint8_t mode=0;
  bool forwarding=true,available=true,accept_first=true;
  static bool reached(uint32_t now,uint32_t deadline) { return int32_t(now-deadline)>=0; }
public:
  static bool forwarded(const mesh::Packet* packet) {
    // Mesh::sendFlood resets count to zero (ordinary and scoped). Its sole
    // flood forwarding path appends self before returning ACTION_RETRANSMIT.
    return packet && packet->isRouteFlood() && packet->getPathHashCount()!=0;
  }
  uint8_t getMode() const { return mode; }
  void setMode(uint8_t value) { mode=value<=2?value:0; }
  bool eligible(const mesh::Packet* packet) const {
    return available && mode && packet && packet->isRouteFlood() &&
      (forwarding || !forwarded(packet)) &&
      (mode==2 || packet->getPayloadType()==PAYLOAD_TYPE_GRP_TXT);
  }
  void firstStarted() { accept_first=true; }
  void invalidate() {
    // Also reject retaining an original TX that started before reconfiguration.
    // A fresh successful first start re-arms this latch; mode changes alone do not.
    accept_first=false;
    for(auto& p:pending)if(p.packet)p.flags|=CANCELLED;
  }
  void cancelTrace() { for(auto& p:pending)if(p.packet && (p.flags&TRACE))p.flags|=CANCELLED; }
  void configure(uint8_t value,bool repeat_enabled,bool radio_available,uint32_t profile) {
    if(profile!=context || (available && !radio_available))invalidate();
    context=profile;forwarding=repeat_enabled;available=radio_available;setMode(value);
    // Latch ineligibility: off/on before servicing must not resurrect copies.
    for(auto& p:pending)if(p.packet && !eligible(p.packet))p.flags|=CANCELLED;
  }
  unsigned pendingCount() const { unsigned n=0;for(const auto& p:pending)if(p.packet)++n;return n; }
  bool isRepeat(const mesh::Packet* packet) const {
    if(packet)for(const auto& p:pending)if(p.packet==packet)return true;
    return false;
  }
  void started(mesh::Packet* packet) { for(auto& p:pending)if(p.packet==packet)p.flags|=IN_FLIGHT; }
  bool forget(mesh::Packet* packet) {
    if(packet)for(auto& p:pending)if(p.packet==packet) { p=Pending{};return true; }
    return false;
  }
  bool canStart(const mesh::Packet* packet,uint32_t now) const {
    for(const auto& p:pending)if(p.packet==packet)
      return !(p.flags&CANCELLED) && eligible(packet) && !reached(now,p.expires_at);
    return true;
  }
  mesh::Packet* nextReady(uint32_t now) const {
    for(const auto& p:pending)if(p.packet && !(p.flags&IN_FLIGHT) &&
      reached(now,p.ready_at) && canStart(p.packet,now))return p.packet;
    return nullptr;
  }
  void deferReady(uint32_t now) {
    for(auto& p:pending)if(p.packet && !(p.flags&IN_FLIGHT) && reached(now,p.ready_at))
      p.ready_at=now+RETRY_DELAY_MS;
  }
  mesh::Packet* takeExpired(uint32_t now,const mesh::Packet* active) {
    for(auto& p:pending)if(p.packet && p.packet!=active && !(p.flags&IN_FLIGHT) &&
      ((p.flags&CANCELLED) || !eligible(p.packet) || reached(now,p.expires_at))) {
      auto packet=p.packet;p=Pending{};return packet;
    }
    return nullptr;
  }
  bool onTransmitComplete(mesh::Packet* packet,mesh::PacketManager& manager,
                          mesh::RNG& rng,uint32_t now,bool trace=false) {
    if(forget(packet))return false; // marker survives callbacks: no third TX
    if(!accept_first || !eligible(packet) || manager.getFreeCount()<FREE_PACKET_RESERVE)return false;
    for(auto& p:pending)if(!p.packet) {
      p.packet=packet;p.ready_at=now+rng.nextInt(MIN_DELAY_MS,MAX_DELAY_MS+1);
      p.expires_at=now+MAX_AGE_MS;p.flags=trace?TRACE:0;return true;
    }
    return false;
  }
};
static_assert(sizeof(MultiFlood)<=(sizeof(void*)==4?72u:112u),"Multi-flood exceeded its fixed RAM budget");
} // namespace zen
