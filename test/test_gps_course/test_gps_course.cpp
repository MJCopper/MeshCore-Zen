#include <gtest/gtest.h>
#include <limits.h>

#include "../../examples/companion_radio/solo/GpsCourse.h"

using solo::GpsCourse;

TEST(GpsCourse, RequiresFixValidValuesAndMovement) {
  EXPECT_TRUE(GpsCourse::available(true, 90000, 800));
  EXPECT_FALSE(GpsCourse::available(false, 90000, 800));
  EXPECT_FALSE(GpsCourse::available(true, LONG_MIN, 800));
  EXPECT_FALSE(GpsCourse::available(true, 90000, LONG_MIN));
  EXPECT_FALSE(GpsCourse::available(true, 90000, 799));
  EXPECT_FALSE(GpsCourse::available(true, 360000, 800));
}

TEST(GpsCourse, RoundsCourseToEightDirections) {
  EXPECT_STREQ(GpsCourse::label(GpsCourse::direction(0)), "N");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::direction(22499)), "N");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::direction(22500)), "NE");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::direction(90000)), "E");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::direction(337500)), "N");
}

TEST(GpsCourse, CompassTapeWrapsAroundNorth) {
  uint8_t north = GpsCourse::direction(0);
  EXPECT_STREQ(GpsCourse::label(GpsCourse::offset(north, -2)), "W");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::offset(north, -1)), "NW");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::offset(north, 1)), "NE");
  EXPECT_STREQ(GpsCourse::label(GpsCourse::offset(north, 2)), "E");
}

TEST(GpsCourse, RequiresTenMetresAndRetainsLastCourseForFifteenMinutes) {
  GpsCourse course;
  long value = LONG_MIN;
  course.update(1000, true, false, 0, true, 0, 0, 90000, 3000, 10);
  course.update(2000, true, false, 0, true, 0, 45, 90000, 3000, 10);  // approximately 5 m
  EXPECT_EQ(course.read(2000, value), GpsCourse::NONE);
  course.update(3000, true, false, 0, true, 0, 91, 90000, 3000, 10);  // approximately 10.1 m
  ASSERT_EQ(course.read(3000, value), GpsCourse::LIVE);
  EXPECT_EQ(value, 90000);

  course.update(9001, true, false, 0, true, 0, 91, 90000, 0, 10);
  EXPECT_EQ(course.read(9001, value), GpsCourse::LAST);
  EXPECT_EQ(course.read(3000 + GpsCourse::RETAIN_MS, value), GpsCourse::LAST);
  EXPECT_EQ(course.read(3001 + GpsCourse::RETAIN_MS, value), GpsCourse::NONE);
}

TEST(GpsCourse, CalculatesTravelOnlyBetweenCompletedPollingSessions) {
  GpsCourse course;
  long value = LONG_MIN;
  course.update(1000, true, true, 300, true, 0, 0, 0, 0, 10);
  course.update(2000, false, true, 300, true, 0, 0, 0, 0, 10);
  EXPECT_EQ(course.read(2000, value), GpsCourse::NONE);

  course.update(10000, true, true, 300, true, 0, 200, 0, 0, 10); // about 22 m east
  EXPECT_EQ(course.read(10000, value), GpsCourse::NONE);
  course.update(11000, false, true, 300, true, 0, 200, 0, 0, 10);
  ASSERT_EQ(course.read(11000, value), GpsCourse::TRAVEL);
  EXPECT_NEAR(value, 90000, 10);

  course.update(20000, true, true, 300, true, 0, 220, 0, 0, 10); // only about 2 m
  course.update(21000, false, true, 300, true, 0, 220, 0, 0, 10);
  EXPECT_EQ(course.read(21000, value), GpsCourse::NONE);
}

TEST(GpsCourse, HoldsLiveCourseAcrossBriefSlowSamples) {
  GpsCourse course;
  long value = LONG_MIN;
  course.update(1000, true, false, 0, true, 0, 0, 0, 2000, 10);
  course.update(2000, true, false, 0, true, 0, 100, 0, 2000, 10);
  ASSERT_EQ(course.read(2000, value), GpsCourse::LIVE);
  course.update(5000, true, false, 0, true, 0, 100, 0, 300, 10);
  EXPECT_EQ(course.read(5000, value), GpsCourse::LIVE);
  course.update(8001, true, false, 0, true, 0, 100, 0, 300, 10);
  EXPECT_EQ(course.read(8001, value), GpsCourse::LAST);
}

TEST(GpsCourse, RejectsPoorHdopAndAccumulatesShortPollingMoves) {
  GpsCourse course;
  long value = LONG_MIN;
  course.update(1000, true, true, 300, true, 0, 0, 0, 0, 10);
  course.update(2000, false, true, 300, true, 0, 0, 0, 0, 10);
  course.update(3000, true, true, 300, true, 0, 100, 0, 0, 10);
  course.update(4000, false, true, 300, true, 0, 100, 0, 0, 10);
  EXPECT_EQ(course.read(4000, value), GpsCourse::NONE);
  course.update(5000, true, true, 300, true, 0, 200, 0, 0, 10);
  course.update(6000, false, true, 300, true, 0, 200, 0, 0, 10);
  EXPECT_EQ(course.read(6000, value), GpsCourse::TRAVEL);

  GpsCourse poor;
  poor.update(1000, true, true, 300, true, 0, 0, 0, 0, 50);
  poor.update(2000, false, true, 300, true, 0, 0, 0, 0, 50);
  poor.update(3000, true, true, 300, true, 0, 300, 0, 0, 50);
  poor.update(4000, false, true, 300, true, 0, 300, 0, 0, 50);
  EXPECT_EQ(poor.read(4000, value), GpsCourse::NONE);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
