#include "../sensor_mocks/SensorIntegrationFixture.h"
#include "TraceRequestClock.h"
#include "TraceAdvertRouteCache.h"
#include <helpers/RepeaterLogin.h>

TEST(RequestClock, RebootAndRollbackNeverReusePossiblyEmittedValues) {
  MemoryFileSystem fs; uint8_t key[32]{1}; uint32_t a,b;
  TraceRequestClock clock; clock.begin(&fs,key); ASSERT_TRUE(clock.next(1800000000,a));
  auto writes=fs.writes; ASSERT_TRUE(clock.next(1,b)); EXPECT_GT(b,a); EXPECT_EQ(writes,fs.writes);
  auto upper=clock.reservedUpper(); TraceRequestClock reboot; reboot.begin(&fs,key);
  ASSERT_TRUE(reboot.next(1,b)); EXPECT_GE(b,upper); EXPECT_GT(b,a);
  ASSERT_TRUE(reboot.next(1900000000,b)); EXPECT_EQ(1900000000u,b);
}
TEST(RequestClock, BothReplicasMustCommitAndSingleSurvivorIsConservative) {
  MemoryFileSystem fs; uint8_t key[32]{1}; uint32_t value;
  TraceRequestClock clock; clock.begin(&fs,key); ASSERT_TRUE(clock.next(1000,value));
  fs.fail_write_path="/trace_clock_b.tmp";
  EXPECT_FALSE(clock.next(3000,value)); EXPECT_FALSE(clock.healthy());
  fs.fail_write_path.clear(); fs.remove("/trace_clock_a");
  TraceRequestClock reboot; reboot.begin(&fs,key); ASSERT_TRUE(reboot.next(1,value));
  EXPECT_GE(value,1256u); // Failed newer range never emitted; surviving old bound covers all prior sends.
}
TEST(RequestClock, CorruptOrForeignJournalFailsClosedAndPolicySaveIsAtomic) {
  MemoryFileSystem fs; uint8_t key[32]{1}; uint32_t value;
  TraceRequestClock clock; clock.begin(&fs,key); ASSERT_TRUE(clock.next(1000,value));
  key[31]=2; TraceRequestClock foreign; foreign.begin(&fs,key); EXPECT_FALSE(foreign.next(2000,value));
  key[31]=0; (*fs.files["/trace_clock_a"])[0]^=1; (*fs.files["/trace_clock_b"])[0]^=1;
  TraceRequestClock corrupt; corrupt.begin(&fs,key); EXPECT_FALSE(corrupt.next(2000,value));
  TraceGuestPolicy policy; policy.begin(&fs); EXPECT_TRUE(policy.enabled());
  ASSERT_TRUE(policy.set(false)); fs.fail_rename=true; EXPECT_FALSE(policy.set(true)); EXPECT_FALSE(policy.enabled());
  fs.fail_rename=false; TraceGuestPolicy loaded; loaded.begin(&fs); EXPECT_FALSE(loaded.enabled());
}
TEST(RequestClock, ExhaustionAndMissingFilesystemNeverInventTimestamps) {
  uint8_t key[32]{}; uint32_t value=42; TraceRequestClock clock; clock.begin(nullptr,key);
  EXPECT_FALSE(clock.next(1000,value)); EXPECT_EQ(42u,value);
  MemoryFileSystem fs; clock.begin(&fs,key); EXPECT_FALSE(clock.next(UINT32_MAX-200,value));
}
TEST(FirstContact, GuestReplyIsNotAnEchoAndOnlyFreshDiscoverySuppliesRoute) {
  TraceDiscovery d; uint8_t key[32]{1}; mesh::Packet p{};
  d.begin(key,100); d.queued(&p,1000,6000); d.started(&p,100); d.completed(&p,101);
  d.tick(6101); EXPECT_EQ(TraceDiscovery::LOGIN_QUEUE,d.phase());
  d.queued(&p,1001,8000); d.started(&p,6200); d.completed(&p,6201);
  uint8_t reply[16]{}; uint32_t server=9000; memcpy(reply,&server,4); reply[12]=2;
  ASSERT_TRUE(d.loginResponse(key,reply,sizeof(reply),6300)); EXPECT_EQ(2,d.attempt());
  EXPECT_FALSE(d.owns(key,server)); EXPECT_TRUE(d.owns(key,1000));
  EXPECT_EQ(TraceDiscovery::QUEUE,d.phase());
  d.queued(&p,1002,6000); d.started(&p,6400); EXPECT_TRUE(d.current(key,1002));
  d.collect(); d.cancel(); EXPECT_FALSE(d.current(key,1002)); EXPECT_TRUE(d.owns(key,1002));
}
TEST(FirstContact, PublicOffControlCutoffUnexpectedRoleAndMalformedResponses) {
  TraceDiscovery d; uint8_t key[32]{1},other[32]{2}; mesh::Packet p{};
  d.begin(key,100,false); d.tick(6100); EXPECT_EQ(TraceDiscovery::QUEUE,d.phase()); EXPECT_EQ(2,d.attempt());
  d.tick(12100); EXPECT_EQ(TraceDiscovery::ADVERT_WAIT,d.phase());
  d.begin(key,100); d.tick(6100); d.queued(&p,100,8000); d.started(&p,6200); d.completed(&p,6201);
  uint8_t body[16]{}; uint32_t server=100; memcpy(body,&server,4); body[12]=2;
  EXPECT_FALSE(d.loginResponse(other,body,16,6300)); EXPECT_FALSE(d.loginResponse(key,body,8,6300));
  body[6]=1; body[7]=3; EXPECT_TRUE(d.loginResponse(key,body,16,6300));
  EXPECT_EQ(TraceDiscovery::ADVERT_WAIT,d.phase());
  d.begin(key,UINT32_MAX-100); d.tick(uint32_t(UINT32_MAX-100+TraceTimingPolicy::CONTROL_TIMEOUT_MILLIS));
  EXPECT_EQ(TraceDiscovery::ADVERT_WAIT,d.phase());
}
TEST(RepeaterFirstContact, ProductionHelperCreatesOnlyConfiguredRoleAndFreshReplayState) {
  ClientACL acl; TestRandom rng; mesh::LocalIdentity sender(&rng),target(&rng); uint8_t secret[32],nonce[4]{};
  sender.calcSharedSecret(secret,target); bool persist=true;
  EXPECT_EQ(nullptr,authenticateRepeaterClient(acl,sender,secret,100,100,"","admin","locked",persist));
  auto c=authenticateRepeaterClient(acl,sender,secret,100,100,"","admin","",persist);
  ASSERT_NE(nullptr,c); EXPECT_FALSE(persist); EXPECT_EQ(PERM_ACL_GUEST,c->permissions&3);
  EXPECT_EQ(100u,c->last_timestamp); EXPECT_EQ(0,memcmp(secret,c->shared_secret,32));
  uint8_t reply[13]; EXPECT_EQ(13,encodeRepeaterLoginReply(reply,*c,200,nonce,2));
  uint32_t server; memcpy(&server,reply,4); EXPECT_EQ(200u,server); EXPECT_EQ(0,reply[4]); EXPECT_EQ(2,reply[12]);
  c->last_timestamp=1000;
  ASSERT_NE(nullptr,authenticateRepeaterClient(acl,sender,secret,101,100,"","admin","",persist));
  EXPECT_EQ(1000u,c->last_timestamp); // Login readiness does not repair a stale subsequent REQ.
}

