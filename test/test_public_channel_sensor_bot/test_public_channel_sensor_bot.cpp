#include <gtest/gtest.h>
#include <cstring>
#include "../../examples/bme680_sensor/PublicChannelSensorBot.h"

static bool acceptCommand(PublicChannelSensorBot& bot, const char* command,
                          uint32_t serial, bool trace_busy, bool& busy_trace,
                          uint32_t now = 1000, uint8_t channel_index = 0) {
  uint8_t data[MAX_PACKET_PAYLOAD] = {};
  memcpy(data, &serial, sizeof(serial));
  strcpy(reinterpret_cast<char*>(&data[5]), command);
  uint8_t metric_mask = 0;
  return bot.accept(PAYLOAD_TYPE_GRP_TXT, data, 5 + strlen(command), now,
                    trace_busy, metric_mask, busy_trace, channel_index);
}

TEST(PublicChannelSensorBot, BusyTraceDoesNotUseCommandRateSlots) {
  PublicChannelSensorBot bot;
  bool busy_trace = false;
  for (uint32_t i = 1; i <= 8; i++) {
    EXPECT_TRUE(acceptCommand(bot, "Alice: !hillvue trace", i, true, busy_trace));
    EXPECT_TRUE(busy_trace);
  }
  for (uint32_t i = 9; i <= 12; i++) {
    EXPECT_TRUE(acceptCommand(bot, "Alice: !hillvue trace", i, false, busy_trace));
    EXPECT_FALSE(busy_trace);
  }
  EXPECT_FALSE(acceptCommand(bot, "Alice: !hillvue trace", 13, false, busy_trace));
}

TEST(PublicChannelSensorBot, ParsesMeasurementAliasesAndCombinations) {
  const char* commands[] = { "Alice: !hillvue", "Alice: !hillvue all",
    "Alice: !HILLVUE Tem H", "Alice: !hillvue voltage", "Alice: !hillvue ping",
    "Alice: !hillvue path", "Alice: !hillvue trace" };
  const uint8_t expected[] = { 31, 31, 3, 16, 32, 64, 128 };
  for (int i = 0; i < 7; i++) {
    PublicChannelSensorBot bot;
    uint8_t data[MAX_PACKET_PAYLOAD] = {};
    strcpy(reinterpret_cast<char*>(data + 5), commands[i]);
    uint8_t metrics = 0;
    bool busy = false;
    ASSERT_TRUE(bot.accept(PAYLOAD_TYPE_GRP_TXT, data, 5 + strlen(commands[i]),
                           100, false, metrics, busy));
    EXPECT_EQ(expected[i], metrics);
    EXPECT_FALSE(busy);
  }
}

TEST(PublicChannelSensorBot, ExtractsNormalizedRequesterLabelForTraceAcknowledgement) {
  PublicChannelSensorBot bot;
  uint8_t data[MAX_PACKET_PAYLOAD] = {};
  const char* command = "  Alice   Smith : !hillvue trace";
  strcpy(reinterpret_cast<char*>(&data[5]), command);
  uint8_t metrics = 0;
  bool busy = false, targeted = false;
  PublicChannelSensorBot::TraceAction action;
  char requester[32]{};
  ASSERT_TRUE(bot.accept(PAYLOAD_TYPE_GRP_TXT, data, 5 + strlen(command), 100,
                         false, metrics, busy, 0, nullptr, &targeted, &action,
                         requester, sizeof(requester)));
  EXPECT_EQ(PublicChannelSensorBot::REQUEST_TRACE, metrics);
  EXPECT_FALSE(targeted);
  EXPECT_EQ(PublicChannelSensorBot::TRACE_START, action);
  EXPECT_STREQ("Alice Smith", requester);
}

TEST(PublicChannelSensorBot, RejectsInvalidCommandsAndDuplicates) {
  for (const char* command : { "Alice: !hillvuex", "Alice: !hillvue nonsense",
       "Alice: !hillvue ping t", "Alice: !hillvue path air", "!hillvue" }) {
    PublicChannelSensorBot bot;
    bool busy;
    EXPECT_FALSE(acceptCommand(bot, command, 1, false, busy));
  }
  PublicChannelSensorBot bot;
  bool busy;
  EXPECT_TRUE(acceptCommand(bot, "Alice: !hillvue t", 1, false, busy));
  EXPECT_FALSE(acceptCommand(bot, "Alice: !hillvue t", 1, false, busy));
}

TEST(PublicChannelSensorBot, RollingMinuteIsSharedAcrossUsersAndHandlesWraparound) {
  for (uint32_t start : { 1000u, UINT32_MAX - 1000u }) {
    PublicChannelSensorBot bot;
    bool busy;
    for (uint32_t i = 1; i <= 4; i++)
      EXPECT_TRUE(acceptCommand(bot, i % 2 ? "Alice: !hillvue t" : "Bob: !hillvue h",
                                i, false, busy, start));
    EXPECT_FALSE(acceptCommand(bot, "Carol: !hillvue", 5, false, busy, start + 59999));
    EXPECT_TRUE(acceptCommand(bot, "Carol: !hillvue", 6, false, busy, start + 60000));
  }
}

TEST(PublicChannelSensorBot, PublicOnlyLookupDoesNotReturnDuplicateCandidates) {
  PublicChannelSensorBot bot("");
  mesh::GroupChannel candidates[2];
  EXPECT_EQ(1, bot.findChannel(bot.channel().hash, candidates, 2));
  EXPECT_EQ(0, bot.channelIndex(bot.channel()));
  EXPECT_EQ(0, bot.findChannel(bot.channel().hash, candidates, 0));
  EXPECT_EQ(0, bot.findChannel(nullptr, candidates, 2));
}

TEST(PublicChannelSensorBot, ActiveCancelBypassesExhaustedSlotsButIdleCancelAndStatusDoNot) {
  PublicChannelSensorBot bot;
  bool busy;
  for (uint32_t i = 1; i <= 4; i++) EXPECT_TRUE(acceptCommand(bot, "Alice: !hillvue ping", i, false, busy));
  EXPECT_FALSE(acceptCommand(bot, "Alice: !hillvue trace status", 5, true, busy));
  EXPECT_TRUE(acceptCommand(bot, "Alice: !hillvue trace cancel", 6, true, busy));
  EXPECT_FALSE(busy);
  EXPECT_FALSE(acceptCommand(bot, "Alice: !hillvue trace cancel", 7, false, busy));
  EXPECT_FALSE(acceptCommand(bot, "Alice: !hillvue trace cancel", 6, true, busy)); // duplicate
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
