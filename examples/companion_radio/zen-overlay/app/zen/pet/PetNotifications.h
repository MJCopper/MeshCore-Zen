#pragma once

#include "PetEngine.h"
#include "PetAssets.h"
#include <stdio.h>

namespace zen { namespace pet {

// RAM-only event collector. Zen owns presentation, muting, wake and volume.
class PetNotifications {
public:
  enum Sound : uint8_t { HUNGRY, VERY_HUNGRY, REWARD, READY, EVOLVED, TRAIN_WON, TRAIN_LOST, COUNT };
  struct Alert { Sound sound = REWARD; char text[80] = {}; bool action = false; };
  static const char* melody(Sound sound) {
    static const char* const MELODIES[] = {
      "PetHungry:d=16,o=5,b=140:e,c",
      "PetEmpty:d=16,o=5,b=130:g,e,c",
      "PetReward:d=32,o=6,b=180:c,e,g",
      "PetReady:d=16,o=5,b=180:c,e,g,c6",
      "PetEvolve:d=16,o=6,b=160:g,c7,8e7,g7,8c7",
      "PetWon:d=16,o=5,b=180:c,g,8c6",
      "PetLost:d=8,o=5,b=120:e,d,c"
    };
    return sound < COUNT ? MELODIES[sound] : nullptr;
  }
private:
  static constexpr uint8_t EVOLUTION_QUEUE = Evolution::LEVELS - 1;
  uint16_t _ready_claimed = 0, _xp = 0;
  uint8_t _bond = 0, _hunger = 0, _ready_level = 0;
  uint8_t _evolutions[EVOLUTION_QUEUE] = {}, _evolution_head = 0, _evolution_count = 0;
  bool _hungry_claimed = false, _very_claimed = false;
  bool _enabled = false, _sleep = false, _paused = false, _cooldown = false;
  uint32_t _next = 0;
  uint8_t _training[2] = {}, _training_count = 0;
public:
  void training(bool won) {
    if (_enabled && !_sleep && !_paused && _training_count < 2)
      _training[_training_count++] = won ? TRAIN_WON : TRAIN_LOST;
  }
  void reward(uint16_t xp, uint8_t bond) {
    if (!_enabled || _paused) return;
    uint32_t total = _xp + xp;
    _xp = total > Evolution::xp(Evolution::LEVELS) ? Evolution::xp(Evolution::LEVELS) : total;
    unsigned int combined = _bond + bond;
    _bond = combined > 100 ? 100 : combined;
  }
  void evolved(uint8_t id) {
    if (_enabled && !_paused && !_sleep && _evolution_count < EVOLUTION_QUEUE)
      _evolutions[(_evolution_head+_evolution_count++)%EVOLUTION_QUEUE] = id;
    _ready_level = 0;
  }
  void observe(const State& state, uint8_t level, bool ready,
               bool enabled, bool sleep, bool paused) {
    _enabled = enabled; _sleep = sleep; _paused = paused;
    if (!enabled) {
      _xp = _bond = _hunger = _ready_level = 0;
      _evolution_head = _evolution_count = 0;
      _training_count = 0;
      _cooldown = false;
      return;
    }
    if (state.fullness > 40) { _hungry_claimed = _very_claimed = false; }
    // Recheck held alerts after feeding, evolution or a deferred presentation.
    if (state.fullness > 25) _hunger = 0;
    else if (_hunger == 2 && state.fullness > 10) _hunger = 0;
    if (_ready_level && (!ready || level != _ready_level)) _ready_level = 0;
    if (paused) return;
    if (state.fullness <= 10 && !_very_claimed) {
      _very_claimed = _hungry_claimed = true; _hunger = 2;
    } else if (state.fullness <= 25 && !_hungry_claimed) {
      _hungry_claimed = true; _hunger = 1;
    }
    uint16_t bit = 1u << (level - 1);
    if (ready && !(_ready_claimed & bit)) {
      _ready_claimed |= bit; _ready_level = level;
    }
  }
  bool take(uint32_t now, Alert& alert, bool training_only = false) {
    if (!_enabled || _sleep || _paused ||
        (_cooldown && (int32_t)(now - _next) < 0)) return false;
    alert.action = false;
    if (_training_count) {
      alert.sound = (Sound)_training[0];
      snprintf(alert.text,sizeof(alert.text),"%s",alert.sound == TRAIN_WON ? "Won!" : "Lost!");
      _training[0] = _training[1]; --_training_count;
    } else if (training_only) return false;
    else if (_evolution_count) {
      alert.sound = EVOLVED;
      snprintf(alert.text,sizeof(alert.text),"Pet: Evolved to %s",form(_evolutions[_evolution_head]).name);
      _evolution_head = (_evolution_head+1)%EVOLUTION_QUEUE; --_evolution_count;
    } else if (_ready_level || _xp || _bond) {
      const bool ready = _ready_level != 0;
      alert.sound = ready ? READY : REWARD;
      if (_xp && _bond)
        snprintf(alert.text,sizeof(alert.text),"Pet: +%u XP, +%u Bond%s",_xp,_bond,
                 ready ? "; Evolve ready" : "");
      else if (_xp)
        snprintf(alert.text,sizeof(alert.text),"Pet: +%u XP%s",_xp,ready ? "; Evolve ready" : "");
      else if (_bond)
        snprintf(alert.text,sizeof(alert.text),"Pet: +%u Bond%s",_bond,ready ? "; Evolve ready" : "");
      else snprintf(alert.text,sizeof(alert.text),"Pet: Ready to evolve");
      _xp = _bond = _ready_level = 0;
    } else if (_hunger) {
      alert.sound = _hunger == 2 ? VERY_HUNGRY : HUNGRY;
      snprintf(alert.text,sizeof(alert.text),"Pet: %s",_hunger == 2 ? "Very hungry" : "Hungry");
      _hunger = 0;
    } else return false;
    _next = now + 5000; _cooldown = true;
    return true;
  }
};

} } // namespace zen::pet