class AdvertConversion : public ::testing::TestWithParam<std::tuple<int,int>> {};
TEST_P(AdvertConversion, ExactOutwardAndReturnBytesForEveryRouteAndWidth) {
  int width=std::get<0>(GetParam()),hops=std::get<1>(GetParam()); mesh::Packet p{}; p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;
  p.setPathHashSizeAndCount(width,hops-1); uint8_t key[32]{0x71,0x88};
  for(int i=0;i<(hops-1)*width;i++) p.path[i]=0x20+i;
  TraceRoute route{}; ASSERT_TRUE(TraceAdvertRouteCache::convert(p,key,route));
  EXPECT_EQ(hops,route.repeaters); EXPECT_EQ((2*hops-1)*width,route.bytes);
  for(int i=0;i<hops-1;i++) {
    EXPECT_EQ(0,memcmp(route.path+i*width,p.path+(hops-2-i)*width,width));
    EXPECT_EQ(0,memcmp(route.path+(hops+i)*width,p.path+i*width,width));
  }
  EXPECT_EQ(0,memcmp(route.path+(hops-1)*width,key,width));
}
INSTANTIATE_TEST_SUITE_P(AllLengthsAndWidths,AdvertConversion,
  ::testing::Combine(::testing::Values(1,2),::testing::Range(1,11)));

