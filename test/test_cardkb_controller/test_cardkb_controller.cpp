#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/src/helpers/ui/CardKBController.h"

TEST(CardKB, SuspendsBusTrafficAndDiscardsQueuedWakeKey) {
  g_mock_millis = 0;
  TwoWire wire;
  CardKBController keyboard;
  CardKBController::Event event;
  keyboard.begin(wire);
  EXPECT_TRUE(keyboard.isActive());
  keyboard.suspend();
  EXPECT_FALSE(keyboard.isActive());
  wire.next = 'x';
  EXPECT_FALSE(keyboard.poll(event));
  EXPECT_EQ(0u, wire.reads);
  keyboard.resume();
  EXPECT_TRUE(keyboard.isActive());
  EXPECT_FALSE(keyboard.poll(event));
  g_mock_millis += 30;
  EXPECT_FALSE(keyboard.poll(event));
  wire.next = 'a';
  g_mock_millis += 30;
  ASSERT_TRUE(keyboard.poll(event));
  EXPECT_EQ('a', event.key);
}

TEST(CardKB, StopsPollingAfterThreeFailures) {
  g_mock_millis = 0;
  TwoWire wire;
  CardKBController keyboard;
  CardKBController::Event event;
  keyboard.begin(wire);
  wire.connected = false;
  for (int i = 0; i < 10; i++) {
    g_mock_millis += 30;
    EXPECT_FALSE(keyboard.poll(event));
  }
  EXPECT_EQ(3u, wire.reads);
  EXPECT_FALSE(keyboard.isPresent());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
