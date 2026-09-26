#pragma once
#include "DigitEditor.h"
#include "../RadioPresets.h"

// Shared manual radio-parameter editing for Settings › Radio and Tools ›
// Repeater: a digit-by-digit Freq editor plus left/right stepping of the
// discrete SF/BW/CR sets. Both screens edit the same field set with the same
// bounds — only the target fields (companion params vs. the dedicated repeater
// profile) and the "apply to the radio" call differ, which stay with the screen.
// No dependency on UITask or the radio driver: the screen passes the Zen UI
// policy bounds in, keeping radio application inside the MeshCore baseline.
struct RadioParamsEditor {
  DigitEditor freq;            // the modal Freq editor; active flag lives here
  float* _freq_field = nullptr;  // field begin() is editing, written back on change

  bool active() const { return freq.active; }
  void render(ZenDisplayDriver& d, int x, int y) { freq.render(d, x, y); }

  // Open the digit-by-digit Freq editor on `field`. Bounds are the radio chip's
  // own validated range, so a digit can't be nudged to a value setFrequency()
  // would reject. 3 integer + 3 decimal digits = kHz precision (the SX126x
  // range fits 3 integer digits).
  void beginFreq(float& field, float min_mhz, float max_mhz) {
    _freq_field = &field;
    freq.begin(field, min_mhz, max_mhz, 3, 3);
  }

  // Feed a key to the open Freq editor and write the edited value back to the
  // field. Returns true if the field changed — the caller then applies it to the
  // radio and marks itself dirty. Cursor moves / open-close consume the key but
  // report no change.
  bool handleFreqInput(char c) {
    float before = freq.value;
    freq.handleInput(c);
    if (_freq_field && freq.value != before) { *_freq_field = freq.value; return true; }
    return false;
  }

  // Step the discrete SF/BW/CR sets within their valid LoRa ranges. dir > 0 =
  // right/up, dir < 0 = left/down. Mutates and returns true only when the value
  // actually changed (so a step at the limit is a no-op the caller can ignore).
  static bool stepSF(uint8_t& sf, int dir) {
    if (dir > 0 && sf < 12) { sf++; return true; }
    if (dir < 0 && sf > 5)  { sf--; return true; }
    return false;
  }
  static bool stepCR(uint8_t& cr, int dir) {
    if (dir > 0 && cr < 8) { cr++; return true; }
    if (dir < 0 && cr > 5) { cr--; return true; }
    return false;
  }
  static bool stepBW(float& bw, int dir) {
    int idx = nearestBwIndex(bw);
    if (dir > 0 && idx < LORA_BW_OPT_COUNT - 1) { bw = LORA_BW_OPTS[idx + 1]; return true; }
    if (dir < 0 && idx > 0)                     { bw = LORA_BW_OPTS[idx - 1]; return true; }
    return false;
  }
};
