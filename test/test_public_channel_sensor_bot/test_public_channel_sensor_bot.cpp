#include <gtest/gtest.h>
#include <cstring>
#include "../../examples/simple_sensor/PublicChannelSensorBot.h"

static bool acceptCommand(PublicChannelSensorBot& bot, const char* command,
                          uint32_t serial, bool trace_busy, bool& busy_trace) {
  uint8_t data[MAX_PACKET_PAYLOAD] = {};
  memcpy(data, &serial, sizeof(serial));
  strcpy(reinterpret_cast<char*>(&data[5]), command);
  uint8_t metric_mask = 0;
  return bot.accept(PAYLOAD_TYPE_GRP_TXT, data, 5 + strlen(command), 1000,
                    trace_busy, metric_mask, busy_trace);
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
