#pragma once

#include "PetRewardDay.h"
#include "PetEngine.h"
#include <string.h>

namespace zen { namespace pet {

// Passive observer only: no MeshCore, UI, timers, allocation or storage calls.
class PetMeshRewards {
public:
  enum Kind : uint8_t { DM, ROOM, CHANNEL };
  struct Progress {
    bool dm = false, shared = false, conversation = false, favourite = false;
    uint8_t bond = 0;
    uint16_t pending_xp = 0;
    uint8_t pending_bond = 0;
  };
private:
  struct Peer { uint8_t key[32] = {}; bool used = false, sent = false, received = false; };
  struct Attempt {
    uint8_t key[32] = {};
    uint32_t token = 0, message = 0;
    Kind kind = DM;
    bool used = false, completed = false;
  };
  struct Seen { uint8_t key[32] = {}; uint64_t token = 0; bool used = false; };
  PetRewardDay _day;
  Progress _progress;
  Peer _peers[8];
  Attempt _attempts[16];
  Seen _seen[8];
  uint8_t _peer_next = 0, _attempt_next = 0, _seen_next = 0;
  bool _enabled = false, _paused = false, _sleep = false;
  bool _restore_block=false;
  void clearEvidence() {
    for (auto& peer : _peers) peer = Peer();
    for (auto& attempt : _attempts) attempt = Attempt();
  }
  void bond(uint8_t target) {
    if (target <= _progress.bond) return;
    _progress.pending_bond += target - _progress.bond;
    if (_progress.pending_bond > 100) _progress.pending_bond = 100;
    _progress.bond = target;
  }
  void social(const uint8_t* key, bool sent, bool favourite) {
    Peer* match = nullptr;
    for (auto& peer : _peers)
      if (peer.used && memcmp(peer.key,key,32) == 0) { match = &peer; break; }
    if (!match) {
      match = &_peers[_peer_next++ % 8]; *match = Peer();
      memcpy(match->key,key,32); match->used = true;
    }
    if (sent) match->sent = true; else match->received = true;
    if (favourite) { _progress.favourite = true; bond(3); }
    if (match->sent && match->received) {
      _progress.conversation = true; bond(5);
    }
  }
  void xp(uint8_t amount) {
    uint32_t total = _progress.pending_xp + amount;
    const uint16_t limit = Evolution::xp(Evolution::LEVELS);
    _progress.pending_xp = total > limit ? limit : total;
  }
public:
  struct Checkpoint { Progress progress; PetRewardDay::Checkpoint day; };
  Checkpoint checkpoint() const { return {_progress,_day.checkpoint()}; }
  void restore(const Checkpoint& c,uint32_t now) {
    *this=PetMeshRewards(); _progress=c.progress; _day.restore(c.day,now);
    _restore_block=true;
  }
  const Progress& progress() const { return _progress; }
  void retire() {
    // Preserve claimed daily allowances, date guard and receive deduplication.
    // Old conversation evidence and unclaimed bonuses belong to the old pet.
    _progress.pending_xp=0; _progress.pending_bond=0; clearEvidence();
  }
  bool synchronized() const { return _day.synchronized(); }
  void update(uint32_t now, bool enabled, bool sleeping, bool paused,
              bool synced, int64_t local) {
    if (enabled || _enabled) {
      if (_day.update(now,synced,local)) {
        _restore_block=false;
        uint16_t pending_xp = _progress.pending_xp;
        uint8_t pending_bond = _progress.pending_bond;
        _progress = Progress();
        _progress.pending_xp = pending_xp; _progress.pending_bond = pending_bond;
        for (auto& peer : _peers) peer = Peer();
      }
    }
    if ((!enabled && _enabled) || (paused && !_paused)) clearEvidence();
    if (!enabled) { _progress.pending_xp = 0; _progress.pending_bond = 0; }
    _enabled = enabled; _sleep = sleeping; _paused = paused;
  }
  void started(Kind kind, const uint8_t* key, uint32_t message,
               uint8_t retry, uint32_t token, bool allowed) {
    if (_restore_block || !_enabled || _paused || !allowed || !token || (kind != CHANNEL && !key)) return;
    bool found = false, completed = false;
    for (const auto& attempt : _attempts) {
      if (attempt.used && attempt.kind == kind && attempt.message == message &&
          (kind == CHANNEL || memcmp(attempt.key,key,32) == 0)) {
        found = true; completed |= attempt.completed;
        if (attempt.token == token) return;
      }
    }
    if (completed || (retry && !found)) return;
    Attempt& attempt = _attempts[_attempt_next++ % 16]; attempt = Attempt();
    attempt.used = true; attempt.kind = kind; attempt.message = message; attempt.token = token;
    if (key) memcpy(attempt.key,key,32);
  }
  // Adapter resolves the current full identity/policy before completing.
  bool lookup(uint32_t token, Kind kind, uint8_t* key) const {
    for (const auto& attempt : _attempts)
      if (attempt.used && !attempt.completed && attempt.token == token &&
          ((kind == CHANNEL) == (attempt.kind == CHANNEL))) {
        memcpy(key,attempt.key,32); return true;
      }
    return false;
  }
  void completed(uint32_t token, Kind kind, bool allowed, bool favourite) {
    if (_restore_block || !_enabled || _paused || !allowed) return;
    for (auto& attempt : _attempts) {
      if (!attempt.used || attempt.completed || attempt.token != token ||
          ((kind == CHANNEL) != (attempt.kind == CHANNEL))) continue;
      const Attempt delivered = attempt;
      for (auto& sibling : _attempts)
        if (sibling.used && sibling.kind == delivered.kind && sibling.message == delivered.message &&
            memcmp(sibling.key,delivered.key,32) == 0) sibling.completed = true;
      if (delivered.kind == DM) {
        if (!_progress.dm) { _progress.dm = true; xp(20); }
        social(delivered.key,true,favourite);
      } else {
        if (!_progress.shared) { _progress.shared = true; xp(15); }
        if (delivered.kind == ROOM && favourite) { _progress.favourite = true; bond(3); }
      }
      return;
    }
  }
  void received(const uint8_t* key, uint64_t token, bool room, bool allowed, bool favourite) {
    if (_restore_block || !_enabled || _paused || !allowed || !key) return;
    if (token) {
      for (const auto& seen : _seen)
        if (seen.used && seen.token == token && memcmp(seen.key,key,32) == 0) return;
      Seen& seen = _seen[_seen_next++ % 8]; seen.used = true; seen.token = token;
      memcpy(seen.key,key,32);
    }
    if (!room) social(key,false,favourite);
    else if (favourite) { _progress.favourite = true; bond(3); }
  }
  bool apply(Engine& engine) {
    if (!_enabled || _paused || _sleep || engine.available() != Engine::OK ||
        (!_progress.pending_xp && !_progress.pending_bond)) return false;
    engine.bonus(_progress.pending_xp,_progress.pending_bond);
    _progress.pending_xp = 0; _progress.pending_bond = 0; return true;
  }
};

} } // namespace zen::pet
