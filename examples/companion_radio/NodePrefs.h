#pragma once
#include <cstdint>
#include <stdio.h>

// Firmware's own boot-default radio params.
#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

#define ADVERT_SOUND_SCOPE_ALL        0
#define ADVERT_SOUND_SCOPE_ZERO_HOP   1

struct NodePrefs {  // persisted to file
  float airtime_factor;
  char node_name[32];
  float freq;
  uint8_t sf;
  uint8_t cr;
  uint8_t multi_acks;
  uint8_t manual_add_contacts;
  float bw;
  int8_t tx_power_dbm;
  uint8_t telemetry_mode_base;
  uint8_t telemetry_mode_loc;
  uint8_t telemetry_mode_env;
  float rx_delay_base;
  uint32_t ble_pin;
  uint8_t  advert_loc_policy;
  uint8_t  buzzer_quiet;
  uint8_t  buzzer_volume;   // 0=min..4=max, default 4
  uint8_t  gps_enabled;      // GPS enabled flag (0=disabled, 1=enabled)
  uint32_t gps_interval;     // GPS read interval in seconds
  uint8_t autoadd_config;    // bitmask for auto-add contacts config
  uint8_t rx_boosted_gain; // SX126x RX boosted gain mode (0=power saving, 1=boosted)
  uint8_t client_repeat;
  uint8_t path_hash_mode;    // which path mode to use when sending
  uint8_t autoadd_max_hops;  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
  char default_scope_name[31];
  uint8_t default_scope_key[16];
  uint8_t display_brightness;   // 0=min..4=max, default 4 (UI level 5/full)
  uint16_t auto_off_secs;       // display auto-off: 0=never, else seconds (default 15)
  int8_t tz_offset_hours;       // timezone offset from UTC, -12..+14 (default 0)
  uint16_t low_batt_mv;         // reserved legacy field; shutdown uses BatteryPolicy
  uint8_t batt_display_mode;   // 0=icon, 1=percent, 2=voltage
  // The first five entries are editable Quick Replies. Keep all ten records so
  // existing preference files retain their binary layout and later fields do
  // not shift; slots 5..9 are reserved legacy storage.
  char custom_msgs[10][140];
  uint64_t ch_notif_override;  // bitmask: bit i = channel i has explicit notification setting [del→onChannelRemoved]
  uint64_t ch_notif_muted;     // bitmask: bit i = channel i muted (only if override bit set) [del→onChannelRemoved]
  uint8_t  dm_show_all;        // 0=favourites only (default), 1=all chat contacts
  uint8_t  room_fav_only;      // 0=all room servers (default), 1=favourites only
  uint8_t  ringtone_bpm_idx;   // index into {60,90,120,150,180}
  uint8_t  ringtone_len;       // active notes (0 = default, editor maximum 16)
  uint8_t  ringtone_notes[32]; // legacy-sized storage; bit7 extends pitch to sharps
  uint16_t home_pages_mask;    // bitmask of visible home pages (bit0=Clock..bit8=Shutdown); 0=all visible
  uint8_t  bot_enabled;         // 0=disabled, 1=DM trigger-reply active — DM only; channel/room have their own bot_channel_enabled/bot_room_enabled and don't depend on this
  uint8_t  bot_channel_enabled; // 0=disabled, 1=channel bot active for bot_channel_idx
  uint8_t  bot_channel_idx;     // channel index for channel bot [del→onChannelRemoved]
  char     bot_trigger[64];     // DM trigger phrase (case-insensitive contains; "*" = any DM)
  char     bot_reply_dm[140];   // auto-reply text for DM
  char     bot_reply_ch[140];   // auto-reply text for channel
  char     bot_trigger_ch[64];  // channel trigger phrase (independent of DM; "*" = any channel msg)
  uint8_t  bot_commands_enabled; // 0=off, 1=answer !ping/!batt/!loc/!time/!help DM commands — DM only, see bot_commands_ch/bot_commands_room below for the other two targets
  uint8_t  bot_quiet_start;     // quiet-hours start hour, local 0-23 (start==end → disabled)
  uint8_t  bot_quiet_end;       // quiet-hours end hour, local 0-23
  uint8_t  clock_hide_seconds; // 0=show HH:MM:SS/refresh 1s (default), 1=hide/refresh 60s
  uint8_t  clock_12h;          // 0=24h, 1=12h with AM/PM (default)
  uint8_t  buzzer_auto;        // 0=manual (default), 1=auto-mute when BT connected
  struct DmNotifEntry { uint8_t prefix[4]; uint8_t state; }; // state: 0=default,1=muted,2=force-on
  static const int DM_NOTIF_TABLE_MAX = 16;
  DmNotifEntry dm_notif[DM_NOTIF_TABLE_MAX]; // 16*5 = 80 bytes [del→onContactRemoved]
  // Reserved former clock-dashboard field selections. Keep these bytes in the
  // serialized layout so existing preference files remain aligned.
  uint8_t  reserved_dashboard_fields[3];
  uint32_t advert_auto_interval_sec; // periodic 0-hop advert with GPS: 0=off, else seconds
  // Second melody slot (same packing as ringtone_*)
  uint8_t  ringtone2_bpm_idx;
  uint8_t  ringtone2_len;       // active notes (0 = default, editor maximum 16)
  uint8_t  ringtone2_notes[32]; // legacy-sized storage; bit7 extends pitch to sharps
  // Global selection IDs from solo/BuiltinMelodies.h.
  uint8_t  notif_melody_dm;
  uint8_t  notif_melody_ch;
  uint8_t  notif_melody_ad;
  // Advert sound filter: 0=all adverts, 1=zero-hop only
  uint8_t  advert_sound_scope;
  // Legacy per-channel Custom1/Custom2 override masks, retained for migration.
  uint64_t ch_notif_melody_set;  // bit i = channel i has explicit melody [del→onChannelRemoved]
  uint64_t ch_notif_melody_2;    // bit i = use melody 2 (else melody 1, when set bit is set)
  // Per-DM melody table
  struct DmMelodyEntry { uint8_t prefix[4]; uint8_t slot; }; // 0=global; otherwise selection+1
  static const int DM_MELODY_TABLE_MAX = 16;
  DmMelodyEntry dm_melody[DM_MELODY_TABLE_MAX]; // [del→onContactRemoved]
  uint8_t  use_lemon_font;      // 0=default Adafruit font, 1=Lemon font (Unicode, pixel-accurate wrap)
  uint8_t  display_rotation;    // 0-3; only used on e-ink displays
  // Home screen page order: each byte = HomePageBit + 1. 0 terminates the list.
  // Validity gated by page_order_set magic (see below) — not by entry value range,
  // so a junk byte in 1..HPB_COUNT cannot trigger custom-order mode.
  // Declared as a literal (not HPB_COUNT) so the field offset stays stable across
  // builds that add HomePageBit entries. The first PAGE_ORDER_LEN_V1 bytes persist
  // at this offset (backward-compatible); the remaining slots live at the file
  // tail (see DataStore) so pre-0x0019 saves still load without shifting.
  uint8_t  page_order[13];
  uint8_t  joystick_rotation;   // 0-3 steps CW; independent of display_rotation
  uint8_t  eink_full_refresh_every; // index into {0,5,10,20,30}: full refresh every N partials (0=off)
  uint8_t  page_order_set;      // 0xA5 = page_order is user-configured; anything else = use default
  static const uint8_t PAGE_ORDER_MAGIC = 0xA5;

