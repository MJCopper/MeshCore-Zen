#pragma once

#include <stdint.h>

namespace solo {

struct RelayEchoTiming {
  static const uint32_t QUEUE_WINDOW_MS = 120000;
  static const uint32_t MIN_ECHO_MS = 10000;
  static const uint32_t MAX_ECHO_MS = 30000;

  static uint32_t echoWindow(uint32_t airtime_ms) {
    if (airtime_ms >= MAX_ECHO_MS / 4) return MAX_ECHO_MS;
    uint32_t window = airtime_ms * 4;
    return window < MIN_ECHO_MS ? MIN_ECHO_MS : window;
  }

  static bool noRelayHeard(bool tracked, bool transmitted, uint8_t heard) {
    return tracked && transmitted && heard == 0;
  }
};

} // namespace solo
