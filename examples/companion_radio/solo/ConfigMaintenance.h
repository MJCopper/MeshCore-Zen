#pragma once

#include "../NodePrefs.h"
#include "BuiltinMelodies.h"
#include "NotificationPreferences.h"
#include "RingtoneModel.h"
#include "TimezonePolicy.h"
#include "SoloPrefsDefaults.h"
#include <string.h>

namespace solo {

// Semantic migrations and sanitisation for Zen-owned preferences. The file
// sentinel protects byte layout; this version tracks the meaning of the data.
class ConfigMaintenance {
  static bool clearBytes(void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    bool changed = false;
    for (size_t i = 0; i < size; i++) changed |= bytes[i] != 0;
    if (changed) memset(data, 0, size);
    return changed;
  }

  template <class Entry>
  static bool compactTable(Entry* table, int count, uint8_t Entry::* value,
                           uint8_t maximum) {
    Entry clean[NodePrefs::DM_NOTIF_TABLE_MAX];
    memset(clean, 0, sizeof(clean));
    int used = 0;
    for (int i = 0; i < count; i++) {
      uint8_t setting = table[i].*value;
      if (!setting || setting > maximum) continue;
      int duplicate = -1;
      for (int j = 0; j < used; j++) {
        if (memcmp(clean[j].prefix, table[i].prefix, 4) == 0) { duplicate = j; break; }
      }
      if (duplicate >= 0) clean[duplicate].*value = setting;
      else clean[used++] = table[i];
    }
    bool changed = memcmp(table, clean, sizeof(Entry) * count) != 0;
    if (changed) memcpy(table, clean, sizeof(Entry) * count);
    return changed;
  }

