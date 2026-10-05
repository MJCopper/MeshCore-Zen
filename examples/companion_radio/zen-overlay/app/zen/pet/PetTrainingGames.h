#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// Pure, RAM-only game state. The pet engine alone commits training costs/XP.
// All time and entropy are supplied by the UI; no hardware or mesh dependency.
class PetTrainingGames {
public:
  enum Game : uint8_t { ARROWS, FOOD, TIMING, BOXES, CHANGED_SHAPE, COUNT };
  enum Phase : uint8_t { OFF, INSTRUCTIONS, SHOW, COVERED, PLAY, FAILED, WON,
                        BOX_HIDE, BOX_HINT, BOX_MOVE, BOX_PAUSE, ANSWER };
  enum Action : uint8_t { NONE, UP, RIGHT, DOWN, LEFT, ENTER, BACK };
  static constexpr uint8_t FOOD_LANES = 5;
private:
  Game _game = ARROWS, _last = COUNT;
  Phase _phase = OFF;
  uint32_t _rng = 0x6d2b79f5UL, _at = 0, _started = 0;
  uint8_t _sequence[6] = {}, _symbols[3] = {};
  uint8_t _initial = 0, _target = 0, _replacement = 0;
  uint8_t _selection = 1, _progress = 0, _stage = 0, _drop = 0, _marker = 0;
  bool _retry = false, _reserved = false, _slow = false;
  uint32_t random() {
    _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
    return _rng;
  }
  void resetTimingMarker(uint32_t now) {
    // Start outside the new target, so repeated Enter cannot win.
    _marker = zoneStart() ? 0 : 8; _at = now;
  }
  void applyBoxSwap() {
    uint8_t a=_sequence[_stage*2], b=_sequence[_stage*2+1];
    if (_target == a) _target = b; else if (_target == b) _target = a;
    ++_stage; _phase = BOX_PAUSE;
  }
  void startRound(uint32_t now) {
    _selection = _game == FOOD ? FOOD_LANES/2 : 1;
    _progress = _stage = _drop = _marker = 0;
    _target = _initial; _at = _started = now;
    _phase = _game == FOOD || _game == TIMING ? PLAY : SHOW;
    if (_game == TIMING) resetTimingMarker(now);
  }
public:
  Game game() const { return _game; }
  Phase phase() const { return _phase; }
  bool active() const { return _phase != OFF; }
  bool retryUsed() const { return _retry; }
  bool slow() const { return _slow; }
  bool previewing() const {
    return _phase == SHOW || _phase == COVERED || _phase == BOX_HIDE ||
        _phase == BOX_HINT || _phase == BOX_MOVE || _phase == BOX_PAUSE;
  }
  uint8_t selection() const { return _selection; }
  uint8_t progress() const { return _progress; }
  uint8_t stage() const { return _stage; }
  uint8_t boxFrame() const { return _marker; }
  uint8_t target() const { return _game == FOOD ? _sequence[_drop] : _target; }
  uint8_t arrow(uint8_t i) const { return i < 3 ? _sequence[i] : 0; }
  uint8_t swap(uint8_t i) const { return i < 6 ? _sequence[i] : 0; }
  uint8_t symbol(uint8_t i) const {
    if (i >= 3) return 0;
    return _phase == PLAY && i == _initial ? _replacement : _symbols[i];
  }
  uint8_t marker() const { return _marker <= 8 ? _marker : 16 - _marker; }
  static uint8_t timingWidth(uint8_t turn) { return turn == 0 ? 5 : turn == 1 ? 3 : 2; }
  uint8_t timingStart(uint8_t turn) const { return _sequence[turn < 3 ? turn : 2]; }
  uint8_t zoneStart() const { return timingStart(_progress); }
  uint8_t zoneWidth() const { return timingWidth(_progress); }
  bool inZone() const { return marker() >= zoneStart() && marker() < zoneStart()+zoneWidth(); }
  static const char* name(Game game) {
    static const char* const NAMES[] = {
      "Follow the Arrows","Catch the Food","Perfect Timing","Find Your Pet","Which One Changed?"
    };
    return NAMES[game < COUNT ? game : ARROWS];
  }
  uint32_t interval() const {
    if (_phase == COVERED) return 3000;
    if (_game == TIMING) return _slow ? 700 : 160;
    if (_game == FOOD) return _slow ? 1200 : 600;
    if (_game == BOXES) {
      if (_phase == SHOW) return 3000;
      if (_phase == BOX_HIDE) return 1000;
      if (_phase == BOX_HINT) return _slow ? 2000 : 1000;
      if (_phase == BOX_MOVE) return 150;
      if (_phase == BOX_PAUSE) return _slow ? 1500 : 700;
      if (_phase == ANSWER) return _slow ? 2500 : 1500;
    }
    return _slow ? 5000 : 3000;
  }
  void begin(uint32_t now, uint32_t seed, bool slow) {
    if (active()) return;
    _slow = slow;
    if (!_reserved) {
      _rng ^= seed ? seed : 0x9e3779b9UL;
      if (!_rng) _rng = 1;
      _game = _last == COUNT ? (Game)(random()%COUNT) :
          (Game)((_last+1+random()%(COUNT-1))%COUNT);
      _last = _game; _reserved = true; _retry = false;
      for (uint8_t i=0;i<6;++i)
        _sequence[i] = random()%(_game == ARROWS ? 4 : _game == FOOD ? FOOD_LANES : 3);
      if (_game == BOXES) {
        for (uint8_t i=0;i<6;i+=2)
          if (_sequence[i] == _sequence[i+1]) _sequence[i+1] = (_sequence[i]+1)%3;
      }
      _initial = random()%3;
      // Three distinct geometric symbols; replace one with the missing fourth.
      uint8_t missing = random()%4;
      for (uint8_t i=0,n=0;i<4;++i) if (i != missing) _symbols[n++] = i;
      for (uint8_t i=0;i<3;++i) {
        uint8_t j=random()%3, temp=_symbols[i]; _symbols[i]=_symbols[j]; _symbols[j]=temp;
      }
      _replacement = missing;
      if (_game == TIMING) {
        // Store all placements up front; retries/cancellation retain them.
        for (uint8_t turn=0;turn<3;++turn) {
          uint8_t positions = 10-timingWidth(turn);
          uint8_t start = random() % (positions-(turn ? 1 : 0));
          if (turn && start >= _sequence[turn-1]) ++start;
          _sequence[turn] = start;
        }
      }
    }
    _phase = INSTRUCTIONS; _at = now;
  }
  void cancel() {
    if (_phase == FAILED && _retry) _reserved = false;
    _phase = OFF;
  }
  void finish() { _phase = OFF; _reserved = false; }
  void tick(uint32_t now) {
    if (!previewing() && _phase != PLAY && _phase != ANSWER) return;
    if (_phase != ANSWER && now - _started >= (_slow ? 60000UL : 45000UL)) { _phase = FAILED; return; }
    if (now - _at < interval()) return;
    // One visible step per update, rather than catching up unseen animation.
    _at = now;
    if (_phase == SHOW) {
      if (_game == CHANGED_SHAPE) { _phase = COVERED; return; }
      if (_game != BOXES) { _phase = PLAY; return; }
      _phase = BOX_HIDE;
    } else if (_phase == COVERED) {
      _phase = PLAY;
    } else if (_phase == BOX_HIDE) {
      _phase = BOX_HINT;
    } else if (_phase == BOX_HINT) {
      if (_slow) applyBoxSwap();
      else { _marker = 0; _phase = BOX_MOVE; }
    } else if (_phase == BOX_MOVE) {
      if (++_marker == 4) applyBoxSwap();
    } else if (_phase == BOX_PAUSE) {
      _phase = _stage == 3 ? PLAY : BOX_HINT;
    } else if (_phase == ANSWER) {
      _phase = _selection == _target ? WON : FAILED;
    } else if (_game == TIMING) {
      _marker = (_marker+1)%16;
    } else if (_game == FOOD) {
      if (++_stage < 3) return;
      if (_selection == _sequence[_drop]) ++_progress;
      if (_progress == 3) { _phase = WON; return; }
      if (++_drop == 5) { _phase = FAILED; return; }
      _stage = 0;
    }
  }
  void input(Action action, uint32_t now) {
    if (!active()) return;
    if (action == BACK) { cancel(); return; }
    tick(now);
    if (_phase == INSTRUCTIONS && action == ENTER) { startRound(now); return; }
    if (_phase == FAILED && action == ENTER) {
      if (_retry) finish();
      else { _retry = true; _phase = INSTRUCTIONS; }
      return;
    }
    if (_phase != PLAY) return;
    if (_game == ARROWS) {
      if (action >= UP && action <= LEFT) {
        if (action- UP != _sequence[_progress]) _phase = FAILED;
        else if (++_progress == 3) _phase = WON;
      }
    } else if (_game == TIMING && action == ENTER) {
      if (!inZone()) _phase = FAILED;
      else if (++_progress == 3) _phase = WON;
      else resetTimingMarker(now);
    } else if (_game == FOOD || _game == BOXES || _game == CHANGED_SHAPE) {
      if (_game == FOOD || _game == BOXES) {
        // Direct spatial selection stops at edges; changed shapes retain wrap.
        uint8_t positions = _game == FOOD ? FOOD_LANES : 3;
        if (action == LEFT && _selection > 0) --_selection;
        if (action == RIGHT && _selection+1 < positions) ++_selection;
      } else {
        if (action == LEFT) _selection = (_selection+2)%3;
        if (action == RIGHT) _selection = (_selection+1)%3;
      }
      if (action == ENTER && _game != FOOD) {
        if (_game == BOXES) { _phase = ANSWER; _at = now; }
        else _phase = _selection == _target ? WON : FAILED;
      }
    }
  }
  int refreshMs(uint32_t now) const {
    if (previewing() || _phase == ANSWER ||
        (_phase == PLAY && (_game == FOOD || _game == TIMING))) {
      uint32_t elapsed=now-_at;
      return elapsed >= interval() ? 1 : interval()-elapsed;
    }
    return 5000;
  }
};

} } // namespace zen::pet
