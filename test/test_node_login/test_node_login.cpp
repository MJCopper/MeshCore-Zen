#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/NodeLoginCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NodeLoginResponse.h"

TEST(NodeLogin, RejectsTruncatedAndUnrelatedResponses) {
  uint8_t data[64]{};
  for (uint8_t len = 0; len < 6; len++)
    EXPECT_FALSE(zen::NodeLoginResponse::valid(data, len));
  EXPECT_TRUE(zen::NodeLoginResponse::valid(data, 13));
  EXPECT_TRUE(zen::NodeLoginResponse::valid(data, 14));
  EXPECT_TRUE(zen::NodeLoginResponse::valid(data, sizeof(data)));
  data[4] = 'O'; data[5] = 'K';
  EXPECT_TRUE(zen::NodeLoginResponse::valid(data, 6));
  data[4] = 42;
  EXPECT_FALSE(zen::NodeLoginResponse::valid(data, 13));
}

TEST(NodeLogin, AppReservationExpiresAcrossMillisRollover) {
  EXPECT_TRUE(zen::NodeLoginResponse::busy(1, 20, UINT32_MAX - 10));
  EXPECT_TRUE(zen::NodeLoginResponse::busy(1, 20, 19));
  EXPECT_FALSE(zen::NodeLoginResponse::busy(1, 20, 20));
  EXPECT_FALSE(zen::NodeLoginResponse::busy(0, 20, 19));
}

TEST(NodeLogin, LegacyPasswordSuccessGrantsAdminButBlankAclDoesNot) {
  EXPECT_TRUE(zen::NodeLoginResponse::grantsAdmin(true, 3, false));
  EXPECT_TRUE(zen::NodeLoginResponse::grantsAdmin(true, 0, true));
  EXPECT_FALSE(zen::NodeLoginResponse::grantsAdmin(true, 0, false));
  EXPECT_FALSE(zen::NodeLoginResponse::grantsAdmin(false, 3, true));
}

TEST(NodeLogin, ExplicitAdminRoleOverridesIncompleteAclRoleBits) {
  EXPECT_EQ(zen::NodeLoginResponse::effectivePermissions(1, 0), 3);
  EXPECT_EQ(zen::NodeLoginResponse::effectivePermissions(1, 0xA2), 0xA3);
  EXPECT_EQ(zen::NodeLoginResponse::effectivePermissions(0, 3), 3);
  EXPECT_EQ(zen::NodeLoginResponse::effectivePermissions(2, 2), 2);
}

TEST(NodeLogin, KeepsOwnerAndRejectsOtherContacts) {
  zen::NodeLoginCoordinator login;
  uint8_t first[32] = {1}, second[32] = {2};
  zen::NodeLoginCoordinator::Attempt result;
  ASSERT_TRUE(login.begin(login.MESSAGES, first, "secret", true));
  EXPECT_FALSE(login.begin(login.MESSAGES, second, "", false));
  EXPECT_FALSE(login.complete(second, result));
  ASSERT_TRUE(login.take(result));
  EXPECT_STREQ("secret", result.password);
  EXPECT_FALSE(login.complete(first, result));
  ASSERT_TRUE(login.begin(login.MESSAGES, second, "", false));
  EXPECT_FALSE(login.cancel(login.MESSAGES, first));
  EXPECT_TRUE(login.complete(second, result));
}

TEST(NodeLogin, TracksSensorCarouselAsAnIndependentOwner) {
  zen::NodeLoginCoordinator login;
  uint8_t sensor[32] = {4, 3, 2, 1};
  zen::NodeLoginCoordinator::Attempt result;
  ASSERT_TRUE(login.begin(login.SENSOR, sensor, "", false));
  EXPECT_TRUE(login.ownedBy(login.SENSOR));
  ASSERT_TRUE(login.take(result));
  EXPECT_EQ(result.owner, login.SENSOR);
  EXPECT_STREQ(result.password, "");
}

TEST(NodeLogin, RestartPreservesOwnerAndCredential) {
  zen::NodeLoginCoordinator login;
  uint8_t key[32] = {9, 8, 7, 6};
  zen::NodeLoginCoordinator::Attempt attempt;
  ASSERT_TRUE(login.begin(login.ADMIN, key, "pw", false));
  ASSERT_TRUE(login.take(attempt));
  ASSERT_TRUE(login.restart(attempt));
  ASSERT_TRUE(login.take(attempt));
  EXPECT_STREQ(attempt.password, "pw");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
