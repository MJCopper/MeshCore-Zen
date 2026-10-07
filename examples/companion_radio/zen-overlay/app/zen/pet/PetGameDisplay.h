#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// RAM-only auto-off lease. Never wakes a display or overrides manual sleep.
class PetGameDisplay {
  bool _playing=false,_watching=false,_result=false,_held=false;
  uint32_t _input_at=0,_result_at=0;
public:
  struct Decision { bool hold=false,expired=false,released=false; };
  void input(uint32_t now) { _input_at=now; }
  void result(uint32_t now) { _playing=false; _result=true; _result_at=now; }
  void cancel() { _playing=_watching=_result=false; }
  Decision update(uint32_t now,bool playing,bool watching,bool allowed) {
    Decision d;
    if(!allowed) cancel();
    else if(playing) {
      if(!_playing || (_watching && !watching)) _input_at=now;
      _playing=true; _watching=watching; _result=false;
      d.expired=!watching && uint32_t(now-_input_at)>=60000;
      d.hold=!d.expired;
    } else {
      _playing=_watching=false;
      if(_result && uint32_t(now-_result_at)>=5000)_result=false;
      d.hold=_result;
    }
    // Safety/visibility loss must not extend an already expired normal timer.
    d.released=allowed && _held && !d.hold && !d.expired;
    _held=d.hold;
    return d;
  }
};

} }