  // Favourites dial: 6 pinned contacts, stored as first 6 bytes of pub_key per slot.
  // All-zero = empty slot (probability a real pub_key starts with 6 zero bytes is 2^-48).
  // Layout transposes between landscape (3×2) and portrait (2×3).
  static const uint8_t FAVOURITES_COUNT = 6;
  // Keep six serialized slots for preference compatibility, but the focused
  // Wio UI exposes four full-width dial entries.
  static const uint8_t FAVOURITES_DIAL_COUNT = 4;
  static const uint8_t FAVOURITE_PREFIX_LEN = 6;
  uint8_t favourite_contacts[FAVOURITES_COUNT][FAVOURITE_PREFIX_LEN]; // [del→onContactRemoved]

  // GPS trail cadence. Logging on/off is a runtime state (Tools › Trail),
  // not a persisted preference.
  uint8_t trail_interval_idx;   // reserved — sampling cadence is now fixed at TrailStore::SAMPLING_SECS
  uint8_t trail_min_delta_idx;  // min-distance gate level (0=finest..3); metres or feet per units_imperial
  uint8_t trail_units_idx;      // legacy: old combined speed/pace+unit index (km/h, mph, min/km, min/mi)

  uint64_t ch_fav_bitmask;      // bit i = channel i is marked as favourite [del→onChannelRemoved]
  uint8_t  ch_fav_only;         // 0=show all channels (default), 1=show favourites only

