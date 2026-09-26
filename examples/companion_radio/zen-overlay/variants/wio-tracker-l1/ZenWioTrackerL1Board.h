#pragma once

#include <WioTrackerL1Board.h>

// Zen adds stable battery-divider sampling without copying ownership of the
// Wio board implementation from MeshCore.
class ZenWioTrackerL1Board : public WioTrackerL1Board {
public:
  // NRF52Board is a virtual base, so the most-derived Zen board owns its
  // construction just as the baseline Wio board does.
  ZenWioTrackerL1Board() : NRF52Board("WioTrackerL1 OTA"), WioTrackerL1Board() {}
  void begin();

  uint16_t getBattMilliVolts() override {
    // VBAT_ENABLE is held HIGH continuously (see begin()/initVariant()) so the
    // divider node is already settled — don't gate it per-read, that reads high
    // and jittery because 10ms wasn't enough for the divider to stabilize.
    analogReadResolution(12);
    analogReference(AR_INTERNAL);
    int adcvalue = analogRead(PIN_VBAT_READ);
    return (adcvalue * ADC_MULTIPLIER * AREF_VOLTAGE) / 4.096;
  }

};
