#include <gtest/gtest.h>
#include "../../examples/companion_radio/NodePrefs.h"
#include <target.h>
TEST(Companion118, RadioCommandsUpdateRealPreferencesAndDirtyState) {
  NodePrefs prefs;
  char reply[180] = {};
  auto radio = prefs.getRadioPrefs();
  ASSERT_TRUE(radio->handleCommand("set radio 915,250,10,5", 0, reply));
  EXPECT_FLOAT_EQ(915, prefs.freq);
  EXPECT_FLOAT_EQ(250, prefs.bw);
  EXPECT_EQ(10, prefs.sf); EXPECT_EQ(5, prefs.cr);
  EXPECT_TRUE(prefs.isDirty());
  prefs.clearDirty(); EXPECT_FALSE(prefs.isDirty());
  ASSERT_TRUE(radio->handleCommand("set tx 20", 0, reply));
  EXPECT_EQ(20, prefs.tx_power_dbm); EXPECT_EQ(20, radio_driver.tx_power);
  ASSERT_TRUE(radio->handleCommand("set radio.rxgain on", 0, reply));
  EXPECT_TRUE(radio_driver.rx_boosted);
  EXPECT_EQ(1, prefs.rx_boosted_gain);
}
TEST(Companion118, DynamicFemKeysRemainIndependentAndRejectUnknownKeys) {
  NodePrefs prefs;
  ASSERT_TRUE(prefs.getCustom()->setByKey("fem_rxgain", "1"));
  ASSERT_TRUE(prefs.getCustom()->setByKey("fem_txgain", "0"));
  EXPECT_EQ(1, prefs.radio_fem_rxgain); EXPECT_EQ(0, prefs.radio_fem_txgain);
  EXPECT_TRUE(prefs.isDirty());
  char value[32] = {};
  ASSERT_TRUE(prefs.getCustom()->getByKey("fem_rxgain", value, sizeof(value)));
  EXPECT_STREQ("1", value);
  EXPECT_FALSE(prefs.getRadioPrefs()->setByKey("unknown", "1"));
}
TEST(Companion118, InvalidRadioCommandDoesNotReplaceConfiguredRadio) {
  NodePrefs prefs; prefs.freq = 915; prefs.sf = 10;
  char reply[180] = {};
  ASSERT_TRUE(prefs.getRadioPrefs()->handleCommand("set radio 100,250,20,5", 0, reply));
  EXPECT_FLOAT_EQ(915, prefs.freq); EXPECT_EQ(10, prefs.sf);
  EXPECT_STREQ("Error, invalid radio params", reply);
}
int main(int argc,char**argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