TEST(AdvertRoutes, ScopeFreshnessPersistenceAndProbationNeverReplayAResult) {
  MemoryFileSystem fs; TraceAdvertRouteCache cache; uint8_t self[32]{1},key[32]{0x71,0x88};
  cache.begin(&fs,self,0,1000,true); mesh::Identity id(key); mesh::Packet p{};
  p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD; p.setPathHashSizeAndCount(2,1); p.path[0]=0x20;p.path[1]=0x30;
  uint8_t data[40]; auto length=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(data);
  mesh::PacketScope scope{}; scope.kind=mesh::PacketScope::RESOLVED; scope.key[0]=1;
  ASSERT_TRUE(cache.observe(&p,id,1000,data,length,100,1000,true,7,scope));
  EXPECT_FALSE(cache.observe(&p,id,1000,data,length,1000,1001,true,7,scope));
  TraceRoute route{}; ASSERT_TRUE(cache.best(key,7,scope,101,route)); EXPECT_FALSE(cache.best(key,8,scope,101,route));
  EXPECT_FALSE(cache.best(key,7,{},101,route)); EXPECT_FALSE(cache.best(key,7,scope,100+TraceAdvertRouteCache::TTL,route));
  cache.loop(30100); ASSERT_TRUE(fs.exists("/trace_adverts"));
  EXPECT_LT(fs.files["/trace_adverts"]->size(),1024u); auto writes=fs.writes;
  cache.loop(60100); EXPECT_EQ(writes,fs.writes);
  TraceAdvertRouteCache reboot; reboot.begin(&fs,self,0,0,false);
  EXPECT_TRUE(reboot.best(key,7,scope,1,route)); // candidate only, always probed anew
  TraceAdvertRouteCache expired; expired.begin(&fs,self,0,87400,true); EXPECT_FALSE(expired.best(key,7,scope,1,route));
  self[31]=2; TraceAdvertRouteCache foreign; foreign.begin(&fs,self,0,1001,true); EXPECT_FALSE(foreign.best(key,7,scope,1,route));
}
TEST(AdvertRoutes, TwoAlternativesAndFailedSaveRetainLastGoodState) {
  MemoryFileSystem fs; TraceAdvertRouteCache cache; uint8_t self[32]{1},key[32]{0x71};
  cache.begin(&fs,self,0,1000,true); mesh::Identity id(key); mesh::Packet p{};
  p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD; p.setPathHashSizeAndCount(1,1);p.path[0]=0x20;
  uint8_t data[40]; auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(data);
  ASSERT_TRUE(cache.observe(&p,id,1000,data,len,1,1000,true,0,{})); cache.loop(30001);
  p.path[0]=0x30; ASSERT_TRUE(cache.observe(&p,id,1001,data,len,30002,1001,true,0,{}));
  TraceRoute a{},b{}; ASSERT_TRUE(cache.best(key,0,{},30003,a)); ASSERT_TRUE(cache.best(key,0,{},30003,b,&a)); EXPECT_FALSE(a.equals(b));
  fs.fail_rename=true; cache.loop(630001);
  TraceAdvertRouteCache reboot; reboot.begin(&fs,self,0,1002,true); TraceRoute r{};
  ASSERT_TRUE(reboot.best(key,0,{},1,r)); EXPECT_EQ(0x20,r.path[0]);
}

