#include <gtest/gtest.h>
#include <string.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetNotifications.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationProfiles.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetGameAudio.h"
using namespace zen::pet;

static void observe(PetNotifications& n, uint8_t fullness=70, bool ready=false,
                    bool sleep=false, bool paused=false, bool enabled=true, uint8_t level=1) {
  State s; s.fullness=fullness;
  n.observe(s,level,ready,enabled,sleep,paused);
}
TEST(PetNotifications, HealthyStartupIsSilent) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  EXPECT_FALSE(n.take(0,a));
}
TEST(PetNotifications, TrainingDisplaysDuringQuietTimeWithoutSound) {
  auto event = zen::NotificationProfiles::event(zen::NotificationType::PET_TRAINING);
  zen::NotificationContext context; context.quiet_time = true;
  auto result = zen::NotificationCoordinator::decide(event,context);
  EXPECT_TRUE(result.show_visual); EXPECT_FALSE(result.play_sound);
  context.silent = true; context.mode = zen::NotificationPolicy::OFF;
  result = zen::NotificationCoordinator::decide(event,context);
  EXPECT_TRUE(result.show_visual); EXPECT_FALSE(result.play_sound);
  EXPECT_FALSE(result.wake_screen); EXPECT_FALSE(result.record_unread);
  context.low_power = true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).show_visual);
}
TEST(PetNotifications, QuietTrainingDeliveryDoesNotConsumeDeferredBackgroundAlerts) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  n.reward(20,0); n.training(true);
  ASSERT_TRUE(n.take(0,a,true)); EXPECT_STREQ("Won!",a.text);
  EXPECT_FALSE(n.take(5000,a,true));
  ASSERT_TRUE(n.take(5000,a)); EXPECT_EQ(PetNotifications::REWARD,a.sound);
}
TEST(PetNotifications, TrainingResultsHaveExactLabelsAndPriority) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  n.reward(20,0); n.training(false); n.training(true);
  ASSERT_TRUE(n.take(0,a,true)); EXPECT_STREQ("Lost!",a.text);
  EXPECT_EQ(PetNotifications::TRAIN_LOST,a.sound);
  EXPECT_FALSE(n.take(1,a,true));
  ASSERT_TRUE(n.take(5000,a,true)); EXPECT_STREQ("Won!",a.text);
  EXPECT_EQ(PetNotifications::TRAIN_WON,a.sound);
  EXPECT_FALSE(n.take(10000,a,true));
  ASSERT_TRUE(n.take(10000,a)); EXPECT_EQ(PetNotifications::REWARD,a.sound);
  n.training(false); observe(n,70,false,false,false,false); observe(n);
  EXPECT_FALSE(n.take(15000,a));
}
TEST(PetNotifications, HungerThresholdsLatchUntilFedAboveForty) {
  PetNotifications n; PetNotifications::Alert a;
  observe(n,26); EXPECT_FALSE(n.take(0,a));
  observe(n,25); ASSERT_TRUE(n.take(0,a));
  EXPECT_EQ(PetNotifications::HUNGRY,a.sound); EXPECT_STREQ("Pet: Hungry",a.text);
  observe(n,20); EXPECT_FALSE(n.take(5000,a));
  observe(n,10); ASSERT_TRUE(n.take(5000,a));
  EXPECT_EQ(PetNotifications::VERY_HUNGRY,a.sound);
  observe(n,40); observe(n,25); EXPECT_FALSE(n.take(10000,a));
  observe(n,41); observe(n,25); EXPECT_TRUE(n.take(10000,a));
}
TEST(PetNotifications, SevereHungerSupersedesPendingHunger) {
  PetNotifications n; PetNotifications::Alert a;
  observe(n,25,false,true); observe(n,10,false,true);
  EXPECT_FALSE(n.take(0,a)); observe(n,10);
  ASSERT_TRUE(n.take(1,a)); EXPECT_STREQ("Pet: Very hungry",a.text);
  EXPECT_FALSE(n.take(6000,a));
}
TEST(PetNotifications, StaleHungerIsCancelledBeforePresentation) {
  PetNotifications n; PetNotifications::Alert a;
  observe(n,25,false,true); observe(n,50); EXPECT_FALSE(n.take(0,a));
  observe(n,10,false,true); observe(n,35); EXPECT_FALSE(n.take(1,a));
}
TEST(PetNotifications, RewardAmountsCombineAndZeroIsSilent) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  n.reward(0,0); EXPECT_FALSE(n.take(0,a));
  n.reward(20,3); n.reward(15,2); ASSERT_TRUE(n.take(0,a));
  EXPECT_EQ(PetNotifications::REWARD,a.sound); EXPECT_STREQ("Pet: +35 XP, +5 Bond",a.text);
  EXPECT_FALSE(n.take(5000,a));
  n.reward(0,2); ASSERT_TRUE(n.take(5000,a)); EXPECT_STREQ("Pet: +2 Bond",a.text);
}
TEST(PetNotifications, ReadyCombinesWithRewardAndOnlyOccursOncePerLevel) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  n.reward(20,0); observe(n,70,true); ASSERT_TRUE(n.take(0,a));
  EXPECT_EQ(PetNotifications::READY,a.sound);
  EXPECT_STREQ("Pet: +20 XP; Evolve ready",a.text);
  observe(n,70,true); EXPECT_FALSE(n.take(5000,a));
  observe(n,70,true,false,false,true,2); ASSERT_TRUE(n.take(5000,a));
  EXPECT_STREQ("Pet: Ready to evolve",a.text);
}
TEST(PetNotifications, FinalStageReadinessUsesRetirementWording) {
  PetNotifications n; PetNotifications::Alert a;
  observe(n,70,true,false,false,true,12); ASSERT_TRUE(n.take(0,a));
  EXPECT_STREQ("Pet: Ready to retire",a.text);
  PetNotifications bonus; observe(bonus,70,false,false,false,true,12);
  bonus.reward(20,1); observe(bonus,70,true,false,false,true,12);
  ASSERT_TRUE(bonus.take(0,a)); EXPECT_STREQ("Pet: +20 XP, +1 Bond; Retire ready",a.text);
}
TEST(PetNotifications, SleepDefersAndLowPowerSuspends) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  n.reward(20,0); observe(n,70,true,true); EXPECT_FALSE(n.take(0,a));
  observe(n,70,true,false,true); EXPECT_FALSE(n.take(1,a));
  observe(n,70,true); EXPECT_TRUE(n.take(2,a));
}
TEST(PetNotifications, DisabledClearsPendingWithoutRepeatingReadiness) {
  PetNotifications n; PetNotifications::Alert a;
  observe(n,70,true); n.reward(20,0);
  observe(n,70,true,false,false,false);
  observe(n,70,true); EXPECT_FALSE(n.take(0,a));
}
TEST(PetNotifications, EvolutionCancelsObsoleteReadyAlertAndQueuesEachForm) {
  PetNotifications n; PetNotifications::Alert a; observe(n,70,true);
  n.evolved(1); observe(n,70,false,false,false,true,2);
  n.evolved(3); observe(n,70,false,false,false,true,3);
  ASSERT_TRUE(n.take(0,a)); EXPECT_EQ(PetNotifications::EVOLVED,a.sound);
  EXPECT_STREQ("Pet: Evolved to Bramble",a.text);
  EXPECT_FALSE(n.take(4999,a)); ASSERT_TRUE(n.take(5000,a));
  EXPECT_STREQ("Pet: Evolved to Grove",a.text); EXPECT_FALSE(n.take(10000,a));
}
TEST(PetNotifications, PresentationCooldownHandlesMillisWrap) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  n.reward(1,0); ASSERT_TRUE(n.take(0xfffffff0UL,a)); n.reward(2,0);
  EXPECT_FALSE(n.take(100,a)); EXPECT_TRUE(n.take(5000,a));
}
TEST(PetNotifications, AllElevenEvolutionsAndLateReadinessAreRetained) {
  PetNotifications n; PetNotifications::Alert a; observe(n);
  uint8_t id=0;
  for(uint8_t level=2;level<=Evolution::LEVELS;++level) {
    id=Evolution::child(id,0); n.evolved(id);
  }
  id=0;
  uint32_t now=0;
  for(uint8_t level=2;level<=Evolution::LEVELS;++level,now+=5000) {
    id=Evolution::child(id,0);
    ASSERT_TRUE(n.take(now,a)); EXPECT_EQ(PetNotifications::EVOLVED,a.sound);
    char expected[80]; snprintf(expected,sizeof(expected),"Pet: Evolved to %s",form(id).name);
    EXPECT_STREQ(expected,a.text);
  }
  EXPECT_FALSE(n.take(now,a));
  for(uint8_t level=10;level<Evolution::LEVELS;++level,now+=5000) {
    observe(n,70,true,false,false,true,level);
    ASSERT_TRUE(n.take(now,a)); EXPECT_EQ(PetNotifications::READY,a.sound);
    observe(n,70,true,false,false,true,level);
    EXPECT_FALSE(n.take(now+5000,a));
  }
}
TEST(PetNotifications, MelodiesAreUniqueShortAndSeparateFromSavedCatalogue) {
  for(uint8_t i=0;i<PetNotifications::COUNT;++i) {
    const char* m=PetNotifications::melody((PetNotifications::Sound)i);
    ASSERT_NE(nullptr,m); const char* notes=strrchr(m,':'); ASSERT_NE(nullptr,notes);
    unsigned count=1; for(const char* p=notes+1;*p;++p) if(*p==',') ++count;
    EXPECT_GE(count,2u); EXPECT_LE(count,6u);
    for(uint8_t j=0;j<i;++j)
      EXPECT_STRNE(m,PetNotifications::melody((PetNotifications::Sound)j));
  }
}
TEST(PetNotifications, UsesSharedMutePopupAndWakeRulesWithoutUnread) {
  auto event=zen::NotificationProfiles::event(zen::NotificationType::PET);
  zen::NotificationContext context;
  auto on=zen::NotificationCoordinator::decide(event,context);
  EXPECT_TRUE(on.show_visual); EXPECT_TRUE(on.play_sound); EXPECT_TRUE(on.wake_screen);
  EXPECT_FALSE(on.record_unread);
  context.silent=true;
  auto silent=zen::NotificationCoordinator::decide(event,context);
  EXPECT_TRUE(silent.show_visual); EXPECT_FALSE(silent.play_sound);
  context.silent=false; context.mode=zen::NotificationPolicy::OFF;
  auto off=zen::NotificationCoordinator::decide(event,context);
  EXPECT_TRUE(off.show_visual); EXPECT_FALSE(off.play_sound); EXPECT_FALSE(off.wake_screen);
  context.screen_wake=zen::NotificationPolicy::ScreenWake::ALWAYS;
  EXPECT_TRUE(zen::NotificationCoordinator::decide(event,context).wake_screen);
  context.mode=zen::NotificationPolicy::AUTO; context.client_connected=true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).play_sound);
  context.quiet_time=true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).show_visual);
  context.quiet_time=false; context.low_power=true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).show_visual);
  EXPECT_EQ(0,zen::NotificationCoordinator::popupPriority(zen::NotificationType::PET));
}
TEST(PetNotifications, GameEffectsFollowMuteRulesWithoutPopupOrWake) {
  auto event=PetGameAudio::event(PetGameAudio::melody(PetTrainingGames::HIT));
  zen::NotificationContext context;
  auto result=zen::NotificationCoordinator::decide(event,context);
  EXPECT_TRUE(result.play_sound); EXPECT_FALSE(result.show_visual);
  EXPECT_FALSE(result.wake_screen); EXPECT_FALSE(result.record_unread);
  EXPECT_EQ(nullptr,event.popup);
  context.silent=true; EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).play_sound);
  context.silent=false; context.quiet_time=true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).play_sound);
  context.quiet_time=false; context.mode=zen::NotificationPolicy::OFF;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).play_sound);
  context.mode=zen::NotificationPolicy::AUTO; context.client_connected=true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).play_sound);
  context.mode=zen::NotificationPolicy::ON; context.low_power=true;
  EXPECT_FALSE(zen::NotificationCoordinator::decide(event,context).play_sound);
  for(int cue=1;cue<=PetTrainingGames::COVER;++cue)
    EXPECT_NE(nullptr,PetGameAudio::melody((PetTrainingGames::Cue)cue));
  EXPECT_EQ(nullptr,PetGameAudio::melody(PetTrainingGames::SILENT));
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
