#pragma once
#include <stdint.h>

namespace zen {
enum class TimeSyncSource : uint8_t {
  NONE, RESTORED, GPS, COMPANION, USB_CLI, MESH
};
}
