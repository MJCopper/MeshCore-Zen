#include <gtest/gtest.h>
#include <cstring>

#include "../../examples/companion_radio/solo/ConfigMaintenance.h"

TEST(ConfigMaintenance, ClearsRemovedFeaturesOnceAndTracksSchema) {
  NodePrefs prefs{};
  prefs.custom_msgs[5][0] = 'x';
  prefs.bot_enabled = 1;
  prefs.locator_enabled = 1;
  prefs.alarm_on = 1;
  prefs.gpio1_mode = 3;
  prefs.reserved_radio_power = 4;
  prefs.favourite_contacts[0][0] = 9;

  EXPECT_TRUE(solo::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.zen_config_schema, solo::ConfigMaintenance::CURRENT_SCHEMA);
  EXPECT_EQ(prefs.custom_msgs[5][0], 'x');
  EXPECT_EQ(prefs.bot_enabled, 0);
  EXPECT_EQ(prefs.locator_enabled, 0);
  EXPECT_EQ(prefs.alarm_on, 0);
  EXPECT_EQ(prefs.gpio1_mode, 0);
  EXPECT_EQ(prefs.reserved_radio_power, 4);
  EXPECT_EQ(prefs.favourite_contacts[0][0], 9);
  EXPECT_FALSE(solo::ConfigMaintenance::apply(prefs));
}

TEST(ConfigMaintenance, RepairsActiveValuesAfterMigration) {
  NodePrefs prefs{};
  prefs.zen_config_schema = solo::ConfigMaintenance::CURRENT_SCHEMA;
  prefs.notif_melody_ch = 255;
  prefs.ringtone_bpm_idx = 99;
  prefs.ringtone_len = 31;
  prefs.quiet_time_start_min = 2000;
  prefs.ble_pin = 42;
  prefs.gps_interval = 21600;
  prefs.notification_screen_wake = 255;
  prefs.notif_melody_new_contact = 255;

  EXPECT_TRUE(solo::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.notif_melody_ch, solo::BuiltinMelodies::KERPLOP);
  EXPECT_EQ(prefs.ringtone_bpm_idx, 2);
  EXPECT_EQ(prefs.ringtone_len, solo::RingtoneModel::MAX_NOTES);
  EXPECT_EQ(prefs.quiet_time_start_min, 21 * 60);
  EXPECT_EQ(prefs.ble_pin, 0U);
  EXPECT_EQ(prefs.gps_interval, 3600U);
  EXPECT_EQ(prefs.notification_screen_wake, 1);
  EXPECT_EQ(prefs.notif_melody_new_contact, solo::BuiltinMelodies::NONE);
}

TEST(ConfigMaintenance, PreservesAlwaysScreenWake) {
  NodePrefs prefs{};
  prefs.zen_config_schema = solo::ConfigMaintenance::CURRENT_SCHEMA;
  prefs.notification_screen_wake = 2;
  EXPECT_FALSE(solo::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.notification_screen_wake, 2);
}

TEST(ConfigMaintenance, MigratesRemovedLongGpsPollingToOneHour) {
  NodePrefs prefs{};
  prefs.zen_config_schema = 4;
  prefs.gps_interval = 10800;

  EXPECT_TRUE(solo::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.zen_config_schema, solo::ConfigMaintenance::CURRENT_SCHEMA);
  EXPECT_EQ(prefs.gps_interval, 3600U);
  EXPECT_FALSE(solo::ConfigMaintenance::apply(prefs));
}

TEST(ConfigMaintenance, ClearsRetiredCardKbByteBeforeAdaptiveGpsUse) {
  NodePrefs prefs{};
  prefs.zen_config_schema = 5;
  prefs.gps_adaptive = 1;

  EXPECT_TRUE(solo::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.zen_config_schema, solo::ConfigMaintenance::CURRENT_SCHEMA);
  EXPECT_EQ(prefs.gps_adaptive, 0);
}

TEST(ConfigMaintenance, MigratesLegacyTimezoneWithoutChangingLocalTime) {
  NodePrefs prefs{};
  prefs.zen_config_schema = 2;
  prefs.tz_offset_hours = 10;

  EXPECT_TRUE(solo::ConfigMaintenance::apply(prefs));
  EXPECT_EQ(prefs.timezone_mode, solo::TimezonePolicy::MANUAL);
  EXPECT_EQ(prefs.timezone_city, 0);
  EXPECT_EQ(prefs.timezone_manual_min, 600);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
