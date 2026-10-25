#pragma once
#include "TraceStorage.h"
#include "TraceRouteCandidates.h"
#include <helpers/AdvertDataHelpers.h>

// Advert payload is authenticated upstream; its mutable path is only a hint.
class TraceAdvertRouteCache {
public:
  static const uint8_t CAPACITY=48;
  static const uint32_t TTL=24UL*60*60*1000;
private:
  struct Hint {
    uint32_t profile=0, observed=0, utc=0, origin=0, fingerprint=0, validated=0;
    uint8_t via[18]{}, scope[16]{};
    uint8_t count=0, width=0, misses=0;
    int8_t snr=0;
    bool probation=false, age_unknown=false;
  };
  static_assert(sizeof(Hint)==64,"Update cache format/version when changing hint layout");
  struct Entry { uint8_t key[32]{}; Hint hints[2]; } _entries[CAPACITY];
  struct Header { uint32_t magic, size, crc; uint8_t self[32]; };
  FILESYSTEM* _fs=nullptr;
  uint8_t _self[32]{};
  bool _dirty=false, _saved=false, _primary_valid=false;
  uint32_t _dirty_at=0, _saved_at=0;
  void dirty(uint32_t now) { if(!_dirty) _dirty_at=now; _dirty=true; }
  static bool usable(const Hint& h,uint32_t now) { return h.width && (h.age_unknown || (uint32_t)(now-h.observed)<TTL); }
public:
  static bool convert(const mesh::Packet& packet,const uint8_t* target,TraceRoute& route) {
    if(!packet.isRouteFlood() || !mesh::Packet::isValidPathLen(packet.path_len) ||
       packet.getPathHashSize()>2 || packet.getPathHashCount()>=10) return false;
    uint8_t via[18]{}, width=packet.getPathHashSize(), count=packet.getPathHashCount();
    for(uint8_t i=0;i<count;i++) memcpy(via+i*width,packet.path+(count-1-i)*width,width);
    return TraceRoute::fromOutbound(via,((width-1)<<6)|count,target,route)==TraceRoute::VALID;
  }
  void begin(FILESYSTEM* fs,const uint8_t* self,uint32_t now,uint32_t utc,bool trusted) {
    _fs=fs; memcpy(_self,self,32); clear(); _saved=false; _saved_at=0; _primary_valid=false;
    if(!fs) return;
    auto load=[&](const char* path) {
      auto f=trace_storage::open(fs,path,false); Header h{};
      memset(_entries,0,sizeof(_entries));
      bool ok=f && f.read((uint8_t*)&h,sizeof(h))==sizeof(h) &&
          h.magic==0x32414354 && h.size<=sizeof(_entries) && h.size%sizeof(Entry)==0 &&
          f.size()==sizeof(h)+h.size && !memcmp(h.self,self,32) &&
          f.read((uint8_t*)_entries,h.size)==h.size && h.crc==trace_storage::checksum(_entries,h.size);
      f.close(); return ok;
    };
    _primary_valid=load("/trace_adverts");
    if(!_primary_valid) {
      if(!load("/trace_adverts.bak")) { clear(); return; }
      fs->remove("/trace_adverts");
      _primary_valid=fs->rename("/trace_adverts.bak","/trace_adverts");
    }
    for(auto& e:_entries) for(auto& r:e.hints) {
      if(!r.width) continue;
      if(r.width>2 || r.count>=10) { r=Hint{}; continue; }
      if(trusted && r.utc && utc>=r.utc) {
        uint32_t age=utc-r.utc;
        if(age>=TTL/1000) { r=Hint{}; continue; }
        r.observed=now-age*1000; r.probation=false;
      } else { r.probation=true; r.age_unknown=true; }
    }
  }
  void clear() { memset(_entries,0,sizeof(_entries)); _dirty=false; }
  bool observe(const mesh::Packet* packet,const mesh::Identity& id,uint32_t origin,
               const uint8_t* data,size_t length,uint32_t now,uint32_t utc,bool trusted,
               uint32_t profile,const mesh::PacketScope& scope) {
    if(!packet || !data || !length || !scope.reusable()) return false;
    AdvertDataParser parser(data,length);
    TraceRoute route{};
    if(!parser.isValid() || parser.getType()!=ADV_TYPE_REPEATER || !convert(*packet,id.pub_key,route)) return false;
    Entry* entry=nullptr; Entry* empty=nullptr; Entry* oldest=&_entries[0]; uint32_t age=0;
    for(auto& e:_entries) {
      if(!memcmp(e.key,id.pub_key,32) && (e.hints[0].width || e.hints[1].width)) entry=&e;
      if(!e.hints[0].width && !e.hints[1].width) empty=&e;
      uint32_t a=(uint32_t)(now-e.hints[0].observed); if(a>=age) {age=a;oldest=&e;}
    }
    if(!entry) { entry=empty?empty:oldest; *entry=Entry{}; memcpy(entry->key,id.pub_key,32); }
    uint32_t fp=trace_storage::checksum(data,length)^origin;
    for(const auto& h:entry->hints) if(h.width && h.origin==origin && h.fingerprint==fp) return false;
    uint8_t width=packet->getPathHashSize(),count=packet->getPathHashCount();
    Hint* selected=nullptr;
    for(auto& h:entry->hints) if(h.width==width && h.count==count && h.profile==profile &&
        !memcmp(h.scope,scope.key,16) && !memcmp(h.via,route.path,count*width)) selected=&h;
    bool same=selected!=nullptr;
    if(!selected) selected=!entry->hints[0].width?&entry->hints[0]:!entry->hints[1].width?&entry->hints[1]:
        (uint32_t)(now-entry->hints[0].observed)>=(uint32_t)(now-entry->hints[1].observed)?&entry->hints[0]:&entry->hints[1];
    uint32_t latest=entry->hints[0].origin>entry->hints[1].origin?entry->hints[0].origin:entry->hints[1].origin;
    uint32_t validated=same?selected->validated:0;
    uint8_t misses=same?selected->misses:0;
    *selected=Hint{}; selected->validated=validated; selected->misses=misses; selected->width=width; selected->count=count; selected->profile=profile;
    memcpy(selected->via,route.path,count*width); memcpy(selected->scope,scope.key,16);
    selected->observed=now; selected->utc=trusted?utc:0; selected->origin=origin;
    selected->fingerprint=fp; selected->probation=origin<latest; selected->snr=packet->_snr;
    dirty(now); return true;
  }
  bool best(const uint8_t* key,uint32_t profile,const mesh::PacketScope& scope,uint32_t now,
            TraceRoute& out,const TraceRoute* exclude=nullptr,bool* probation=nullptr) const {
    if(!scope.reusable()) return false;
    const Hint* chosen=nullptr;
    for(const auto& e:_entries) if(!memcmp(key,e.key,32)) for(const auto& h:e.hints) {
      if(!usable(h,now) || h.profile!=profile || memcmp(h.scope,scope.key,16)) continue;
      TraceRoute candidate{};
      if(TraceRoute::fromOutbound(h.via,((h.width-1)<<6)|h.count,key,candidate)!=TraceRoute::VALID ||
         (exclude && candidate.equals(*exclude))) continue;
      bool better=!chosen;
      if(chosen) {
        if(bool(h.validated)!=bool(chosen->validated)) better=h.validated!=0;
        else if(h.misses!=chosen->misses) better=h.misses<chosen->misses;
        else if(h.probation!=chosen->probation) better=!h.probation;
        else if(h.count!=chosen->count) better=h.count<chosen->count;
        else if(h.observed!=chosen->observed) better=(uint32_t)(now-h.observed)<(uint32_t)(now-chosen->observed);
        else better=h.snr>chosen->snr;
      }
      if(better) { chosen=&h; out=candidate; if(probation) *probation=h.probation; }
    }
    return chosen!=nullptr;
  }
  void outcome(const uint8_t* key,const TraceRoute& route,uint32_t profile,const mesh::PacketScope& scope,uint32_t now,bool success) {
    for(auto& e:_entries) if(!memcmp(e.key,key,32)) for(auto& h:e.hints) if(h.profile==profile && !memcmp(h.scope,scope.key,16) && h.width==1+route.flags &&
        h.count+1==route.repeaters && !memcmp(h.via,route.path,h.count*h.width)) {
      if(success) { h.validated=now?now:1; h.misses=0; } else if(h.misses<255) ++h.misses;
    }
  }
  void loop(uint32_t now) {
    if(!_fs || !_dirty || (uint32_t)(now-_dirty_at)<30000 || (_saved && (uint32_t)(now-_saved_at)<600000)) return;
    _saved=true; _saved_at=now;
    // Keep only populated entries and never require primary+backup+temp space.
    if(!_primary_valid && _fs->exists("/trace_adverts.bak")) {
      _fs->remove("/trace_adverts");
      if(!_fs->rename("/trace_adverts.bak","/trace_adverts")) return;
      _primary_valid=true;
    }
    if(_primary_valid) _fs->remove("/trace_adverts.bak");
    Header h{}; h.magic=0x32414354; memcpy(h.self,_self,32); h.crc=2166136261UL;
    for(const auto& e:_entries) if(e.hints[0].width || e.hints[1].width) {
      h.size+=sizeof(Entry); h.crc=trace_storage::checksum(&e,sizeof(e),h.crc);
    }
    _fs->remove("/trace_adverts.tmp"); auto f=trace_storage::open(_fs,"/trace_adverts.tmp",true);
    bool ok=f && f.write((uint8_t*)&h,sizeof(h))==sizeof(h);
    for(const auto& e:_entries) if(ok && (e.hints[0].width || e.hints[1].width))
      ok=f.write((const uint8_t*)&e,sizeof(e))==sizeof(e);
    f.close();
    bool moved=false;
    if(ok && _fs->exists("/trace_adverts")) {
      _fs->remove("/trace_adverts.bak");
      moved=_fs->rename("/trace_adverts","/trace_adverts.bak"); ok=moved;
    }
    if(ok) ok=_fs->rename("/trace_adverts.tmp","/trace_adverts");
    if(ok) { _dirty=false; _primary_valid=true; }
    else {
      if(moved) _fs->rename("/trace_adverts.bak","/trace_adverts");
      _fs->remove("/trace_adverts.tmp");
    }
  }
};
static_assert(sizeof(TraceAdvertRouteCache)<=10*1024,"Advert route cache exceeds budget");
