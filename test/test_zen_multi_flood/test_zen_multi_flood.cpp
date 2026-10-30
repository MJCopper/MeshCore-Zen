#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/MultiFloodSetting.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/MultiFlood.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/MultiFloodContext.h"
using FloodTextDoubler=zen::MultiFlood;
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterTraceSession.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ManualTransmitActivity.h"
#include <array>
#include <vector>
#include <new>
#include <cstdlib>
static bool track_allocations=false;
static unsigned allocation_count=0;
void* operator new(std::size_t n) { if(track_allocations)++allocation_count;void* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p; }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace mesh;

class FakeClock : public MillisecondClock {
public:
  unsigned long now = 1000;
  unsigned long getMillis() override { return now; }
};

class FakeRNG : public RNG {
public:
  uint32_t value = 0;
  unsigned calls = 0;
  void random(uint8_t* dest, size_t len) override {
    EXPECT_EQ(sizeof(value), len);
    memcpy(dest, &value, len);
    calls++;
  }
};

class FakeRadio : public Radio {
public:
  bool transmitting = false, complete = false, busy = false, fail = false;
  unsigned finishes = 0;
  unsigned busyChecks = 0, busyOnCheck = 0;
  unsigned airtimeChecks = 0, advanceOnAirtimeCheck = 0;
  unsigned advanceMs = 0;
  FakeClock* clock = nullptr;
  std::vector<std::vector<uint8_t>> frames;
  std::vector<uint8_t> incoming;

  int recvRaw(uint8_t* bytes, int size) override {
    if (transmitting || incoming.empty()) return 0;
    EXPECT_LE(incoming.size(), static_cast<size_t>(size));
    int len = incoming.size();
    memcpy(bytes, incoming.data(), len);
    incoming.clear();
    return len;
  }
  zen::MultiFlood* policyToInvalidate=nullptr;
  unsigned invalidateAtAirtimeCheck=0;
  uint32_t getEstAirtimeFor(int) override {
    ++airtimeChecks;
    if(airtimeChecks==invalidateAtAirtimeCheck && policyToInvalidate)policyToInvalidate->invalidate();
    if (airtimeChecks == advanceOnAirtimeCheck && clock) clock->now += advanceMs;
    return 100;
  }
  float packetScore(float, int) override { return 1; }
  bool startSendRaw(const uint8_t* bytes, int len) override {
    if (fail) { fail = false; return false; }
    EXPECT_FALSE(transmitting);
    frames.emplace_back(bytes, bytes + len);
    transmitting = true;
    complete = false;
    return true;
  }
  bool isSendComplete() override { return complete; }
  void onSendFinished() override {
    transmitting = complete = false;
    finishes++;
  }
  bool isInRecvMode() const override { return !transmitting; }
  bool isReceiving() override {
    busyChecks++;
    return busy || (busyOnCheck && busyChecks >= busyOnCheck);
  }
};

class TestDispatcher : public Dispatcher {
public:
  FloodTextDoubler doubler;
  FakeRNG rng;
  SimpleMeshTables seen;
  bool enabled = true, forward = false;
  bool zen_policy=false,forwarding_enabled=true,available=true,first_trace=false;
  uint32_t profile=1,observed_profile=1;
  zen::MultiFloodContext context;
  uint8_t identity[PUB_KEY_SIZE]={1},scope[16]={};
  uint8_t hash_mode=0;
  void sync() {
    if(zen_policy) {
      if(profile!=observed_profile)doubler.invalidate();
      observed_profile=profile;
      context.packetContext(doubler,identity,hash_mode,scope);
      doubler.configure(doubler.getMode(),forwarding_enabled,available);
    }
  }
  unsigned operation_starts=0,operation_completions=0,operation_failures=0;
  zen::RepeaterTraceSession* session=nullptr;
  zen::ManualTransmitActivity activity;
  unsigned received = 0;
  unsigned logDelay = 0;
  float budgetFactor = 1;
  unsigned long budgetWindow = 3600000;

  TestDispatcher(FakeRadio& radio, FakeClock& clock, PacketManager& manager)
    : Dispatcher(radio, clock, manager) {}

protected:
  bool retainAfterTransmit(Packet* packet, uint32_t completed_at) override {
    sync();
    return enabled && doubler.onTransmitComplete(packet, *_mgr, rng, completed_at,first_trace);
  }
  void onPacketReleased(Packet* packet) override { doubler.forget(packet); }
  void serviceOptionalPackets(uint32_t now, const Packet* active) override {
    sync();
    if (enabled) while (auto packet = doubler.takeExpired(now, active)) releasePacket(packet);
  }
  Packet* getOptionalPacket(uint32_t now) override { sync();return enabled ? doubler.nextReady(now) : nullptr; }
  void deferOptionalPackets(uint32_t now) override { if (enabled) doubler.deferReady(now); }
  bool allowPacketStart(Packet* packet, uint32_t) override {
    sync();
    if (!enabled || !doubler.isRepeat(packet)) return true;
    if (_radio->isReceiving()) return false;
    return doubler.canStart(packet, (uint32_t)_ms->getMillis());
  }
  void onTxStarted(Packet* packet,uint32_t now) override {
    sync();
    bool repeat=doubler.isRepeat(packet);
    doubler.started(packet);
    if(zen_policy && repeat)return;
    ++operation_starts;activity.started(packet,now,100);
    if(session)session->txStarted(packet,now);
  }
  void logTx(Packet* packet, int) override {
    static_cast<FakeClock*>(_ms)->now += logDelay;
    if(zen_policy && doubler.isRepeat(packet))return;
    ++operation_completions;activity.complete(packet);
    first_trace=session && session->ownsPacket(packet);
    if(session)session->txComplete(packet);
  }
  void logTxFail(Packet* packet,int) override {
    if(zen_policy && doubler.isRepeat(packet))return;
    ++operation_failures;activity.complete(packet);
    if(session)session->txFailed(packet,uint32_t(_ms->getMillis()));
  }
public:
  bool cancelQueued(Packet* packet) { return cancelQueuedPacket(packet); }
protected:
  float getAirtimeBudgetFactor() const override { return budgetFactor; }
  unsigned long getDutyCycleWindowMs() const override { return budgetWindow; }
  int calcRxDelay(float, uint32_t) const override { return 0; }
  DispatcherAction onRecvPacket(Packet* packet) override {
    received++;
    // Use the production duplicate table; routing itself is not under test here.
    if (!forward || (zen_policy && !forwarding_enabled) || seen.wasSeen(packet)) return ACTION_RELEASE;
    seen.markSeen(packet);
    if(zen_policy)packet->setPathHashCount(packet->getPathHashCount()+1);
    doubler.noteForwarded(packet);
    return ACTION_RETRANSMIT(1);
  }
};

class DoublerTest : public ::testing::Test {
protected:
  FakeClock clock;
  FakeRadio radio;
  StaticPoolPacketManager manager{32};
  TestDispatcher dispatcher{radio, clock, manager};

