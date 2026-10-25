#include "../sensor_mocks/SensorIntegrationFixture.h"
#include <helpers/RepeaterLogin.h>
#include "../timing_reference/LoRaReferenceVectors.h"
#include <tuple>

TEST(CompanionTiming, ExactFirmwareFormulaWithExplicitSensorMargin) {
  EXPECT_EQ(4700u,TraceTimingPolicy::floodTimeout(200));
  EXPECT_EQ(7300u,TraceTimingPolicy::attemptTimeout(2,200));
  EXPECT_EQ(3100u,TraceTimingPolicy::floodTimeout(100));
  EXPECT_EQ(4900u,TraceTimingPolicy::attemptTimeout(2,100));
  EXPECT_EQ(6600u,TraceTimingPolicy::attemptTimeout(3,100));
  EXPECT_EQ(39000u,TraceTimingPolicy::attemptTimeout(3,1000));
  EXPECT_EQ(126500u,TraceTimingPolicy::attemptTimeout(10,1000));
  EXPECT_EQ(0x7ffff000u,TraceTimingPolicy::floodTimeout(UINT32_MAX));
  EXPECT_EQ(0x7ffff000u,TraceTimingPolicy::attemptTimeout(10,UINT32_MAX));
}
TEST(CompanionTiming, FrozenBudgetBelowAboveSixtyAndCappedAtOneHundredTwenty) {
  TraceTransactionBudget bare,known,unknown,slow;
  ASSERT_TRUE(bare.calculate(false,false,true,2,100,100,100,100));
  EXPECT_EQ(37000u,bare.duration); EXPECT_EQ(2,bare.planned_probes);
  ASSERT_TRUE(known.calculate(true,true,true,2,100,100,100,100));
  ASSERT_TRUE(unknown.calculate(true,true,false,10,100,100,100,100));
  EXPECT_GT(unknown.duration,60000u); EXPECT_EQ(1,unknown.planned_probes);
  ASSERT_TRUE(slow.calculate(true,true,true,10,1000,1000,1000,1000));
  EXPECT_EQ(120000u,slow.duration); EXPECT_TRUE(slow.capped); EXPECT_EQ(1,slow.planned_probes);
  auto end=uint32_t(UINT32_MAX-500+bare.duration);
  EXPECT_EQ(bare.duration-bare.report_reserve,bare.work(UINT32_MAX-500,end));
  EXPECT_EQ(0u,bare.work(end-bare.report_reserve,end));
  EXPECT_EQ(0u,TraceTransactionBudget::remaining(end,end));
  EXPECT_FALSE(bare.calculate(false,false,true,2,100,100,0,100));
  EXPECT_FALSE(bare.calculate(false,false,true,0,100,100,100,100));
}
TEST(CompanionTiming, QueueAndReportAreReservedOnceAndRouteEntriesAreNotBytes) {
  TraceTransactionBudget b;
  ASSERT_TRUE(b.calculate(false,false,true,2,100,100,100,100));
  uint64_t expected=8000+b.report_reserve+1150+5500+2*4900+2000;
  EXPECT_EQ(((expected+999)/1000)*1000,b.duration);
  for(uint8_t width : {1,2}) for(uint8_t count=1;count<=10;count++) {
    TraceRoute r{}; uint8_t key[32]{1},via[18]{};
    ASSERT_EQ(TraceRoute::VALID,TraceRoute::fromOutbound(via,((width-1)<<6)|(count-1),key,r));
    EXPECT_EQ(11+(2*count-1)*(width+1),r.maxPacketLength());
    EXPECT_EQ(1500u+850u*(2*count),TraceTimingPolicy::attemptTimeout(count,100));
  }
}

TEST(CompanionTiming, BareRetrySizingKeepsTheOriginalRouteWhenALongerAlternateCannotFit) {
  TraceTransactionBudget budget;
  ASSERT_TRUE(budget.calculate(false, false, true, 2, 100, 100, 100, 100));
  const uint32_t start = 1000;
  const uint32_t deadline = start + budget.duration;
  const uint32_t retry_at = start + 12000;

  EXPECT_TRUE(budget.fitsProbe(2, 100, 1500, retry_at, deadline, 0));
  EXPECT_FALSE(budget.fitsProbe(10, 100, 1500, retry_at, deadline, 0));
  EXPECT_TRUE(budget.fitsProbe(2, 100, 1500, retry_at, deadline, 8000));
  EXPECT_FALSE(budget.fitsProbe(10, 100, 1500, retry_at, deadline, 8000));
}

