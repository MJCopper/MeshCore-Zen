#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// RAM-only allowance clock. Clock corrections re-anchor rather than renew.
class PetRewardDay {
  bool _started = false, _synced = false, _guard = false;
  uint32_t _last_ms = 0;
  uint64_t _elapsed = 0;
  int64_t _local = 0, _date = 0;
  static int64_t date(int64_t seconds) {
    return seconds >= 0 ? seconds / 86400 : (seconds - 86399) / 86400;
  }
public:
  struct Checkpoint { int64_t date=0; uint64_t elapsed=0; bool synced=false; };
  Checkpoint checkpoint() const {
    Checkpoint c; c.date=_date; c.elapsed=_elapsed; c.synced=_synced; return c;
  }
  void restore(const Checkpoint& c,uint32_t now) {
    *this=PetRewardDay(); _started=true; _last_ms=now;
    _date=c.date; _local=c.date*86400; _synced=c.synced; _guard=true;
    // A restored allowance must establish a fresh safe day, not replay an
    // allowance from a potentially day-old checkpoint after every reboot.
  }
  bool synchronized() const { return _synced; }
  bool update(uint32_t now, bool synced, int64_t local) {
    if (!_started) {
      _started = true; _last_ms = now; _synced = synced;
      _local = local; _date = date(local); return false;
    }
    uint32_t delta = now - _last_ms;
    _last_ms = now; _elapsed += delta;
    bool reset = false;
    if (synced) {
      int64_t day = date(local);
      int64_t drift = (local - _local) * 1000 - delta;
      if (!_synced || drift < -2000 || drift > 120000) {
        _guard = true;
      } else if (day > _date && (!_guard || _elapsed >= 72000000ULL)) {
        reset = true;
      }
      _local = local; _date = day;
    } else if (_elapsed >= 86400000ULL) {
      reset = true;
    }
    _synced = synced;
    if (reset) { _elapsed = 0; _guard = false; }
    return reset;
  }
};

} } // namespace zen::pet
