#pragma once

#include "PetSnapshot.h"

namespace zen { namespace pet {

// Backend can only read/replace Zen pet records, never baseline files.
class PetSnapshotStore {
  uint32_t _generation=0;
  bool _locked=false;
  bool _verify_failed=false;
public:
  enum Load { RESTORED, MISSING, CORRUPT, INCOMPATIBLE, UNAVAILABLE };
  bool locked() const { return _locked; }
  bool verificationFailed() const { return _verify_failed; }
  template<class Backend> Load load(Backend& backend,PetSnapshot& snapshot) {
    if(!backend.available())return UNAVAILABLE;
    uint8_t bytes[PetSnapshotCodec::SIZE]; bool found=false,valid=false;
    PetSnapshot best; uint32_t best_generation=0;
    // Inspect every candidate for unsupported schemas before permitting writes.
    for(unsigned i=0;i<3;++i) {
      size_t size=0;
      if(!backend.read(i,bytes,sizeof(bytes),size)) { found|=size!=0; continue; }
      found=true; PetSnapshot candidate; uint32_t generation=0;
      auto status=PetSnapshotCodec::decode(bytes,size,candidate,generation);
      if(status==PetSnapshotCodec::NEWER) { _locked=true; return INCOMPATIBLE; }
      if(status==PetSnapshotCodec::VALID && !valid) {
        best=candidate; best_generation=generation; valid=true;
      }
    }
    if(!valid)return found?CORRUPT:MISSING;
    snapshot=best; _generation=best_generation; return RESTORED;
  }
  template<class Backend> bool save(Backend& backend,const PetSnapshot& snapshot) {
    _verify_failed=false;
    if(_locked || !backend.available())return false;
    uint8_t bytes[PetSnapshotCodec::SIZE]; uint32_t generation=_generation+1;
    if(!PetSnapshotCodec::encode(snapshot,generation,bytes) ||
        !backend.replace(bytes,sizeof(bytes)))return false;
    size_t size=0; uint8_t check[sizeof(bytes)];
    PetSnapshot decoded; uint32_t found=0;
    if(!backend.read(0,check,sizeof(check),size) ||
        PetSnapshotCodec::decode(check,size,decoded,found)!=PetSnapshotCodec::VALID ||
        found!=generation || memcmp(bytes,check,sizeof(bytes))) { _verify_failed=true; return false; }
    _generation=generation; return true;
  }
};

} }
