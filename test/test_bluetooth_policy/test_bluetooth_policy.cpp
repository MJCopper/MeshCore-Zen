#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/BluetoothPolicyAdapter.h"

using zen::BluetoothPolicyAdapter;

TEST(BluetoothPolicy, ActsOnlyWhenStateDiffers) {
  BluetoothPolicyAdapter policy;
  EXPECT_EQ(BluetoothPolicyAdapter::NO_ACTION, policy.action(false, false));
  EXPECT_EQ(BluetoothPolicyAdapter::NO_ACTION, policy.action(true, true));
  EXPECT_EQ(BluetoothPolicyAdapter::ENABLE, policy.action(true, false));
  EXPECT_EQ(BluetoothPolicyAdapter::DISABLE, policy.action(false, true));
}

TEST(BluetoothPolicy, ReportsConnectionEdges) {
  BluetoothPolicyAdapter policy;
  EXPECT_EQ(BluetoothPolicyAdapter::NO_EVENT, policy.observe(true, false));
  EXPECT_EQ(BluetoothPolicyAdapter::CONNECTED, policy.observe(true, true));
  EXPECT_EQ(BluetoothPolicyAdapter::NO_EVENT, policy.observe(true, true));
  EXPECT_EQ(BluetoothPolicyAdapter::DISCONNECTED, policy.observe(true, false));
}

TEST(BluetoothPolicy, DisabledInterfaceCannotRemainConnected) {
  BluetoothPolicyAdapter policy;
  EXPECT_EQ(BluetoothPolicyAdapter::NO_EVENT, policy.observe(true, true));
  EXPECT_EQ(BluetoothPolicyAdapter::DISCONNECTED, policy.observe(false, true));
  EXPECT_EQ(BluetoothPolicyAdapter::NO_EVENT, policy.observe(false, false));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