class CompanionFlow : public SensorIntegration {
protected:
  TestRadio remote_radio; StaticPoolPacketManager remote_pool{32}; SimpleMeshTables remote_tables;
  TestMesh remote{remote_radio,clock,random,rtc,remote_pool,remote_tables}; mesh::LocalIdentity target;
  void SetUp() override {
    SensorIntegration::SetUp(); mesh.complete_trace_ack_immediately=false;
    mesh.self_id=mesh::LocalIdentity(&random); target=mesh::LocalIdentity(&random); remote.self_id=target;
    remote.begin(); bot.begin(&fs);
    uint8_t app[40]; auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(app);
    bot.onAdvertRecv(nullptr,target,100,app,len);
  }
  std::vector<uint8_t> wire(mesh::Packet& p) {
    uint8_t bytes[MAX_TRANS_UNIT]; auto len=p.writeTo(bytes); return {bytes,bytes+len};
  }
  void startLogin() {
    char text[64]; snprintf(text,sizeof(text),"Alice: !hillvue trace %02x%02x",target.pub_key[0],target.pub_key[1]);
    radio.incoming=command(text); tick(100); finish(200); tick(201); finish(301);
    tick(3401); tick(3402); finish(3502);
    ASSERT_EQ(1u,bot.diagnostics().login_starts);
  }
  void loginReply(uint32_t now,uint8_t via_count=1,uint8_t width=1,uint8_t admin=0) {
    uint8_t body[13]{},nonce[4]{},secret[32],path[18]{}; ClientInfo client{};
    client.permissions=admin?PERM_ACL_ADMIN:PERM_ACL_GUEST;
    encodeRepeaterLoginReply(body,client,1800000000,nonce,2);
    target.calcSharedSecret(secret,mesh.self_id.pub_key);
    for(unsigned i=0;i<via_count*width;i++) path[i]=0x20+i;
    auto p=remote.createPathReturn(mesh.self_id,secret,path,((width-1)<<6)|via_count,PAYLOAD_TYPE_RESPONSE,body,13);
    ASSERT_NE(nullptr,p); p->header|=ROUTE_TYPE_FLOOD;
    radio.incoming=wire(*p); remote.releasePacket(p); tick(now);
  }
  void finishUntil(uint32_t end) {
    for(uint32_t now=g_mock_millis+50;now<=end;now+=50) { if(radio.transmitting) radio.complete=true; tick(now); }
  }
};
TEST_F(CompanionFlow, ValidLoginPathSkipsSecondRequestAndPreservesAveragedTraceOutput) {
  startLogin(); auto deadline=bot.traceDeadline(); loginReply(3600); tick(3601);
  ASSERT_EQ(4u,radio.sent.size()); EXPECT_TRUE(bot.diagnostics().login_path);
  EXPECT_FALSE(bot.diagnostics().control[2].sent); EXPECT_EQ(1u,bot.diagnostics().discovery_starts);
  mesh::Packet p{}; ASSERT_TRUE(mesh.tryParsePacket(&p,radio.sent[3].data(),radio.sent[3].size()));
  EXPECT_EQ(PAYLOAD_TYPE_TRACE,p.getPayloadType()); EXPECT_EQ(0x20,p.payload[9]); EXPECT_EQ(target.pub_key[0],p.payload[10]);
  finish(3701); traceReply(3,4000,{-24,-32,-16,-8}); finishUntil(8000);
  ASSERT_EQ(5u,radio.sent.size()); EXPECT_NE(std::string::npos,publicText(4).find("avg SNR"));
  EXPECT_EQ(deadline,bot.traceDeadline()); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(CompanionFlow, LateLoginCancelsQueuedRetryBeforeItCanTransmit) {
  startLogin(); tick(6602); tick(6852);
  ASSERT_GT(bot.diagnostics().control[3].queued,0u); loginReply(6853); tick(6854);
  EXPECT_EQ(1u,bot.diagnostics().login_starts); EXPECT_TRUE(bot.diagnostics().control[3].cancelled);
  EXPECT_FALSE(bot.diagnostics().control[3].sent); EXPECT_TRUE(bot.diagnostics().login_path);
}
TEST_F(CompanionFlow, DirectZeroIntermediateLoginPathStillIncludesTarget) {
  startLogin(); loginReply(3600,0); tick(3601);
  ASSERT_EQ(4u,radio.sent.size()); mesh::Packet p{};
  ASSERT_TRUE(mesh.tryParsePacket(&p,radio.sent[3].data(),radio.sent[3].size()));
  EXPECT_EQ(target.pub_key[0],p.payload[9]); EXPECT_EQ(10,p.payload_len);
}
TEST_F(CompanionFlow, UnexpectedAdminReplyCannotBecomeALoginTracePath) {
  startLogin(); loginReply(3600,1,1,1); finishUntil(8000);
  EXPECT_FALSE(bot.diagnostics().login_path); EXPECT_EQ(0u,bot.diagnostics().starts);
  EXPECT_EQ(1u,bot.diagnostics().login_starts);
}
TEST_F(CompanionFlow, LateReplyAfterCancellationCannotQueueTraceOrMutateManagementRoute) {
  startLogin(); radio.incoming=command("Alice: !hillvue trace cancel"); tick(3600);
  auto sent=radio.sent.size(); loginReply(3700); finishUntil(8000);
  EXPECT_FALSE(bot.diagnostics().login_path); EXPECT_EQ(0u,bot.diagnostics().starts);
  EXPECT_EQ(0u,mesh.management_path_updates); EXPECT_LE(radio.sent.size(),sent+1);
}
TEST_F(CompanionFlow, DiagnosticsAreBoundedAndFrozenForRadioTiming) {
  startLogin(); char reply[160];
  for(const char* command : {"get trace.timing","get trace.airtime","get trace.radio"}) {
    ASSERT_TRUE(bot.tracePolicyCommand(command,reply)); EXPECT_LT(strlen(reply),160u);
  }
  EXPECT_LT(bot.timing().duration,120001u);
  EXPECT_EQ(0u,radio.estimates_during_tx);
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }

class PHYTiming : public ::testing::TestWithParam<std::tuple<int,int,int>> {};
TEST_P(PHYTiming, GeneratedPinnedRadioLibAirtimeDrivesBudgetsAcrossRoutes) {
  int phy=std::get<0>(GetParam()),width=std::get<1>(GetParam()),count=std::get<2>(GetParam());
  const auto& reference=LORA_REFERENCE[phy]; int length=11+(2*count-1)*(width+1);
  auto air=reference.airtime[length]; TraceTransactionBudget budget;
  bool supported=budget.calculate(true,true,true,count,reference.airtime[22],reference.airtime[53],air,reference.airtime[185]);
  EXPECT_EQ(1500u+16*reference.airtime[53],budget.login_wait);
  EXPECT_EQ(1500u+(6*air+250)*(2*count),TraceTimingPolicy::attemptTimeout(count,air));
  if(supported) { EXPECT_LE(budget.duration,120000u); EXPECT_GT(budget.duration,budget.report_reserve); }
  for(unsigned i=1;i<255;i++) EXPECT_LE(reference.airtime[i],reference.airtime[i+1]);
}
INSTANTIATE_TEST_SUITE_P(AllPHYWidthsAndLengths,PHYTiming,
    ::testing::Combine(::testing::Range(0,8),::testing::Values(1,2),::testing::Range(1,11)));
TEST(CompanionTiming, AustraliaNarrowReferenceIncludesPreambleAndCodingRate) {
  EXPECT_EQ(320u,LORA_REFERENCE[0].airtime[53]);
  EXPECT_GT(LORA_REFERENCE[0].airtime[53],LORA_REFERENCE[5].airtime[53]);
  EXPECT_GT(LORA_REFERENCE[0].airtime[53],LORA_REFERENCE[6].airtime[53]);
  EXPECT_GE(LORA_REFERENCE[0].airtime[53],LORA_REFERENCE[7].airtime[53]);
}

class LoginReturnRoutes : public CompanionFlow, public ::testing::WithParamInterface<std::tuple<int,int>> {};
TEST_P(LoginReturnRoutes, ExactRoundTripPathFromAuthenticatedLoginAcrossAllLengthsAndWidths) {
  int width=std::get<0>(GetParam()),count=std::get<1>(GetParam());
  startLogin(); loginReply(3600,count-1,width); tick(3601);
  ASSERT_EQ(4u,radio.sent.size()); mesh::Packet p{};
  ASSERT_TRUE(mesh.tryParsePacket(&p,radio.sent[3].data(),radio.sent[3].size()));
  uint8_t via[18]{}; for(int i=0;i<(count-1)*width;i++) via[i]=0x20+i;
  TraceRoute expected{}; ASSERT_EQ(TraceRoute::VALID,TraceRoute::fromOutbound(via,((width-1)<<6)|(count-1),target.pub_key,expected));
  EXPECT_EQ(expected.bytes+9,p.payload_len); EXPECT_EQ(0,memcmp(expected.path,p.payload+9,expected.bytes));
  EXPECT_TRUE(bot.diagnostics().login_path); EXPECT_EQ(1u,bot.diagnostics().discovery_starts);
  finish(3701); traceReply(3,4000); finishUntil(12500);
  EXPECT_TRUE(bot.diagnostics().has_result); EXPECT_EQ(32,pool.getFreeCount());
}
INSTANTIATE_TEST_SUITE_P(AllLengthsBothWidths,LoginReturnRoutes,
    ::testing::Combine(::testing::Values(1,2),::testing::Range(1,11)));
TEST_F(CompanionFlow, SuccessfulLoginWithUnsupportedPathUsesOnlyTheRemainingDiscovery) {
  startLogin(); loginReply(3600,1,3); tick(3601);
  EXPECT_FALSE(bot.diagnostics().login_path); EXPECT_EQ(1u,bot.diagnostics().route_rejections);
  EXPECT_EQ(1u,bot.diagnostics().login_starts); EXPECT_EQ(2u,bot.diagnostics().discovery_starts);
  EXPECT_TRUE(bot.diagnostics().control[2].sent); EXPECT_EQ(0u,bot.diagnostics().starts);
}
TEST_F(CompanionFlow, PlainLoginResponseEstablishesReadinessButDoesNotInventAPath) {
  startLogin(); uint8_t body[13]{},nonce[4]{},secret[32]; ClientInfo client{}; client.permissions=PERM_ACL_GUEST;
  encodeRepeaterLoginReply(body,client,1800000000,nonce,2); target.calcSharedSecret(secret,mesh.self_id.pub_key);
  auto p=remote.createDatagram(PAYLOAD_TYPE_RESPONSE,mesh.self_id,secret,body,13);
  ASSERT_NE(nullptr,p); p->header|=ROUTE_TYPE_FLOOD; radio.incoming=wire(*p); remote.releasePacket(p);
  tick(3600); tick(3601);
  EXPECT_EQ(1u,bot.diagnostics().accepted_logins); EXPECT_FALSE(bot.diagnostics().login_path);
  EXPECT_EQ(2u,bot.diagnostics().discovery_starts); EXPECT_EQ(0u,bot.diagnostics().starts);
}
TEST_F(CompanionFlow, LateLoginDuringInFlightRetryLeavesDispatcherOwnershipIntact) {
  startLogin(); tick(6602); tick(6852); tick(6853);
  ASSERT_EQ(2u,bot.diagnostics().login_starts); loginReply(6900);
  EXPECT_FALSE(bot.diagnostics().login_path); EXPECT_FALSE(bot.diagnostics().control[3].cancelled);
  finish(6950); tick(6951); EXPECT_TRUE(bot.diagnostics().login_path);
  EXPECT_EQ(1u,bot.diagnostics().starts); EXPECT_EQ(2u,bot.diagnostics().login_starts);
}
