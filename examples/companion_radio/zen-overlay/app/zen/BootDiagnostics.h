#pragma once

#include <stdint.h>
#include "OperationResult.h"

#if defined(NRF52_PLATFORM)
#include <nrf.h>
#endif

namespace zen {

// Reads the hardware reset-cause register directly; never touches flash and
// does not depend on NRF52_POWER_MANAGEMENT (not enabled for this variant).
class BootDiagnostics {
public:
  static bool abnormalReset(OperationReason& reason) {
#if defined(NRF52_PLATFORM)
    uint32_t cause = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = 0xFFFFFFFF; // write-1-to-clear for the next boot
    if (cause & POWER_RESETREAS_DOG_Msk) { reason = OperationReason::WATCHDOG_RESET; return true; }
    if (cause & POWER_RESETREAS_LOCKUP_Msk) { reason = OperationReason::CPU_LOCKUP; return true; }
#else
    (void)reason;
#endif
    return false;
  }
};

} // namespace zen
