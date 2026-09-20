#include <gtest/gtest.h>
#include <helpers/TracePathBounds.h>

TEST(TracePathBounds, AcceptsNextHopAndExactEnd) {
  EXPECT_EQ(mesh::TRACE_PATH_NEXT, mesh::getTracePathPosition(0, 0, 19));
  EXPECT_EQ(mesh::TRACE_PATH_NEXT, mesh::getTracePathPosition(18, 1, 38));
  EXPECT_EQ(mesh::TRACE_PATH_END, mesh::getTracePathPosition(19, 1, 38));
}

TEST(TracePathBounds, RejectsPartialAndOverrunHashes) {
  EXPECT_EQ(mesh::TRACE_PATH_INVALID, mesh::getTracePathPosition(21, 3, 174));
  EXPECT_EQ(mesh::TRACE_PATH_INVALID, mesh::getTracePathPosition(20, 1, 38));
  EXPECT_EQ(mesh::TRACE_PATH_INVALID, mesh::getTracePathPosition(1, 1, 3));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
