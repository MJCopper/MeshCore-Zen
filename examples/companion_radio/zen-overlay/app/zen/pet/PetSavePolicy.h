#pragma once

#include <stdint.h>

namespace zen { namespace pet {

class PetSavePolicy {
  uint32_t _last=0,_retry_at=0,_manual_at=0;
  uint64_t _elapsed=0;
  int64_t _date=-1,_checkpoint_date=-1;
  bool _synced=false,_manual=false,_manual_used=false,_initial=false;
  bool _auto_locked=false,_retry=false;
  bool _daily_due=false;
public:
  static int64_t date(int64_t local) { return local>=0?local/86400:(local-86399)/86400; }
  void begin(uint32_t now,bool initial,bool auto_locked,int64_t saved_date) {
    _last=now; _initial=initial; _auto_locked=auto_locked; _checkpoint_date=saved_date;
  }
  void request(uint32_t now) {
    if(_manual || (_manual_used && uint32_t(now-_manual_at)<2000))return;
    _manual=true; _manual_used=true; _manual_at=now;
  }
  void update(uint32_t now,bool enabled,bool synced,int64_t local) {
    _elapsed+=uint32_t(now-_last); _last=now; _synced=synced;
    if(synced) {
      int64_t day=date(local);
      if(day>_date) {
        _date=day;
        if(day>_checkpoint_date && _elapsed>=86400000ULL)_daily_due=true;
      }
    }
    if(!enabled)_manual=false;
  }
  bool manual() const { return _manual; }
  bool due(uint32_t now) const {
    if(_manual)return true;
    if(_auto_locked || (_retry && uint32_t(now-_retry_at)<3600000))return false;
    return _initial || (_elapsed>=86400000ULL && (!_synced || _daily_due));
  }
  int64_t checkpointDate() const { return _date>_checkpoint_date?_date:_checkpoint_date; }
  void complete(bool ok,uint32_t now) {
    _manual=false;
    if(ok) {
      _initial=false; _elapsed=0; _checkpoint_date=checkpointDate(); _retry=false;
      _daily_due=false; _auto_locked=false;
    } else { _retry=true; _retry_at=now; }
  }
};

} }
