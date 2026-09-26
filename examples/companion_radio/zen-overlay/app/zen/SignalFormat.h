#pragma once

#include <stdint.h>
#include <stdio.h>

namespace zen {

// MeshCore carries SNR as signed quarter-dB units. Render one decimal place
// using integer rounding, omitting the sign for the usual positive case.
inline bool formatQuarterDb(char* out, size_t size, int snr_x4) {
  if (!out || !size) return false;
  int magnitude = snr_x4 < 0 ? -snr_x4 : snr_x4;
  int tenths = (magnitude * 10 + 2) / 4;
  int n = snprintf(out, size, "%s%d.%d", snr_x4 < 0 ? "-" : "",
                   tenths / 10, tenths % 10);
  return n >= 0 && (size_t)n < size;
}

} // namespace zen
