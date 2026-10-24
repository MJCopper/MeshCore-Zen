#pragma once
#include <stdint.h>
#include "DisplayActivity.h"
namespace zen {
// Only explicit on-device transmissions enter this bounded observer. Packet
// ownership remains with Dispatcher; expiration never frees a queued packet.
class ManualTransmitActivity {
public:
  static constexpr unsigned CAPACITY=4;
  static constexpr uint32_t QUEUE_MS=120000;
private:
  struct Slot { const void* packet=nullptr; uint32_t generation=0,deadline=0; } _slots[CAPACITY];
  uint32_t _sequence=0;
public:
  bool begin(const void* packet,uint32_t now) {
    if(!packet)return false;
    for(auto& s:_slots) {
      if(s.packet && (int32_t)(now-s.deadline)<0) continue;
      s.packet=packet;s.deadline=now+QUEUE_MS;
      if(++_sequence==0) ++_sequence;
      s.generation=_sequence;return true;
    }
    return false;
  }
  void started(const void* packet,uint32_t now,uint32_t airtime) {
    if(!packet)return;
    for(auto& s:_slots) if(s.packet==packet) s.deadline=now+airtime+5000;
  }
  bool complete(const void* packet) {
    if(!packet)return false;
    for(auto& s:_slots) if(s.packet==packet){s.packet=nullptr;return true;}
    return false;
  }
  void clear() { for(auto& s:_slots)s.packet=nullptr; }
  void observe(DisplayActivity& activity,uint32_t now) const {
    for(unsigned i=0;i<CAPACITY;++i) {
      const auto& s=_slots[i];
      activity.observe({DisplayActivity::ADVERT_FIRST+i,s.generation},
                       s.packet && (int32_t)(now-s.deadline)<0,s.deadline);
    }
  }
};
}
