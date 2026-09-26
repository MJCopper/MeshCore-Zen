#include <gtest/gtest.h>
#include <cstring>

#include "../../examples/companion_radio/zen-overlay/app/zen/ConfigMaintenance.h"

TEST(ConfigMaintenance, ClearsRemovedFeaturesOnceAndTracksSchema) {
  ZenPrefs prefs{};
  prefs.custom_msgs[5][0] = 'x';
  prefs.bot_enabled = 1;
  prefs.locator_enabled = 1;
  prefs.alarm_on = 1;
  prefs.gpio1_mode = 3;
  prefs.reserved_radio_power = 4;
  prefs.favourite_contacts[0][0] = 9;

  EXPECT_TRUE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.zen_config_schema, zen::ConfigMaintenance::CURRENT_SCHEMA);
  EXPECT_EQ(prefs.custom_msgs[5][0], 'x');
  EXPECT_EQ(prefs.bot_enabled, 0);
  EXPECT_EQ(prefs.locator_enabled, 0);
  EXPECT_EQ(prefs.alarm_on, 0);
  EXPECT_EQ(prefs.gpio1_mode, 0);
  EXPECT_EQ(prefs.reserved_radio_power, 4);
  EXPECT_EQ(prefs.favourite_contacts[0][0], 9);
  EXPECT_FALSE(zen::ConfigMaintenance::apply(prefs));
}

TEST(ConfigMaintenance, FreshDefaultsRemainCurrentWithoutLegacyMigration) {
  ZenPrefs prefs{};
  zen::PrefsDefaults::apply(prefs);
  prefs.gps_adaptive = 1;
  prefs.zen_config_schema = zen::ConfigMaintenance::CURRENT_SCHEMA;

  EXPECT_FALSE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(1, prefs.bluetooth_enabled);
  EXPECT_EQ(1, prefs.keyboard_type);
  EXPECT_EQ((uint16_t)(1U << 11), prefs.child_visible_pages);
  EXPECT_EQ(1, prefs.gps_adaptive);
}

TEST(ConfigMaintenance, RepairsActiveValuesAfterMigration) {
  ZenPrefs prefs{};
  prefs.zen_config_schema = zen::ConfigMaintenance::CURRENT_SCHEMA;
  prefs.notif_melody_ch = 255;
  prefs.ringtone_bpm_idx = 99;
  prefs.ringtone_len = 31;
  prefs.quiet_time_start_min = 2000;
  prefs.ble_pin = 42;
  prefs.gps_interval = 21600;
  prefs.notification_screen_wake = 255;
  prefs.notif_melody_new_contact = 255;

  EXPECT_TRUE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.notif_melody_ch, zen::BuiltinMelodies::KERPLOP);
  EXPECT_EQ(prefs.ringtone_bpm_idx, 2);
  EXPECT_EQ(prefs.ringtone_len, zen::RingtoneModel::MAX_NOTES);
  EXPECT_EQ(prefs.quiet_time_start_min, 21 * 60);
  EXPECT_EQ(prefs.ble_pin, 42U);
  EXPECT_EQ(prefs.gps_interval, 21600U);
  EXPECT_EQ(prefs.notification_screen_wake, 1);
  EXPECT_EQ(prefs.notif_melody_new_contact, zen::BuiltinMelodies::NONE);
}

TEST(ConfigMaintenance, PreservesAlwaysScreenWake) {
  ZenPrefs prefs{};
  prefs.zen_config_schema = zen::ConfigMaintenance::CURRENT_SCHEMA;
  prefs.notification_screen_wake = 2;
  EXPECT_FALSE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.notification_screen_wake, 2);
}

TEST(ConfigMaintenance, DoesNotChangeMeshCoreGpsPolling) {
  ZenPrefs prefs{};
  prefs.zen_config_schema = 4;
  prefs.gps_interval = 10800;

  EXPECT_TRUE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.zen_config_schema, zen::ConfigMaintenance::CURRENT_SCHEMA);
  EXPECT_EQ(prefs.gps_interval, 10800U);
  EXPECT_FALSE(zen::ConfigMaintenance::apply(prefs));
}

TEST(ConfigMaintenance, ClearsRetiredCardKbByteBeforeAdaptiveGpsUse) {
  ZenPrefs prefs{};
  prefs.zen_config_schema = 5;
  prefs.gps_adaptive = 1;

  EXPECT_TRUE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.zen_config_schema, zen::ConfigMaintenance::CURRENT_SCHEMA);
  EXPECT_EQ(prefs.gps_adaptive, 0);
}

TEST(ConfigMaintenance, MigratesLegacyTimezoneWithoutChangingLocalTime) {
  ZenPrefs prefs{};
  prefs.zen_config_schema = 2;
  prefs.tz_offset_hours = 10;

  EXPECT_TRUE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.timezone_mode, zen::TimezonePolicy::MANUAL);
  EXPECT_EQ(prefs.timezone_city, 0);
  EXPECT_EQ(prefs.timezone_manual_min, 600);
}

TEST(ConfigMaintenance, NormalizesZenOverlayValues) {
  ZenPrefs prefs = {};
  prefs.zen_config_schema = zen::ConfigMaintenance::CURRENT_SCHEMA;
  prefs.display_brightness = 99;
  prefs.auto_off_secs = 17;
  prefs.clock_hide_seconds = 4;
  prefs.units_imperial = 8;
  prefs.eink_full_refresh_every = 9;
  prefs.buzzer_volume = 7;
  prefs.page_order_set = 1;
  prefs.custom_msgs[0][sizeof(prefs.custom_msgs[0]) - 1] = 'x';
  std::memset(prefs.user_radio_presets[0].name, 'x', sizeof(prefs.user_radio_presets[0].name));
  prefs.user_radio_presets[0].freq = NAN;

  EXPECT_TRUE(zen::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(4, prefs.display_brightness);
  EXPECT_EQ(15, prefs.auto_off_secs);
  EXPECT_EQ(0, prefs.clock_hide_seconds);
  EXPECT_EQ(0, prefs.units_imperial);
  EXPECT_EQ(0, prefs.eink_full_refresh_every);
  EXPECT_EQ(4, prefs.buzzer_volume);
  EXPECT_EQ(0, prefs.page_order_set);
  EXPECT_EQ('\0', prefs.custom_msgs[0][sizeof(prefs.custom_msgs[0]) - 1]);
  EXPECT_EQ('\0', prefs.user_radio_presets[0].name[0]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
