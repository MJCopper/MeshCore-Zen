#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/PathDetails.h"

TEST(PathDetails, DecodesUnknownDirectAndMultiBytePaths) {
  auto unknown = zen::pathShape(0xFF, 64);
  EXPECT_TRUE(unknown.valid);
  EXPECT_FALSE(unknown.known);
  auto direct = zen::pathShape(0, 64);
  EXPECT_TRUE(direct.valid);
  EXPECT_EQ(0, direct.hops);
  EXPECT_EQ(1, direct.hash_bytes);
  auto three = zen::pathShape((2 << 6) | 4, 64);
  EXPECT_TRUE(three.valid);
  EXPECT_EQ(4, three.hops);
  EXPECT_EQ(3, three.hash_bytes);
  EXPECT_FALSE(zen::pathShape((3 << 6) | 1, 64).valid);
  EXPECT_FALSE(zen::pathShape((2 << 6) | 22, 64).valid);
}

TEST(PathDetails, ResolvesOnlyUniqueKnownPrefixes) {
  const uint8_t keys[] = {0x10, 0x20, 0x30, 0x10, 0x21, 0x40};
  const uint8_t one[] = {0x10};
  const uint8_t two[] = {0x10, 0x20};
  const uint8_t missing[] = {0x99};
  EXPECT_EQ(-1, zen::uniquePrefixMatch(one, 1, keys, 2, 3));
  EXPECT_EQ(0, zen::uniquePrefixMatch(two, 2, keys, 2, 3));
  EXPECT_EQ(-1, zen::uniquePrefixMatch(missing, 1, keys, 2, 3));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
