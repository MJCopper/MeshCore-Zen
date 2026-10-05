#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetBatteryPolicy.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetEngine.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetAssets.h"
#include "PetCareSimulation.h"
using zen::pet::Engine;
TEST(Pet, DisabledFrozenAndRebootCreatesStarter) {
  Engine e;
  e.update(0,false,false,false); e.update(1000,true,false,false);
  EXPECT_EQ(Engine::OK,e.train());
  e.update(1001,false,false,false); auto energy=e.state().energy;
  e.update(10000000,false,false,false); e.update(10000001,true,false,false);
  EXPECT_EQ(energy,e.state().energy); EXPECT_EQ(20,e.state().xp);
  Engine reboot; reboot.update(0,true,false,false);
  EXPECT_EQ(0,reboot.state().xp); EXPECT_EQ(100,reboot.state().energy);
}
TEST(Pet, SleepLowPowerCooldownAndFractionalRecovery) {
  Engine e; e.update(0,true,false,false); e.train();
  EXPECT_EQ(Engine::COOLDOWN,e.train()); e.update(1,true,true,false);
  EXPECT_EQ(Engine::SLEEPING,e.feed()); auto full=e.state().fullness;
  for(uint32_t t=1001;t<=3600001;t+=1000) e.update(t,true,true,false);
  EXPECT_EQ(100,e.state().energy); EXPECT_EQ(full,e.state().fullness);
  e.update(3600002,true,false,true); EXPECT_EQ(Engine::SUSPENDED,e.train());
}
TEST(Pet, AllOneHundredEightyNineFormsReachableAndEvolutionIsExplicit) {
  bool visited[zen::pet::Evolution::FORMS] = {};
  for(int path=0;path<64;++path) {
    Engine e; uint32_t t=0; e.update(t,true,false,false);
    uint8_t lineage=0;
    visited[0]=true;
    for(uint8_t level=1;level<zen::pet::Evolution::LEVELS;++level) {
      const uint8_t previous=e.state().form;
      for(int i=0;i<300 && !e.ready();++i) {
        t+=14400000UL; e.update(t,true,false,false); e.feed(); e.train();
      }
      ASSERT_TRUE(e.ready()); EXPECT_EQ(previous,e.state().form);
      EXPECT_EQ((level & 1)?2:1,e.choices());
      EXPECT_EQ(Engine::NOT_READY,e.evolve(e.choices()));
      uint8_t choice=(level & 1)?(path >> (5-level/2)) & 1:0;
      if(level & 1) lineage=lineage*2+choice;
      EXPECT_EQ(Engine::OK,e.evolve(choice));
      EXPECT_EQ(level+1,e.level());
      EXPECT_EQ(zen::pet::Evolution::offset(level+1)+lineage,e.state().form);
      visited[e.state().form]=true;
    }
    EXPECT_EQ(125+path,e.state().form); EXPECT_FALSE(e.ready());
    EXPECT_EQ(0,e.choices()); EXPECT_EQ(Engine::NOT_READY,e.evolve(0));
    for(int i=0;i<10;++i) {
      t+=14400000UL; e.update(t,true,false,false); e.feed(); e.train();
    }
    EXPECT_EQ(zen::pet::Evolution::xp(zen::pet::Evolution::LEVELS),e.state().xp);
  }
  for(bool reached:visited) EXPECT_TRUE(reached);
}
TEST(Pet, WrapAndFeedingLimits) {
  Engine e; e.update(0xfffffff0UL,true,false,false);
  EXPECT_EQ(Engine::OK,e.feed()); EXPECT_EQ(Engine::FULL,e.feed());
  e.update(400000,true,false,false); EXPECT_EQ(95,e.state().fullness);
  for(int i=0;i<zen::pet::Evolution::FORMS;++i) {
    EXPECT_LE(zen::pet::form(i).size,32);
    EXPECT_GE(zen::pet::form(i).size,16);
    EXPECT_NE(nullptr,zen::pet::form(i).name);
    EXPECT_NE(nullptr,zen::pet::form(i).silhouette);
  }
}
TEST(Pet, ProgressionIsNonLinearAndFormsAreComplete) {
  using zen::pet::Evolution;
  uint16_t previous=0, previous_gap=0;
  uint8_t total=0;
  for(uint8_t level=1;level<=Evolution::LEVELS;++level) {
    total+=Evolution::count(level);
    if(level<Evolution::LEVELS) {
      uint16_t gap=Evolution::xp(level)-previous;
      // The starter includes its initial resources; later gaps increase.
      if(level>2) EXPECT_GT(gap,previous_gap);
      previous=Evolution::xp(level); previous_gap=gap;
    }
    for(uint8_t index=0;index<Evolution::count(level);++index) {
      uint8_t id=Evolution::offset(level)+index;
      EXPECT_EQ(level,Evolution::level(id));
      for(uint8_t other=0;other<id;++other)
        EXPECT_STRNE(zen::pet::form(id).name,zen::pet::form(other).name);
    }
  }
  EXPECT_EQ(Evolution::FORMS,total);
}
TEST(Pet, GoodCareWithoutBonusesReachesLevelTwelveAroundSixtyDays) {
  PetCareSimulation::Policy policy;
  auto result=PetCareSimulation::run(policy,70);
  ASSERT_EQ(12,result.level); EXPECT_EQ(431u,result.wins);
  const double targets[]={0,.25,.75,1.5,3,5.5,9.5,15.5,23.5,33.5,45.5,60};
  for(uint8_t level=1;level<12;++level)
    EXPECT_NEAR(targets[level],double(result.reached[level])/1440,.5) << unsigned(level+1);
  EXPECT_NEAR(60,double(result.reached[11])/1440,.05);
  EXPECT_GT(result.reached[11],uint64_t(UINT32_MAX)/60000); // Crosses millis wrap.
}
TEST(Pet, BonusesAccelerateProgressAndMissedCareOrLowPowerExtendIt) {
  PetCareSimulation::Policy policy;
  auto baseline=PetCareSimulation::run(policy);
  policy.bonuses=true; auto bonus=PetCareSimulation::run(policy);
  EXPECT_EQ(12,bonus.level); EXPECT_LT(bonus.reached[11],baseline.reached[11]);
  policy.bonuses=false; policy.sleep=false; auto awake=PetCareSimulation::run(policy);
  EXPECT_EQ(12,awake.level); EXPECT_GT(awake.reached[11],baseline.reached[11]);
  policy.sleep=true; policy.low_power=true; auto paused=PetCareSimulation::run(policy);
  EXPECT_EQ(12,paused.level); EXPECT_GT(paused.reached[11],baseline.reached[11]);
  policy.low_power=false; policy.daily_training_limit=2;
  auto occasional=PetCareSimulation::run(policy);
  EXPECT_EQ(12,occasional.level); EXPECT_GT(occasional.reached[11],baseline.reached[11]);
}
TEST(Pet, MatureAssetsHaveUniquePortraitsNamesAndNonemptyPreviews) {
  for(uint8_t id=93;id<zen::pet::Evolution::FORMS;++id) {
    const auto& asset=zen::pet::form(id);
    ASSERT_EQ(16,asset.source_size); bool visible=false;
    for(uint8_t row=0;row<8;++row) visible|=asset.preview[row]!=0;
    EXPECT_TRUE(visible); EXPECT_LE(strlen(asset.name),11u);
    for(uint8_t other=93;other<id;++other)
      EXPECT_NE(0,memcmp(asset.silhouette,zen::pet::form(other).silhouette,32));
  }
  EXPECT_STREQ("Oakwarden",zen::pet::form(93).name);
  EXPECT_STREQ("Oaksage",zen::pet::form(94).name);
  EXPECT_STREQ("Oaktitan",zen::pet::form(125).name);
  EXPECT_STREQ("Oakcrown",zen::pet::form(126).name);
  EXPECT_STREQ("Oakoracle",zen::pet::form(127).name);
  EXPECT_STREQ("Oakastral",zen::pet::form(128).name);
}
TEST(Pet, BatteryBandsHysteresisAndUnavailableReadings) {
  zen::pet::PetBatteryPolicy p;
  EXPECT_EQ(5,p.update(-1)); EXPECT_EQ(5,p.update(50));
  EXPECT_EQ(6,p.update(49)); EXPECT_EQ(6,p.update(51)); EXPECT_EQ(5,p.update(52));
  EXPECT_EQ(7,p.update(24)); EXPECT_EQ(7,p.update(26)); EXPECT_EQ(6,p.update(27));
  EXPECT_EQ(8,p.update(9)); EXPECT_EQ(8,p.update(11)); EXPECT_EQ(7,p.update(12));
  EXPECT_EQ(5,p.update(100)); EXPECT_EQ(8,p.update(0));
  EXPECT_EQ(5,p.update(101)); EXPECT_EQ(7,p.update(25-1));
  EXPECT_EQ(5,p.update(-1)); EXPECT_EQ(5,p.update(50));
}
TEST(Pet, HungerRateIsProspectiveAndSleepAndLowPowerPauseIt) {
  Engine e; e.update(0,true,false,false,5);
  e.update(3600000,true,false,false,8); EXPECT_EQ(65,e.state().fullness);
  e.update(7200000,true,true,false,8); EXPECT_EQ(57,e.state().fullness);
  e.update(10800000,true,false,true,8); EXPECT_EQ(57,e.state().fullness);
  e.update(14400000,true,false,false,5); EXPECT_EQ(57,e.state().fullness);
  e.update(18000000,true,false,false,5); EXPECT_EQ(52,e.state().fullness);
  EXPECT_EQ(Engine::OK,e.feed()); EXPECT_EQ(77,e.state().fullness);
}
TEST(Pet, AllBatteryRatesRemainPlayableButDelayEvolution) {
  double previous=0;
  for(uint8_t rate=5;rate<=8;++rate) {
    PetCareSimulation::Policy policy; policy.hunger_rate=rate;
    auto result=PetCareSimulation::run(policy);
    ASSERT_EQ(12,result.level);
    double days=result.reached[11]/1440.0;
    EXPECT_GT(days,previous); previous=days;
    RecordProperty((std::string("hunger_")+std::to_string(rate)).c_str(),days);
  }
}
TEST(Pet, TrainingRestPausesAndExpiresAcrossUptimeWrap) {
  Engine e; uint32_t now=UINT32_MAX-100;
  e.update(now,true,false,false); ASSERT_EQ(Engine::OK,e.train());
  EXPECT_EQ(300000u,e.trainingRestMillis());
  e.update(now+=60000,true,false,false); EXPECT_EQ(240000u,e.trainingRestMillis());
  e.update(now,true,false,true);
  e.update(now+=60000,true,false,false); EXPECT_EQ(240000u,e.trainingRestMillis());
  e.update(now,false,false,false);
  e.update(now+=60000,true,false,false); EXPECT_EQ(240000u,e.trainingRestMillis());
  e.update(now+=239999,true,false,false); EXPECT_EQ(1u,e.trainingRestMillis());
  e.update(now+=1,true,false,false); EXPECT_EQ(0u,e.trainingRestMillis());
  EXPECT_EQ(Engine::OK,e.trainingAvailable());
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
