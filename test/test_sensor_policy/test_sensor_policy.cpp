#include <gtest/gtest.h>
#include <Arduino.h>
#include <limits>
#include "SensorManagementPolicy.h"
#include "Bme680Telemetry.h"
#include "PublicChannelSensorBot.h"

static mesh::Identity identity(uint8_t prefix) {
  uint8_t bytes[PUB_KEY_SIZE] = {};
  bytes[0] = prefix;
  return mesh::Identity(bytes);
}

static CayenneLPP reading(uint8_t channel = 7, float temperature = 22.4f) {
  CayenneLPP raw(180);
  raw.addTemperature(channel, temperature);
  raw.addRelativeHumidity(channel, 48);
  raw.addBarometricPressure(channel, 1013.2f);
  raw.addAltitude(channel, 300);
  raw.addGenericSensor(channel, 100000);
  return raw;
}

TEST(SensorPolicy, BlankAndWrongPasswordGetReadOnlyTelemetryRole) {
  for (const char* password : { "", "wrong", "guest" }) {
    ClientACL clients;
    uint8_t secret[32] = { 42 };
    bool persist = true;
    auto client = SensorManagementPolicy::login(clients, identity(1), secret, 1, 2,
                                                password, "admin", persist);
    ASSERT_NE(nullptr, client);
    EXPECT_EQ(PERM_ACL_READ_ONLY, client->permissions);
    EXPECT_EQ(2, SensorManagementPolicy::loginMode(*client));
    EXPECT_FALSE(persist);
    EXPECT_FALSE(SensorManagementPolicy::persistClient(client));
    EXPECT_EQ(0, memcmp(secret, client->shared_secret, 32));
  }
}

TEST(SensorPolicy, AdminPasswordAndKnownBlankPasswordRetainAdminAccess) {
  ClientACL clients;
  uint8_t secret[32] = {};
  bool persist;
  auto client = SensorManagementPolicy::login(clients, identity(1), secret, 1, 2,
                                              "admin", "admin", persist);
  ASSERT_NE(nullptr, client);
  EXPECT_TRUE(client->isAdmin());
  EXPECT_TRUE(persist);
  EXPECT_EQ(1, SensorManagementPolicy::loginMode(*client));
  EXPECT_NE(nullptr, SensorManagementPolicy::login(clients, identity(1), secret, 2, 3,
                                                  "", "admin", persist));
  EXPECT_TRUE(client->isAdmin());
  EXPECT_FALSE(persist);
  EXPECT_EQ(nullptr, SensorManagementPolicy::login(clients, identity(1), secret, 2, 3,
                                                  "", "admin", persist));
}

TEST(SensorPolicy, OnlyAdminSessionsAreSavedAndWireLayoutIsStable) {
  ClientACL clients;
  uint8_t secret[32] = {};
  bool persist;
  SensorManagementPolicy::login(clients, identity(1), secret, 1, 2, "admin", "admin", persist);
  SensorManagementPolicy::login(clients, identity(2), secret, 1, 2, "", "admin", persist);
  MemoryFileSystem fs;
  clients.save(&fs, SensorManagementPolicy::persistClient);
  EXPECT_EQ(136u, fs.files.at("/s_contacts")->size());
  EXPECT_EQ(56u, sizeof(SensorStats));
  EXPECT_EQ(40u, offsetof(SensorStats, err_events));
  EXPECT_EQ(52u, offsetof(SensorStats, n_recv_errors));
}

TEST(SensorPolicy, OwnerInfoIsBoundedAndFormattedForApp) {
  uint8_t bytes[32] = {};
  int length = SensorManagementPolicy::ownerInfo("v1.17.1", "Hillvue", "Owner", bytes, sizeof(bytes));
  EXPECT_STREQ("v1.17.1\nHillvue\nOwner", (const char*)bytes);
  EXPECT_EQ(strlen((char*)bytes), (size_t)length);
  EXPECT_EQ(7, SensorManagementPolicy::ownerInfo("long version", "name", "owner", bytes, 8));
}

TEST(SensorTelemetry, DiscoversChannelAndAppliesAltitudeOffset) {
  Bme680Telemetry snapshot;
  auto raw = reading(7, -5.5f);
  ASSERT_TRUE(snapshot.ingest(raw));
  EXPECT_EQ(7, snapshot.channel());
  EXPECT_FLOAT_EQ(-5.5f, snapshot.temperature);
  EXPECT_FLOAT_EQ(48, snapshot.humidity);
  EXPECT_FLOAT_EQ(400, snapshot.altitude);
  EXPECT_EQ(1, snapshot.samples());
  EXPECT_FALSE(snapshot.airReady());
  CayenneLPP empty(180);
  EXPECT_FALSE(snapshot.ingest(empty));
  EXPECT_EQ(1, snapshot.samples());
  EXPECT_FLOAT_EQ(-5.5f, snapshot.temperature);
}

