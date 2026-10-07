#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetGameDisplay.h"

using Display=zen::pet::PetGameDisplay;

TEST(PetGameDisplay, PlayerIdleIsBoundedAndInputExtendsIt) {
  Display d;
  EXPECT_TRUE(d.update(100,true,false,true).hold);
  EXPECT_TRUE(d.update(60099,true,false,true).hold);
  EXPECT_TRUE(d.update(60100,true,false,true).expired);
  d.input(60101);
  EXPECT_TRUE(d.update(60101,true,false,true).hold);
  EXPECT_TRUE(d.update(120101,true,false,true).expired);
}
TEST(PetGameDisplay, WatchingDoesNotConsumePlayerAllowance) {
  Display d; d.update(0,true,false,true);
  EXPECT_TRUE(d.update(59000,true,true,true).hold);
  EXPECT_TRUE(d.update(90000,true,true,true).hold);
  EXPECT_TRUE(d.update(100000,true,false,true).hold);
  EXPECT_TRUE(d.update(159999,true,false,true).hold);
  EXPECT_TRUE(d.update(160000,true,false,true).expired);
}
TEST(PetGameDisplay, AnimationUpdatesDoNotExtendInputTimeout) {
  Display d;
  for(unsigned now=0;now<60000;now+=100)EXPECT_TRUE(d.update(now,true,false,true).hold);
  EXPECT_TRUE(d.update(60000,true,false,true).expired);
}
TEST(PetGameDisplay, ResultsReleaseOnceAfterExactlyFiveSeconds) {
  Display d; d.update(0,true,false,true); d.result(100);
  EXPECT_TRUE(d.update(100,false,false,true).hold);
  EXPECT_TRUE(d.update(5099,false,false,true).hold);
  auto end=d.update(5100,false,false,true);
  EXPECT_FALSE(end.hold); EXPECT_TRUE(end.released); EXPECT_FALSE(end.expired);
  EXPECT_FALSE(d.update(5101,false,false,true).released);
}
TEST(PetGameDisplay, CancellationVisibilityAndSafetyOverrideLeaseAndGrace) {
  for(bool result:{false,true}) {
    Display d; d.update(0,true,false,true); if(result)d.result(1);
    auto off=d.update(2,!result,false,false);
    EXPECT_FALSE(off.hold); EXPECT_FALSE(off.expired);
    EXPECT_FALSE(off.released);
    EXPECT_FALSE(d.update(3,false,false,true).hold);
    d.update(4,true,false,true); d.cancel();
    EXPECT_TRUE(d.update(5,false,false,true).released);
  }
}
TEST(PetGameDisplay, RetryStartsFreshAndUptimeWrapIsSafe) {
  Display d; uint32_t start=0xfffffff0u;
  d.update(start,true,false,true);
  EXPECT_TRUE(d.update(start+60000,true,false,true).expired);
  d.result(start+60001);
  EXPECT_TRUE(d.update(start+65000,false,false,true).hold);
  EXPECT_TRUE(d.update(start+65001,false,false,true).released);
  d.update(start+65002,true,false,true);
  EXPECT_TRUE(d.update(start+125001,true,false,true).hold);
  EXPECT_TRUE(d.update(start+125002,true,false,true).expired);
}
TEST(PetGameDisplay, IdlePagesNeverAcquireLease) {
  Display d;
  EXPECT_FALSE(d.update(0,false,false,true).hold);
  EXPECT_FALSE(d.update(1000000,false,false,true).hold);
}

int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
