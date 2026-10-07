#pragma once

#include "../ZenStore.h"
#include "../OperationResult.h"

namespace zen { namespace pet {

// Only these paths cross the filesystem adapter. MeshCore storage is not exposed.
class PetSnapshotFilesystem {
  ZenStore& _store;
public:
  explicit PetSnapshotFilesystem(ZenStore& store):_store(store) {}
  static const char* path(unsigned candidate) {
    return candidate==0?"/zen_pet":candidate==1?"/zen_pet.tmp":"/zen_pet.bak";
  }
  bool available() const { return _store.available(); }
  OperationReason failureReason() const {
    using Failure=ZenStore::Failure;
    switch(_store.failure()) {
      case Failure::TEMP_VERIFY_FAILED: case Failure::PROMOTION_VERIFY_FAILED:
        return OperationReason::PET_VERIFY_FAILED;
      case Failure::BACKUP_FAILED:return OperationReason::PET_BACKUP_FAILED;
      case Failure::RENAME_FAILED: case Failure::PROMOTION_FAILED:return OperationReason::PET_PROMOTION_FAILED;
      default:return OperationReason::PET_WRITE_FAILED;
    }
  }
  bool read(unsigned candidate,uint8_t* data,size_t capacity,size_t& size) {
    size=0; if(!available())return false;
    File file=_store.openRead(path(candidate));
    if(!file)return false;
    size=file.size(); size_t count=size<capacity?size:capacity;
    bool ok=file.read(data,count)==(int)count;
    file.close(); return ok;
  }
  bool replace(const uint8_t* data,size_t size) {
    return _store.replaceExtension(path(1),path(0),path(2),data,size);
  }
};

} }
