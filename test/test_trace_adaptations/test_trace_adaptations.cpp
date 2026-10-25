#include <gtest/gtest.h>
#include "MemoryFS.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RequestRTC.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/TraceAdvertRouteCache.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/TraceTransactionBudget.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenPrefsCodec.h"
using namespace zen;
static const uint8_t SELF[32]={0x11},TARGET[32]={0x22};
class Wall:public mesh::RTCClock {
public:uint32_t now=1700000000;
  uint32_t getCurrentTime()override{return now;}
  void setCurrentTime(uint32_t t)override{now=t;}
};
TEST(TraceClock, SharedVirtualClockSurvivesRebootRollbackAndNeverChangesWallTime) {
  MemoryFS fs;Wall wall;RequestRTC first(wall);first.begin(&fs,SELF);
  mesh::RTCClock* shared=&first;
  uint32_t login=shared->getCurrentTimeUnique(),telemetry=shared->getCurrentTimeUnique();
  EXPECT_GT(telemetry,login);EXPECT_EQ(1700000000u,wall.now);
  RequestRTC reboot(wall);reboot.begin(&fs,SELF);wall.now=100;
  uint32_t trace=reboot.getCurrentTimeUnique();EXPECT_GT(trace,telemetry);
  EXPECT_GT(reboot.getCurrentTimeUnique(),trace);EXPECT_EQ(100u,wall.now);
  wall.now=1800000000;EXPECT_GE(reboot.getCurrentTimeUnique(),wall.now);
}
TEST(TraceClock, PartialReplicaCommitCorruptionAndMissingFilesystemFailSafely) {
  MemoryFS fs;TraceRequestClock clock;clock.begin(&fs,SELF);uint32_t value=0;
  fs.fail_path="/trace_clock_b_1100000000000000.tmp";EXPECT_FALSE(clock.next(1700000000,value));EXPECT_EQ(0u,value);
  fs.fail_path.clear();TraceRequestClock reboot;reboot.begin(&fs,SELF);
  ASSERT_TRUE(reboot.next(1700000000,value));EXPECT_GE(value,1700000256u);
  fs.files["/trace_clock_a_1100000000000000"]->at(0)^=1;fs.files["/trace_clock_b_1100000000000000"]->at(0)^=1;
  TraceRequestClock corrupt;corrupt.begin(&fs,SELF);EXPECT_FALSE(corrupt.healthy());
  TraceRequestClock absent;absent.begin(nullptr,SELF);EXPECT_FALSE(absent.next(1700000000,value));
}
TEST(TraceClock, ExhaustionForeignIdentityAndRenameFailureNeverEmit) {
  MemoryFS fs;TraceRequestClock clock;clock.begin(&fs,SELF);uint32_t value=0;
  EXPECT_FALSE(clock.next(UINT32_MAX,value));
  TraceRequestClock fresh;fresh.begin(&fs,SELF);fs.fail_rename=true;
  EXPECT_FALSE(fresh.next(1700000000,value));fs.fail_rename=false;
  TraceRequestClock valid;valid.begin(&fs,SELF);ASSERT_TRUE(valid.next(1700000000,value));
  TraceRequestClock foreign;foreign.begin(&fs,TARGET);EXPECT_TRUE(foreign.healthy());
  EXPECT_TRUE(foreign.next(1700000000,value));
}
TEST(TraceTimingAdaptation, ExactSensorFormulasAndFrozenLocalBudget) {
  EXPECT_EQ(3100u,TraceTimingPolicy::floodTimeout(100));
  EXPECT_EQ(4900u,TraceTimingPolicy::attemptTimeout(2,100));
  EXPECT_EQ(1150u,TraceTimingPolicy::transmitWatchdog(100));
  TraceTransactionBudget b;ASSERT_TRUE(b.calculate(true,false,10,100,100,100));
  EXPECT_LE(b.duration,120000u);auto frozen=b.duration;
  TraceRoute route;ASSERT_EQ(TraceRoute::VALID,TraceRoute::fromOutbound(nullptr,0,TARGET,route));
  EXPECT_TRUE(b.fits(route,100,0,0,b.duration,0));
  EXPECT_FALSE(b.fits(route,100,2000,b.duration-1000,b.duration,0));EXPECT_EQ(frozen,b.duration);
  EXPECT_FALSE(b.calculate(true,false,10,1000000,1000000,1000000));
}
TEST(TracePrefs, PublicLoginDefaultsOnAndRoundTripsWithoutChangingTailShape) {
  ZenPrefs p{};EXPECT_TRUE(p.tracePublicLogin());p.setTracePublicLogin(false);
  uint8_t data[ZenPrefsCodec::MAX_ENCODED_SIZE];auto n=ZenPrefsCodec::encode(p,1,data,sizeof(data));
  ASSERT_GT(n,0u);ZenPrefs restored{};ASSERT_TRUE(ZenPrefsCodec::decode(restored,data,n));
  EXPECT_FALSE(restored.tracePublicLogin());p.setTracePublicLogin(true);
  n=ZenPrefsCodec::encode(p,2,data,sizeof(data));ASSERT_TRUE(ZenPrefsCodec::decode(restored,data,n));
  EXPECT_TRUE(restored.tracePublicLogin());
}
static void advert(TraceAdvertRouteCache& cache,unsigned count,unsigned width,uint32_t now,const TraceScope& scope={}) {
  mesh::Packet packet;packet.header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT);
  packet.path_len=((width-1)<<6)|count;packet._snr=24;
  for(unsigned i=0;i<count*width;++i)packet.path[i]=i+1;
  const uint8_t data[]={ADV_TYPE_REPEATER};
  ASSERT_TRUE(cache.observe(&packet,mesh::Identity(TARGET),1700000000+now,data,sizeof(data),now,9,scope));
}
TEST(TraceAdvertAdaptation, AllLengthsAndWidthsBuildExactTargetAndReturn) {
  for(unsigned width=1;width<=2;++width)for(unsigned count=0;count<10;++count) {
    TraceAdvertRouteCache cache;cache.clear();advert(cache,count,width,1);
    TraceRoute route;ASSERT_TRUE(cache.best(TARGET,9,{},2,route));
    EXPECT_EQ(count+1,route.repeaters);EXPECT_EQ((2*count+1)*width,route.bytes);
    EXPECT_EQ(0,memcmp(route.path+count*width,TARGET,width));
    for(unsigned i=0;i<count;++i)
      EXPECT_EQ(0,memcmp(route.path+i*width,route.path+(2*count-i)*width,width));
  }
}
TEST(TraceAdvertAdaptation, ScopeProfileAndAgeGateTheSingleHint) {
  TraceAdvertRouteCache cache;cache.clear();advert(cache,2,1,1);
  TraceRoute route;EXPECT_FALSE(cache.best(TARGET,10,{},2,route));
  TraceScope foreign;foreign.kind=TraceScope::RESOLVED;foreign.key[0]=1;
  EXPECT_FALSE(cache.best(TARGET,9,foreign,2,route));
  ASSERT_TRUE(cache.best(TARGET,9,{},2,route));
  EXPECT_FALSE(cache.best(TARGET,9,{},TraceAdvertRouteCache::TTL+1,route));
}
TEST(TraceAdvertAdaptation, FreshCacheAndIdentityResetHaveNoFallbackUntilAdvertArrives) {
  TraceAdvertRouteCache cache;TraceRoute route;
  EXPECT_FALSE(cache.best(TARGET,9,{},0,route));
  advert(cache,1,2,1);ASSERT_TRUE(cache.best(TARGET,9,{},2,route));
  cache.clear();EXPECT_FALSE(cache.best(TARGET,9,{},3,route));
  advert(cache,1,1,4);EXPECT_TRUE(cache.best(TARGET,9,{},5,route));
  TraceAdvertRouteCache rebooted;EXPECT_FALSE(rebooted.best(TARGET,9,{},6,route));
}
int main(int argc,char** argv){::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}

