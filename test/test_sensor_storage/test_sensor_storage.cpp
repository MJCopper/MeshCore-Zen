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

TEST(SensorStorage, DisplayNamesRemoveWholeRepeaterWordsBeforeTruncation) {
  RepeaterNameCache cache;
  const struct { const char* name; const char* expected; } cases[] = {
    { "Hillvue Repeater", "Hillvue" },
    { "Tamworth rptr", "Tamworth" },
    { "RPTR North Ridge", "North Ridge" },
    { "North RePeAtEr Hill", "North Hill" },
    { "  rptr   Hillvue  repeater  ", "Hillvue" },
    { "Repeater RPTR", "12" },
    { "RepeaterHill", "RepeaterHill" },
    { "My_rptr", "My_rptr" },
    { "rptr2 Hill", "rptr2 Hill" },
    { "Hillvue-Repeater", "Hillvue-" },
    { "Repeater Long Location Name", "Long Locatio" },
    { "Repeater \xF0\x9F\xA6\x89 Hill", "\xF0\x9F\xA6\x89 Hill" },
    { "Repeater\xC3\xA9", "Repeater\xC3\xA9" },
  };
  uint32_t timestamp = 0;
  for (const auto& entry : cases) {
    SCOPED_TRACE(entry.name);
    advert(cache, identity(0x12), entry.name, ++timestamp);
    char label[13];
    cache.formatLabel(identity(0x12).pub_key, 1, label, sizeof(label));
    EXPECT_STREQ(entry.expected, label);
  }
}

