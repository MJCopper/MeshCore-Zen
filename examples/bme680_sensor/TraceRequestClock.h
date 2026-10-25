#pragma once
#include "TraceStorage.h"
#include <limits.h>

// Both replicas cover a range before any value from it can be emitted.
class TraceRequestClock {
  struct Record { uint32_t magic; uint8_t key[32]; uint32_t upper, crc; };
  FILESYSTEM* _fs = nullptr;
  uint8_t _key[32]{};
  uint32_t _next = 0, _upper = 0;
  bool _healthy = false;
  bool valid(const Record& r) const {
    return r.magic == 0x31514354 && !memcmp(r.key,_key,32) &&
        r.crc == trace_storage::checksum(&r, sizeof(r)-4);
  }
public:
  void begin(FILESYSTEM* fs, const uint8_t* key) {
    _fs=fs; memcpy(_key,key,32); _next=_upper=0;
    Record a{},b{};
    bool av=trace_storage::read(fs,"/trace_clock_a",&a,sizeof(a)) && valid(a);
    bool bv=trace_storage::read(fs,"/trace_clock_b",&b,sizeof(b)) && valid(b);
    bool exists=fs && (fs->exists("/trace_clock_a") || fs->exists("/trace_clock_b"));
    _healthy=fs && (!exists || av || bv);
    if(av) _next=a.upper;
    if(bv && b.upper>_next) _next=b.upper;
  }
  bool healthy() const { return _healthy; }
  uint32_t reservedUpper() const { return _upper; }
  bool next(uint32_t rtc, uint32_t& value) {
    if(!_healthy) return false;
    uint32_t candidate=rtc>_next ? rtc : _next;
    if(candidate==0 || candidate>UINT32_MAX-256) { _healthy=false; return false; }
    if(candidate>=_upper) {
      Record r{}; r.magic=0x31514354; memcpy(r.key,_key,32); r.upper=candidate+256;
      r.crc=trace_storage::checksum(&r,sizeof(r)-4);
      if(!trace_storage::write(_fs,"/trace_clock_a",&r,sizeof(r)) ||
         !trace_storage::write(_fs,"/trace_clock_b",&r,sizeof(r))) { _healthy=false; return false; }
      Record a{},b{};
      if(!trace_storage::read(_fs,"/trace_clock_a",&a,sizeof(a)) || memcmp(&a,&r,sizeof(r)) ||
         !trace_storage::read(_fs,"/trace_clock_b",&b,sizeof(b)) || memcmp(&b,&r,sizeof(r))) {
        _healthy=false; return false;
      }
      _upper=r.upper;
    }
    value=candidate; _next=candidate+1; return true;
  }
};
