#pragma once

#include <helpers/sensors/EnvironmentSensorManager.h>
#include "Bme680Telemetry.h"

#ifndef TELEM_BME680_ALTITUDE_OFFSET_M
  #define TELEM_BME680_ALTITUDE_OFFSET_M 100.0f
#endif

// Reuse the upstream driver and apply the installation's telemetry policy here.
class Bme680SensorManager : public EnvironmentSensorManager {
  CayenneLPP _raw{MAX_PACKET_PAYLOAD - 4};
public:
  Bme680Telemetry snapshot{TELEM_BME680_ALTITUDE_OFFSET_M};
  bool querySensors(uint8_t permissions, CayenneLPP& output) override {
    _raw.reset();
    bool queried = EnvironmentSensorManager::querySensors(permissions, _raw);
    snapshot.ingest(_raw);
    snapshot.appendEnvironment(output, permissions);
    return queried;
  }
};