  // Global measurement system for every distance/speed shown in the UI
  // (Nearby, Trail, navigate-to-point). 0=metric (default), 1=imperial.
  uint8_t  units_imperial;
  // Trail Summary readout: 0=speed (km/h or mph), 1=pace (min/km or min/mi).
  // The km-vs-mi choice now comes from units_imperial, so this is just the mode.
  uint8_t  trail_show_pace;
  // Reserved former RX duty-cycle control. Base MeshCore companion and repeater
  // operation use continuous receive; retain this byte only so existing Solo
  // preference files remain aligned.
  uint8_t  reserved_rx_powersave;
  // Reserved former adaptive-power byte. Retained only so existing Zen
  // preferences keep the same on-flash layout.
  uint8_t  reserved_radio_power;

  // Retained only to preserve the existing preferences-file layout. On-device
  // On-device delivery uses the fixed policy in solo/NodeRouteRetry.h.
  uint8_t  reserved_dm_resend_count;

  // User-saved radio presets, written by the "Save current..." entry in the
  // shared preset picker (Settings > Radio and Tools > Repeater both populate
  // these same slots). name[0] == '\0' marks an empty slot.
  struct UserRadioPreset {
    char name[16];
    float freq;
    float bw;
    uint8_t sf;
    uint8_t cr;
  };
  static const uint8_t USER_RADIO_PRESET_MAX = 4;
  UserRadioPreset user_radio_presets[USER_RADIO_PRESET_MAX];

  // Reserved former repeater controls. Keep their serialized positions so
  // existing preference files remain aligned; runtime behaviour is fixed.
  uint8_t  reserved_repeat_skip_adverts;
  uint8_t  reserved_repeat_max_hops;
  uint8_t  reserved_repeat_delay_boost;
  int8_t   reserved_repeat_min_snr;
  uint8_t  reserved_repeat_suppress_dup;

  // Reserved former dedicated-repeater profile fields. They remain serialized
  // to preserve the existing MeshCore preference layout, but are ignored:
  // repeater mode always shares freq/bw/sf/cr above.
  uint8_t  repeater_use_profile;
  float    repeater_freq;
  float    repeater_bw;
  uint8_t  repeater_sf;
  uint8_t  repeater_cr;

  // Track positions shared by other nodes via [LOC] messages (LiveTrackStore).
  // 0 = ignore shared positions (default), 1 = parse incoming DM/channel [LOC]
  // shares and update the live-track table (Nearby "Live" view / map).
  uint8_t  track_shared_loc;

  // Live location sharing — a message-based "beacon". When enabled, the device
  // periodically sends a [LOC] message to the chosen target while it moves.
  // Configured from the Map (Trail screen) "Live share" menu.
  uint8_t  loc_share_enabled;       // 0=off (default), 1=auto-sharing on
  uint8_t  loc_share_target_type;   // 0=channel, 1=DM contact
  uint8_t  loc_share_channel_idx;   // target channel index (when target_type==0) [del→onChannelRemoved]
  uint8_t  loc_share_dm_prefix[6];  // target contact pubkey prefix (when target_type==1) [del→onContactRemoved]
  uint8_t  loc_share_move_idx;      // movement gate level (index into locShareMoveMeters)
  uint8_t  loc_share_interval_idx;  // min send interval (index into locShareIntervalSecs)
  uint8_t  loc_share_heartbeat_idx; // stationary heartbeat (index into locShareHeartbeatSecs)

  // Locator — a single geofence around a saved point. When enabled the device
  // watches its own GPS fix and beeps + shows an alert when it crosses into
  // (arrive) or out of (leave) the radius. The target coordinate/label is a
  // snapshot of a chosen waypoint, so the alert survives the waypoint being
  // edited or deleted. Configured from Tools › Locator.
  uint8_t  locator_enabled;      // 0=off (default), 1=armed
  uint8_t  locator_has_target;   // 0=no target chosen yet, 1=target set
  uint8_t  locator_radius_idx;   // index into locatorRadiusMeters
  uint8_t  locator_mode;         // 0=arrive, 1=leave, 2=both
  int32_t  locator_lat_1e6;      // target latitude  (1e6-scaled; last-known for a contact)
  int32_t  locator_lon_1e6;      // target longitude (1e6-scaled; last-known for a contact)
  char     locator_label[12];    // target name for the alert text (WAYPOINT_LABEL_LEN)
  // Target can be a static waypoint or a live contact: for a contact the engine
  // re-reads the latest [LOC] position each evaluation (keyed by pubkey prefix),
  // so the geofence follows a moving person ("alert when my friend is near").
  uint8_t  locator_target_kind;  // 0=waypoint (static), 1=live contact
  uint8_t  locator_key[6];       // contact pubkey prefix when target_kind==1 [del→onContactRemoved]

