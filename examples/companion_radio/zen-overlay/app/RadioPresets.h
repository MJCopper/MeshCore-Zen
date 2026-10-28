#pragma once
#include <math.h>
#include <stdint.h>

// Offline catalogue generated from the committed official MeshCore API snapshot.
struct RadioPreset {
  const char* name;
  float freq;   // MHz
  float bw;     // kHz
  uint8_t sf;
  uint8_t cr;
  uint8_t path_hash_size; // 0=unspecified; otherwise 1..3 bytes.
};

#include "RadioPresets.generated.h"

// Standard SX126x/SX127x LoRa bandwidths (kHz) — manual BW fields (companion and
// repeater profile alike) cycle through these rather than a free-form value, so
// they can't land on an unsupported setting.
static const float LORA_BW_OPTS[] = { 7.8f, 10.4f, 15.6f, 20.8f, 31.25f, 41.7f, 62.5f, 125.0f, 250.0f, 500.0f };
static const int LORA_BW_OPT_COUNT = sizeof(LORA_BW_OPTS) / sizeof(LORA_BW_OPTS[0]);

// Index of the LORA_BW_OPTS entry nearest `bw` — used both to render the current
// BW and to step left/right through the table without drifting off-grid.
static inline int nearestBwIndex(float bw) {
  int best = 0;
  float best_diff = 1e9f;
  for (int i = 0; i < LORA_BW_OPT_COUNT; i++) {
    float diff = fabsf(bw - LORA_BW_OPTS[i]);
    if (diff < best_diff) { best_diff = diff; best = i; }
  }
  return best;
}

// True if (freq, bw, sf, cr) matches the radio's currently-applied (cur_*) params.
// Floats are compared with a small epsilon since they may have round-tripped
// through unit conversions (e.g. kHz integers from the companion app).
static inline bool radioParamsMatchPreset(float cur_freq, float cur_bw, uint8_t cur_sf, uint8_t cur_cr,
                                           float freq, float bw, uint8_t sf, uint8_t cr) {
  return fabsf(cur_freq - freq) < 0.001f && fabsf(cur_bw - bw) < 0.001f && cur_sf == sf && cur_cr == cr;
}
