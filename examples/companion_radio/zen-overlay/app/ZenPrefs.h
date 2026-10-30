#pragma once
#include <cstdint>
#include <type_traits>
#include <stdio.h>
#include <string.h>
#include "../../NodePrefs.h"

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

#define ADVERT_SOUND_SCOPE_ALL        0
#define ADVERT_SOUND_SCOPE_ZERO_HOP   1

struct ZenSettings {  // Active Zen data only; independently staged during loading.
  uint8_t  buzzer_volume;   // 0=min..4=max, default 4
  uint8_t display_brightness;   // 0=min..4=max, default 4 (UI level 5/full)
  uint16_t auto_off_secs;       // display auto-off: 0=never, else seconds (default 15)
  int8_t tz_offset_hours;       // timezone offset from UTC, -12..+14 (default 0)
  uint8_t batt_display_mode;   // 0=icon, 1=percent, 2=voltage
  char custom_msgs[5][140];
  uint64_t ch_notif_override;  // bitmask: bit i = channel i has explicit notification setting [del→onChannelRemoved]
  uint64_t ch_notif_muted;     // bitmask: bit i = channel i muted (only if override bit set) [del→onChannelRemoved]
  uint8_t  dm_show_all;        // 0=favourites only (default), 1=all chat contacts
  uint8_t  room_fav_only;      // 0=all room servers (default), 1=favourites only
  uint8_t  ringtone_bpm_idx;   // index into {60,90,120,150,180}
  uint8_t  ringtone_len;       // active notes (0 = default, editor maximum 16)
  uint8_t  ringtone_notes[32]; // legacy-sized storage; bit7 extends pitch to sharps
  uint16_t home_pages_mask;    // bitmask of visible home pages (bit0=Clock..bit8=Shutdown); 0=all visible
  uint8_t pet_enabled;
  uint8_t  clock_hide_seconds; // 0=show HH:MM:SS/refresh 1s (default), 1=hide/refresh 60s
  uint8_t  clock_12h;          // 0=24h, 1=12h with AM/PM (default)
  uint8_t  buzzer_auto;        // 0=manual (default), 1=auto-mute when BT connected
  struct DmNotifEntry { uint8_t prefix[4]; uint8_t state; }; // state: 0=default,1=muted,2=force-on
  static const int DM_NOTIF_TABLE_MAX = 16;
  DmNotifEntry dm_notif[DM_NOTIF_TABLE_MAX]; // 16*5 = 80 bytes [del→onContactRemoved]
  uint8_t multi_flood_mode; // 0=Off, 1=channel floods, 2=all floods; independent of forwarding.
  uint8_t trace_login_setting; // 0=public fallback allowed, 1=disabled.
  bool tracePublicLogin() const { return trace_login_setting!=1; }
  void setTracePublicLogin(bool enabled) { trace_login_setting=enabled?0:1; }
  uint32_t advert_auto_interval_sec; // periodic 0-hop advert with GPS: 0=off, else seconds
  uint8_t  ringtone2_bpm_idx;
  uint8_t  ringtone2_len;       // active notes (0 = default, editor maximum 16)
  uint8_t  ringtone2_notes[32]; // legacy-sized storage; bit7 extends pitch to sharps
  uint8_t  notif_melody_dm;
  uint8_t  notif_melody_ch;
  uint8_t  notif_melody_ad;
  uint8_t  advert_sound_scope;
  struct DmMelodyEntry { uint8_t prefix[4]; uint8_t slot; }; // 0=global; otherwise selection+1
  static const int DM_MELODY_TABLE_MAX = 16;
  DmMelodyEntry dm_melody[DM_MELODY_TABLE_MAX]; // [del→onContactRemoved]
  uint8_t  use_lemon_font;      // 0=default Adafruit font, 1=Lemon font (Unicode, pixel-accurate wrap)
  uint8_t  display_rotation;    // 0-3; only used on e-ink displays
  uint8_t  page_order[13];
  uint8_t  joystick_rotation;   // 0-3 steps CW; independent of display_rotation
  uint8_t  eink_full_refresh_every; // index into {0,5,10,20,30}: full refresh every N partials (0=off)
  uint8_t  page_order_set;      // 0xA5 = page_order is user-configured; anything else = use default
  static const uint8_t PAGE_ORDER_MAGIC = 0xA5;

  static const uint8_t FAVOURITES_COUNT = 6;
  static const uint8_t FAVOURITES_DIAL_COUNT = 4;
  static const uint8_t FAVOURITE_PREFIX_LEN = 6;
  uint8_t favourite_contacts[FAVOURITES_COUNT][FAVOURITE_PREFIX_LEN]; // [del→onContactRemoved]

  uint64_t ch_fav_bitmask;      // bit i = channel i is marked as favourite [del→onChannelRemoved]
  uint8_t  ch_fav_only;         // 0=show all channels (default), 1=show favourites only

  uint8_t  units_imperial;

