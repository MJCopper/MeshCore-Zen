#pragma once
#include "../ZenPrefs.h"
#include <stddef.h>
#include <stdint.h>
namespace zen {
// Stable tags and frozen nRF52 v1/v2 tail offsets. Do not renumber.
// Wire widths are frozen; a width change requires a format migration.
struct SettingsField { uint16_t offset,size,legacy; uint8_t tag; };
static constexpr SettingsField SETTINGS_FIELDS[] = {
  {uint16_t(offsetof(ZenSettings,buzzer_volume)),1,0,32},
  {uint16_t(offsetof(ZenSettings,display_brightness)),1,1,33},
  {uint16_t(offsetof(ZenSettings,auto_off_secs)),2,2,34},
  {uint16_t(offsetof(ZenSettings,tz_offset_hours)),1,4,35},
  {uint16_t(offsetof(ZenSettings,batt_display_mode)),1,8,36},
  {uint16_t(offsetof(ZenSettings,custom_msgs)),700,9,37},
  {uint16_t(offsetof(ZenSettings,ch_notif_override)),8,1416,38},
  {uint16_t(offsetof(ZenSettings,ch_notif_muted)),8,1424,39},
  {uint16_t(offsetof(ZenSettings,dm_show_all)),1,1432,40},
  {uint16_t(offsetof(ZenSettings,room_fav_only)),1,1433,41},
  {uint16_t(offsetof(ZenSettings,ringtone_bpm_idx)),1,1434,42},
  {uint16_t(offsetof(ZenSettings,ringtone_len)),1,1435,43},
  {uint16_t(offsetof(ZenSettings,ringtone_notes)),32,1436,44},
  {uint16_t(offsetof(ZenSettings,home_pages_mask)),2,1468,45},
  {uint16_t(offsetof(ZenSettings,clock_hide_seconds)),1,1884,46},
  {uint16_t(offsetof(ZenSettings,clock_12h)),1,1885,47},
  {uint16_t(offsetof(ZenSettings,buzzer_auto)),1,1886,48},
  {uint16_t(offsetof(ZenSettings,dm_notif)),80,1887,49},
  {uint16_t(offsetof(ZenSettings,advert_auto_interval_sec)),4,1972,50},
  {uint16_t(offsetof(ZenSettings,ringtone2_bpm_idx)),1,1976,51},
  {uint16_t(offsetof(ZenSettings,ringtone2_len)),1,1977,52},
  {uint16_t(offsetof(ZenSettings,ringtone2_notes)),32,1978,53},
  {uint16_t(offsetof(ZenSettings,notif_melody_dm)),1,2010,54},
  {uint16_t(offsetof(ZenSettings,notif_melody_ch)),1,2011,55},
  {uint16_t(offsetof(ZenSettings,notif_melody_ad)),1,2012,56},
  {uint16_t(offsetof(ZenSettings,advert_sound_scope)),1,2013,57},
  {uint16_t(offsetof(ZenSettings,dm_melody)),80,2032,58},
  {uint16_t(offsetof(ZenSettings,use_lemon_font)),1,2112,59},
  {uint16_t(offsetof(ZenSettings,display_rotation)),1,2113,60},
  {uint16_t(offsetof(ZenSettings,page_order)),13,2114,61},
  {uint16_t(offsetof(ZenSettings,joystick_rotation)),1,2127,62},
  {uint16_t(offsetof(ZenSettings,eink_full_refresh_every)),1,2128,63},
  {uint16_t(offsetof(ZenSettings,page_order_set)),1,2129,64},
  {uint16_t(offsetof(ZenSettings,favourite_contacts)),36,2130,65},
  {uint16_t(offsetof(ZenSettings,ch_fav_bitmask)),8,2176,66},
  {uint16_t(offsetof(ZenSettings,ch_fav_only)),1,2184,67},
  {uint16_t(offsetof(ZenSettings,units_imperial)),1,2185,68},
  {uint16_t(offsetof(ZenSettings,keyboard_type)),1,2373,69},
  {uint16_t(offsetof(ZenSettings,gps_adaptive)),1,2599,70},
  {uint16_t(offsetof(ZenSettings,child_mode_enabled)),1,2600,71},
  {uint16_t(offsetof(ZenSettings,child_mode_pin_hash)),4,2604,72},
  {uint16_t(offsetof(ZenSettings,child_visible_pages)),2,2608,73},
  {uint16_t(offsetof(ZenSettings,child_channels_enabled)),1,2610,74},
  {uint16_t(offsetof(ZenSettings,quiet_time_enabled)),1,2611,75},
  {uint16_t(offsetof(ZenSettings,quiet_time_start_min)),2,2612,76},
  {uint16_t(offsetof(ZenSettings,quiet_time_end_min)),2,2614,77},
  {uint16_t(offsetof(ZenSettings,bluetooth_enabled)),1,2628,78},
  {uint16_t(offsetof(ZenSettings,channel_melody_overrides)),32,2629,79},
  {uint16_t(offsetof(ZenSettings,zen_config_schema)),2,2662,80},
  {uint16_t(offsetof(ZenSettings,timezone_mode)),1,2664,81},
  {uint16_t(offsetof(ZenSettings,timezone_city)),1,2665,82},
  {uint16_t(offsetof(ZenSettings,timezone_manual_min)),2,2666,83},
  {uint16_t(offsetof(ZenSettings,child_rooms_enabled)),1,2668,84},
  {uint16_t(offsetof(ZenSettings,notification_screen_wake)),1,2669,85},
  {uint16_t(offsetof(ZenSettings,notif_melody_new_contact)),1,2670,86},
  {uint16_t(offsetof(ZenSettings,pet_enabled)),1,1470,87},
};
static constexpr size_t LEGACY_SETTINGS_SIZE=2672;
static_assert(sizeof(((ZenSettings*)0)->buzzer_volume)==1,"Wire width changed: buzzer_volume");
static_assert(sizeof(((ZenSettings*)0)->display_brightness)==1,"Wire width changed: display_brightness");
static_assert(sizeof(((ZenSettings*)0)->auto_off_secs)==2,"Wire width changed: auto_off_secs");
static_assert(sizeof(((ZenSettings*)0)->tz_offset_hours)==1,"Wire width changed: tz_offset_hours");
static_assert(sizeof(((ZenSettings*)0)->batt_display_mode)==1,"Wire width changed: batt_display_mode");
static_assert(sizeof(((ZenSettings*)0)->custom_msgs)==700,"Wire width changed: custom_msgs");
static_assert(sizeof(((ZenSettings*)0)->ch_notif_override)==8,"Wire width changed: ch_notif_override");
static_assert(sizeof(((ZenSettings*)0)->ch_notif_muted)==8,"Wire width changed: ch_notif_muted");
static_assert(sizeof(((ZenSettings*)0)->dm_show_all)==1,"Wire width changed: dm_show_all");
static_assert(sizeof(((ZenSettings*)0)->room_fav_only)==1,"Wire width changed: room_fav_only");
static_assert(sizeof(((ZenSettings*)0)->ringtone_bpm_idx)==1,"Wire width changed: ringtone_bpm_idx");
static_assert(sizeof(((ZenSettings*)0)->ringtone_len)==1,"Wire width changed: ringtone_len");
static_assert(sizeof(((ZenSettings*)0)->ringtone_notes)==32,"Wire width changed: ringtone_notes");
static_assert(sizeof(((ZenSettings*)0)->home_pages_mask)==2,"Wire width changed: home_pages_mask");
static_assert(sizeof(((ZenSettings*)0)->clock_hide_seconds)==1,"Wire width changed: clock_hide_seconds");
static_assert(sizeof(((ZenSettings*)0)->clock_12h)==1,"Wire width changed: clock_12h");
static_assert(sizeof(((ZenSettings*)0)->buzzer_auto)==1,"Wire width changed: buzzer_auto");
static_assert(sizeof(((ZenSettings*)0)->dm_notif)==80,"Wire width changed: dm_notif");
static_assert(sizeof(((ZenSettings*)0)->advert_auto_interval_sec)==4,"Wire width changed: advert_auto_interval_sec");
static_assert(sizeof(((ZenSettings*)0)->ringtone2_bpm_idx)==1,"Wire width changed: ringtone2_bpm_idx");
static_assert(sizeof(((ZenSettings*)0)->ringtone2_len)==1,"Wire width changed: ringtone2_len");
static_assert(sizeof(((ZenSettings*)0)->ringtone2_notes)==32,"Wire width changed: ringtone2_notes");
static_assert(sizeof(((ZenSettings*)0)->notif_melody_dm)==1,"Wire width changed: notif_melody_dm");
static_assert(sizeof(((ZenSettings*)0)->notif_melody_ch)==1,"Wire width changed: notif_melody_ch");
static_assert(sizeof(((ZenSettings*)0)->notif_melody_ad)==1,"Wire width changed: notif_melody_ad");
static_assert(sizeof(((ZenSettings*)0)->advert_sound_scope)==1,"Wire width changed: advert_sound_scope");
static_assert(sizeof(((ZenSettings*)0)->dm_melody)==80,"Wire width changed: dm_melody");
static_assert(sizeof(((ZenSettings*)0)->use_lemon_font)==1,"Wire width changed: use_lemon_font");
static_assert(sizeof(((ZenSettings*)0)->display_rotation)==1,"Wire width changed: display_rotation");
static_assert(sizeof(((ZenSettings*)0)->page_order)==13,"Wire width changed: page_order");
static_assert(sizeof(((ZenSettings*)0)->joystick_rotation)==1,"Wire width changed: joystick_rotation");
static_assert(sizeof(((ZenSettings*)0)->eink_full_refresh_every)==1,"Wire width changed: eink_full_refresh_every");
static_assert(sizeof(((ZenSettings*)0)->page_order_set)==1,"Wire width changed: page_order_set");
static_assert(sizeof(((ZenSettings*)0)->favourite_contacts)==36,"Wire width changed: favourite_contacts");
static_assert(sizeof(((ZenSettings*)0)->ch_fav_bitmask)==8,"Wire width changed: ch_fav_bitmask");
static_assert(sizeof(((ZenSettings*)0)->ch_fav_only)==1,"Wire width changed: ch_fav_only");
static_assert(sizeof(((ZenSettings*)0)->units_imperial)==1,"Wire width changed: units_imperial");
static_assert(sizeof(((ZenSettings*)0)->keyboard_type)==1,"Wire width changed: keyboard_type");
static_assert(sizeof(((ZenSettings*)0)->gps_adaptive)==1,"Wire width changed: gps_adaptive");
static_assert(sizeof(((ZenSettings*)0)->child_mode_enabled)==1,"Wire width changed: child_mode_enabled");
static_assert(sizeof(((ZenSettings*)0)->child_mode_pin_hash)==4,"Wire width changed: child_mode_pin_hash");
static_assert(sizeof(((ZenSettings*)0)->child_visible_pages)==2,"Wire width changed: child_visible_pages");
static_assert(sizeof(((ZenSettings*)0)->child_channels_enabled)==1,"Wire width changed: child_channels_enabled");
static_assert(sizeof(((ZenSettings*)0)->quiet_time_enabled)==1,"Wire width changed: quiet_time_enabled");
static_assert(sizeof(((ZenSettings*)0)->quiet_time_start_min)==2,"Wire width changed: quiet_time_start_min");
static_assert(sizeof(((ZenSettings*)0)->quiet_time_end_min)==2,"Wire width changed: quiet_time_end_min");
static_assert(sizeof(((ZenSettings*)0)->bluetooth_enabled)==1,"Wire width changed: bluetooth_enabled");
static_assert(sizeof(((ZenSettings*)0)->channel_melody_overrides)==32,"Wire width changed: channel_melody_overrides");
static_assert(sizeof(((ZenSettings*)0)->zen_config_schema)==2,"Wire width changed: zen_config_schema");
static_assert(sizeof(((ZenSettings*)0)->timezone_mode)==1,"Wire width changed: timezone_mode");
static_assert(sizeof(((ZenSettings*)0)->timezone_city)==1,"Wire width changed: timezone_city");
static_assert(sizeof(((ZenSettings*)0)->timezone_manual_min)==2,"Wire width changed: timezone_manual_min");
static_assert(sizeof(((ZenSettings*)0)->child_rooms_enabled)==1,"Wire width changed: child_rooms_enabled");
static_assert(sizeof(((ZenSettings*)0)->notification_screen_wake)==1,"Wire width changed: notification_screen_wake");
static_assert(sizeof(((ZenSettings*)0)->notif_melody_new_contact)==1,"Wire width changed: notif_melody_new_contact");
static_assert(sizeof(((ZenSettings*)0)->pet_enabled)==1,"Wire width changed: pet_enabled");
}