  // Trail auto-pause — when tracking, automatically freeze the trail (timer +
  // sampling) after the device has sat still for this long, and resume on the
  // next real movement. 0 = off. Index into trailAutoPauseSecs.
  uint8_t  trail_autopause_idx;

  // Locator proximity beeper — when on (and the alert is armed with a target),
  // the device ticks while inside the radius and shortens the gap between ticks
  // the closer it gets to the target, like a homing beeper. Independent of the
  // discrete arrive/leave alert (locator_mode).
  uint8_t  locator_beeper;       // 0=off (default), 1=on

  // GPS averaging for waypoint marking — when set, "Mark here" samples the GPS
  // fix for this many seconds and stores the mean position, for a more accurate
  // mark than a single instantaneous fix. 0 = off (instant mark, the default).
  // Index into gpsAvgSecs(). [Tools › Trail › Settings › Mark avg]
  uint8_t  gps_avg_idx;

  // Alarm clock — a wake alarm, configured from the Clock page (Enter › Alarm).
  // Stored as a local time-of-day; the actual fire instant is (re)computed as an
  // absolute time in tickBackground(), which is what makes it robust to RTC
  // re-syncs: the mesh (every inbound packet), the companion app, GPS and the
  // CLI can all jump getCurrentTime() at any moment, but an absolute target
  // instant stays correct across small corrections and still fires (late) if
  // the clock jumps over it. With alarm_repeat_mask == 0 it's one-shot: fires
  // once, then alarm_on clears. A non-zero mask instead re-arms it for the next
  // matching weekday (see UITask::computeAlarmNextFire/evaluateAlarm) and
  // alarm_on stays set. The minutnik (countdown) and stoper (stopwatch) are
  // runtime-only and not persisted.
  uint8_t  alarm_on;    // 0=off (default), 1=armed
  uint8_t  alarm_hour;  // 0-23, local time
  uint8_t  alarm_min;   // 0-59
  // Repeat days as a bitmask, bit i = 1<<tm_wday (Sunday=0 .. Saturday=6, per
  // struct tm — matches what computeAlarmNextFire() already reads from
  // gmtime()). 0 = no repeat (one-shot, the original/default behaviour).
  // Cycled through the presets below via the alarm's Repeat row.
  static const uint8_t ALARM_REPEAT_NONE     = 0x00;  // one-shot (default)
  static const uint8_t ALARM_REPEAT_DAILY    = 0x7F;  // every day
  static const uint8_t ALARM_REPEAT_WEEKDAYS = 0x3E;  // Mon-Fri
  static const uint8_t ALARM_REPEAT_WEEKENDS = 0x41;  // Sat+Sun
  static const uint8_t ALARM_REPEAT_COUNT    = 4;
  static uint8_t alarmRepeatMaskForIdx(uint8_t idx) {
    static const uint8_t M[ALARM_REPEAT_COUNT] =
      { ALARM_REPEAT_NONE, ALARM_REPEAT_DAILY, ALARM_REPEAT_WEEKDAYS, ALARM_REPEAT_WEEKENDS };
    return M[idx < ALARM_REPEAT_COUNT ? idx : 0];
  }
  // Reverse lookup for display: an arbitrary mask (e.g. loaded from a future
  // custom-day picker) that doesn't match a preset just reads as "Off" here —
  // it still fires correctly, computeAlarmNextFire() reads the raw mask.
  static uint8_t alarmRepeatIdxForMask(uint8_t mask) {
    switch (mask) {
      case ALARM_REPEAT_DAILY:    return 1;
      case ALARM_REPEAT_WEEKDAYS: return 2;
      case ALARM_REPEAT_WEEKENDS: return 3;
      default:                    return 0;
    }
  }
  static const char* alarmRepeatLabel(uint8_t idx) {
    static const char* L[ALARM_REPEAT_COUNT] = { "OFF", "Daily", "Weekdays", "Weekends" };
    return L[idx < ALARM_REPEAT_COUNT ? idx : 0];
  }

  // On-screen keyboard layout, shared across every text-entry screen (Settings >
  // Keyboard). 0=ABC grid, alphabetical order, 1=T9 predictive (default when
  // using the on-screen keyboard). CardKB always supplies direct QWERTY input.
  uint8_t  keyboard_type;