class RepairRadio : public SensorIntegration {
protected:
  TestRadio remote_radio; StaticPoolPacketManager remote_pool{32}; SimpleMeshTables remote_tables;
  TestMesh remote{remote_radio,clock,random,rtc,remote_pool,remote_tables}; mesh::LocalIdentity target;
  void SetUp() override {
    SensorIntegration::SetUp(); mesh.complete_trace_ack_immediately=false;
    mesh.self_id=mesh::LocalIdentity(&random); target=mesh::LocalIdentity(&random); remote.self_id=target;
    remote.begin(); bot.begin(&fs);
  }
  std::vector<uint8_t> wire(const mesh::Packet& p) {
    std::vector<uint8_t> bytes{p.header};
    if(p.hasTransportCodes()) { auto c=(const uint8_t*)p.transport_codes; bytes.insert(bytes.end(),c,c+4); }
    bytes.push_back(p.path_len); bytes.insert(bytes.end(),p.path,p.path+p.getPathByteLen());
    bytes.insert(bytes.end(),p.payload,p.payload+p.payload_len); return bytes;
  }
  void advert(uint32_t at=10,uint8_t via=0x20) {
    g_mock_millis=at; uint8_t app[40]; auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target Repeater").encodeTo(app);
    auto p=remote.createAdvert(target,app,len); ASSERT_NE(nullptr,p);
    p->header|=ROUTE_TYPE_FLOOD; p->setPathHashSizeAndCount(1,1); p->path[0]=via;
    radio.incoming=wire(*p);remote.releasePacket(p);tick(at);
  }
  void request(uint32_t at=100) {
    char text[60];snprintf(text,sizeof(text),"Alice: !hillvue trace %02x%02x",target.pub_key[0],target.pub_key[1]);
    radio.incoming=command(text);tick(at);
  }
  void guestOff() { char reply[80]; ASSERT_TRUE(bot.tracePolicyCommand("set trace.login off",reply)); EXPECT_STREQ("OK",reply); }
};
TEST_F(RepairRadio, VerifiedAdvertSuppliesLoginFreeFallbackAndNewRoundTripResult) {
  guestOff(); advert(); request();finish(200);tick(201);finish(301);
  tick(6200);tick(6201);finish(6301);tick(12200);tick(12201);
  ASSERT_EQ(4u,radio.sent.size());
  mesh::Packet trace{};ASSERT_TRUE(mesh.tryParsePacket(&trace,radio.sent[3].data(),radio.sent[3].size()));
  EXPECT_EQ(PAYLOAD_TYPE_TRACE,trace.getPayloadType()); EXPECT_EQ(0x20,trace.payload[9]); EXPECT_EQ(target.pub_key[0],trace.payload[10]);
  EXPECT_TRUE(bot.diagnostics().advert_route); EXPECT_EQ(0u,bot.diagnostics().login_starts);
  finish(12301);traceReply(3,13000);tick(17000);finish(17100);
  ASSERT_EQ(5u,radio.sent.size());EXPECT_NE(std::string::npos,publicText(4).find("avg SNR"));
  EXPECT_EQ(32,pool.getFreeCount()); tick(30100); EXPECT_TRUE(fs.exists("/trace_adverts"));
}
TEST_F(RepairRadio, InvalidSignatureCannotHarvestAdvertRoute) {
  uint8_t app[40]; auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(app);
  auto p=remote.createAdvert(target,app,len);p->header|=ROUTE_TYPE_FLOOD;p->setPathHashSizeAndCount(1,0);
  auto bytes=wire(*p); remote.releasePacket(p); bytes.back()^=1;
  radio.incoming=bytes;tick(10);request();tick(3000);finish(3100);
  ASSERT_EQ(1u,radio.sent.size());EXPECT_NE(std::string::npos,publicText(0).find("unknown repeater"));
  EXPECT_EQ(0u,bot.diagnostics().advert_observations);
}
TEST_F(RepairRadio, BrokenJournalStillAllowsAdvertOnlyTraceWithoutAuthPackets) {
  fs.files["/trace_clock_a"]=std::make_shared<std::vector<uint8_t>>(4,0xff); bot.begin(&fs);
  advert();request();finish(200);tick(201);tick(202);
  ASSERT_EQ(2u,radio.sent.size());mesh::Packet p{};mesh.tryParsePacket(&p,radio.sent[1].data(),radio.sent[1].size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE,p.getPayloadType()); EXPECT_FALSE(bot.diagnostics().clock_healthy);
  EXPECT_EQ(0u,bot.diagnostics().discovery_starts);finish(302);
}

TEST_F(RepairRadio, DefaultControlScopeAndWidthDoNotChangePublicReplyContext) {
  auto region=mesh.regions.putRegion("#outbound",0); region->flags=0;
  ASSERT_EQ(1,mesh.regions.getTransportKeysFor(*region,&mesh.default_scope,1));
  mesh.outbound_width=2; advert(); request();
  ASSERT_EQ(1u,radio.sent.size()); mesh::Packet ack{};
  mesh.tryParsePacket(&ack,radio.sent[0].data(),radio.sent[0].size());
  EXPECT_EQ(ROUTE_TYPE_FLOOD,ack.getRouteType()); EXPECT_EQ(1,ack.getPathHashSize());
  finish(200);tick(201); ASSERT_EQ(2u,radio.sent.size()); mesh::Packet req{};
  mesh.tryParsePacket(&req,radio.sent[1].data(),radio.sent[1].size());
  EXPECT_EQ(ROUTE_TYPE_TRANSPORT_FLOOD,req.getRouteType()); EXPECT_EQ(2,req.getPathHashSize());
  EXPECT_EQ(mesh.default_scope.calcTransportCode(&req),req.transport_codes[0]); finish(301);
}

