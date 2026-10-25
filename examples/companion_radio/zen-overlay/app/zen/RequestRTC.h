#pragma once
#include <MeshCore.h>
#include "TraceRequestClock.h"
namespace zen {
// Wall time stays on the physical RTC. All MeshCore unique-time producers share
// one durable sequence, including companion login, telemetry and CLI requests.
class RequestRTC:public mesh::RTCClock {
  mesh::RTCClock& _wall;
  TraceRequestClock _sequence;
  bool _ready=false,_prefetched=false;
  uint32_t _value=0,_wall_generation=0;
  bool _trusted=false,_observed_generation=false;
public:
  explicit RequestRTC(mesh::RTCClock& wall):_wall(wall){}
  void begin(FILESYSTEM* fs,const uint8_t* identity) { _sequence.begin(fs,identity);_prefetched=false;_ready=true; }
  uint32_t getCurrentTime() override { return _wall.getCurrentTime(); }
  void setCurrentTime(uint32_t value) override { _wall.setCurrentTime(value);_trusted=true; }
  void observeWallGeneration(uint32_t generation) {
    if(_observed_generation && generation!=_wall_generation)_trusted=true;
    _observed_generation=true;_wall_generation=generation;
  }
  bool trustedWallTime() const { return _trusted; }
  void tick() override { _wall.tick(); }
  bool prepare() {
    if(!_ready)return false;
    if(_prefetched && _value>=_wall.getCurrentTime())return true;
    _prefetched=_sequence.next(_wall.getCurrentTime(),_value);return _prefetched;
  }
  bool healthy() const { return _ready && _sequence.healthy(); }
  uint32_t getCurrentTimeUnique() override {
    if(!_ready)return _wall.getCurrentTimeUnique(); // pre-identity boot only
    if(!prepare())return 0;
    _prefetched=false;return _value;
  }
};
}
