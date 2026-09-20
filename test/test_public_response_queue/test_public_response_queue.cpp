#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include "../../examples/simple_sensor/PublicResponseQueue.h"

TEST(PublicResponseQueue, KeepsShortRepliesInOneMessage) {
  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_FALSE(PublicResponseQueue::split("Pong", 128, first, second));
  EXPECT_STREQ("Pong", first);
  EXPECT_STREQ("", second);
}

TEST(PublicResponseQueue, SplitsAtCompleteLines) {
  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_TRUE(PublicResponseQueue::split("Trace: 1500 ms RTT\n1 Ridge +3.5 dB\n"
                                         "2 Valley -8.0 dB\n3 Summit +1.0 dB",
                                         50, first, second));
  EXPECT_STREQ("1/2\nTrace: 1500 ms RTT\n1 Ridge +3.5 dB", first);
  EXPECT_STREQ("2/2\n2 Valley -8.0 dB\n3 Summit +1.0 dB", second);
}

TEST(PublicResponseQueue, MarksOverflowAfterTwoMessages) {
  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_TRUE(PublicResponseQueue::split("First line\nSecond line\nThird line\nFourth line",
                                         24, first, second));
  EXPECT_STREQ("1/2\nFirst line", first);
  EXPECT_STREQ("2/2\n...truncated", second);
  EXPECT_LE(strlen(second), 24u);
}

TEST(PublicResponseQueue, SchedulesFourIndependentSecondParts) {
  PublicResponseQueue queue;
  for (int i = 0; i < 4; i++) EXPECT_TRUE(queue.schedule("2/2\nMore", 1000 + i));
  EXPECT_FALSE(queue.schedule("2/2\nExtra", 1005));
  char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_FALSE(queue.takeDue(999, part));
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(queue.takeDue(1000 + i, part));
    EXPECT_STREQ("2/2\nMore", part);
  }
  EXPECT_FALSE(queue.takeDue(1004, part));
}

TEST(PublicResponseQueue, StartsGapAfterFirstTransmissionCompletes) {
  PublicResponseQueue queue;
  int first_packet = 1;
  char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_TRUE(queue.scheduleAfterFirst("2/2\nMore", &first_packet));
  EXPECT_FALSE(queue.takeDue(60000, part));
  queue.onFirstSent(&first_packet, 60000);
  EXPECT_FALSE(queue.takeDue(62999, part));
  EXPECT_TRUE(queue.takeDue(63000, part));
  EXPECT_STREQ("2/2\nMore", part);
}

TEST(PublicResponseQueue, FailedFirstTransmissionReleasesSecondPart) {
  PublicResponseQueue queue;
  int first_packet = 1;
  char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_TRUE(queue.scheduleAfterFirst("2/2\nMore", &first_packet));
  queue.onFirstFailed(&first_packet);
  EXPECT_FALSE(queue.takeDue(60000, part));
  for (int i = 0; i < PublicResponseQueue::MAX_PENDING; i++)
    EXPECT_TRUE(queue.schedule("2/2\nOther", 0));
}

TEST(PublicResponseQueue, TenFullTraceLabelsFitWithDefaultSensorName) {
  std::string response = "Trace: 90000 ms RTT";
  for (int i = 1; i <= 10; i++)
    response += "\n" + std::to_string(i) + " ABCDEFGHIJKL -32.0 dB";

  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_TRUE(PublicResponseQueue::split(response.c_str(), 160 - strlen("BME680 Sensor: "),
                                         first, second));
  EXPECT_NE(std::string::npos, std::string(first).find("5 ABCDEFGHIJKL"));
  EXPECT_EQ(std::string::npos, std::string(first).find("6 ABCDEFGHIJKL"));
  EXPECT_NE(std::string::npos, std::string(second).find("10 ABCDEFGHIJKL -32.0 dB"));
  EXPECT_EQ(std::string::npos, std::string(second).find("...truncated"));
}

TEST(PublicResponseQueue, LongSensorNameMarksOverflowWithoutPartialLine) {
  std::string response = "Trace: 90000 ms RTT";
  for (int i = 1; i <= 10; i++)
    response += "\n" + std::to_string(i) + " ABCDEFGHIJKL -32.0 dB";

  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  EXPECT_TRUE(PublicResponseQueue::split(response.c_str(), 160 - 33, first, second));
  EXPECT_NE(std::string::npos, std::string(second).find("...truncated"));
  EXPECT_EQ(std::string::npos, std::string(second).find("10 ABCDEFGHIJKL"));
  EXPECT_LE(strlen(first), 127u);
  EXPECT_LE(strlen(second), 127u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
