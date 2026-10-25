#pragma once
#include "TraceStorage.h"
class TraceGuestPolicy {
  FILESYSTEM* _fs=nullptr;
  bool _enabled=true;
  struct Record { uint32_t magic, enabled, crc; };
public:
  void begin(FILESYSTEM* fs) {
    _fs=fs; Record r{};
    bool valid=trace_storage::read(fs,"/trace_policy",&r,sizeof(r)) && r.magic==0x31504754 &&
        r.enabled<=1 && r.crc==trace_storage::checksum(&r,sizeof(r)-4);
    _enabled=valid?r.enabled:!(fs && fs->exists("/trace_policy"));
  }
  bool enabled() const { return _enabled; }
  bool set(bool value) {
    Record r{0x31504754, value?1u:0u, 0}; r.crc=trace_storage::checksum(&r,sizeof(r)-4);
    if(!trace_storage::write(_fs,"/trace_policy",&r,sizeof(r))) return false;
    _enabled=value; return true;
  }
};
