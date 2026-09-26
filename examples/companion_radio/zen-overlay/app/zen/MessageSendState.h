#pragma once

#include <stdint.h>
#include "MessageDeliveryCoordinator.h"

namespace zen {

struct MessageSendState {
  uint32_t ack_tag = 0;
  uint32_t ack_deadline_ms = 0;
  uint32_t timestamp = 0;
  uint8_t route = ROUTE_NONE;
  void reset() { ack_tag = ack_deadline_ms = timestamp = 0; route = ROUTE_NONE; }
};

} // namespace zen
