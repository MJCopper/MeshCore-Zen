#include "../sensor_mocks/SensorIntegrationFixture.h"

class SensorMultiFlood : public SensorIntegration {
protected:
  void SetUp() override {
    SensorIntegration::SetUp(); mesh.complete_trace_ack_immediately=false;
    ASSERT_EQ(DoublerSettings::DEFAULTED,mesh.beginFlood(&fs));
    mesh.applyFloodRadio(916.575,62.5,7,7);
  }
  void mode(unsigned value) {
    char command[40],reply[160]; snprintf(command,sizeof(command),"set multi.flood %u",value);
    ASSERT_TRUE(mesh.floodCommand(command,reply)); ASSERT_STREQ("OK",reply);
  }
  mesh::Packet* send(uint8_t type=PAYLOAD_TYPE_GRP_TXT,uint8_t width=1,bool scoped=false,bool direct=false) {
    auto p=mesh.obtainNewPacket(); EXPECT_NE(nullptr,p);
    p->header=type<<PH_TYPE_SHIFT; p->payload_len=5;
    for(int i=0;i<5;i++) p->payload[i]=0x30+i;
    if(direct) mesh.sendZeroHop(p);
    else if(scoped) { uint16_t codes[2]{0xabcd,0}; mesh.sendFlood(p,codes,0,width); }
    else mesh.sendFlood(p,0,width);
    return p;
  }
  void command(const char* text,char* reply) { ASSERT_TRUE(mesh.floodCommand(text,reply)); }
};
TEST_F(SensorMultiFlood, MissingSettingsDefaultOffWithoutWritingAndOriginalStillCompletes) {
  auto writes=fs.writes; char reply[160]; command("get multi.flood",reply); EXPECT_STREQ("> 0",reply);
  EXPECT_EQ(writes,fs.writes); EXPECT_FALSE(fs.exists("/doubler.json"));
  send(); tick(100); finish(200); tick(5000);
  EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(0u,mesh.floodStats.retained); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, TextModesRepeatExactBytesAcrossScopesAndHashWidthsWithoutThirdTx) {
  for(unsigned setting : {1u,2u}) for(uint8_t width : {1,2,3}) for(bool scoped : {false,true}) {
    mode(setting); auto base=radio.sent.size(); auto now=g_mock_millis;
    send(PAYLOAD_TYPE_GRP_TXT,width,scoped); tick(now+100); finish(now+200);
    EXPECT_EQ(31,pool.getFreeCount()); EXPECT_TRUE(mesh.hasOptionalFloodWork());
    tick(now+699); EXPECT_EQ(base+1,radio.sent.size());
    tick(now+1001); ASSERT_EQ(base+2,radio.sent.size()); EXPECT_EQ(radio.sent[base],radio.sent[base+1]);
    finish(now+1101); tick(now+5000); EXPECT_EQ(base+2,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
  }
}
TEST_F(SensorMultiFlood, ModeOneExcludesOtherPayloadsAndModeTwoCoversThem) {
  for(uint8_t type : {PAYLOAD_TYPE_ACK,PAYLOAD_TYPE_ADVERT,PAYLOAD_TYPE_REQ,PAYLOAD_TYPE_ANON_REQ,PAYLOAD_TYPE_RESPONSE,PAYLOAD_TYPE_PATH,PAYLOAD_TYPE_GRP_DATA}) {
    mode(1); auto base=radio.sent.size(); auto now=g_mock_millis;
    send(type); tick(now+100); finish(now+200); tick(now+1200); EXPECT_EQ(base+1,radio.sent.size());
    mode(2); now=g_mock_millis; base=radio.sent.size();
    send(type); tick(now+100); finish(now+200); tick(now+1001); finish(now+1101);
    EXPECT_EQ(base+2,radio.sent.size()); EXPECT_EQ(radio.sent[base],radio.sent[base+1]); EXPECT_EQ(32,pool.getFreeCount());
  }
}
TEST_F(SensorMultiFlood, DirectAndZeroHopPacketsAreNeverRepeated) {
  mode(2); send(PAYLOAD_TYPE_GRP_TXT,1,false,true); tick(100); finish(200); tick(5000);
  EXPECT_EQ(1u,radio.sent.size()); EXPECT_FALSE(mesh.hasOptionalFloodWork()); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, ModeChangesCancelImmediatelyAndCannotReviveWaitingCopy) {
  mode(1); send(); tick(100); finish(200); mode(0); mode(1); tick(1001);
  EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(1u,mesh.floodStats.cancelled); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, SaveFailureKeepsEffectiveModeAndPendingCopy) {
  mode(1); send(); tick(100); finish(200); fs.fail_rename=true;
  char reply[160]; command("set multi.flood 0",reply); EXPECT_NE(nullptr,strstr(reply,"unable to save"));
  command("get multi.flood",reply); EXPECT_STREQ("> 1",reply); tick(1001); finish(1101);
  EXPECT_EQ(2u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, SettingsPersistWithoutTouchingBaselineFilesAndStrictArgumentsFail) {
  mode(2); auto writes=fs.writes; auto original=*fs.files["/doubler.json"];
  char reply[160];
  for(const char* invalid : {"set multi.flood -1","set multi.flood 3","set multi.flood 1abc","set multi.flood 1 2","get multi.flood 1"}) {
    command(invalid,reply); EXPECT_NE(nullptr,strstr(reply,"Err"));
  }
  EXPECT_EQ(writes,fs.writes); EXPECT_EQ(original,*fs.files["/doubler.json"]);
  ASSERT_EQ(DoublerSettings::VALID,mesh.beginFlood(&fs)); command("get multi.flood",reply); EXPECT_STREQ("> 2",reply);
  EXPECT_FALSE(fs.exists("/prefs.json")); EXPECT_FALSE(fs.exists("/com_prefs")); EXPECT_FALSE(fs.exists("/doubler.tmp"));
}
TEST_F(SensorMultiFlood, CorruptFileFailsOffAndFutureSchemaCannotBeOverwritten) {
  auto put=[&](const char* text){ fs.files["/doubler.json"]=std::make_shared<std::vector<uint8_t>>(text,text+strlen(text)); };
  put("bad"); EXPECT_EQ(DoublerSettings::CORRUPT,mesh.beginFlood(&fs));
  char reply[160]; command("get multi.flood",reply); EXPECT_STREQ("> 0",reply); mode(0);
  put("{\"schema\":2,\"multi_flood\":2}"); EXPECT_EQ(DoublerSettings::UNSUPPORTED,mesh.beginFlood(&fs));
  auto writes=fs.writes; command("set multi.flood 1",reply); EXPECT_NE(nullptr,strstr(reply,"preserved")); EXPECT_EQ(writes,fs.writes);
}
TEST_F(SensorMultiFlood, AppliedRadioAndPacketContextChangesInvalidateCopiesWithoutFlashWrites) {
  mode(2);
  for(int change=0;change<4;change++) {
    auto now=g_mock_millis; send(); tick(now+100); finish(now+200); auto writes=fs.writes;
    if(change==0) { mesh.applyFloodRadio(917,62.5,7,7); mesh.applyFloodRadio(916.575,62.5,7,7); }
    if(change==1) { mesh.outbound_width=2; mesh.changeFloodContext(); mesh.outbound_width=1; mesh.changeFloodContext(); }
    if(change==2) { mesh.default_scope.key[0]=1; mesh.changeFloodContext(); mesh.default_scope.key[0]=0; mesh.changeFloodContext(); }
    if(change==3) { mesh.self_id.pub_key[31]^=1; mesh.changeFloodContext(); mesh.self_id.pub_key[31]^=1; mesh.changeFloodContext(); }
    auto count=radio.sent.size(); tick(now+1001); EXPECT_EQ(count,radio.sent.size()); EXPECT_EQ(writes,fs.writes); EXPECT_EQ(32,pool.getFreeCount());
  }
}
TEST_F(SensorMultiFlood, BusyChannelLetsCopyExpireWithoutForcingTxOrChangingNormalTraffic) {
  mode(2); send(); tick(100); finish(200); radio.busy=true;
  tick(1001); tick(2500); tick(3200); radio.busy=false; tick(5000);
  EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(1u,mesh.floodStats.expired); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, NormalPacketDueBeforeRepeatAlwaysWins) {
  mode(2); send(); tick(100); finish(200); send(PAYLOAD_TYPE_REQ); tick(1001);
  ASSERT_EQ(2u,radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_REQ,(radio.sent[1][0]>>PH_TYPE_SHIFT)&15);
  finish(1101); tick(1102); EXPECT_EQ(3u,radio.sent.size());
}
TEST_F(SensorMultiFlood, ReceivedForwardedZeroPathPacketIsExplicitlyExcluded) {
  mode(2); mesh.raw_forward=true;
  mesh::Packet p{}; p.header=(PAYLOAD_TYPE_GRP_TXT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD; p.payload_len=3;
  uint8_t bytes[MAX_TRANS_UNIT]; auto size=p.writeTo(bytes); radio.incoming.assign(bytes,bytes+size);
  tick(100); finish(200); tick(1001); EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, FailedOptionalStartCannotFailTheSuccessfulLogicalDeliveryOrCreateThirdTx) {
  mode(1); send(); tick(100); finish(200); radio.fail=true; tick(1001);
  radio.fail=false; tick(5000); EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(1u,mesh.floodStats.failed);
  EXPECT_EQ(32,pool.getFreeCount()); EXPECT_EQ(0u,bot.diagnostics().local_failures);
}
TEST_F(SensorMultiFlood, TraceOwnerCancellationAndDeadlineDoNotCancelUnrelatedTraffic) {
  mode(2); auto p=send(); mesh.tagTracePacket(p,123,10000); tick(100); finish(200);
  mesh.cancelTraceCopies(123); send(PAYLOAD_TYPE_REQ); tick(300); finish(400); tick(1200); finish(1300);
  EXPECT_EQ(3u,radio.sent.size()); EXPECT_EQ(radio.sent[1],radio.sent[2]); EXPECT_EQ(32,pool.getFreeCount());
  p=send(); mesh.tagTracePacket(p,124,1600); tick(1400); finish(1500); tick(1600); tick(2600);
  EXPECT_EQ(4u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, CancelInFlightOriginalOrRepeatNeverFreesRadioOwnedPacket) {
  mode(1); auto p=send(); mesh.tagTracePacket(p,123,10000); tick(100); mesh.cancelTraceCopies(123);
  tick(150); EXPECT_EQ(31,pool.getFreeCount()); finish(200); tick(1001); EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
  p=send(); mesh.tagTracePacket(p,124,10000); tick(1100); finish(1200); tick(2001);
  ASSERT_EQ(3u,radio.sent.size()); mesh.cancelTraceCopies(124); tick(2050); EXPECT_EQ(31,pool.getFreeCount());
  finish(2101); tick(5000); EXPECT_EQ(3u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, OptionalAirtimeIsCountedTwiceAndDiagnosticsAreReadOnly) {
  mode(1); send(); tick(100); finish(200); tick(1001); finish(1101);
  EXPECT_EQ(200u,mesh.getTotalAirTime()); EXPECT_EQ(2u,mesh.getNumSentFlood());
  auto writes=fs.writes; auto packets=radio.sent.size(); char reply[160]; command("get multi.flood.stats",reply);
  EXPECT_NE(nullptr,strstr(reply,"tx=1")); EXPECT_EQ(writes,fs.writes); EXPECT_EQ(packets,radio.sent.size());
}
TEST_F(SensorMultiFlood, PublicBotCannotChangeModeAndItsReplyIsDoubledOnce) {
  mode(1); radio.incoming=SensorIntegration::command("Alice: !hillvue set multi.flood 2"); tick(100);
  char reply[160]; command("get multi.flood",reply); EXPECT_STREQ("> 1",reply);
  radio.incoming=SensorIntegration::command("Alice: !hillvue ping"); tick(3000); completeUntil(8000);
  ASSERT_EQ(2u,radio.sent.size()); EXPECT_EQ(radio.sent[0],radio.sent[1]); EXPECT_NE(std::string::npos,publicText(0).find("Pong"));
}
TEST_F(SensorMultiFlood, TraceAcknowledgementAndResultCopiesDoNotRestartLogicalTimers) {
  mode(1); radio.incoming=SensorIntegration::command("Alice: !hillvue trace"); tick(100);
  auto end=bot.traceDeadline(); finish(200); tick(201); finish(301); traceReply(1,600); completeUntil(7000);
  EXPECT_TRUE(bot.diagnostics().has_result); EXPECT_EQ(1u,bot.diagnostics().starts); EXPECT_EQ(0u,bot.diagnostics().report_failures);
  EXPECT_EQ(end,bot.traceDeadline()); EXPECT_EQ(TraceReportDelivery::DELIVERED,bot.diagnostics().delivery);
  EXPECT_EQ(32,pool.getFreeCount()); EXPECT_GE(mesh.floodStats.transmitted,1u);
}
TEST_F(SensorMultiFlood, TraceCancelRemovesPendingAcknowledgementCopy) {
  mode(1); radio.incoming=SensorIntegration::command("Alice: !hillvue trace"); tick(100); finish(200);
  radio.incoming=SensorIntegration::command("Alice: !hillvue trace cancel"); tick(300); completeUntil(5000);
  unsigned acknowledgements=0;
  for(size_t i=0;i<radio.sent.size();i++) if(((radio.sent[i][0]>>PH_TYPE_SHIFT)&15)==PAYLOAD_TYPE_GRP_TXT && publicText(i).find("Trace requested")!=std::string::npos) ++acknowledgements;
  EXPECT_EQ(1u,acknowledgements); EXPECT_EQ(32,pool.getFreeCount()); EXPECT_EQ(0u,bot.diagnostics().starts);
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }

class MultiFloodTarget : public SensorMultiFlood {
protected:
  void startTarget() {
    mesh.self_id=mesh::LocalIdentity(&random); bot.begin(&fs);
    mesh::LocalIdentity target(&random); uint8_t app[40]; auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(app);
    bot.onAdvertRecv(nullptr,target,100,app,len);
    char text[64]; snprintf(text,sizeof(text),"Alice: !hillvue trace %02x%02x",target.pub_key[0],target.pub_key[1]);
    radio.incoming=SensorIntegration::command(text); tick(100);
  }
};
TEST_F(MultiFloodTarget, SixPhysicalLoginFramesRemainThreeLogicalAttemptsWithFreshTags) {
  mode(2); startTarget(); auto end=bot.traceDeadline(); completeUntil(25000);
  EXPECT_EQ(3u,bot.diagnostics().login_starts); unsigned logins=0;
  for(const auto& bytes:radio.sent) if(((bytes[0]>>PH_TYPE_SHIFT)&15)==PAYLOAD_TYPE_ANON_REQ) ++logins;
  EXPECT_EQ(6u,logins); EXPECT_EQ(0u,bot.diagnostics().starts); EXPECT_EQ(end,bot.traceDeadline());
  EXPECT_LT(bot.diagnostics().control[1].tag,bot.diagnostics().control[3].tag);
  EXPECT_LT(bot.diagnostics().control[3].tag,bot.diagnostics().control[4].tag);
  EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, FailedTerminalCopyPreservesDeliveredTraceAndNeverPublishesFailure) {
  mode(1); radio.incoming=SensorIntegration::command("Alice: !hillvue trace"); tick(100);
  finish(200); tick(201); finish(301); traceReply(1,600);
  for(uint32_t now=650;now<5000;now+=50) {
    if(radio.transmitting) radio.complete=true; tick(now);
    if(bot.diagnostics().delivery==TraceReportDelivery::DELIVERED) break;
  }
  ASSERT_EQ(TraceReportDelivery::DELIVERED,bot.diagnostics().delivery); ASSERT_TRUE(mesh.hasOptionalFloodWork());
  auto sent=radio.sent.size(); radio.fail=true; completeUntil(g_mock_millis+1500); radio.fail=false;
  EXPECT_EQ(sent,radio.sent.size()); EXPECT_EQ(TraceReportDelivery::DELIVERED,bot.diagnostics().delivery);
  EXPECT_EQ(0u,bot.diagnostics().report_failures); EXPECT_EQ(1u,mesh.floodStats.failed); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, OldTerminalCopyCannotCompleteOrRescheduleANewerTrace) {
  mode(1); radio.incoming=SensorIntegration::command("Alice: !hillvue trace"); tick(100);
  finish(200); tick(201); finish(301); traceReply(1,600);
  for(uint32_t now=650;now<5000;now+=50) {
    if(radio.transmitting) radio.complete=true; tick(now);
    if(bot.diagnostics().delivery==TraceReportDelivery::DELIVERED) break;
  }
  ASSERT_TRUE(mesh.hasOptionalFloodWork()); auto now=g_mock_millis;
  radio.incoming=SensorIntegration::command("Bob: !hillvue trace"); tick(now+1);
  auto new_end=bot.traceDeadline(); finish(now+101); tick(now+102); finish(now+202);
  completeUntil(now+1800);
  EXPECT_EQ(new_end,bot.traceDeadline()); EXPECT_EQ(1u,bot.diagnostics().starts);
  EXPECT_FALSE(bot.diagnostics().has_result); EXPECT_EQ(TraceReportDelivery::NONE,bot.diagnostics().delivery);
}
TEST_F(SensorMultiFlood, LowPoolHeadroomSkipsCopyAndReturnsEverySlot) {
  mode(2); send(); tick(100); std::vector<mesh::Packet*> held;
  while(pool.getFreeCount()>3) held.push_back(mesh.obtainNewPacket());
  finish(200); EXPECT_FALSE(mesh.hasOptionalFloodWork()); EXPECT_EQ(1u,mesh.floodStats.skipped);
  for(auto p:held) mesh.releasePacket(p); tick(1001); EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
}

TEST_F(SensorMultiFlood, TraceCopyNeedsFullTxWatchdogBeforeItsFrozenDeadline) {
  mode(1); auto p=send(); mesh.tagTracePacket(p,123,1500); tick(100); finish(200); tick(1001);
  EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(1u,mesh.floodStats.skipped); EXPECT_EQ(32,pool.getFreeCount());
}
TEST_F(SensorMultiFlood, OwnerDeadlineIsRecheckedAfterRadioPreparationAdvancesTime) {
  mode(1); auto p=send(); mesh.tagTracePacket(p,123,2300); tick(100); finish(200);
  radio.advance_at_estimate=radio.estimate_calls+3; radio.advance_ms=1400;
  tick(1001); EXPECT_EQ(2401u,g_mock_millis); EXPECT_EQ(1u,radio.sent.size()); EXPECT_EQ(32,pool.getFreeCount());
}
