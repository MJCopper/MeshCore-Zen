#pragma once

#include <helpers/AutoDiscoverRTCClock.h>

namespace zen {

// Observes authoritative clock writes without changing the baseline RTC
// discovery, storage or ticking implementation.
class TrackingRTCClock : public AutoDiscoverRTCClock {
  uint32_t _set_generation = 0;

public:
  explicit TrackingRTCClock(mesh::RTCClock& fallback)
      : AutoDiscoverRTCClock(fallback) {}

  void setCurrentTime(uint32_t time) override {
    AutoDiscoverRTCClock::setCurrentTime(time);
    ++_set_generation;
  }

  uint32_t getSetGeneration() const { return _set_generation; }
};

}  // namespace zen
