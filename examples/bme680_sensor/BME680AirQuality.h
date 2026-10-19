#pragma once

#include <stdint.h>

// Lightweight, RAM-only relative air-quality estimate for the BME680. This is
// not Bosch BSEC IAQ: it is intended to make changes in a fixed installation
// easier to interpret without persisting calibration state to flash.
class BME680AirQuality {
  static const uint8_t WARMUP_SAMPLES = 10;

  float _gas_baseline;
  uint8_t _samples;

public:
  BME680AirQuality() : _gas_baseline(0.0f), _samples(0) { }

  // Returns true when score contains a usable 0 (best) to 500 (worst) value.
  bool update(float gas_ohms, float humidity_percent, float& score);
  uint8_t samples() const { return _samples; }
};
