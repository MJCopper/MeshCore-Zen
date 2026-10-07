#pragma once

#include <stdint.h>
#include "PetEvolution.h"

namespace zen { namespace pet {

// Gameplay has no platform, transport or storage dependency. Persistence
// captures explicit durations/credits, never raw uptime timestamps.
struct State {
  uint8_t form = 0, energy = 100, fullness = 70, bond = 0, food = 3;
  uint16_t xp = 0;
};

class Engine {
  State _state;
  bool _born = false, _enabled = false, _sleep = false, _paused = false;
  uint32_t _last = 0, _cooldown = 0;
  uint32_t _energy_credit = 0, _food_credit = 0, _hunger_credit = 0;
  uint8_t _hunger_rate = 5;
  static uint8_t add(uint8_t value, uint32_t amount, uint8_t limit) {
    return amount >= (uint32_t)(limit - value) ? limit : value + amount;
  }
public:
  struct Checkpoint {
    State state;
    uint32_t cooldown=0,energy_credit=0,food_credit=0,hunger_credit=0;
  };
  Checkpoint checkpoint() const {
    Checkpoint c; c.state=_state; c.cooldown=_cooldown;
    c.energy_credit=_energy_credit; c.food_credit=_food_credit;
    c.hunger_credit=_hunger_credit; return c;
  }
  void restore(const Checkpoint& c,uint32_t now) {
    *this=Engine(); _state=c.state; _born=true; _last=now;
    _cooldown=c.cooldown; _energy_credit=c.energy_credit;
    _food_credit=c.food_credit; _hunger_credit=c.hunger_credit;
  }
  enum Result : uint8_t { OK, SLEEPING, SUSPENDED, FULL, NO_FOOD, TIRED, COOLDOWN, NOT_READY };
  const State& state() const { return _state; }
  uint8_t level() const { return Evolution::level(_state.form); }
  bool sleeping() const { return _sleep; }
  bool paused() const { return _paused; }
  uint8_t hungerRate() const { return _hunger_rate; }
  uint32_t trainingRestMillis() const { return _cooldown; }
  bool ready() const {
    return level() < Evolution::LEVELS && _state.xp >= Evolution::xp(level()) &&
        _state.bond >= Evolution::bond(level());
  }
  uint8_t choices() const { return Evolution::choices(level()); }
  uint8_t child(uint8_t choice) const { return Evolution::child(_state.form, choice); }
  void update(uint32_t now, bool enabled, bool sleep, bool paused, uint8_t hunger_rate = 5) {
    if (!_born) {
      _last = now;
      if (enabled) { _state = State(); _born = true; }
    }
    uint32_t elapsed = now - _last;
    _last = now;
    if (_born && _enabled && !_paused) {
      // Lazy elapsed-time accounting, including millis() wrap. No timers,
      // offline progression or RTC synchronization are needed.
      _cooldown = elapsed >= _cooldown ? 0 : _cooldown - elapsed;
      uint64_t credit = (uint64_t)_energy_credit + (uint64_t)elapsed * (_sleep ? 20 : 10);
      _state.energy = add(_state.energy, credit / 3600000UL, 100);
      _energy_credit = _state.energy == 100 ? 0 : credit % 3600000UL;
      credit = (uint64_t)_food_credit + elapsed;
      _state.food = add(_state.food, credit / 14400000UL, 5);
      _food_credit = _state.food == 5 ? 0 : credit % 14400000UL;
      if (!_sleep) {
        credit = (uint64_t)_hunger_credit + (uint64_t)elapsed * _hunger_rate;
        uint32_t loss = credit / 3600000UL;
        _state.fullness = loss >= _state.fullness ? 0 : _state.fullness - loss;
        _hunger_credit = _state.fullness == 0 ? 0 : credit % 3600000UL;
      }
    }
    _enabled = enabled; _sleep = sleep; _paused = paused;
    // Account elapsed time at the previous rate before adopting a new sample.
    _hunger_rate = hunger_rate < 5 ? 5 : hunger_rate > 8 ? 8 : hunger_rate;
  }
  Result available() const {
    return !_enabled || _paused ? SUSPENDED : _sleep ? SLEEPING : OK;
  }
  void bonus(uint16_t xp, uint8_t bond) {
    if (available() != OK) return;
    const uint16_t limit = Evolution::xp(Evolution::LEVELS);
    _state.xp = xp >= limit - _state.xp ? limit : _state.xp + xp;
    _state.bond = add(_state.bond,bond,100);
  }
  Result feed() {
    if (available() != OK) return available();
    if (_state.fullness > 75) return FULL;
    if (!_state.food) return NO_FOOD;
    --_state.food;
    _state.fullness = add(_state.fullness, 25, 100);
    _state.bond = add(_state.bond, 5, 100);
    return OK;
  }
  Result trainingAvailable() const {
    if (available() != OK) return available();
    if (_cooldown) return COOLDOWN;
    if (_state.energy < 20 || _state.fullness < 10) return TIRED;
    return OK;
  }
  Result train() {
    if (trainingAvailable() != OK) return trainingAvailable();
    _state.energy -= 20; _state.fullness -= 10;
    const uint16_t limit = Evolution::xp(Evolution::LEVELS);
    // Training improves with maturity, but increasing thresholds still make
    // later levels take more successful sessions than the early levels.
    const uint16_t reward = 20 + 10 * (level() - 1);
    _state.xp = _state.xp >= limit - reward ? limit : _state.xp + reward;
    _state.bond = add(_state.bond, 2, 100);
    _cooldown = 300000UL;
    return OK;
  }
  Result evolve(uint8_t choice) {
    if (available() != OK) return available();
    if (!ready() || choice >= choices()) return NOT_READY;
    _state.form = child(choice);
    return OK;
  }
};

} } // namespace zen::pet
