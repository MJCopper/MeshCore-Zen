#include <gtest/gtest.h>
#define MAX_TEXT_LEN 160
#define OUT_PATH_UNKNOWN 255
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageDeliveryCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageDisplayActivity.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ManualTransmitActivity.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/SensorAccessCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetGameDisplay.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/DisplayActivity.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RemoteNodeCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationCoordinator.h"
using Activity=zen::DisplayActivity;
using Token=Activity::Token;
static const uint8_t KEY[32]={1};

TEST(DisplayActivity, LongOperationHoldsExpiredScreenAndThenGetsFullTimeout) {
  Activity a;a.beginSample(0,true);a.observe({Activity::REMOTE,1},true);
  EXPECT_TRUE(a.endSample(30000).hold);
  a.beginSample(45000,true);a.observe({Activity::REMOTE,1},true);
  auto active=a.endSample(30000);
  EXPECT_FALSE(Activity::shouldBlank(active.hold,45000,30000,30000,false));
  a.beginSample(45001,true);auto end=a.endSample(30000);
  EXPECT_TRUE(end.released);EXPECT_EQ(75001u,end.deadline);
  EXPECT_FALSE(Activity::shouldBlank(end.hold,75000,end.deadline,30000,false));
  EXPECT_TRUE(Activity::shouldBlank(end.hold,75001,end.deadline,30000,false));
}
TEST(DisplayActivity, FinalReleaseAndLatestTimeoutSettingAreAuthoritative) {
  Activity a;a.beginSample(100,true);a.observe({Activity::REMOTE,1},true);a.observe({0,4},true);
  a.endSample(30000);a.beginSample(500,true);a.observe({0,4},true);
  auto one=a.endSample(60000);EXPECT_TRUE(one.hold);EXPECT_FALSE(one.released);
  a.beginSample(1000,true);auto last=a.endSample(60000);
  EXPECT_TRUE(last.released);EXPECT_EQ(61000u,last.deadline);
  a.beginSample(1001,true);EXPECT_FALSE(a.endSample().released);
}
TEST(DisplayActivity, CompletionExactlyAtOriginalDeadlineRestartsCountdown) {
  Activity a;a.beginSample(0,true);a.observe({Activity::PING,2},true,3000);a.endSample(1000);
  a.beginSample(1000,true);auto d=a.endSample(1000);
  EXPECT_TRUE(d.released);EXPECT_EQ(2000u,d.deadline);
  EXPECT_FALSE(Activity::shouldBlank(d.hold,1000,d.deadline,1000,false));
}
TEST(DisplayActivity, StaleAndDuplicateCompletionsCannotReleaseReplacement) {
  Activity a;a.beginSample(0,true);a.observe({Activity::REMOTE,1},true);a.endSample();
  a.beginSample(100,true);a.observe({Activity::REMOTE,2},true);
  EXPECT_FALSE(a.finish({Activity::REMOTE,1}));EXPECT_TRUE(a.endSample().hold);
  a.beginSample(101,true);EXPECT_TRUE(a.finish({Activity::REMOTE,2}));
  EXPECT_FALSE(a.finish({Activity::REMOTE,2}));EXPECT_TRUE(a.endSample().released);
}
TEST(DisplayActivity, ManualSleepSuppressesOldOperationsAcrossWakeAndLateResults) {
  Activity a;a.beginSample(0,true);a.observe({0,1},true);a.endSample();a.suspend();
  a.beginSample(100,true);a.observe({0,1},true);auto wake=a.endSample(30000);
  EXPECT_FALSE(wake.hold);EXPECT_FALSE(wake.released);
  a.beginSample(200,true);EXPECT_FALSE(a.endSample(30000).released);
  a.beginSample(201,true);a.observe({0,2},true);EXPECT_TRUE(a.endSample().hold);
}
TEST(DisplayActivity, WorkStartedWhileDisplayOffDoesNotReattachAfterNotificationWake) {
  Activity a;a.beginSample(0,false);a.observe({0,1},true);EXPECT_FALSE(a.endSample().hold);
  a.beginSample(100,true);a.observe({0,1},true);EXPECT_FALSE(a.endSample().hold);
}
TEST(DisplayActivity, LowPowerSuppressionDoesNotExtendExpiredDeadline) {
  Activity a;a.beginSample(0,true);a.observe({Activity::REMOTE,1},true);a.endSample();
  a.beginSample(100,false);a.observe({Activity::REMOTE,1},true);
  EXPECT_FALSE(a.endSample(30000).released);
  a.beginSample(101,true);a.observe({Activity::REMOTE,1},true);
  EXPECT_FALSE(a.endSample().hold);
}
TEST(DisplayActivity, RecoveryExpiresOnceAndCannotBeRenewedByRedrawing) {
  Activity a;a.beginSample(0,true);a.observe({0,1},true);a.endSample();
  a.beginSample(Activity::RECOVERY_MS,true);a.observe({0,1},true);
  EXPECT_TRUE(a.endSample().released);
  a.beginSample(Activity::RECOVERY_MS+1,true);a.observe({0,1},true);
  EXPECT_FALSE(a.endSample().hold);
}
TEST(DisplayActivity, OperationDeadlineAndRetryExtensionAreRespected) {
  Activity a;a.beginSample(0,true);a.observe({0,1},true,5000);a.endSample();
  a.beginSample(5000,true);a.observe({0,1},true,10000);EXPECT_TRUE(a.endSample().hold);
  a.beginSample(10000,true);a.observe({0,1},true,10000);EXPECT_TRUE(a.endSample().released);
}
TEST(DisplayActivity, FixedCapacityRejectsInvalidOwnersWithoutDamagingOtherHolds) {
  Activity a;a.beginSample(0,true);
  for(unsigned i=0;i<Activity::CAPACITY;++i)EXPECT_TRUE(a.observe({i,1},true));
  EXPECT_FALSE(a.observe({Activity::CAPACITY,1},true));EXPECT_FALSE(a.observe({0,0},true));
  EXPECT_TRUE(a.endSample().hold);
}
TEST(DisplayActivity, TimerWrapAndZeroDeadlineAreSafe) {
  Activity a;uint32_t start=0xfffffff0u;
  a.beginSample(start,true);a.observe({0,1},true,start+100);a.endSample();
  a.beginSample(start+99,true);a.observe({0,1},true,start+100);EXPECT_TRUE(a.endSample().hold);
  a.beginSample(start+100,true);auto end=a.endSample(1000);
  EXPECT_EQ(start+1100,end.deadline);EXPECT_TRUE(end.released);
}
TEST(DisplayActivity, AutoBlankDisabledAndNotificationTimeoutKeepExistingMeaning) {
  EXPECT_FALSE(Activity::shouldBlank(false,100000,10,0,false));
  EXPECT_TRUE(Activity::shouldBlank(false,100000,10,0,true));
  EXPECT_FALSE(Activity::shouldBlank(true,100000,10,0,true));
  Activity a;a.beginSample(0,true);a.observe({0,1},true);a.endSample();
  a.beginSample(100,true);auto d=a.endSample(0);EXPECT_TRUE(d.released);EXPECT_EQ(0u,d.deadline);
}
TEST(DisplayActivity, FailedStartAndPassiveScreensHaveNoHold) {
  Activity a;a.beginSample(0,true);a.observe({Activity::REMOTE,1},false);
  auto d=a.endSample(30000);EXPECT_FALSE(d.hold);EXPECT_FALSE(d.released);
}
TEST(DisplayActivity, ActualRemoteLifecycleKeepsHoldAcrossRetryAndVerification) {
  Activity a;zen::RemoteNodeCoordinator remote;
  ASSERT_TRUE(remote.begin(zen::RemoteNodeCoordinator::ADMIN_OWNER,
    zen::RemoteNodeOperation::ADMIN_WRITE,KEY,true,zen::RemoteNodeOperation::DO_NOT_RETRY,5000));
  uint32_t generation=remote.generation();
  a.beginSample(0,true);a.observe({Activity::REMOTE,generation},remote.active());a.endSample();
  ASSERT_TRUE(remote.complete(zen::RemoteNodeCoordinator::ADMIN_OWNER,KEY,zen::RemoteNodeOperation::SUCCESS));
  ASSERT_TRUE(remote.begin(zen::RemoteNodeCoordinator::ADMIN_OWNER,
    zen::RemoteNodeOperation::ADMIN_READ,KEY,true,zen::RemoteNodeOperation::RETRY_SAFE,10000,true));
  EXPECT_EQ(generation,remote.generation());
  a.beginSample(6000,true);a.observe({Activity::REMOTE,remote.generation()},remote.active());
  EXPECT_TRUE(a.endSample().hold);
  remote.timeout(zen::RemoteNodeCoordinator::ADMIN_OWNER,10000,true);
  ASSERT_TRUE(remote.restart(zen::RemoteNodeCoordinator::ADMIN_OWNER,20000));
  a.beginSample(10001,true);a.observe({Activity::REMOTE,remote.generation()},remote.active());
  EXPECT_TRUE(a.endSample().hold);
  remote.cancel(zen::RemoteNodeCoordinator::ADMIN_OWNER);
  a.beginSample(10002,true);a.observe({Activity::REMOTE,remote.generation()},remote.active());
  EXPECT_TRUE(a.endSample().released);
}
TEST(DisplayActivity, NotificationsCannotShortenHoldAndCompletionUsesNormalTimeout) {
  Activity a;zen::NotificationWakeController wake;
  a.beginSample(0,true);a.observe({Activity::REMOTE,1},true);a.endSample();
  auto notification=wake.present(false,true,100,30000);
  EXPECT_TRUE(notification.turn_on);
  a.beginSample(6000,true);a.observe({Activity::REMOTE,1},true);auto d=a.endSample(30000);
  EXPECT_FALSE(Activity::shouldBlank(d.hold,6000,notification.deadline,30000,wake.active()));
  a.beginSample(7000,true);auto end=a.endSample(30000);wake.interaction();
  EXPECT_EQ(37000u,end.deadline);EXPECT_FALSE(wake.active());
}
TEST(DisplaySources, MessagesRemainIndependentAcrossAckRetryFailureAndEviction) {
  Activity a;zen::DirectMessageRecord first{},second{};
  first.activity_seq=1;second.activity_seq=2;first.outgoing=second.outgoing=true;
  zen::MessageDeliveryCoordinator::begin(first,true,101,1000,zen::ROUTE_PATH);
  zen::MessageDeliveryCoordinator::begin(second,true,102,1000,zen::ROUTE_FLOOD);
  a.beginSample(0,true);
  zen::MessageDisplayActivity::direct(a,0,first);zen::MessageDisplayActivity::direct(a,1,second);
  EXPECT_TRUE(a.endSample().hold);
  uint8_t route=0;ASSERT_TRUE(zen::MessageDeliveryCoordinator::acknowledge(first,101,route));
  zen::MessageDeliveryCoordinator::recordAttempt(second,1,103,20000,zen::ROUTE_FLOOD);
  a.beginSample(1001,true);
  zen::MessageDisplayActivity::direct(a,0,first);zen::MessageDisplayActivity::direct(a,1,second);
  auto d=a.endSample();EXPECT_TRUE(d.hold);EXPECT_FALSE(d.released);
  zen::MessageDeliveryCoordinator::fail(second);
  a.beginSample(1100,true);zen::MessageDisplayActivity::direct(a,1,second);
  EXPECT_TRUE(a.endSample().released);
  second.activity_seq=3;
  zen::MessageDeliveryCoordinator::begin(second,true,104,30000,zen::ROUTE_FLOOD);
  a.beginSample(1200,true);zen::MessageDisplayActivity::direct(a,1,second);a.endSample();
  a.beginSample(1201,true); // eviction/removal: authoritative record disappears
  EXPECT_TRUE(a.endSample().released);
}
TEST(DisplaySources, IncomingCompanionAndUnconfirmedMessagesDoNotAcquireHolds) {
  Activity a;zen::DirectMessageRecord e{};e.activity_seq=1;
  a.beginSample(0,true);
  zen::MessageDeliveryCoordinator::begin(e,false,101,1000,zen::ROUTE_PATH);
  zen::MessageDisplayActivity::direct(a,0,e);
  e.outgoing=true;
  zen::MessageDeliveryCoordinator::begin(e,true,101,1000,zen::ROUTE_PATH,zen::DELIVERY_ORIGIN_COMPANION);
  zen::MessageDisplayActivity::direct(a,1,e);
  e.delivery_origin=zen::DELIVERY_ORIGIN_ZEN;e.ack_status=zen::DELIVERY_NONE;
  zen::MessageDisplayActivity::direct(a,2,e);
  EXPECT_FALSE(a.endSample().hold);
}
TEST(DisplaySources, ChannelEchoExpiryQueueEvictionAndCompanionOriginReleaseCorrectly) {
  Activity a;zen::ChannelMessageRecord e{};e.local_send=true;
  zen::MessageDeliveryCoordinator::armRelay(e,1);
  a.beginSample(0,true);zen::MessageDisplayActivity::channel(a,0,e,true);a.endSample();
  a.beginSample(100,true);zen::MessageDisplayActivity::channel(a,0,e,false);
  EXPECT_TRUE(a.endSample().released); // real transport no longer tracks the packet
  zen::MessageDeliveryCoordinator::armRelay(e,2);
  a.beginSample(101,true);zen::MessageDisplayActivity::channel(a,0,e,true);a.endSample();
  zen::MessageDeliveryCoordinator::hearRelay(e,2);
  a.beginSample(102,true);zen::MessageDisplayActivity::channel(a,0,e,true);
  EXPECT_TRUE(a.endSample().released);
  zen::MessageDeliveryCoordinator::armRelay(e,3);e.local_send=false;
  a.beginSample(103,true);zen::MessageDisplayActivity::channel(a,0,e,true);
  EXPECT_FALSE(a.endSample().hold);
}
TEST(DisplaySources, SensorLoginSettlingAndTelemetryAreOneLogicalHold) {
  Activity a;zen::SensorAccessCoordinator sensor;
  auto sample=[&](uint32_t now,bool telemetry,bool login) {
    a.beginSample(now,true);
    a.observe({Activity::SENSOR,1},sensor.displayPending(telemetry,login));
    return a.endSample(30000);
  };
  sensor.beginTelemetry();EXPECT_TRUE(sample(0,true,false).hold);
  sensor.telemetryTimedOut();sensor.loginStarted(false);
  EXPECT_TRUE(sample(100,false,true).hold);
  sensor.loginSucceeded(200);EXPECT_TRUE(sample(200,false,false).hold);
  EXPECT_TRUE(sample(1199,false,false).hold);
  ASSERT_EQ(zen::SensorAccessCoordinator::SEND_TELEMETRY,sensor.tick(1200));
  EXPECT_TRUE(sample(1200,true,false).hold);
  auto done=sample(1300,false,false);EXPECT_TRUE(done.released);EXPECT_EQ(31300u,done.deadline);
}
TEST(DisplaySources, SensorPasswordOfferIsIdleAndFailedStartHasNoHold) {
  zen::SensorAccessCoordinator sensor;
  sensor.telemetrySendFailed();EXPECT_FALSE(sensor.displayPending(false,false));
  sensor.loginFailed(false);EXPECT_FALSE(sensor.displayPending(false,false));
  sensor.offerPassword();EXPECT_FALSE(sensor.displayPending(false,false));
  sensor.loginStarted(true);EXPECT_TRUE(sensor.displayPending(false,true));
  sensor.loginFailed(true);EXPECT_FALSE(sensor.displayPending(false,false));
}
TEST(DisplaySources, ManualAdvertCompletesOnTxOrFailureAndOtherPacketsAreIgnored) {
  Activity a;zen::ManualTransmitActivity tx;int packet[5];
  ASSERT_TRUE(tx.begin(&packet[0],0));ASSERT_TRUE(tx.begin(&packet[1],0));
  a.beginSample(0,true);tx.observe(a,0);EXPECT_TRUE(a.endSample().hold);
  EXPECT_FALSE(tx.complete(&packet[2]));ASSERT_TRUE(tx.complete(&packet[0]));
  a.beginSample(100,true);tx.observe(a,100);EXPECT_TRUE(a.endSample().hold);
  ASSERT_TRUE(tx.complete(&packet[1])); // same observer called by TX-failure hook
  a.beginSample(200,true);tx.observe(a,200);EXPECT_TRUE(a.endSample().released);
  EXPECT_FALSE(tx.complete(&packet[1]));
}
TEST(DisplaySources, AdvertCapacityQueueTimeoutAndPowerClearAreBounded) {
  Activity a;zen::ManualTransmitActivity tx;int packet[5];
  for(unsigned i=0;i<4;++i) ASSERT_TRUE(tx.begin(&packet[i],0));
  EXPECT_FALSE(tx.begin(&packet[4],0));EXPECT_FALSE(tx.begin(nullptr,0));
  a.beginSample(0,true);tx.observe(a,0);a.endSample();
  a.beginSample(120000,true);tx.observe(a,120000);EXPECT_TRUE(a.endSample().released);
  ASSERT_TRUE(tx.begin(&packet[4],120001));
  a.beginSample(120001,true);tx.observe(a,120001);EXPECT_TRUE(a.endSample().hold);
  tx.clear();a.beginSample(120002,true);tx.observe(a,120002);EXPECT_TRUE(a.endSample().released);
}
TEST(DisplaySources, GameInactivityDoesNotBlankOverAnotherOperation) {
  Activity a;zen::pet::PetGameDisplay game;
  auto sample=[&](uint32_t now,bool playing,bool watching) {
    a.beginSample(now,true);auto g=game.update(now,playing,watching,true);
    a.observe({Activity::GAME,g.generation},g.hold);
    a.observe({Activity::REMOTE,1},true);return a.endSample(30000);
  };
  EXPECT_TRUE(sample(0,true,false).hold);
  EXPECT_TRUE(sample(60000,true,false).hold); // game idle expired; radio still owns a hold
  EXPECT_TRUE(sample(60001,false,false).hold);
  a.beginSample(60002,true);auto end=a.endSample(30000);
  EXPECT_TRUE(end.released);EXPECT_EQ(90002u,end.deadline);
}
TEST(DisplaySources, GameResultUsesNormalTimeoutAndNextGameAcquiresNewGeneration) {
  Activity a;zen::pet::PetGameDisplay game;
  a.beginSample(0,true);auto first=game.update(0,true,true,true);
  a.observe({Activity::GAME,first.generation},first.hold);a.endSample();
  game.result(100);a.beginSample(100,true);auto result=game.update(100,false,false,true);
  a.observe({Activity::GAME,result.generation},result.hold);
  auto d=a.endSample(30000);EXPECT_TRUE(d.released);EXPECT_EQ(30100u,d.deadline);
  auto next=game.update(101,true,false,true);EXPECT_NE(first.generation,next.generation);
}
TEST(DisplaySources, DiscoveryWindowEndsWithAndWithoutResultsAndRescanStartsFresh) {
  // Discovery completes on its collection deadline, independently of result count.
  for(unsigned results:{0u,5u}) {
    (void)results;Activity a;
    a.beginSample(0,true);a.observe({Activity::DISCOVERY,1},true,8000);a.endSample();
    a.beginSample(7999,true);a.observe({Activity::DISCOVERY,1},true,8000);
    EXPECT_TRUE(a.endSample().hold);
    a.beginSample(8000,true);a.observe({Activity::DISCOVERY,1},false,8000);
    EXPECT_EQ(38000u,a.endSample(30000).deadline);
    a.beginSample(8001,true);a.observe({Activity::DISCOVERY,2},true,16001);
    EXPECT_TRUE(a.endSample().hold);
  }
}
TEST(DisplaySources, ManualTxStartExtendsQueueDeadlineForActualAirtime) {
  Activity a;zen::ManualTransmitActivity tx;int packet;
  ASSERT_TRUE(tx.begin(&packet,0));
  a.beginSample(0,true);tx.observe(a,0);a.endSample();
  tx.started(&packet,119999,20000);
  a.beginSample(130000,true);tx.observe(a,130000);EXPECT_TRUE(a.endSample().hold);
  tx.complete(&packet);a.beginSample(140000,true);tx.observe(a,140000);
  EXPECT_TRUE(a.endSample().released);
}
TEST(DisplaySources, ManualSleepDuringSensorDelayDoesNotReattachTelemetryContinuation) {
  Activity a;zen::SensorAccessCoordinator sensor;
  sensor.loginSucceeded(0);
  a.beginSample(0,true);a.observe({Activity::SENSOR,1},sensor.displayPending(false,false));a.endSample();
  a.suspend();ASSERT_EQ(zen::SensorAccessCoordinator::SEND_TELEMETRY,sensor.tick(1000));
  a.beginSample(1000,true);a.observe({Activity::SENSOR,1},sensor.displayPending(true,false));
  EXPECT_FALSE(a.endSample().hold);
}
TEST(DisplaySources, GamePlayerIdleRestartsFullTimeoutRatherThanForcingScreenOff) {
  Activity a;zen::pet::PetGameDisplay game;
  a.beginSample(0,true);auto g=game.update(0,true,false,true);
  a.observe({Activity::GAME,g.generation},g.hold);a.endSample();
  a.beginSample(60000,true);g=game.update(60000,true,false,true);
  EXPECT_TRUE(g.expired);a.observe({Activity::GAME,g.generation},g.hold);
  auto d=a.endSample(30000);EXPECT_TRUE(d.released);EXPECT_EQ(90000u,d.deadline);
  EXPECT_FALSE(Activity::shouldBlank(d.hold,60000,d.deadline,30000,false));
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS(); }
