#include "Bme680Telemetry.h"
#include <helpers/SensorManager.h>

bool Bme680Telemetry::ingest(CayenneLPP& raw) {
  float t = NAN, h = NAN, p = NAN, a = NAN, gas = NAN;
  uint8_t channel = 0;
  const uint8_t* bytes = raw.getBuffer();
  size_t pos = 0;
  while (pos + 2 <= raw.getSize()) {
    uint8_t ch = bytes[pos++], type = bytes[pos++];
    uint8_t length;
    uint32_t multiplier = 1;
    bool is_signed = false;
    switch (type) {
      case LPP_TEMPERATURE: length = 2; multiplier = 10; is_signed = true; break;
      case LPP_RELATIVE_HUMIDITY: length = 1; multiplier = 2; break;
      case LPP_BAROMETRIC_PRESSURE: length = 2; multiplier = 10; break;
      case LPP_ALTITUDE: length = 2; is_signed = true; break;
      case LPP_GENERIC_SENSOR: length = 4; break;
      default: return false;
    }
    if (!length || length > 4 || pos + length > raw.getSize()) return false;
    uint32_t value = 0;
    for (uint8_t i = 0; i < length; i++) value = (value << 8) | bytes[pos + i];
    bool negative = is_signed && (bytes[pos] & 0x80);
    if (negative && length < 4) value |= ~((1UL << (length * 8)) - 1);
    float number = (negative ? (float)(int32_t)value : (float)value) /
        multiplier;
    if (type == LPP_TEMPERATURE && !channel) channel = ch;
    if (ch == channel) {
      switch (type) {
        case LPP_TEMPERATURE: t = number; break;
        case LPP_RELATIVE_HUMIDITY: h = number; break;
        case LPP_BAROMETRIC_PRESSURE: p = number; break;
        case LPP_ALTITUDE: a = number; break;
        case LPP_GENERIC_SENSOR: gas = number; break;
      }
    }
    pos += length;
  }
  if (pos != raw.getSize() || !channel || !isfinite(t) || !isfinite(h) || !isfinite(p) || !isfinite(a) ||
      !isfinite(gas) || p <= 0 || gas <= 0 || h < 0 || h > 100) return false;
  temperature = t;
  humidity = h;
  pressure = p;
  altitude = a + _altitude_offset;
  _channel = channel;
  _ready = true;
  _air_ready = _quality.update(gas, h, air_quality);
  return true;
}

void Bme680Telemetry::appendEnvironment(CayenneLPP& output, uint8_t permissions) const {
  if (!_ready || !(permissions & TELEM_PERM_ENVIRONMENT)) return;
  output.addTemperature(_channel, temperature);
  output.addRelativeHumidity(_channel, humidity);
  output.addBarometricPressure(_channel, pressure);
  output.addAltitude(_channel, altitude);
  if (_air_ready) output.addGenericSensor(_channel, air_quality);
}

void Bme680Telemetry::encode(CayenneLPP& output, uint8_t permissions) const {
  output.reset();
  if (isfinite(voltage)) output.addVoltage(TELEM_CHANNEL_SELF, voltage);
  appendEnvironment(output, permissions);
}
