#pragma once

#include <stdint.h>

namespace zen {

class TimeDeadline {
public:
  static bool due(uint32_t now, uint32_t deadline) {
    return deadline == 0 || (int32_t)(now - deadline) >= 0;
  }
  static bool active(uint32_t now, uint32_t deadline) {
    return deadline != 0 && (int32_t)(now - deadline) < 0;
  }
  static bool after(uint32_t first, uint32_t second) {
    return (int32_t)(first - second) > 0;
  }
};

} // namespace zen
