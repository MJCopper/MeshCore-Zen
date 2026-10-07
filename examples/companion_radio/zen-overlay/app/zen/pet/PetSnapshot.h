#pragma once

#include "PetMeshRewards.h"
#include <stddef.h>

namespace zen { namespace pet {

struct PetSnapshot {
  Engine::Checkpoint engine;
  PetMeshRewards::Checkpoint rewards;
  uint8_t temperament=0;
  int64_t saved_date=-1;
};

// Fixed little-endian record. No structure padding, pointers or uptime anchors.
struct PetSnapshotCodec {
  static constexpr size_t SIZE=96;
  static constexpr uint16_t VERSION=1;
  enum Status { VALID, INVALID, NEWER };
  static uint32_t checksum(const uint8_t* p,size_t n) {
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<n;++i) {
      crc^=p[i];
      for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320UL:0);
    }
    return ~crc;
  }
  static bool valid(const PetSnapshot& s) {
    const auto& e=s.engine; const auto& v=e.state; const auto& r=s.rewards.progress;
    return v.form<Evolution::FORMS && v.energy<=100 && v.fullness<=100 &&
        v.bond<=100 && v.food<=5 && v.xp<=Evolution::xp(Evolution::LEVELS) &&
        s.temperament<4 && e.cooldown<=300000 && e.energy_credit<3600000 &&
        e.hunger_credit<3600000 && e.food_credit<14400000 && r.bond<=5 &&
        r.pending_bond<=100 && r.pending_xp<=Evolution::xp(Evolution::LEVELS) &&
        s.saved_date>=-1 && s.saved_date<=4000000 &&
        s.rewards.day.date>=-4000000 && s.rewards.day.date<=4000000;
  }
  static bool encode(const PetSnapshot& s,uint32_t generation,uint8_t* out) {
    if(!valid(s) || !out)return false;
    memset(out,0,SIZE); size_t at=0;
    auto put=[&](uint64_t value,unsigned bytes) {
      for(unsigned i=0;i<bytes;++i) { out[at++]=value&255; value>>=8; }
    };
    put(0x5445505aUL,4); put(VERSION,2); put(SIZE,2); put(generation,4);
    const auto& e=s.engine; const auto& v=e.state; const auto& r=s.rewards.progress;
    put(v.form,1); put(v.energy,1); put(v.fullness,1); put(v.bond,1); put(v.food,1);
    put(v.xp,2); put(s.temperament,1);
    put(e.cooldown,4); put(e.energy_credit,4); put(e.food_credit,4); put(e.hunger_credit,4);
    put(r.dm | (r.shared<<1) | (r.conversation<<2) | (r.favourite<<3),1);
    put(r.bond,1); put(r.pending_xp,2); put(r.pending_bond,1);
    put(s.rewards.day.date,8); put(s.rewards.day.elapsed,8); put(s.rewards.day.synced,1);
    put(s.saved_date,8);
    at=SIZE-4; put(checksum(out,SIZE-4),4); return true;
  }
  static Status decode(const uint8_t* in,size_t size,PetSnapshot& s,uint32_t& generation) {
    if(!in || size<8)return INVALID;
    size_t at=0;
    auto get=[&](unsigned bytes) {
      uint64_t value=0;
      for(unsigned i=0;i<bytes;++i)value|=uint64_t(in[at++])<<(8*i);
      return value;
    };
    if(get(4)!=0x5445505aUL)return INVALID;
    unsigned version=get(2);
    if(version>VERSION)return NEWER;
    if(size!=SIZE || version!=VERSION || get(2)!=SIZE)return INVALID;
    size_t crc_at=SIZE-4;
    uint32_t crc=0; for(unsigned i=0;i<4;++i)crc|=uint32_t(in[crc_at+i])<<(8*i);
    if(crc!=checksum(in,SIZE-4))return INVALID;
    generation=get(4); PetSnapshot decoded;
    auto& e=decoded.engine; auto& v=e.state; auto& r=decoded.rewards.progress;
    v.form=get(1); v.energy=get(1); v.fullness=get(1); v.bond=get(1); v.food=get(1);
    v.xp=get(2); decoded.temperament=get(1);
    e.cooldown=get(4); e.energy_credit=get(4); e.food_credit=get(4); e.hunger_credit=get(4);
    unsigned flags=get(1); if(flags>15)return INVALID;
    r.dm=flags&1; r.shared=flags&2; r.conversation=flags&4; r.favourite=flags&8;
    r.bond=get(1); r.pending_xp=get(2); r.pending_bond=get(1);
    decoded.rewards.day.date=(int64_t)get(8); decoded.rewards.day.elapsed=get(8);
    unsigned synced=get(1); if(synced>1)return INVALID;
    decoded.rewards.day.synced=synced; decoded.saved_date=(int64_t)get(8);
    if(!valid(decoded))return INVALID;
    s=decoded; return VALID;
  }
  static uint64_t fingerprint(PetSnapshot s) {
    s.saved_date=-1; s.rewards.day.elapsed=0;
    uint8_t bytes[SIZE]; if(!encode(s,0,bytes))return 0;
    uint64_t hash=14695981039346656037ULL;
    for(uint8_t b:bytes)hash=(hash^b)*1099511628211ULL;
    return hash;
  }
};

} }
