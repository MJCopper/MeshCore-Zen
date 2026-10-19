#include <gtest/gtest.h>
#include <Arduino.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/ClientACL.h>
#include "../../examples/bme680_sensor/RepeaterNameCache.h"

static mesh::Identity identity(uint8_t prefix) {
  uint8_t bytes[PUB_KEY_SIZE] = {};
  bytes[0] = prefix;
  return mesh::Identity(bytes);
}

static void advert(RepeaterNameCache& cache, const mesh::Identity& id,
                   const char* name, uint32_t time, uint8_t type = ADV_TYPE_REPEATER) {
  uint8_t data[MAX_ADVERT_DATA_SIZE];
  uint8_t length = AdvertDataBuilder(type, name).encodeTo(data);
  cache.onAdvert(id, time, data, length, g_mock_millis);
}

TEST(SensorStorage, RepeaterNamesPersistWithoutWritingUnchangedAdverts) {
  MemoryFileSystem fs;
  RepeaterNameCache cache;
  g_mock_millis = 0;
  cache.begin(&fs);
  auto id = identity(0x12);
  advert(cache, id, "Ridge", 1);
  cache.loop(29999);
  EXPECT_EQ(0u, fs.writes);
  cache.loop(30000);
  EXPECT_GT(fs.writes, 0u);
  size_t writes = fs.writes;
  advert(cache, id, "Ridge", 2);
  cache.loop(700000);
  EXPECT_EQ(writes, fs.writes);
  RepeaterNameCache rebooted;
  rebooted.begin(&fs);
  char label[13];
  rebooted.formatLabel(id.pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("Ridge", label);
}

TEST(SensorStorage, RejectsNonRepeatersAndRecoversFromCorruptCache) {
  MemoryFileSystem fs;
  RepeaterNameCache cache;
  cache.begin(&fs);
  auto id = identity(0x21);
  advert(cache, id, "Phone", 1, ADV_TYPE_CHAT);
  char label[13];
  cache.formatLabel(id.pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("21", label);
  fs.files["/repeaters"] = std::make_shared<std::vector<uint8_t>>(3, 0xFF);
  RepeaterNameCache rebooted;
  rebooted.begin(&fs);
  rebooted.formatLabel(id.pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("21", label);
}

TEST(SensorStorage, GuestPressureDoesNotEvictAdministrators) {
  ClientACL acl;
  for (int i = 1; i <= MAX_CLIENTS; i++)
    ASSERT_NE(nullptr, acl.putClientPreservingAdmins(identity(i), PERM_ACL_ADMIN));
  EXPECT_EQ(nullptr, acl.putClientPreservingAdmins(identity(200), PERM_ACL_READ_ONLY));
  for (int i = 1; i <= MAX_CLIENTS; i++)
    EXPECT_TRUE(acl.getClient(identity(i).pub_key, PUB_KEY_SIZE)->isAdmin());
}

TEST(SensorStorage, AmbiguousHashesUseHexAndLabelsPreserveUtf8) {
  MemoryFileSystem fs;
  RepeaterNameCache cache;
  cache.begin(&fs);
  auto a = identity(0x42), b = identity(0x42);
  a.pub_key[1] = 1;
  b.pub_key[1] = 2;
  advert(cache, a, "Ridge", 1);
  advert(cache, b, "Valley", 1);
  char label[13];
  cache.formatLabel(a.pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("42", label);
  cache.formatLabel(a.pub_key, 2, label, sizeof(label));
  EXPECT_STREQ("Ridge", label);
  advert(cache, a, "ABCDEFGHIJK\xF0\x9F\xA6\x89", 2);
  cache.formatLabel(a.pub_key, 2, label, sizeof(label));
  EXPECT_STREQ("ABCDEFGHIJK", label);
}

TEST(SensorStorage, CacheCapacityAndFailedReplacementKeepPreviousFile) {
  MemoryFileSystem fs;
  RepeaterNameCache cache;
  g_mock_millis = 0;
  cache.begin(&fs);
  for (int i = 1; i <= 50; i++) advert(cache, identity(i), "Repeater", 1);
  cache.loop(30000);
  ASSERT_TRUE(fs.exists("/repeaters"));
  EXPECT_EQ(12u + 48u * 68u, fs.files.at("/repeaters")->size());
  auto previous = *fs.files.at("/repeaters");
  g_mock_millis = 700000;
  advert(cache, identity(50), "Renamed", 2);
  fs.fail_rename = true;
  cache.loop(730000);
  EXPECT_EQ(previous, *fs.files.at("/repeaters"));
  EXPECT_FALSE(fs.exists("/repeaters.tmp"));
  fs.fail_rename = false;
  cache.loop(1330000);
  EXPECT_NE(previous, *fs.files.at("/repeaters"));
}

TEST(SensorStorage, RealSignaturesAndSharedSecretsAreCompatible) {
  class Random : public mesh::RNG {
    uint8_t value = 0;
    void random(uint8_t* dest, size_t length) override {
      for (size_t i = 0; i < length; i++) dest[i] = ++value;
    }
  } rng;
  mesh::LocalIdentity a(&rng), b(&rng);
  uint8_t signature[SIGNATURE_SIZE], sa[32], sb[32];
  const uint8_t text[] = { 1, 2, 3 };
  a.sign(signature, text, sizeof(text));
  EXPECT_TRUE(a.verify(signature, text, sizeof(text)));
  signature[0] ^= 1;
  EXPECT_FALSE(a.verify(signature, text, sizeof(text)));
  a.calcSharedSecret(sa, b);
  b.calcSharedSecret(sb, a);
  EXPECT_EQ(0, memcmp(sa, sb, sizeof(sa)));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
