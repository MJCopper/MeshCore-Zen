#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/TransportTrace.h"

TEST(TransportTrace, KeepsOnlyMetadataInRamRing) {
  zen::TransportTrace trace;
  uint8_t key[4] = {1, 2, 3, 4};
  g_mock_millis = 42;
  trace.add(zen::TransportTrace::DM_QUEUED, key, 1);
  ASSERT_EQ(1, trace.size());
  const auto* entry = trace.newest(0);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(42u, entry->occurred_ms);
  EXPECT_EQ(zen::TransportTrace::DM_QUEUED, entry->event);
  EXPECT_EQ(1, entry->detail);
  EXPECT_EQ(0, memcmp(key, entry->key, sizeof(key)));
}

TEST(TransportTrace, WrapsWithoutAllocation) {
  zen::TransportTrace trace;
  for (uint8_t i = 0; i < zen::TransportTrace::CAPACITY + 3; i++) {
    g_mock_millis = i;
    trace.add(zen::TransportTrace::RESPONSE_RECEIVED, nullptr, i);
  }
  EXPECT_EQ(zen::TransportTrace::CAPACITY, trace.size());
  EXPECT_EQ(zen::TransportTrace::CAPACITY + 2, trace.newest(0)->detail);
  trace.clear();
  EXPECT_EQ(0, trace.size());
}

TEST(TransportTrace, DistinguishesRadioCompletionFromFailure) {
  zen::TransportTrace trace;
  trace.add(zen::TransportTrace::TX_COMPLETE, nullptr, 4);
  trace.add(zen::TransportTrace::TX_FAILED, nullptr, 5);
  EXPECT_STREQ("Radio TX ok", zen::TransportTrace::name(
      (zen::TransportTrace::Event)trace.newest(1)->event));
  EXPECT_STREQ("Radio TX fail", zen::TransportTrace::name(
      (zen::TransportTrace::Event)trace.newest(0)->event));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