  // Historical script values retained for preference-file migration. Solo's
  // keyboard now ignores both script bytes and always uses Latin/EN-US.
  static const uint8_t KB_ALPHABET_LATIN_ONLY = 0;
  static const uint8_t KB_ALPHABET_CYRILLIC   = 1;
  static const uint8_t KB_ALPHABET_GREEK      = 2;
  static const uint8_t KB_ALPHABET_COUNT      = 3;
  static const char* keyboardAlphabetLabel(uint8_t idx) {
    static const char* L[KB_ALPHABET_COUNT] = { "Latin", "Cyrillic", "Greek" };
    return L[idx < KB_ALPHABET_COUNT ? idx : 0];
  }

  // Auto-save the live GPS trail to /trail on shutdown (covers the low-battery
  // auto-shutdown, which otherwise loses the whole route). Only writes when the
  // trail has points and the toggle is on. 0 = off (default), 1 = on.
  // [Tools › Trail › Settings › Auto-save]
  uint8_t  trail_autosave_lowbatt;

  // Appended at the tail (see the alarm doc comment above for the field itself
  // and the serialization tripwire below for why new fields land here, not
  // inline with alarm_on/hour/min).
  uint8_t  alarm_repeat_mask;

  // Reserved former additional-script selection. Retained for preference-file
  // compatibility; the Solo keyboard is now always Latin/EN-US.
  uint8_t  keyboard_alt_alphabet;

  // Bot DM allow-list: who is allowed to trigger a DM auto-reply or run a
  // command. 0 = all (default, matches the original bot_enabled behaviour).
  // 1 = favourites only — gate on ContactInfo::flags bit 0, the same
  // "favourite"/starred bit the Messages screen's DM list filter (dm_show_all)
  // already reads, so no separate allow-list storage is needed.
  uint8_t  bot_dm_scope;

  // Room-server auto-reply bot — same trigger/reply/command shape as the DM
  // and channel bots above, but targets a single room server (like the
  // channel bot targets a single channel) since posting requires that room's
  // own login session (see MyMesh::sendNodeLogin/logoutRoom). If the device
  // has never logged into this room, replies silently fail to send — same
  // as a manual post would — so log in at least once from Messages > Rooms
  // before relying on the bot there.
  uint8_t  bot_room_enabled;                          // 0=disabled, 1=room bot active for bot_room_prefix
  uint8_t  bot_room_prefix[6];                        // target room's pubkey prefix [del→onContactRemoved]
  char     bot_trigger_room[64];                      // room trigger phrase (independent of DM/channel; "*" = any post)
  char     bot_reply_room[140];                       // auto-reply text for the room

  // Per-target Commands toggle for channel/room, splitting what used to be
  // one bot_commands_enabled shared across all three (see that field's doc
  // comment) — e.g. answer !ping in DM but stay quiet on a public channel.
  // Upgraders: DataStore seeds both from the old shared bot_commands_enabled
  // on first load past the schema bump, so existing behaviour is preserved
  // until the user deliberately splits them apart.
  uint8_t  bot_commands_ch;
  uint8_t  bot_commands_room;

  // Reserved former main-script selection. Retained for preference-file
  // compatibility; the Solo keyboard is now always Latin/EN-US.
  uint8_t  keyboard_main_alphabet;

  // Per-target toggle for bot *action* commands (!buzz/!gps/!advert) --
  // separate from bot_commands_ch/bot_commands_room above, which only gate
  // the read-only query commands (!ping/!batt/...). Nested under that
  // Commands toggle (Commands=off means neither queries nor actions run for
  // that target); Actions=on additionally lets the state-changing commands
  // through. Default 0 (off) -- these change device behaviour remotely, so
  // upgraders don't get them silently enabled.
  uint8_t  bot_actions_dm;
  uint8_t  bot_actions_ch;
  uint8_t  bot_actions_room;

  // User-assignable GPIO pins (board-specific, see PIN_GPIO1..4 in the
  // variant header -- currently Wio Tracker L1 only). One byte per pin packs
  // direction + output level (+ analog, gpio1/2 only) into one field:
  // 0=Off 1=Input 2=Output(low) 3=Output(high) 4=Analog. Mode 4 is only
  // meaningful for gpio1/gpio2 (the nRF52840's AIN0/AIN5 -- gpio3/gpio4 have
  // no ADC channel), so those two fields are clamped to 0-3 on load, not 0-4
  // (see DataStore::loadPrefsInt). Default 0 (off/disconnected) on upgrade.
  uint8_t  gpio1_mode;
  uint8_t  gpio2_mode;
  uint8_t  gpio3_mode;
  uint8_t  gpio4_mode;

