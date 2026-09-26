#include <gtest/gtest.h>
#include <cstring>
#include <helpers/AdvertDataHelpers.h>

#define ZEN_FEATURE_CHILD_MODE 1
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenPolicy.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/PinAttemptLimiter.h"

// The native test environment does not link src/Identity.cpp; this policy test
// only needs a zeroed contact identity.
namespace mesh {
Identity::Identity() { std::memset(pub_key, 0, sizeof(pub_key)); }
}

TEST(ZenPolicy, ChildConfigurationFailsClosedWithoutParentSession) {
  ZenPrefs prefs;
  prefs.child_mode_enabled = 1;

  EXPECT_TRUE(zen::Policy::childLocked(&prefs, false));
  EXPECT_FALSE(zen::Policy::childLocked(&prefs, true));
}

TEST(ZenPolicy, AllowsOnlyFavouriteContactOfExpectedTypeWhileLocked) {
  ZenPrefs prefs;
  ContactInfo contact;
  contact.type = ADV_TYPE_CHAT;
  contact.flags = 0;

  EXPECT_FALSE(zen::Policy::contactAllowed(&prefs, true, &contact, ADV_TYPE_CHAT));
  contact.flags = 0x01;
  EXPECT_TRUE(zen::Policy::contactAllowed(&prefs, true, &contact, ADV_TYPE_CHAT));
  EXPECT_FALSE(zen::Policy::contactAllowed(&prefs, true, &contact, ADV_TYPE_ROOM));
}

TEST(ZenPolicy, RoomsRequireBothFavouriteAndParentPermission) {
  ZenPrefs prefs;
  ContactInfo room;
  std::memset(&prefs, 0, sizeof(prefs));
  room.type = ADV_TYPE_ROOM;
  room.flags = 0x01;
  EXPECT_FALSE(zen::Policy::contactAllowed(&prefs, true, &room, ADV_TYPE_ROOM));
  prefs.child_rooms_enabled = 1;
  EXPECT_TRUE(zen::Policy::contactAllowed(&prefs, true, &room, ADV_TYPE_ROOM));
  room.flags = 0;
  EXPECT_FALSE(zen::Policy::contactAllowed(&prefs, true, &room, ADV_TYPE_ROOM));
}

TEST(ZenPolicy, MatchesStoredAndReportedContactTypes) {
  EXPECT_TRUE(zen::Policy::contactIdentityMatches(ADV_TYPE_CHAT, ADV_TYPE_CHAT,
                                                   ADV_TYPE_CHAT));
  EXPECT_FALSE(zen::Policy::contactIdentityMatches(ADV_TYPE_ROOM, ADV_TYPE_CHAT,
                                                    ADV_TYPE_CHAT));
  EXPECT_FALSE(zen::Policy::contactIdentityMatches(ADV_TYPE_CHAT, ADV_TYPE_ROOM,
                                                    ADV_TYPE_CHAT));
}

TEST(ZenPolicy, BoundsFavouriteChannelMask) {
  ZenPrefs prefs;
  std::memset(&prefs, 0, sizeof(prefs));
  prefs.child_channels_enabled = 1;
  prefs.ch_fav_bitmask = (1ULL << 0) | (1ULL << 63);
  uint8_t private_secret[16] = {1};

  EXPECT_TRUE(zen::Policy::channelAllowed(&prefs, true, 0, "Family", private_secret));
  EXPECT_TRUE(zen::Policy::channelAllowed(&prefs, true, 63, "Family", private_secret));
  EXPECT_FALSE(zen::Policy::channelAllowed(&prefs, true, 1, "Family", private_secret));
  EXPECT_FALSE(zen::Policy::channelAllowed(&prefs, true, 64, "Family", private_secret));
}

TEST(ZenPolicy, ParentPinBackoffStartsAfterThreeFailures) {
  zen::PinAttemptLimiter limiter;
  EXPECT_EQ(limiter.failed(1000), 0u);
  EXPECT_EQ(limiter.failed(1000), 0u);
  EXPECT_EQ(limiter.failed(1000), 0u);
  EXPECT_EQ(limiter.failed(1000), 5u);
  EXPECT_EQ(limiter.remainingSeconds(1000), 5u);
  EXPECT_EQ(limiter.failed(6000), 15u);
  EXPECT_EQ(limiter.failed(21000), 30u);
  limiter.reset();
  EXPECT_EQ(limiter.remainingSeconds(21000), 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