  static bool applySchemaMigrations(NodePrefs& prefs) {
    bool changed = false;
    while (prefs.zen_config_schema < CURRENT_SCHEMA) {
      switch (prefs.zen_config_schema) {
        case 0:
          // The schema-1 cleanup has completed on every supported device.
          // Retain this step so new records join the migration sequence without
          // repeating the retired bulk cleanup.
          prefs.zen_config_schema = 1;
          changed = true;
          break;
        case 1:
          // Zen no longer ships the legacy Bot, Trail, Live Share, Locator,
          // Clock Tools or user-GPIO features. Keep their bytes in NodePrefs so
          // the established file layout remains readable, but erase dormant
          // configuration once so it cannot reactivate accidentally.
          changed |= clearBytes(&prefs.bot_enabled, sizeof(prefs.bot_enabled));
          changed |= clearBytes(&prefs.bot_channel_enabled, sizeof(prefs.bot_channel_enabled));
          changed |= clearBytes(&prefs.bot_channel_idx, sizeof(prefs.bot_channel_idx));
          changed |= clearBytes(prefs.bot_trigger, sizeof(prefs.bot_trigger));
          changed |= clearBytes(prefs.bot_reply_dm, sizeof(prefs.bot_reply_dm));
          changed |= clearBytes(prefs.bot_reply_ch, sizeof(prefs.bot_reply_ch));
          changed |= clearBytes(prefs.bot_trigger_ch, sizeof(prefs.bot_trigger_ch));
          changed |= clearBytes(&prefs.bot_commands_enabled, sizeof(prefs.bot_commands_enabled));
          changed |= clearBytes(&prefs.bot_quiet_start, sizeof(prefs.bot_quiet_start));
          changed |= clearBytes(&prefs.bot_quiet_end, sizeof(prefs.bot_quiet_end));
          changed |= clearBytes(&prefs.trail_interval_idx, sizeof(prefs.trail_interval_idx));
          changed |= clearBytes(&prefs.trail_min_delta_idx, sizeof(prefs.trail_min_delta_idx));
          changed |= clearBytes(&prefs.trail_units_idx, sizeof(prefs.trail_units_idx));
          changed |= clearBytes(&prefs.trail_show_pace, sizeof(prefs.trail_show_pace));
          changed |= clearBytes(&prefs.track_shared_loc, sizeof(prefs.track_shared_loc));
          changed |= clearBytes(&prefs.loc_share_enabled, sizeof(prefs.loc_share_enabled));
          changed |= clearBytes(&prefs.loc_share_target_type, sizeof(prefs.loc_share_target_type));
          changed |= clearBytes(&prefs.loc_share_channel_idx, sizeof(prefs.loc_share_channel_idx));
          changed |= clearBytes(prefs.loc_share_dm_prefix, sizeof(prefs.loc_share_dm_prefix));
          changed |= clearBytes(&prefs.loc_share_move_idx, sizeof(prefs.loc_share_move_idx));
          changed |= clearBytes(&prefs.loc_share_interval_idx, sizeof(prefs.loc_share_interval_idx));
          changed |= clearBytes(&prefs.loc_share_heartbeat_idx, sizeof(prefs.loc_share_heartbeat_idx));
          changed |= clearBytes(&prefs.locator_enabled, sizeof(prefs.locator_enabled));
          changed |= clearBytes(&prefs.locator_has_target, sizeof(prefs.locator_has_target));
          changed |= clearBytes(&prefs.locator_radius_idx, sizeof(prefs.locator_radius_idx));
          changed |= clearBytes(&prefs.locator_mode, sizeof(prefs.locator_mode));
          changed |= clearBytes(&prefs.locator_lat_1e6, sizeof(prefs.locator_lat_1e6));
          changed |= clearBytes(&prefs.locator_lon_1e6, sizeof(prefs.locator_lon_1e6));
          changed |= clearBytes(prefs.locator_label, sizeof(prefs.locator_label));
          changed |= clearBytes(&prefs.locator_target_kind, sizeof(prefs.locator_target_kind));
          changed |= clearBytes(prefs.locator_key, sizeof(prefs.locator_key));
          changed |= clearBytes(&prefs.trail_autopause_idx, sizeof(prefs.trail_autopause_idx));
          changed |= clearBytes(&prefs.locator_beeper, sizeof(prefs.locator_beeper));
          changed |= clearBytes(&prefs.gps_avg_idx, sizeof(prefs.gps_avg_idx));
          changed |= clearBytes(&prefs.alarm_on, sizeof(prefs.alarm_on));
          changed |= clearBytes(&prefs.alarm_hour, sizeof(prefs.alarm_hour));
          changed |= clearBytes(&prefs.alarm_min, sizeof(prefs.alarm_min));
          changed |= clearBytes(&prefs.alarm_repeat_mask, sizeof(prefs.alarm_repeat_mask));
          changed |= clearBytes(&prefs.trail_autosave_lowbatt, sizeof(prefs.trail_autosave_lowbatt));
          changed |= clearBytes(&prefs.bot_dm_scope, sizeof(prefs.bot_dm_scope));
          changed |= clearBytes(&prefs.bot_room_enabled, sizeof(prefs.bot_room_enabled));
          changed |= clearBytes(prefs.bot_room_prefix, sizeof(prefs.bot_room_prefix));
          changed |= clearBytes(prefs.bot_trigger_room, sizeof(prefs.bot_trigger_room));
          changed |= clearBytes(prefs.bot_reply_room, sizeof(prefs.bot_reply_room));
          changed |= clearBytes(&prefs.bot_commands_ch, sizeof(prefs.bot_commands_ch));
          changed |= clearBytes(&prefs.bot_commands_room, sizeof(prefs.bot_commands_room));
          changed |= clearBytes(&prefs.bot_actions_dm, sizeof(prefs.bot_actions_dm));
          changed |= clearBytes(&prefs.bot_actions_ch, sizeof(prefs.bot_actions_ch));
          changed |= clearBytes(&prefs.bot_actions_room, sizeof(prefs.bot_actions_room));
          changed |= clearBytes(&prefs.gpio1_mode, sizeof(prefs.gpio1_mode));
          changed |= clearBytes(&prefs.gpio2_mode, sizeof(prefs.gpio2_mode));
          changed |= clearBytes(&prefs.gpio3_mode, sizeof(prefs.gpio3_mode));
          changed |= clearBytes(&prefs.gpio4_mode, sizeof(prefs.gpio4_mode));
          prefs.zen_config_schema = 2;
          changed = true;
          break;
        case 2:
          prefs.timezone_mode = TimezonePolicy::MANUAL;
          prefs.timezone_city = 0;
          prefs.timezone_manual_min = (int16_t)prefs.tz_offset_hours * 60;
          prefs.zen_config_schema = 3;
          changed = true;
          break;
        case 3:
          prefs.child_rooms_enabled = 0;
          prefs.zen_config_schema = 4;
          changed = true;
          break;
        default:
          // A future firmware may encounter an unrecognised older schema. Move
          // it to the current version; active-value validation still runs below.
          prefs.zen_config_schema = CURRENT_SCHEMA;
          changed = true;
          break;
      }
    }
    return changed;
  }

public:
  enum : uint16_t { CURRENT_SCHEMA = 4 };

  static bool migrateMelodySchema(NodePrefs& prefs, uint32_t file_schema) {
    if (file_schema >= 0xC0DE002A) return false;
    prefs.notif_melody_dm = BuiltinMelodies::migrateLegacyGlobal(prefs.notif_melody_dm, false);
    prefs.notif_melody_ch = BuiltinMelodies::migrateLegacyGlobal(prefs.notif_melody_ch, true);
    prefs.notif_melody_ad = BuiltinMelodies::migrateLegacyGlobal(prefs.notif_melody_ad, false);
    for (int i = 0; i < NodePrefs::DM_MELODY_TABLE_MAX; i++)
      prefs.dm_melody[i].slot = BuiltinMelodies::migrateLegacyOverride(prefs.dm_melody[i].slot);
    for (uint8_t i = 0; i < 64; i++) {
      uint64_t mask = 1ULL << i;
      if (prefs.ch_notif_melody_set & mask) {
        uint8_t legacy = (prefs.ch_notif_melody_2 & mask) ? 2 : 1;
        NotificationPreferences::setChannelMelody(
            &prefs, i, BuiltinMelodies::migrateLegacyOverride(legacy));
      }
    }
    prefs.ch_notif_melody_set = 0;
    prefs.ch_notif_melody_2 = 0;
    return true;
  }