#include "TraceAirtimeReference.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RequestMessageClock.h"
TEST(TracePHY, ActualRadioLibCalculatorCoversAustraliaCr8AndSupportedRadioMatrix) {
  struct PHY {uint8_t sf;float bw;uint8_t cr;uint16_t pre;bool crc,implicit;};
  const PHY profiles[]={{7,62.5,8,32,true,false},{7,62.5,7,32,true,false},
    {8,62.5,5,32,true,false},{9,62.5,7,16,true,false},{11,62.5,7,16,true,false},
    {12,62.5,8,16,true,false},{7,125,5,32,true,false},{5,250,6,16,true,false},
    {6,500,8,16,false,false},{7,62.5,7,32,true,true},{12,7.8,8,16,true,false}};
  for(const auto& p:profiles)for(unsigned n=1;n<=10;++n)for(unsigned width=1;width<=2;++width)for(unsigned scope:{0u,4u}) {
    SCOPED_TRACE(::testing::Message()<<"SF"<<int(p.sf)<<" CR"<<int(p.cr)<<" BW"<<p.bw<<" hops"<<n<<" width"<<width<<" scope"<<scope);
    ReferenceData dr{{p.sf,p.bw,p.cr}};
    ReferenceConfig pc{{(1000u<<p.sf)/p.bw>=16000,p.implicit,p.crc,p.pre}};
    auto air=[&](unsigned bytes){return referenceAirtime(dr,pc,bytes)/1000;};
    auto probe=air(11+(2*n-1)*(width+1));
    EXPECT_EQ(1500ULL+(6ULL*probe+250)*2*n,TraceTimingPolicy::attemptTimeout(n,probe));
    TraceTransactionBudget budget;
    bool valid=budget.calculate(true,true,n,air(22+scope),air(53+scope),probe);
    EXPECT_LE(budget.duration,120000u);
    if(valid){EXPECT_GT(budget.duration,0u);EXPECT_GE(budget.planned_probes,1u);}
    else EXPECT_GT(uint64_t(TraceTimingPolicy::attemptTimeout(1,probe))+5500,budget.duration-1);
  }
}
TEST(TraceClock, PrepareFailsBeforeAnyProducerCanEmitOnFullStorage) {
  MemoryFS fs;Wall wall;RequestRTC clock(wall);clock.begin(&fs,SELF);fs.fail_write=true;
  EXPECT_FALSE(clock.prepare());EXPECT_EQ(0u,clock.getCurrentTimeUnique());EXPECT_EQ(wall.now,clock.getCurrentTime());
}
TEST(TraceClock, OneSurvivingReplicaSkipsReservedRangeAndRepeaterMessageRetriesKeepWireTime) {
  MemoryFS fs;Wall wall;RequestRTC clock(wall);clock.begin(&fs,SELF);
  RequestMessageClock messages;auto original=messages.timestamp(TARGET,wall.now,0,"version",clock);
  EXPECT_GT(clock.getCurrentTimeUnique(),original); // telemetry after plain repeater command
  EXPECT_EQ(original,messages.timestamp(TARGET,wall.now,1,"version",clock));
  EXPECT_EQ(0u,messages.timestamp(TARGET,wall.now,1,"different",clock));
  fs.files["/trace_clock_b_1100000000000000"]->at(0)^=1;
  RequestRTC reboot(wall);reboot.begin(&fs,SELF);EXPECT_GT(reboot.getCurrentTimeUnique(),original);
}
TEST(TraceAdvertAdaptation, RejectsMalformedAndUnsupportedHints) {
  TraceAdvertRouteCache cache;
  mesh::Packet packet;packet.header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT);packet.path_len=0;
  const uint8_t short_data[]={uint8_t(ADV_TYPE_REPEATER|ADV_LATLON_MASK)};
  EXPECT_FALSE(cache.observe(&packet,mesh::Identity(TARGET),1,short_data,1,0,9,{}));
  const uint8_t data[]={ADV_TYPE_REPEATER};packet.path_len=0x80;
  EXPECT_FALSE(cache.observe(&packet,mesh::Identity(TARGET),1,data,1,0,9,{}));
}
TEST(TraceAdvertAdaptation, ExpiryUsesMonotonicTimeAcrossMillisRollover) {
  TraceAdvertRouteCache cache;const uint32_t start=UINT32_MAX-1000;
  advert(cache,1,1,start);TraceRoute route;
  EXPECT_TRUE(cache.best(TARGET,9,{},start+TraceAdvertRouteCache::TTL-1,route));
  EXPECT_FALSE(cache.best(TARGET,9,{},start+TraceAdvertRouteCache::TTL,route));
  advert(cache,2,1,start+TraceAdvertRouteCache::TTL);
  EXPECT_TRUE(cache.best(TARGET,9,{},start+TraceAdvertRouteCache::TTL+1,route));
}

