#pragma once
#include "TraceStorage.h"
#include <limits.h>
#include <initializer_list>

namespace zen {

// Both replicas cover a range before any value from it can be emitted.
class TraceRequestClock {
  struct Record { uint32_t magic; uint8_t key[32]; uint32_t upper, crc; };
  FILESYSTEM* _fs = nullptr;
  uint8_t _key[32]{};
  uint32_t _next = 0, _upper = 0;
  bool _healthy = false;
  char _path_a[48]{},_path_b[48]{};
  bool valid(const Record& r) const {
    return r.magic == 0x31514354 && !memcmp(r.key,_key,32) &&
        r.crc == trace_storage::checksum(&r, sizeof(r)-4);
  }
public:
  void begin(FILESYSTEM* fs, const uint8_t* key) {
    _fs=fs; memcpy(_key,key,32); _next=_upper=0;
    char prefix[17];for(unsigned i=0;i<8;++i)sprintf(prefix+2*i,"%02x",key[i]);
    snprintf(_path_a,sizeof(_path_a),"/trace_clock_a_%s",prefix);
    snprintf(_path_b,sizeof(_path_b),"/trace_clock_b_%s",prefix);
    Record a{},b{};
    bool av=trace_storage::read(fs,_path_a,&a,sizeof(a)) && valid(a);
    bool bv=trace_storage::read(fs,_path_b,&b,sizeof(b)) && valid(b);
    bool exists=fs && (fs->exists(_path_a) || fs->exists(_path_b));
    _healthy=fs && (!exists || av || bv);
    if(av) _next=a.upper;
    if(bv && b.upper>_next) _next=b.upper;
    // Preserve matching sensor-format reservations during firmware migration.
    // Scoped records let an explicitly changed/restored identity retain its own floor.
    bool legacy_exists=false,legacy_valid=false;
    for(const char* legacy:{"/trace_clock_a","/trace_clock_b"}) {
      Record r{};
      if(fs && fs->exists(legacy))legacy_exists=true;
      if(trace_storage::read(fs,legacy,&r,sizeof(r)) && r.magic==0x31514354 &&
         r.crc==trace_storage::checksum(&r,sizeof(r)-4)) {
        legacy_valid=true;
        if(valid(r) && r.upper>_next)_next=r.upper;
      }
    }
    if(!exists && legacy_exists && !legacy_valid)_healthy=false;
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
      if(!trace_storage::write(_fs,_path_a,&r,sizeof(r)) ||
         !trace_storage::write(_fs,_path_b,&r,sizeof(r))) { _healthy=false; return false; }
      Record a{},b{};
      if(!trace_storage::read(_fs,_path_a,&a,sizeof(a)) || memcmp(&a,&r,sizeof(r)) ||
         !trace_storage::read(_fs,_path_b,&b,sizeof(b)) || memcmp(&b,&r,sizeof(r))) {
        _healthy=false; return false;
      }
      _upper=r.upper;
    }
    value=candidate; _next=candidate+1; return true;
  }
};
} // namespace zen
