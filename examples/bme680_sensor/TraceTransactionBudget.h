#pragma once
#include "TraceTimingPolicy.h"
#include <stdint.h>

// A frozen upper bound, not a required waiting period. All amounts are ms.
struct TraceTransactionBudget {
  static constexpr uint32_t DURATION_MILLIS = 120000;
  static constexpr uint32_t REPORT_RESERVE_MILLIS = 5000; // default for standalone arithmetic callers
  static constexpr uint32_t WORK_QUEUE_MILLIS = 8000, REPORT_QUEUE_MILLIS = 2000;
  uint32_t duration=0, report_reserve=0, probe_reserve=0, discovery_wait=0, login_wait=0;
  uint32_t control_air=0, login_air=0, probe_air=0, public_air=0;
  uint8_t planned_probes=0;
  bool capped=false, known_route=false;
  bool calculate(bool targeted,bool guest,bool known,uint8_t repeaters,
                 uint32_t control,uint32_t login,uint32_t probe,uint32_t public_frame) {
    if(!probe || !public_frame || !repeaters || repeaters>10 || (targeted && (!control || !login))) return false;
    control_air=control; login_air=login; probe_air=probe; public_air=public_frame; known_route=known;
    discovery_wait=TraceTimingPolicy::floodTimeout(control);
    login_wait=TraceTimingPolicy::floodTimeout(login);
    report_reserve=TraceTimingPolicy::bounded(5000ULL+2ULL*TraceTimingPolicy::transmitWatchdog(public_frame)+3000);
    probe_reserve=TraceTimingPolicy::bounded(TraceTimingPolicy::attemptTimeout(repeaters,probe)+5500ULL);
    planned_probes=known?2:1;
    // Control branch: last login succeeds without a path, requiring final REQ.
    uint64_t controls=targeted?2ULL*(discovery_wait+TraceTimingPolicy::transmitWatchdog(control)):0;
    if(targeted && guest) controls+=3ULL*(login_wait+TraceTimingPolicy::transmitWatchdog(login))+750;
    uint64_t fixed=WORK_QUEUE_MILLIS+report_reserve+TraceTimingPolicy::transmitWatchdog(public_frame)+5500ULL;
    uint64_t one=TraceTimingPolicy::attemptTimeout(repeaters,probe);
    uint64_t total=fixed+controls+one*planned_probes+(planned_probes==2?2000:0);
    if(total>DURATION_MILLIS && planned_probes==2) { planned_probes=1; total=fixed+controls+one; }
    capped=total>DURATION_MILLIS;
    duration=capped?DURATION_MILLIS:(uint32_t)(((total+999)/1000)*1000);
    return uint64_t(report_reserve)+TraceTimingPolicy::attemptTimeout(1,probe)<duration;
  }
  uint32_t seconds() const { return (duration+999)/1000; }
  uint32_t queueRemaining(uint32_t used) const {
    return used >= WORK_QUEUE_MILLIS ? 0 : WORK_QUEUE_MILLIS - used;
  }
  bool fitsProbe(uint8_t repeaters, uint32_t airtime, uint32_t scheduled_delay,
                 uint32_t now, uint32_t deadline, uint32_t queue_used) const {
    uint64_t required = (uint64_t)scheduled_delay +
        TraceTimingPolicy::attemptTimeout(repeaters, airtime) + queueRemaining(queue_used);
    return required <= work(now, deadline);
  }
  static uint32_t remaining(uint32_t now, uint32_t deadline) {
    return (int32_t)(deadline-now)>0?deadline-now:0;
  }
  static uint32_t workRemaining(uint32_t now,uint32_t deadline,uint32_t reserve=REPORT_RESERVE_MILLIS) {
    auto left=remaining(now,deadline); return left>reserve?left-reserve:0;
  }
  static uint32_t attemptWindow(uint32_t now,uint32_t deadline,uint8_t slots) {
    return slots?workRemaining(now,deadline)/slots:0;
  }
  uint32_t work(uint32_t now,uint32_t deadline) const { return workRemaining(now,deadline,report_reserve); }
};
