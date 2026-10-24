#pragma once
#include <stdint.h>

namespace zen {
// UI-only, fixed storage. Each authoritative operation reports its slot and
// generation once per UITask loop. Missing operations release at endSample(),
// so chained stages and replacements cannot briefly expose an expired timer.
class DisplayActivity {
public:
  enum Owner : unsigned { DM_FIRST=0, CHANNEL_FIRST=32, REMOTE=80, SENSOR,
    PING, DISCOVERY, GAME, ADVERT_FIRST, CAPACITY=ADVERT_FIRST+4 };
  struct Token { unsigned owner; uint32_t generation; };
  struct Decision { bool hold; bool released; uint32_t deadline; };
  static constexpr uint32_t RECOVERY_MS=3600000; // missing callback safety ceiling
private:
  struct Slot {
    uint32_t generation=0, deadline=0;
    bool present=false,seen=false,suppressed=false;
  } _slots[CAPACITY];
  bool _held=false,_allowed=false;
  uint32_t _now=0;
public:
  void beginSample(uint32_t now,bool allowed) {
    _now=now;_allowed=allowed;
    for(auto& s:_slots) s.seen=false;
    if(!allowed) suspend();
  }
  // deadline is the operation's current wait deadline plus recovery grace.
  // Zero uses a fixed ceiling measured from acquisition, never from rendering.
  bool observe(Token token,bool active,uint32_t deadline=0) {
    if(token.owner>=CAPACITY || !token.generation) return false;
    auto& s=_slots[token.owner];
    if(!active) return finish(token);
    if(!s.present || s.generation!=token.generation) {
      s.generation=token.generation; s.deadline=deadline?deadline:_now+RECOVERY_MS;
      s.present=s.seen=true;s.suppressed=!_allowed;
    } else {
      s.seen=true;
      if(deadline) s.deadline=deadline;
    }
    if(!_allowed || (int32_t)(_now-s.deadline)>=0) s.suppressed=true;
    return true;
  }
  bool finish(Token token) {
    if(token.owner>=CAPACITY) return false;
    auto& s=_slots[token.owner];
    if(!s.present || s.generation!=token.generation) return false;
    s.present=false;return true;
  }
  // Keep identities as tombstones until their operations disappear. Waking
  // cannot reacquire an old operation, but a genuinely new generation can.
  void suspend() {
    for(auto& s:_slots) if(s.present)s.suppressed=true;
    _held=false;
  }
  Decision endSample(uint32_t normal_timeout=0) {
    bool hold=false;
    for(auto& s:_slots) {
      if(!s.seen) s.present=false;
      if(s.present && !s.suppressed) hold=true;
    }
    bool released=_allowed && _held && !hold;
    Decision d{hold,released,released && normal_timeout ? _now+normal_timeout : 0};
    _held=hold;return d;
  }
  static bool shouldBlank(bool held,uint32_t now,uint32_t deadline,
                          uint32_t normal_timeout,bool notification_only) {
    return !held && (normal_timeout || notification_only) && (int32_t)(now-deadline)>=0;
  }
};
} // namespace zen
