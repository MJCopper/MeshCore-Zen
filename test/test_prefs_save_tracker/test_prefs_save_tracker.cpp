#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/PrefsSaveTracker.h"

TEST(PrefsSaveTracker, SkipsUnchangedStateAndTracksCoordinates) {
  NodePrefs prefs = {};
  solo::PrefsSaveTracker tracker;
  EXPECT_TRUE(tracker.needsSave(prefs, 0, 0));

  tracker.markSaved(prefs, 0, 0);
  EXPECT_FALSE(tracker.needsSave(prefs, 0, 0));
  prefs.freq = 915.875f;
  EXPECT_TRUE(tracker.needsSave(prefs, 0, 0));
  tracker.markSaved(prefs, 0, 0);
  EXPECT_FALSE(tracker.needsSave(prefs, 0, 0));
  EXPECT_TRUE(tracker.needsSave(prefs, -33.86, 151.21));
}

TEST(PrefsSaveTracker, FailedSaveLeavesPreviousStatePending) {
  NodePrefs prefs = {};
  solo::PrefsSaveTracker tracker;
  tracker.markSaved(prefs, 0, 0);
  prefs.gps_enabled = 1;
  // A failed write does not call markSaved(). Shutdown may retry it.
  EXPECT_TRUE(tracker.needsSave(prefs, 0, 0));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
