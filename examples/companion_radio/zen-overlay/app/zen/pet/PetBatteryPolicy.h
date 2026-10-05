#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// RAM-only battery adapter. Percentage comes from Zen's existing cached sample;
// neither this policy nor the care engine owns battery hardware or timers.
class PetBatteryPolicy {
  uint8_t _band = 0;
  bool _known = false;
public:
  uint8_t update(int percent) {
    static const uint8_t THRESHOLDS[] = {50,25,10};
    if (percent < 0 || percent > 100) { _band = 0; _known = false; return rate(); }
    if (!_known) {
      _band = percent < 10 ? 3 : percent < 25 ? 2 : percent < 50 ? 1 : 0;
      _known = true;
    } else {
      // Enter a lower band at its threshold; recover two percentage points
      // above it to avoid toggling on voltage noise. Large jumps cross bands.
      while (_band < 3 && percent < THRESHOLDS[_band]) ++_band;
      while (_band > 0 && percent >= THRESHOLDS[_band-1]+2) --_band;
    }
    return rate();
  }
  uint8_t rate() const { return 5 + _band; }
};

} }
