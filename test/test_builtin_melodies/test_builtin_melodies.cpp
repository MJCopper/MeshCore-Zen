#include <gtest/gtest.h>
#include <cstring>

#include "../../examples/companion_radio/zen-overlay/app/zen/BuiltinMelodies.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationPreferences.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ConfigMaintenance.h"

TEST(BuiltinMelodies, ProvidesEightNamedShortMelodies) {
  for (uint8_t i = 0; i < zen::BuiltinMelodies::CUSTOM1; i++) {
    const char* name = zen::BuiltinMelodies::label(i);
    const char* melody = zen::BuiltinMelodies::melody(i);
    ASSERT_NE(name, nullptr);
    ASSERT_NE(melody, nullptr);
    EXPECT_GT(std::strlen(name), 0U);
    const char* notes = std::strrchr(melody, ':');
    ASSERT_NE(notes, nullptr);
    int count = 1;
    for (const char* p = notes + 1; *p; p++) if (*p == ',') count++;
    EXPECT_GE(count, 2);
    EXPECT_LE(count, 6);
  }
  EXPECT_EQ(zen::BuiltinMelodies::melody(zen::BuiltinMelodies::CUSTOM1), nullptr);
  EXPECT_EQ(zen::BuiltinMelodies::melody(zen::BuiltinMelodies::NONE), nullptr);
}

TEST(BuiltinMelodies, ResolvesGlobalAndExplicitOverridesForPreview) {
  EXPECT_EQ(zen::BuiltinMelodies::resolveOverride(0, zen::BuiltinMelodies::CHIME),
            zen::BuiltinMelodies::CHIME);
  EXPECT_EQ(zen::BuiltinMelodies::resolveOverride(
                zen::BuiltinMelodies::NONE + 1, zen::BuiltinMelodies::MESSAGE),
            zen::BuiltinMelodies::NONE);
}

