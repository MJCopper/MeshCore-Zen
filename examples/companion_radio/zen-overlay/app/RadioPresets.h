#pragma once
#include <math.h>

// Community-suggested per-region RF presets (freq/bw/sf/cr), sourced from the
// MeshCore app's "suggested_radio_settings" config plus a few community additions.
// Kept here (rather than guessed) since wrong values are a regulatory/connectivity
// concern, not just cosmetic.
struct RadioPreset {
  const char* name;
  float freq;   // MHz
  float bw;     // kHz
  uint8_t sf;
  uint8_t cr;
};

// Deprecated entries and anything outside the 868-9xx MHz bands (433MHz) have been
// dropped — only current, in-band presets are kept.
static const RadioPreset RADIO_PRESETS[] = {
  { "Australia",               915.800f, 250.0f, 10, 5 },
  { "Australia (Narrow)",      916.575f,  62.5f,  7, 8 },
  { "Australia (Mid)",         915.075f, 125.0f,  9, 5 },
  { "Australia: SA, WA",       923.125f,  62.5f,  8, 8 },
  { "Australia: QLD",          923.125f,  62.5f,  8, 5 },
  { "Brazil",                  923.125f,  62.5f,  8, 8 },
  { "EU/UK (Narrow)",          869.618f,  62.5f,  8, 8 },
  { "Czech Republic (Narrow)", 869.432f,  62.5f,  7, 5 },
  { "Netherlands",             869.618f,  62.5f,  7, 5 },
  { "New Zealand",             917.375f, 250.0f, 11, 5 },
  { "New Zealand (Narrow)",    917.375f,  62.5f,  7, 5 },
  { "Portugal 868",            869.618f,  62.5f,  7, 6 },
  { "Switzerland",             869.618f,  62.5f,  8, 8 },
  { "USA/Canada (Recommended)",910.525f,  62.5f,  7, 5 },
  { "USA/SOCAL",                927.875f,  62.5f,  7, 5 },
  { "Vietnam (Narrow)",        920.250f,  62.5f,  8, 5 },
};
static const int RADIO_PRESET_COUNT = sizeof(RADIO_PRESETS) / sizeof(RADIO_PRESETS[0]);

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
