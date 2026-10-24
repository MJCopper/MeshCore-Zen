#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/DisplayActivity.h"
#include <deque>
#include <vector>
#include <cstring>
#include <helpers/ui/ZenDisplayDriver.h>
#include "../../examples/companion_radio/zen-overlay/app/ui-new/RepeaterTraceView.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/TraceNameResolver.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterTraceSession.h"
using namespace zen;
static const uint8_t TARGET[32]={0xCC,0xDD};
class FakeTraceTransport:public RepeaterTraceTransport {
public:
  uint32_t now=0,sequence=10,airtime=100,flood_timeout=5000;
  bool allocation_fails=false;
  unsigned allocated=0,cancelled=0,emitted_floods=0,emitted_traces=0,allocation_calls=0;
  mesh::Packet pool[64];std::deque<mesh::Packet*> queue;
  uint32_t traceNow() const override { return now; }
  uint32_t traceRandom() override { return ++sequence; }
  uint32_t traceDiscoveryTag() override { return 1700000000u+(++discovery_clock); }
  uint32_t discovery_clock=0;
  uint32_t traceAirtime(unsigned) const override { return airtime; }
  uint32_t traceFloodTimeout(uint32_t) const override { return flood_timeout; }
  mesh::Packet* packet() {
    ++allocation_calls;if(allocation_fails||allocated>=64)return nullptr;
    auto p=&pool[allocated++];p->path_len=0;p->_snr=0;return p;
  }
  mesh::Packet* traceQueueDiscovery(const uint8_t*,uint32_t tag) override {
    auto p=packet();if(!p)return nullptr;
    p->header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_REQ<<PH_TYPE_SHIFT);p->payload_len=20;
    memset(p->payload,0,20);memcpy(p->payload,&tag,4);queue.push_back(p);return p;
  }
  mesh::Packet* traceQueueLoop(const TraceRoute& route,uint32_t tag,uint32_t auth) override {
    auto p=packet();if(!p)return nullptr;
    p->header=ROUTE_TYPE_DIRECT|(PAYLOAD_TYPE_TRACE<<PH_TYPE_SHIFT);
    p->payload_len=9+route.bytes;memcpy(p->payload,&tag,4);memcpy(p->payload+4,&auth,4);
    p->payload[8]=route.flags;memcpy(p->payload+9,route.path,route.bytes);queue.push_back(p);return p;
  }
  bool traceCancelQueued(mesh::Packet* p) override {
    for(auto i=queue.begin();i!=queue.end();++i)if(*i==p){queue.erase(i);++cancelled;return true;}
    return false;
  }
  mesh::Packet* transmit(RepeaterTraceSession& s,bool success=true) {
    if(queue.empty())return nullptr;
    auto p=queue.front();queue.pop_front();
    if(p->getPayloadType()==PAYLOAD_TYPE_TRACE)++emitted_traces;else ++emitted_floods;
    if(success){s.txStarted(p,now);s.txComplete(p);}else s.txFailed(p,now);
    return p;
  }
  static uint32_t tag(const mesh::Packet* p) { uint32_t n;memcpy(&n,p->payload,4);return n; }
};
class RepeaterTraceTest:public ::testing::Test {
protected:
  FakeTraceTransport tx;RepeaterTraceSession session{tx};
  mesh::Packet* discovery=nullptr;mesh::Packet* first=nullptr;mesh::Packet* second=nullptr;
  uint8_t via[4]={0x11,0x12,0x22,0x23};
  void begin(unsigned count=2,unsigned width=1) {
    ASSERT_TRUE(session.start(TARGET,true,width));session.tick();
    ASSERT_FALSE(tx.queue.empty());discovery=tx.transmit(session);ASSERT_NE(discovery,nullptr);
    tx.now+=20;
    ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),via,((width-1)<<6)|count));
    tx.now+=3000;session.tick();ASSERT_FALSE(tx.queue.empty());
    first=tx.transmit(session);ASSERT_NE(first,nullptr);
  }
  bool reply(mesh::Packet* sent,int final_snr=0,const uint8_t* values=nullptr) {
    uint32_t auth;memcpy(&auth,sent->payload+4,4);
    unsigned width=1<<(sent->payload[8]&3),bytes=sent->payload_len-9,count=bytes/width;
    mesh::Packet result;result.header=ROUTE_TYPE_DIRECT|(PAYLOAD_TYPE_TRACE<<PH_TYPE_SHIFT);
    result.path_len=count;result._snr=final_snr;
    uint8_t snrs[19]{};if(values)memcpy(snrs,values,count);
    return session.receive(&result,tx.tag(sent),auth,sent->payload[8],snrs,sent->payload+9,bytes);
  }
  void retry(unsigned repeaters=3) {
    tx.now+=TraceTimingPolicy::attemptTimeout(repeaters,tx.airtime);session.tick();
    ASSERT_EQ(session.phase(),RepeaterTraceSession::RETRY_PAUSE);
    tx.now+=2100;session.tick();ASSERT_FALSE(tx.queue.empty());second=tx.queue.back();
  }
};
TEST_F(RepeaterTraceTest, DiscoveryToFullLoopProducesBothDirectionsAndTrueRtt) {
  begin(2,1);
  const uint8_t expected[]={0x11,0x12,0xCC,0x12,0x11};
  ASSERT_EQ(first->payload_len,14);EXPECT_EQ(memcmp(first->payload+9,expected,5),0);
  const uint8_t snrs[]={21,(uint8_t)-8,28,(uint8_t)-13,18};
  tx.now+=1234;ASSERT_TRUE(reply(first,0,snrs));
  ASSERT_TRUE(session.hasResult());EXPECT_FALSE(session.active());
  const auto& r=session.result();EXPECT_EQ(r.elapsed_millis,1234u);EXPECT_EQ(r.route.repeaters,3);
  EXPECT_EQ(RepeaterTraceSession::outward(r,0),21);
  EXPECT_EQ(RepeaterTraceSession::outward(r,1),-8);
  EXPECT_EQ(RepeaterTraceSession::outward(r,2),28);
  EXPECT_EQ(RepeaterTraceSession::returning(r,0),0); // valid zero, final reception
  EXPECT_EQ(RepeaterTraceSession::returning(r,1),18);
  EXPECT_EQ(RepeaterTraceSession::returning(r,2),-13);
  EXPECT_EQ(session.floodSubmissions(),1u);EXPECT_EQ(session.traceSubmissions(),1u);
}
TEST_F(RepeaterTraceTest, DirectNeighbourHasTwoRealSnrValues) {
  begin(0,1);ASSERT_EQ(first->payload_len,10);EXPECT_EQ(first->payload[9],TARGET[0]);
  uint8_t snrs[]={0};tx.now+=40;ASSERT_TRUE(reply(first,-5,snrs));
  EXPECT_EQ(session.result().route.repeaters,1);EXPECT_EQ(session.result().elapsed_millis,40u);
  EXPECT_EQ(RepeaterTraceSession::outward(session.result(),0),0);
  EXPECT_EQ(RepeaterTraceSession::returning(session.result(),0),-5);
}
TEST_F(RepeaterTraceTest, TwoByteRoutePreservesHashBoundariesAndCount) {
  begin(2,2);const uint8_t expected[]={0x11,0x12,0x22,0x23,0xCC,0xDD,0x22,0x23,0x11,0x12};
  EXPECT_EQ(first->payload[8],1);EXPECT_EQ(first->payload_len,19);
  EXPECT_EQ(memcmp(first->payload+9,expected,10),0);tx.now+=500;ASSERT_TRUE(reply(first));
  EXPECT_EQ(session.result().route.repeaters,3);
}
TEST_F(RepeaterTraceTest, QueueWaitAndSettlingAreExcludedFromTraceRtt) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();tx.now=8000;
  discovery=tx.transmit(session);ASSERT_NE(discovery,nullptr);tx.now=8100;
  ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),nullptr,0));
  tx.now=11100;session.tick();ASSERT_FALSE(tx.queue.empty());tx.now=19100;
  first=tx.transmit(session);tx.now=19421;ASSERT_TRUE(reply(first));
  EXPECT_EQ(session.result().elapsed_millis,321u);
}
TEST_F(RepeaterTraceTest, DiscoveryTimeoutRetriesOnlyOnce) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();discovery=tx.transmit(session);
  tx.now=4999;session.tick();EXPECT_TRUE(tx.queue.empty());
  tx.now=5000;session.tick();session.tick();auto next=tx.transmit(session);ASSERT_NE(next,nullptr);
  EXPECT_NE(tx.tag(next),tx.tag(discovery));
  tx.now=10000;session.tick();EXPECT_FALSE(session.active());
  EXPECT_EQ(session.error(),RepeaterTraceSession::DISCOVERY_TIMEOUT);
  EXPECT_EQ(tx.emitted_floods,2u);EXPECT_EQ(tx.emitted_traces,0u);
  for(int i=0;i<100;++i)session.tick();EXPECT_TRUE(tx.queue.empty());
}
TEST_F(RepeaterTraceTest, LateFirstDiscoveryCancelsQueuedRetryAndUsesItsRoute) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();discovery=tx.transmit(session);
  tx.now=5000;session.tick();session.tick();ASSERT_EQ(tx.queue.size(),1u);
  ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),nullptr,0));
  EXPECT_TRUE(tx.queue.empty());EXPECT_EQ(tx.cancelled,1u);
  EXPECT_EQ(session.phase(),RepeaterTraceSession::SETTLING);
}
TEST_F(RepeaterTraceTest, UnrelatedDiscoveryCannotStartTrace) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();discovery=tx.transmit(session);
  uint8_t wrong[32]={0};EXPECT_FALSE(session.pathResponse(wrong,tx.tag(discovery),nullptr,0));
  EXPECT_FALSE(session.pathResponse(TARGET,tx.tag(discovery)+1,nullptr,0));
  EXPECT_EQ(session.phase(),RepeaterTraceSession::DISCOVERY_WAIT);
}
TEST_F(RepeaterTraceTest, TraceRetrySucceedsWithFreshIdentifiers) {
  begin();retry();EXPECT_NE(tx.tag(first),tx.tag(second));second=tx.transmit(session);
  tx.now+=123;ASSERT_TRUE(reply(second));EXPECT_EQ(session.result().attempt,1);
  EXPECT_EQ(session.result().elapsed_millis,123u);EXPECT_EQ(session.traceSubmissions(),2u);
}
TEST_F(RepeaterTraceTest, FirstResultBeforeRetryTransmissionCancelsRetry) {
  begin();retry();ASSERT_EQ(tx.queue.size(),1u);ASSERT_TRUE(reply(first));
  EXPECT_TRUE(tx.queue.empty());EXPECT_EQ(tx.cancelled,1u);EXPECT_EQ(tx.emitted_traces,1u);
  EXPECT_EQ(session.result().attempt,0);
}
TEST_F(RepeaterTraceTest, FirstResultDuringRetryIsFallbackUntilRetryExpires) {
  begin();retry();second=tx.transmit(session);tx.now+=100;
  ASSERT_TRUE(reply(first));EXPECT_FALSE(session.hasResult());EXPECT_TRUE(session.active());
  tx.now+=TraceTimingPolicy::attemptTimeout(3,tx.airtime);session.tick();
  ASSERT_TRUE(session.hasResult());EXPECT_EQ(session.result().attempt,0);
}
TEST_F(RepeaterTraceTest, RetryResultTakesPrecedenceOverStoredFirstResult) {
  begin();retry();second=tx.transmit(session);tx.now+=100;
  ASSERT_TRUE(reply(first));tx.now+=200;ASSERT_TRUE(reply(second));
  EXPECT_EQ(session.result().attempt,1);EXPECT_EQ(session.result().elapsed_millis,300u);
}
TEST_F(RepeaterTraceTest, FinalGraceAcceptsLateResultButCommittedFailureDoesNot) {
  begin();retry();second=tx.transmit(session);
  tx.now+=TraceTimingPolicy::attemptTimeout(3,tx.airtime);session.tick();
  EXPECT_EQ(session.phase(),RepeaterTraceSession::FINAL_GRACE);
  tx.now+=2999;session.tick();ASSERT_TRUE(reply(first));EXPECT_TRUE(session.hasResult());
  FakeTraceTransport other;RepeaterTraceSession cancelled(other);
  EXPECT_FALSE(cancelled.receive(first,0,0,0,nullptr,nullptr,0));
}
TEST_F(RepeaterTraceTest, TwoTraceTimeoutsProduceBoundedFailureWithoutBlamingARepeater) {
  begin();retry();second=tx.transmit(session);
  tx.now+=TraceTimingPolicy::attemptTimeout(3,tx.airtime);session.tick();
  tx.now+=3000;session.tick();EXPECT_FALSE(session.active());
  EXPECT_EQ(session.error(),RepeaterTraceSession::TRACE_TIMEOUT);
  EXPECT_STREQ(session.phaseName(),"Trace: no result after retry");
  EXPECT_FALSE(reply(first));EXPECT_EQ(tx.emitted_traces,2u);EXPECT_EQ(tx.emitted_floods,1u);
}
TEST_F(RepeaterTraceTest, MalformedForeignAndPartialTraceRepliesCannotComplete) {
  begin();uint32_t auth;memcpy(&auth,first->payload+4,4);
  mesh::Packet p;p.header=ROUTE_TYPE_DIRECT|(PAYLOAD_TYPE_TRACE<<PH_TYPE_SHIFT);p.path_len=5;p._snr=4;
  uint8_t snrs[19]{},hashes[38]{};memcpy(hashes,first->payload+9,5);
  EXPECT_FALSE(session.receive(&p,tx.tag(first)+1,auth,0,snrs,hashes,5));
  EXPECT_FALSE(session.receive(&p,tx.tag(first),auth+1,0,snrs,hashes,5));
  EXPECT_FALSE(session.receive(&p,tx.tag(first),auth,1,snrs,hashes,5));
  EXPECT_FALSE(session.receive(&p,tx.tag(first),auth,0,snrs,hashes,4));
  hashes[0]^=1;EXPECT_FALSE(session.receive(&p,tx.tag(first),auth,0,snrs,hashes,5));hashes[0]^=1;
  p.path_len=3;EXPECT_FALSE(session.receive(&p,tx.tag(first),auth,0,snrs,hashes,5));
  p.path_len=5;EXPECT_FALSE(session.receive(&p,tx.tag(first),auth,0,nullptr,hashes,5));
  p.header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_TRACE<<PH_TYPE_SHIFT);
  EXPECT_FALSE(session.receive(&p,tx.tag(first),auth,0,snrs,hashes,5));EXPECT_TRUE(session.active());
}
TEST_F(RepeaterTraceTest, AlternateObservedRouteCanBeUsedForRetryAndFallbackStaysOriginal) {
  begin(2,1);uint8_t alternate[]={0x44};
  ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),alternate,1));
  retry();EXPECT_EQ(second->payload[9],0x44);second=tx.transmit(session);
  ASSERT_TRUE(reply(first));tx.now+=TraceTimingPolicy::attemptTimeout(2,tx.airtime);session.tick();
  ASSERT_TRUE(session.hasResult());EXPECT_EQ(session.result().route.path[0],0x11);
}
TEST_F(RepeaterTraceTest, AllocationFailureUsesBackoffAndBoundedAdmission) {
  tx.allocation_fails=true;ASSERT_TRUE(session.start(TARGET,true,1));session.tick();
  auto calls=tx.allocation_calls;for(int i=0;i<100;++i)session.tick();EXPECT_EQ(tx.allocation_calls,calls);
  tx.now=10000;session.tick();tx.now=20000;session.tick();EXPECT_FALSE(session.active());
  EXPECT_EQ(session.error(),RepeaterTraceSession::RADIO_BUSY);
  EXPECT_EQ(tx.emitted_floods,0u);EXPECT_EQ(tx.emitted_traces,0u);
}
TEST_F(RepeaterTraceTest, UnsentQueuedPacketIsCancelledOnQueueTimeout) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();ASSERT_EQ(tx.queue.size(),1u);
  tx.now=10000;session.tick();EXPECT_TRUE(tx.queue.empty());EXPECT_EQ(tx.cancelled,1u);
}
TEST_F(RepeaterTraceTest, FailedRadioStartConsumesOnlyOneOfTwoProbeSubmissions) {
  begin(); // initial trace has already transmitted; create another transaction instead
  session.cancel();ASSERT_TRUE(session.start(TARGET,true,1));session.tick();discovery=tx.transmit(session);
  ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),nullptr,0));tx.now+=3000;session.tick();
  first=tx.transmit(session,false);ASSERT_NE(first,nullptr);session.tick();
  EXPECT_EQ(session.phase(),RepeaterTraceSession::RETRY_PAUSE);
  tx.now+=2100;session.tick();second=tx.transmit(session,false);ASSERT_NE(second,nullptr);
  session.tick();tx.now+=3000;session.tick();EXPECT_FALSE(session.active());
  EXPECT_EQ(session.traceSubmissions(),2u);
}
TEST_F(RepeaterTraceTest, CancellationNeverCancelsAnUnownedPacketOrReopensFromLateReply) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();auto unrelated=&tx.pool[63];tx.queue.push_back(unrelated);
  session.cancel();ASSERT_EQ(tx.queue.size(),1u);EXPECT_EQ(tx.queue.front(),unrelated);
  session.tick();EXPECT_EQ(session.phase(),RepeaterTraceSession::CANCELLED);
  tx.queue.clear();begin();session.cancel();EXPECT_FALSE(reply(first));EXPECT_FALSE(session.hasResult());
}
TEST_F(RepeaterTraceTest, ContextAndPowerChangesInvalidateActiveTransaction) {
  begin();session.contextChanged();EXPECT_EQ(session.error(),RepeaterTraceSession::RADIO_CHANGED);
  EXPECT_FALSE(reply(first));EXPECT_FALSE(session.hasResult());
  EXPECT_FALSE(session.start(TARGET,false,1));EXPECT_EQ(session.error(),RepeaterTraceSession::POWER_RESTRICTED);
  EXPECT_FALSE(session.start(TARGET,true,3));EXPECT_EQ(session.error(),RepeaterTraceSession::UNSUPPORTED_PATH);
  EXPECT_FALSE(session.start(nullptr,true,1));
}
TEST_F(RepeaterTraceTest, TimerRolloverPreservesRttAndTimeouts) {
  tx.now=UINT32_MAX-10000;
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();discovery=tx.transmit(session);
  tx.now+=10;ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),nullptr,0));
  tx.now+=3000;session.tick();ASSERT_FALSE(tx.queue.empty());
  tx.now=UINT32_MAX-20;first=tx.transmit(session);tx.now+=75;
  ASSERT_TRUE(reply(first));EXPECT_EQ(session.result().elapsed_millis,75u);
}
TEST_F(RepeaterTraceTest, BusyStartAndTargetRemovalPreserveOwnership) {
  begin();EXPECT_FALSE(session.start(TARGET,true,1));EXPECT_TRUE(session.active());
  session.cancel(RepeaterTraceSession::NODE_REMOVED);
  EXPECT_EQ(session.error(),RepeaterTraceSession::NODE_REMOVED);EXPECT_FALSE(reply(first));
}
TEST_F(RepeaterTraceTest, NewTransactionResetsAttemptAndConfigurationInvalidatesCompletedResult) {
  begin();retry();second=tx.transmit(session);ASSERT_TRUE(reply(second));
  EXPECT_EQ(session.traceAttempt(),2u);
  ASSERT_TRUE(session.start(TARGET,true,1));EXPECT_EQ(session.traceAttempt(),1u);
  session.cancel();begin();ASSERT_TRUE(reply(first));ASSERT_TRUE(session.hasResult());
  session.contextChanged();EXPECT_FALSE(session.hasResult());
  EXPECT_EQ(session.phase(),RepeaterTraceSession::CANCELLED);
  EXPECT_EQ(session.error(),RepeaterTraceSession::RADIO_CHANGED);
}
TEST(TraceRoute, LimitsAndHashWidthsAreValidatedIncludingDirectNeighbour) {
  TraceRoute r;uint8_t path[64]{};
  EXPECT_EQ(TraceRoute::fromOutbound(nullptr,0,TARGET,r),TraceRoute::VALID);EXPECT_EQ(r.repeaters,1);
  EXPECT_EQ(TraceRoute::fromOutbound(path,64|9,TARGET,r),TraceRoute::VALID);EXPECT_EQ(r.repeaters,10);EXPECT_EQ(r.bytes,38);
  EXPECT_EQ(TraceRoute::fromOutbound(path,10,TARGET,r),TraceRoute::TOO_LONG);
  EXPECT_EQ(TraceRoute::fromOutbound(path,128|1,TARGET,r),TraceRoute::INVALID);
  EXPECT_EQ(TraceRoute::fromOutbound(path,255,TARGET,r),TraceRoute::INVALID);
  EXPECT_EQ(TraceRoute::fromOutbound(nullptr,1,TARGET,r),TraceRoute::INVALID);
}
TEST(TraceTiming, SensorPolicyIsAirtimeAwareAndBounded) {
  EXPECT_EQ(TraceTimingPolicy::attemptTimeout(2,1),10000u);
  EXPECT_EQ(TraceTimingPolicy::attemptTimeout(3,1),13000u);
  EXPECT_GT(TraceTimingPolicy::attemptTimeout(10,10000),30000u);
  EXPECT_GE(TraceTimingPolicy::settleDelay(1,0),300u);
  EXPECT_LE(TraceTimingPolicy::settleDelay(UINT32_MAX,250),2250u);
}
TEST_F(RepeaterTraceTest, OverheardPartialProgressCannotCompleteOrExtendTimeout) {
  begin();mesh::Packet progress=*first;progress.path_len=2;
  tx.now+=TraceTimingPolicy::attemptTimeout(3,tx.airtime)-1;
  session.observe(&progress);EXPECT_EQ(session.furthest(0),2);EXPECT_FALSE(session.hasResult());
  progress.payload[0]^=1;progress.path_len=3;session.observe(&progress);EXPECT_EQ(session.furthest(0),2);
  tx.now+=1;session.tick();EXPECT_EQ(session.phase(),RepeaterTraceSession::RETRY_PAUSE);
}
TEST_F(RepeaterTraceTest, MaximumTwoByteLoopFitsAndReturnsAllTwentyMeasurements) {
  uint8_t hops[18];for(unsigned i=0;i<18;++i)hops[i]=i+1;
  ASSERT_TRUE(session.start(TARGET,true,2));session.tick();discovery=tx.transmit(session);
  ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),hops,64|9));
  tx.now+=3000;session.tick();first=tx.transmit(session);ASSERT_NE(first,nullptr);
  EXPECT_EQ(first->payload_len,47);uint8_t values[19];for(unsigned i=0;i<19;++i)values[i]=i;
  tx.now+=123;ASSERT_TRUE(reply(first,-128,values));
  EXPECT_EQ(session.result().route.repeaters,10);
  EXPECT_EQ(RepeaterTraceSession::returning(session.result(),0),-128);
  EXPECT_EQ(RepeaterTraceSession::outward(session.result(),9),9);
  EXPECT_EQ(RepeaterTraceSession::returning(session.result(),9),10);
}
class TraceDisplay:public ZenDisplayDriver {
public:
  struct Bounds{int x,y,w,h;};std::vector<Bounds> bounds;std::vector<std::string> texts;
  int x=0,y=0,lh,cw;
  TraceDisplay(int w,int h,int line_height,int char_width):ZenDisplayDriver(w,h),lh(line_height),cw(char_width){}
  bool isOn() override{return true;}void turnOn() override{}void turnOff() override{}void clear() override{}
  void startFrame(ColorVal=0) override{bounds.clear();texts.clear();}void endFrame() override{}
  void setColor(ColorVal) override{}void setTextSize(int) override{}void setCursor(int a,int b) override{x=a;y=b;}
  void print(const char* text) override{bounds.push_back({x,y,int(strlen(text))*cw,lh});texts.emplace_back(text);}
  void fillRect(int a,int b,int w,int h) override{bounds.push_back({a,b,w,h});}
  void drawRect(int a,int b,int w,int h) override{bounds.push_back({a,b,w,h});}
  void drawXbm(int a,int b,const uint8_t*,int w,int h) override{bounds.push_back({a,b,w,h});}
  uint16_t getTextWidth(const char* text) override{return strlen(text)*cw;}
  int getLineHeight() const override{return lh;}int getCharWidth() const override{return cw;}
  bool inBounds() const {for(auto b:bounds)if(b.x<0||b.y<0||b.x+b.w>width()||b.y+b.h>height())return false;return true;}
};
TEST_F(RepeaterTraceTest, ResultsAndProgressFitOledAndBothEinkOrientations) {
  const int configs[][4]={{128,64,9,6},{122,250,9,6},{250,122,18,12}};
  for(const auto& c:configs){
    TraceDisplay display(c[0],c[1],c[2],c[3]);RepeaterTraceView view;
    auto lookup=[](const uint8_t*,unsigned,char* out,size_t size){snprintf(out,size,"Hillvue-Repeater-Long-Name");};
    view.render(display,session,"Long Repeater Name Here",lookup);EXPECT_TRUE(display.inBounds());
  }
  begin();uint8_t values[]={21,(uint8_t)-8,28,(uint8_t)-13,18};tx.now+=100;ASSERT_TRUE(reply(first,0,values));
  for(const auto& c:configs){
    TraceDisplay display(c[0],c[1],c[2],c[3]);RepeaterTraceView view;bool saw_exact=false,saw_zero=false;
    auto lookup=[](const uint8_t*,unsigned,char* out,size_t size){snprintf(out,size,"Hillvue-Repeater-Long-Name");};
    for(int i=0;i<35;++i){
      display.startFrame();view.render(display,session,"Hillvue Repeater",lookup);ASSERT_TRUE(display.inBounds());
      for(const auto& text:display.texts){if(text.find("+5.25")!=std::string::npos)saw_exact=true;if(text.find("+0.00")!=std::string::npos)saw_zero=true;}
      view.scroll(1);
    }
    EXPECT_TRUE(saw_exact);EXPECT_TRUE(saw_zero);
    view.scroll(-100);display.startFrame();view.render(display,session,"Hillvue Repeater",lookup);EXPECT_TRUE(display.inBounds());
  }
}
TEST(TraceNames, AmbiguousPrefixesStayHashesAndUtf8IsNotSplit) {
  uint8_t a[32]={1,2},b[32]={1,3};char out[40];TraceNameResolver one;
  one.observe(a,2,a,"Hillvue");one.observe(a,2,b,"Other");one.format(a,2,out,sizeof(out));EXPECT_STREQ(out,"Hillvue");
  TraceNameResolver collision;collision.observe(a,1,a,"Hillvue");collision.observe(a,1,b,"Other");
  collision.format(a,1,out,sizeof(out));EXPECT_STREQ(out,"?01");
  TraceNameResolver unknown;unknown.format(a,2,out,sizeof(out));EXPECT_STREQ(out,"0102");
  TraceNameResolver utf;utf.observe(a,2,a,"山谷");char short_name[5];utf.format(a,2,short_name,sizeof(short_name));EXPECT_STREQ(short_name,"山");
  unknown.format(nullptr,2,out,sizeof(out));EXPECT_STREQ(out,"?");
}
TEST(TraceFormatting, SignedQuarterDbValuesAreExactIncludingZero) {
  char out[16];RepeaterTraceView::snr(out,sizeof(out),0);EXPECT_STREQ(out,"+0.00");
  RepeaterTraceView::snr(out,sizeof(out),-1);EXPECT_STREQ(out,"-0.25");
  RepeaterTraceView::snr(out,sizeof(out),21);EXPECT_STREQ(out,"+5.25");
  RepeaterTraceView::snr(out,sizeof(out),-128);EXPECT_STREQ(out,"-32.00");
}
TEST_F(RepeaterTraceTest, DiscoveryClockIsIndependentFromRandomTraceIdsAndLatePathIsAbsorbed) {
  begin();EXPECT_EQ(tx.tag(discovery),1700000001u);
  EXPECT_GT(tx.tag(discovery),tx.tag(first));session.cancel();
  EXPECT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),nullptr,0));
  EXPECT_EQ(session.phase(),RepeaterTraceSession::CANCELLED);EXPECT_FALSE(session.hasResult());
}
TEST(TraceCandidates, DirectedEdgeDiversityAndHistoryExpiryFollowSensorPolicy) {
  TraceRoute a,b;uint8_t via[]={1,2},other[]={1,3};
  ASSERT_EQ(TraceRoute::fromOutbound(via,2,TARGET,a),TraceRoute::VALID);
  ASSERT_EQ(TraceRoute::fromOutbound(other,2,TARGET,b),TraceRoute::VALID);
  EXPECT_EQ(traceNovelEdges(a,a),0u);EXPECT_GT(traceNovelEdges(b,a),0u);
  TraceRouteHistory history;history.record(a,0,1,0,true,100);
  EXPECT_GT(history.rank(a,0,1,1),history.rank(b,0,1,1));
  EXPECT_EQ(history.rank(a,0,1,TraceRouteHistory::TTL_MILLIS),1);
  EXPECT_EQ(history.rank(a,0,2,1),1);
}
int main(int argc,char** argv){::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}

