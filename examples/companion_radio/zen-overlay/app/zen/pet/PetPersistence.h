#pragma once

#include "PetSnapshotStore.h"
#include "PetSavePolicy.h"

namespace zen { namespace pet {

// Loop-owned orchestration only. Gameplay exports values; the backend owns I/O.
class PetPersistence {
  PetSnapshotStore _store;
  PetSavePolicy _policy;
  bool _started=false,_known=false;
  bool _manual_result=false;
  bool _pending_notice=false;
  uint64_t _fingerprint=0;
public:
  enum Result { NONE, SAVED, UNCHANGED, LOW_BATTERY, STORAGE_UNAVAILABLE, LOCKED,
                SAVE_FAILED, VERIFY_FAILED, DEFERRED };
  bool started() const { return _started; }
  bool manualResult() const { return _manual_result; }
  template<class Backend,class Page> PetSnapshotStore::Load start(uint32_t now,Backend& backend,Page& page) {
    PetSnapshot snapshot;
    auto status=_store.load(backend,snapshot); _started=true;
    bool restored=status==PetSnapshotStore::RESTORED;
    if(restored) {
      page.restore(snapshot,now); _fingerprint=PetSnapshotCodec::fingerprint(snapshot); _known=true;
    }
    _policy.begin(now,status==PetSnapshotStore::MISSING,
        status==PetSnapshotStore::CORRUPT || status==PetSnapshotStore::INCOMPATIBLE ||
        status==PetSnapshotStore::UNAVAILABLE,restored?snapshot.saved_date:-1);
    return status;
  }
  template<class Backend,class Page> Result process(uint32_t now,Backend& backend,Page& page,
      bool enabled,bool synced,int64_t local,bool battery_safe,bool storage_safe) {
    _policy.update(now,enabled,synced,local);
    if(page.takeSaveRequest() && enabled)_policy.request(now);
    if(!enabled || !_policy.due(now) || page.trainingActive())return NONE;
    bool manual=_policy.manual();
    _manual_result=manual;
    auto fail=[&](Result reason) { _pending_notice=false; _policy.complete(false,now); return reason; };
    if(_store.locked())return fail(LOCKED);
    if(!backend.available())return fail(STORAGE_UNAVAILABLE);
    if(!battery_safe)return manual?fail(LOW_BATTERY):NONE;
    // Busy transport/internal flash is deferred, not a failed manual save.
    if(!storage_safe) {
      if(manual && !_pending_notice) { _pending_notice=true; return DEFERRED; }
      return NONE;
    }
    _pending_notice=false;
    PetSnapshot snapshot=page.checkpoint(); snapshot.saved_date=_policy.checkpointDate();
    uint64_t fingerprint=PetSnapshotCodec::fingerprint(snapshot);
    if(_known && fingerprint==_fingerprint) {
      _policy.complete(true,now); return manual?UNCHANGED:NONE;
    }
    if(!_store.save(backend,snapshot))return fail(_store.verificationFailed()?VERIFY_FAILED:SAVE_FAILED);
    _fingerprint=fingerprint; _known=true; _policy.complete(true,now);
    return manual?SAVED:NONE;
  }
};

} }
