#include <gtest/gtest.h>
#include <Dispatcher.h>
#include <helpers/StaticPoolPacketManager.h>
struct Clock:mesh::MillisecondClock { unsigned long now=100;unsigned long getMillis() override{return now;} };
struct Radio:mesh::Radio {
  bool busy=false,complete=false,reject=false;
  unsigned sends=0;
  int recvRaw(uint8_t*,int) override{return 0;}
  uint32_t getEstAirtimeFor(int) override{return 100;}
  float packetScore(float,int) override{return 1;}
  bool startSendRaw(const uint8_t*,int) override{++sends;return !reject;}
  bool isSendComplete() override{return complete;}
  void onSendFinished() override{complete=false;}
  bool isInRecvMode() const override{return true;}
  bool isReceiving() override{return busy;}
};
struct Dispatch:mesh::Dispatcher {
  unsigned starts=0,completed=0,failed=0;uint32_t started_at=0;mesh::Packet* started=nullptr;
  Dispatch(Radio& radio,Clock& clock,StaticPoolPacketManager& pool):Dispatcher(radio,clock,pool){}
  mesh::DispatcherAction onRecvPacket(mesh::Packet*) override{return ACTION_RELEASE;}
  void onTxStarted(mesh::Packet* p,uint32_t now) override{++starts;started_at=now;started=p;}
  void logTx(mesh::Packet*,int) override{++completed;}
  void logTxFail(mesh::Packet*,int) override{++failed;}
  bool queued(mesh::Packet* p) const{return isQueuedPacket(p);}
  bool cancel(mesh::Packet* p){return cancelQueuedPacket(p);}
  mesh::Packet* submit(uint32_t delay=0) {
    auto p=obtainNewPacket();p->header=ROUTE_TYPE_DIRECT|(PAYLOAD_TYPE_ACK<<PH_TYPE_SHIFT);
    p->payload_len=4;p->path_len=0;memset(p->payload,0,4);sendPacket(p,0,delay);return p;
  }
};
TEST(TraceDispatcher, TxHookUsesActualStartAndExcludesScheduledWait) {
  Radio radio;Clock clock;StaticPoolPacketManager pool(4);Dispatch mesh(radio,clock,pool);mesh.begin();
  auto packet=mesh.submit(500);clock.now=599;mesh.loop();EXPECT_EQ(mesh.starts,0u);
  clock.now=601;mesh.loop();EXPECT_EQ(mesh.starts,1u);EXPECT_EQ(mesh.started_at,601u);EXPECT_EQ(mesh.started,packet);
  EXPECT_FALSE(mesh.queued(packet));EXPECT_FALSE(mesh.cancel(packet)); // in-flight ownership stays with Dispatcher
  radio.complete=true;clock.now=650;mesh.loop();EXPECT_EQ(mesh.completed,1u);EXPECT_EQ(pool.getFreeCount(),4);
}
TEST(TraceDispatcher, CancellationOnlyRemovesRequestedQueuedPacket) {
  Radio radio;Clock clock;StaticPoolPacketManager pool(4);Dispatch mesh(radio,clock,pool);mesh.begin();
  auto a=mesh.submit(1000),b=mesh.submit(1000);
  ASSERT_TRUE(mesh.cancel(a));EXPECT_FALSE(mesh.cancel(a));EXPECT_TRUE(mesh.queued(b));
  EXPECT_EQ(pool.getFreeCount(),3);EXPECT_EQ(pool.getOutboundTotal(),1);
  clock.now=1101;mesh.loop();EXPECT_EQ(mesh.started,b);EXPECT_EQ(mesh.starts,1u);
  radio.complete=true;clock.now=1200;mesh.loop();EXPECT_EQ(pool.getFreeCount(),4);
  EXPECT_EQ(mesh.completed,1u);
}
TEST(TraceDispatcher, FailedRadioStartDoesNotCallStartedAndReleasesExactlyOnce) {
  Radio radio;radio.reject=true;Clock clock;StaticPoolPacketManager pool(4);Dispatch mesh(radio,clock,pool);mesh.begin();
  mesh.submit();clock.now=101;mesh.loop();EXPECT_EQ(mesh.starts,0u);EXPECT_EQ(mesh.failed,1u);EXPECT_EQ(pool.getFreeCount(),4);
  radio.reject=false;mesh.submit();clock.now=102;mesh.loop();EXPECT_EQ(mesh.starts,1u);
  radio.complete=true;clock.now=120;mesh.loop();EXPECT_EQ(pool.getFreeCount(),4);
  EXPECT_EQ(mesh.failed,1u);
}
TEST(TraceDispatcher, BusyChannelDoesNotStartTxClockEarly) {
  Radio radio;radio.busy=true;Clock clock;StaticPoolPacketManager pool(4);Dispatch mesh(radio,clock,pool);mesh.begin();
  mesh.submit();clock.now=200;mesh.loop();EXPECT_EQ(mesh.starts,0u);
  radio.busy=false;clock.now=1000;mesh.loop();EXPECT_EQ(mesh.starts,1u);EXPECT_EQ(mesh.started_at,1000u);
}
TEST(TraceDispatcher, StandardConsumerWithoutTxOverrideRetainsBaselineLifecycle) {
  struct Standard:mesh::Dispatcher {
    unsigned completed=0;
    Standard(Radio& r,Clock& c,StaticPoolPacketManager& p):Dispatcher(r,c,p){}
    mesh::DispatcherAction onRecvPacket(mesh::Packet*) override{return ACTION_RELEASE;}
    void logTx(mesh::Packet*,int) override{++completed;}
  };
  Radio radio;Clock clock;StaticPoolPacketManager pool(2);Standard mesh(radio,clock,pool);mesh.begin();
  auto p=mesh.obtainNewPacket();p->header=ROUTE_TYPE_DIRECT|(PAYLOAD_TYPE_ACK<<PH_TYPE_SHIFT);
  p->path_len=0;p->payload_len=4;memset(p->payload,0,4);mesh.sendPacket(p,0);
  clock.now=101;mesh.loop();EXPECT_EQ(radio.sends,1u);
  radio.complete=true;clock.now=150;mesh.loop();EXPECT_EQ(mesh.completed,1u);EXPECT_EQ(pool.getFreeCount(),2);
}
int main(int argc,char** argv){::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}
