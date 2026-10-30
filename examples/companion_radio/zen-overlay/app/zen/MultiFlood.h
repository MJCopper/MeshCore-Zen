#pragma once
#include <Dispatcher.h>

namespace zen {
// Adapted from meshcore/doubler 7f0cb629. Packets remain pool-owned values:
// retains at most four pool packets, never a payload copy. Origin records do
// not own packets and cover the complete Zen pool.
class MultiFlood {
public:
  static constexpr uint32_t MIN_DELAY_MS=500,MAX_DELAY_MS=800,MAX_AGE_MS=3000,RETRY_DELAY_MS=200;
  static constexpr uint8_t MAX_PENDING=4;
  static constexpr int FREE_PACKET_RESERVE=4;
private:
  enum : uint8_t { CANCELLED=1,IN_FLIGHT=2,TRACE=4,FORWARDED=8 };
  struct Pending {
    mesh::Packet* packet=nullptr;
    uint32_t ready_at=0,expires_at=0;
    uint8_t flags=0;
  } pending[MAX_PENDING];
  // Match Zen's 32-packet pool. Overflow fails closed until reboot.
  mesh::Packet* forwarded_packets[32] = {};
  mesh::Packet* first_active=nullptr;
  uint32_t generation=0,first_generation=0;
  uint8_t mode=0;
  bool forwarding=true,available=true,tracking_failed=false;
  static bool reached(uint32_t now,uint32_t deadline) { return int32_t(now-deadline)>=0; }
public:
  void noteForwarded(mesh::Packet* packet) {
    if(!packet)return;
    for(auto p:forwarded_packets)if(p==packet)return;
    for(auto& p:forwarded_packets)if(!p) { p=packet;return; }
    tracking_failed=true;
  }
  bool forwarded(const mesh::Packet* packet) const {
    if(!packet)return false;
    for(auto p:forwarded_packets)if(p==packet)return true;
    for(const auto& p:pending)if(p.packet==packet && (p.flags&FORWARDED))return true;
    // Mesh::sendFlood resets count to zero (ordinary and scoped). Its sole
    // flood forwarding path appends self before returning ACTION_RETRANSMIT.
    return packet->isRouteFlood() && packet->getPathHashCount()!=0;
  }
  uint8_t getMode() const { return mode; }
  void setMode(uint8_t value) {
    mode=value<=2?value:0;
    // Latch ineligibility: off/on before servicing cannot resurrect copies.
    for(auto& p:pending)if(p.packet && !eligible(p.packet))p.flags|=CANCELLED;
  }
  bool eligible(const mesh::Packet* packet) const {
    return !tracking_failed && available && mode && packet && packet->isRouteFlood() &&
      (forwarding || !forwarded(packet)) &&
      (mode==2 || packet->getPayloadType()==PAYLOAD_TYPE_GRP_TXT);
  }

  void invalidate() {
    // A/B/A changes must also invalidate an original that is already on air.
    ++generation;
    for(auto& p:pending)if(p.packet)p.flags|=CANCELLED;
  }
  void cancelTrace() { for(auto& p:pending)if(p.packet && (p.flags&TRACE))p.flags|=CANCELLED; }
  void configure(uint8_t value,bool repeat_enabled,bool radio_available) {
    if(available!=radio_available)invalidate();
    forwarding=repeat_enabled;available=radio_available;setMode(value);
  }
  unsigned pendingCount() const { unsigned n=0;for(const auto& p:pending)if(p.packet)++n;return n; }
  bool isRepeat(const mesh::Packet* packet) const {
    if(packet)for(const auto& p:pending)if(p.packet==packet)return true;
    return false;
  }
  void started(mesh::Packet* packet) {
    if(!packet)return;
    for(auto& p:pending)if(p.packet==packet) { p.flags|=IN_FLIGHT;return; }
    first_active=packet;first_generation=generation;
  }
  bool forget(mesh::Packet* packet) {
    if(!packet)return false;
    if(first_active==packet)first_active=nullptr;
    for(auto& p:forwarded_packets)if(p==packet)p=nullptr;
    for(auto& p:pending)if(p.packet==packet) { p=Pending{};return true; }
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
    const bool current=first_active==packet && first_generation==generation;
    const bool allowed=eligible(packet),relayed=forwarded(packet);
    if(forget(packet))return false; // marker survives callbacks: no third TX
    if(!current || !allowed || manager.getFreeCount()<FREE_PACKET_RESERVE)return false;
    for(auto& p:pending)if(!p.packet) {
      p.packet=packet;p.ready_at=now+rng.nextInt(MIN_DELAY_MS,MAX_DELAY_MS+1);
      p.expires_at=now+MAX_AGE_MS;p.flags=(trace?TRACE:0)|(relayed?FORWARDED:0);return true;
    }
    return false;
  }
};
static_assert(sizeof(MultiFlood)<=(sizeof(void*)==4?208u:376u),"Multi-flood exceeded its fixed RAM budget");
} // namespace zen