  // Reuses the retired CardKB display-toggle byte so the established preference
  // layout remains unchanged. Schema migration clears its old meaning before
  // it is interpreted as the Adaptive GPS-mode selector.
  uint8_t  gps_adaptive;

  // Parent-controlled child UI. The PIN stores a small non-cryptographic hash:
  // this is a practical on-device UI lock, not protection against reflashing.
  // child_visible_pages uses HomePageBit positions. Favourites defaults on;
  // Recent, Map, Sensors and Shutdown default off (seeded in MyMesh defaults).
  uint8_t  child_mode_enabled;
  uint32_t child_mode_pin_hash;
  uint16_t child_visible_pages;
  uint8_t  child_channels_enabled;  // show favourited private channels while child mode is locked

  uint8_t  quiet_time_enabled;       // mute notification presentation during the local-time interval
  uint16_t quiet_time_start_min;     // local minute-of-day, default 21:00
  uint16_t quiet_time_end_min;       // local minute-of-day, default 07:00

  // Reserved former configurable repeater timing fields. Serialized positions
  // are retained for compatibility; runtime timing is fixed in RepeaterTiming.
  float reserved_repeat_rx_delay_base;
  float reserved_repeat_flood_tx_factor;
  float reserved_repeat_direct_tx_factor;
  uint8_t bluetooth_enabled;  // persisted BLE state; USB remains independently available
  // Two four-bit selection overrides per byte. 0=Global; otherwise the
  // BuiltinMelodies selection ID + 1. Appended to preserve the old layout.
  uint8_t channel_melody_overrides[32];
  // Zen-owned semantic configuration schema. Unlike the file-layout sentinel,
  // this tracks ordered value migrations at boot.
  uint16_t zen_config_schema;
  uint8_t timezone_mode;       // 0=fixed manual offset, 1=capital-city rules
  uint8_t timezone_city;       // TimezonePolicy city index
  int16_t timezone_manual_min; // signed fixed UTC offset, minute resolution
  uint8_t child_rooms_enabled; // allow favourited room servers while Child Mode is locked
  uint8_t notification_screen_wake; // 0=Off, 1=On (mode-dependent), 2=Always
  uint8_t notif_melody_new_contact; // separate alert for a contact added by an advert

  // Single source of truth for the live-share option tables (shared by the Map
  // UI labels and the auto-send engine in UITask).
  static const uint8_t LOC_SHARE_MOVE_COUNT = 4;
  static uint16_t locShareMoveMeters(uint8_t idx) {
    static const uint16_t M[LOC_SHARE_MOVE_COUNT] = { 50, 100, 250, 500 };
    return M[idx < LOC_SHARE_MOVE_COUNT ? idx : 1];
  }
  static const uint8_t LOC_SHARE_INTERVAL_COUNT = 4;
  static uint16_t locShareIntervalSecs(uint8_t idx) {
    static const uint16_t S[LOC_SHARE_INTERVAL_COUNT] = { 30, 60, 120, 300 };
    return S[idx < LOC_SHARE_INTERVAL_COUNT ? idx : 1];
  }
  static const uint8_t LOC_SHARE_HEARTBEAT_COUNT = 3;
  static uint16_t locShareHeartbeatSecs(uint8_t idx) {
    static const uint16_t H[LOC_SHARE_HEARTBEAT_COUNT] = { 0, 300, 900 };  // off / 5 min / 15 min
    return H[idx < LOC_SHARE_HEARTBEAT_COUNT ? idx : 0];
  }

  // Locator option tables (shared by the Tools › Locator UI labels and the
  // evaluation engine in UITask).
  static const uint8_t LOCATOR_RADIUS_COUNT = 5;
  static uint16_t locatorRadiusMeters(uint8_t idx) {
    static const uint16_t R[LOCATOR_RADIUS_COUNT] = { 50, 100, 250, 500, 1000 };
    return R[idx < LOCATOR_RADIUS_COUNT ? idx : 1];
  }
  static const uint8_t LOCATOR_MODE_COUNT = 3;  // 0=arrive, 1=leave, 2=both
  static const char* locatorModeLabel(uint8_t m) {
    static const char* L[LOCATOR_MODE_COUNT] = { "Arrive", "Leave", "Both" };
    return L[m < LOCATOR_MODE_COUNT ? m : 0];
  }

