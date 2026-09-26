#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/AdminSession.h"

TEST(AdminSession, RequiresAuthorizationAndFullIdentity) {
  zen::AdminSession session;
  uint8_t key[32] = {1};
  uint8_t other[32] = {1}; other[31] = 2;
  EXPECT_FALSE(session.ready(key, 0));
  session.authorize(key);
  EXPECT_TRUE(session.ready(key, 0));
  EXPECT_FALSE(session.ready(other, 0));
  session.close(0);
  EXPECT_FALSE(session.ready(key, 0));
}

TEST(AdminSession, MatchesTaggedReplyAndRejectsStaleAndAppReplies) {
  zen::AdminSession session;
  uint8_t key[32] = {1};
  uint8_t other[32] = {1}; other[31] = 2;
  session.authorize(key);
  char request[161];
  ASSERT_TRUE(session.formatRequest(request, sizeof(request), "get name"));
  EXPECT_STREQ(request, "AA|get name");
  session.begin(0, 1000);
  EXPECT_FALSE(session.ready(key, 1));
  EXPECT_FALSE(session.complete(key, "> unrelated", 1));
  EXPECT_FALSE(session.complete(other, "AA|> name", 1));
  EXPECT_FALSE(session.complete(key, "AB|> name", 1));
  EXPECT_TRUE(session.complete(key, "AA|> name", 1));
  ASSERT_TRUE(session.formatRequest(request, sizeof(request), "get tx"));
  session.begin(2, 1000);
  EXPECT_FALSE(session.complete(key, "AA|> name", 3));
  EXPECT_TRUE(session.complete(key, "AB|> 22", 3));
}

TEST(AdminSession, LocalCancellationRemainsUsableButAppOverlapQuarantines) {
  zen::AdminSession session;
  uint8_t key[32] = {1};
  session.authorize(key);
  session.begin(0, 1000);
  session.cancel(10);
  EXPECT_TRUE(session.ready(key, 11));
  session.begin(11, 1000);
  session.appCommand(20, 2000);
  EXPECT_FALSE(session.pending());
  EXPECT_FALSE(session.ready(key, 62019));
  EXPECT_TRUE(session.ready(key, 62020));
}

TEST(AdminSession, ExpiryAndDrainSurviveMillisRollover) {
  zen::AdminSession session;
  uint8_t key[32] = {1};
  char request[161];
  session.authorize(key);
  session.formatRequest(request, sizeof(request), "reboot");
  session.begin(0xFFFFFF00, 512);
  EXPECT_FALSE(session.expired(0));
  EXPECT_TRUE(session.expired(256));
  EXPECT_FALSE(session.complete(key, "AA|OK", 256));
  session.cancel(256);
  EXPECT_TRUE(session.ready(key, 257));
}

TEST(AdminSession, BoundsWireTextAndNeverReusesRequestIds) {
  zen::AdminSession session;
  char request[161];
  char text[159]; memset(text, 'a', 158); text[158] = 0;
  EXPECT_FALSE(session.formatRequest(request, sizeof(request), text));
  text[157] = 0;
  ASSERT_TRUE(session.formatRequest(request, sizeof(request), text));
  EXPECT_EQ(strlen(request), 160u);
  for (int i = 1; i < 4096; i++) ASSERT_TRUE(session.formatRequest(request, sizeof(request), "ver"));
  EXPECT_FALSE(session.formatRequest(request, sizeof(request), "ver"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