TEST_F(RepeaterTraceTest, ReTraceCancelsQueuedWorkAndStartsFreshDiscoveryForSameTarget) {
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();
  ASSERT_EQ(tx.queue.size(),1u);
  const uint32_t previous_tag=FakeTraceTransport::tag(tx.queue.front());
  session.cancel();
  EXPECT_TRUE(tx.queue.empty());
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();
  ASSERT_EQ(tx.queue.size(),1u);
  EXPECT_NE(FakeTraceTransport::tag(tx.queue.front()),previous_tag);
  EXPECT_EQ(session.floodSubmissions(),1u);
  EXPECT_EQ(session.traceSubmissions(),0u);
  EXPECT_FALSE(session.hasResult());
  EXPECT_TRUE(session.active());
}

TEST_F(RepeaterTraceTest, DisplayHoldTracksDiscoveryLoopAndResultWithOperationDeadlines) {
  DisplayActivity activity;
  ASSERT_TRUE(session.start(TARGET,true,1));session.tick();
  activity.beginSample(tx.now,true);
  activity.observe({DisplayActivity::REMOTE,1},session.active(),session.displayDeadline());
  EXPECT_TRUE(activity.endSample().hold);
  discovery=tx.transmit(session);tx.now+=20;
  ASSERT_TRUE(session.pathResponse(TARGET,tx.tag(discovery),via,1));
  activity.beginSample(tx.now,true);
  activity.observe({DisplayActivity::REMOTE,1},session.active(),session.displayDeadline());
  EXPECT_TRUE(activity.endSample().hold);
  tx.now+=3000;session.tick();first=tx.transmit(session);
  activity.beginSample(tx.now,true);
  activity.observe({DisplayActivity::REMOTE,1},session.active(),session.displayDeadline());
  EXPECT_TRUE(activity.endSample().hold);
  tx.now+=100;ASSERT_TRUE(reply(first));
  activity.beginSample(tx.now,true);
  activity.observe({DisplayActivity::REMOTE,1},session.active(),session.displayDeadline());
  auto done=activity.endSample(30000);
  EXPECT_TRUE(done.released);EXPECT_EQ(tx.now+30000,done.deadline);
}
TEST_F(RepeaterTraceTest, DisplayHoldSurvivesTraceRetryAndFinalGraceThenReleasesOnFailure) {
  DisplayActivity activity;begin(1);
  auto sample=[&]() {
    activity.beginSample(tx.now,true);
    activity.observe({DisplayActivity::REMOTE,1},session.active(),session.displayDeadline());
    return activity.endSample(30000);
  };
  EXPECT_TRUE(sample().hold);
  tx.now+=60000;session.tick();EXPECT_TRUE(sample().hold);
  tx.now+=2001;session.tick();second=tx.transmit(session);ASSERT_NE(second,nullptr);
  EXPECT_TRUE(sample().hold);
  tx.now+=60000;session.tick();EXPECT_TRUE(sample().hold);
  tx.now+=3001;session.tick();EXPECT_FALSE(session.active());
  auto d=sample();EXPECT_TRUE(d.released);EXPECT_EQ(tx.now+30000,d.deadline);
}
