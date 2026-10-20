#include <gtest/gtest.h>
#include "../../examples/simple_room_server/ArchiveSessionPolicy.h"
TEST(ArchiveSession, HopCountUsesEncodedCountAndHandlesUnknownReversePath) {
  EXPECT_EQ(2, ArchiveSessionPolicy::hops(true, 2, 0xff));
  EXPECT_EQ(0, ArchiveSessionPolicy::hops(false, 9, 0xff));
  EXPECT_EQ(1, ArchiveSessionPolicy::hops(false, 9, 0x81));
  EXPECT_EQ(2, ArchiveSessionPolicy::hops(false, 0, 0x42));
}
TEST(ArchiveSession, FullBufferWrapAndTwentyMessageFloor) {
  struct Post { uint32_t post_timestamp = 0; } posts[256];
  EXPECT_EQ(0u, ArchiveSessionPolicy::historyFloor(posts, 0, 20));
  for (unsigned i=1; i<=300; i++) posts[(i-1)%256].post_timestamp=i;
  uint32_t floor = ArchiveSessionPolicy::historyFloor(posts, 300, 20);
  EXPECT_EQ(280u, floor);
  unsigned count=0;
  for (auto p:posts) count += ArchiveSessionPolicy::inSnapshot(p.post_timestamp, 0, floor, 300);
  EXPECT_EQ(20u,count);
  count=0;
  for (auto p:posts) count += ArchiveSessionPolicy::inSnapshot(p.post_timestamp, 0, 0, 300);
  EXPECT_EQ(256u,count);
}
TEST(ArchiveSession, SnapshotAndSyncSinceExcludeNewAndPreviouslyDeliveredPosts) {
  EXPECT_FALSE(ArchiveSessionPolicy::inSnapshot(301, 0, 280, 300));
  EXPECT_FALSE(ArchiveSessionPolicy::inSnapshot(290, 290, 280, 300));
  EXPECT_FALSE(ArchiveSessionPolicy::inSnapshot(280, 0, 280, 300));
  EXPECT_TRUE(ArchiveSessionPolicy::inSnapshot(300, 290, 280, 300));
}
int main(int argc,char**argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
