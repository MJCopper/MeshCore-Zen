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

  void clearSignal() {
    _sample_count = _sample_head = 0;
    _last_sample_ms = 0;
    _have_sample = false;
  }

public:
  void reset() { *this = RepeaterSignalMonitor(); }

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

  // Strength is link margin above the approximate LoRa demodulation floor:
  // <=5 dB is low, <=10 dB medium, and greater than 10 dB high.
  uint8_t bars(uint8_t sf, uint32_t now, bool radio_available) const {
    if (!radio_available || !fresh(now) || sf < 7 || sf > 12) return 0;
    int floor_x4 = (-30 - 10 * (sf - 7)); // SF7 -7.5 dB through SF12 -20 dB
    int margin_x4 = averageSnrX4() - floor_x4;
    if (margin_x4 <= 20) return 1;
    if (margin_x4 <= 40) return 2;
    return 3;
  }
};

} // namespace zen