TEST(TraceClock, ExplicitIdentitySwitchAndRestorePreserveSeparateDurableFloors) {
  MemoryFS fs;Wall wall;RequestRTC clock(wall);clock.begin(&fs,SELF);
  uint32_t self=clock.getCurrentTimeUnique();ASSERT_TRUE(clock.prepare());
  clock.begin(&fs,TARGET);auto other=clock.getCurrentTimeUnique();EXPECT_GE(other,wall.now);
  clock.begin(&fs,SELF);EXPECT_GT(clock.getCurrentTimeUnique(),self);
  ASSERT_TRUE(clock.prepare());wall.now+=10000;
  EXPECT_GE(clock.getCurrentTimeUnique(),wall.now);EXPECT_FALSE(clock.trustedWallTime());
  clock.observeWallGeneration(1);clock.observeWallGeneration(2);EXPECT_TRUE(clock.trustedWallTime());
}

TEST(TraceClock, ScopedPrefixCollisionAndCorruptLegacyReservationFailClosed) {
  MemoryFS fs;TraceRequestClock clock;clock.begin(&fs,SELF);uint32_t value=0;
  ASSERT_TRUE(clock.next(1700000000,value));uint8_t collision[32];memcpy(collision,SELF,32);collision[31]=1;
  TraceRequestClock foreign;foreign.begin(&fs,collision);EXPECT_FALSE(foreign.healthy());
  MemoryFS legacy;legacy.files["/trace_clock_a"]=std::make_shared<std::vector<uint8_t>>(44,0);
  TraceRequestClock corrupt;corrupt.begin(&legacy,SELF);EXPECT_FALSE(corrupt.next(1700000000,value));
}
TEST(TracePrefs, LegacyTailCannotAccidentallyDisableNewPublicLoginPolicy) {
  ZenPrefs source{};source.setTracePublicLogin(false);source.custom_msgs[0][0]='X';
  uint8_t bytes[ZenPrefsCodec::MAX_ENCODED_SIZE];size_t n=ZenPrefsCodec::encode(source,1,bytes,sizeof(bytes));
  ASSERT_GT(n,8u);ASSERT_EQ(bytes[n-8],11u); // strip the newly tagged policy from a real record
  uint16_t length=bytes[5]|(uint16_t(bytes[6])<<8);length-=4;bytes[5]=length;bytes[6]=length>>8;
  n-=4;auto crc=trace_storage::checksum(bytes,n-4);memcpy(bytes+n-4,&crc,4);
  ZenPrefs loaded{};ASSERT_TRUE(ZenPrefsCodec::decode(loaded,bytes,n));
  EXPECT_TRUE(loaded.tracePublicLogin());EXPECT_EQ('X',loaded.custom_msgs[0][0]);
}

