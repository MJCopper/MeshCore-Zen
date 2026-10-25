#pragma once
#include "TraceTimingPolicy.h"
namespace zen {
// Sensor 6f5506a3 budget adapted for a local result: no public ACK/report RF.
struct TraceTransactionBudget {
  static constexpr uint32_t MAX_MS=120000, QUEUE_MS=8000;
  uint32_t duration=0,probe_reserve=0,discovery_wait=0,login_wait=0;
  uint8_t planned_probes=1;
  bool capped=false;
  bool calculate(bool guest,bool known,uint8_t repeaters,uint32_t control,uint32_t login,uint32_t probe) {
    if(!control || !login || !probe || !repeaters || repeaters>10)return false;
    discovery_wait=TraceTimingPolicy::floodTimeout(control);
    login_wait=TraceTimingPolicy::floodTimeout(login);
    uint64_t controls=2ULL*(discovery_wait+uint64_t(TraceTimingPolicy::transmitWatchdog(control)));
    if(guest)controls+=3ULL*(login_wait+uint64_t(TraceTimingPolicy::transmitWatchdog(login)))+750;
    uint64_t one=TraceTimingPolicy::attemptTimeout(repeaters,probe);
    probe_reserve=TraceTimingPolicy::bounded(one+5500);
    planned_probes=known?2:1;
    uint64_t total=QUEUE_MS+controls+one*planned_probes+5500+(known?2000:0);
    if(total>MAX_MS && planned_probes==2){planned_probes=1;total=QUEUE_MS+controls+one+5500;}
    capped=total>MAX_MS;duration=capped?MAX_MS:uint32_t((total+999)/1000)*1000;
    return uint64_t(TraceTimingPolicy::attemptTimeout(1,probe))+5500<duration;
  }
  static uint32_t remaining(uint32_t now,uint32_t deadline) {
    return int32_t(deadline-now)>0?deadline-now:0;
  }
  bool fits(const TraceRoute& route,uint32_t airtime,uint32_t delay,uint32_t now,uint32_t deadline,uint32_t used) const {
    return uint64_t(delay)+TraceTimingPolicy::attemptTimeout(route.repeaters,airtime)+
      (used<QUEUE_MS?QUEUE_MS-used:0)+TraceTimingPolicy::FINAL_GRACE_MILLIS<=remaining(now,deadline);
  }
};
}
