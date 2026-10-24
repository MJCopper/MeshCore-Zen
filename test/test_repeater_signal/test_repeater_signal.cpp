#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterSignalMonitor.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/SignalReception.h"

using Monitor = zen::RepeaterSignalMonitor;
static const uint8_t A[32] = {1}, B[32] = {2};
static bool sample(Monitor& m, const uint8_t* key, int snr, uint32_t time,
                   const uint8_t* token = nullptr, int rssi = -60) {
  return m.noteSample(key, key ? Monitor::ADVERT : Monitor::ROUTED, snr, rssi, time, token);
}

TEST(RepeaterSignal, AcceptsRelayedTrafficButNotDirectCompanions) {
  EXPECT_FALSE(Monitor::qualifiesRoute(true,false,0));
  EXPECT_TRUE(Monitor::qualifiesRoute(true,false,1));
  EXPECT_FALSE(Monitor::qualifiesRoute(false,true,1));
  EXPECT_TRUE(Monitor::qualifiesRoute(false,true,2));
  EXPECT_FALSE(Monitor::qualifiesRoute(false,false,4));
}
TEST(RepeaterSignal, ExactInitialBoundariesAcrossAllSpreadingFactors) {
  const int snrs[] = {-80,0,20,21,28,29,39,40,80};
  const int bars[] = {1,1,1,2,2,3,3,4,4};
  for (int sf=5;sf<=12;++sf) for (unsigned i=0;i<9;++i) {
    Monitor m; m.configure(916.575f,62.5f,sf,8);
    sample(m,A,snrs[i]-10*(sf-7),100);
    EXPECT_EQ(m.snapshot(100).bars,bars[i]) << sf << ":" << i;
  }
}
TEST(RepeaterSignal, StrongIdentifiedRepeaterIsNotDilutedByOtherTraffic) {
  Monitor m;
  for (unsigned t=1;t<=3;++t) sample(m,B,0,t);
  sample(m,A,48,4); // first sample on A is 12 dB
  for (unsigned t=5;t<=12;++t) sample(m,nullptr,-20,t);
  auto s=m.snapshot(12);
  EXPECT_EQ(s.bars,4); EXPECT_EQ(s.key[0],1); EXPECT_EQ(s.count,1);
  EXPECT_EQ(s.snr_x4,48); EXPECT_TRUE(s.identified);
}
TEST(RepeaterSignal, FallbackDoesNotOverrideFreshIdentifiedEvidence) {
  Monitor m; sample(m,A,0,0); sample(m,nullptr,80,100);
  EXPECT_EQ(m.snapshot(100).bars,1);
  auto s=m.snapshot(Monitor::WINDOW_MS);
  EXPECT_EQ(s.bars,4); EXPECT_FALSE(s.identified);
}
TEST(RepeaterSignal, SamplesExpireIndividuallyEvenWithNewTraffic) {
  Monitor m;
  sample(m,A,0,0); sample(m,A,0,1); sample(m,A,0,2);
  sample(m,A,48,Monitor::WINDOW_MS-1);
  auto s=m.snapshot(Monitor::WINDOW_MS+2);
  EXPECT_EQ(s.count,1); EXPECT_EQ(s.snr_x4,48); EXPECT_EQ(s.bars,4);
  EXPECT_EQ(m.snapshot(2*Monitor::WINDOW_MS-1).status,Monitor::STALE);
}
TEST(RepeaterSignal, ExpiredSelectionDoesNotRetainHysteresis) {
  Monitor m; sample(m,A,48,0); sample(m,B,0,100);
  EXPECT_EQ(m.snapshot(100).bars,4);
  auto s=m.snapshot(Monitor::WINDOW_MS);
  EXPECT_EQ(s.key[0],2); EXPECT_EQ(s.bars,1);
}
TEST(RepeaterSignal, HistoryDistinguishesNoneStaleAndUnavailableAcrossRollover) {
  Monitor m; EXPECT_EQ(m.snapshot(0).status,Monitor::NONE);
  const uint32_t start=UINT32_MAX-1000;
  sample(m,A,40,start);
  EXPECT_EQ(m.snapshot(500).status,Monitor::FRESH);
  EXPECT_EQ(m.snapshot(start+Monitor::WINDOW_MS).status,Monitor::STALE);
  EXPECT_EQ(m.snapshot(500,false).status,Monitor::UNAVAILABLE);
  EXPECT_EQ(m.snapshot(500,false).bars,0);
  sample(m,A,0,start+Monitor::WINDOW_MS+1);
  EXPECT_EQ(m.snapshot(start+Monitor::WINDOW_MS+1).bars,1);
}
TEST(RepeaterSignal, RingReplacesAllFourSamplesAndPreservesFractionalAverages) {
  Monitor m;
  for (int i=0;i<4;++i) sample(m,A,20,i);
  for (int i=0;i<4;++i) sample(m,A,40,10+i);
  EXPECT_EQ(m.snapshot(14).count,4); EXPECT_EQ(m.snapshot(14).snr_x4,40);
  sample(m,A,41,15);
  EXPECT_EQ(m.snapshot(15).count,4);
  Monitor::Event e; ASSERT_TRUE(m.history(0,e));
  EXPECT_EQ(e.snr_x4,41); EXPECT_EQ(e.selected.snr_x4,40);
}
TEST(RepeaterSignal, FractionalQuarterDbAveragesKeepExactBoundariesAfterExpiry) {
  for (int boundary : {20,28,40}) {
    Monitor m;
    sample(m,A,80,0);
    sample(m,A,boundary,100);
    sample(m,A,boundary,101);
    sample(m,A,boundary+(boundary==40?-1:1),102);
    auto result=m.snapshot(Monitor::WINDOW_MS);
    EXPECT_EQ(result.count,3);
    EXPECT_EQ(result.bars,boundary==20?2:3);
  }
}
TEST(RepeaterSignal, DuplicatePayloadsCannotRefreshAgeOrConfirmTransitions) {
  Monitor m; uint8_t token[8]={7};
  ASSERT_TRUE(sample(m,A,20,0,token));
  EXPECT_FALSE(sample(m,A,80,100,token));
  EXPECT_EQ(m.snapshot(100).count,1); EXPECT_EQ(m.snapshot(100).age_ms,100);
  EXPECT_EQ(m.historyCount(),1);
  EXPECT_TRUE(sample(m,B,80,100,token)); // same payload, different identified sender
  EXPECT_TRUE(sample(m,A,80,Monitor::DUPLICATE_MS,token));
}
TEST(RepeaterSignal, HysteresisNeedsTwoReceptionsBeyondOneDbMargin) {
  Monitor m;
  for (int i=0;i<4;++i) sample(m,A,40,i);
  sample(m,A,32,10); EXPECT_EQ(m.snapshot(10).bars,4); // avg 9.5
  sample(m,A,32,11); EXPECT_EQ(m.snapshot(11).bars,4); // avg 9, not below
  sample(m,A,32,12); EXPECT_EQ(m.snapshot(12).bars,4); // first confirmation
  sample(m,A,32,13); EXPECT_EQ(m.snapshot(13).bars,3);
  Monitor up;
  for (int i=0;i<4;++i) sample(up,A,20,i);
  for (int i=0;i<3;++i) sample(up,A,60,10+i);
  EXPECT_EQ(up.snapshot(12).bars,1);
  sample(up,A,60,13); EXPECT_EQ(up.snapshot(13).bars,4);
  EXPECT_EQ(up.snapshot(13).bars,4); // reading/rendering cannot confirm a transition
}
TEST(RepeaterSignal, HysteresisConfirmationsStayWithEachRepeater) {
  Monitor m;
  for (int i=0;i<4;++i) { sample(m,A,40,i); sample(m,B,40,i); }
  for (int i=0;i<3;++i) { sample(m,A,32,10+i); sample(m,B,32,10+i); }
  EXPECT_EQ(m.snapshot(12).bars,4); // each only has one eligible confirmation
  sample(m,A,32,13); // B remains higher and selected
  EXPECT_EQ(m.snapshot(13).key[0],2); EXPECT_EQ(m.snapshot(13).bars,4);
  sample(m,B,32,14); EXPECT_EQ(m.snapshot(14).bars,3);
}
TEST(RepeaterSignal, DiscoveryAndPassiveEvidenceShareTheSameLink) {
  Monitor m; sample(m,A,40,0);
  m.beginDiscovery(0);
  EXPECT_TRUE(m.noteSample(A,Monitor::DISCOVERY,0,-90,10));
  EXPECT_EQ(m.snapshot(10).count,2); EXPECT_EQ(m.snapshot(10).snr_x4,20);
  m.finishDiscovery(11); EXPECT_EQ(m.failedScans(),0);
  m.beginDiscovery(0); m.finishDiscovery(12);
  EXPECT_EQ(m.failedScans(),1); EXPECT_EQ(m.snapshot(12).status,Monitor::NOT_FOUND);
  EXPECT_EQ(m.snapshot(12).count,0);
  EXPECT_FALSE(m.noteSample(A,Monitor::DISCOVERY,80,-50,13)); // late response
  m.beginDiscovery(0); m.cancelDiscovery(); m.finishDiscovery(14);
  EXPECT_EQ(m.failedScans(),1);
}
TEST(RepeaterSignal, FifteenMinuteBoundaryAndFailureStatePriority) {
  Monitor m;
  EXPECT_EQ(Monitor::WINDOW_MS,900000u); EXPECT_EQ(Monitor::SCAN_INTERVAL_MS,900000u);
  EXPECT_EQ(m.snapshot(0).status,Monitor::NONE);
  sample(m,A,40,0);
  EXPECT_EQ(m.snapshot(899999).status,Monitor::FRESH);
  EXPECT_EQ(m.snapshot(900000).status,Monitor::STALE);
  m.beginDiscovery(900000); // scanning preserves the triangle until completion
  EXPECT_EQ(m.snapshot(900001).status,Monitor::STALE);
  m.finishDiscovery(908000);
  EXPECT_EQ(m.snapshot(908000).status,Monitor::NOT_FOUND);
  EXPECT_EQ(m.snapshot(4000000).status,Monitor::NOT_FOUND); // X persists
  EXPECT_EQ(m.snapshot(908000,false).status,Monitor::UNAVAILABLE);
  sample(m,B,0,4000001);
  EXPECT_EQ(m.snapshot(4000001).status,Monitor::FRESH);
  EXPECT_EQ(m.snapshot(4000001).bars,1);
}
TEST(RepeaterSignal, FailedDiscoveryClearsOldReadingButNotConcurrentPassiveReception) {
  Monitor m; sample(m,A,48,0);
  m.beginDiscovery(100); m.finishDiscovery(8100);
  EXPECT_EQ(m.snapshot(8100).status,Monitor::NOT_FOUND);
  EXPECT_EQ(m.snapshot(8100).count,0);
  // A delayed callback for a pre-failure advert must not resurrect that reading.
  uint32_t old=50;
  EXPECT_FALSE(m.noteSample(A,Monitor::ADVERT,48,-60,8200,nullptr,&old));
  EXPECT_EQ(m.snapshot(8200).status,Monitor::NOT_FOUND);
  m.beginDiscovery(9000);
  sample(m,B,40,9500);
  m.finishDiscovery(17000);
  EXPECT_EQ(m.failedScans(),2);
  EXPECT_EQ(m.snapshot(17000).status,Monitor::FRESH);
  EXPECT_EQ(m.snapshot(17000).key[0],2);
  // A new successful discovery also clears X.
  m.beginDiscovery(18000); m.finishDiscovery(26000);
  EXPECT_EQ(m.snapshot(26000).status,Monitor::NOT_FOUND);
  m.beginDiscovery(27000);
  EXPECT_TRUE(m.noteSample(A,Monitor::DISCOVERY,48,-40,27500));
  m.finishDiscovery(35000);
  EXPECT_EQ(m.snapshot(35000).bars,4);
}
TEST(RepeaterSignal, ContinuousScreenOnQueuesAtMostOneScanPerFifteenMinutes) {
  Monitor m; unsigned queued=0;
  auto poll=[&](uint32_t now,bool screen_on) {
    if (m.shouldDiscover(now,screen_on,true,true)) {
      ++queued; m.beginDiscovery(now); m.noteScanAttempt(now);
    }
  };
  poll(0,true); ASSERT_EQ(queued,1u);
  EXPECT_TRUE(m.discovering());
  for (uint32_t now=1;now<8000;now+=11) poll(now,true);
  m.finishDiscovery(8000);
  EXPECT_EQ(m.snapshot(8000).status,Monitor::NOT_FOUND);
  for (uint32_t now=8000;now<900000;now+=97) poll(now,true);
  EXPECT_EQ(queued,1u);
  poll(900000,false); EXPECT_EQ(queued,1u); // never wake a sleeping display
  poll(900000,true); EXPECT_EQ(queued,2u); // no new wake transition required
  poll(900001,true); EXPECT_EQ(queued,2u);
  m.noteSample(A,Monitor::DISCOVERY,40,-60,900100);
  m.finishDiscovery(908000);
  poll(1800000,true); EXPECT_EQ(queued,2u); // reception extended freshness
  poll(1800100,true); EXPECT_EQ(queued,3u);
}
TEST(RepeaterSignal, AutomaticGatesAndAllocationBackoffDoNotCreateFailureState) {
  Monitor m;
  EXPECT_FALSE(m.shouldDiscover(0,false,true,true));
  EXPECT_FALSE(m.shouldDiscover(0,true,false,true));
  EXPECT_FALSE(m.shouldDiscover(0,true,true,false)); // Low Power/Emergency
  EXPECT_TRUE(m.shouldDiscover(0,true,true,true));
  m.noteQueueFailure(0);
  EXPECT_EQ(m.snapshot(1).status,Monitor::NONE);
  EXPECT_EQ(m.failedScans(),0);
  EXPECT_EQ(m.scanDelayMs(4999),1u);
  EXPECT_FALSE(m.shouldDiscover(4999,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(5000,true,true,true));
  m.noteScanAttempt(5000); m.beginDiscovery(5000); // manual or automatic uses same budget
  m.cancelDiscovery(); m.finishDiscovery(13000);
  EXPECT_EQ(m.failedScans(),0);
  EXPECT_EQ(m.snapshot(13000).status,Monitor::NONE);
  EXPECT_FALSE(m.shouldDiscover(904999,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(905000,true,true,true));
}
TEST(RepeaterSignal, ConfigChangesClearNoneFoundAndPreserveAllocationBackoff) {
  Monitor m; m.beginDiscovery(0); m.finishDiscovery(8000);
  EXPECT_EQ(m.snapshot(8000).status,Monitor::NOT_FOUND);
  m.noteQueueFailure(9000); m.configure(918,125,8,5);
  EXPECT_EQ(m.snapshot(9001).status,Monitor::NONE);
  EXPECT_FALSE(m.shouldDiscover(13999,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(14000,true,true,true));
}
TEST(RepeaterSignal, BackoffAndFailureCooldownSurviveTimerRollover) {
  Monitor m; uint32_t start=UINT32_MAX-100;
  m.noteQueueFailure(start);
  EXPECT_FALSE(m.shouldDiscover(start+4999,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(start+5000,true,true,true));
  m.noteScanAttempt(start+5000); m.beginDiscovery(start+5000);
  m.finishDiscovery(start+13000);
  EXPECT_EQ(m.snapshot(start+13000).status,Monitor::NOT_FOUND);
  EXPECT_FALSE(m.shouldDiscover(start+904999,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(start+905000,true,true,true));
}
TEST(RepeaterSignal, InvalidIdentitySourcePairsAreRejected) {
  Monitor m;
  EXPECT_FALSE(m.noteSample(nullptr,Monitor::ADVERT,40,-50,0));
  EXPECT_FALSE(m.noteSample(A,Monitor::ROUTED,40,-50,0));
  EXPECT_FALSE(m.noteSample(nullptr,Monitor::DISCOVERY,40,-50,0));
  EXPECT_EQ(m.historyCount(),0);
}
TEST(RepeaterSignal, AutomaticProbeBudgetSurvivesConfigChangesAndRollover) {
  Monitor m; const uint32_t start=UINT32_MAX-1000;
  EXPECT_TRUE(m.shouldDiscover(start,true,true,true));
  m.noteScanAttempt(start);
  for (int i=0;i<100;++i) EXPECT_FALSE(m.shouldDiscover(start+i,true,true,true));
  m.configure(917,125,8,5);
  EXPECT_FALSE(m.shouldDiscover(start+Monitor::SCAN_INTERVAL_MS-1,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(start+Monitor::SCAN_INTERVAL_MS,true,true,true));
  EXPECT_FALSE(m.shouldDiscover(start+Monitor::SCAN_INTERVAL_MS,true,false,true));
  sample(m,A,0,start+Monitor::SCAN_INTERVAL_MS);
  EXPECT_FALSE(m.shouldDiscover(start+Monitor::SCAN_INTERVAL_MS+1,true,true,true));
  EXPECT_TRUE(m.shouldDiscover(start+Monitor::SCAN_INTERVAL_MS+Monitor::WINDOW_MS,true,true,true));
}
TEST(RepeaterSignal, SettingsChangesClearHistoryAndCancelDiscovery) {
  Monitor m;
  const float configs[][4]={{917,62.5,7,8},{917,125,7,8},{917,125,8,8},{917,125,8,5},{918,125,8,5}};
  for (const auto& c:configs) {
    EXPECT_TRUE(m.configure(c[0],c[1],c[2],c[3]));
    EXPECT_EQ(m.snapshot(100).status,Monitor::NONE); EXPECT_EQ(m.historyCount(),0);
    EXPECT_FALSE(m.noteSample(A,Monitor::DISCOVERY,40,-60,100));
    sample(m,A,40,100); m.beginDiscovery(0);
    EXPECT_FALSE(m.configure(c[0],c[1],c[2],c[3]));
    EXPECT_TRUE(m.fresh(100));
  }
}
TEST(RepeaterSignal, LruTableAndFullIdentityComparisonAreBounded) {
  Monitor m; uint8_t key[32]={1};
  for (unsigned i=0;i<Monitor::LINK_COUNT;++i) {
    key[31]=i; sample(m,key,i==0?80:0,i);
  }
  EXPECT_EQ(m.snapshot(10).key[31],0); // identities share the same prefix
  key[31]=99; sample(m,key,0,20); // oldest (strong) link must be evicted
  EXPECT_EQ(m.snapshot(20).bars,1);
  EXPECT_TRUE(m.snapshot(20).identified);
  EXPECT_LE(sizeof(Monitor)+sizeof(zen::SignalAdvertCapture),4096u);
}
TEST(RepeaterSignal, DiagnosticHistoryWrapsAndRecordsSelectedSourceAndAge) {
  Monitor m; sample(m,A,48,0);
  for (unsigned t=1;t<40;++t) sample(m,nullptr,0,t,nullptr,-100);
  EXPECT_EQ(m.historyCount(),Monitor::HISTORY_COUNT);
  Monitor::Event e; ASSERT_TRUE(m.history(0,e));
  EXPECT_EQ(e.source,Monitor::ROUTED); EXPECT_EQ(e.selected.key[0],1);
  EXPECT_EQ(e.selected.age_ms,39); EXPECT_EQ(e.rssi,-100);
  ASSERT_TRUE(m.history(15,e)); EXPECT_EQ(e.at,24);
  EXPECT_FALSE(m.history(16,e));
  char text[166]; m.formatSummary(text,sizeof(text),40,true);
  EXPECT_NE(strstr(text,"bars=4"),nullptr);
  m.formatHistory(text,sizeof(text),0,40); EXPECT_NE(strstr(text,"selected=01000000/id"),nullptr);
  char tiny[4]; m.formatHistory(tiny,sizeof(tiny),0,40); EXPECT_EQ(tiny[3],0);
}
TEST(RepeaterSignal, InvalidSettingsAndExtremeMetricsAreHandled) {
  Monitor m; m.configure(917,62.5,4,8);
  EXPECT_FALSE(sample(m,A,40,0)); EXPECT_EQ(m.snapshot(0).status,Monitor::NONE);
  m.configure(917,62.5,13,8); EXPECT_FALSE(sample(m,A,40,0));
  m.configure(917,62.5,7,8); sample(m,A,1000,0,nullptr,100);
  EXPECT_EQ(m.snapshot(0).snr_x4,127); EXPECT_EQ(m.snapshot(0).rssi,0);
  m.reset(); sample(m,A,-1000,0,nullptr,-1000);
  EXPECT_EQ(m.snapshot(0).snr_x4,-128); EXPECT_EQ(m.snapshot(0).rssi,-255);
  EXPECT_EQ(m.snapshot(0).bars,1);
}
TEST(SignalReception, MalformedAndUnsupportedPayloadsDoNotQualify) {
  mesh::Packet p; p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;
  p.payload_len=100; EXPECT_FALSE(zen::signalPayloadValid(p));
  p.payload_len=101; EXPECT_TRUE(zen::signalPayloadValid(p));
  p.path_len=255; EXPECT_FALSE(zen::signalPayloadValid(p));
  p.path_len=0; p.header|=(PAYLOAD_VER_2<<PH_VER_SHIFT);
  EXPECT_FALSE(zen::signalPayloadValid(p));
  p.header=(PAYLOAD_TYPE_ACK<<PH_TYPE_SHIFT)|ROUTE_TYPE_DIRECT;
  p.payload_len=3; EXPECT_FALSE(zen::signalPayloadValid(p));
  p.payload_len=4; EXPECT_TRUE(zen::signalPayloadValid(p));
  p.header=(PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT)|ROUTE_TYPE_DIRECT;
  EXPECT_FALSE(zen::signalPayloadValid(p));
}
TEST(SignalReception, DiscoveryRequiresActiveMatchingZeroHopRepeaterReply) {
  mesh::Packet p;
  p.header=(PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT)|ROUTE_TYPE_DIRECT;
  p.payload_len=6+PUB_KEY_SIZE;
  memset(p.payload,0,p.payload_len);
  p.payload[0]=0x90|ADV_TYPE_REPEATER;
  uint32_t tag=12345; memcpy(p.payload+2,&tag,4); memcpy(p.payload+6,A,32);
  ASSERT_EQ(zen::signalDiscoveryKey(p,tag,true),p.payload+6);
  EXPECT_EQ(zen::signalDiscoveryKey(p,tag,false),nullptr); // cancelled/expired
  EXPECT_EQ(zen::signalDiscoveryKey(p,tag+1,true),nullptr);
  EXPECT_EQ(zen::signalDiscoveryKey(p,0,true),nullptr);
  p.payload_len--; EXPECT_EQ(zen::signalDiscoveryKey(p,tag,true),nullptr);
  p.payload_len++;
  p.payload[0]=0x90|1; EXPECT_EQ(zen::signalDiscoveryKey(p,tag,true),nullptr);
  p.payload[0]=0x90|ADV_TYPE_REPEATER;
  p.path_len=1; EXPECT_EQ(zen::signalDiscoveryKey(p,tag,true),nullptr);
  p.path_len=192; EXPECT_EQ(zen::signalDiscoveryKey(p,tag,true),nullptr);
  p.path_len=0; p.header=(PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;
  EXPECT_EQ(zen::signalDiscoveryKey(p,tag,true),nullptr);
}
TEST(SignalReception, DelayedAdvertRetainsSameReceptionMetricsAndTimestamp) {
  zen::SignalAdvertCapture capture; mesh::Packet p;
  p.header=(PAYLOAD_TYPE_ADVERT<<PH_TYPE_SHIFT)|ROUTE_TYPE_FLOOD;
  p.payload_len=1; p.payload[0]=1; p._snr=48;
  capture.remember(&p,-45,100);
  p._snr=-20; // a subsequent radio reading must not be substituted
  zen::SignalAdvertCapture::Reading r;
  ASSERT_TRUE(capture.take(&p,32100,r)); EXPECT_EQ(r.rssi,-45); EXPECT_EQ(r.snr,48); EXPECT_EQ(r.at,100);
  EXPECT_FALSE(capture.take(&p,32100,r));
  Monitor m; m.noteSample(A,Monitor::ADVERT,r.snr,r.rssi,32100,r.token,&r.at);
  EXPECT_EQ(m.snapshot(32100).age_ms,32000);
}
TEST(SignalReception, CaptureRejectsReusedPointersExpiredAndResetEntries) {
  zen::SignalAdvertCapture capture; mesh::Packet p;
  p.payload_len=1; p.payload[0]=1; p._snr=40;
  capture.remember(&p,-50,0); p.payload[0]=2;
  zen::SignalAdvertCapture::Reading r; EXPECT_FALSE(capture.take(&p,1,r));
  capture.remember(&p,-60,2); EXPECT_FALSE(capture.take(&p,33003,r));
  capture.remember(&p,-70,5); capture.reset(); EXPECT_FALSE(capture.take(&p,6,r));
  capture.remember(&p,-80,7); capture.remember(&p,-90,8);
  ASSERT_TRUE(capture.take(&p,9,r)); EXPECT_EQ(r.rssi,-90);
}
TEST(SignalReception, CaptureRingIsBoundedAcrossPoolReuseAndRollover) {
  zen::SignalAdvertCapture capture; mesh::Packet p[zen::SignalAdvertCapture::CAPACITY+1];
  for (unsigned i=0;i<=zen::SignalAdvertCapture::CAPACITY;++i) {
    p[i].payload_len=1;p[i].payload[0]=i;p[i]._snr=40;
    capture.remember(&p[i],-50,UINT32_MAX-10);
  }
  zen::SignalAdvertCapture::Reading r;
  EXPECT_FALSE(capture.take(&p[0],10,r)); EXPECT_TRUE(capture.take(&p[1],10,r));
}

int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
