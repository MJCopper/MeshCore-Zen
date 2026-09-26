#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterTiming.h"

TEST(RepeaterTiming, MatchesStandardRepeaterDefaults) {
  EXPECT_FLOAT_EQ(zen::RepeaterTiming::RX_DELAY_BASE, 10.0f);
  EXPECT_FLOAT_EQ(zen::RepeaterTiming::FLOOD_TX_FACTOR, 0.5f);
  EXPECT_FLOAT_EQ(zen::RepeaterTiming::DIRECT_TX_FACTOR, 0.3f);
  EXPECT_EQ(zen::RepeaterTiming::YIELD_MULTIPLIER, 2);
}

TEST(RepeaterTiming, ScalesAirtimeIntoDelayWindow) {
  EXPECT_EQ(zen::RepeaterTiming::delayWindow(1000, 0.5f), 500u);
  EXPECT_EQ(zen::RepeaterTiming::delayWindow(1000, 0.3f), 300u);
}


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
