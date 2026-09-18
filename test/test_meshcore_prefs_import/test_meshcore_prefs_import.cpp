#include <gtest/gtest.h>
#include <Stream.h>
#include <cstring>

#include "../../examples/companion_radio/solo/MeshCorePrefsImport.h"

class PrefsStream : public Stream {
  const char* text;
  int pos = 0;
  int length;
public:
  explicit PrefsStream(const char* value) : text(value), length(strlen(value)) { }
  int available() override { return length - pos; }
  int read() override { return pos < length ? text[pos++] : -1; }
  int peek() override { return pos < length ? text[pos] : -1; }
};

TEST(MeshCorePrefsImport, PreservesSharedSettingsAndZenOptions) {
  PrefsStream input(
      "{name:\"Test Node\",lat:-33.86,lon:151.21,"
      "radio:{freq:915.875,bw:250,sf:10,cr:5,rxgain:1,tx:22,af:1,rxdelay:0,hash_mode:1,multi_ack:2},"
      "gps:{en:1,int:300,adv_loc:1},repeat:{disable:1},"
      "comp:{auto_max:3,defs_nm:\"Test\",defs_key:\"00112233445566778899AABBCCDDEEFF\","
      "pin:123456,buzz_q:1,auto_add:2,man_add:1,tel_base:2,tel_loc:1,tel_env:0}}");
  solo::MeshCorePrefsImport importer;
  ASSERT_TRUE(importer.read(input));
  NodePrefs prefs = {};
  prefs.home_pages_mask = 0x55;
  prefs.child_mode_enabled = 1;
  double latitude = 0, longitude = 0;
  importer.apply(prefs, latitude, longitude);

  EXPECT_STREQ("Test Node", prefs.node_name);
  EXPECT_FLOAT_EQ(915.875f, prefs.freq);
  EXPECT_FLOAT_EQ(250.0f, prefs.bw);
  EXPECT_EQ(10, prefs.sf);
  EXPECT_EQ(5, prefs.cr);
  EXPECT_EQ(22, prefs.tx_power_dbm);
  EXPECT_EQ(300u, prefs.gps_interval);
  EXPECT_EQ(123456u, prefs.ble_pin);
  EXPECT_EQ(0, prefs.client_repeat);
  EXPECT_EQ(0x11, prefs.default_scope_key[1]);
  EXPECT_DOUBLE_EQ(-33.86, latitude);
  EXPECT_DOUBLE_EQ(151.21, longitude);
  EXPECT_EQ(0x55, prefs.home_pages_mask);
  EXPECT_EQ(1, prefs.child_mode_enabled);
}

TEST(MeshCorePrefsImport, RejectsIncompleteOrMalformedRadio) {
  PrefsStream incomplete("{name:\"Test\",radio:{freq:915.875}}");
  solo::MeshCorePrefsImport importer;
  EXPECT_FALSE(importer.read(incomplete));

  PrefsStream malformed("{radio:{freq:915.875,bw:250,sf:10,cr:5}");
  solo::MeshCorePrefsImport second;
  EXPECT_FALSE(second.read(malformed));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
