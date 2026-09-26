#pragma once

#include <stdint.h>
#include "BatteryPolicy.h"

namespace zen {

// Estimates time to BatteryPolicy::EMPTY_MV from the device's own recent
// discharge rate. Samples live only in RAM and are fed by UITask's existing
// battery poll, so this adds no ADC reads, wakeups or flash writes.
class BatteryRuntimeEstimator {
public:
  enum State : uint8_t { UNKNOWN, MODEL, ESTIMATE, CHARGING, PAUSED, EMPTY };

private:
  struct Sample {
    uint32_t at;
    uint16_t percent_x100;
    uint16_t mv;
  };

  static constexpr uint32_t DEFAULT_FULL_SECONDS = 5UL * 24UL * 60UL * 60UL;
  static constexpr uint32_t MIN_FULL_SECONDS = 4UL * 24UL * 60UL * 60UL;
  static constexpr uint32_t MAX_FULL_SECONDS = 7UL * 24UL * 60UL * 60UL;
  static constexpr uint32_t SAMPLE_INTERVAL_MS = 60UL * 60UL * 1000UL;
  static constexpr uint32_t SETTLING_MS = 2UL * 60UL * 60UL * 1000UL;
  static constexpr uint32_t MIN_SPAN_MS = 6UL * 60UL * 60UL * 1000UL;
  static constexpr uint32_t FULL_CONFIDENCE_MS = 24UL * 60UL * 60UL * 1000UL;
  static constexpr uint16_t MIN_DROP_X100 = 500;       // 5.00%
  static constexpr uint16_t FULL_CONFIDENCE_DROP = 1500; // 15.00%
  static constexpr uint16_t RESET_RISE_MV = 80;
  static constexpr uint32_t MAX_ESTIMATE_SECONDS = 30UL * 24UL * 60UL * 60UL;
  static constexpr uint8_t MAX_SAMPLES = 25;

  Sample _samples[MAX_SAMPLES];
  uint8_t _count = 0;
  State _state = UNKNOWN;
  uint32_t _seconds = 0;
  uint16_t _confidence = 0;
  bool _was_external = false;
  bool _was_emergency = false;
  bool _settling = false;
  uint32_t _settling_until = 0;

  void clearSamples() { _count = 0; _confidence = 0; }

  void useModel(uint16_t current_pct) {
    _state = MODEL;
    _confidence = 0;
    _seconds = (uint32_t)((uint64_t)DEFAULT_FULL_SECONDS * current_pct / 10000UL);
  }

  // Least-squares trend across all hourly points is less sensitive to one
  // noisy endpoint than extrapolating the first and last readings directly.
  void calculate(uint16_t current_pct) {
    useModel(current_pct);
    if (_count < 2) return;
    const Sample& first = _samples[0];
    const Sample& last = _samples[_count - 1];
    uint32_t elapsed = (uint32_t)(last.at - first.at);
    if (elapsed < MIN_SPAN_MS) return;

    int64_t sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
    for (uint8_t i = 0; i < _count; i++) {
      int32_t x = (uint32_t)(_samples[i].at - first.at) / 60000UL;
      int32_t y = _samples[i].percent_x100;
      sum_x += x; sum_y += y; sum_xx += (int64_t)x * x; sum_xy += (int64_t)x * y;
    }
    int64_t denominator = (int64_t)_count * sum_xx - sum_x * sum_x;
    int64_t numerator = (int64_t)_count * sum_xy - sum_x * sum_y;
    if (denominator <= 0 || numerator >= 0) return;
    uint32_t span_minutes = elapsed / 60000UL;
    uint32_t drop = (uint32_t)(((-numerator) * span_minutes + denominator / 2) /
                               denominator);
    if (drop < MIN_DROP_X100) return;

    uint32_t time_conf = elapsed >= FULL_CONFIDENCE_MS ? 1000
        : (uint32_t)((uint64_t)elapsed * 1000 / FULL_CONFIDENCE_MS);
    uint32_t drop_conf = drop >= FULL_CONFIDENCE_DROP ? 1000
        : drop * 1000UL / FULL_CONFIDENCE_DROP;
    _confidence = (uint16_t)(time_conf < drop_conf ? time_conf : drop_conf);

    uint32_t observed_full = (uint32_t)((uint64_t)(elapsed / 1000UL) * 10000UL / drop);
    if (_confidence < 1000) {
      if (observed_full < MIN_FULL_SECONDS) observed_full = MIN_FULL_SECONDS;
      if (observed_full > MAX_FULL_SECONDS) observed_full = MAX_FULL_SECONDS;
    }
    if (observed_full > MAX_ESTIMATE_SECONDS) observed_full = MAX_ESTIMATE_SECONDS;
    uint32_t observed_remaining = (uint32_t)((uint64_t)observed_full * current_pct / 10000UL);
    uint32_t model_remaining = _seconds;
    _seconds = (uint32_t)(((uint64_t)model_remaining * (1000 - _confidence) +
                           (uint64_t)observed_remaining * _confidence) / 1000UL);
    _state = ESTIMATE;
  }

public:
  void reset() {
    clearSamples(); _state = UNKNOWN; _seconds = 0;
    _was_external = _was_emergency = _settling = false;
  }

  void update(uint32_t now, uint16_t mv, bool external_power, bool emergency) {
    if (external_power) {
      clearSamples(); _state = CHARGING; _seconds = 0;
      _was_external = true; _settling = false;
      return;
    }
    if (emergency) {
      clearSamples(); _state = PAUSED; _seconds = 0;
      _was_emergency = true; _settling = false;
      return;
    }
    if (!mv) { clearSamples(); _state = UNKNOWN; _seconds = 0; return; }
    uint16_t pct = BatteryPolicy::percentX100(mv);
    if (pct == 0) { clearSamples(); _state = EMPTY; _seconds = 0; return; }

    if (_was_external) {
      clearSamples();
      _was_external = false;
      _settling = true;
      _settling_until = now + SETTLING_MS;
    }
    if (_was_emergency) { clearSamples(); _was_emergency = false; }
    if (_settling && (int32_t)(now - _settling_until) < 0) {
      useModel(pct);
      return;
    }
    _settling = false;

    if (_count && mv > (uint16_t)(_samples[_count - 1].mv + RESET_RISE_MV)) clearSamples();
    if (!_count || (uint32_t)(now - _samples[_count - 1].at) >= SAMPLE_INTERVAL_MS) {
      if (_count == MAX_SAMPLES) {
        for (uint8_t i = 1; i < _count; i++) _samples[i - 1] = _samples[i];
        _count--;
      }
      _samples[_count++] = { now, pct, mv };
    }
    calculate(pct);
  }

  State state() const { return _state; }
  uint32_t seconds() const { return (_state == MODEL || _state == ESTIMATE) ? _seconds : 0; }
  uint16_t confidencePermille() const { return _confidence; }
  uint8_t sampleCount() const { return _count; }
};

} // namespace zen