TEST(SensorTelemetry, CachedEncodingNeverAdvancesCalibration) {
  Bme680Telemetry snapshot;
  auto raw = reading();
  ASSERT_TRUE(snapshot.ingest(raw));
  snapshot.setVoltage(3.7f);
  CayenneLPP output(180);
  for (int i = 0; i < 100; i++) snapshot.encode(output);
  EXPECT_EQ(1, snapshot.samples());
  EXPECT_EQ(0, output.getBuffer()[0] - 1);
  EXPECT_EQ(LPP_VOLTAGE, output.getBuffer()[1]);
  snapshot.encode(output, 0);
  EXPECT_EQ(4, output.getSize());
  for (int i = 0; i < 9; i++) ASSERT_TRUE(snapshot.ingest(raw));
  EXPECT_TRUE(snapshot.airReady());
  EXPECT_EQ(10, snapshot.samples());
}

TEST(SensorTelemetry, NonFiniteGasCannotAdvanceCalibration) {
  BME680AirQuality quality;
  float result;
  EXPECT_FALSE(quality.update(std::numeric_limits<float>::quiet_NaN(), 40, result));
  EXPECT_FALSE(quality.update(100000, std::numeric_limits<float>::infinity(), result));
  EXPECT_EQ(0, quality.samples());
}

TEST(SensorTelemetry, InvalidSamplesPreserveTheLastSuccessfulSnapshot) {
  Bme680Telemetry snapshot;
  auto good = reading();
  ASSERT_TRUE(snapshot.ingest(good));
  for (bool bad_humidity : { false, true }) {
    CayenneLPP invalid(180);
    invalid.addTemperature(7, 10);
    invalid.addRelativeHumidity(7, bad_humidity ? 110 : 40);
    invalid.addBarometricPressure(7, 1013);
    invalid.addAltitude(7, 200);
    invalid.addGenericSensor(7, bad_humidity ? 100000 : 0);
    EXPECT_FALSE(snapshot.ingest(invalid));
    EXPECT_EQ(1, snapshot.samples());
    EXPECT_FLOAT_EQ(400, snapshot.altitude);
    EXPECT_FLOAT_EQ(22.4f, snapshot.temperature);
  }
}

TEST(SensorTelemetry, BeforeFirstSampleOnlyValidCachedVoltageIsReturned) {
  Bme680Telemetry snapshot;
  CayenneLPP output(180);
  snapshot.encode(output);
  EXPECT_EQ(0, output.getSize());
  snapshot.setVoltage(3.7f);
  snapshot.setVoltage(std::numeric_limits<float>::quiet_NaN());
  snapshot.setVoltage(-1);
  snapshot.encode(output);
  EXPECT_EQ(4, output.getSize());
  EXPECT_FLOAT_EQ(3.7f, snapshot.voltage);
  EXPECT_FALSE(snapshot.ready());
  EXPECT_EQ(0, snapshot.samples());
}

TEST(SensorChannels, DuplicateTrackingDistinguishesChannelsButSharesRateLimit) {
  PublicChannelSensorBot bot("#hillvue");
  auto accept = [&](uint32_t timestamp, uint8_t channel_index) {
    uint8_t data[MAX_PACKET_PAYLOAD] = {};
    memcpy(data, &timestamp, 4);
    const char* text = "Alice: !hillvue ping";
    strcpy((char*)data + 5, text);
    uint8_t metrics;
    bool busy;
    return bot.accept(PAYLOAD_TYPE_GRP_TXT, data, 5 + strlen(text), 1000,
                       false, metrics, busy, channel_index);
  };
  EXPECT_TRUE(accept(1, 0));
  EXPECT_TRUE(accept(1, 1));
  EXPECT_FALSE(accept(1, 0));
  EXPECT_FALSE(accept(1, 1));
  EXPECT_TRUE(accept(2, 0));
  EXPECT_TRUE(accept(2, 1));
  EXPECT_FALSE(accept(3, 0));
}

TEST(SensorChannels, HashCollisionReturnsBothKeysWithoutExceedingCapacity) {
  PublicChannelSensorBot public_only("");
  for (int i = 0; i < 4096; i++) {
    char name[32];
    snprintf(name, sizeof(name), "#collision%d", i);
    PublicChannelSensorBot bot(name);
    if (bot.channel().hash[0] != public_only.channel().hash[0]) continue;
    mesh::GroupChannel candidates[2];
    ASSERT_EQ(2, bot.findChannel(bot.channel().hash, candidates, 2));
    EXPECT_EQ(0, bot.channelIndex(candidates[0]));
    EXPECT_EQ(1, bot.channelIndex(candidates[1]));
    EXPECT_EQ(1, bot.findChannel(bot.channel().hash, candidates, 1));
    candidates[0].secret[0] ^= 1;
    EXPECT_EQ(-1, bot.channelIndex(candidates[0]));
    return;
  }
  FAIL() << "No collision found in deterministic test channels";
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
