#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <helpers/sensors/LPPDataHelpers.h>

namespace zen {

// One on-demand reply, kept in RAM. Stable channel ordering retains the wire
// order within a channel. Validate lengths before decoding untrusted packets.
class SensorTelemetry {
  uint8_t _data[255] = {};
  uint8_t _offsets[85] = {};
  uint8_t _count = 0;
  bool _invalid = false;

  static int size(uint8_t type) {
    switch (type) {
      case LPP_DIGITAL_INPUT: case LPP_DIGITAL_OUTPUT: case LPP_PRESENCE:
      case LPP_RELATIVE_HUMIDITY: case LPP_PERCENTAGE: case LPP_SWITCH: return 1;
      case LPP_ANALOG_INPUT: case LPP_ANALOG_OUTPUT: case LPP_LUMINOSITY:
      case LPP_TEMPERATURE: case LPP_BAROMETRIC_PRESSURE: case LPP_VOLTAGE:
      case LPP_CURRENT: case LPP_ALTITUDE: case LPP_CONCENTRATION:
      case LPP_POWER: case LPP_DIRECTION: return 2;
      case LPP_COLOUR: return 3;
      case LPP_GENERIC_SENSOR: case LPP_FREQUENCY: case LPP_DISTANCE:
      case LPP_ENERGY: case LPP_UNIXTIME: return 4;
      case LPP_ACCELEROMETER: case LPP_GYROMETER: return 6;
      case LPP_GPS: return 9;
      default: return 0;
    }
  }
  static uint32_t number(const uint8_t* p, int n) {
    uint32_t v = 0;
    while (n--) v = (v << 8) | *p++;
    return v;
  }
  static int components(uint8_t type) {
    return type == LPP_GPS || type == LPP_ACCELEROMETER ||
           type == LPP_GYROMETER || type == LPP_COLOUR ? 3 : 1;
  }

public:
  void clear() { _count = 0; _invalid = false; }
  bool invalid() const { return _invalid; }
  void load(const uint8_t* data, uint8_t len) {
    clear();
    if (len) memcpy(_data, data, len);
    unsigned pos = 0;
    while (pos < len && _data[pos] != 0) {
      if (pos + 2 >= len) { _invalid = true; break; }
      int n = size(_data[pos + 1]);
      // Unknown types have no trustworthy length; do not interpret their
      // payload bytes as another record.
      if (!n || pos + 2 + n > len) { _invalid = true; break; }
      int i = _count++;
      while (i > 0 && _data[_offsets[i - 1]] < _data[pos]) {
        _offsets[i] = _offsets[i - 1];
        i--;
      }
      _offsets[i] = (uint8_t)pos;
      pos += 2 + n;
    }
  }
  int rows() const {
    int n = 0;
    for (int i = 0; i < _count; i++) n += components(_data[_offsets[i] + 1]);
    return n;
  }
  bool firstValue(uint8_t wanted_type, float& out) const {
    for (int i = 0; i < _count; i++) {
      const uint8_t offset = _offsets[i];
      const uint8_t type = _data[offset + 1];
      if (type != wanted_type || components(type) != 1) continue;
      const uint8_t* p = &_data[offset + 2];
      int n = size(type);
      uint32_t raw = number(p, n);
      bool signed_value = false;
      double divisor = 1.0;
      switch (type) {
        case LPP_ANALOG_INPUT: case LPP_ANALOG_OUTPUT:
          divisor = 100.0; signed_value = true; break;
        case LPP_TEMPERATURE: divisor = 10.0; signed_value = true; break;
        case LPP_RELATIVE_HUMIDITY: divisor = 2.0; break;
        case LPP_BAROMETRIC_PRESSURE: divisor = 10.0; break;
        case LPP_VOLTAGE: divisor = 100.0; break;
        case LPP_CURRENT: divisor = 1000.0; signed_value = true; break;
        case LPP_ALTITUDE: signed_value = true; break;
        case LPP_DISTANCE: case LPP_ENERGY: divisor = 1000.0; break;
        default: break;
      }
      double value = raw;
      if (signed_value && (raw & (1UL << (n * 8 - 1))))
        value -= (double)(1ULL << (n * 8));
      out = (float)(value / divisor);
      return true;
    }
    return false;
  }
  bool format(int row, char* out, size_t capacity) const {
    if (!out || !capacity || row < 0) return false;
    out[0] = 0;
    int i = 0;
    for (; i < _count; i++) {
      int n = components(_data[_offsets[i] + 1]);
      if (row < n) break;
      row -= n;
    }
    if (i == _count) return false;
    uint8_t type = _data[_offsets[i] + 1];
    const uint8_t* p = &_data[_offsets[i] + 2];
    const char* label = "Value";
    const char* unit = "";
    int n = size(type), precision = 0;
    double divisor = 1;
    bool signed_value = false;
    switch (type) {
      case LPP_DIGITAL_INPUT: label = "Input"; break;
      case LPP_DIGITAL_OUTPUT: label = "Output"; break;
      case LPP_ANALOG_INPUT: case LPP_ANALOG_OUTPUT:
        label = type == LPP_ANALOG_INPUT ? "Analog in" : "Analog out";
        divisor = 100; precision = 2; signed_value = true; break;
      case LPP_GENERIC_SENSOR: label = "Sensor"; break;
      case LPP_LUMINOSITY: label = "Light"; unit = " lx"; break;
      case LPP_PRESENCE: label = "Presence"; break;
      case LPP_TEMPERATURE: label = "Temp"; unit = " C"; divisor = 10; precision = 1; signed_value = true; break;
      case LPP_RELATIVE_HUMIDITY: label = "Humidity"; unit = "%"; divisor = 2; precision = 1; break;
      case LPP_BAROMETRIC_PRESSURE: label = "Pressure"; unit = " hPa"; divisor = 10; precision = 1; break;
      case LPP_VOLTAGE: label = "Voltage"; unit = " V"; divisor = 100; precision = 2; break;
      case LPP_CURRENT: label = "Current"; unit = " A"; divisor = 1000; precision = 3; signed_value = true; break;
      case LPP_FREQUENCY: label = "Frequency"; unit = " Hz"; break;
      case LPP_PERCENTAGE: label = "Percent"; unit = "%"; break;
      case LPP_ALTITUDE: label = "Altitude"; unit = " m"; signed_value = true; break;
      case LPP_CONCENTRATION: label = "Conc."; unit = " ppm"; break;
      case LPP_POWER: label = "Power"; unit = " W"; break;
      case LPP_DISTANCE: label = "Distance"; unit = " m"; divisor = 1000; precision = 3; break;
      case LPP_ENERGY: label = "Energy"; unit = " kWh"; divisor = 1000; precision = 3; break;
      case LPP_DIRECTION: label = "Direction"; unit = " deg"; break;
      case LPP_UNIXTIME: label = "Time"; break;
      case LPP_SWITCH: label = "Switch"; break;
      case LPP_COLOUR: {
        static const char* LABELS[] = { "Red", "Green", "Blue" };
        label = LABELS[row]; n = 1; p += row; break;
      }
      case LPP_GPS: {
        static const char* LABELS[] = { "Latitude", "Longitude", "Altitude" };
        label = LABELS[row]; n = 3; p += row * n; signed_value = true;
        divisor = row == 2 ? 100 : 10000; precision = row == 2 ? 2 : 4;
        unit = row == 2 ? " m" : ""; break;
      }
      case LPP_ACCELEROMETER: case LPP_GYROMETER: {
        static const char* ACC[] = { "Accel X", "Accel Y", "Accel Z" };
        static const char* GYR[] = { "Gyro X", "Gyro Y", "Gyro Z" };
        bool accel = type == LPP_ACCELEROMETER;
        label = accel ? ACC[row] : GYR[row]; n = 2; p += row * n;
        divisor = accel ? 1000 : 100; precision = accel ? 3 : 2;
        unit = accel ? " g" : " d/s"; signed_value = true; break;
      }
    }
    uint32_t raw = number(p, n);
    double value = raw;
    if (signed_value && (raw & (1UL << (n * 8 - 1))))
      value -= (double)(1ULL << (n * 8));
    snprintf(out, capacity, "%s: %.*f%s", label, precision, value / divisor, unit);
    return true;
  }
};

} // namespace zen
