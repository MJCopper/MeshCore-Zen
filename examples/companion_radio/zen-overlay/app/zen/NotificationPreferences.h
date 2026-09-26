#pragma once

#include "../ZenPrefs.h"
#include <stdint.h>
#include <string.h>

// Shared interpretation of the compact notification preference storage.
// Keeping reads, writes and deletion cleanup here ensures the settings UI and
// notification dispatcher cannot disagree about the encoded state values.
namespace zen {

class NotificationPreferences {
  template <class Entry>
  static uint8_t tableGet(const Entry* table, int count, const uint8_t* pub_key,
                          uint8_t Entry::* value) {
    if (!table || !pub_key) return 0;
    for (int i = 0; i < count; i++)
      if (table[i].*value && memcmp(table[i].prefix, pub_key, 4) == 0)
        return table[i].*value;
    return 0;
  }

  template <class Entry>
  static bool tableSet(Entry* table, int count, const uint8_t* pub_key,
                       uint8_t Entry::* value, uint8_t setting) {
    if (!table || !pub_key) return false;
    for (int i = 0; i < count; i++) {
      if (table[i].*value && memcmp(table[i].prefix, pub_key, 4) == 0) {
        if (setting == 0) memset(&table[i], 0, sizeof(table[i]));
        else table[i].*value = setting;
        return true;
      }
    }
    if (setting == 0) return true;
    for (int i = 0; i < count; i++) {
      if (table[i].*value == 0) {
        memcpy(table[i].prefix, pub_key, 4);
        table[i].*value = setting;
        return true;
      }
    }
    return false;
  }

  static uint8_t maskGet(uint64_t presence, uint64_t variant, uint8_t index,
                         uint8_t variant_set, uint8_t variant_clear) {
    if (index >= 64) return 0;
    uint64_t mask = 1ULL << index;
    if (!(presence & mask)) return 0;
    return (variant & mask) ? variant_set : variant_clear;
  }

  static void maskSet(uint64_t& presence, uint64_t& variant, uint8_t index,
                      uint8_t setting, uint8_t variant_set) {
    if (index >= 64) return;
    uint64_t mask = 1ULL << index;
    if (setting == 0) {
      presence &= ~mask;
      variant &= ~mask;
    } else {
      presence |= mask;
      if (setting == variant_set) variant |= mask;
      else variant &= ~mask;
    }
  }

  static uint8_t nibbleGet(const uint8_t* values, uint8_t index) {
    if (!values || index >= 64) return 0;
    uint8_t packed = values[index >> 1];
    return index & 1 ? packed >> 4 : packed & 0x0F;
  }

  static void nibbleSet(uint8_t* values, uint8_t index, uint8_t setting) {
    if (!values || index >= 64) return;
    uint8_t& packed = values[index >> 1];
    if (index & 1) packed = (packed & 0x0F) | ((setting & 0x0F) << 4);
    else           packed = (packed & 0xF0) | (setting & 0x0F);
  }

public:
  // Notification state: 0=global, 1=no local alert, 2=local alert in Auto.
  static uint8_t dmState(const ZenPrefs* prefs, const uint8_t* pub_key) {
    return prefs ? tableGet(prefs->dm_notif, ZenPrefs::DM_NOTIF_TABLE_MAX,
                            pub_key, &ZenPrefs::DmNotifEntry::state) : 0;
  }

  static bool setDmState(ZenPrefs* prefs, const uint8_t* pub_key, uint8_t state) {
    return prefs && tableSet(prefs->dm_notif, ZenPrefs::DM_NOTIF_TABLE_MAX,
                             pub_key, &ZenPrefs::DmNotifEntry::state, state);
  }

  static uint8_t channelState(const ZenPrefs* prefs, uint8_t index) {
    return prefs ? maskGet(prefs->ch_notif_override, prefs->ch_notif_muted,
                           index, 1, 2) : 0;
  }

  static bool setChannelState(ZenPrefs* prefs, uint8_t index, uint8_t state) {
    if (!prefs || index >= 64) return false;
    maskSet(prefs->ch_notif_override, prefs->ch_notif_muted, index, state, 1);
    return true;
  }

  static uint8_t dmMelody(const ZenPrefs* prefs, const uint8_t* pub_key) {
    return prefs ? tableGet(prefs->dm_melody, ZenPrefs::DM_MELODY_TABLE_MAX,
                            pub_key, &ZenPrefs::DmMelodyEntry::slot) : 0;
  }

  static bool setDmMelody(ZenPrefs* prefs, const uint8_t* pub_key, uint8_t slot) {
    return prefs && tableSet(prefs->dm_melody, ZenPrefs::DM_MELODY_TABLE_MAX,
                             pub_key, &ZenPrefs::DmMelodyEntry::slot, slot);
  }

  static uint8_t channelMelody(const ZenPrefs* prefs, uint8_t index) {
    return prefs ? nibbleGet(prefs->channel_melody_overrides, index) : 0;
  }

  static bool setChannelMelody(ZenPrefs* prefs, uint8_t index, uint8_t slot) {
    if (!prefs || index >= 64) return false;
    nibbleSet(prefs->channel_melody_overrides, index, slot);
    return true;
  }

  static bool removeContact(ZenPrefs* prefs, const uint8_t* pub_key) {
    if (!prefs || !pub_key) return false;
    bool changed = dmState(prefs, pub_key) || dmMelody(prefs, pub_key);
    setDmState(prefs, pub_key, 0);
    setDmMelody(prefs, pub_key, 0);
    return changed;
  }

  static bool removeChannel(ZenPrefs* prefs, uint8_t index) {
    if (!prefs || index >= 64) return false;
    bool changed = channelState(prefs, index) || channelMelody(prefs, index);
    setChannelState(prefs, index, 0);
    setChannelMelody(prefs, index, 0);
    return changed;
  }
};

} // namespace zen