  // GPS-averaging durations for waypoint marking (seconds). 0 = off (instant).
  static const uint8_t GPS_AVG_COUNT = 4;
  static uint16_t gpsAvgSecs(uint8_t idx) {
    static const uint16_t S[GPS_AVG_COUNT] = { 0, 5, 10, 30 };
    return S[idx < GPS_AVG_COUNT ? idx : 0];
  }
  static const char* gpsAvgLabel(uint8_t idx) {
    static const char* L[GPS_AVG_COUNT] = { "Off", "5s", "10s", "30s" };
    return L[idx < GPS_AVG_COUNT ? idx : 0];
  }

  // Trail auto-pause delays (seconds). 0 = off.
  static const uint8_t TRAIL_AUTOPAUSE_COUNT = 4;
  static uint16_t trailAutoPauseSecs(uint8_t idx) {
    static const uint16_t S[TRAIL_AUTOPAUSE_COUNT] = { 0, 60, 120, 300 };  // off / 1 / 2 / 5 min
    return S[idx < TRAIL_AUTOPAUSE_COUNT ? idx : 0];
  }
  static const char* trailAutoPauseLabel(uint8_t idx) {
    static const char* L[TRAIL_AUTOPAUSE_COUNT] = { "Off", "1m", "2m", "5m" };
    return L[idx < TRAIL_AUTOPAUSE_COUNT ? idx : 0];
  }
  // Movement under this many metres counts as "stationary" for auto-pause.
  // Deliberately coarser than the trail min-delta gate (and independent of it)
  // so GPS jitter while parked doesn't keep resetting the idle timer. Engine
  // tuning only — not persisted.
  static const uint16_t TRAIL_AUTOPAUSE_MOVE_M = 15;

  // Tail sentinel written at the end of /new_prefs. Bump the low byte when
  // adding/removing/reordering fields in DataStore::savePrefs/loadPrefsInt so
  // older saves are detected on load and skipped (zero-init defaults kept).
  // High 24 bits identify the file format; low byte is the schema revision.
  static const uint32_t SCHEMA_SENTINEL = 0xC0DE002F;

  // Bit-index for each home page. Used by page_order (entries store bit+1) and
  // by home_pages_mask. Single source of truth — both HomeScreen::pageBit/bitToPage
  // and SettingsScreen::homePageBitIndex route their per-enum lookups through these.
  enum HomePageBit : uint8_t {
    HPB_CLOCK      = 0,
    HPB_RECENT     = 1,
    HPB_RADIO      = 2,
    HPB_BLUETOOTH  = 3,
    HPB_ADVERT     = 4,
    HPB_GPS        = 5,
    HPB_SENSORS    = 6,
    HPB_TOOLS      = 7,
    HPB_SHUTDOWN   = 8,
    HPB_SETTINGS   = 9,
    HPB_QUICK_MSG  = 10,
    HPB_FAVOURITES = 11,
    HPB_MAP        = 12,
    HPB_COUNT      = 13,
  };

  // Number of usable page_order[] slots — one per home page (== HPB_COUNT), so
  // every page can be reordered. Any page still missing from a saved order is
  // appended at navigation time via buildVisibleOrder's fallback.
  static const uint8_t PAGE_ORDER_LEN = 13;
  // Bytes of page_order persisted at the original file offset. Slots beyond this
  // (PAGE_ORDER_LEN - PAGE_ORDER_LEN_V1) are stored at the file tail, so a
  // pre-0x0019 save — whose order was exactly this long — loads without shifting
  // every field written after page_order.
  static const uint8_t PAGE_ORDER_LEN_V1 = 11;

