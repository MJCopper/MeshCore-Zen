#pragma once
#include <stdint.h>
namespace solo {
class PinAttemptLimiter {
  uint8_t _failures = 0;
  uint32_t _blocked_until = 0;
public:
  void reset() { _failures = 0; _blocked_until = 0; }
  uint32_t remainingSeconds(uint32_t now) const {
    return (int32_t)(now - _blocked_until) >= 0 ? 0 : (_blocked_until - now + 999) / 1000;
  }
  uint32_t failed(uint32_t now) {
    if (_failures < 255) _failures++;
    uint32_t seconds = _failures == 4 ? 5 : _failures == 5 ? 15 : _failures >= 6 ? 30 : 0;
    _blocked_until = now + seconds * 1000UL;
    return seconds;
  }
};
} // namespace solo
