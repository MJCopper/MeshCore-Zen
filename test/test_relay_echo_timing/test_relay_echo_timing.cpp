#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/RelayEchoTiming.h"

TEST(RelayEchoTiming, AllowsAirtimeScaledRepeatDelay) {
  EXPECT_EQ(10000u, zen::RelayEchoTiming::echoWindow(500));
  EXPECT_EQ(16000u, zen::RelayEchoTiming::echoWindow(4000));
  EXPECT_EQ(30000u, zen::RelayEchoTiming::echoWindow(7500));
  EXPECT_EQ(30000u, zen::RelayEchoTiming::echoWindow(UINT32_MAX));
}

TEST(RelayEchoTiming, WarnsOnlyForAnActualUnheardTransmission) {
  EXPECT_TRUE(zen::RelayEchoTiming::noRelayHeard(true, true, 0));
  EXPECT_FALSE(zen::RelayEchoTiming::noRelayHeard(true, true, 1));
  EXPECT_FALSE(zen::RelayEchoTiming::noRelayHeard(true, false, 0));
  EXPECT_FALSE(zen::RelayEchoTiming::noRelayHeard(false, true, 0));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
