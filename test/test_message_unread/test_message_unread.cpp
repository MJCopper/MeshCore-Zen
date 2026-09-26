#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageUnreadCoordinator.h"

TEST(MessageUnreadCoordinator, RecordsClampsAndClearsConversationCounts) {
  zen::MessageUnreadCoordinator unread;
  const uint8_t key[4] = {1, 2, 3, 4};
  unread.record(zen::MessageUnreadCoordinator::DIRECT, key, false);
  unread.record(zen::MessageUnreadCoordinator::DIRECT, key, false);
  EXPECT_TRUE(unread.has(zen::MessageUnreadCoordinator::DIRECT, key));
  EXPECT_EQ(1, unread.get(zen::MessageUnreadCoordinator::DIRECT, key, 1));
  EXPECT_EQ(2, unread.get(zen::MessageUnreadCoordinator::DIRECT, key, 4));
  unread.clear(zen::MessageUnreadCoordinator::DIRECT, key);
  EXPECT_EQ(0, unread.get(zen::MessageUnreadCoordinator::DIRECT, key, 4));
}

TEST(MessageUnreadCoordinator, KeepsRoomAndDirectStateIndependent) {
  zen::MessageUnreadCoordinator unread;
  const uint8_t key[4] = {9, 8, 7, 6};
  unread.record(zen::MessageUnreadCoordinator::ROOM, key, false);
  unread.record(zen::MessageUnreadCoordinator::DIRECT, key, true);
  EXPECT_EQ(1, unread.get(zen::MessageUnreadCoordinator::ROOM, key, 1));
  EXPECT_FALSE(unread.has(zen::MessageUnreadCoordinator::DIRECT, key));
}

TEST(MessageUnreadCoordinator, ReclaimsReadSlotsAndDropsEvictedHistory) {
  zen::MessageUnreadCoordinator unread;
  uint8_t keys[17][4]{};
  for (uint8_t i = 0; i < 16; i++) {
    keys[i][0] = i + 1;
    unread.record(zen::MessageUnreadCoordinator::DIRECT, keys[i], false);
  }
  unread.clear(zen::MessageUnreadCoordinator::DIRECT, keys[0]);
  keys[16][0] = 99;
  unread.record(zen::MessageUnreadCoordinator::DIRECT, keys[16], false);
  EXPECT_FALSE(unread.has(zen::MessageUnreadCoordinator::DIRECT, keys[0]));
  EXPECT_TRUE(unread.has(zen::MessageUnreadCoordinator::DIRECT, keys[16]));

  unread.reconcile(zen::MessageUnreadCoordinator::DIRECT,
      [](const uint8_t*) { return 0; });
  EXPECT_FALSE(unread.has(zen::MessageUnreadCoordinator::DIRECT, keys[16]));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