TEST(TraceClock, MatchingLegacySensorReservationsAreImportedConservatively) {
  MemoryFS fs;TraceRequestClock prior;prior.begin(&fs,SELF);uint32_t before=0;
  ASSERT_TRUE(prior.next(1700000000,before));
  fs.rename("/trace_clock_a_1100000000000000","/trace_clock_a");
  fs.rename("/trace_clock_b_1100000000000000","/trace_clock_b");
  TraceRequestClock upgraded;upgraded.begin(&fs,SELF);uint32_t after=0;
  ASSERT_TRUE(upgraded.next(1700000000,after));EXPECT_GE(after,before+256);
}

TEST(TraceAdvertAdaptation, EightSingleHintsStayWithin672BytesAndReplacePreviousRoute) {
  EXPECT_EQ(8,unsigned(TraceAdvertRouteCache::CAPACITY));
  EXPECT_EQ(sizeof(TraceAdvertRouteCache),672u);
  TraceAdvertRouteCache cache;cache.clear();
  advert(cache,1,1,1);TraceRoute first,latest;
  ASSERT_TRUE(cache.best(TARGET,9,{},2,first));
  advert(cache,2,1,3);
  ASSERT_TRUE(cache.best(TARGET,9,{},4,latest));
  EXPECT_EQ(3,latest.repeaters);EXPECT_FALSE(latest.equals(first));
  EXPECT_FALSE(cache.best(TARGET,9,{},4,first,&latest));
}
TEST(TraceAdvertAdaptation, NinthRepeaterEvictsOldestAndRefreshPreservesActiveEntry) {
  TraceAdvertRouteCache cache;
  mesh::Packet packet;packet.header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT);packet.path_len=0;
  const uint8_t data[]={ADV_TYPE_REPEATER};
  auto observe=[&](uint8_t number,uint32_t now) {
    uint8_t key[32]={number};
    ASSERT_TRUE(cache.observe(&packet,mesh::Identity(key),1700000000+now,data,sizeof(data),now,9,{}));
  };
  for(uint8_t i=1;i<=8;++i)observe(i,i);
  observe(1,9);observe(9,10);
  TraceRoute route;uint8_t evicted[32]={2},refreshed[32]={1};
  EXPECT_FALSE(cache.best(evicted,9,{},11,route));EXPECT_TRUE(cache.best(refreshed,9,{},11,route));
  for(uint8_t i=1;i<=9;++i) {
    uint8_t key[32]={i};EXPECT_EQ(i!=2,cache.best(key,9,{},11,route));
  }
}
TEST(TraceAdvertAdaptation, DuplicateAdvertCannotRefreshExpiryOrReplaceRetainedPath) {
  TraceAdvertRouteCache cache;
  mesh::Packet packet;packet.header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT);packet.path_len=0;
  const uint8_t data[]={ADV_TYPE_REPEATER};
  ASSERT_TRUE(cache.observe(&packet,mesh::Identity(TARGET),1,data,1,10,9,{}));
  packet.path_len=1;packet.path[0]=0x33;
  EXPECT_FALSE(cache.observe(&packet,mesh::Identity(TARGET),1,data,1,TraceAdvertRouteCache::TTL+9,9,{}));
  TraceRoute route;ASSERT_TRUE(cache.best(TARGET,9,{},TraceAdvertRouteCache::TTL+9,route));
  EXPECT_EQ(1,route.repeaters);
  EXPECT_FALSE(cache.best(TARGET,9,{},TraceAdvertRouteCache::TTL+10,route));
  EXPECT_TRUE(cache.observe(&packet,mesh::Identity(TARGET),2,data,1,TraceAdvertRouteCache::TTL+10,9,{}));
  ASSERT_TRUE(cache.best(TARGET,9,{},TraceAdvertRouteCache::TTL+10,route));EXPECT_EQ(2,route.repeaters);
}

TEST(TraceAdvertAdaptation, FullIdentityAndResolvedScopeRemainIsolated) {
  TraceAdvertRouteCache cache;TraceScope scope;scope.kind=TraceScope::RESOLVED;scope.key[0]=7;
  advert(cache,1,1,1,scope);
  uint8_t collision[32];memcpy(collision,TARGET,32);collision[31]=1;
  TraceRoute route;EXPECT_FALSE(cache.best(collision,9,scope,2,route));
  EXPECT_FALSE(cache.best(TARGET,9,{},2,route));
  ASSERT_TRUE(cache.best(TARGET,9,scope,2,route));
  TraceScope unresolved;unresolved.kind=TraceScope::UNRESOLVED;
  EXPECT_FALSE(cache.best(TARGET,9,unresolved,2,route));
  mesh::Packet packet;packet.header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT);packet.path_len=0;
  const uint8_t data[]={ADV_TYPE_REPEATER};
  EXPECT_FALSE(cache.observe(&packet,mesh::Identity(TARGET),2,data,1,3,9,unresolved));
  EXPECT_TRUE(cache.best(TARGET,9,scope,4,route));
}
