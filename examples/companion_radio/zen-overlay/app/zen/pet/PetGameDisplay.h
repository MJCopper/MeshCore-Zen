#pragma once
#include <stdint.h>
namespace zen { namespace pet {
// Game-only activity classification; UITask owns the common display lease.
class PetGameDisplay {
  bool _playing=false,_watching=false,_held=false,_expired=false;
  uint32_t _input_at=0,_generation=0;
public:
  struct Decision { bool hold=false,expired=false,released=false; uint32_t generation=0; };
  void input(uint32_t now) {
    _input_at=now;
    if(_expired) { if(++_generation==0) ++_generation; _expired=false; }
  }
  void result(uint32_t) { _playing=false; }
  void cancel() { _playing=_watching=_expired=false; }
  Decision update(uint32_t now,bool playing,bool watching,bool allowed) {
    Decision d;
    if(!allowed) cancel();
    else if(playing) {
      if(!_playing) { if(++_generation==0) ++_generation; _input_at=now; }
      if(_watching && !watching) _input_at=now;
      _playing=true;_watching=watching;
      d.expired=!watching && uint32_t(now-_input_at)>=60000;
      _expired=d.expired;d.hold=!d.expired;
    } else { _playing=_watching=_expired=false; }
    d.released=allowed && _held && !d.hold;
    d.generation=_generation;_held=d.hold;return d;
  }
};
} }