  struct UserRadioPreset {
    char name[16];
    float freq;
    float bw;
    uint8_t sf;
    uint8_t cr;
  };
  static const uint8_t USER_RADIO_PRESET_MAX = 4;
  UserRadioPreset user_radio_presets[USER_RADIO_PRESET_MAX];

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

  uint8_t  keyboard_type;

  static const uint8_t KB_ALPHABET_LATIN_ONLY = 0;
  static const uint8_t KB_ALPHABET_CYRILLIC   = 1;
  static const uint8_t KB_ALPHABET_GREEK      = 2;
  static const uint8_t KB_ALPHABET_COUNT      = 3;
  static const char* keyboardAlphabetLabel(uint8_t idx) {
    static const char* L[KB_ALPHABET_COUNT] = { "Latin", "Cyrillic", "Greek" };
    return L[idx < KB_ALPHABET_COUNT ? idx : 0];
  }

  uint8_t  gps_adaptive;

  uint8_t  child_mode_enabled;
  uint32_t child_mode_pin_hash;
  uint16_t child_visible_pages;
  uint8_t  child_channels_enabled;  // show favourited private channels while child mode is locked

  uint8_t  quiet_time_enabled;       // mute notification presentation during the local-time interval
  uint16_t quiet_time_start_min;     // local minute-of-day, default 21:00
  uint16_t quiet_time_end_min;       // local minute-of-day, default 07:00

  uint8_t bluetooth_enabled;  // persisted BLE companion state
  uint8_t channel_melody_overrides[32];
  uint16_t zen_config_schema;
  uint8_t timezone_mode;       // 0=fixed manual offset, 1=capital-city rules
  uint8_t timezone_city;       // TimezonePolicy city index
  int16_t timezone_manual_min; // signed fixed UTC offset, minute resolution
  uint8_t child_rooms_enabled; // allow favourited room servers while Child Mode is locked
  uint8_t notification_screen_wake; // 0=Off, 1=On (mode-dependent), 2=Always
  uint8_t notif_melody_new_contact; // separate alert for a contact added by an advert

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

  static const uint8_t GPS_AVG_COUNT = 4;
  static uint16_t gpsAvgSecs(uint8_t idx) {
    static const uint16_t S[GPS_AVG_COUNT] = { 0, 5, 10, 30 };
    return S[idx < GPS_AVG_COUNT ? idx : 0];
  }
  static const char* gpsAvgLabel(uint8_t idx) {
    static const char* L[GPS_AVG_COUNT] = { "Off", "5s", "10s", "30s" };
    return L[idx < GPS_AVG_COUNT ? idx : 0];
  }

  static const uint8_t TRAIL_AUTOPAUSE_COUNT = 4;
  static uint16_t trailAutoPauseSecs(uint8_t idx) {
    static const uint16_t S[TRAIL_AUTOPAUSE_COUNT] = { 0, 60, 120, 300 };  // off / 1 / 2 / 5 min
    return S[idx < TRAIL_AUTOPAUSE_COUNT ? idx : 0];
  }
  static const char* trailAutoPauseLabel(uint8_t idx) {
    static const char* L[TRAIL_AUTOPAUSE_COUNT] = { "Off", "1m", "2m", "5m" };
    return L[idx < TRAIL_AUTOPAUSE_COUNT ? idx : 0];
  }
  static const uint16_t TRAIL_AUTOPAUSE_MOVE_M = 15;

  static const uint32_t SCHEMA_SENTINEL = 0xC0DE002F;

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

  static const uint8_t PAGE_ORDER_LEN = 13;
  static const uint8_t PAGE_ORDER_LEN_V1 = 11;

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
  static const uint16_t HP_ALL        = HP_CLOCK | HP_RECENT | HP_RADIO |
                                        HP_BLUETOOTH | HP_ADVERT | HP_GPS |
                                        HP_TOOLS | HP_FAVOURITES | HP_SENSORS;
  static const uint16_t HP_DEFAULT    = HP_ALL;

  static const char* homePageLabel(uint8_t bit) {
    static const char* labels[HPB_COUNT] = {
      "Clock", "Recent", "Radio", "Bluetooth", "Advert",
      "GPS", "Sensors", "Tools", "", "Settings", "Messages", "Favourites", ""
    };
    return (bit < HPB_COUNT) ? labels[bit] : "";
  }

  ZenSettings() { memset(this,0,sizeof(*this)); }
};

struct ZenPrefs : public NodePrefs, public ZenSettings {
  ZenPrefs() : NodePrefs(), ZenSettings() {}
  ZenSettings& zenSettings() { return *this; }
  const ZenSettings& zenSettings() const { return *this; }
  void resetZen() { memset(static_cast<ZenSettings*>(this),0,sizeof(ZenSettings)); }
};
static_assert(sizeof(ZenSettings)<=1536,"Active settings exceed the boot staging budget");
static_assert(std::is_standard_layout<ZenSettings>::value,"Settings field offsets require a standard layout");
static_assert(std::is_trivially_copyable<ZenSettings>::value,"Boot settings staging must be a plain value copy");
