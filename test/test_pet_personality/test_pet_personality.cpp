#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetPersonality.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetSleep.h"

using P=zen::pet::PetPersonality;

TEST(PetPersonality, TemperamentsAreSeededIndependentlyAndSurviveOffOn) {
  unsigned counts[4]={};
  for(unsigned seed=1;seed<=4096;++seed) {
    P a,b; P::Context c; a.update(0,c,seed); b.update(0,c,seed);
    auto born=a.temperament(); EXPECT_EQ(born,b.temperament()); ++counts[born];
    c.ready=true; a.update(10,c); EXPECT_EQ(born,a.temperament());
    c.enabled=false; a.update(20,c);
    c.enabled=true; a.update(30,c,seed+1); EXPECT_EQ(born,a.temperament());
  }
  for(unsigned count:counts) { EXPECT_GT(count,900u); EXPECT_LT(count,1150u); }
}
TEST(PetPersonality, ExpressionsHaveBoundedLifetimeAndSleepOverridesThem) {
  P pet; P::Context c; pet.update(0,c,42);
  pet.event(P::FED,0); EXPECT_EQ(P::HAPPY,pet.presentation(0).pose);
  EXPECT_NE(nullptr,pet.presentation(0).phrase);
  pet.update(4000,c); EXPECT_EQ(P::NEUTRAL,pet.presentation(4000).pose);
  pet.event(P::WON,4000); EXPECT_EQ(P::PROUD,pet.presentation(4000).pose);
  pet.event(P::LOST,5000); EXPECT_EQ(P::SULKING,pet.presentation(5000).pose);
  c.ready=true; pet.update(9000,c); EXPECT_EQ(P::EXCITED,pet.presentation(9000).pose);
  c.sleeping=true; pet.update(9001,c); EXPECT_EQ(P::SLEEP,pet.presentation(9001).pose);
  EXPECT_EQ(nullptr,pet.presentation(9001).phrase);
  c.paused=true; pet.update(9002,c); EXPECT_EQ(P::REST,pet.presentation(9002).pose);
  pet.event(P::FED,9002); EXPECT_EQ(P::REST,pet.presentation(9002).pose);
  c.sleeping=c.paused=c.ready=false; c.bedtime=true; pet.update(9003,c);
  EXPECT_EQ(P::SLEEPY,pet.presentation(9003).pose);
}
TEST(PetPersonality, BubblesRespectSpacingSuppressionAndDoNotWaitBehindMenus) {
  P pet; P::Context c; pet.update(0,c,91); pet.event(P::FED,0);
  std::string first=pet.presentation(0).phrase;
  pet.update(4000,c); pet.event(P::FED,29999);
  EXPECT_EQ(nullptr,pet.presentation(29999).phrase);
  pet.event(P::FED,30000); ASSERT_NE(nullptr,pet.presentation(30000).phrase);
  EXPECT_NE(first,pet.presentation(30000).phrase);
  c.ordinary=false; pet.update(30001,c); EXPECT_EQ(nullptr,pet.presentation(30001).phrase);
  c.ordinary=true; c.unobscured=false; pet.update(30002,c);
  EXPECT_EQ(nullptr,pet.presentation(30002).phrase);
  c.unobscured=true; pet.update(34000,c); EXPECT_EQ(nullptr,pet.presentation(34000).phrase);
}
TEST(PetPersonality, GreetingRequiresFiveMinutesAwayAndIsNotMenuNavigation) {
  P pet; P::Context c; pet.update(0,c,23);
  c.ordinary=false; pet.update(10,c); pet.update(400000,c);
  c.ordinary=true; pet.update(400001,c); EXPECT_EQ(nullptr,pet.presentation(400001).phrase);
  pet.hide(400002); c.page_visible=false; pet.update(400002,c);
  c.page_visible=true; pet.update(699999,c); EXPECT_EQ(nullptr,pet.presentation(699999).phrase);
  pet.hide(700000); c.page_visible=false; pet.update(700000,c);
  c.page_visible=true; pet.update(1000000,c);
  ASSERT_NE(nullptr,pet.presentation(1000000).phrase);
  EXPECT_EQ(P::NEUTRAL,pet.presentation(1000000).pose);
}
TEST(PetPersonality, BatteryRecoveryNeedsPowerDistinctSamplesAndSustainedRise) {
  P pet; P::Context c; c.battery_percent=50; pet.update(0,c,7);
  c.battery_percent=80; c.battery_sample=1; pet.update(120000,c);
  EXPECT_EQ(P::NEUTRAL,pet.presentation(120000).pose); // Voltage rebound without power.
  c.external_power=true; c.battery_percent=50; c.battery_sample=2; pet.update(130000,c);
  c.battery_percent=53; c.battery_sample=3; pet.update(140000,c);
  pet.update(300000,c); EXPECT_EQ(P::NEUTRAL,pet.presentation(300000).pose); // Same sample.
  c.battery_sample=4; c.battery_percent=52; pet.update(300001,c); // Noise resets confirmation.
  c.battery_sample=5; c.battery_percent=53; pet.update(300002,c);
  c.battery_sample=6; pet.update(420001,c); EXPECT_EQ(P::NEUTRAL,pet.presentation(420001).pose);
  c.battery_sample=7; pet.update(420002,c);
  EXPECT_TRUE(pet.presentation(420002).pose==P::HAPPY || pet.presentation(420002).pose==P::RELAXED);
  c.battery_sample=8; c.battery_percent=63; pet.update(430000,c);
  c.battery_sample=9; pet.update(1019999,c); EXPECT_EQ(P::NEUTRAL,pet.presentation(1019999).pose);
  c.battery_sample=10; pet.update(1020002,c);
  EXPECT_TRUE(pet.presentation(1020002).pose==P::HAPPY || pet.presentation(1020002).pose==P::RELAXED);
  c.battery_sample=11; c.external_power=false; pet.update(1020003,c);
  c.battery_sample=12; c.external_power=true; c.battery_percent=99; pet.update(1020004,c);
  c.battery_sample=13; c.battery_percent=100; pet.update(1500000,c);
  EXPECT_EQ(P::NEUTRAL,pet.presentation(1500000).pose); // Full battery cannot rise three points.
}
TEST(PetPersonality, UnknownBatteryAndHiddenRecoveryCannotReplayLater) {
  P pet; P::Context c; c.external_power=true; c.battery_percent=-1;
  pet.update(0,c); c.battery_sample=1; c.battery_percent=50; pet.update(10,c);
  c.battery_sample=2; c.battery_percent=53; pet.update(20,c);
  c.page_visible=false; c.battery_sample=3; pet.update(120020,c);
  c.page_visible=true; pet.update(120021,c);
  EXPECT_EQ(P::NEUTRAL,pet.presentation(120021).pose);
  EXPECT_EQ(nullptr,pet.presentation(120021).phrase);
}
TEST(PetPersonality, QuirksOnlyAdvanceWhileVisibleAndEinkRedrawsAreBounded) {
  P pet; P::Context c; c.slow=true; pet.update(0,c,17);
  EXPECT_TRUE(pet.takeRedraw(0,true)); pet.event(P::FED,1);
  EXPECT_FALSE(pet.takeRedraw(1,true)); EXPECT_TRUE(pet.takeRedraw(5000,true));
  bool quirk=false;
  for(uint32_t t=5000;t<=60000;t+=1000) {
    pet.update(t,c); quirk|=pet.presentation(t).quirk!=P::NONE;
  }
  EXPECT_TRUE(quirk); c.page_visible=false; pet.update(60001,c);
  pet.update(3600000,c); EXPECT_EQ(P::NONE,pet.presentation(3600000).quirk);
  EXPECT_FALSE(pet.takeRedraw(3600000,true));
  c.page_visible=true; pet.update(3600001,c);
  EXPECT_EQ(P::NONE,pet.presentation(3600001).quirk);
}
TEST(PetPersonality, ReactionsAndRecoveryWorkAcrossUptimeWrap) {
  P pet; P::Context c; uint32_t start=UINT32_MAX-100;
  pet.update(start,c,20); pet.event(P::FED,start);
  pet.update(start+3999,c); EXPECT_EQ(P::HAPPY,pet.presentation(start+3999).pose);
  pet.update(start+4000,c); EXPECT_EQ(P::NEUTRAL,pet.presentation(start+4000).pose);
  pet.hide(start+4000); c.page_visible=false; pet.update(start+4000,c);
  c.page_visible=true; pet.update(start+304000,c);
  EXPECT_NE(nullptr,pet.presentation(start+304000).phrase);
}
TEST(PetPersonality, AmbientRecoveryCannotReplaceCareAndHideSuppressesRedraw) {
  P pet; P::Context c; pet.update(0,c,50); pet.event(P::WON,0);
  pet.event(P::RECOVERY,1); EXPECT_EQ(P::PROUD,pet.presentation(1).pose);
  pet.event(P::LOST,2); EXPECT_EQ(P::SULKING,pet.presentation(2).pose);
  EXPECT_EQ(nullptr,pet.presentation(2).phrase); // No stale winning phrase.
  pet.hide(3); EXPECT_FALSE(pet.takeRedraw(3,false));
  EXPECT_FALSE(pet.presentation(3).active);
}
TEST(PetPersonality, BedtimeUsesLocalScheduleAndWakeOverrideIsSeparate) {
  using zen::pet::PetSleep;
  EXPECT_TRUE(PetSleep::nearBedtime(true,21*3600+45*60,22*60,6*60));
  EXPECT_FALSE(PetSleep::nearBedtime(true,22*3600,22*60,6*60));
  EXPECT_FALSE(PetSleep::nearBedtime(false,21*3600+45*60,22*60,6*60));
  EXPECT_FALSE(PetSleep::nearBedtime(true,21*3600+45*60,22*60,22*60));
  EXPECT_TRUE(PetSleep::nearBedtime(true,23*3600+55*60,5,60));
  EXPECT_TRUE(PetSleep::nearBedtime(true,-5*60,5,60));
  PetSleep sleep; sleep.sleeping(0,true,true); sleep.wake(0);
  EXPECT_FALSE(sleep.sleeping(1,true,true)); EXPECT_TRUE(sleep.wakeActive());
  EXPECT_TRUE(sleep.sleeping(1800000,true,true)); EXPECT_FALSE(sleep.wakeActive());
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