TEST_F(RepairRadio, AdvertAlternativeUsesOnlyTheOneRemainingProbe) {
  guestOff(); advert(10,0x20); advert(1010,0x30); request(1100);
  finish(1200);tick(1201);finish(1301);tick(7200);tick(7201);finish(7301);tick(13200);tick(13201);finish(13301);
  ASSERT_EQ(4u,radio.sent.size());
  mesh::Packet first{};mesh.tryParsePacket(&first,radio.sent[3].data(),radio.sent[3].size());
  tick(18201);tick(20500);finish(20600);
  ASSERT_EQ(5u,radio.sent.size());mesh::Packet retry{};
  mesh.tryParsePacket(&retry,radio.sent[4].data(),radio.sent[4].size());
  EXPECT_NE(first.payload[9],retry.payload[9]);
  traceReply(4,21000);completeUntil(25000);
  ASSERT_EQ(6u,radio.sent.size()); EXPECT_NE(std::string::npos,publicText(5).find("avg SNR"));
  EXPECT_EQ(2u,bot.diagnostics().starts); EXPECT_TRUE(bot.diagnostics().advert_route);
  EXPECT_EQ(32,pool.getFreeCount());
}

TEST_F(RepairRadio, CancellingQueuedGuestLoginPreventsLateControlTraffic) {
  advert();request();finish(200);tick(201);finish(301);
  radio.busy=true;mesh.cad_max_duration=UINT32_MAX;tick(6200);
  radio.incoming=command("Bob: !hillvue trace cancel");tick(6300);
  radio.busy=false;tick(9000);finish(9100);
  ASSERT_EQ(3u,radio.sent.size());EXPECT_EQ("BME680 Sensor: Trace: cancelled",publicText(2));
  tick(60000);EXPECT_EQ(3u,radio.sent.size());EXPECT_EQ(32,pool.getFreeCount());
}

TEST(FirstContact, RemoteClockRollbackIsReadinessOnlyAndStillNeedsFreshRequestTag) {
  TraceDiscovery d;uint8_t key[32]{1};mesh::Packet p{};uint8_t body[16]{};
  for(uint32_t server : {2000u,1000u}) {
    d.begin(key,100);d.tick(6100);d.queued(&p,1,8000);d.started(&p,6200);d.completed(&p,6201);
    memcpy(body,&server,4);body[12]=2;
    ASSERT_TRUE(d.loginResponse(key,body,sizeof(body),6300));
    EXPECT_EQ(server==1000,d.serverClockRegressed());EXPECT_EQ(TraceDiscovery::QUEUE,d.phase());
    EXPECT_FALSE(d.owns(key,server));d.cancel();
  }
}

TEST(AdvertRoutes, CorruptPrimaryRecoversBackupWithoutInventingAge) {
  MemoryFileSystem fs;TraceAdvertRouteCache cache;uint8_t self[32]{1},key[32]{0x71};
  cache.begin(&fs,self,0,1000,true);mesh::Identity id(key);mesh::Packet p{};
  p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;p.setPathHashSizeAndCount(1,0);
  uint8_t data[40];auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(data);
  ASSERT_TRUE(cache.observe(&p,id,1000,data,len,1,1000,true,0,{}));cache.loop(30001);
  ASSERT_TRUE(cache.observe(&p,id,1001,data,len,30002,1001,true,0,{}));cache.loop(630001);
  ASSERT_TRUE(fs.exists("/trace_adverts.bak"));(*fs.files["/trace_adverts"])[0]^=1;
  TraceAdvertRouteCache reboot;reboot.begin(&fs,self,0,0,false);TraceRoute r{};bool probation=false;
  EXPECT_TRUE(reboot.best(key,0,{},1,r,nullptr,&probation));EXPECT_TRUE(probation);
}

TEST(AdvertRoutes, FullCacheCheckpointsNeedOnlyTwoCompactFiles) {
  MemoryFileSystem fs;TraceAdvertRouteCache cache;uint8_t self[32]{1};
  cache.begin(&fs,self,0,1000,true);mesh::Packet p{};
  p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;p.setPathHashSizeAndCount(1,0);
  uint8_t data[40];auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(data);
  for(uint8_t i=0;i<48;i++) {
    uint8_t key[32]{0x71};key[31]=i;mesh::Identity id(key);
    ASSERT_TRUE(cache.observe(&p,id,1000,data,len,1,1000,true,0,{}));
  }
  cache.loop(30001);
  for(uint32_t now : {630002u,1230003u}) {
    uint8_t key[32]{0x71};mesh::Identity id(key);
    ASSERT_TRUE(cache.observe(&p,id,1001+now,data,len,now-30001,1001,true,0,{}));
    cache.loop(now);
    ASSERT_TRUE(fs.exists("/trace_adverts"));ASSERT_TRUE(fs.exists("/trace_adverts.bak"));
    EXPECT_FALSE(fs.exists("/trace_adverts.tmp"));
    EXPECT_LT(fs.files["/trace_adverts"]->size()+fs.files["/trace_adverts.bak"]->size(),16000u);
  }
}

