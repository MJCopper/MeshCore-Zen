#include <gtest/gtest.h>
#include <cstring>

#include "../../examples/companion_radio/solo/ChannelSlotPolicy.h"

struct TestChannel {
  struct { uint8_t secret[32]; } channel;
  char name[32];
};

struct TestChannels {
  TestChannel slots[3] = {};
  bool getChannel(int idx, TestChannel& out) {
    if (idx < 0 || idx >= 3) return false;
    out = slots[idx];
    return true;
  }
};

TEST(ChannelSlots, BlankNameDoesNotMakeAnOccupiedSlotFree) {
  TestChannels channels;
  channels.slots[0].channel.secret[0] = 1;
  EXPECT_EQ(1, solo::ChannelSlotPolicy::firstFree<TestChannel>(channels, 3));
  channels.slots[1].channel.secret[0] = 2;
  channels.slots[2].channel.secret[0] = 3;
  EXPECT_EQ(-1, solo::ChannelSlotPolicy::firstFree<TestChannel>(channels, 3));
}

TEST(ChannelSlots, DuplicateUsesFullKeyNotNameAndExcludesEditedSlot) {
  TestChannels channels;
  channels.slots[0].channel.secret[0] = 1;
  std::strcpy(channels.slots[0].name, "Public");
  channels.slots[1].channel.secret[0] = 2;
  std::strcpy(channels.slots[1].name, "Public");
  EXPECT_EQ(0, solo::ChannelSlotPolicy::duplicate<TestChannel>(
      channels, 3, 2, channels.slots[0].channel.secret));
  EXPECT_EQ(-1, solo::ChannelSlotPolicy::duplicate<TestChannel>(
      channels, 3, 0, channels.slots[0].channel.secret));
  EXPECT_EQ(-1, solo::ChannelSlotPolicy::duplicate<TestChannel>(
      channels, 3, 1, channels.slots[1].channel.secret));
  uint8_t longer_key[32] = {};
  longer_key[0] = 1;
  longer_key[16] = 1;
  EXPECT_EQ(-1, solo::ChannelSlotPolicy::duplicate<TestChannel>(channels, 3, 2, longer_key));
  uint8_t empty[32] = {};
  EXPECT_EQ(-1, solo::ChannelSlotPolicy::duplicate<TestChannel>(channels, 3, 2, empty));
}

TEST(ChannelSlots, FailedSaveRestoresPreviousRamValue) {
  TestChannels channels;
  channels.slots[0].channel.secret[0] = 1;
  TestChannel previous = channels.slots[0];
  TestChannel next = previous;
  next.channel.secret[0] = 2;
  int writes = 0;
  bool saved = solo::ChannelSlotPolicy::saveWithRollback(0, next, previous,
      [&channels, &writes](int idx, const TestChannel& value) {
        channels.slots[idx] = value;
        writes++;
        return true;
      }, []() { return false; });
  EXPECT_FALSE(saved);
  EXPECT_EQ(2, writes);
  EXPECT_EQ(1, channels.slots[0].channel.secret[0]);
  EXPECT_TRUE(solo::ChannelSlotPolicy::saveWithRollback(0, next, previous,
      [&channels](int idx, const TestChannel& value) {
        channels.slots[idx] = value;
        return true;
      }, []() { return true; }));
  EXPECT_EQ(2, channels.slots[0].channel.secret[0]);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