  static bool apply(NodePrefs& prefs) {
    bool changed = false;
    if (prefs.timezone_mode > TimezonePolicy::CITY) { prefs.timezone_mode = TimezonePolicy::MANUAL; changed = true; }
    if (prefs.timezone_city >= TimezonePolicy::CITY_COUNT) { prefs.timezone_city = 0; changed = true; }
    if (prefs.timezone_manual_min < -720 || prefs.timezone_manual_min > 840) {
      prefs.timezone_manual_min = 0; changed = true;
    }

    if (prefs.ble_pin != 0 && (prefs.ble_pin < 100000 || prefs.ble_pin > 999999)) {
      prefs.ble_pin = 0;
      changed = true;
    }

    uint8_t old_dm = prefs.notif_melody_dm;
    uint8_t old_ch = prefs.notif_melody_ch;
    uint8_t old_ad = prefs.notif_melody_ad;
    prefs.notif_melody_dm = BuiltinMelodies::validate(old_dm);
    prefs.notif_melody_ch = BuiltinMelodies::validate(old_ch, BuiltinMelodies::KERPLOP);
    prefs.notif_melody_ad = BuiltinMelodies::validate(old_ad);
    changed |= old_dm != prefs.notif_melody_dm || old_ch != prefs.notif_melody_ch ||
               old_ad != prefs.notif_melody_ad;

    if (prefs.ringtone_bpm_idx >= RingtoneModel::BPM_COUNT) { prefs.ringtone_bpm_idx = 2; changed = true; }
    if (prefs.ringtone2_bpm_idx >= RingtoneModel::BPM_COUNT) { prefs.ringtone2_bpm_idx = 2; changed = true; }
    if (prefs.ringtone_len > RingtoneModel::MAX_NOTES) { prefs.ringtone_len = RingtoneModel::MAX_NOTES; changed = true; }
    if (prefs.ringtone2_len > RingtoneModel::MAX_NOTES) { prefs.ringtone2_len = RingtoneModel::MAX_NOTES; changed = true; }
    changed |= clearBytes(prefs.ringtone_notes + RingtoneModel::MAX_NOTES,
                          RingtoneModel::STORAGE_NOTES - RingtoneModel::MAX_NOTES);
    changed |= clearBytes(prefs.ringtone2_notes + RingtoneModel::MAX_NOTES,
                          RingtoneModel::STORAGE_NOTES - RingtoneModel::MAX_NOTES);

    changed |= compactTable(prefs.dm_notif, NodePrefs::DM_NOTIF_TABLE_MAX,
                            &NodePrefs::DmNotifEntry::state, 2);
    changed |= compactTable(prefs.dm_melody, NodePrefs::DM_MELODY_TABLE_MAX,
                            &NodePrefs::DmMelodyEntry::slot, BuiltinMelodies::COUNT);
    for (uint8_t i = 0; i < 64; i++) {
      if (NotificationPreferences::channelMelody(&prefs, i) > BuiltinMelodies::COUNT) {
        NotificationPreferences::setChannelMelody(&prefs, i, 0);
        changed = true;
      }
    }

    uint8_t before_child = prefs.child_mode_enabled;
    uint8_t before_child_ch = prefs.child_channels_enabled;
    uint8_t before_child_rooms = prefs.child_rooms_enabled;
    uint16_t before_child_pages = prefs.child_visible_pages;
    uint8_t before_quiet = prefs.quiet_time_enabled;
    uint16_t before_quiet_start = prefs.quiet_time_start_min;
    uint16_t before_quiet_end = prefs.quiet_time_end_min;
    uint8_t before_bt = prefs.bluetooth_enabled;
    uint8_t before_keyboard = prefs.keyboard_type;
    PrefsDefaults::normalize(prefs);
    changed |= before_child != prefs.child_mode_enabled || before_child_ch != prefs.child_channels_enabled ||
               before_child_rooms != prefs.child_rooms_enabled ||
               before_child_pages != prefs.child_visible_pages || before_quiet != prefs.quiet_time_enabled ||
               before_quiet_start != prefs.quiet_time_start_min || before_quiet_end != prefs.quiet_time_end_min ||
               before_bt != prefs.bluetooth_enabled || before_keyboard != prefs.keyboard_type;

    changed |= applySchemaMigrations(prefs);
    return changed;
  }
};

} // namespace solo
