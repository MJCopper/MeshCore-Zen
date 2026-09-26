#pragma once

#include <stdint.h>

namespace zen {

// Shared retry policy for on-device requests that may be carrying a stale
// contact path. It owns no radio state, so login and telemetry flows can apply
// the same escalation without coupling their different UI state machines.
class NodeRouteRetry {
public:
  enum Action : uint8_t { EXHAUSTED, RETRY_PATH, RETRY_FLOOD };

private:
  uint8_t _path_retries_left = 0;
  uint8_t _flood_tries_left = 0;

public:
  bool exhausted() const {
    return _path_retries_left == 0 && _flood_tries_left == 0;
  }

  void reset() {
    _path_retries_left = 0;
    _flood_tries_left = 0;
  }

  void begin(bool has_known_path) {
    // The initial request has already been sent. A known route gets one more
    // path try followed by three floods; an unknown route gets two more floods.
    _path_retries_left = has_known_path ? 1 : 0;
    _flood_tries_left = has_known_path ? 3 : 2;
  }

  Action next(bool path_available = true) {
    if (_path_retries_left) {
      _path_retries_left--;
      if (path_available) return RETRY_PATH;
    }
    if (_flood_tries_left) {
      _flood_tries_left--;
      return RETRY_FLOOD;
    }
    return EXHAUSTED;
  }
};

} // namespace zen
