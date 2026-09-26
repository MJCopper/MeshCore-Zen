#pragma once

#include "../ZenPrefs.h"
#include <stdint.h>
#include <string.h>

// Shared metadata and saved-order maintenance for the home carousel. Screen
// enums remain local to their UI classes; persisted bit IDs and policy live
// here so the settings editor and runtime carousel use the same rules.
namespace homepage {

static inline bool alwaysVisible(uint8_t bit) {
  return bit == ZenPrefs::HPB_CLOCK || bit == ZenPrefs::HPB_SETTINGS ||
         bit == ZenPrefs::HPB_QUICK_MSG;
}

static inline bool childOptional(uint8_t bit) { return bit == ZenPrefs::HPB_FAVOURITES; }

static inline bool childHidden(uint8_t bit) {
  return bit == ZenPrefs::HPB_RADIO || bit == ZenPrefs::HPB_BLUETOOTH ||
         bit == ZenPrefs::HPB_ADVERT || bit == ZenPrefs::HPB_GPS ||
         bit == ZenPrefs::HPB_TOOLS || bit == ZenPrefs::HPB_SENSORS;
}

static inline bool visible(const ZenPrefs* prefs, uint8_t bit, bool child_locked) {
  if (alwaysVisible(bit)) return true;
  uint16_t mask = (prefs && prefs->home_pages_mask) ? prefs->home_pages_mask
                                                     : ZenPrefs::HP_ALL;
  if (child_locked) {
    if (childOptional(bit))
      return prefs && (prefs->child_visible_pages & (uint16_t)(1U << bit));
    if (childHidden(bit)) return false;
  }
  return (mask & (uint16_t)(1U << bit)) != 0;
}

static inline int defaultOrder(uint8_t* order, int capacity) {
  if (!order || capacity <= 0) return 0;
  static const uint8_t BITS[] = {
    ZenPrefs::HPB_CLOCK, ZenPrefs::HPB_QUICK_MSG, ZenPrefs::HPB_FAVOURITES,
    ZenPrefs::HPB_SENSORS,
#if ENV_INCLUDE_GPS == 1
    ZenPrefs::HPB_GPS,
#endif
    ZenPrefs::HPB_ADVERT, ZenPrefs::HPB_BLUETOOTH, ZenPrefs::HPB_RADIO,
    ZenPrefs::HPB_TOOLS, ZenPrefs::HPB_SETTINGS,
  };
  int count = 0;
  for (int i = 0; i < (int)(sizeof(BITS) / sizeof(BITS[0])) && count < capacity; i++)
    order[count++] = BITS[i];
  return count;
}

static inline void ensureOrder(ZenPrefs* prefs) {
  if (!prefs) return;

  uint8_t required[ZenPrefs::PAGE_ORDER_LEN];
  int required_count = defaultOrder(required, ZenPrefs::PAGE_ORDER_LEN);
  bool valid = prefs->page_order_set == ZenPrefs::PAGE_ORDER_MAGIC;
  uint16_t present = 0;
  int length = 0;
  if (valid) {
    for (; length < ZenPrefs::PAGE_ORDER_LEN; length++) {
      uint8_t stored = prefs->page_order[length];
      if (stored < 1 || stored > ZenPrefs::HPB_COUNT) break;
      uint16_t mask = (uint16_t)(1U << (stored - 1));
      if (present & mask) { valid = false; break; }
      present |= mask;
    }
    valid = valid && (present & (uint16_t)(1U << ZenPrefs::HPB_CLOCK));
  }

  if (!valid) {
    memset(prefs->page_order, 0, sizeof(prefs->page_order));
    length = 0;
    present = 0;
  } else {
    // Clock is the fixed home anchor. Repair older custom orders that moved it
    // while preserving the relative order of every other page.
    int clock = -1;
    for (int i = 0; i < length; i++)
      if (prefs->page_order[i] == ZenPrefs::HPB_CLOCK + 1) { clock = i; break; }
    if (clock > 0) {
      uint8_t stored_clock = prefs->page_order[clock];
      for (int i = clock; i > 0; i--) prefs->page_order[i] = prefs->page_order[i - 1];
      prefs->page_order[0] = stored_clock;
    }
  }

  if (valid && !(present & (uint16_t)(1U << ZenPrefs::HPB_FAVOURITES))) {
    // Preserve the established migration rule: Favourites was introduced
    // immediately after Clock, rather than appearing at the end of an older
    // custom order.
    int clock = -1;
    for (int i = 0; i < length; i++)
      if (prefs->page_order[i] == ZenPrefs::HPB_CLOCK + 1) { clock = i; break; }
    int insert = clock + 1;
    if (clock >= 0 && insert < ZenPrefs::PAGE_ORDER_LEN) {
      int tail = length < ZenPrefs::PAGE_ORDER_LEN ? length : ZenPrefs::PAGE_ORDER_LEN - 1;
      for (int i = tail; i > insert; i--) prefs->page_order[i] = prefs->page_order[i - 1];
      prefs->page_order[insert] = ZenPrefs::HPB_FAVOURITES + 1;
      if (length < ZenPrefs::PAGE_ORDER_LEN) length++;
      present |= (uint16_t)(1U << ZenPrefs::HPB_FAVOURITES);
    }
  }

  // Preserve a valid user order and append pages introduced by newer builds.
  for (int i = 0; i < required_count && length < ZenPrefs::PAGE_ORDER_LEN; i++) {
    uint16_t mask = (uint16_t)(1U << required[i]);
    if (!(present & mask)) {
      prefs->page_order[length++] = required[i] + 1;
      present |= mask;
    }
  }
  while (length < ZenPrefs::PAGE_ORDER_LEN) prefs->page_order[length++] = 0;
  prefs->page_order_set = ZenPrefs::PAGE_ORDER_MAGIC;
}

static inline int position(const ZenPrefs* prefs, uint8_t bit) {
  if (!prefs || prefs->page_order_set != ZenPrefs::PAGE_ORDER_MAGIC) return 0;
  for (int i = 0; i < ZenPrefs::PAGE_ORDER_LEN; i++) {
    uint8_t stored = prefs->page_order[i];
    if (stored < 1 || stored > ZenPrefs::HPB_COUNT) break;
    if (stored - 1 == bit) return i + 1;
  }
  return 0;
}

// Authoritative runtime order. Persisted order is honoured when valid, then
// any newly introduced visible pages are appended in the default order.
static inline int visibleOrder(const ZenPrefs* prefs, bool child_locked,
                               uint8_t* out, int capacity) {
  if (!out || capacity <= 0) return 0;
  int count = 0;
  uint16_t present = 0;
  if (prefs && prefs->page_order_set == ZenPrefs::PAGE_ORDER_MAGIC) {
    for (int i = 0; i < ZenPrefs::PAGE_ORDER_LEN && count < capacity; i++) {
      uint8_t stored = prefs->page_order[i];
      if (stored < 1 || stored > ZenPrefs::HPB_COUNT) break;
      uint8_t bit = stored - 1;
      uint16_t mask = (uint16_t)(1U << bit);
      if (present & mask) continue;
      present |= mask;
      if (visible(prefs, bit, child_locked)) out[count++] = bit;
    }
  }
  uint8_t defaults[ZenPrefs::PAGE_ORDER_LEN];
  int defaults_count = defaultOrder(defaults, ZenPrefs::PAGE_ORDER_LEN);
  for (int i = 0; i < defaults_count && count < capacity; i++) {
    uint8_t bit = defaults[i];
    uint16_t mask = (uint16_t)(1U << bit);
    if (!(present & mask) && visible(prefs, bit, child_locked)) out[count++] = bit;
  }
  return count;
}

static inline void move(ZenPrefs* prefs, uint8_t bit, int delta) {
  ensureOrder(prefs);
  if (!prefs || bit == ZenPrefs::HPB_CLOCK) return;
  int current = -1, count = 0;
  for (int i = 0; i < ZenPrefs::PAGE_ORDER_LEN; i++) {
    uint8_t stored = prefs->page_order[i];
    if (stored < 1 || stored > ZenPrefs::HPB_COUNT) break;
    if (stored - 1 == bit) current = i;
    count++;
  }
  int next = current + delta;
  if (current < 0 || next < 1 || next >= count) return;
  uint8_t temp = prefs->page_order[current];
  prefs->page_order[current] = prefs->page_order[next];
  prefs->page_order[next] = temp;
}

} // namespace homepage
