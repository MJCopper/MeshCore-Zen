#include <gtest/gtest.h>
#include <cstring>

#define ZEN_FEATURE_CHILD_MODE 1
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenRuntime.h"

TEST(ZenRuntime, OwnsParentSessionAndLockTransitions) {
  ZenPrefs prefs;
  prefs.child_mode_enabled = 1;

  zen::Runtime runtime;
  runtime.begin(&prefs);
  EXPECT_TRUE(runtime.childLocked(&prefs));
  EXPECT_TRUE(runtime.recordChildLockState(&prefs));
  EXPECT_FALSE(runtime.recordChildLockState(&prefs));

  runtime.setParentUnlocked(true);
  EXPECT_FALSE(runtime.childLocked(&prefs));
  EXPECT_FALSE(runtime.recordChildLockState(&prefs));

  runtime.setParentUnlocked(false);
  EXPECT_TRUE(runtime.recordChildLockState(&prefs));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
