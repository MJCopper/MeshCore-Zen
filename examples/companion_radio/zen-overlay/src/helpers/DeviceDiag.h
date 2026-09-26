#pragma once
#include <stdint.h>

// Cross-platform free-heap / stack-headroom estimate for the on-device
// diagnostics screen. A total_bytes of 0 means "not supported on this
// platform" — callers should display "N/A" rather than 0/0.
struct DeviceDiag {
  static void getHeapStats(uint32_t& free_bytes, uint32_t& total_bytes);
  static uint32_t getStackFreeBytes();   // current task's min-ever headroom; 0 = unsupported
};
