#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/StorageHealth.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/FloodScopeView.h"

TEST(StorageHealth, ReservesReplacementAndMetadataSpace) {
  // A 12 KiB live file needs 12 KiB for its replacement, 4 KiB growth
  // headroom and two 4 KiB metadata blocks.
  EXPECT_TRUE(zen::StorageHealth::lowSpace(24 * 1024 - 1, 12 * 1024, 4096));
  EXPECT_FALSE(zen::StorageHealth::lowSpace(24 * 1024, 12 * 1024, 4096));
  EXPECT_TRUE(zen::StorageHealth::lowSpace(4 * 1024, 0, 4096));
}

TEST(StorageHealth, RejectsOnlyDefinitePerFileShortfall) {
  EXPECT_TRUE(zen::StorageHealth::cannotStageReplacement(4095, 4096));
  EXPECT_FALSE(zen::StorageHealth::cannotStageReplacement(4096, 4096));
  EXPECT_FALSE(zen::StorageHealth::cannotStageReplacement(0, 0));
}

TEST(StorageHealth, DigestRejectsShortWritesAndTracksCompleteRecord) {
  const uint8_t field[] = {1, 2, 3};
  zen::CheckedRecordDigest complete;
  complete.record(field, sizeof(field), sizeof(field));
  EXPECT_TRUE(complete.good());
  EXPECT_EQ(sizeof(field), complete.size());

  zen::CheckedRecordDigest short_write;
  short_write.record(field, sizeof(field), 2);
  EXPECT_FALSE(short_write.good());
  EXPECT_EQ(0u, short_write.size());
}

TEST(FloodScopeView, ExplicitUnscopedThenAppOverrideThenDefault) {
  using View = zen::FloodScopeView;
  EXPECT_EQ(View::APP_UNSCOPED, View::state(true, true, true));
  EXPECT_EQ(View::APP_OVERRIDE, View::state(false, true, true));
  EXPECT_EQ(View::DEFAULT, View::state(false, false, true));
  EXPECT_EQ(View::UNSCOPED, View::state(false, false, false));
}

TEST(FloodScopeView, NormalisesAndValidatesRegionNames) {
  char name[31];
  EXPECT_TRUE(zen::FloodScopeView::normaliseName("#Sydney", name, sizeof(name)));
  EXPECT_STREQ("Sydney", name);
  EXPECT_TRUE(zen::FloodScopeView::normaliseName("", name, sizeof(name)));
  EXPECT_STREQ("", name);
  EXPECT_FALSE(zen::FloodScopeView::normaliseName("Bad#name", name, sizeof(name)));
  EXPECT_FALSE(zen::FloodScopeView::normaliseName(" bad", name, sizeof(name)));
  EXPECT_FALSE(zen::FloodScopeView::normaliseName("A*", name, sizeof(name)));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