  // Bitmasks for home_pages_mask (bit=1 → page visible; 0 field = all visible).
  // SETTINGS and QUICK_MSG have no mask bit — they're always visible.
  static const uint16_t HP_CLOCK      = 1 << HPB_CLOCK;
  static const uint16_t HP_RECENT     = 1 << HPB_RECENT;
  static const uint16_t HP_RADIO      = 1 << HPB_RADIO;
  static const uint16_t HP_BLUETOOTH  = 1 << HPB_BLUETOOTH;
  static const uint16_t HP_ADVERT     = 1 << HPB_ADVERT;
  static const uint16_t HP_GPS        = 1 << HPB_GPS;
  static const uint16_t HP_SENSORS    = 1 << HPB_SENSORS;
  static const uint16_t HP_TOOLS      = 1 << HPB_TOOLS;
  static const uint16_t HP_SHUTDOWN   = 1 << HPB_SHUTDOWN;
  static const uint16_t HP_FAVOURITES = 1 << HPB_FAVOURITES;
  static const uint16_t HP_MAP        = 1 << HPB_MAP;
  // Retired page bits remain reserved above so MeshCore preference layouts and
  // existing page orders retain their numeric meaning. They are not included
  // in active masks or exposed as labels.
  static const uint16_t HP_ALL        = HP_CLOCK | HP_RECENT | HP_RADIO |
                                        HP_BLUETOOTH | HP_ADVERT | HP_GPS |
                                        HP_TOOLS | HP_FAVOURITES | HP_SENSORS;
  // Messages and Settings have no mask bit and are always visible. Fresh
  // installs expose every available page; users can hide individual pages.
  static const uint16_t HP_DEFAULT    = HP_ALL;

  // Label for home page by bit-index; returns "" for out-of-range.
  // Array indices match HomePageBit values.
  static const char* homePageLabel(uint8_t bit) {
    static const char* labels[HPB_COUNT] = {
      "Clock", "Recent", "Radio", "Bluetooth", "Advert",
      "GPS", "Sensors", "Tools", "", "Settings", "Messages", "Favourites", ""
    };
    return (bit < HPB_COUNT) ? labels[bit] : "";
  }

};

// ── Serialization tripwire ───────────────────────────────────────────────────
// NodePrefs is written/read field-by-field, in order, by DataStore::savePrefs()
// and loadPrefsInt(); the on-disk format IS the struct's field layout. There is
// no automatic check that those two hand-written sequences match the struct, so
// a forgotten read/write silently misaligns EVERY field after it.
//
// This assert is the manual checkpoint. Changing a data member changes sizeof
// and trips it. When it trips, do ALL of the following, then update the number:
//   1. add the field's rd(...)   in DataStore::loadPrefsInt(), in struct order
//   2. add the field's write(...) in DataStore::savePrefs(),   in struct order
//   3. clamp it on load (an upgrader's file lacks it → stray bytes)
//   4. bump SCHEMA_SENTINEL's low byte
// (Padding can also shift sizeof; a "false" trip just means re-check + rebump.)
// gps_adaptive reuses keyboard_cardkb_compact (0xC0DE0023) in existing tail padding --
// confirmed via a real build -- leaving sizeof unchanged at 2720.
// Child Mode fields through child_channels_enabled (0xC0DE0024/25) grow the
// struct by 16 bytes including alignment padding, confirmed via a real build.
// Quiet Time (0xC0DE0026) fits into the resulting tail padding, confirmed via
// a real build, so the combined layout remains the same size.
// Repeater timing (0xC0DE0027/28) adds three floats at the persisted tail and
// grows the struct to 2752 bytes including alignment padding, confirmed by the
// Wio Tracker build.
// bluetooth_enabled (0xC0DE0029) is appended after those floats; older records
// default to enabled and retain their existing sentinel alignment.
// channel_melody_overrides (0xC0DE002A) adds 32 bytes at the persisted tail.
// zen_config_schema (0xC0DE002B) consumes tail padding; size remains unchanged.
// timezone mode, city and manual minutes (0xC0DE002C) are appended together;
// alignment grows the Wio Tracker layout to 2792 bytes. child_rooms_enabled
// (0xC0DE002D) consumes existing tail padding, so the size remains unchanged.
// notification_screen_wake (0xC0DE002E) follows child_rooms_enabled and
// consumes existing tail padding; older records default to enabled on load.
// notif_melody_new_contact (0xC0DE002F) follows screen wake and defaults to
// None for older records. It uses existing tail padding; the Wio Tracker
// layout remains 2792 bytes on both display builds.
// keyboard_main_alphabet (added in an earlier bump) landed in existing tail
// padding -- confirmed via a real build's sizeof() -- so that bump left the
// size unchanged. bot_actions_dm/ch/room and gpio1..4_mode (the last two
// bumps, 7 more uint8_t total) added 8 bytes, not 7 -- one byte of tail
// padding got consumed along the way. 2720 confirmed via a real
// WioTrackerL1_Zen_E-INK build.
static_assert(sizeof(NodePrefs) == 2792,
              "NodePrefs layout changed — sync DataStore save/load + clamp, bump "
              "SCHEMA_SENTINEL, then update this size (see steps above).");
