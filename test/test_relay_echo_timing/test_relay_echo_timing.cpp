#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/RelayEchoTiming.h"

TEST(RelayEchoTiming, AllowsAirtimeScaledRepeatDelay) {
  EXPECT_EQ(10000u, solo::RelayEchoTiming::echoWindow(500));
  EXPECT_EQ(16000u, solo::RelayEchoTiming::echoWindow(4000));
  EXPECT_EQ(30000u, solo::RelayEchoTiming::echoWindow(7500));
  EXPECT_EQ(30000u, solo::RelayEchoTiming::echoWindow(UINT32_MAX));
}

TEST(RelayEchoTiming, WarnsOnlyForAnActualUnheardTransmission) {
  EXPECT_TRUE(solo::RelayEchoTiming::noRelayHeard(true, true, 0));
  EXPECT_FALSE(solo::RelayEchoTiming::noRelayHeard(true, true, 1));
  EXPECT_FALSE(solo::RelayEchoTiming::noRelayHeard(true, false, 0));
  EXPECT_FALSE(solo::RelayEchoTiming::noRelayHeard(false, true, 0));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
