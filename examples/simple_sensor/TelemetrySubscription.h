#pragma once
#include <stddef.h>
#include <string.h>
#include <helpers/sensors/LPPDataHelpers.h>

// Sensor-runtime LPP access. Humidity is one byte at 0.5%, including in
// subscription thresholds and historical queries (the 1.18 helper says two).
struct SensorLPP : LPPData {
  static uint8_t getDataSize(uint8_t type) {
    return type == LPP_RELATIVE_HUMIDITY ? 1 : LPPData::getDataSize(type);
  }
  static uint32_t getMultiplier(uint8_t type) {
    return type == LPP_RELATIVE_HUMIDITY ? 2 : LPPData::getMultiplier(type);
  }
  static bool validDeltas(const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    size_t i = 0;
    while (i < len) {
      if (len - i < 3) return false;
      uint8_t type = data[i + 1];
      // Subscription comparisons are scalar, not GPS, vectors or polylines.
      switch (type) {
        case LPP_DIGITAL_INPUT: case LPP_DIGITAL_OUTPUT: case LPP_ANALOG_INPUT:
        case LPP_ANALOG_OUTPUT: case LPP_GENERIC_SENSOR: case LPP_LUMINOSITY:
        case LPP_PRESENCE: case LPP_TEMPERATURE: case LPP_RELATIVE_HUMIDITY:
        case LPP_BAROMETRIC_PRESSURE: case LPP_VOLTAGE: case LPP_CURRENT:
        case LPP_FREQUENCY: case LPP_PERCENTAGE: case LPP_ALTITUDE:
        case LPP_CONCENTRATION: case LPP_POWER: case LPP_DISTANCE:
        case LPP_ENERGY: case LPP_DIRECTION: case LPP_UNIXTIME: case LPP_SWITCH:
          break;
        default: return false;
      }
      size_t size = getDataSize(type);
      if (i + 2 + size > len) return false;
      if (getFloat(data + i + 2, size, getMultiplier(type), isSigned(type)) < 0) return false;
      i += 2 + size;
    }
    return true;
  }
};
