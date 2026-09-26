#pragma once

#include <stdint.h>

// RAM-only state machine for Adaptive GPS power control. It deliberately owns
// policy only; the sensor manager remains responsible for switching hardware.
class GpsAdaptivePolicy {
public:
  enum Action : uint8_t { KEEP, START, STOP };
  enum Phase : uint8_t {
    DISABLED, INITIAL_SEARCH, TRACKING,
    SHORT_STANDBY, SHORT_SEARCH, LONG_STANDBY, LONG_SEARCH
  };

  static constexpr uint32_t INITIAL_SEARCH_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t FIX_LOSS_GRACE_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t SEARCH_MS = 90UL * 1000UL;
  static constexpr uint32_t SHORT_PHASE_MS = 15UL * 60UL * 1000UL;
  static constexpr uint32_t SHORT_STANDBY_MS = 2UL * 60UL * 1000UL;
  static constexpr uint32_t LONG_STANDBY_MS = 5UL * 60UL * 1000UL;

  void setEnabled(uint32_t now, bool enabled, bool receiver_active) {
    if (!enabled) {
      _phase = DISABLED;
      _deadline = _poor_since = _backoff_started = 0;
    } else if (_phase == DISABLED) {
      beginInitial(now);
      (void)receiver_active;
    }
  }

  Action update(uint32_t now, bool quality_good, bool receiver_active,
                bool force_active = false) {
    if (_phase == DISABLED) return KEEP;
    if (force_active) {
      if (quality_good) enterTracking();
      return receiver_active ? KEEP : START;
    }

    switch (_phase) {
      case INITIAL_SEARCH:
        if (quality_good) enterTracking();
        else if (due(now, _deadline)) return enterBackoff(now);
        return receiver_active ? KEEP : START;
      case TRACKING:
        if (quality_good) _poor_since = 0;
        else if (_poor_since == 0) _poor_since = now ? now : 1;
        else if ((uint32_t)(now - _poor_since) >= FIX_LOSS_GRACE_MS)
          return enterBackoff(now);
        return receiver_active ? KEEP : START;
      case SHORT_STANDBY:
      case LONG_STANDBY:
        if (!due(now, _deadline)) return receiver_active ? STOP : KEEP;
        _phase = _phase == SHORT_STANDBY ? SHORT_SEARCH : LONG_SEARCH;
        _deadline = now + SEARCH_MS;
        return receiver_active ? KEEP : START;
      case SHORT_SEARCH:
      case LONG_SEARCH:
        if (quality_good) {
          enterTracking();
          return KEEP;
        }
        if (!due(now, _deadline)) return receiver_active ? KEEP : START;
        if ((uint32_t)(now - _backoff_started) >= SHORT_PHASE_MS) {
          _phase = LONG_STANDBY;
          _deadline = now + LONG_STANDBY_MS;
        } else {
          _phase = SHORT_STANDBY;
          _deadline = now + SHORT_STANDBY_MS;
        }
        return receiver_active ? STOP : KEEP;
      default:
        return KEEP;
    }
  }

  Action onUserWake(uint32_t now, bool receiver_active) {
    if (!inBackoff()) return KEEP;
    beginInitial(now);
    return receiver_active ? KEEP : START;
  }

  bool retryRemaining(uint32_t now, uint32_t& remaining_ms) const {
    if (_phase != SHORT_STANDBY && _phase != LONG_STANDBY) return false;
    remaining_ms = due(now, _deadline) ? 0 : (uint32_t)(_deadline - now);
    return true;
  }

  Phase phase() const { return _phase; }

private:
  Phase _phase = DISABLED;
  uint32_t _deadline = 0;
  uint32_t _poor_since = 0;
  uint32_t _backoff_started = 0;

  static bool due(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
  }

  bool inBackoff() const {
    return _phase == SHORT_STANDBY || _phase == SHORT_SEARCH ||
           _phase == LONG_STANDBY || _phase == LONG_SEARCH;
  }

  void beginInitial(uint32_t now) {
    _phase = INITIAL_SEARCH;
    _deadline = now + INITIAL_SEARCH_MS;
    _poor_since = _backoff_started = 0;
  }

  void enterTracking() {
    _phase = TRACKING;
    _deadline = _poor_since = _backoff_started = 0;
  }

  Action enterBackoff(uint32_t now) {
    _phase = SHORT_STANDBY;
    _deadline = now + SHORT_STANDBY_MS;
    _poor_since = 0;
    _backoff_started = now;
    return STOP;
  }
};
