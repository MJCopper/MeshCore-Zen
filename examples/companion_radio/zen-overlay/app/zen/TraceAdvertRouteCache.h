#pragma once
#include "TraceRoute.h"
#include "TraceScope.h"
#include <helpers/AdvertDataHelpers.h>

namespace zen {

// RAM-only hints from verified adverts, never completed trace measurements.
// Empty after reboot/identity change; no filesystem or wall-clock dependency.
class TraceAdvertRouteCache {
public:
  static const uint8_t CAPACITY=8;
  static const uint32_t TTL=24UL*60*60*1000;
private:
  struct Hint {
    uint32_t profile=0, observed=0, origin=0, fingerprint=0;
    uint8_t via[18]{}, scope[16]{};
    uint8_t count=0, width=0;
  };
  struct Entry { uint8_t key[32]{}; Hint hint; } _entries[CAPACITY];
  static_assert(sizeof(Hint)==52,"Single hint exceeds its RAM budget");
  static_assert(sizeof(Entry)==84,"Single-hint entry exceeds its RAM budget");
  static bool usable(const Hint& h,uint32_t now) {
    return h.width && uint32_t(now-h.observed)<TTL;
  }
  static uint32_t fingerprint(const uint8_t* data,size_t size,uint32_t origin) {
    uint32_t hash=2166136261UL;
    while(size--) hash=(hash^*data++)*16777619UL;
    return hash^origin;
  }
public:
  static bool convert(const mesh::Packet& packet,const uint8_t* target,TraceRoute& route) {
    if(!packet.isRouteFlood() || !mesh::Packet::isValidPathLen(packet.path_len) ||
       packet.getPathHashSize()>2 || packet.getPathHashCount()>=10) return false;
    uint8_t via[18]{}, width=packet.getPathHashSize(), count=packet.getPathHashCount();
    for(uint8_t i=0;i<count;i++) memcpy(via+i*width,packet.path+(count-1-i)*width,width);
    return TraceRoute::fromOutbound(via,((width-1)<<6)|count,target,route)==TraceRoute::VALID;
  }
  void clear() { memset(_entries,0,sizeof(_entries)); }
  bool observe(const mesh::Packet* packet,const mesh::Identity& id,uint32_t origin,
               const uint8_t* data,size_t length,uint32_t now,
               uint32_t profile,const TraceScope& scope) {
    if(!packet || !data || !length || length>MAX_ADVERT_DATA_SIZE || !scope.reusable()) return false;
    unsigned minimum=1+((data[0]&ADV_LATLON_MASK)?8:0)+((data[0]&ADV_FEAT1_MASK)?2:0)+((data[0]&ADV_FEAT2_MASK)?2:0);
    if(length<minimum)return false;
    AdvertDataParser parser(data,length);
    TraceRoute route{};
    if(!parser.isValid() || parser.getType()!=ADV_TYPE_REPEATER || !convert(*packet,id.pub_key,route)) return false;
    Entry* entry=nullptr; Entry* empty=nullptr; Entry* oldest=&_entries[0]; uint32_t age=0;
    for(auto& e:_entries) {
      if(e.hint.width && !memcmp(e.key,id.pub_key,32)) entry=&e;
      if(!e.hint.width) empty=&e;
      uint32_t a=uint32_t(now-e.hint.observed); if(a>=age) {age=a;oldest=&e;}
    }
    const uint32_t fp=fingerprint(data,length,origin);
    // A repeated advert must not extend freshness or change the retained path.
    if(entry && entry->hint.origin==origin && entry->hint.fingerprint==fp) return false;
    if(!entry) { entry=empty?empty:oldest; memcpy(entry->key,id.pub_key,32); }
    Hint& hint=entry->hint; hint=Hint{};
    hint.width=packet->getPathHashSize();hint.count=packet->getPathHashCount();
    hint.profile=profile;hint.observed=now;hint.origin=origin;hint.fingerprint=fp;
    memcpy(hint.via,route.path,hint.count*hint.width);memcpy(hint.scope,scope.key,16);
    return true;
  }
  bool best(const uint8_t* key,uint32_t profile,const TraceScope& scope,uint32_t now,
            TraceRoute& out,const TraceRoute* exclude=nullptr) const {
    if(!scope.reusable()) return false;
    for(const auto& e:_entries) {
      const Hint& h=e.hint;
      if(memcmp(key,e.key,32) || !usable(h,now) || h.profile!=profile || memcmp(h.scope,scope.key,16)) continue;
      TraceRoute candidate{};
      if(TraceRoute::fromOutbound(h.via,((h.width-1)<<6)|h.count,key,candidate)!=TraceRoute::VALID ||
         (exclude && candidate.equals(*exclude))) return false;
      out=candidate;return true;
    }
    return false;
  }
};
static_assert(sizeof(TraceAdvertRouteCache)==672,"Eight RAM-only hints must stay within their budget");
} // namespace zen
