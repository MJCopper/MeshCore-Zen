#pragma once

#include <stdint.h>

namespace zen {

// RAM-only view of recent repeater link quality. Passive samples avoid extra
// airtime; a user-wake scan is requested only when both the sample and previous
// attempt are stale. Unsigned subtraction keeps all deadlines rollover-safe.
class RepeaterSignalMonitor {
  static constexpr uint32_t REFRESH_MS = 30UL * 60UL * 1000UL;
  static constexpr uint32_t EXPIRE_MS = 2UL * 60UL * 60UL * 1000UL;
  static constexpr uint8_t SAMPLE_COUNT = 4;
  int8_t _samples[SAMPLE_COUNT]{};
  uint8_t _sample_count = 0;
  uint8_t _sample_head = 0;
  uint32_t _last_sample_ms = 0;
  uint32_t _last_scan_ms = 0;
  bool _have_sample = false;
  bool _have_scan = false;
  bool _discovering = false;
  bool _discover_seen = false;
  int8_t _discover_best_x4 = INT8_MIN;

  float _freq = 0, _bw = 0;
  uint8_t _sf = 0, _cr = 0;

  void clearSignal() {
    _sample_count = _sample_head = 0;
    _last_sample_ms = 0;
    _have_sample = false;
  }

public:
  void reset() { *this = RepeaterSignalMonitor(); }

  // Called by the single radio-configuration application path. Unchanged
  // settings (including power-state restoration) retain their history.
  bool configure(float freq, float bw, uint8_t sf, uint8_t cr) {
    if (_freq == freq && _bw == bw && _sf == sf && _cr == cr) return false;
    reset();
    _freq = freq; _bw = bw; _sf = sf; _cr = cr;
    return true;
  }

  static bool qualifiesRoute(bool flood, bool direct, uint8_t path_count) {
    // Flood paths start empty and acquire one hash per relay. Direct paths
    // include the destination itself, so they need a second hop to prove that
    // a repeater—not a zero-hop companion—physically sent the packet to us.
    return (flood && path_count > 0) || (direct && path_count > 1);
  }

  void noteSample(int snr_x4, uint32_t now) {
    if (snr_x4 < INT8_MIN) snr_x4 = INT8_MIN;
    if (snr_x4 > INT8_MAX) snr_x4 = INT8_MAX;
    _samples[_sample_head] = (int8_t)snr_x4;
    _sample_head = (_sample_head + 1) % SAMPLE_COUNT;
    if (_sample_count < SAMPLE_COUNT) _sample_count++;
    _last_sample_ms = now;
    _have_sample = true;
  }

  bool shouldScanOnUserWake(uint32_t now, bool radio_available) const {
    if (!radio_available) return false;
    bool signal_stale = !_have_sample || now - _last_sample_ms >= REFRESH_MS;
    bool attempt_due = !_have_scan || now - _last_scan_ms >= REFRESH_MS;
    return signal_stale && attempt_due;
  }

  void noteScanAttempt(uint32_t now) {
    _last_scan_ms = now;
    _have_scan = true;
  }

  void beginDiscovery() {
    _discovering = true;
    _discover_seen = false;
    _discover_best_x4 = INT8_MIN;
  }

  void cancelDiscovery() { _discovering = false; }

  void noteDiscovery(int snr_x4) {
    if (!_discovering) return;
    if (snr_x4 < INT8_MIN) snr_x4 = INT8_MIN;
    if (snr_x4 > INT8_MAX) snr_x4 = INT8_MAX;
    if (!_discover_seen || snr_x4 > _discover_best_x4)
      _discover_best_x4 = (int8_t)snr_x4;
    _discover_seen = true;
  }

  // A completed discovery is authoritative: use its strongest response alone,
  // or clear the indicator when no repeater answered during the scan window.
  void finishDiscovery(uint32_t now) {
    if (!_discovering) return;
    bool seen = _discover_seen;
    int best = _discover_best_x4;
    _discovering = false;
    clearSignal();
    if (seen) noteSample(best, now);
  }

  bool fresh(uint32_t now) const {
    return _have_sample && now - _last_sample_ms < EXPIRE_MS;
  }

  int averageSnrX4() const {
    if (!_sample_count) return INT16_MIN;
    int total = 0;
    for (uint8_t i = 0; i < _sample_count; i++) total += _samples[i];
    return total / _sample_count;
  }

  // Approximate SF-adjusted reception quality, not delivery probability.
  // Semtech SX1261/2 datasheet Table 6-1: SF5 -2.5 dB to SF12 -20 dB.
  // Calibrated at SF7: >5 dB = 2, >7 dB = 3, >=10 dB = 4 bars.
  // Compare sums to preserve fractional-quarter-dB averages at boundaries.
  uint8_t bars(uint8_t sf, uint32_t now, bool radio_available) const {
    if (!radio_available || !fresh(now) || sf < 5 || sf > 12) return 0;
    const int floor_x4 = -10 - 10 * (sf - 5);
    int total = 0;
    for (uint8_t i = 0; i < _sample_count; i++) total += _samples[i];
    const int margin_total = total - floor_x4 * _sample_count;
    if (margin_total >= 70 * _sample_count) return 4;
    if (margin_total > 58 * _sample_count) return 3;
    if (margin_total > 50 * _sample_count) return 2;
    return 1;
  }
};

} // namespace zen
