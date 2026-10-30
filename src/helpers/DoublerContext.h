#pragma once

#include "FloodTextDoubler.h"

// Runtime-only observations. Radio settings enter here only when applied to
// hardware; reboot-only preference edits and per-message scopes are excluded.
class DoublerContext {
  bool have_radio = false, have_packet = false;
  float frequency = 0, bandwidth = 0;
  uint8_t sf = 0, cr = 0, hash_mode = 0;
  uint8_t identity[PUB_KEY_SIZE] = {}, scope[16] = {};

public:
  void radioApplied(FloodTextDoubler& policy, float freq, float bw, uint8_t spreading, uint8_t coding) {
    if (have_radio && (frequency != freq || bandwidth != bw || sf != spreading || cr != coding))
      policy.invalidateContext();
    frequency = freq; bandwidth = bw; sf = spreading; cr = coding;
    have_radio = true;
  }

  void packetContext(FloodTextDoubler& policy, const uint8_t* pubkey,
                     uint8_t path_hash_mode, const uint8_t* default_scope) {
    if (have_packet && (hash_mode != path_hash_mode || memcmp(identity, pubkey, sizeof(identity))
                       || memcmp(scope, default_scope, sizeof(scope)))) policy.invalidateContext();
    memcpy(identity, pubkey, sizeof(identity));
    memcpy(scope, default_scope, sizeof(scope));
    hash_mode = path_hash_mode;
    have_packet = true;
  }
};
static_assert(sizeof(DoublerContext) <= 64, "Doubler context exceeded its fixed RAM budget");
