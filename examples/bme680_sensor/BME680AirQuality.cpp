#include "BME680AirQuality.h"

#include <Arduino.h>

bool BME680AirQuality::update(float gas_ohms, float humidity_percent, float& score) {
  score = 0.0f;
  if (!isfinite(gas_ohms) || !isfinite(humidity_percent) || gas_ohms <= 0.0f ||
      humidity_percent < 0.0f || humidity_percent > 100.0f) return false;

  if (_gas_baseline <= 0.0f || gas_ohms > _gas_baseline) {
    // Gas resistance normally rises during heater warm-up. Track that quickly,
    // then adapt upward gradually as a cleaner-air baseline is observed.
    _gas_baseline = _samples < WARMUP_SAMPLES
        ? gas_ohms
        : _gas_baseline * 0.95f + gas_ohms * 0.05f;
  } else if (_samples >= WARMUP_SAMPLES) {
    // Follow long-term sensor ageing very slowly without treating a short VOC
    // event as the new clean-air baseline.
    _gas_baseline = _gas_baseline * 0.9995f + gas_ohms * 0.0005f;
  }

  if (_samples < WARMUP_SAMPLES) _samples++;
  if (_samples < WARMUP_SAMPLES) return false;

  float gas_ratio = constrain(gas_ohms / _gas_baseline, 0.0f, 1.0f);
  float gas_score = gas_ratio * 75.0f;

  float humidity_score;
  if (humidity_percent >= 38.0f && humidity_percent <= 42.0f) {
    humidity_score = 25.0f;
  } else if (humidity_percent < 38.0f) {
    humidity_score = constrain(humidity_percent / 40.0f, 0.0f, 1.0f) * 25.0f;
  } else {
    humidity_score = constrain((100.0f - humidity_percent) / 60.0f, 0.0f, 1.0f) * 25.0f;
  }

  score = constrain((100.0f - gas_score - humidity_score) * 5.0f, 0.0f, 500.0f);
  return true;
}
