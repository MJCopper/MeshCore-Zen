#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace zen {

// Compact, preference-compatible ringtone codec. Legacy notes used bits 0..6;
// bit 7 was unused and now records a sharp. Flats use their enharmonic sharp.
class RingtoneModel {
public:
  static constexpr uint8_t MAX_NOTES = 16;
  static constexpr uint8_t STORAGE_NOTES = 32;
  static constexpr uint8_t PITCH_COUNT = 13;
  static constexpr uint8_t BPM_COUNT = 5;
  static constexpr uint8_t DURATION_COUNT = 4;

  static uint16_t bpm(uint8_t index) {
    static const uint16_t VALUES[BPM_COUNT] = { 60, 90, 120, 150, 180 };
    return VALUES[index < BPM_COUNT ? index : 2];
  }

  static const char* durationLabel(uint8_t index) {
    static const char* const LABELS[DURATION_COUNT] = { "1/4", "1/8", "1/16", "1/32" };
    return LABELS[index < DURATION_COUNT ? index : 0];
  }

  static uint8_t octave(uint8_t note) { return ((note >> 3) & 0x03) + 4; }
  static uint8_t duration(uint8_t note) { return (note >> 5) & 0x03; }

  static uint8_t pitchIndex(uint8_t note) {
    uint8_t natural = note & 0x07;
    if (natural == 0) return 0;
    static const uint8_t NATURAL_INDEX[8] = { 0, 1, 3, 5, 6, 8, 10, 12 };
    uint8_t index = NATURAL_INDEX[natural];
    return (note & 0x80) && index < 12 ? index + 1 : index;
  }

  static uint8_t pack(uint8_t pitch_index, uint8_t note_octave, uint8_t duration_index) {
    static const uint8_t NATURAL[13] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6, 7 };
    static const uint8_t SHARP[13]   = { 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
    if (pitch_index >= PITCH_COUNT) pitch_index = 0;
    if (note_octave < 4) note_octave = 4;
    if (note_octave > 7) note_octave = 7;
    if (duration_index >= DURATION_COUNT) duration_index = 0;
    return NATURAL[pitch_index] | ((note_octave - 4) << 3) |
           (duration_index << 5) | (SHARP[pitch_index] ? 0x80 : 0);
  }

  static uint8_t withPitch(uint8_t note, uint8_t pitch_index) {
    return pack(pitch_index, octave(note), duration(note));
  }
  static uint8_t withOctave(uint8_t note, uint8_t note_octave) {
    return pack(pitchIndex(note), note_octave, duration(note));
  }
  static uint8_t withDuration(uint8_t note, uint8_t duration_index) {
    return pack(pitchIndex(note), octave(note), duration_index);
  }

  static void label(uint8_t note, char* out, size_t size) {
    if (!out || size == 0) return;
    static const char* const PITCHES[PITCH_COUNT] = {
      "--", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    uint8_t pitch = pitchIndex(note);
    if (pitch == 0) snprintf(out, size, "--");
    else snprintf(out, size, "%s%u", PITCHES[pitch], octave(note));
  }

  static void buildRTTTL(const uint8_t* notes, uint8_t len, uint8_t bpm_index,
                         char* out, size_t size) {
    if (!out || size == 0) return;
    out[0] = '\0';
    if (!notes || len == 0) return;
    if (len > MAX_NOTES) len = MAX_NOTES;
    static const uint8_t DURATIONS[DURATION_COUNT] = { 4, 8, 16, 32 };
    static const char NATURAL_NAMES[8] = { 'p', 'c', 'd', 'e', 'f', 'g', 'a', 'b' };
    int header = snprintf(out, size, "Ring:d=8,o=5,b=%u:", bpm(bpm_index));
    if (header < 0 || (size_t)header >= size) { out[size - 1] = '\0'; return; }
    size_t used = (size_t)header;
    for (uint8_t i = 0; i < len; i++) {
      uint8_t natural = notes[i] & 0x07;
      uint8_t dur = DURATIONS[duration(notes[i])];
      int written = natural == 0
          ? snprintf(out + used, size - used, "%s%up", i ? "," : "", dur)
          : snprintf(out + used, size - used, "%s%u%c%s%u", i ? "," : "", dur,
                     NATURAL_NAMES[natural], (notes[i] & 0x80) ? "#" : "", octave(notes[i]));
      if (written < 0 || (size_t)written >= size - used) { out[size - 1] = '\0'; return; }
      used += (size_t)written;
    }
  }
};

} // namespace zen