int main(int argc,char** argv) {::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}

TEST_F(RepairRadio, PrivateDiagnosticsAreBoundedReadOnlyAndRetainControlEvidence) {
  advert(); request(); finish(200); tick(201); finish(301);
  auto sent=radio.sent.size(); auto writes=fs.writes;
  struct { char reply[160]; char guard[8]; } out{};
  memset(out.guard,'X',sizeof(out.guard));
  ASSERT_TRUE(bot.tracePolicyCommand("get trace.peer",out.reply));
  EXPECT_NE(nullptr,strstr(out.reply,"target="));
  char prefix[5]; snprintf(prefix,sizeof(prefix),"%02x%02x",target.pub_key[0],target.pub_key[1]);
  EXPECT_NE(nullptr,strstr(out.reply,prefix));
  EXPECT_NE(nullptr,strstr(out.reply,"scope="));
  ASSERT_TRUE(bot.tracePolicyCommand("get trace.control 0",out.reply));
  EXPECT_NE(nullptr,strstr(out.reply,"sent=1"));
  ASSERT_TRUE(bot.tracePolicyCommand("get trace.diag",out.reply));
  EXPECT_NE(nullptr,strstr(out.reply,"login_reply=0"));
  EXPECT_FALSE(bot.tracePolicyCommand("get trace.control 5",out.reply));
  EXPECT_FALSE(bot.tracePolicyCommand("get trace.control 0 extra",out.reply));
  EXPECT_EQ("XXXXXXXX",std::string(out.guard,sizeof(out.guard)));
  EXPECT_EQ(sent,radio.sent.size()); EXPECT_EQ(writes,fs.writes);
}

