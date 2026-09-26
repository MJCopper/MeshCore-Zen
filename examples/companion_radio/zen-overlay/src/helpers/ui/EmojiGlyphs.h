#pragma once

#include <Adafruit_GFX.h>
#include "ZenDisplayDriver.h"
#include "EmojiGlyphData.h"
#include "EmojiOverrides.h"

// Tiny emoji support for the 6x9 monochrome UI font. The generated artwork is
// isolated in EmojiGlyphData.h so changing the source set never touches the
// renderer or the main MiscFixed font.

static inline int16_t emojiGlyphIndex(uint32_t cp) {
  for (uint8_t i = 0; i < emojiOverrideCount; i++)
    if (pgm_read_dword(&emojiOverrideCodepoints[i]) == cp) return -2 - i;
  int16_t lo = 0, hi = emojiScalarGlyphCount - 1;
  while (lo <= hi) {
    int16_t mid = (lo + hi) >> 1;
    uint32_t value = pgm_read_dword(&emojiGlyphCodepoints[mid]);
    if (value == cp) return mid;
    if (value < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

static inline bool emojiIsVariation(uint32_t cp) {
  return cp == 0xFE0E || cp == 0xFE0F;
}

static inline bool emojiIsModifier(uint32_t cp) {
  return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

static inline bool emojiIsRegionalIndicator(uint32_t cp) {
  return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

static inline int16_t emojiFlagGlyphIndex(uint32_t first, const uint8_t* p) {
  if (!*p) return -1;
  const uint8_t* next = p;
  uint32_t second = ZenDisplayDriver::decodeCodepoint(next);
  if (!emojiIsRegionalIndicator(second)) return -1;
  for (uint8_t i = 0; i < emojiFlagCount; i++) {
    if (pgm_read_dword(&emojiFlagFirst[i]) == first &&
        pgm_read_dword(&emojiFlagSecond[i]) == second)
      return (int16_t)pgm_read_word(&emojiFlagGlyphIndices[i]);
  }
  return -1;
}

static inline bool emojiIsCodepoint(uint32_t cp) {
  return (cp >= 0x1F000 && cp <= 0x1FAFF) ||
         (cp >= 0x2600 && cp <= 0x27BF) || emojiIsRegionalIndicator(cp);
}

static inline bool emojiConsumeKeycap(const uint8_t*& p, uint32_t first) {
  if (!((first >= '0' && first <= '9') || first == '#' || first == '*')) return false;
  const uint8_t* next = p;
  if (!*next) return false;
  uint32_t cp = ZenDisplayDriver::decodeCodepoint(next);
  if (emojiIsVariation(cp)) {
    if (!*next) return false;
    cp = ZenDisplayDriver::decodeCodepoint(next);
  }
  if (cp != 0x20E3) return false;
  p = next;
  return true;
}

// Consume selectors/modifiers and any joined tail after the first codepoint.
// Unsupported compound emoji therefore occupy one cell instead of producing a
// row of substitution marks. Supported bases use their neutral artwork for all
// skin tones until dedicated variants are added.
static inline void emojiConsumeSuffix(const uint8_t*& p, uint32_t first) {
  bool regional = emojiIsRegionalIndicator(first);
  bool joined = false;
  while (*p) {
    const uint8_t* next = p;
    uint32_t cp = ZenDisplayDriver::decodeCodepoint(next);
    if (emojiIsVariation(cp) || emojiIsModifier(cp) || cp == 0x20E3) {
      p = next;
      continue;
    }
    if (regional && emojiIsRegionalIndicator(cp)) {
      p = next;
      regional = false;
      continue;
    }
    if (cp == 0x200D) {
      p = next;
      joined = true;
      continue;
    }
    if (joined) {
      p = next;
      joined = false;
      continue;
    }
    break;
  }
}

static inline void emojiDrawRows(Adafruit_GFX& gfx, int16_t x, int16_t y,
                                 const uint8_t* rows, int sz, uint16_t color) {
  for (uint8_t row = 0; row < 8; row++) {
    uint8_t bits = pgm_read_byte(rows + row);
    for (uint8_t col = 0; col < 5; col++) {
      if (!(bits & (0x10 >> col))) continue;
      if (sz == 1) gfx.drawPixel(x + col, y + row, color);
      else gfx.fillRect(x + col * sz, y + row * sz, sz, sz, color);
    }
  }
}

static inline int16_t emojiDrawGlyph(Adafruit_GFX& gfx, int16_t x, int16_t y,
                                     int16_t index, int sz, uint16_t color) {
  // A guaranteed hollow-diamond fallback. U+25C7 is outside MiscFixed's range,
  // so keeping the pixels here avoids depending on another font.
  static const uint8_t fallback[8] PROGMEM = {
    0x04, 0x0A, 0x11, 0x11, 0x11, 0x0A, 0x04, 0x00
  };
  const uint8_t* rows = index >= 0 ? emojiGlyphRows[index] :
                        index <= -2 ? emojiOverrideRows[-2 - index] : fallback;
  emojiDrawRows(gfx, x, y, rows, sz, color);
  return x + 6 * sz;
}