TEST(BuiltinMelodies, RetainsTheTwoExistingDefaults) {
  EXPECT_STREQ(zen::BuiltinMelodies::melody(zen::BuiltinMelodies::MESSAGE),
               "MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
  EXPECT_STREQ(zen::BuiltinMelodies::melody(zen::BuiltinMelodies::KERPLOP),
               "kerplop:d=16,o=6,b=120:32g#,32c#");
}

TEST(BuiltinMelodies, CheerOrbitAndAlertHaveDistinctLengths) {
  static const uint8_t selections[] = {
    zen::BuiltinMelodies::CHEER,
    zen::BuiltinMelodies::ORBIT,
    zen::BuiltinMelodies::ALERT
  };
  const int expected[] = { 6, 5, 2 };
  for (int i = 0; i < 3; i++) {
    const char* notes = std::strrchr(zen::BuiltinMelodies::melody(selections[i]), ':');
    ASSERT_NE(notes, nullptr);
    int count = 1;
    for (const char* p = notes + 1; *p; p++) if (*p == ',') count++;
    EXPECT_EQ(count, expected[i]);
  }
}

TEST(BuiltinMelodies, MigratesLegacyGlobalSelectionsByContext) {
  EXPECT_EQ(zen::BuiltinMelodies::migrateLegacyGlobal(0, false), zen::BuiltinMelodies::MESSAGE);
  EXPECT_EQ(zen::BuiltinMelodies::migrateLegacyGlobal(0, true), zen::BuiltinMelodies::KERPLOP);
  EXPECT_EQ(zen::BuiltinMelodies::migrateLegacyGlobal(1, false), zen::BuiltinMelodies::CUSTOM1);
  EXPECT_EQ(zen::BuiltinMelodies::migrateLegacyGlobal(2, true), zen::BuiltinMelodies::CUSTOM2);
  EXPECT_EQ(zen::BuiltinMelodies::migrateLegacyGlobal(3, false), zen::BuiltinMelodies::NONE);
}

TEST(BuiltinMelodies, StoresAllChannelAndDirectMessageOverrides) {
  ZenPrefs prefs{};
  for (uint8_t selection = 0; selection < zen::BuiltinMelodies::COUNT; selection++) {
    uint8_t stored = selection + 1;
    zen::NotificationPreferences::setChannelMelody(&prefs, selection, stored);
    EXPECT_EQ(zen::NotificationPreferences::channelMelody(&prefs, selection), stored);
  }
  uint8_t key[4] = { 1, 2, 3, 4 };
  zen::NotificationPreferences::setDmMelody(&prefs, key, zen::BuiltinMelodies::ALERT + 1);
  EXPECT_EQ(zen::NotificationPreferences::dmMelody(&prefs, key),
            zen::BuiltinMelodies::ALERT + 1);
  zen::NotificationPreferences::setChannelMelody(&prefs, 1, 0);
  EXPECT_EQ(zen::NotificationPreferences::channelMelody(&prefs, 1), 0);
  EXPECT_NE(zen::NotificationPreferences::channelMelody(&prefs, 0), 0);
  EXPECT_NE(zen::NotificationPreferences::channelMelody(&prefs, 2), 0);
}

TEST(BuiltinMelodies, FullDirectMessageTableRejectsInsteadOfEvicting) {
  ZenPrefs prefs{};
  uint8_t key[4] = {};
  for (uint8_t i = 0; i < ZenPrefs::DM_MELODY_TABLE_MAX; i++) {
    key[0] = i + 1;
    ASSERT_TRUE(zen::NotificationPreferences::setDmMelody(
        &prefs, key, zen::BuiltinMelodies::CHIME + 1));
  }
  uint8_t overflow[4] = { 99, 1, 2, 3 };
  EXPECT_FALSE(zen::NotificationPreferences::setDmMelody(
      &prefs, overflow, zen::BuiltinMelodies::ALERT + 1));
  uint8_t first[4] = { 1, 0, 0, 0 };
  EXPECT_EQ(zen::NotificationPreferences::dmMelody(&prefs, first),
            zen::BuiltinMelodies::CHIME + 1);
  EXPECT_EQ(zen::NotificationPreferences::dmMelody(&prefs, overflow), 0);
}

TEST(BuiltinMelodies, MigratesACompleteLegacyPreferenceState) {
  ZenPrefs prefs{};
  prefs.notif_melody_dm = 1;
  prefs.notif_melody_ch = 0;
  prefs.notif_melody_ad = 3;
  prefs.dm_melody[0].prefix[0] = 42;
  prefs.dm_melody[0].slot = 2;
  prefs.ch_notif_melody_set = (1ULL << 3) | (1ULL << 4);
  prefs.ch_notif_melody_2 = 1ULL << 4;

  EXPECT_TRUE(zen::ConfigMaintenance::migrateMelodySchema(prefs, 0xC0DE0029));
  EXPECT_EQ(prefs.notif_melody_dm, zen::BuiltinMelodies::CUSTOM1);
  EXPECT_EQ(prefs.notif_melody_ch, zen::BuiltinMelodies::KERPLOP);
  EXPECT_EQ(prefs.notif_melody_ad, zen::BuiltinMelodies::NONE);
  EXPECT_EQ(prefs.dm_melody[0].slot, zen::BuiltinMelodies::CUSTOM2 + 1);
  EXPECT_EQ(zen::NotificationPreferences::channelMelody(&prefs, 3),
            zen::BuiltinMelodies::CUSTOM1 + 1);
  EXPECT_EQ(zen::NotificationPreferences::channelMelody(&prefs, 4),
            zen::BuiltinMelodies::CUSTOM2 + 1);
  EXPECT_EQ(prefs.ch_notif_melody_set, 0U);
  EXPECT_EQ(prefs.ch_notif_melody_2, 0U);
  EXPECT_FALSE(zen::ConfigMaintenance::migrateMelodySchema(prefs, 0xC0DE002A));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