TEST_F(RepairRadio, DiagnosticCountersDistinguishCandidateFramesAndDelayedAuthenticatedLogin) {
  advert(); request(); finish(200); tick(201); finish(301);
  tick(6200); tick(6201); finish(6301); tick(15000);
  mesh::Packet reply{}; reply.header=(PAYLOAD_TYPE_PATH<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;
  reply.payload_len=2; reply.payload[0]=mesh.self_id.pub_key[0]; reply.payload[1]=target.pub_key[0];
  bot.observeRoute(&reply);
  EXPECT_EQ(1u,bot.diagnostics().candidate_replies); EXPECT_EQ(0u,bot.diagnostics().decrypted_replies);
  uint8_t body[16]{}; uint32_t server=1800000000; memcpy(body,&server,4); body[12]=2;
  EXPECT_TRUE(bot.onLoginResponse(target.pub_key,body,sizeof(body),&reply));
  EXPECT_EQ(1u,bot.diagnostics().decrypted_replies); EXPECT_EQ(0u,bot.diagnostics().ignored_logins);
  EXPECT_EQ(1u,bot.diagnostics().accepted_logins);
  char output[160]; ASSERT_TRUE(bot.tracePolicyCommand("get trace.diag",output));
  EXPECT_NE(nullptr,strstr(output,"rx=1 decrypted=1 login_reply=1 login_ignored=0"));
}

TEST(FirstContactTiming, QueueDelayAndAirtimeDoNotConsumeReplyWindow) {
  TraceDiscovery d; uint8_t key[32]{1},body[16]{}; mesh::Packet p{},other{};
  uint32_t server=100; memcpy(body,&server,4);
  d.begin(key,100); d.tick(6100); d.queued(&p,101,8000,2000);
  d.started(&p,13000); EXPECT_EQ(TraceDiscovery::LOGIN_TX,d.phase());
  EXPECT_FALSE(d.loginResponse(key,body,sizeof(body),13001));
  d.completed(&other,13001); EXPECT_EQ(TraceDiscovery::LOGIN_TX,d.phase());
  d.completed(&p,14000); EXPECT_EQ(8000u,d.remaining(14000));
  d.completed(&p,18000); EXPECT_EQ(4000u,d.remaining(18000));
  d.tick(18000); EXPECT_EQ(TraceDiscovery::LOGIN_WAIT,d.phase());
  ASSERT_TRUE(d.loginResponse(key,body,sizeof(body),21999));
  EXPECT_EQ(TraceDiscovery::QUEUE,d.phase()); EXPECT_EQ(2,d.attempt());
}
TEST(FirstContactTiming, ReplyDeadlineIsExclusiveAndDoesNotResurrectAfterExpiry) {
  TraceDiscovery d; uint8_t key[32]{1},body[16]{1}; mesh::Packet p{};
  d.begin(key,100); d.tick(6100); d.queued(&p,101,8000,2000);
  d.started(&p,13000); d.completed(&p,14000);
  EXPECT_FALSE(d.loginResponse(key,body,sizeof(body),22000));
  d.tick(22000); EXPECT_EQ(TraceDiscovery::LOGIN_QUEUE,d.phase());
  EXPECT_STREQ("Trace: guest login not acknowledged",d.failureText());
  d.completed(&p,22001); EXPECT_TRUE(d.loginResponse(key,body,sizeof(body),22002)); // late reply can serve the queued retry
}
TEST(FirstContactTiming, MissingCompletionIsTransmitFailureAndLateCompletionCannotRestart) {
  TraceDiscovery d; uint8_t key[32]{1}; mesh::Packet p{};
  d.begin(key,100); d.tick(6100); d.queued(&p,101,8000,2000); d.started(&p,13000);
  d.completed(&p,15000);
  EXPECT_EQ(TraceDiscovery::LOGIN_QUEUE,d.phase());
  EXPECT_STREQ("Trace: discovery transmit failed",d.failureText());
  d.completed(&p,15001); EXPECT_EQ(TraceDiscovery::LOGIN_QUEUE,d.phase());
}
TEST(FirstContactTiming, QueueExpiryFailureAndCancellationDoNotArmReplyWindow) {
  uint8_t key[32]{1},body[16]{1}; mesh::Packet p{};
  for(int mode=0;mode<3;mode++) {
    TraceDiscovery d; d.begin(key,100); d.tick(6100); d.queued(&p,101,8000,2000);
    if(mode==0) { d.tick(14100); EXPECT_STREQ("Trace: discovery radio busy",d.failureText()); }
    if(mode==1) { d.started(&p,13000); d.failed(&p,13001); }
    if(mode==2) { d.started(&p,13000); d.cancel(); }
    d.completed(&p,14101); EXPECT_FALSE(d.loginResponse(key,body,sizeof(body),14102));
    EXPECT_EQ(mode==1,d.login());
  }
}
TEST(FirstContactTiming, ReplyWindowIsClippedToOriginalControlCutoffAcrossWrap) {
  for(uint32_t begin : {100u,UINT32_MAX-15000}) {
    TraceDiscovery d; uint8_t key[32]{1},body[16]{1}; mesh::Packet p{};
    d.begin(key,begin); d.tick(begin+6000); d.queued(&p,101,8000,40000);
    d.started(&p,begin+13000); d.completed(&p,begin+39000);
    EXPECT_EQ(1000u,d.remaining(begin+39000));
    EXPECT_FALSE(d.loginResponse(key,body,sizeof(body),begin+40000));
    d.tick(begin+40000); EXPECT_EQ(TraceDiscovery::ADVERT_WAIT,d.phase());
  }
}
TEST_F(RepairRadio, RealDispatcherBusyQueueStillAllowsFullLoginReplyWindowAfterCompletion) {
  advert(); request(); finish(200); tick(201); finish(301);
  radio.busy=true; tick(6200); tick(12000);
  EXPECT_EQ(0u,bot.diagnostics().login_starts);
  radio.busy=false; tick(13200); finish(13300);
  ASSERT_EQ(1u,bot.diagnostics().login_starts);
  ASSERT_EQ(13300u,bot.diagnostics().control[1].completed);
  uint8_t secret[32],body[13]{},nonce[4]{},path[1]{0x20};
  target.calcSharedSecret(secret,mesh.self_id.pub_key);
  ClientInfo client{}; client.permissions=PERM_ACL_GUEST;
  encodeRepeaterLoginReply(body,client,1800000000,nonce,2);
  auto reply=remote.createPathReturn(mesh.self_id,secret,path,1,PAYLOAD_TYPE_RESPONSE,body,sizeof(body));
  ASSERT_NE(nullptr,reply); reply->header|=ROUTE_TYPE_FLOOD;
  radio.incoming=wire(*reply); remote.releasePacket(reply); tick(19000);
  EXPECT_EQ(1u,bot.diagnostics().accepted_logins);
  EXPECT_EQ(0u,bot.diagnostics().ignored_logins);
  radio.incoming=command("Alice: !hillvue trace status");
  tick(19001);
  EXPECT_EQ(0u,bot.diagnostics().control[2].tag); EXPECT_TRUE(bot.diagnostics().login_path);
}

TEST(GuestLoginRetries, ExactlyThreeAttemptsWithIndependentReplyWindows) {
  TraceDiscovery d; uint8_t key[32]{1}; mesh::Packet p{};
  d.begin(key,100); d.tick(6100);
  uint32_t now=6200;
  for(unsigned attempt=1;attempt<=3;attempt++) {
    ASSERT_EQ(TraceDiscovery::LOGIN_QUEUE,d.phase()); EXPECT_EQ(attempt,d.loginAttempt());
    ASSERT_TRUE(d.ready(now)); d.queued(&p,100+attempt,8000,2000); d.started(&p,now);
    d.completed(&p,now+100); EXPECT_EQ(8000u,d.remaining(now+100));
    d.tick(now+8100);
    if(attempt<3) { EXPECT_FALSE(d.ready(now+8100)); now+=8700; }
  }
  EXPECT_EQ(TraceDiscovery::ADVERT_WAIT,d.phase());
  d.tick(39999); EXPECT_FALSE(d.ready(39999));
  EXPECT_STREQ("Trace: guest login not acknowledged",d.failureText());
}
TEST(GuestLoginRetries, SuccessOnRetryStopsFurtherLoginsAndUsesFreshDiscovery) {
  TraceDiscovery d; uint8_t key[32]{1},body[16]{1}; mesh::Packet p{};
  d.begin(key,100); d.tick(6100); d.queued(&p,1,8000,2000); d.started(&p,6200); d.completed(&p,6300);
  d.tick(14300); ASSERT_EQ(2,d.loginAttempt());
  d.queued(&p,2,8000,2000); d.started(&p,14600); d.completed(&p,14700);
  ASSERT_TRUE(d.loginResponse(key,body,sizeof(body),15000));
  EXPECT_FALSE(d.login()); EXPECT_EQ(2,d.attempt());
  d.queued(&p,3,6000); d.started(&p,15100); EXPECT_TRUE(d.current(key,3));
  EXPECT_FALSE(d.owns(key,1)); EXPECT_FALSE(d.owns(key,2));
}
TEST(GuestLoginRetries, AbsoluteCutoffAndCancellationPreventAdditionalAttempts) {
  TraceDiscovery d; uint8_t key[32]{1}; mesh::Packet p{};
  d.begin(key,100); d.tick(6100); d.queued(&p,1,8000,40000); d.started(&p,6200);
  d.completed(&p,39000); d.tick(40100);
  EXPECT_EQ(TraceDiscovery::ADVERT_WAIT,d.phase()); EXPECT_EQ(1,d.loginAttempt());
  d.begin(key,100); d.tick(6100); d.queued(&p,1,8000,2000); d.started(&p,6200); d.completed(&p,6300);
  d.tick(14300); d.cancel(); d.tick(15000); EXPECT_FALSE(d.ready(15000)); EXPECT_EQ(TraceDiscovery::IDLE,d.phase());
}
TEST_F(RepairRadio, RealDispatcherSendsTwoGuestRetriesWithDistinctDurableTags) {
  advert(); request(); finish(200); tick(201); finish(301);
  tick(6200); tick(6201); finish(6301);
  tick(14301); tick(14552); tick(14553); finish(14653);
  tick(22653); tick(23154); tick(23155); finish(23255);
  ASSERT_EQ(3u,bot.diagnostics().login_starts);
  const auto& evidence=bot.diagnostics();
  EXPECT_LT(evidence.control[1].tag,evidence.control[3].tag);
  EXPECT_LT(evidence.control[3].tag,evidence.control[4].tag);
  EXPECT_TRUE(evidence.control[1].sent); EXPECT_TRUE(evidence.control[3].sent); EXPECT_TRUE(evidence.control[4].sent);
  tick(31255); tick(40000); EXPECT_EQ(3u,bot.diagnostics().login_starts);
  tick(60100); EXPECT_EQ(32,pool.getFreeCount());
}
