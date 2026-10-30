#include <gtest/gtest.h>
#include <helpers/FloodTextDoubler.h>
#include <helpers/DoublerContext.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <array>
#include <vector>
#include <new>
#include <cstdlib>

static bool track_policy_allocations = false;
static unsigned policy_allocations = 0;
void* operator new(std::size_t n) {
  if (track_policy_allocations) policy_allocations++;
  void* p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

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
  unsigned invalidateAtAirtimeCheck = 0;
  FloodTextDoubler* policyToInvalidate = nullptr;
  uint32_t profile = 1;
  std::vector<uint32_t> txProfiles;
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
  uint32_t getEstAirtimeFor(int) override {
    if (++airtimeChecks == advanceOnAirtimeCheck && clock) clock->now += advanceMs;
    if (airtimeChecks == invalidateAtAirtimeCheck && policyToInvalidate) policyToInvalidate->invalidateContext();
    return 100;
  }
  float packetScore(float, int) override { return 1; }
  bool startSendRaw(const uint8_t* bytes, int len) override {
    if (fail) { fail = false; return false; }
    EXPECT_FALSE(transmitting);
    frames.emplace_back(bytes, bytes + len);
    txProfiles.push_back(profile);
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
  unsigned received = 0;
  unsigned logDelay = 0;
  float budgetFactor = 1;
  unsigned long budgetWindow = 3600000;

  TestDispatcher(FakeRadio& radio, FakeClock& clock, PacketManager& manager, bool local_only = false)
    : Dispatcher(radio, clock, manager), doubler(local_only) {}

protected:
  void onTxStarted(Packet* packet, uint32_t) override { if (enabled) doubler.onTransmitStarted(packet); }
  bool retainAfterTransmit(Packet* packet, uint32_t completed_at) override {
    return enabled && doubler.onTransmitComplete(packet, *_mgr, rng, completed_at);
  }
  void onPacketReleased(Packet* packet) override { doubler.forget(packet); }
  void serviceOptionalPackets(uint32_t now, const Packet* active) override {
    if (enabled) while (auto packet = doubler.takeExpired(now, active)) releasePacket(packet);
  }
  Packet* getOptionalPacket(uint32_t now) override { return enabled ? doubler.nextReady(now) : nullptr; }
  void deferOptionalPackets(uint32_t now) override { if (enabled) doubler.deferReady(now); }
  bool allowPacketStart(Packet* packet, uint32_t) override {
    if (!enabled || !doubler.isRepeat(packet)) return true;
    if (_radio->isReceiving()) return false;
    return doubler.canStart(packet, (uint32_t)_ms->getMillis());
  }
  void logTx(Packet*, int) override { static_cast<FakeClock*>(_ms)->now += logDelay; }
  float getAirtimeBudgetFactor() const override { return budgetFactor; }
  unsigned long getDutyCycleWindowMs() const override { return budgetWindow; }
  int calcRxDelay(float, uint32_t) const override { return 0; }
  DispatcherAction onRecvPacket(Packet* packet) override {
    received++;
    // Use the production duplicate table; routing itself is not under test here.
    if (!forward || seen.wasSeen(packet)) return ACTION_RELEASE;
    seen.markSeen(packet);
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
  dispatcher.doubler.onTransmitStarted(p);
  ASSERT_TRUE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, now));
  EXPECT_EQ(nullptr, dispatcher.doubler.nextReady(now + 499));
  EXPECT_EQ(p, dispatcher.doubler.nextReady(now + 500));
  dispatcher.doubler.onTransmitStarted(p);
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
  dispatcher.doubler.onTransmitStarted(p);
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


// Companion uses the production dispatcher and local-only policy with its real
// 16-packet pool size. Packet factories model Mesh's outgoing wire path states.
class CompanionDoublerTest : public ::testing::Test {
protected:
  FakeClock clock;
  FakeRadio radio;
  StaticPoolPacketManager manager{16};
  TestDispatcher dispatcher{radio, clock, manager, true};
  void SetUp() override { radio.clock = &clock; dispatcher.begin(); }
  void tick(unsigned ms = 1) { clock.now += ms; dispatcher.loop(); }
  void finish() { radio.complete = true; tick(100); }
  Packet* packet(uint8_t route = ROUTE_TYPE_FLOOD, uint8_t type = PAYLOAD_TYPE_GRP_TXT,
                 uint8_t hops = 0, uint8_t hashSize = 1) {
    auto p = dispatcher.obtainNewPacket();
    EXPECT_NE(nullptr, p);
    p->header = route | (type << PH_TYPE_SHIFT);
    p->setPathHashSizeAndCount(hashSize, hops);
    memset(p->path, 0x42, sizeof(p->path));
    p->transport_codes[0] = 0x1234;
    p->transport_codes[1] = 0x5678;
    p->payload_len = 19;
    memset(p->payload, 0x35, p->payload_len);
    return p;
  }
  void send(Packet* p) {
    dispatcher.sendPacket(p, 1);
    tick();
    EXPECT_TRUE(radio.transmitting);
  }
};

TEST_F(CompanionDoublerTest, DefaultOffStillSendsOriginalLocalFlood) {
  EXPECT_EQ(0, dispatcher.doubler.getMode());
  send(packet()); finish(); tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(CompanionDoublerTest, EveryModeRoutePayloadAndOriginHasExpectedCount) {
  for (uint8_t mode = 0; mode <= 2; mode++) {
    dispatcher.doubler.setMode(mode);
    for (uint8_t route = 0; route < 4; route++) {
      for (uint8_t type = 0; type < 16; type++) {
        for (uint8_t hops = 0; hops <= 1; hops++) {
          auto before = radio.frames.size();
          bool flood = route == ROUTE_TYPE_FLOOD || route == ROUTE_TYPE_TRANSPORT_FLOOD;
          bool repeat = hops == 0 && flood && (mode == 2 || (mode == 1 && type == PAYLOAD_TYPE_GRP_TXT));
          send(packet(route, type, hops)); finish(); tick(500);
          EXPECT_EQ(before + (repeat ? 2 : 1), radio.frames.size())
              << +mode << ":" << +route << ":" << +type << ":" << +hops;
          if (repeat) {
            EXPECT_EQ(radio.frames[before], radio.frames[before + 1]);
            finish();
          }
          EXPECT_EQ(16, manager.getFreeCount());
        }
      }
    }
  }
}

TEST_F(CompanionDoublerTest, ScopedLocalFloodsWorkWithAllPathHashSizes) {
  dispatcher.doubler.setMode(1);
  for (uint8_t hashSize = 1; hashSize <= 3; hashSize++) {
    auto before = radio.frames.size();
    send(packet(ROUTE_TYPE_TRANSPORT_FLOOD, PAYLOAD_TYPE_GRP_TXT, 0, hashSize));
    finish(); tick(500); finish();
    ASSERT_EQ(before + 2, radio.frames.size());
    EXPECT_EQ(radio.frames[before], radio.frames[before + 1]);
    send(packet(ROUTE_TYPE_TRANSPORT_FLOOD, PAYLOAD_TYPE_GRP_TXT, 1, hashSize));
    finish(); tick(500);
    EXPECT_EQ(before + 3, radio.frames.size());  // relayed flood gets no extra TX
  }
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(CompanionDoublerTest, BurstRetainsAtMostFourCopiesInSixteenPacketPool) {
  dispatcher.doubler.setMode(2);
  for (unsigned i = 0; i < 5; i++) { send(packet()); finish(); }
  EXPECT_EQ(4u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(12, manager.getFreeCount());
  for (unsigned i = 0; i < 4; i++) { tick(150); finish(); }
  EXPECT_EQ(9u, radio.frames.size());
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(CompanionDoublerTest, LowPoolCapacitySkipsCopyWithoutBlockingOriginal) {
  dispatcher.doubler.setMode(1);
  send(packet());
  std::vector<Packet*> held;
  while (manager.getFreeCount() > 3) held.push_back(manager.allocNew());
  finish(); tick(1000);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  for (auto p : held) dispatcher.releasePacket(p);
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(CompanionDoublerTest, NormalTrafficWinsAndBusyLocalRepeatExpires) {
  dispatcher.doubler.setMode(1);
  send(packet()); finish();
  auto original = radio.frames.front();
  dispatcher.sendPacket(packet(ROUTE_TYPE_DIRECT, PAYLOAD_TYPE_ACK), 1);
  tick(500);
  ASSERT_EQ(2u, radio.frames.size());
  EXPECT_NE(original, radio.frames.back());
  finish();
  radio.busy = true;
  tick(2400);  // repeat reaches its original three-second deadline
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(16, manager.getFreeCount());
  radio.busy = false;
  tick();
  EXPECT_EQ(2u, radio.frames.size());
}

TEST_F(CompanionDoublerTest, DistinctApplicationAttemptsEachHaveAtMostOneExtraTx) {
  dispatcher.doubler.setMode(2);
  for (unsigned attempt = 0; attempt < 3; attempt++) {
    auto p = packet(ROUTE_TYPE_FLOOD, PAYLOAD_TYPE_TXT_MSG);
    p->payload[0] = attempt;
    send(p); finish(); tick(500); finish(); tick(500);
    ASSERT_EQ((attempt + 1) * 2, radio.frames.size());
    EXPECT_EQ(radio.frames[attempt * 2], radio.frames[attempt * 2 + 1]);
  }
  EXPECT_EQ(16, manager.getFreeCount());
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
  DoublerContext context;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  send(packet()); finish();
  context.radioApplied(dispatcher.doubler, 917.0f, 62.5f, 7, 7);
  radio.profile = 2;
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  send(packet()); finish(); tick(500); finish();
  ASSERT_EQ(3u, radio.frames.size());
  EXPECT_EQ((std::vector<uint32_t>{1, 2, 2}), radio.txProfiles);
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ContextChangeDuringOriginalRejectsItsCompletionCopy) {
  DoublerContext context;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  send(packet());
  context.radioApplied(dispatcher.doubler, 917.0f, 62.5f, 7, 7);
  radio.profile = 2;
  finish(); tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  send(packet()); finish(); tick(500); finish();
  EXPECT_EQ(3u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, RadioRoundTripCannotReviveWaitingOrActiveOriginal) {
  DoublerContext context;
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
  dispatcher.doubler.invalidateContext();
  EXPECT_EQ(nullptr, dispatcher.doubler.takeExpired(clock.now + 5000, nullptr));
  EXPECT_EQ(nullptr, dispatcher.doubler.nextReady(clock.now));
  EXPECT_EQ(31, manager.getFreeCount());
  finish(); tick(1000);
  EXPECT_EQ(2u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, AvailabilityOffOnPermanentlyCancelsOldCopies) {
  send(packet()); finish();
  dispatcher.doubler.setAvailable(false);
  dispatcher.doubler.setAvailable(true);
  tick(500);
  EXPECT_EQ(1u, radio.frames.size());
  EXPECT_EQ(32, manager.getFreeCount());
  send(packet());
  dispatcher.doubler.setAvailable(false);
  dispatcher.doubler.setAvailable(true);
  finish(); tick(500);
  EXPECT_EQ(2u, radio.frames.size());
  send(packet()); finish(); tick(500); finish();
  EXPECT_EQ(4u, radio.frames.size());
}

TEST_F(DoublerTest, IdentityHashModeAndDefaultScopeChangesCancelCopies) {
  for (unsigned change = 0; change < 3; change++) {
    DoublerContext context;
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
    DoublerContext context;
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
  DoublerContext context;
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
  DoublerContext context;
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
  dispatcher.doubler.onTransmitStarted(p);
  dispatcher.doubler.invalidateContext();
  dispatcher.releasePacket(p);
  EXPECT_FALSE(dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, clock.now));
  EXPECT_EQ(0u, dispatcher.doubler.pendingCount());
  EXPECT_EQ(32, manager.getFreeCount());
}

TEST_F(DoublerTest, ContextAndCancellationPolicyNeverAllocate) {
  auto p = packet();
  DoublerContext context;
  uint8_t identity[PUB_KEY_SIZE] = {}, scope[16] = {};
  policy_allocations = 0;
  track_policy_allocations = true;
  context.radioApplied(dispatcher.doubler, 916.575f, 62.5f, 7, 7);
  context.packetContext(dispatcher.doubler, identity, 0, scope);
  dispatcher.doubler.onTransmitStarted(p);
  bool retained = dispatcher.doubler.onTransmitComplete(p, manager, dispatcher.rng, 1000);
  dispatcher.doubler.setMode(0);
  dispatcher.doubler.setMode(1);
  context.radioApplied(dispatcher.doubler, 917.0f, 125.0f, 8, 5);
  auto expired = dispatcher.doubler.takeExpired(1001, nullptr);
  track_policy_allocations = false;
  EXPECT_TRUE(retained);
  EXPECT_EQ(p, expired);
  EXPECT_EQ(0u, policy_allocations);
  dispatcher.releasePacket(expired);
  EXPECT_EQ(32, manager.getFreeCount());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
