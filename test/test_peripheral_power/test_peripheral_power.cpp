#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/PeripheralPowerCoordinator.h"

using zen::PeripheralPowerCoordinator;

TEST(PeripheralPower, ResolvesNormalRequests) {
  PeripheralPowerCoordinator c;
  auto& r = c.requests();
  r.display_on = true;
  r.cardkb_present = true;
  r.gps_saved_on = true;
  r.bluetooth_saved_on = true;
  r.brightness = 4;
  auto e = c.resolve();
  EXPECT_TRUE(e.display_on);
  EXPECT_TRUE(e.cardkb_polling);
  EXPECT_TRUE(e.gps_policy_on);
  EXPECT_FALSE(e.gps_force_on);
  EXPECT_TRUE(e.bluetooth_on);
  EXPECT_TRUE(e.radio_on);
  EXPECT_EQ(4, e.brightness);
}

TEST(PeripheralPower, LowPowerAndEmergencyResolveWithoutChangingIntent) {
  PeripheralPowerCoordinator c;
  auto& r = c.requests();
  r.display_on = true;
  r.cardkb_present = true;
  r.gps_saved_on = true;
  r.gps_time_sync = true;
  r.bluetooth_saved_on = true;
  r.brightness = 4;
  c.restrictions().low_power = true;
  auto e = c.resolve();
  EXPECT_TRUE(e.display_on);
  EXPECT_TRUE(e.cardkb_polling);
  EXPECT_EQ(0, e.brightness);
  EXPECT_FALSE(e.gps_policy_on);
  EXPECT_FALSE(e.gps_force_on);
  EXPECT_FALSE(e.bluetooth_on);
  EXPECT_FALSE(e.radio_on);

  c.restrictions().emergency = true;
  r.emergency_gps = true;
  r.bluetooth_session = 1;
  e = c.resolve();
  EXPECT_FALSE(e.gps_policy_on);
  EXPECT_TRUE(e.gps_force_on);
  EXPECT_TRUE(e.bluetooth_on);
  EXPECT_TRUE(e.radio_on);
  EXPECT_TRUE(r.gps_saved_on);
  EXPECT_TRUE(r.bluetooth_saved_on);
}

TEST(PeripheralPower, ChildModeAlwaysBlocksBluetooth) {
  PeripheralPowerCoordinator c;
  c.requests().bluetooth_saved_on = true;
  c.requests().bluetooth_session = 1;
  c.restrictions().low_power = true;
  c.restrictions().emergency = true;
  c.restrictions().child_locked = true;
  EXPECT_FALSE(c.resolve().bluetooth_on);
}

TEST(PeripheralPower, SessionAndTimeSyncClaimsRemainIndependent) {
  PeripheralPowerCoordinator c;
  c.requests().gps_saved_on = true;
  c.requests().gps_session = 0;
  c.requests().gps_time_sync = true;
  c.requests().bluetooth_saved_on = true;
  c.requests().bluetooth_session = 0;
  auto e = c.resolve();
  EXPECT_FALSE(e.gps_policy_on);
  EXPECT_TRUE(e.gps_force_on);
  EXPECT_FALSE(e.bluetooth_on);
}

TEST(PeripheralPower, ReconciliationIsIdempotent) {
  PeripheralPowerCoordinator c;
  c.requests().display_on = true;
  auto t = c.transition();
  EXPECT_EQ(PeripheralPowerCoordinator::CHANGE_ALL, t.changes);
  c.markApplied(t.target);
  EXPECT_EQ(0, c.transition().changes);
  c.requests().display_on = false;
  EXPECT_EQ(PeripheralPowerCoordinator::CHANGE_DISPLAY, c.transition().changes);
}

TEST(PeripheralPower, ShutdownOverridesEveryRequest) {
  PeripheralPowerCoordinator c;
  c.requests().display_on = true;
  c.requests().gps_saved_on = true;
  c.requests().bluetooth_saved_on = true;
  c.restrictions().emergency = true;
  c.restrictions().low_power = true;
  c.restrictions().shutting_down = true;
  auto e = c.resolve();
  EXPECT_FALSE(e.display_on);
  EXPECT_FALSE(e.cardkb_polling);
  EXPECT_FALSE(e.gps_policy_on);
  EXPECT_FALSE(e.gps_force_on);
  EXPECT_FALSE(e.bluetooth_on);
  EXPECT_FALSE(e.radio_on);
}

TEST(PeripheralPower, ChildRelockAndUnlockRestoreSavedBluetooth) {
  PeripheralPowerCoordinator c;
  c.requests().bluetooth_saved_on = true;
  c.markApplied(c.resolve());

  c.restrictions().child_locked = true;
  auto locked = c.transition();
  EXPECT_FALSE(locked.target.bluetooth_on);
  EXPECT_EQ(PeripheralPowerCoordinator::CHANGE_BLUETOOTH, locked.changes);
  c.markApplied(locked.target);

  c.restrictions().child_locked = false;
  auto restored = c.transition();
  EXPECT_TRUE(restored.target.bluetooth_on);
  EXPECT_EQ(PeripheralPowerCoordinator::CHANGE_BLUETOOTH, restored.changes);
}

TEST(PeripheralPower, EmergencyExpiryReturnsEveryPeripheralToLowPower) {
  PeripheralPowerCoordinator c;
  c.restrictions().low_power = true;
  c.restrictions().emergency = true;
  c.requests().emergency_gps = true;
  c.requests().bluetooth_session = 1;
  c.markApplied(c.resolve());

  c.restrictions().emergency = false;
  c.requests().emergency_gps = false;
  c.requests().bluetooth_session = 0;
  auto expired = c.transition();
  EXPECT_FALSE(expired.target.gps_force_on);
  EXPECT_FALSE(expired.target.bluetooth_on);
  EXPECT_FALSE(expired.target.radio_on);
  EXPECT_NE(0, expired.changes & PeripheralPowerCoordinator::CHANGE_POWER_CONTEXT);
}

TEST(PeripheralPower, FailedAppliedStateRemainsPendingUntilAStateEventRetriesIt) {
  PeripheralPowerCoordinator c;
  c.requests().bluetooth_saved_on = true;
  auto first = c.transition();
  auto failed = first.target;
  failed.bluetooth_on = false;
  c.markApplied(failed);
  EXPECT_NE(0, c.pendingChanges() & PeripheralPowerCoordinator::CHANGE_BLUETOOTH);

  c.markApplied(c.effective());
  EXPECT_EQ(0, c.pendingChanges());
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
