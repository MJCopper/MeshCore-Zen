#pragma once

#include "TraceRouteCandidates.h"
#include <helpers/PacketScope.h>

// Exact-route evidence, scoped to transport key, channel and radio settings, never persisted.
class TraceRouteHistory {
public:
  static const uint8_t CAPACITY = 4;
  static const uint32_t TTL_MILLIS = 15 * 60 * 1000;
  void clear() { for (auto& entry : _entries) entry.active = false; }
  void record(const TraceRoute& route, uint8_t channel, uint32_t profile,
              uint32_t now, bool success, uint32_t rtt = 0, const mesh::PacketScope& scope = {}, const uint8_t* target_key = NULL) {
    if (!scope.reusable()) return;
    Entry* target = NULL;
    for (auto& entry : _entries) {
      if (entry.active && sameTarget(entry, target_key) && entry.scope.equals(scope) && entry.channel == channel && entry.profile == profile && entry.route.equals(route)) {
        target = &entry; break;
      }
    }
    if (!target) {
      for (auto& entry : _entries)
        if (!entry.active || (uint32_t)(now - entry.updated) >= TTL_MILLIS) { target = &entry; break; }
    }
    if (!target) {
      target = &_entries[0];
      for (auto& entry : _entries)
        if ((uint32_t)(now - entry.updated) > (uint32_t)(now - target->updated)) target = &entry;
    }
    bool same = target->active && sameTarget(*target, target_key) && target->scope.equals(scope) && target->channel == channel && target->profile == profile &&
                target->route.equals(route) && (uint32_t)(now - target->updated) < TTL_MILLIS;
    if (!same) *target = Entry{};
    target->active = true;
    target->route = route;
    target->scope = scope;
    target->targeted = target_key != NULL;
    if (target_key) memcpy(target->key, target_key, sizeof(target->key));
    target->channel = channel;
    target->profile = profile;
    target->updated = now;
    target->success = success;
    uint8_t& count = success ? target->successes : target->misses;
    if (count != 255) count++;
    if (success) target->rtt = rtt;
  }
  uint8_t rank(const TraceRoute& route, uint8_t channel, uint32_t profile, uint32_t now, const mesh::PacketScope& scope = {}, const uint8_t* target_key = NULL) const {
    if (!scope.reusable()) return 1;
    for (const auto& entry : _entries)
      if (entry.active && sameTarget(entry, target_key) && entry.scope.equals(scope) && entry.channel == channel && entry.profile == profile &&
          (uint32_t)(now - entry.updated) < TTL_MILLIS && entry.route.equals(route))
        return entry.success ? 2 : 0;
    return 1;
  }
private:
  struct Entry {
    bool targeted = false;
    uint8_t key[32]{};
    TraceRoute route{};
    mesh::PacketScope scope;
    uint32_t profile = 0, updated = 0, rtt = 0;
    uint8_t channel = 0, successes = 0, misses = 0;
    bool success = false, active = false;
  } _entries[CAPACITY];
  static bool sameTarget(const Entry& entry, const uint8_t* key) {
    return entry.targeted == (key != NULL) && (!key || memcmp(entry.key, key, 32) == 0);
  }
};