  void SetUp() override {
    radio.clock = &clock;
    EXPECT_EQ(0, dispatcher.doubler.getMode());
    dispatcher.doubler.setMode(1);  // repetition tests explicitly enable channel text
    dispatcher.begin();
  }
  void tick(unsigned long ms = 1) { clock.now += ms; dispatcher.loop(); }
  Packet* packet(uint8_t route = ROUTE_TYPE_FLOOD, uint8_t type = PAYLOAD_TYPE_GRP_TXT) {
    Packet* p = dispatcher.obtainNewPacket();
    EXPECT_NE(nullptr, p);
    p->header = route | (type << PH_TYPE_SHIFT);
    p->path_len = 2;
    p->path[0] = 0x12;
    p->path[1] = 0x34;
    p->transport_codes[0] = 0x5678;
    p->transport_codes[1] = 0x9ABC;
    p->payload_len = 19;  // channel hash, MAC, encrypted block
    memset(p->payload, 0xA5, p->payload_len);
    return p;
  }
  void send(Packet* p, uint8_t priority = 1) {
    dispatcher.sendPacket(p, priority);
    tick();
    EXPECT_TRUE(radio.transmitting);
  }
  void finish() {
    radio.complete = true;
    tick(100);
  }
};

TEST_F(DoublerTest, SecondCopyIsIdenticalAndNeverCreatesThird) {
  send(packet());
  finish();
  EXPECT_FALSE(radio.transmitting);
  EXPECT_EQ(1u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(1u, radio.frames.size());
  tick(499);
  EXPECT_EQ(1u, radio.frames.size());
  tick();
  ASSERT_EQ(2u, radio.frames.size());
  EXPECT_EQ(radio.frames[0], radio.frames[1]);
  finish();
  tick(5000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  EXPECT_EQ(2u, dispatcher.getNumSentFlood());
  EXPECT_EQ(200u, dispatcher.getTotalAirTime());
}

TEST_F(DoublerTest, ScopedFloodPreservesTransportCodesAndEightHundredMsBoundary) {
  dispatcher.rng.value = 300;
  send(packet(ROUTE_TYPE_TRANSPORT_FLOOD));
  finish();
  tick(799);
  EXPECT_EQ(1u, radio.frames.size());
  tick();
  ASSERT_EQ(2u, radio.frames.size());
  EXPECT_EQ(radio.frames[0], radio.frames[1]);
  finish();
}

TEST_F(DoublerTest, LoggingDoesNotExtendTheSelectedGap) {
  dispatcher.logDelay = 200;
  send(packet()); finish();
  tick(299);
  EXPECT_EQ(1u, radio.frames.size());
  tick();  // 200 ms logging + 300 ms waiting = 500 ms since completion
  EXPECT_EQ(2u, radio.frames.size());
  finish();
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, DrawsFreshRandomDelayForEachFirstCompletion) {
  send(packet()); finish(); tick(500); finish();
  dispatcher.rng.value = 150;
  send(packet()); finish();
  tick(649);
  EXPECT_EQ(3u, radio.frames.size());
  tick();
  EXPECT_EQ(4u, radio.frames.size());
  finish();
  EXPECT_EQ(2u, dispatcher.rng.calls);
}

TEST_F(DoublerTest, EveryOtherPayloadAndDirectRouteIsExcluded) {
  for (uint8_t route = 0; route < 4; route++) {
    for (uint8_t type = 0; type < 16; type++) {
      if ((route == ROUTE_TYPE_FLOOD || route == ROUTE_TYPE_TRANSPORT_FLOOD)
          && type == PAYLOAD_TYPE_GRP_TXT) continue;
      auto before = radio.frames.size();
      send(packet(route, type)); finish(); tick(801);
      EXPECT_EQ(before + 1, radio.frames.size()) << +route << ":" << +type;
      EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
    }
  }
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, DisabledPolicyRetainsBaselineBehaviour) {
  dispatcher.enabled = false;
  send(packet()); finish(); tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ReceivesDuringGapAndDefersRepeatWhileReceiving) {
  send(packet()); finish();
  Packet* incoming = packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK);
  uint8_t raw[MAX_TRANS_UNIT];
  auto len = incoming->writeTo(raw);
  radio.incoming.assign(raw, raw + len);
  dispatcher.releasePacket(incoming);
  tick(100);
  EXPECT_EQ(1u, dispatcher.received);
  radio.busy = true;
  tick(400);  // repeat is now due, but cannot interrupt incoming traffic
  EXPECT_EQ(1u, radio.frames.size());
  tick(201);
  EXPECT_EQ(1u, radio.frames.size());
  radio.busy = false;
  tick(201);
  EXPECT_EQ(2u, radio.frames.size());
  finish();
}

TEST_F(DoublerTest, NormalPacketHasPriorityOverDueRepeat) {
  send(packet()); finish();
  Packet* normal = packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK);
  dispatcher.sendPacket(normal, 10);
  tick(500);
  ASSERT_EQ(2u, radio.frames.size());
  EXPECT_EQ(PAYLOAD_TYPE_ACK, (radio.frames.back()[0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK);
  finish();
  tick();  // baseline scheduler waits until next_tx_time has passed
  ASSERT_EQ(3u, radio.frames.size());
  EXPECT_EQ(radio.frames.front(), radio.frames.back());
  finish();
}

TEST_F(DoublerTest, FailedFirstStartDoesNotScheduleRepeat) {
  radio.fail = true;
  dispatcher.sendPacket(packet(), 1);
  tick(); tick(1000);
  EXPECT_TRUE(radio.frames.empty());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, FirstTimeoutDoesNotScheduleRepeat) {
  send(packet()); tick(151); tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, SecondStartFailureClearsPendingState) {
  send(packet()); finish();
  radio.fail = true;
  tick(500); tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  send(packet()); finish(); tick(500); finish();
  EXPECT_EQ(3u, radio.frames.size());  // pooled packets can start a fresh pair
}

TEST_F(DoublerTest, SecondTimeoutClearsPendingState) {
  send(packet()); finish(); tick(500); tick(151); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, LowAirtimeBudgetDefersSecondCopy) {
  dispatcher.budgetFactor = 9;
  dispatcher.budgetWindow = 1000;  // initial budget = 100 ms
  dispatcher.begin();
  send(packet()); finish();
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  tick(501);
  EXPECT_EQ(2u, radio.frames.size());
  finish();
  EXPECT_EQ(200u, dispatcher.getTotalAirTime());
}

TEST_F(DoublerTest, LowPoolCapacitySkipsRepeatAndReleasesOriginal) {
  send(packet());
  std::vector<Packet*> held;
  while (manager.getFreeCount() > 3) held.push_back(manager.allocNew());
  finish(); tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  for (auto p : held) dispatcher.releasePacket(p);
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ExactlyFourFreeSlotsStillAllowRepeat) {
  send(packet());
  std::vector<Packet*> held;
  while (manager.getFreeCount() > 4) held.push_back(manager.allocNew());
  finish();
  EXPECT_EQ(1u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(4, manager.getFreeCount());
  for (auto p : held) dispatcher.releasePacket(p);
  tick(500); finish();
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ExplicitReleaseClearsRepeatBookkeeping) {
  send(packet()); finish();
  Packet* p = dispatcher.doubler.nextReady((uint32_t)clock.now + 500);
  ASSERT_NE(nullptr, p);
  dispatcher.releasePacket(p);
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
}

TEST_F(DoublerTest, PendingRepeatsAreBounded) {
  for (unsigned i = 0; i < 5; i++) { send(packet()); finish(); }
  EXPECT_EQ(4u, dispatcher.doubler.pendingCount());
  // All repeats have lower priority than originals. Drain them and check leaks.
  for (unsigned i = 0; i < 4; i++) { tick(150); finish(); }
  EXPECT_EQ(9u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, DuplicateReceptionDoesNotCreateAnotherPair) {
  dispatcher.forward = true;
  Packet* incoming = packet();
  uint8_t raw[MAX_TRANS_UNIT];
  auto len = incoming->writeTo(raw);
  dispatcher.releasePacket(incoming);
  radio.incoming.assign(raw, raw + len);
  tick(); finish();
  radio.incoming.assign(raw, raw + len);
  tick(100);
  EXPECT_EQ(2u, dispatcher.received);
  EXPECT_EQ(1u, dispatcher.seen.getNumFloodDups());
  tick(400); finish(); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, RepeatDelayWrapsAtThirtyTwoBits) {
  // Native unsigned long is 64-bit; test the policy timestamps directly
  // with the actual uint32_t timestamps used on the nRF52.
  Packet* p = packet();
  uint32_t now = UINT32_MAX - 200;
  dispatcher.doubler.started(p);
  ASSERT_TRUE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, now));
  EXPECT_EQ(nullptr, dispatcher.doubler.nextReady(now + 499));
  EXPECT_EQ(p, dispatcher.doubler.nextReady(now + 500));
  EXPECT_FALSE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, now + 600));
  dispatcher.releasePacket(p);
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, RepeatedUseDoesNotExhaustPool) {
  for (unsigned i = 0; i < 200; i++) {
    send(packet()); finish(); tick(500); finish();
    ASSERT_EQ(32, manager.getFreeCount());
    ASSERT_EQ(0u, dispatcher.doubler.pendingCount());
  }
  EXPECT_EQ(400u, radio.frames.size());
}


