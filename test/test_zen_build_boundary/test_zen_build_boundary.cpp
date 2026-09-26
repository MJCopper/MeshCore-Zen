#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenPrefsCodec.h"

TEST(ZenBuildBoundary, ZenCodecNeverChangesBaselinePreferences) {
  ZenPrefs source;
  source.freq = 915.0f;
  source.tx_power_dbm = 22;
  source.child_mode_enabled = 1;
  source.quiet_time_enabled = 1;

  uint8_t encoded[zen::ZenPrefsCodec::MAX_ENCODED_SIZE] = {};
  size_t size = zen::ZenPrefsCodec::encode(source, 1, encoded, sizeof(encoded));
  ASSERT_GT(size, 0u);

  ZenPrefs destination;
  destination.freq = 433.0f;
  destination.tx_power_dbm = 10;
  ASSERT_TRUE(zen::ZenPrefsCodec::decode(destination, encoded, size));
  EXPECT_FLOAT_EQ(433.0f, destination.freq);
  EXPECT_EQ(10, destination.tx_power_dbm);
  EXPECT_EQ(1u, destination.child_mode_enabled);
  EXPECT_EQ(1u, destination.quiet_time_enabled);
}

TEST(ZenBuildBoundary, ZenCodecRoundTripsEveryActiveRecordGroup) {
  ZenPrefs source;
  source.child_mode_enabled = 1;
  source.child_mode_pin_hash = 0x12345678;
  source.quiet_time_enabled = 1;
  source.quiet_time_start_min = 123;
  source.bluetooth_enabled = 0;
  source.timezone_manual_min = 630;
  source.display_brightness = 2;
  source.home_pages_mask = 0x155;
  source.favourite_contacts[2][3] = 0xA5;
  source.notif_melody_dm = 4;
  source.ringtone_notes[3] = 17;
  source.dm_notif[1].state = 2;
  strcpy(source.custom_msgs[4], "On my way");
  strcpy(source.user_radio_presets[1].name, "Test preset");
  source.user_radio_presets[1].freq = 916.25f;
  source.user_radio_presets[1].sf = 9;
  source.advert_auto_interval_sec = 10800;
  source.locator_radius_idx = 3;
  source.alarm_hour = 7;
  source.display_brightness = 2;
  source.auto_off_secs = 60;
  source.bluetooth_enabled = 0;
  source.timezone_manual_min = -330;
  source.page_order_set = ZenPrefs::PAGE_ORDER_MAGIC;
  source.page_order[0] = ZenPrefs::HPB_COUNT;
  source.dm_show_all = 1;
  source.ch_fav_only = 1;

  uint8_t encoded[zen::ZenPrefsCodec::MAX_ENCODED_SIZE] = {};
  size_t size = zen::ZenPrefsCodec::encode(source, 42, encoded, sizeof(encoded));
  ASSERT_GT(size, 0u);

  ZenPrefs restored;
  uint32_t generation = 0;
  ASSERT_TRUE(zen::ZenPrefsCodec::decode(restored, encoded, size, &generation));
  EXPECT_EQ(42u, generation);
  EXPECT_EQ(source.child_mode_pin_hash, restored.child_mode_pin_hash);
  EXPECT_EQ(source.quiet_time_start_min, restored.quiet_time_start_min);
  EXPECT_EQ(source.bluetooth_enabled, restored.bluetooth_enabled);
  EXPECT_EQ(source.timezone_manual_min, restored.timezone_manual_min);
  EXPECT_EQ(source.home_pages_mask, restored.home_pages_mask);
  EXPECT_EQ(0xA5, restored.favourite_contacts[2][3]);
  EXPECT_EQ(source.notif_melody_dm, restored.notif_melody_dm);
  EXPECT_EQ(17, restored.ringtone_notes[3]);
  EXPECT_EQ(2, restored.dm_notif[1].state);
  EXPECT_STREQ("On my way", restored.custom_msgs[4]);
  EXPECT_STREQ("Test preset", restored.user_radio_presets[1].name);
  EXPECT_FLOAT_EQ(916.25f, restored.user_radio_presets[1].freq);
  EXPECT_EQ(9, restored.user_radio_presets[1].sf);
  EXPECT_EQ(10800u, restored.advert_auto_interval_sec);
  EXPECT_EQ(3, restored.locator_radius_idx);
  EXPECT_EQ(7, restored.alarm_hour);
  EXPECT_EQ(2, restored.display_brightness);
  EXPECT_EQ(60, restored.auto_off_secs);
  EXPECT_EQ(0, restored.bluetooth_enabled);
  EXPECT_EQ(-330, restored.timezone_manual_min);
  EXPECT_EQ(0xA5, restored.page_order_set);
  EXPECT_EQ(ZenPrefs::HPB_COUNT, restored.page_order[0]);
  EXPECT_EQ(1, restored.dm_show_all);
  EXPECT_EQ(1, restored.ch_fav_only);
}

TEST(ZenBuildBoundary, NewerCodecVersionsAreDetectedAndRejected) {
  ZenPrefs source;
  uint8_t encoded[zen::ZenPrefsCodec::MAX_ENCODED_SIZE] = {};
  size_t size = zen::ZenPrefsCodec::encode(source, 1, encoded, sizeof(encoded));
  ASSERT_GT(size, 5u);

  encoded[4] = 3;  // Header version immediately follows the four-byte magic.
  EXPECT_TRUE(zen::ZenPrefsCodec::hasNewerVersion(encoded, size));
  ZenPrefs destination;
  EXPECT_FALSE(zen::ZenPrefsCodec::decode(destination, encoded, size));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