TEST(SensorStorage, DisplayCleanupPreservesUtf8AndTinyBufferBoundaries) {
  RepeaterNameCache cache;
  auto id = identity(0x12);
  advert(cache, id, "Repeater ABCDEFGHIJK\xF0\x9F\xA6\x89", 1);
  char label[13];
  cache.formatLabel(id.pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("ABCDEFGHIJK", label);
  char tiny[2] = { 'x', 'x' };
  cache.formatLabel(id.pub_key, 1, tiny, 1);
  EXPECT_EQ(0, tiny[0]);
  EXPECT_EQ('x', tiny[1]);
  cache.formatLabel(id.pub_key, 1, tiny, 0);
  cache.formatLabel(id.pub_key, 1, NULL, 0);
  advert(cache, id, "Repeater", 2);
  char hash[7];
  cache.formatLabel(id.pub_key, 3, hash, sizeof(hash));
  EXPECT_STREQ("120000", hash);
}

TEST(SensorStorage, FormattingNeverChangesStoredAdvertNamesOrWritesFlash) {
  g_mock_millis = 0;
  MemoryFileSystem fs;
  RepeaterNameCache cache;
  cache.begin(&fs);
  advert(cache, identity(0x12), "Hillvue Repeater", 1);
  cache.loop(30000);
  ASSERT_TRUE(fs.exists("/repeaters"));
  auto original = *fs.files.at("/repeaters");
  size_t writes = fs.writes;
  char label[13];
  cache.formatLabel(identity(0x12).pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("Hillvue", label);
  EXPECT_EQ(original, *fs.files.at("/repeaters"));
  EXPECT_NE(std::string::npos, std::string(original.begin(), original.end()).find("Hillvue Repeater"));
  advert(cache, identity(0x12), "Hillvue Repeater", 2);
  cache.loop(700000);
  EXPECT_EQ(writes, fs.writes);
  RepeaterNameCache rebooted;
  rebooted.begin(&fs);
  rebooted.formatLabel(identity(0x12).pub_key, 1, label, sizeof(label));
  EXPECT_STREQ("Hillvue", label);
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

TEST(SensorStorage, PathFormatterBoundsLongRoutesAndPreservesWholeShortLabels) {
  RepeaterNameCache cache;
  mesh::Packet packet;
  packet.header = ROUTE_TYPE_FLOOD;
  packet.setPathHashSizeAndCount(1, 48);
  memset(packet.path, 0x12, 48);
  advert(cache, identity(0x12), "North Ridge Long Name", 1);
  char response[321];
  cache.formatPath(&packet, response, sizeof(response));
  EXPECT_EQ(0u, std::string(response).find("Path (48 repeaters): North Ridge "));
  EXPECT_NE(std::string::npos, std::string(response).find("\n...truncated"));
  EXPECT_LT(strlen(response), sizeof(response));
  char tiny[5];
  cache.formatPath(&packet, tiny, sizeof(tiny));
  EXPECT_STREQ("Path", tiny);
  packet.setPathHashSizeAndCount(3, 63);
  cache.formatPath(&packet, response, sizeof(response));
  EXPECT_STREQ("Path unavailable (invalid route)", response);
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

TEST(SensorStorage, DestinationLookupPreservesFullNameAndFullIdentity) {
  RepeaterNameCache cache;
  auto id = identity(0x71); id.pub_key[1] = 0x88; id.pub_key[31] = 0x99;
  advert(cache, id, "  Hilltop   Repeater  ", 1);
  uint8_t key[32]{}; char label[32];
  ASSERT_EQ(RepeaterNameCache::UNIQUE, cache.resolvePrefix(id.pub_key, key, label, sizeof(label)));
  EXPECT_EQ(0, memcmp(id.pub_key, key, 32)); EXPECT_STREQ("Hilltop Repeater", label);
  char hop[32]; cache.formatLabel(id.pub_key, 2, hop, sizeof(hop));
  EXPECT_STREQ("Hilltop", hop); // Compact hop policy deliberately stays separate.
}

TEST(SensorStorage, DestinationUnknownAndAmbiguousClearAllOutputs) {
  RepeaterNameCache cache; auto id = identity(0x71); id.pub_key[1] = 0x88;
  uint8_t key[32]; char label[32]; memset(key, 0xff, sizeof(key)); strcpy(label, "stale");
  EXPECT_EQ(RepeaterNameCache::UNKNOWN, cache.resolvePrefix(id.pub_key, key, label, sizeof(label)));
  uint8_t zero[32]{}; EXPECT_EQ(0, memcmp(key, zero, 32)); EXPECT_STREQ("", label);
  advert(cache, id, "Hilltop Repeater", 1);
  auto other = id; other.pub_key[31] = 1; advert(cache, other, "Hilltop Repeater", 1);
  memset(key, 0xff, sizeof(key)); strcpy(label, "stale");
  EXPECT_EQ(RepeaterNameCache::AMBIGUOUS, cache.resolvePrefix(id.pub_key, key, label, sizeof(label)));
  EXPECT_EQ(0, memcmp(key, zero, 32)); EXPECT_STREQ("", label);
}

TEST(SensorStorage, DestinationNamesNormalizeWhitespaceAndFallbackWithoutInventingIdentity) {
  const char* names[] = {"", "   ", "\t\n\x7f", "\xff", "Hill\tTop\nRPT", "Repeater", "Hill \xf0\x9f\x8f\x94"};
  const char* expected[] = {"7188", "7188", "7188", "7188", "Hill Top RPT", "Repeater", "Hill \xf0\x9f\x8f\x94"};
  for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
    RepeaterNameCache cache; auto id = identity(0x71); id.pub_key[1] = 0x88;
    if (i == 3) {
      // Builder strips malformed UTF-8; exercise a named raw advert explicitly.
      uint8_t raw[] = {uint8_t(ADV_TYPE_REPEATER | ADV_NAME_MASK), 0xff};
      cache.onAdvert(id, 1, raw, sizeof(raw), g_mock_millis);
    } else advert(cache, id, names[i], 1);
    uint8_t key[32]{}; char label[32];
    auto result = cache.resolvePrefix(id.pub_key, key, label, sizeof(label));
    if (i == 0) { EXPECT_EQ(RepeaterNameCache::UNKNOWN, result); continue; } // No name field is not admitted.
    ASSERT_EQ(RepeaterNameCache::UNIQUE, result);
    EXPECT_STREQ(expected[i], label); EXPECT_EQ(0, memcmp(key, id.pub_key, 32));
  }
}

TEST(SensorStorage, DestinationMaximumUtf8NameAndSnapshotSurviveCacheReplacement) {
  RepeaterNameCache cache; auto id = identity(0x71); id.pub_key[1] = 0x88;
  std::string long_name = std::string(29, 'a') + "\xf0\x9f\x8f\x94";
  advert(cache, id, long_name.c_str(), 1);
  uint8_t key[32]{}; char label[32];
  ASSERT_EQ(RepeaterNameCache::UNIQUE, cache.resolvePrefix(id.pub_key, key, label, sizeof(label)));
  EXPECT_EQ(std::string(29, 'a'), label); // Incomplete stored UTF-8 tail cannot leak onto radio.
  advert(cache, id, "New Name", 2);
  EXPECT_EQ(std::string(29, 'a'), label); EXPECT_EQ(0, memcmp(key, id.pub_key, 32));
  char current[32]; cache.resolvePrefix(id.pub_key, key, current, sizeof(current));
  EXPECT_STREQ("New Name", current);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
