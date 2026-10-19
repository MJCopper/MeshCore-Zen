#pragma once

#include <CayenneLPP.h>
#include <math.h>
#include "BME680AirQuality.h"

// The sampler owns this snapshot. Requests only read it; calibration is RAM-only.
class Bme680Telemetry {
  BME680AirQuality _quality;
  bool _ready = false;
  bool _air_ready = false;
  uint8_t _channel = 0;
  float _altitude_offset;
public:
  float temperature = NAN;
  float humidity = NAN;
  float pressure = NAN;
  float altitude = NAN;
  float air_quality = NAN;
  float voltage = NAN;

  explicit Bme680Telemetry(float altitude_offset = 100.0f) : _altitude_offset(altitude_offset) { }
  bool ingest(CayenneLPP& raw);
  void setVoltage(float value) { if (isfinite(value) && value > 0) voltage = value; }
  bool ready() const { return _ready; }
  bool airReady() const { return _air_ready; }
  uint8_t samples() const { return _quality.samples(); }
  uint8_t channel() const { return _channel; }
  void appendEnvironment(CayenneLPP& output, uint8_t permissions = 0xFF) const;
  void encode(CayenneLPP& output, uint8_t permissions = 0xFF) const;
};
