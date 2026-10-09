#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetMeshRewards.h"
using namespace zen::pet;

static const uint8_t ALICE[32] = {1};
static const uint8_t BOB[32] = {1,0,0,0,2}; // Same short prefix, different identity.
static void awake(PetMeshRewards& r, uint32_t now=0) {
  r.update(now,true,false,false,false,0);
}
static void send(PetMeshRewards& r, PetMeshRewards::Kind kind,
                 const uint8_t* key, uint32_t token, bool favourite=false) {
  r.started(kind,key,token,0,token,true);
  r.completed(token,kind,true,favourite);
}

TEST(PetRewards, DailyCapsAndSharedChannelRoomAllowance) {
  PetMeshRewards r; awake(r);
  send(r,PetMeshRewards::DM,ALICE,1);
  send(r,PetMeshRewards::DM,BOB,2);
  send(r,PetMeshRewards::CHANNEL,nullptr,3);
  send(r,PetMeshRewards::ROOM,ALICE,4,true);
  r.received(ALICE,5,false,true,true);
  EXPECT_EQ(35,r.progress().pending_xp);
  EXPECT_EQ(5,r.progress().pending_bond);
  EXPECT_TRUE(r.progress().dm); EXPECT_TRUE(r.progress().shared);
  EXPECT_TRUE(r.progress().favourite); EXPECT_TRUE(r.progress().conversation);
}
TEST(PetRewards, RoomFirstSharesAllowanceButDoesNotCountAsDM) {
  PetMeshRewards r; awake(r);
  send(r,PetMeshRewards::ROOM,ALICE,1);
  send(r,PetMeshRewards::CHANNEL,nullptr,2);
  r.received(ALICE,3,true,true,false);
  EXPECT_EQ(15,r.progress().pending_xp); EXPECT_FALSE(r.progress().dm);
  EXPECT_FALSE(r.progress().conversation);
}
TEST(PetRewards, BondMilestonesAreOrderIndependent) {
  for(bool favourite_first:{false,true}) {
    PetMeshRewards r; awake(r);
    if(favourite_first) {
      r.received(BOB,1,false,true,true);
      EXPECT_EQ(3,r.progress().pending_bond);
    }
    send(r,PetMeshRewards::DM,ALICE,2);
    r.received(ALICE,3,false,true,false);
    if(!favourite_first) r.received(BOB,1,false,true,true);
    EXPECT_EQ(5,r.progress().pending_bond); EXPECT_EQ(5,r.progress().bond);
  }
}
TEST(PetRewards, ConversationEitherOrderAndFullKeys) {
  PetMeshRewards r; awake(r);
  r.received(BOB,1,false,true,false);
  send(r,PetMeshRewards::DM,ALICE,2);
  EXPECT_FALSE(r.progress().conversation);
  send(r,PetMeshRewards::DM,BOB,3);
  EXPECT_TRUE(r.progress().conversation);
}
TEST(PetRewards, QueuingIsNotSuccessAndRetriesDeduplicate) {
  PetMeshRewards r; awake(r);
  r.started(PetMeshRewards::DM,ALICE,100,0,1,true);
  r.started(PetMeshRewards::DM,ALICE,100,1,2,true);
  EXPECT_EQ(0,r.progress().pending_xp);
  r.completed(1,PetMeshRewards::DM,true,false); // Late ACK of initial attempt.
  r.completed(2,PetMeshRewards::DM,true,false);
  r.completed(1,PetMeshRewards::DM,true,false);
  EXPECT_EQ(20,r.progress().pending_xp);
  awake(r,86400000UL);
  r.completed(2,PetMeshRewards::DM,true,false);
  r.started(PetMeshRewards::DM,ALICE,100,2,3,true);
  r.completed(3,PetMeshRewards::DM,true,false);
  EXPECT_FALSE(r.progress().dm);
}
TEST(PetRewards, DuplicateRelayAndIncomingDoNotRequalifyNextDay) {
  PetMeshRewards r; awake(r);
  send(r,PetMeshRewards::CHANNEL,nullptr,7);
  r.received(ALICE,8,false,true,true);
  awake(r,86400000UL);
  r.completed(7,PetMeshRewards::CHANNEL,true,false);
  r.received(ALICE,8,false,true,true);
  EXPECT_FALSE(r.progress().shared); EXPECT_FALSE(r.progress().favourite);
}
TEST(PetRewards, ChildEligibilityAtStartAndCompletion) {
  PetMeshRewards r; awake(r);
  r.started(PetMeshRewards::DM,ALICE,1,0,1,false);
  r.completed(1,PetMeshRewards::DM,true,true);
  r.received(ALICE,2,false,false,true);
  EXPECT_EQ(0,r.progress().pending_xp); EXPECT_EQ(0,r.progress().pending_bond);
  r.started(PetMeshRewards::DM,ALICE,3,0,3,true);
  r.completed(3,PetMeshRewards::DM,false,true);
  EXPECT_FALSE(r.progress().dm);
}
TEST(PetRewards, QuietTimeDefersAcrossMidnightAndAppliesOnce) {
  PetMeshRewards r; Engine e;
  r.update(0,true,true,false,true,86300); e.update(0,true,true,false);
  send(r,PetMeshRewards::DM,ALICE,1,true);
  EXPECT_FALSE(r.apply(e));
  r.update(100000,true,true,false,true,86400);
  EXPECT_FALSE(r.progress().dm); EXPECT_EQ(20,r.progress().pending_xp);
  send(r,PetMeshRewards::DM,ALICE,2);
  r.update(100001,true,false,false,true,86400); e.update(100001,true,false,false);
  EXPECT_TRUE(r.apply(e)); EXPECT_FALSE(r.apply(e));
  EXPECT_EQ(40,e.state().xp); EXPECT_EQ(3,e.state().bond);
}
TEST(PetRewards, DisabledAndLowPowerCannotCreateRetroactiveRewards) {
  PetMeshRewards r; awake(r);
  r.started(PetMeshRewards::DM,ALICE,1,0,1,true);
  r.update(1,false,false,false,false,0);
  awake(r,2); r.completed(1,PetMeshRewards::DM,true,true);
  r.started(PetMeshRewards::DM,ALICE,1,1,2,true);
  r.completed(2,PetMeshRewards::DM,true,true);
  EXPECT_FALSE(r.progress().dm);
  r.update(3,true,false,true,false,0);
  send(r,PetMeshRewards::DM,ALICE,3,true);
  r.received(ALICE,4,false,true,true);
  awake(r,4); EXPECT_FALSE(r.progress().dm); EXPECT_FALSE(r.progress().favourite);
}
TEST(PetRewards, DisabledDiscardsPendingButKeepsClaims) {
  PetMeshRewards r; awake(r); send(r,PetMeshRewards::DM,ALICE,1);
  r.update(1,false,false,false,false,0); awake(r,2);
  send(r,PetMeshRewards::DM,ALICE,2);
  EXPECT_TRUE(r.progress().dm); EXPECT_EQ(0,r.progress().pending_xp);
}
TEST(PetRewards, LowPowerRetainsPreviouslyEarnedPending) {
  PetMeshRewards r; Engine e; awake(r); e.update(0,true,false,false);
  send(r,PetMeshRewards::DM,ALICE,1);
  r.update(1,true,false,true,false,0); EXPECT_FALSE(r.apply(e));
  awake(r,2); EXPECT_TRUE(r.apply(e)); EXPECT_EQ(20,e.state().xp);
}
TEST(PetRewards, RetirementPreservesAllowancesButClearsOldConversationEvidence) {
  PetMeshRewards r; awake(r); Engine e; e.update(0,true,false,false);
  send(r,PetMeshRewards::DM,ALICE,1);
  ASSERT_TRUE(r.progress().dm); r.retire();
  EXPECT_EQ(0,r.progress().pending_xp); EXPECT_EQ(0,r.progress().pending_bond);
  r.received(ALICE,17,false,true,false); EXPECT_FALSE(r.progress().conversation);
  send(r,PetMeshRewards::DM,ALICE,2);
  EXPECT_EQ(0,r.progress().pending_xp); EXPECT_TRUE(r.progress().conversation);
  EXPECT_EQ(5,r.progress().pending_bond);
  r.retire(); EXPECT_EQ(5,r.progress().bond);
  r.completed(2,PetMeshRewards::DM,true,true); EXPECT_EQ(0,r.progress().pending_bond);
  send(r,PetMeshRewards::CHANNEL,nullptr,3); EXPECT_EQ(15,r.progress().pending_xp);
}
TEST(PetRewards, XPAndBondSaturateWithoutAutomaticEvolution) {
  Engine e; e.update(0,true,false,false); e.bonus(65535,255);
  EXPECT_EQ(zen::pet::Evolution::xp(zen::pet::Evolution::LEVELS),e.state().xp); EXPECT_EQ(100,e.state().bond);
  EXPECT_EQ(1,e.level()); e.bonus(35,5);
  EXPECT_EQ(zen::pet::Evolution::xp(zen::pet::Evolution::LEVELS),e.state().xp); EXPECT_EQ(100,e.state().bond);
}
TEST(PetRewards, UptimeRolloverHandlesMillisWrap) {
  PetRewardDay d;
  EXPECT_FALSE(d.update(0xfffffff0UL,false,0));
  EXPECT_FALSE(d.update(100,false,0));
  EXPECT_TRUE(d.update(86400100UL,false,0));
}
TEST(PetRewards, LocalMidnightAndDST) {
  PetRewardDay d;
  d.update(0,true,86300); EXPECT_TRUE(d.update(100000,true,86400));
  // DST jump does not itself renew the allowance.
  EXPECT_FALSE(d.update(200000,true,90100));
  EXPECT_FALSE(d.update(300000,true,86600));
}
TEST(PetRewards, SynchronizationAndClockCorrectionsDoNotRenew) {
  PetRewardDay d; d.update(0,false,0);
  EXPECT_FALSE(d.update(1000,true,86399));
  EXPECT_FALSE(d.update(2000,true,86400)); // Guard after first synchronization.
  EXPECT_FALSE(d.update(3000,true,172799));
  EXPECT_FALSE(d.update(4000,true,172800));
  EXPECT_FALSE(d.update(5000,true,86399));
  EXPECT_FALSE(d.update(6000,true,86400));
}
TEST(PetRewards, DailyResetAfterCorrectionGuardExpires) {
  PetRewardDay d; d.update(0,true,0);
  EXPECT_FALSE(d.update(1000,true,3600));
  EXPECT_TRUE(d.update(82801000UL,true,86400));
}
TEST(PetRewards, DailyConversationEvidenceClearsAndRebootResets) {
  PetMeshRewards r; awake(r); r.received(ALICE,1,false,true,false);
  awake(r,86400000UL); send(r,PetMeshRewards::DM,ALICE,2);
  EXPECT_FALSE(r.progress().conversation);
  PetMeshRewards reboot; awake(reboot); EXPECT_FALSE(reboot.progress().dm);
}
TEST(PetRewards, UnknownAndAdministrativeAcknowledgementsIgnored) {
  PetMeshRewards r; awake(r);
  r.completed(99,PetMeshRewards::DM,true,true);
  r.started(PetMeshRewards::DM,ALICE,1,0,0,true);
  r.completed(0,PetMeshRewards::DM,true,true);
  EXPECT_EQ(0,r.progress().pending_xp); EXPECT_EQ(0,r.progress().pending_bond);
}
TEST(PetRewards, FixedPeerEvictionNeverMatchesShortPrefix) {
  PetMeshRewards r; awake(r);
  for(uint8_t i=0;i<16;++i) {
    uint8_t key[32]={1}; key[4]=i+1;
    r.received(key,i+1,false,true,false);
  }
  send(r,PetMeshRewards::DM,ALICE,30);
  EXPECT_FALSE(r.progress().conversation);
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