TEST_F(DoublerTest, RepeatMayStartJustBeforeExpiryAndFinishAfterIt) {
  send(packet()); finish(); tick(2999);
  ASSERT_EQ(2u, radio.frames.size());
  tick();  // now expired, but the active transmission must not be freed
  EXPECT_TRUE(radio.transmitting);
  EXPECT_EQ(1u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(31, manager.getFreeCount());
  finish();
  EXPECT_EQ(32, manager.getFreeCount());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
}

TEST_F(DoublerTest, RepeatCannotStartAtExpiry) {
  send(packet()); finish(); tick(3000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  EXPECT_EQ(0, manager.getOutboundTotal());
}

TEST_F(DoublerTest, RepeatCannotStartAfterExpiry) {
  send(packet()); finish(); tick(5000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, WaitingRepeatExpiresWhileNormalPacketIsTransmitting) {
  send(packet()); finish();
  dispatcher.sendPacket(packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK), 1);
  tick(2990);
  ASSERT_TRUE(radio.transmitting);
  ASSERT_EQ(2u, radio.frames.size());
  tick(10);  // waiting repeat expires; active normal TX is left alone
  EXPECT_TRUE(radio.transmitting);
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(31, manager.getFreeCount());
  finish();
  EXPECT_EQ(32, manager.getFreeCount());
  EXPECT_EQ(2u, radio.frames.size());
}

TEST_F(DoublerTest, BusyWaitingDoesNotRefreshExpiryOrForceRepeat) {
  send(packet()); finish();
  radio.busy = true;
  tick(500);
  tick(2499);
  EXPECT_EQ(1u, dispatcher.doubler.pendingCount());
  tick();
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  tick(5000);  // beyond baseline CAD force-send timeout
  EXPECT_EQ(1u, radio.frames.size());
  radio.busy = false;
  tick();
  EXPECT_EQ(1u, radio.frames.size());
}

TEST_F(DoublerTest, BudgetBlockedRepeatStillExpires) {
  dispatcher.budgetFactor = 99;
  dispatcher.budgetWindow = 10000;
  dispatcher.begin();
  send(packet()); finish(); tick(3000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ContinuousNormalTrafficKeepsOrderAndExpiresRepeat) {
  send(packet()); finish();
  for (unsigned i = 0; i < 31; i++) {
    Packet* normal = packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK);
    normal->payload[0] = i;
    send(normal); finish();
    ASSERT_EQ(i + 2, radio.frames.size());
    EXPECT_EQ(i, radio.frames.back()[4]);  // header, path length, two path bytes
  }
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  send(packet()); finish(); tick(500); finish();
  EXPECT_EQ(34u, radio.frames.size());  // expired records can be reused
}

TEST_F(DoublerTest, MultipleExpiredCopiesReleaseDistinctPoolSlots) {
  for (unsigned i = 0; i < 4; i++) { send(packet()); finish(); }
  EXPECT_EQ(4u, dispatcher.doubler.pendingCount());
  tick(3000); tick(5000);
  EXPECT_EQ(4u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  std::vector<Packet*> allocated;
  for (unsigned i = 0; i < 32; i++) {
    Packet* p = manager.allocNew();
    ASSERT_NE(nullptr, p);
    for (auto previous : allocated) EXPECT_NE(previous, p);
    allocated.push_back(p);
  }
  EXPECT_EQ(nullptr, manager.allocNew());
  for (auto p : allocated) dispatcher.releasePacket(p);
}

TEST_F(DoublerTest, OptionalBusyWaitDoesNotDelayNewNormalPacket) {
  send(packet()); finish();
  radio.busy = true;
  tick(500);
  radio.busy = false;
  send(packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK));  // no optional 200 ms backoff
  ASSERT_EQ(2u, radio.frames.size());
  EXPECT_EQ(PAYLOAD_TYPE_ACK, (radio.frames.back()[0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK);
  finish(); tick(101); finish();  // optional retry pause does not affect normal TX
  EXPECT_EQ(3u, radio.frames.size());
}

TEST_F(DoublerTest, OptionalBusyWaitDoesNotStartNormalCadTimer) {
  send(packet()); finish();
  radio.busy = true;
  tick(500); tick(2000);
  dispatcher.sendPacket(packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK), 1);
  tick();  // normal packet starts its own baseline CAD wait here
  tick(500); tick(3001);
  EXPECT_EQ(1u, radio.frames.size());
  tick(501);  // now normal packet's own CAD timer exceeds four seconds
  ASSERT_EQ(2u, radio.frames.size());
  EXPECT_EQ(PAYLOAD_TYPE_ACK, (radio.frames.back()[0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK);
  finish();
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
}

TEST_F(DoublerTest, OptionalBusyChecksHaveLocalBackoffWithoutExtendingExpiry) {
  send(packet()); finish();
  radio.busy = true;
  tick(500);
  auto checks = radio.busyChecks;
  tick(199);
  EXPECT_EQ(checks, radio.busyChecks);
  tick();
  EXPECT_EQ(checks + 1, radio.busyChecks);
  tick(2300);  // three seconds total; local retry pauses do not refresh expiry
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  EXPECT_EQ(1u, radio.frames.size());
}

TEST_F(DoublerTest, FinalGuardDropsRepeatWhenReceptionStartsAfterInitialCheck) {
  send(packet()); finish();
  radio.busyOnCheck = radio.busyChecks + 2;  // idle at scheduler check, busy at final guard
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, FinalGuardDropsRepeatThatExpiresDuringPreparation) {
  send(packet()); finish();
  radio.advanceOnAirtimeCheck = radio.airtimeChecks + 2;
  radio.advanceMs = 1;  // expiry crossed during final airtime/serialization preparation
  tick(2999);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ExpiryWrapsAtThirtyTwoBits) {
  Packet* p = packet();
  uint32_t now = UINT32_MAX - 1000;
  dispatcher.doubler.started(p);
  ASSERT_TRUE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, now));
  EXPECT_EQ(p, dispatcher.doubler.nextReady(now + 2999));
  EXPECT_EQ(nullptr, dispatcher.doubler.takeExpired(now + 2999, nullptr));
  EXPECT_FALSE(dispatcher.doubler.canStart(p, now + 3000));
  EXPECT_EQ(nullptr, dispatcher.doubler.nextReady(now + 3000));
  EXPECT_EQ(p, dispatcher.doubler.takeExpired(now + 3000, nullptr));
  dispatcher.releasePacket(p);
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}


TEST_F(DoublerTest, AllModeRouteAndPayloadCombinationsHaveExpectedSendCounts) {
  for (uint8_t mode = 0; mode <= 2; mode++) {
    dispatcher.doubler.setMode(mode);
    for (uint8_t route = 0; route < 4; route++) {
      for (uint8_t type = 0; type < 16; type++) {
        auto before = radio.frames.size();
        bool flood = route == ROUTE_TYPE_FLOOD || route == ROUTE_TYPE_TRANSPORT_FLOOD;
        bool repeat = flood && (mode == 2 || (mode == 1 && type == PAYLOAD_TYPE_GRP_TXT));
        send(packet(route, type)); finish(); tick(500);
        EXPECT_EQ(before + (repeat ? 2 : 1), radio.frames.size()) << +mode << ":" << +route << ":" << +type;
        if (repeat) finish();
        EXPECT_EQ(32, manager.getFreeCount());
        EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
      }
    }
  }
}

TEST_F(DoublerTest, TurningOffCancelsWaitingCopiesOnNextLoop) {
  send(packet()); finish();
  dispatcher.doubler.setMode(0);
  tick();
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
}

TEST_F(DoublerTest, NarrowingModeCancelsOnlyNonChannelCopies) {
  dispatcher.doubler.setMode(2);
  send(packet(ROUTE_TYPE_FLOOD, PAYLOAD_TYPE_ACK)); finish();
  send(packet()); finish();
  EXPECT_EQ(2u, dispatcher.doubler.pendingCount());
  dispatcher.doubler.setMode(1);
  tick();
  EXPECT_EQ(1u, dispatcher.doubler.pendingCount());
  tick(499); finish();
  ASSERT_EQ(3u, radio.frames.size());
  EXPECT_EQ(radio.frames[1], radio.frames[2]);
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, WideningModeDoesNotRefreshExistingDelayOrExpiry) {
  send(packet()); finish();
  tick(499);
  dispatcher.doubler.setMode(2);
  tick();
  ASSERT_EQ(2u, radio.frames.size());
  finish();
  dispatcher.doubler.setMode(1);
  send(packet()); finish();
  tick(2999);
  ASSERT_TRUE(radio.transmitting);
  dispatcher.doubler.setMode(2);
  finish();
  EXPECT_EQ(32, manager.getFreeCount());
  send(packet()); finish();
  tick(2999);
  dispatcher.doubler.setMode(1);  // active repeat can finish despite mode change
  finish();
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
}

TEST_F(DoublerTest, ModeChangeCannotExtendWaitingCopyExpiry) {
  send(packet()); finish();
  radio.busy = true;
  tick(2900);
  dispatcher.doubler.setMode(2);
  tick(100);
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
  EXPECT_EQ(1u, radio.frames.size());
}

TEST_F(DoublerTest, ActiveRepeatFinishesWhenFeatureIsTurnedOff) {
  send(packet()); finish(); tick(500);
  ASSERT_TRUE(radio.transmitting);
  dispatcher.doubler.setMode(0);
  tick();
  EXPECT_EQ(1u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(31, manager.getFreeCount());
  finish(); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ActiveNonChannelRepeatIsNotRepeatedAgainWhenModeNarrows) {
  dispatcher.doubler.setMode(2);
  send(packet(ROUTE_TYPE_FLOOD, PAYLOAD_TYPE_ADVERT)); finish(); tick(500);
  dispatcher.doubler.setMode(1);
  finish(); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, EnableAppliesAtFirstCompletionWithoutReplayingPastPackets) {
  dispatcher.doubler.setMode(0);
  send(packet()); finish();
  dispatcher.doubler.setMode(1);
  tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  send(packet());
  dispatcher.doubler.setMode(0);
  finish(); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());  // disabled before first TX completion
  send(packet());
  dispatcher.doubler.setMode(1);
  finish(); tick(500); finish();
  EXPECT_EQ(4u, radio.frames.size());
}

TEST_F(DoublerTest, DirectAckAndMultipartAckStaySingleInEveryMode) {
  // Baseline multi.acks emits direct ACK/multipart-ACK packets. Vary its packet
  // count here; the baseline ACK generation function itself is unchanged.
  for (uint8_t mode = 0; mode <= 2; mode++) {
    dispatcher.doubler.setMode(mode);
    for (unsigned extra = 0; extra <= 1; extra++) {
      auto before = radio.frames.size();
      for (unsigned i = 0; i < extra; i++) {
        send(packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_MULTIPART)); finish();
      }
      send(packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK)); finish(); tick(1000);
      EXPECT_EQ(before + extra + 1, radio.frames.size());
      EXPECT_EQ(32, manager.getFreeCount());
    }
  }
}

TEST_F(DoublerTest, ZenOriginForwardingModeAndPayloadMatrix) {
  dispatcher.zen_policy=true;
  for(unsigned mode:{0u,1u,2u})for(bool forwarding:{false,true})for(bool origin:{false,true})
    for(uint8_t route:{uint8_t(ROUTE_TYPE_FLOOD),uint8_t(ROUTE_TYPE_TRANSPORT_FLOOD),uint8_t(ROUTE_TYPE_DIRECT),uint8_t(ROUTE_TYPE_TRANSPORT_DIRECT)})
    for(uint8_t width:{uint8_t(1),uint8_t(2),uint8_t(3)})
    for(uint8_t type:{uint8_t(PAYLOAD_TYPE_REQ),uint8_t(PAYLOAD_TYPE_RESPONSE),uint8_t(PAYLOAD_TYPE_TXT_MSG),
      uint8_t(PAYLOAD_TYPE_ACK),uint8_t(PAYLOAD_TYPE_ADVERT),uint8_t(PAYLOAD_TYPE_GRP_TXT),uint8_t(PAYLOAD_TYPE_GRP_DATA),
      uint8_t(PAYLOAD_TYPE_ANON_REQ),uint8_t(PAYLOAD_TYPE_PATH),uint8_t(PAYLOAD_TYPE_TRACE),uint8_t(PAYLOAD_TYPE_MULTIPART),
      uint8_t(PAYLOAD_TYPE_CONTROL),uint8_t(PAYLOAD_TYPE_RAW_CUSTOM)}) {
      dispatcher.doubler.setMode(mode);dispatcher.forwarding_enabled=forwarding;
      auto p=packet(route,type);p->setPathHashSizeAndCount(width,origin?0:1);
      if(!origin)dispatcher.doubler.noteForwarded(p);
      unsigned before=radio.frames.size();send(p);finish();
      bool repeat=mode && (route==ROUTE_TYPE_FLOOD || route==ROUTE_TYPE_TRANSPORT_FLOOD) && (mode==2 || type==PAYLOAD_TYPE_GRP_TXT) && (forwarding || origin);
      EXPECT_EQ(repeat?1u:0u,dispatcher.doubler.pendingCount());
      tick(800);if(radio.transmitting)finish();tick(4000);
      EXPECT_EQ(before+(repeat?2:1),radio.frames.size())<<mode<<forwarding<<origin<<int(type);
      if(repeat && radio.frames.size()==before+2)EXPECT_EQ(radio.frames[before],radio.frames[before+1]);
      EXPECT_EQ(32,manager.getFreeCount());
    }
}
TEST_F(DoublerTest, ZenDisablingForwardingCancelsOnlyForwardedWaitingCopies) {
  dispatcher.zen_policy=true;
  for(unsigned i=0;i<4;++i) { auto p=packet();p->setPathHashCount(i%2);send(p);finish(); }
  ASSERT_EQ(4u,dispatcher.doubler.pendingCount());dispatcher.forwarding_enabled=false;tick();
  EXPECT_EQ(2u,dispatcher.doubler.pendingCount());
  for(unsigned i=0;i<4;++i) { tick(500);if(radio.transmitting)finish(); }
  EXPECT_EQ(6u,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
}
TEST_F(DoublerTest, ZenActiveForwardedRepeatFinishesAfterForwardingDisabled) {
  dispatcher.zen_policy=true;send(packet());finish();tick(500);ASSERT_TRUE(radio.transmitting);
  dispatcher.forwarding_enabled=false;
  dispatcher.doubler.configure(1,false,true);
  EXPECT_EQ(nullptr,dispatcher.doubler.takeExpired(clock.now,nullptr)); // app-level purge cannot free active
  EXPECT_EQ(31,manager.getFreeCount());tick();ASSERT_TRUE(radio.transmitting);finish();
  EXPECT_EQ(2u,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
}
TEST_F(DoublerTest, ZenProfilePowerAndOffOnTransitionsCannotResurrectWaitingCopies) {
  dispatcher.zen_policy=true;
  for(unsigned change:{0u,1u,2u,3u}) {
    dispatcher.available=true;dispatcher.doubler.setMode(2);auto p=packet();p->setPathHashCount(0);send(p);finish();
    unsigned before=radio.frames.size();
    if(change==0)++dispatcher.profile;
    if(change==1)dispatcher.available=false;
    if(change==2)dispatcher.doubler.invalidate();
    if(change==3) {
      dispatcher.doubler.configure(0,true,true);
      dispatcher.doubler.configure(2,true,true);
    }
    tick();EXPECT_EQ(0u,dispatcher.doubler.pendingCount());tick(4000);
    EXPECT_EQ(before,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
  }
}
TEST_F(DoublerTest, ZenRepeatCallbacksCannotCompleteOrFailOperationAgain) {
  dispatcher.zen_policy=true;
  for(unsigned result:{0u,1u,2u}) {
    auto p=packet();p->setPathHashCount(0);ASSERT_TRUE(dispatcher.activity.begin(p,clock.now));
    auto completed=dispatcher.operation_completions,failed=dispatcher.operation_failures,started=dispatcher.operation_starts;
    send(p);finish();EXPECT_EQ(completed+1,dispatcher.operation_completions);
    if(result==1)radio.fail=true;
    tick(500);
    if(result==2)tick(200);else if(radio.transmitting)finish();
    EXPECT_EQ(started+1,dispatcher.operation_starts);
    EXPECT_EQ(completed+1,dispatcher.operation_completions);EXPECT_EQ(failed,dispatcher.operation_failures);
    EXPECT_FALSE(dispatcher.activity.complete(p));EXPECT_EQ(32,manager.getFreeCount());
  }
}
TEST_F(DoublerTest, ZenOptionalTrafficIsCountedAndChargedToAirtime) {
  dispatcher.zen_policy=true;auto p=packet();p->setPathHashCount(0);send(p);finish();
  auto first=dispatcher.getTotalAirTime();tick(500);finish();
  EXPECT_EQ(first+100,dispatcher.getTotalAirTime());EXPECT_EQ(2u,dispatcher.getNumSentFlood());
}
TEST_F(DoublerTest, ZenRejectsReceivedForwardingButStillRepeatsOwnChannelText) {
  dispatcher.zen_policy=true;dispatcher.forward=true;dispatcher.forwarding_enabled=false;
  auto own=packet();own->setPathHashCount(0);
  radio.incoming={uint8_t(ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_GRP_TXT<<PH_TYPE_SHIFT)),0};
  radio.incoming.insert(radio.incoming.end(),19,0xA6);tick();EXPECT_EQ(0u,radio.frames.size());
  send(own);finish();tick(500);finish();EXPECT_EQ(2u,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
}
TEST_F(DoublerTest, ZenReceivingForwardedFloodAppendsPathBeforeOriginClassification) {
  dispatcher.zen_policy=true;dispatcher.forward=true;
  radio.incoming={uint8_t(ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_GRP_TXT<<PH_TYPE_SHIFT)),0};
  radio.incoming.insert(radio.incoming.end(),19,0xA6);tick();
  ASSERT_TRUE(radio.transmitting);ASSERT_GE(radio.frames[0].size(),3u);EXPECT_EQ(1,radio.frames[0][1]);
  finish();dispatcher.forwarding_enabled=false;tick();EXPECT_EQ(0u,dispatcher.doubler.pendingCount());EXPECT_EQ(32,manager.getFreeCount());
}
struct ZenTraceTransport:zen::RepeaterTraceTransport {
  TestDispatcher& dispatcher;FakeClock& clock;
  ZenTraceTransport(TestDispatcher& d,FakeClock& c):dispatcher(d),clock(c){}
  uint32_t traceNow() const override{return clock.now;}
  uint32_t traceRandom() override{return 123;}
  uint32_t traceDiscoveryTag() override{return 1700000000;}
  uint32_t traceAirtime(unsigned) const override{return 100;}
  uint32_t traceFloodTimeout(uint32_t) const override{return 5000;}
  Packet* traceQueueDiscovery(const uint8_t*,uint32_t tag) override {
    auto p=dispatcher.obtainNewPacket();if(!p)return nullptr;
    p->header=ROUTE_TYPE_FLOOD|(PAYLOAD_TYPE_REQ<<PH_TYPE_SHIFT);p->path_len=0;p->payload_len=20;
    memset(p->payload,0,20);memcpy(p->payload,&tag,4);dispatcher.sendPacket(p,1);return p;
  }
  Packet* traceQueueLoop(const zen::TraceRoute&,uint32_t,uint32_t) override{return nullptr;}
  bool traceCancelQueued(Packet* p) override{return dispatcher.cancelQueued(p);}
};
TEST_F(DoublerTest, ZenModeTwoDoesNotRestartRealTraceStateAndCancellationReleasesCopy) {
  dispatcher.zen_policy=true;dispatcher.forwarding_enabled=false;dispatcher.doubler.setMode(2);
  ZenTraceTransport transport(dispatcher,clock);zen::RepeaterTraceSession session(transport);dispatcher.session=&session;
  uint8_t target[32]={7};ASSERT_TRUE(session.start(target,true,1));session.tick();tick();ASSERT_TRUE(radio.transmitting);
  finish();ASSERT_EQ(1u,dispatcher.doubler.pendingCount());auto phase=session.phase();auto attempt=session.discoveryAttempt();auto remaining=session.remainingMillis();
  tick(500);ASSERT_TRUE(radio.transmitting);finish();
  EXPECT_EQ(phase,session.phase());EXPECT_EQ(attempt,session.discoveryAttempt());EXPECT_LT(session.remainingMillis(),remaining);
  EXPECT_EQ(1u,dispatcher.operation_completions);EXPECT_EQ(1u,dispatcher.operation_starts);
  session.cancel();dispatcher.session=nullptr;
  // A fresh real trace request is then cancelled during the optional gap.
  zen::RepeaterTraceSession again(transport);dispatcher.session=&again;ASSERT_TRUE(again.start(target,true,1));again.tick();tick();finish();
  ASSERT_EQ(1u,dispatcher.doubler.pendingCount());again.cancel();dispatcher.doubler.cancelTrace();tick();
  EXPECT_EQ(0u,dispatcher.doubler.pendingCount());EXPECT_EQ(32,manager.getFreeCount());dispatcher.session=nullptr;
}
TEST_F(DoublerTest, ZenTraceCancellationKeepsUnrelatedLocalMessages) {
  dispatcher.doubler.setMode(2);auto control=packet(ROUTE_TYPE_FLOOD,PAYLOAD_TYPE_REQ);control->path_len=0;
  dispatcher.doubler.started(control);
  ASSERT_TRUE(dispatcher.doubler.onTransmitComplete(control,manager,dispatcher.rng,uint32_t(clock.now),true));
  auto text=packet();text->path_len=0;dispatcher.doubler.started(text);ASSERT_TRUE(dispatcher.doubler.onTransmitComplete(text,manager,dispatcher.rng,uint32_t(clock.now),false));
  dispatcher.doubler.cancelTrace();auto expired=dispatcher.doubler.takeExpired(clock.now,nullptr);ASSERT_EQ(control,expired);dispatcher.releasePacket(expired);
  EXPECT_EQ(1u,dispatcher.doubler.pendingCount());EXPECT_EQ(text,dispatcher.doubler.nextReady(clock.now+500));
  dispatcher.releasePacket(text);EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, ZenSchedulerHasFixedMemoryAndNeverAllocates) {
  zen::MultiFlood policy;policy.configure(2,true,true);
  Packet* packets[4];for(auto& p:packets) { p=packet();p->path_len=0; }
  bool accepted=true;allocation_count=0;track_allocations=true;
  for(auto p:packets) { policy.started(p);accepted=policy.onTransmitComplete(p,manager,dispatcher.rng,clock.now)&&accepted; }
  auto ready=policy.nextReady(clock.now+500);policy.deferReady(clock.now+500);
  policy.configure(0,false,false);
  while(auto p=policy.takeExpired(clock.now,nullptr))manager.free(p);
  track_allocations=false;
  EXPECT_TRUE(accepted);EXPECT_NE(nullptr,ready);EXPECT_EQ(0u,allocation_count);
  EXPECT_EQ(0u,policy.pendingCount());EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, ZenBothOriginsAndScopedFloodsPreserveEverySupportedHashWidth) {
  dispatcher.zen_policy=true;
  for(uint8_t route:{uint8_t(ROUTE_TYPE_FLOOD),uint8_t(ROUTE_TYPE_TRANSPORT_FLOOD)})
    for(uint8_t width:{uint8_t(1),uint8_t(2),uint8_t(3)})for(bool local:{false,true}) {
      dispatcher.forwarding_enabled=!local;
      auto p=packet(route);p->setPathHashSizeAndCount(width,local?0:1);
      EXPECT_EQ(!local,dispatcher.doubler.forwarded(p));
      unsigned before=radio.frames.size();send(p);finish();tick(500);finish();
      ASSERT_EQ(before+2,radio.frames.size());EXPECT_EQ(radio.frames[before],radio.frames[before+1]);
      EXPECT_EQ(32,manager.getFreeCount());
    }
}

TEST_F(DoublerTest, ZenReconfigurationDuringOriginalTxCannotCreateCopyOnNewProfile) {
  dispatcher.zen_policy=true;dispatcher.doubler.setMode(2);
  for(unsigned change:{0u,1u,2u}) {
    auto p=packet();p->path_len=0;send(p);unsigned before=radio.frames.size();
    if(change==0) { ++dispatcher.profile;tick(); }
    if(change==1) { dispatcher.available=false;tick();dispatcher.available=true;tick(); }
    if(change==2)dispatcher.doubler.invalidate();
    finish();EXPECT_EQ(0u,dispatcher.doubler.pendingCount());tick(1000);
    EXPECT_EQ(before,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
  }
  // A later ordinary first TX re-arms the latch and remains eligible.
  auto p=packet();p->path_len=0;send(p);finish();EXPECT_EQ(1u,dispatcher.doubler.pendingCount());tick(500);finish();EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, OffOnBeforeCleanupNeverResurrectsCopy) {
  send(packet()); finish();
  dispatcher.doubler.setMode(0);
  dispatcher.doubler.setMode(1);
  EXPECT_EQ(nullptr, dispatcher.doubler.nextReady(clock.now + 500));
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, NarrowWidenBeforeCleanupKeepsOnlyContinuouslyEligibleCopy) {
  dispatcher.doubler.setMode(2);
  send(packet(ROUTE_TYPE_FLOOD, PAYLOAD_TYPE_ACK)); finish();
  send(packet()); finish();
  dispatcher.doubler.setMode(1);
  dispatcher.doubler.setMode(2);
  tick(500); finish();
  ASSERT_EQ(3u, radio.frames.size());
  EXPECT_EQ(radio.frames[1], radio.frames[2]);
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, RadioContextChangeDropsOldCopyAndAllowsFreshProfile) {
  zen::MultiFloodContext context;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  send(packet()); finish();
  context.radioApplied(dispatcher.doubler, 917.0f, 62.5f, 7, 7);
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  send(packet()); finish(); tick(500); finish();
  ASSERT_EQ(3u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ContextChangeDuringOriginalRejectsItsCompletionCopy) {
  zen::MultiFloodContext context;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  send(packet());
  context.radioApplied(dispatcher.doubler, 917.0f, 62.5f, 7, 7);
  finish(); tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  send(packet()); finish(); tick(500); finish();
  EXPECT_EQ(3u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, RadioRoundTripCannotReviveWaitingOrActiveOriginal) {
  zen::MultiFloodContext context;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  for (bool active : {false, true}) {
    auto before = radio.frames.size();
    send(packet());
    if (!active) finish();
    context.radioApplied(dispatcher.doubler, 917.0f, 125.0f, 8, 5);
    context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
    if (active) finish();
    tick(500);
    EXPECT_EQ(before + 1, radio.frames.size());
    EXPECT_EQ(32, manager.getFreeCount());
  }
}

TEST_F(DoublerTest, ContextChangeNeverFreesInFlightCopyEvenWithoutActiveArgument) {
  send(packet()); finish(); tick(500);
  ASSERT_TRUE(radio.transmitting);
  dispatcher.doubler.invalidate();
  EXPECT_EQ(nullptr, dispatcher.doubler.takeExpired(clock.now + 5000, nullptr));
  EXPECT_EQ(nullptr, dispatcher.doubler.nextReady(clock.now));
  EXPECT_EQ(31, manager.getFreeCount());
  finish(); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, AvailabilityOffOnPermanentlyCancelsOldCopies) {
  send(packet()); finish();
  dispatcher.doubler.configure(dispatcher.doubler.getMode(),true,false);
  dispatcher.doubler.configure(dispatcher.doubler.getMode(),true,true);
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
  send(packet());
  dispatcher.doubler.configure(dispatcher.doubler.getMode(),true,false);
  dispatcher.doubler.configure(dispatcher.doubler.getMode(),true,true);
  finish(); tick(500);
  EXPECT_EQ(2u, radio.frames.size());
  send(packet()); finish(); tick(500); finish();
  EXPECT_EQ(4u, radio.frames.size());
}

TEST_F(DoublerTest, IdentityHashModeAndDefaultScopeChangesCancelCopies) {
  for (unsigned change = 0; change < 3; change++) {
    zen::MultiFloodContext context;
    uint8_t identity[PUB_KEY_SIZE] = {}, scope[16] = {}, hashMode = 0;
    context.packetContext(dispatcher.doubler, identity, hashMode, scope);
    auto before = radio.frames.size();
    send(packet()); finish();
    if (change == 0) identity[0] = 1;
    if (change == 1) hashMode = 2;
    if (change == 2) scope[0] = 1;
    context.packetContext(dispatcher.doubler, identity, hashMode, scope);
    tick(500);
    EXPECT_EQ(before + 1, radio.frames.size());
    EXPECT_EQ(32, manager.getFreeCount());
  }
}

TEST_F(DoublerTest, EachAppliedRadioParameterInvalidates) {
  for (unsigned change = 0; change < 4; change++) {
    zen::MultiFloodContext context;
    context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
    auto before = radio.frames.size();
    send(packet()); finish();
    context.radioApplied(dispatcher.doubler, change == 0 ? 917.0f : 916.575f,
                         change == 1 ? 125.0f : 62.5f, change == 2 ? 8 : 7, change == 3 ? 5 : 7);
    tick(500);
    EXPECT_EQ(before + 1, radio.frames.size());
    EXPECT_EQ(32, manager.getFreeCount());
  }
}

TEST_F(DoublerTest, RepeatedIdenticalObservationsDoNotCancelOrRefreshExpiry) {
  zen::MultiFloodContext context;
  uint8_t identity[PUB_KEY_SIZE] = {}, scope[16] = {};
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  context.packetContext(dispatcher.doubler, identity, 0, scope);
  send(packet()); finish();
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  context.packetContext(dispatcher.doubler, identity, 0, scope);
  dispatcher.doubler.setMode(1);
  tick(2999);
  ASSERT_EQ(2u, radio.frames.size());
  finish();
  send(packet()); finish();
  tick(2999);
  context.packetContext(dispatcher.doubler, identity, 0, scope);
  finish();
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, DifferentPerPacketScopesCanBothRepeatWithoutProfileChange) {
  zen::MultiFloodContext context;
  uint8_t identity[PUB_KEY_SIZE] = {}, defaultScope[16] = {};
  context.packetContext(dispatcher.doubler, identity, 0, defaultScope);
  send(packet(ROUTE_TYPE_TRANSPORT_FLOOD)); finish();
  auto second = packet(ROUTE_TYPE_TRANSPORT_FLOOD);
  second->transport_codes[0] ^= 0xFFFF;
  send(second); finish();
  context.packetContext(dispatcher.doubler, identity, 0, defaultScope);
  tick(399); finish(); tick(); finish();
  ASSERT_EQ(4u, radio.frames.size());
  EXPECT_EQ(radio.frames[0], radio.frames[2]);
  EXPECT_EQ(radio.frames[1], radio.frames[3]);
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, FinalGuardRejectsContextChangeDuringPreparation) {
  send(packet()); finish();
  radio.policyToInvalidate = &dispatcher.doubler;
  radio.invalidateAtAirtimeCheck = radio.airtimeChecks + 2;
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, CompletionWithoutSuccessfulStartFailsClosed) {
  auto p = packet();
  EXPECT_FALSE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, clock.now));
  dispatcher.doubler.started(p);
  dispatcher.doubler.invalidate();
  dispatcher.releasePacket(p);
  EXPECT_FALSE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, clock.now));
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ContextAndCancellationPolicyNeverAllocate) {
  auto p = packet();
  zen::MultiFloodContext context;
  uint8_t identity[PUB_KEY_SIZE] = {}, scope[16] = {};
  allocation_count = 0;
  track_allocations = true;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  context.packetContext(dispatcher.doubler, identity, 0, scope);
  dispatcher.doubler.started(p);
  bool retained = dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, 1000);
  dispatcher.doubler.setMode(0);
  dispatcher.doubler.setMode(1);
  context.radioApplied(dispatcher.doubler, 917.0f, 125.0f, 8, 5);
  auto expired = dispatcher.doubler.takeExpired(1001, nullptr);
  track_allocations = false;
  EXPECT_TRUE(retained);
  EXPECT_EQ(p, expired);
  EXPECT_EQ(0u, allocation_count);
  dispatcher.releasePacket(expired);
  EXPECT_EQ(32, manager.getFreeCount());
}


TEST_F(DoublerTest, ExplicitZeroPathRelayKeepsOriginAcrossRetentionAndRoleChanges) {
  dispatcher.zen_policy=true;
  for(uint8_t width:{uint8_t(1),uint8_t(2),uint8_t(3)}) {
    auto p=packet();p->setPathHashSizeAndCount(width,0);
    dispatcher.doubler.noteForwarded(p);
    dispatcher.forwarding_enabled=true;send(p);finish();
    ASSERT_EQ(1u,dispatcher.doubler.pendingCount());
    EXPECT_TRUE(dispatcher.doubler.forwarded(p));
    dispatcher.forwarding_enabled=false;tick();
    EXPECT_EQ(0u,dispatcher.doubler.pendingCount());
    EXPECT_EQ(32,manager.getFreeCount());
    auto local=packet();local->setPathHashSizeAndCount(width,0);
    EXPECT_FALSE(dispatcher.doubler.forwarded(local));
    send(local);finish();tick(500);finish();
    EXPECT_EQ(32,manager.getFreeCount());
  }
  EXPECT_EQ(9u,radio.frames.size());
}

TEST_F(DoublerTest, ZeroPathRelayCannotRepeatWhenForwardingDisabledDuringOriginal) {
  dispatcher.zen_policy=true;
  auto p=packet();p->path_len=0;dispatcher.doubler.noteForwarded(p);send(p);
  dispatcher.forwarding_enabled=false;finish();tick(500);
  EXPECT_EQ(1u,radio.frames.size());EXPECT_EQ(0u,dispatcher.doubler.pendingCount());
  EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, ForwardedStartFailureReleasesOriginBeforePacketReuse) {
  dispatcher.zen_policy=true;dispatcher.forwarding_enabled=false;
  auto p=packet();p->path_len=0;dispatcher.doubler.noteForwarded(p);
  radio.fail=true;dispatcher.sendPacket(p,1);tick();
  EXPECT_EQ(32,manager.getFreeCount());EXPECT_FALSE(dispatcher.doubler.forwarded(p));
  auto local=packet();local->path_len=0;send(local);finish();tick(500);finish();
  EXPECT_EQ(2u,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, OriginTrackerFitsWholeZenPoolAndOverflowFailsClosed) {
  Packet* packets[32];
  for(auto& p:packets) { p=packet();p->path_len=0;dispatcher.doubler.noteForwarded(p); }
  // Repeated observations must not consume slots.
  for(auto p:packets) { dispatcher.doubler.noteForwarded(p);EXPECT_TRUE(dispatcher.doubler.forwarded(p)); }
  for(auto p:packets)dispatcher.releasePacket(p);
  auto local=packet();local->path_len=0;send(local);finish();tick(500);finish();
  EXPECT_EQ(2u,radio.frames.size());
  for(auto& p:packets) { p=packet();p->path_len=0;dispatcher.doubler.noteForwarded(p); }
  Packet unexpected{};dispatcher.doubler.noteForwarded(&unexpected);
  for(auto p:packets)dispatcher.releasePacket(p);
  local=packet();local->path_len=0;send(local);finish();tick(500);
  EXPECT_EQ(3u,radio.frames.size());EXPECT_EQ(0u,dispatcher.doubler.pendingCount());
  EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, PacketContextRoundTripCannotReviveOriginalOrWaitingRepeat) {
  dispatcher.zen_policy=true;
  for(unsigned field:{0u,1u,2u})for(bool active:{false,true}) {
    auto before=radio.frames.size();auto p=packet();p->path_len=0;send(p);if(!active)finish();
    uint8_t* value=field==0?&dispatcher.identity[31]:field==1?&dispatcher.hash_mode:&dispatcher.scope[15];
    ++*value;dispatcher.sync();--*value;dispatcher.sync();
    if(active)finish();tick(500);
    EXPECT_EQ(before+1,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
  }
}

TEST_F(DoublerTest, UnchangedAppliedRadioAndDifferentPacketScopesPreserveCopies) {
  dispatcher.zen_policy=true;
  dispatcher.context.radioApplied(dispatcher.doubler,916.575f,62.5f,7,7);
  auto first=packet(ROUTE_TYPE_TRANSPORT_FLOOD);first->path_len=0;send(first);finish();
  // Reapplying unchanged hardware settings leaves both packet scopes valid.
  dispatcher.context.radioApplied(dispatcher.doubler,916.575f,62.5f,7,7);
  auto second=packet(ROUTE_TYPE_TRANSPORT_FLOOD);second->path_len=0;second->transport_codes[0]^=0xffff;
  send(second);finish();dispatcher.sync();
  tick(399);finish();tick();finish();
  ASSERT_EQ(4u,radio.frames.size());EXPECT_EQ(radio.frames[0],radio.frames[2]);
  EXPECT_EQ(radio.frames[1],radio.frames[3]);EXPECT_EQ(32,manager.getFreeCount());
}

TEST_F(DoublerTest, FailedDisableAndUnchangedSettingsLeaveWaitingCopyEligible) {
  struct Prefs { uint8_t multi_flood_mode=1; } prefs;
  dispatcher.zen_policy=true;auto p=packet();p->path_len=0;send(p);finish();
  unsigned writes=0;
  EXPECT_FALSE(zen::MultiFloodSetting::set(prefs,0,[&](){++writes;return false;}));
  dispatcher.doubler.setMode(prefs.multi_flood_mode);
  EXPECT_TRUE(zen::MultiFloodSetting::set(prefs,1,[&](){++writes;return true;}));
  dispatcher.sync();tick(500);finish();
  EXPECT_EQ(1u,writes);EXPECT_EQ(2u,radio.frames.size());EXPECT_EQ(32,manager.getFreeCount());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
